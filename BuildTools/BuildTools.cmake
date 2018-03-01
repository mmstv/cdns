cmake_minimum_required (VERSION 3.9)
include (CheckCXXCompilerFlag)
include (CheckCCompilerFlag)
include (GNUInstallDirs)
include (CheckIPOSupported) # CMake-3.9 is required

option (BT_COVERAGE "Build Debug Configuration with --coverage" OFF)

set(BT_TEST_TIMEOUT_SCALE 1 CACHE STRING "Scale factor for tests timeout")
set(BT_DEBUG_TEST_TIMEOUT_SCALE 1 CACHE STRING
	"Scale factor for Debug tests timeout")
mark_as_advanced (BT_TEST_TIMEOUT_SCALE BT_DEBUG_TEST_TIMEOUT_SCALE BT_COVERAGE)
if (CMAKE_CROSSCOMPILING AND (CMAKE_CROSSCOMPILING_EMULATOR MATCHES wine)
		AND (BT_TEST_TIMEOUT_SCALE LESS 20))
	# @TODO: There are some strange delays on return from 'main' with `wine`
	# under `ctest`. wine alone, without ctest works fine.
	set (BT_TEST_TIMEOUT_SCALE 20)
endif()

# it is set by default with Xcode but not with Visual Studio
set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)

set(_build_tools_dir "${CMAKE_CURRENT_LIST_DIR}")
# get_filename_component(_build_tools_dir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)

# Known compilers: gnu, msvc, intel, clang
function (check_cxx_flags flags var)
	string (REPLACE ";" "" fl "${flags}")
	string (REPLACE "+" "p" fl ${fl})
	string (REPLACE "-W" "W" fl ${fl})
	string (REPLACE "-" "_" fl ${fl})
	string (REPLACE "=" "_" fl ${fl})
	string (REPLACE ":" "_" fl ${fl})
	string (REPLACE "/" "" fl ${fl})
	string (REPLACE "," "" fl ${fl})
	string (APPEND CMAKE_REQUIRED_FLAGS "${flags}") # adds flag for linking as well
	check_cxx_compiler_flag ("" Flags_${fl})
	set (${var} ${Flags_${fl}} PARENT_SCOPE)
endfunction()

function(add_flags_internal)
	foreach(flag ${ARGV})
		string(FIND "${add_flags_vars}" "${flag}" found)
		if( found LESS 0 )
			string(REPLACE "+" "p" fl "${flag}")
			string(REPLACE "-" "_" fl "${fl}")
			string(REPLACE "=" "_" fl "${fl}")
			string(REPLACE ":" "_" fl "${fl}")
			string(REPLACE "/" "_" fl "${fl}")
			string(REPLACE " " "_" fl "${fl}")

			set(CMAKE_REQUIRED_FLAGS "${flag}") # adds flag for linking as well
			check_cxx_compiler_flag("" cxx_flag_${fl})
			unset(CMAKE_REQUIRED_FLAGS)
			if( cxx_flag_${fl} )
				string (APPEND internal_extra " ${flag}")
			endif()
		endif()
	endforeach()
	set(add_flags_vars "${add_flags_vars} ${internal_extra}" PARENT_SCOPE)
	unset(internal_extra)
endfunction()

function(add_cxx_flags)
	set(add_flags_vars ${CMAKE_CXX_FLAGS})
	add_flags_internal( ${ARGV} )
	# 'add_compile_options' is used only during compilation, not when linking
	set(CMAKE_CXX_FLAGS ${add_flags_vars} PARENT_SCOPE)
	unset(add_flags_vars)
endfunction()

function(add_cxx_release_flags)
	set(add_flags_vars ${CMAKE_CXX_FLAGS_RELEASE})
	add_flags_internal( ${ARGV} )
	# 'add_compile_options' is used only during compilation, not when linking
	#  add_compile_options("$<$<CONFIG:Release>:-fstack-protector-all>")
	set(CMAKE_CXX_FLAGS_RELEASE "${add_flags_vars}" PARENT_SCOPE)
	unset(add_flags_vars)
endfunction()

function(add_cxx_flags_conf cfg)
	string(TOUPPER "_${cfg}" cfgup)
	set(add_flags_vars ${CMAKE_CXX_FLAGS${cfgup}})
	add_flags_internal( ${ARGN} )
	# 'add_compile_options' is used only during compilation, not when linking
	#  add_compile_options("$<$<CONFIG:Release>:-fstack-protector-all>")
	set(CMAKE_CXX_FLAGS${cfgup} "${add_flags_vars}" PARENT_SCOPE)
	unset(add_flags_vars)
endfunction()

