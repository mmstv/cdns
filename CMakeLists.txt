cmake_minimum_required (VERSION 3.9)
# Build Linux Debian package:
#    cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/usr -DCPACK_GENERATOR=DEB ThisFolder
#    ninja package
# List all available targets
#    ninja help
# Run tests on 8 CPUs
#    CTEST_PARALLEL_LEVEL=8 ninja test
# Run valgrind tests on 8 CPUs
#    CTEST_PARALLEL_LEVEL=8 ninja ExperimentalMemCheck
# Build For Windows-64 on Linux:
#    cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=ThisFolder/BuildTools/linux-mingw64.cmake
#        ThisFolder

set (CMAKE_BUILD_TYPE Release CACHE STRING
	"Choose the type of build: None Debug Release RelWithDebInfo MinSizeRel")
# Generate json compile database, used by clang-check, clang-tidy and Vim
# clang-complete. Default advanced option set to OFF
option (CMAKE_EXPORT_COMPILE_COMMANDS
	"Enable/Disable output of compile commands during generation" ON)

project (cdns VERSION 1.0.0 DESCRIPTION "Cryptographic DNS resolver" LANGUAGES CXX)
set (CPACK_PACKAGE_VENDOR "m.mstv")
set (CPACK_PACKAGE_CONTACT "mmstv@github")
set (CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://gihub.com/mmstv/${PROJECT_NAME}")

cmake_host_system_information (RESULT ncpu QUERY NUMBER_OF_LOGICAL_CORES)
if (ncpu AND CMAKE_CTEST_COMMAND MATCHES ctest)
	list (APPEND CMAKE_CTEST_COMMAND "-j${ncpu}") # Used only by CTest module
	message (STATUS "Modified ctest command: ${CMAKE_CTEST_COMMAND}")
	# set (CTEST_PARALLEL_LEVEL ${ncpu})
	# set ($ENV{CTEST_PARALLEL_LEVEL} ${ncpu}) # does not help, instead:
	# CTEST_PARALLEL_LEVEL=8 ninja test
endif()
unset (ncpu)

# include (CMakePrintSystemInformation) too much information
if (CMAKE_CROSSCOMPILING)
	if (MINGW AND NOT CMAKE_CROSSCOMPILING_EMULATOR)
		# search only host for the 'wine' emulator
		find_program (CMAKE_CROSSCOMPILING_EMULATOR wine
			DOC "Emulator for cross-compiled programs" NO_CMAKE_FIND_ROOT_PATH)
		mark_as_advanced (CMAKE_CROSSCOMPILING_EMULATOR)
	endif()
	message (STATUS "Cross-compiling on ${CMAKE_HOST_SYSTEM} "
		"@ ${CMAKE_HOST_SYSTEM_PROCESSOR}")
	message (STATUS "Emulator: ${CMAKE_CROSSCOMPILING_EMULATOR}")
endif()
message (STATUS "Target system: ${CMAKE_SYSTEM} @ ${CMAKE_SYSTEM_PROCESSOR}")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
# @TODO: too many false positives (libm) set (CMAKE_LINK_WHAT_YOU_USE TRUE)
# set (CMAKE_CXX_INCLUDE_WHAT_YOU_USE TRUE "/usr/bin/include-what-you-use-tool")
# get_directory_property (BUILDSYSTEM_TARGETS) # CMake-3.7
# get_directory_property (SUBDIRECTORIES) # CMake-3.7

include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckIncludeFileCXX)
# include(CMakePackageConfigHelpers)

list (APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/BuildTools")
include (BuildTools)
if (BT_TEST_TIMEOUT_SCALE LESS 4)
	set (BT_TEST_TIMEOUT_SCALE 4) # for valgrind
endif()
cxx_sanitizer() # must be called before CTest

if (NOT BUILDNAME) # undocumented used by CTest module
	set (BUILDNAME "${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}")
endif()
if (NOT MEMORYCHECK_TYPE AND (BT_SANITIZE OR CMAKE_CXX_FLAGS MATCHES "-fsanitize"))
	set (MEMORYCHECK_TYPE AddressSanitizer)
else()
	# disable these tests only for valgrind MemCheck
	set (CTEST_CUSTOM_MEMCHECK_IGNORE
		"${PROJECT_NAME}-srvcli-udp;${PROJECT_NAME}-srvcli-tcp"
		"${PROJECT_NAME}-srvcli-tcp-udp;btfailsqrt_backtrace;bttst2_backtrace"
		"btfailtst_backtrace;btfailidivz_backtrace")
endif()
# This file seems required when MemoryCheckType = Sanitizer in DartConfigure.tcl
configure_file (CTestCustom.cmake.in "CTestCustom.cmake")

if (UNIX AND CMAKE_SYSTEM_NAME MATCHES Linux AND NOT (BT_SANITIZE
	OR CMAKE_CXX_FLAGS MATCHES "-fsanitize"))
	# Used by CTest to generate DartConfigure.tcl
	set (MEMORYCHECK_SUPPRESSIONS_FILE
		"${CMAKE_CURRENT_SOURCE_DIR}/valgrind_linux.supp" CACHE FILEPATH
		"File that contains suppressions for the memory checker")
endif()
include (CTest) # call after cxx_sanitizer

set_default_output_paths()
# setup_project_git_version()
cxx_clang_tidy()
cxx_secure()
cxx_optimizations()
cxx_standard(14) # C++14 Standard
cxx_warnings_level(2)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)

find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
	pkg_check_modules(SYSTEMD libsystemd)
	set(HAVE_LIBSYSTEMD ${SYSTEMD_FOUND})
endif()

