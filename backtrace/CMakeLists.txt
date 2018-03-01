# Weired: VERSION 3.0 might add -rdynamic compile flag during compilation checks
# in MinGW. This flag is not supported by MinGW gcc.
cmake_minimum_required (VERSION 3.9)

project(backtrace VERSION 1.0.0 LANGUAGES CXX)
include(CheckIncludeFileCXX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/BuildTools")

macro(find_windows_sdk)
	## SDK Paths on Windows, they are not set by Visual Studio Generator
	## so trying to guess it
	if( MSVC AND WIN32 AND "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
		set(_pvar "ProgramFiles(x86)")
		set(progfiles "$ENV{${_pvar}}")
		set(winkits "${progfiles}\\Windows Kits")
		if(CMAKE_CL_64)
			set(_arch "x64")
		else()
			set(_arch "x86")
		endif()
		list(APPEND CMAKE_PREFIX_PATH "${winkits}")
		if(MSVC_VERSION GREATER 1899)
			# todo: find various versions ?
			set(_ver "10.0.10240.0")
			set(_kit "10")
		elseif(MSVC_VERSION GREATER 1799)
			set(_ver "winv6.3")
			set(_kit "8.1")
		elseif(MSVC_VERSION GREATER 1699)
			set(_ver "winv8")
			set(_kit "8.0")
		elseif(MSVC_VERSION)
			message(WARNING "unsupported MS VC compiler: ${MSVC_VERSION}")
		endif()
		set(winkit "${winkits}/${_kit}")
		list(APPEND CMAKE_PREFIX_PATH "${winkit}")
		list(APPEND CMAKE_INCLUDE_PATH "${winkit}/Include/${_ver}/um")
		list(APPEND CMAKE_LIBRARY_PATH "${winkit}/Lib/${_ver}/um/${_arch}")
		unset(_ver)
		unset(_kit)
		unset(winkit)
		unset(winkits)
		unset(progfiles)
	endif()
endmacro()

find_windows_sdk()

check_include_file_cxx(unistd.h HAVE_UNISTD_H) # open, close, write, STDERR_FILENO
check_include_file_cxx(cxxabi.h HAVE_CXXABI_H) # demangling
if (MINGW AND (CMAKE_HOST_SYSTEM_NAME MATCHES Linux)
	AND (CMAKE_SIZEOF_VOID_P EQUAL 4))
	message (WARNING "xmmintrin.h is broken with i686-w64-mingw32-gcc in Arch Linux")
else()
	check_include_file_cxx(xmmintrin.h HAVE_XMMINTRIN_H) # float exceptions
endif()

if (WIN32)
	set(dbghelpsrc
		"#include <windows.h>
		#include <dbghelp.h>
		int main()
		{
		SymInitialize(0,0,0);
		return 0;
		}
		"
	)
	## @todo use PATH HINTS, otherwise it is not found if LIB environment is not set
	find_library (DBGHLP NAMES dbghelp dbghelp.lib libdbghelp.a DOC "for backtrace")
	if( DBGHLP )
		message(STATUS "Dbghelp.lib was found: ${DBGHLP}")
		find_file (DBGHLPINC dbghelp.h DOC "for backtrace")
		if(DBGHLPINC)
			message(STATUS "dbghelp.h: ${DBGHLPINC}")
			get_filename_component(DBGHLPINCP "${DBGHLPINC}" DIRECTORY)
			set(CMAKE_REQUIRED_LIBRARIES ${DBGHLP})
			check_cxx_source_compiles("${dbghelpsrc}" HAVE_DBGHELP_LIB)
			unset(CMAKE_REQUIRED_LIBRARIES)
		endif()
	endif()
	if( NOT HAVE_DBGHELP_LIB )
		message(WARNING "Dbghelp.lib is not found or inappropriate; no backtrace")
	endif()
else()
	find_package(Backtrace)
	if(Backtrace_FOUND)
		set(HAVE_BACKTRACE TRUE)
		message(STATUS "Backtrace header: ${Backtrace_HEADER}")
	endif()
endif()

set(outcfgfile "${CMAKE_BINARY_DIR}/include/backtrace/config.h")
configure_file(src/backtrace_config.h.cmakein ${outcfgfile})

set(sources src/backtrace.cpp src/catch.cpp src/exe_path.cpp)
set(interface backtrace.hxx catch.hxx exceptions.hxx)
set(private ${outcfgfile})

add_shared_lib(backtrace interface sources private)
target_compile_definitions(backtrace PRIVATE -DHAVE_BACKTRACE_CONFIG_H)
target_include_directories(backtrace PUBLIC
	"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/..>")

if(Backtrace_FOUND)
	if(Backtrace_INCLUDE_DIRS)
		target_include_directories(backtrace PRIVATE ${Backtrace_INCLUDE_DIRS})
	endif()
	if(Backtrace_LIBRARIES)
		message(STATUS "Backtrace libs: ${Backtrace_LIBRARIES}")
		target_link_libraries(backtrace PRIVATE ${Backtrace_LIBRARIES})
	endif()
elseif(HAVE_DBGHELP_LIB)
	target_link_libraries(backtrace PRIVATE "${DBGHLP}")
	target_include_directories(backtrace PRIVATE "${DBGHLPINCP}")
	if(MSVC)
		target_compile_options(backtrace PUBLIC /Oy-)
	endif()
endif()
if ((CMAKE_COMPILER_IS_GNUCXX AND NOT MINGW) OR (CMAKE_SYSTEM_NAME MATCHES FreeBSD)
	OR ((CMAKE_SYSTEM_NAME MATCHES Linux)
	AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang")))
	# needed with gcc beacause of the bug, see:
	# http://stackoverflow.com/questions/19463602/compiling-multithread-code-with-g
	# also with clang on Linux
	find_package(Threads)
	if (THREADS_FOUND)
		target_link_libraries(backtrace PRIVATE Threads::Threads)
	endif()
endif()

# Actually if GNU linker is used
if ((CMAKE_SYSTEM_NAME MATCHES Linux) AND (NOT MINGW))
	# See `man 3 backtrace`
	# This flag is needed to make function names available in the backtrace
	target_link_libraries (backtrace PUBLIC -rdynamic)
endif()

if(HAVE_CXXABI_H)
	add_exe(demangle src/demangle.cpp)
elseif(NOT WIN32)
	message(WARNING "Demangling is not available")
endif()

add_1sec_test(src/tests/bttst.cpp  backtrace)
add_1sec_test(src/tests/btfailtst.cpp  backtrace)
add_1sec_test(src/tests/btfailidivz.cpp  backtrace)
add_1sec_test(src/tests/btfailsqrt.cpp  backtrace)
add_1sec_test(src/tests/btsqrt.cpp  backtrace)
add_1sec_test (src/tests/bttst2.cpp backtrace)
set_tests_properties (bttst2_backtrace PROPERTIES
	PASS_REGULAR_EXPRESSION  "pretty_int"
	FAIL_REGULAR_EXPRESSION "uggly_static;BACKTRACE TESTS PASS!")
if (MINGW)
	# @TODO MinGW does not support backtrace
	set_tests_properties (bttst2_backtrace PROPERTIES DISABLED TRUE)
	set_property (TEST bttst2_backtrace APPEND PROPERTY LABELS nomingw)
endif()
if (BT_SANITIZE OR CMAKE_CXX_FLAGS MATCHES "-fsanitize")
	# These tests do illegal stuff trying to raise signals. Memory sanitizer or
	# valgrind will intercept illegal actions before backtrace library.
	set (_tsts bttst2_backtrace btfailtst_backtrace btfailidivz_backtrace
		btfailsqrt_backtrace)
	set_tests_properties (${_tsts} PROPERTIES DISABLED TRUE)
	set_property (TEST ${_tsts} APPEND PROPERTY LABELS no_sanitize)
endif()

# In MinGW these tests will "succeed" by raising FPE signal but will not print
# backtrace.
set_tests_properties (btfailtst_backtrace btfailidivz_backtrace btfailsqrt_backtrace
	PROPERTIES PASS_REGULAR_EXPRESSION "SIGNAL ERROR!")