macro (cxx_secure)
	# security flags
	if ((CMAKE_CXX_COMPILER_ID MATCHES "Intel") OR CMAKE_COMPILER_IS_GNUCXX
		OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
		add_cxx_flags_conf (Release -fstack-protector-all)
	endif()
	if (UNIX) # maybe just Linux
		# add_definitions can't use generator expressions
		add_compile_options ("$<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2>")
	endif()
	if (UNIX AND (CMAKE_SYSTEM_NAME MATCHES Linux))
		# gnu linker
		# -Wl,-z,now  --  opposite of -Wl,-z,lazy (lazy binding, the default)
		# @TODO: LINK_WHAT_YOU_USE is a target property
		if (NOT CMAKE_LINK_WHAT_YOU_USE)
			set (ld_sec_needed ",--as-needed")
		else ()
			unset (ld_sec_needed)
		endif()
		set (ld_sec "-Wl${ld_sec_needed},--no-omagic,--sort-common")
		if (NOT (CMAKE_CXX_COMPILER_ID MATCHES Clang AND (BT_SANITIZE OR
			CMAKE_CXX_FLAGS MATCHES "-fsanitize")))
			# Clang can't find references to __ubsan_..., __asan_... functions
			set (ld_sec "${ld_sec},--no-undefined")
		endif()
		if (NOT MINGW)
			# -Wl,-z,noexecstack -- no executable stack
			set (ld_sec "${ld_sec},-z,noexecstack")
			# --unresolved-symbols=report-all
		endif()
	endif()
	if (ld_sec)
		check_cxx_flags (${ld_sec} ld_has_sec)
		if (ld_has_sec)
			foreach (m_ EXE SHARED MODULE)
				string (APPEND CMAKE_${m_}_LINKER_FLAGS " ${ld_sec}")
			endforeach()
		endif()
		unset (ld_sec)
	endif()
endmacro()

macro (add_linker_optimizations)
	if (APPLE)
		# optimize away unused dylibs when linking
		set (ld_opt "-Wl,-dead_strip,-dead_strip_dylibs")
		set (ld_opt_sh "${ld_opt} -Wl,-mark_dead_strippable_dylib")
		string (APPEND CMAKE_SHARED_LINKER_FLAGS_RELEASE ${ld_opt_sh})
	elseif (MSVC AND NOT(CMAKE_GENERATOR MATCHES "Visual Studio") )
		# Visual Studio Generator uses: /manifest:embed
		# Ninja does not generate /manifest option, generates: /incremental:no
		set (ld_opt " /manifest:no /incremental:no")
	elseif (UNIX AND (CMAKE_SYSTEM_NAME MATCHES Linux) AND (NOT MINGW))
		# gnu linker
		set (ld_opt "-Wl,-O1,--relax,-z,combreloc")
		# -Wl,-z,now  --  opposite of -Wl,-z,lazy (lazy binding, the default)
		# -Wl,-z,noexecstack -- no executable stack
		set (ld_dl "-Wl,--export-dynamic")
	elseif (MINGW)
		set (ld_opt "-Wl,-O1,--relax")
		# --unresolved-symbols=report-all
		set (ld_dl "-Wl,--export-dynamic")
	endif()
	if (ld_opt)
		check_cxx_flags (${ld_opt} ld_has_opt)
		if (ld_has_opt)
			foreach (_m EXE SHARED MODULE)
				string (APPEND CMAKE_${_m}_LINKER_FLAGS_RELEASE " ${ld_opt}")
			endforeach()
			unset (_m)
		endif()
		unset (ld_opt)
	endif()
	if (ld_dl)
		check_cxx_flags (${ld_dl} ld_has_dyn)
		if (ld_has_dyn)
			string (APPEND CMAKE_MODULE_LINKER_FLAGS " ${ld_dl}")
		endif()
		unset (ld_dl)
	endif()
endmacro()

macro (cxx_optimizations)
	# -fstrict-aliasing faster code, enabled by -O3 ?
	#"-fno-strict-aliasing" slower
	# Mac OSX 10.11 El Capitan clang-7.3.0 and Linux clang-3.8 with -ffast-math
	# raise inexact floating point exception with FPE exceptions turned on, when
	# computing sqrt(0.)
	if( (CMAKE_CXX_COMPILER_ID MATCHES "Intel") OR CMAKE_COMPILER_IS_GNUCXX)
		add_cxx_release_flags("-ffast-math" "-fstrict-aliasing")
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		add_cxx_release_flags("-fstrict-aliasing")
	elseif( MSVC )
		# disable iterator debugging. Visual Studio STL libraray uses
		# lot's of locking [_Lock(_LOCK_DEBUG)] in DEBUG mode, which causes
		# multithreaded apps to become very very slow
		add_definitions (-D_HAS_ITERATOR_DEBUGGING=0 -D_SECURE_SCL=0
			-D_ITERATOR_DEBUG_LEVEL=0)
		add_cxx_flags(
			# /EHsc  # for exceptions, enabled by default by all Generators
			/MP    # parallel compilation
			/Gm-   # disable minimal rebuild
			/analyze-  # disable native analysis
			# /EHa # for SEH exception automatic conversion into C++ exceptions,
			# SEH are non-recoverable errors, so using __try{}__except(){} instead
		)
		add_cxx_release_flags(
			/fp:fast    # fast floating point model
			# /fp:except  # consider floating point exceptions
			/Oy-      # for call stack backtrace
		)
	else()
		message(WARNING
			"Not supported C++ compiler. No extra optimization flags set.")
	endif()
	# IPO is supported with GCC-7 and Clang-5 since CMake-3.9 and
	# with Intel Compiler since CMake-3.0
	check_ipo_supported (RESULT result OUTPUT output LANGUAGES CXX)
	if (result)
		if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND (CMAKE_SYSTEM_NAME MATCHES
			Linux))
			message (WARNING "IPO is OFF, because it is broken With Clang-5.0 on "
				"Linux: -lpthread is not used")
		elseif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
			if (CMAKE_GENERATOR MATCHES Ninja AND CMAKE_VERSION VERSION_LESS "3.9")
				message (WARNING "IPO is OFF, because it is broken with CMake-3.0 "
					"and Ninja-1.5 and Intel Compiler: the -ipo compiler flag is "
					"set but xiar archiver is not used")
			else()
				message (WARNING "IPO is ON, but Intel compiler fails with -ipo "
					"occasionally")
				set (CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
			endif()
		else()
			set (CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
		endif()
	else()
		message (WARNING "IPO is not supported: ${output}")
	endif()
	unset (result)
	unset (output)
	add_linker_optimizations()
endmacro()

macro (cxx_warnings)
	string (REPLACE ";" "" fl "${ARGV}")
	string (REPLACE "+" "p" fl ${fl})
	string (REPLACE "-W" "W" fl ${fl})
	string (REPLACE "-" "_" fl ${fl})
	string (REPLACE "=" "_" fl ${fl})
	string (REPLACE ":" "_" fl ${fl})
	string (REPLACE "/" "" fl ${fl})
	check_cxx_compiler_flag ("${ARGV}" cxx_warn_${fl})
	if (cxx_warn_${fl})
		# These options will be used ONLY during compilation
		add_compile_options (${ARGV})
	else ()
		message (WARNING "Some warning flags are not supported by the compiler")
	endif()
endmacro()

macro (cxx_warnings_level)
	set(_warn_level 1)
	# ATTN! ARGVx is usable only if ${ARGC} > x, see `man cmake-commands`, macro
	if (${ARGC} GREATER 0)
		set(_warn_level ${ARGV0})
	endif()
	set (gnu_and_clang_warnings_1
		-Wunreachable-code
		-Wshadow
		-Wsign-conversion
		-Wconversion
		"-Wformat=2" -Wdiv-by-zero -Wwrite-strings
	)
	# ATTN!
	# clang-5.0 -Wcast-align warns about:
	#    char ch;
	#    int *i = (int*) &ch;
	# But does not warn about:
	#    int *i = reinterpret_cast <int*> (&ch);
	# gcc-7.2.0 might fail to warn about both of those
	set (gnu_and_clang_warnings_2
		-Wold-style-cast -Wcast-align
		-Wcast-qual "-Wfloat-equal" -Wpointer-arith -Wredundant-decls
		-Woverloaded-virtual
		-Wcast-align
		-Wdelete-non-virtual-dtor
		-Wnon-virtual-dtor
		-pedantic
	)
	set (gnu_and_clang_warnings_3
		# -Weffc++
		-Wmissing-declarations -Wswitch-enum
		# -Wno-missing-braces  # too many false positives with Template packs
		# -Wvariable-decl
	)
	if( WIN32 )
		# disable definitions min, max and small by windows.h
		add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
	endif()
	if (MSVC)
		if (_warn_level GREATER 1)
			foreach (wrn " /W2" " /W3")
				if (CMAKE_CXX_FLAGS)
					string (REPLACE ${wrn} " " CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
				endif()
			endforeach()
			cxx_warnings (/W4)
		endif()
		if (_warn_level GREATER 2)
			cxx_warnings (/Wall) # too many warnings in standard library
		endif()
		# disable non-secure API warnings
	elseif (CMAKE_COMPILER_IS_GNUCXX)
		# suggestions for pure functions, lots in flatbuffers.h
		# not very usefull for class methods and templates?
		# "-Wsuggest-attribute=const" "-Wsuggest-attribute=pure"
		cxx_warnings (-Wall -Wextra)
		if (CMAKE_GENERATOR MATCHES Ninja)
			add_cxx_flags ("-fdiagnostics-color")
		endif()
		if (_warn_level GREATER 0)
			cxx_warnings (${gnu_and_clang_warnings_1})
		endif()
		if (_warn_level GREATER 1)
			cxx_warnings (${gnu_and_clang_warnings_2} -Wpacked -Wplacement-new
				-Wnoexcept -Wstack-usage=20480)
		endif()
		if (_warn_level GREATER 2)
			cxx_warnings (${gnu_and_clang_warnings_3})
		endif()
		if (BT_COVERAGE)
			add_cxx_flags_conf (Debug "--coverage")
		endif()
	elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		cxx_warnings (-Wmost -Wextra)
		if (CMAKE_GENERATOR MATCHES Ninja)
			add_cxx_flags ("-fcolor-diagnostics")
		endif()
		if (_warn_level GREATER 0)
			cxx_warnings (${gnu_and_clang_warnings_1} -Wshorten-64-to-32)
		endif()
		if (_warn_level GREATER 1)
			cxx_warnings (${gnu_and_clang_warnings_2})
		endif()
		if (_warn_level GREATER 2)
			cxx_warnings (${gnu_and_clang_warnings_3}
				-Wnormalized=id) # @todo: what is this?
			# -Weverything is really intense
		endif()
		if (BT_COVERAGE)
			add_cxx_flags_conf(Debug "--coverage")
		endif()
	elseif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
		message(STATUS "Intel compiler: WARNING flags not set")
	else()
		message(WARNING "Not supported C++ compiler. No extra flags set.")
	endif()
endmacro()

function(setup_project_git_version)
	if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.git")
		find_package(Git)
		if( GIT_FOUND )
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" show -s --format="DATE: %ai   TAG: %h"
				HEAD WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				OUTPUT_VARIABLE PROJECT_GIT_DATE
			)
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" show -s --date=short --format=%ad HEAD
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				OUTPUT_VARIABLE PROJECT_GIT_DATE_ONLY
			)
			execute_process(
				COMMAND "${GIT_EXECUTABLE}" show -s --date=short --format=%h HEAD
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				OUTPUT_VARIABLE PROJECT_GIT_COMMIT_ONLY
			)
			string(STRIP "${PROJECT_GIT_DATE}" PROJECT_GIT_DATE)
			string(STRIP "${PROJECT_GIT_COMMIT_ONLY}" PROJECT_GIT_COMMIT_ONLY)
			string(STRIP "${PROJECT_GIT_DATE_ONLY}" PROJECT_GIT_DATE_ONLY)
			string(REPLACE "-" "" PROJECT_GIT_DATE_ONLY "${PROJECT_GIT_DATE_ONLY}")
			if( NOT "${PROJECT_GIT_DATE_ONLY}" STREQUAL "")
				string (SUBSTRING "${PROJECT_GIT_DATE_ONLY}" 2 6
					PROJECT_GIT_DATE_ONLY)
			endif()
		endif()
	endif()
	if( PROJECT_GIT_DATE_ONLY AND NOT PROJECT_VERSION_PATCH)
		math(EXPR _year ${PROJECT_GIT_DATE_ONLY}/100/100)
		math(EXPR _day ${PROJECT_GIT_DATE_ONLY}%100 )
		math(EXPR _month "(${PROJECT_GIT_DATE_ONLY}/100)%100")
		math(EXPR _week " ((${_year}-15)*365 + ${_month}*365/12 + ${_day})/7 ")
		set(PROJECT_VERSION_PATCH ${_week})
		set(PROJECT_VERSION_PATCH ${_week} PARENT_SCOPE)
	endif()
	set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH} PARENT_SCOPE)
	set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}")
	set(PROJECT_VERSION "${PROJECT_VERSION}.${PROJECT_VERSION_MINOR}")
	set(PROJECT_VERSION "${PROJECT_VERSION}.${PROJECT_VERSION_PATCH}")
	set(PROJECT_VERSION "${PROJECT_VERSION}" PARENT_SCOPE)
	set(outverfile "${CMAKE_BINARY_DIR}/include/${PROJECT_NAME}/version.hxx")
	configure_file("${_build_tools_dir}/project_git_version.hxx.cmakein"
		 ${outverfile} @ONLY)
	set_source_files_properties(${outverfile} PROPERTIES GENERATED TRUE)
	source_group("Interface" FILES ${outverfile})
	install(FILES ${outverfile}
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}"
		COMPONENT Development)
	unset(outverfile)