check_include_file_cxx (getopt.h HAVE_GETOPT_H)
check_include_file_cxx ("ev++.h" HAVE_EVPP_H)

if (HAVE_EVPP_H)
	if (WIN32)
		set (CMAKE_REQUIRED_LIBRARIES ws2_32)
	endif()
	# ATTN! libev must be DLL to avoid multiple default event loops
	# @TODO: is it possible to check if library is DLL?
	check_library_exists (ev ev_supported_backends "" HAVE_LIBEV)
	unset (CMAKE_REQUIRED_LIBRARIES)
endif()
if (NOT HAVE_LIBEV)
	message (FATAL_ERROR "need libev")
endif()

check_library_exists (sodium sodium_hex2bin "" HAVE_SODIUM)
# minimum: 7/3
if(NOT HAVE_SODIUM)
	message(FATAL_ERROR "Need libsodium")
endif()

set(sodium_funcs
	crypto_box_easy_afternm crypto_core_hchacha20
	crypto_box_curve25519xchacha20poly1305_open_easy_afternm
)
set(CMAKE_REQUIRED_LIBRARIES sodium)
foreach(func ${sodium_funcs})
	string(TOUPPER "${func}" ufunc)
	check_function_exists(${func} HAVE_${ufunc})
endforeach()
unset(CMAKE_REQUIRED_LIBRARIES)
if (NOT HAVE_CRYPTO_BOX_CURVE25519XCHACHA20POLY1305_OPEN_EASY_AFTERNM)
	message (FATAL_ERROR, "need chacha")
endif()

configure_file(package.h.in "include/package.h")
configure_file("data/systemd.service.in" "${PROJECT_NAME}.service")
configure_file("data/systemd.socket.in" "${PROJECT_NAME}.socket")
configure_file("data/default.conf.in" "${PROJECT_NAME}.conf")

add_subdirectory(backtrace)
add_subdirectory(sys) # configuration only
add_subdirectory(network) # configuration only
add_subdirectory(dns)
add_subdirectory(dns/crypt)
if(BUILD_TESTING)
	add_subdirectory(dns/crypt/tests)
endif()

install(FILES data/dnscrypt-resolvers.csv data/dnscrypt-resolvers.csv.sig
	data/dnscrypt-resolvers.csv.minisig data/dns-resolvers.txt
	data/minisign.pub data/whitelist.txt data/blacklist.txt
	"data/apple-blacklist.txt" data/microsoft-blacklist.txt data/social-blacklist.txt
	DESTINATION "${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}"
	COMPONENT "Runtime")

install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}.socket"
	"${PROJECT_BINARY_DIR}/${PROJECT_NAME}.service"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system"
	COMPONENT "Runtime")

# _FULL_SYSCONFDIR => /etc or /usr/local/etc ABSOLUTE path
# _SYSCONFDIR => ${DESTDIR}/${CMAKE_INSTALL_PREFIX}/etc RELATIVE path
# @TODO: it all gets screwed up when packaging, if ABSOLUTE path is used in the
# `install` command
install(FILES "${PROJECT_BINARY_DIR}/${PROJECT_NAME}.conf"
	DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${PROJECT_NAME}"
	COMPONENT "Runtime")

set (CPACK_DEBIAN_PACKAGE_DEPENDS systemd libsodium)
# set (CPACK_DEBIAN_PACKAGE_RELEASE 1)
# set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE x86_64)
set (CPACK_DEBIAN_PACKAGE_REPLACES dnscrypt-proxy dnscrypt-wrapper)
# set (CPACK_RPM_PRE_INSTALL_SCRIPT_FILE pkg.install)
# set (CPACK_RPM_POST_INSTALL_SCRIPT_FILE pkg.install)
#set (CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/pack/postinst")
set (CPACK_WARN_ON_ABSOLUTE_INSTALL_DESTINATION ON)
# set (CPACK_PACKAGE_INSTALL_DIRECTORY ${PROJECT_NAME})
set (CPACK_DMG_FORMAT UDBZ) # bzip2 compressed
set (CPACK_DEBIAN_COMPRESSION_TYPE xz)

set (CPACK_PROJECT_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/pack/config.cmake")
# set (CPACK_PACKAGE_DEFAULT_LOCATION "/") # @TODO: what is this for?
set (CPACK_PACKAGE_RELOCATABLE FALSE) # @TODO: what is this for? RPM only?
set (CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set (CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
# set (CPACK_PACKAGE_DESCRIPTION_FILE "pack/description.txt")
# set (CPACK_INSTALL_SCRIPT "additional_install_for_packing.cmake")
set (CPACK_PACKAGE_CHECKSUM SHA256) # CMake-3.7
set (CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME}")
set (CPACK_STRIP_FILES ON)
# set (CPACK_PACKAGE_ICON "pack/icon.bmp")

if (NOT CPACK_SOURCE_GENERATOR)
	set (CPACK_SOURCE_GENERATOR TXZ) # TGZ, TBZ2
endif()

if (NOT CPACK_GENERATOR)
	if (APPLE)
		set (CPACK_GENERATOR DragNDrop) # "Bundle" "DragNDrop" "PackageMaker" "ZIP")
	elseif (WIN32)
		set (CPACK_GENERATOR ZIP) # "Bundle""PackageMaker" "ZIP")
	elseif (UNIX)
		set (CPACK_GENERATOR TBZ2) # DEB, RPM, TGZ, STGZ
	endif()
endif()

include (CPack)

# get_directory_property (sdirs SUBDIRECTORIES) # CMake-3.7
# get_directory_property (targets DIRECTORY dns BUILDSYSTEM_TARGETS) # CMake-3.7
message ("==> To see all available targets, execute: ${CMAKE_MAKE_PROGRAM} help")