endfunction()

function(get_target_include_dirs target all_idirs)
	get_target_property(idirs ${target} INCLUDE_DIRECTORIES)
	unset(tmp_idirs)
	set(tmp_idirs ${${all_idirs}})
	if(idirs)
		list(APPEND tmp_idirs ${idirs})
	endif()
	unset(idirs)
	get_target_property(iidirs ${target} INTERFACE_INCLUDE_DIRECTORIES)
	if(iidirs)
		list(APPEND tmp_idirs ${iidirs})
	endif()
	unset(iidirs)
	if(tmp_idirs)
		list(REMOVE_DUPLICATES tmp_idirs)
		# setting both local and parent vars
		set(${all_idirs} ${tmp_idirs} PARENT_SCOPE)
		set(${all_idirs} ${tmp_idirs})
	endif()
	unset(tmp_idirs)
	unset(ilibs)
	get_target_property(ilibs ${target} INTERFACE_LINK_LIBRARIES)
	unset(libs)
	get_target_property(libs ${target} LINK_LIBRARIES)
	if( libs )
		list(APPEND ilibs ${libs})
	endif()
	if( ilibs )
		list(REMOVE_DUPLICATES ilibs)
	endif()
	foreach(lib ${ilibs})
		if(TARGET ${lib})
			get_target_include_dirs(${lib} ${all_idirs})
			if(${all_idirs})
				list(REMOVE_DUPLICATES ${all_idirs})
				set(${all_idirs} ${${all_idirs}} PARENT_SCOPE)
			endif()
		endif()
	endforeach()
endfunction()

macro (cxx_standard)
	# ATTN! ARGVx is usable only if ${ARGC} > x, see `man cmake-commands`, macro
	if (${ARGC} GREATER 0)
		set (_cxx_std ${ARGV0})
	else()
		set (_cxx_std 11)
	endif()
	enable_testing()
	set(CMAKE_LINK_DEPENDS_NO_SHARED ON) # faster incremental builds
	set(CMAKE_CXX_STANDARD_REQUIRED ON)
	if (NOT ((CMAKE_SYSTEM_NAME MATCHES SunOS) OR MINGW))
		# this will use -std=c++14 instead of -std=gnu++14, and might define
		# __STRICT_ANSI__, and might disable some non standard system functions
		set (CMAKE_CXX_EXTENSIONS OFF)
	endif()
	if(NOT CMAKE_CXX_STANDARD)
		set(CMAKE_CXX_STANDARD ${_cxx_std}) # supported: 98, 11, 14
	endif()
	unset(_cxx_std)
	if(CMAKE_CXX_STANDARD EQUAL 98)
		message(FATAL_ERROR "C++98 is not supported, it is too ancient. Upgrade.")
	endif()
	get_property(_var_cxx_features GLOBAL PROPERTY CMAKE_CXX_KNOWN_FEATURES)
	foreach(_feature ${_var_cxx_features})
		if(NOT (CMAKE_CXX_COMPILE_FEATURES MATCHES ${_feature}) )
			message(STATUS "Missing C++ feature: ${_feature}")
		endif()
	endforeach()
	unset(_var_cxx_features)
	if( (CMAKE_CXX_STANDARD EQUAL 14 OR CMAKE_CXX_STANDARD GREATER 14)
		AND(
			((CMAKE_CXX_COMPILER_ID MATCHES AppleClang) AND
				(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0.0))
			OR ((CMAKE_CXX_COMPILER_ID STREQUAL Clang) AND
				(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.6.0))
			OR (MSVC AND MSVC_VERSION LESS 1900)
			OR (CMAKE_COMPILER_IS_GNUCXX
				AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.3.0)
			OR NOT (CMAKE_CXX_COMPILE_FEATURES MATCHES cxx_delegating_constructors
				AND CMAKE_CXX_COMPILE_FEATURES MATCHES cxx_digit_separators
				AND CMAKE_CXX_COMPILE_FEATURES MATCHES cxx_attribute_deprecated
				AND CMAKE_CXX_COMPILE_FEATURES MATCHES cxx_binary_literals
				AND CMAKE_CXX_COMPILE_FEATURES MATCHES cxx_variadic_templates)
			)
		)
		message(FATAL_ERROR
			"C++14 or higher standard requires at least Apple Clang C++ 7.0 or"
			" Clang 3.6 or GNU C++ 5.3 or Visual Studio 2015 or more modern "
			"compiler. YOUR C++ is ${CMAKE_CXX_COMPILER_ID} "
			"${CMAKE_CXX_COMPILER_VERSION}")
	endif()
	set (CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)
endmacro()

macro(set_default_output_paths)
	# ATTN! ARGVx is usable only if ${ARGC} > x, see `man cmake-commands`, macro
	if (${ARGC} GREATER 0)
		set(_out_dir "/${ARGV0}")
	else()
		set(_out_dir "")
	endif()
	set (CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION TRUE)
	if(CMAKE_CONFIGURATION_TYPES)
		# this var is only set with Xcode or Visual Studio builders
		set(_cfgs ${CMAKE_CONFIGURATION_TYPES})
		message(STATUS "Available configs: ${CMAKE_CONFIGURATION_TYPES}")
	else()
		message(STATUS "Configuration: ${CMAKE_BUILD_TYPE}")
		# Debug Release RelWithDebInfo MinSizeRel ""
		set(_cfgs "${CMAKE_BUILD_TYPE}")
	endif()
	foreach(cfg ${_cfgs})
		string(TOUPPER "_${cfg}" cfgo)
		if( "${cfg}" STREQUAL "Release" )
			set(odir "${CMAKE_BINARY_DIR}")
		elseif( "${cfg}" STREQUAL "")
			set(odir "${CMAKE_BINARY_DIR}")
			set(cfgo "")
		else()
			set(odir "${CMAKE_BINARY_DIR}/${cfg}")
		endif()
		if(CMAKE_MACOSX_BUNDLE) # iOS Xcode build: everything is a bundle
			set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${cfgo} "${odir}/")
			set(CMAKE_LIBRARY_OUTPUT_DIRECTORY${cfgo}
				"${odir}${_out_dir}/${CMAKE_INSTALL_LIBDIR}")
			set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY${cfgo}
				"${odir}/${CMAKE_INSTALL_LIBDIR}")
		else()
			set(CMAKE_RUNTIME_OUTPUT_DIRECTORY${cfgo}
				"${odir}${_out_dir}/${CMAKE_INSTALL_BINDIR}")
			set(CMAKE_LIBRARY_OUTPUT_DIRECTORY${cfgo}
				"${odir}${_out_dir}/${CMAKE_INSTALL_LIBDIR}")
			set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY${cfgo}
				"${odir}${_out_dir}/${CMAKE_INSTALL_LIBDIR}")
		endif()
	endforeach()
	unset(_cfgs)
	unset(_out_dir)
	set(CMAKE_MACOSX_RPATH ON)
	if(APPLE)
		list(APPEND CMAKE_INSTALL_RPATH
			"@executable_path/../${CMAKE_INSTALL_LIBDIR}"
			"@executable_path/../Frameworks"
		)
		if(IOS)
			list(APPEND CMAKE_INSTALL_RPATH
				"@executable_path/${CMAKE_INSTALL_LIBDIR}"
				"@executable_path/Frameworks"
			)
			## in iOS install does not work properly
			set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
			if(NOT CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY)
				set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "iPhone Developer")
			endif()
			if(NOT CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET)
				set(CMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "8.0")
			endif()
		endif()
	elseif(UNIX)
		# Linux only
		list(APPEND CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
	endif()
endmacro()

function(configure_dll libname dllapi)
	include(GenerateExportHeader)
	get_filename_component(binincdir
		"${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_INCLUDEDIR}" REALPATH)
	set(dllcfgsrc "${binincdir}/${libname}/dll-cmake.hxx")
	set_source_files_properties( ${dllcfgsrc} PROPERTIES GENERATED TRUE)
	if( CMAKE_GENERATOR MATCHES Ninja AND ((CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		OR (CMAKE_C_COMPILER_ID MATCHES "Clang")) )
		# to force absolute path in compilation database: compile_commands.json
		# for code completion with vim clang_complete
		set(binincdir "/${binincdir}/")
	endif()
	target_include_directories(${libname} PUBLIC $<BUILD_INTERFACE:${binincdir}>)
	string(TOUPPER "${dllapi}" dllapiu)
	generate_export_header(${libname} BASE_NAME ${dllapiu}
		EXPORT_MACRO_NAME ${dllapiu}_API
		EXPORT_FILE_NAME ${dllcfgsrc}
	)
	target_sources(${libname} PRIVATE ${dllcfgsrc})
	source_group(Interface FILES ${dllcfgsrc})
	install(FILES ${dllcfgsrc}
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${libname}/"
		COMPONENT Development)
endfunction()

function(install_relative_header fn where_dir)
	if(IS_ABSOLUTE "${fn}" )
		message(FATAL_ERROR "Trying to install relative header with absolute path")
	endif()
	get_filename_component(_name "${fn}" NAME_WE)
	get_filename_component(_dir "${fn}" DIRECTORY)
	if(IS_ABSOLUTE "${_dir}")
		message(FATAL_ERROR  "That's odd")
	endif()
	install(FILES "${fn}"
		DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${where_dir}/${_dir}"
		COMPONENT Development)
endfunction()

function(setup_apple_bundle app)
	string(REPLACE "_" "" _gui_id "${app}")
	set_target_properties(${app} PROPERTIES
		MACOSX_BUNDLE_INFO_STRING "${CPACK_PACKAGE_DESCRIPTION_SUMMARY}"
		MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
		MACOSX_BUNDLE_LONG_VERSION_STRING "${app} ${PROJECT_VERSION}"
		MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
		MACOSX_BUNDLE_COPYRIGHT "${CPACK_PACKAGE_VENDOR} (C) 2013"
		MACOSX_BUNDLE_GUI_IDENTIFIER "${CPACK_PACKAGE_VENDOR}.${_gui_id}"
		MACOSX_BUNDLE_BUNDLE_NAME "${app}"

		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
		RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}
		RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
		RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/RelWithDebInfo
		RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/MinSizeRel
	)
endfunction()


function(add_lib_ typ libname interface_list sources_list)
	# ATTN! ARGVx is usable only if ${ARGC} > x, see `man cmake-commands`, macro
	if (${ARGC} GREATER 4)
		set(private_list ${ARGV4})
	else()
		set(private_list)
	endif()
	add_library(${libname} ${typ} ${${sources_list}} ${${private_list}}
		${${interface_list}})
	target_include_directories(${libname} PUBLIC
		"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
		"$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
	)
	string(TOUPPER "${libname}" libnameu)
	set_target_properties(${libname} PROPERTIES FOLDER Libraries/${PROJECT_NAME}
		LABELS ${PROJECT_NAME}Library
		VERSION "${PROJECT_VERSION}"
		SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
		DEFINE_SYMBOL "${libnameu}_DLL_EXPORTS"
		# install strips directory structure from those files
		# PUBLIC_HEADER "${${interface_list}}"
	)
	source_group("Private Headers" FILES ${${private_list}})
	source_group(Interface FILES ${${interface_list}})
	if(APPLE)
		# @todo Apple does not like big version numbers, 32bit limit
		set(pver ${PROJECT_VERSION})
		set_target_properties(${libname} PROPERTIES LINK_FLAGS
			"-current_version ${pver} -compatibility_version ${pver}"
		)
	endif()
	if( "${typ}" STREQUAL "SHARED")
		configure_dll(${libname} ${libname})
	endif()
	install(TARGETS ${libname}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
		BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
		FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Development
		# that strips directory structure from header files, use install(DIRECTORY)
		# PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
		#COMPONENT Development
		INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT Development
	)
	foreach(head ${${interface_list}})
		install_relative_header(${head} ${libname})
	endforeach()
endfunction()


function(add_shared_lib libname interface_list sources_list)
	add_lib_(SHARED ${libname} ${interface_list} ${sources_list} ${ARGN})
endfunction()

function(add_static_lib libname interface_list sources_list)
	add_lib_(STATIC ${libname} ${interface_list} ${sources_list} ${ARGN})
endfunction()

# @TODO simplify
# Additional Parameters:
#   Third [ARGV2]: time for Debug Configuration,
#   Forth [ARGV3]: time for MSVC Debug configuration
function(set_test_time test time)
	set(_time ${time})
	if(BT_TEST_TIMEOUT_SCALE AND (BT_TEST_TIMEOUT_SCALE GREATER 1))
		math(EXPR _time "${time}*${BT_TEST_TIMEOUT_SCALE}") # for valgrind
	endif()
	set(_labels "${time}_sec;${PROJECT_NAME}")
	math(EXPR _dtime "${_time}*6")
	# ATTN! ARGV3 is usable only if ${ARGC} > 3, see `man cmake-commands`, macro
	if ((${ARGC} GREATER 3) AND MSVC AND (ARGV3 GREATER 0))
		list(APPEND _labels "SLOW_MSVC_DEBUG")
		set(_dtime ${ARGV3})
	# ATTN! ARGV2 is usable only if ${ARGC} > 2, see `man cmake-commands`, macro
	elseif ((${ARGC} GREATER 2) AND (ARGV2 GREATER 0))
		set(_dtime ${ARGV2})
	endif()
	if(BT_DEBUG_TEST_TIMEOUT_SCALE AND (BT_DEBUG_TEST_TIMEOUT_SCALE GREATER 1) )
		math(EXPR _dtime "${_dtime}*${BT_DEBUG_TEST_TIMEOUT_SCALE}")
	endif()
	if(time GREATER 60)
		list(APPEND _labels "intense")
	endif()
	math(EXPR _tmp "${_time}*10")
	if(_dtime GREATER _tmp)
		list(APPEND _labels "SLOW_DEBUG")
	endif()
	# @TODO: how to mark beginning of word in CMake regular expression?
	set_tests_properties (${test} PROPERTIES COST ${_time}
		TIMEOUT "$<$<CONFIG:Debug>:${_dtime}>$<$<NOT:$<CONFIG:Debug>>:${_time}>"
		FAIL_REGULAR_EXPRESSION " *DEBUG!")
	set_property (TEST ${test} APPEND PROPERTY LABELS "${_labels}")
	# the following does not work
	# set_property (TEST ${test} PROPERTY TIMEOUT_AFTER_MATCH "12" "valgrind")
	unset(_labels)
	if (CMAKE_CROSSCOMPILING AND CMAKE_CROSSCOMPILING_EMULATOR MATCHES wine)
		set_property (TEST ${test} APPEND PROPERTY ENVIRONMENT
			"WINEPATH=${CMAKE_FIND_ROOT_PATH}/bin")
	endif()
endfunction()

function (target_is_test exe)
	set_target_properties (${exe} PROPERTIES FOLDER Tests/${PROJECT_NAME})
	set_property (TARGET ${exe} APPEND PROPERTY LABELS Tests)
	if (CMAKE_MACOSX_BUNDLE) # $<TARGET_PROPERTY:${exename},MACOSX_BUNDLE> )
		setup_apple_bundle(${exe})
	endif()
	if (NOT WIN32)
		# on windows test executables need to be in the same folder
		# as dlls, because it seems to be complicated to set environment
		# path variable with set_tests_properties( ENVIRONMENT "PATH=")
		# and not absolutely necessary
		if(CMAKE_CONFIGURATION_TYPES)
			# this var is only set with Xcode or Visual Studio builders
			set(_cfgs ${CMAKE_CONFIGURATION_TYPES})
		else()
			set(_cfgs "${CMAKE_BUILD_TYPE}")
		endif()
		foreach(build ${_cfgs})
			string(TOUPPER "${build}" bu)
			if( NOT "${bu}" STREQUAL "" )
				set(bu "_${bu}")
			endif()
			set_target_properties(${exe} PROPERTIES RUNTIME_OUTPUT_DIRECTORY${bu}
				"${CMAKE_RUNTIME_OUTPUT_DIRECTORY${bu}}/tests/${PROJECT_NAME}"
			)
		endforeach()
	endif()
endfunction()

function (exe_timed_test exe name time)
	unset (_pc)
	if (CMAKE_CROSSCOMPILING)
		set (_pc "$<TARGET_PROPERTY:${exe},CROSSCOMPILING_EMULATOR>")
	endif()
	list (APPEND _pc "$<TARGET_FILE:${exe}>")
	if ("x${name}" STREQUAL "x")
		set (_tname "${exe}")
	else()
		set (_tname "${exe}_${name}")
	endif()
	add_test (NAME ${_tname} COMMAND ${_pc} ${ARGN})
	unset (_pc)
	set_test_time ("${_tname}" "${time}")
endfunction()

function(add_timed_test time fn)
	get_filename_component(fnwe "${fn}" NAME_WE)
	# ATTN! ARGVx is usable only if ${ARGC} > x, see `man cmake-commands`, macro
	if (${ARGC} GREATER 2)
		set(_lib ${ARGV2})
	else()
		set(_lib ${PROJECT_NAME})
	endif()
	set(exe "${fnwe}_${_lib}")
	add_executable(${exe} "${fn}")
	if(ARGN)
		target_link_libraries(${exe} ${ARGN})
	endif()
	target_is_test (${exe})
	exe_timed_test ("${exe}" "" "${time}")
endfunction()

function(add_15sec_test fn)
	add_timed_test(15 ${fn} ${ARGN})
endfunction()


function(add_1sec_test fn)
	add_timed_test(1 ${fn} ${ARGN})
endfunction()


function(add_3sec_test fn)
	add_timed_test(3 ${fn} ${ARGN})
endfunction()


function(add_1min_test fn)
	add_timed_test(60 ${fn} ${ARGN})
endfunction()


function(add_exe exename sources_list)
	add_executable(${exename} ${sources_list})
	if (NOT (UNIX OR MINGW))
		# in UNIX it adds -X.Y.Z version suffix to the executable name
		set_target_properties(${exename} PROPERTIES VERSION "${PROJECT_VERSION}")
	endif()
	set_target_properties(${exename} PROPERTIES FOLDER "Executables/${PROJECT_NAME}"
		LABELS "${PROJECT_NAME}Executable"
		SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}"
	)
	if(ARGN)
		target_link_libraries(${exename} ${ARGN})
	endif()
	if( CMAKE_MACOSX_BUNDLE ) # $<TARGET_PROPERTY:${exename},MACOSX_BUNDLE> )
		setup_apple_bundle(${exename})
	endif()
	## -current_version is only for Apple DLLs
	install(TARGETS ${exename}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
		BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT Runtime
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
		FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT Runtime
	)
endfunction()

macro (cxx_clang_tidy)
	option (BT_TIDY "Enable Static Code Analysis" OFF)
	mark_as_advanced (BT_TIDY)

	# CMake-3.8
	if (BT_TIDY AND NOT DEFINED CMAKE_CXX_CLANG_TIDY)
		# clang++ --analyze
		find_program (BT_CLANG_TIDY NAMES clang-tidy clang-check
			DOC "C++ static code analysis tool")
		mark_as_advanced (BT_CLANG_TIDY)
		if (BT_CLANG_TIDY MATCHES clang-tidy)
			# 'linker' input unused [clang-diagnostic-unused-command-line-argument]
			# warning is issued by clang-tidy because of -MD -MT -MF compiler options
			# It is disabled by '-extra-arg=-w'
			set (CMAKE_CXX_CLANG_TIDY "${BT_CLANG_TIDY}"
				"-format-style=file" "-analyze-temporary-dtors"
				"-header-filter=.*"
				"-extra-arg=-w")
		elseif (BT_CLANG_TIDY MATCHES clang-check)
			set (CMAKE_CXX_CLANG_TIDY "${BT_CLANG_TIDY}" "-analyze")
		else()
			message (FATAL_ERROR "Usupported clang-tidy: ${BT_CLANG_TIDY}")
		endif()
		if (NOT (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
			message (WARNING "clang-tidy is not supported with "
				"${CMAKE_CXX_COMPILER_ID}")
			if (NOT CMAKE_EXPORT_COMPILE_COMMANDS)
				message (FATAL_ERROR "Enable CMAKE_EXPORT_COMPILE_COMMANDS for "
					"clang-tidy")
			endif()
			list (APPEND CMAKE_CXX_CLANG_TIDY "-p=${CMAKE_BINARY_DIR}"
				"-extra-arg=-Wno-unknown-warning-option")
		endif()
	endif()
	if (DEFINED CMAKE_CXX_CLANG_TIDY)
		message (STATUS "C++ static analysis: [${CMAKE_CXX_CLANG_TIDY}]")
	else()
		message (STATUS "C++ static analysis: [OFF]")
	endif()
endmacro()

macro (cxx_sanitizer)
	option (BT_SANITIZE "Enable Runtime Santizer" OFF)
	mark_as_advanced (BT_SANITIZE)
	if (BT_SANITIZE)
		if ((CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
				AND NOT MINGW)
			if (CMAKE_SYSTEM_NAME MATCHES Linux AND CMAKE_SYSTEM MATCHES grsec)
				# Error output:
				#    Shadow memory range interleaves with an existing memory mapping.
				#    ASan cannot proceed correctly. ABORTING.
				message (WARNING "ASAN maybe broken on Grsec Linux")
				add_cxx_flags ("-fsanitize=undefined")
			else()
				add_cxx_flags ("-fsanitize=address,leak,undefined")
			endif()
		else ()
			message (WARNING "Sanitizer requested but not supported by compiler")
		endif ()
	endif()
endmacro()
