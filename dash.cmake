cmake_minimum_required (VERSION 3.9)
include (CTestCoverageCollectGCOV)
# @TODO: This script does not set CompilerName in output .xml files

if (NOT CTEST_SITE)
	set (CTEST_SITE "unspecified")
endif()

if (TEST_CLEAN) # or TestModel = Nightly
	set (TEST_MEMCHECK ON)
	set (TEST_COVER ON) # --coverage
	set (TEST_TIDY ON) # static code analysis with clang-tidy
	set (TEST_MODEL Nightly)
else()
	unset (conf_opts)
endif()

if (NOT TEST_MODEL)
	set (TEST_MODEL Experimental)
endif()

macro (get_time)
	string (TIMESTAMP _time "%a@%H:%M:%S")
endmacro()

if (TEST_MEMCHECK)
	# Increasing test timeouts because of valgrind
	set (conf_opts "-DBT_TEST_TIMEOUT_SCALE=16;-DBT_DEBUG_TEST_TIMEOUT_SCALE=40")
endif()
if (TEST_COVER)
	list (APPEND conf_opts "-DBT_COVERAGE=ON")
endif()
if (TEST_TIDY)
	list (APPEND conf_opts "-DBT_TIDY=ON")
endif()
# set(CTEST_USE_LAUNCHERS TRUE)

# ctest -DCTEST_CMAKE_GENERATOR=Ninja -S path/to/this/file
if (NOT CTEST_CMAKE_GENERATOR)
	if (CMAKE_HOST_APPLE OR CMAKE_HOST_UNIX)
		set (CTEST_CMAKE_GENERATOR "Unix Makefiles")
	elseif (CMAKE_HOST_WIN32)
		if (NOT "$ENV{VS_GENERATOR}" STREQUAL "")
			set (CTEST_CMAKE_GENERATOR "$ENV{VS_GENERATOR}")
		elseif ("$ENV{label}" STREQUAL "VS2015")
			set (CTEST_CMAKE_GENERATOR "Visual Studio 14 2015 Win64")
		else()
			set (CTEST_CMAKE_GENERATOR "Visual Studio 14 2015")
		endif()
	endif()
endif()

# ctest -DCTEST_SOURCE_DIRECTORY=src -S path/to/this/file
if (NOT CTEST_SOURCE_DIRECTORY)
	get_filename_component (cursrcdir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
	get_filename_component (srcdir "${cursrcdir}/" REALPATH)
	set (CTEST_SOURCE_DIRECTORY "${srcdir}")
endif()

# ctest -C Debug -S path/to/this/file
if (NOT "$ENV{CMAKE_CONFIG_TYPE}" STREQUAL "")
	set (CTEST_CONFIGURATION_TYPE "$ENV{CMAKE_CONFIG_TYPE}")
else()
	set (CTEST_CONFIGURATION_TYPE Release)
endif()

if (CTEST_CONFIGURATION_TYPE MATCHES Debug)
	set (CTEST_TEST_TIMEOUT 600) # seconds
else()
	set (CTEST_TEST_TIMEOUT 360) # seconds
endif()

set (CTEST_BUILD_NAME "${CMAKE_SYSTEM_NAME}${CTEST_CONFIGURATION_TYPE}")
if (TEST_CLEAN)
	set (CTEST_BUILD_NAME "${CTEST_BUILD_NAME}-CLEAN")
endif()

if (CMAKE_C_COMPILER)
	list (APPEND conf_opts "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
endif()
if (CMAKE_CXX_COMPILER)
	list (APPEND conf_opts "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
endif()

string (REPLACE " " "" _generator "${CTEST_CMAKE_GENERATOR}")
set (_build_p "build${CMAKE_HOST_SYSTEM_NAME}")

# always use unique folder for a Configuration because Testing folder will be
# overwritten otherwise
set (CTEST_BINARY_DIRECTORY "${_build_p}${_generator}${CTEST_CONFIGURATION_TYPE}")

if (TEST_NCPU)
	set (NCPU ${TEST_NCPU})
else()
	cmake_host_system_information (RESULT NCPU QUERY NUMBER_OF_LOGICAL_CORES)
endif()

if (NOT NCPU EQUAL 0)
	if (CTEST_CMAKE_GENERATOR MATCHES "Unix Makefiles")
		set (CTEST_BUILD_FLAGS -j${NCPU})
	elseif (CTEST_CMAKE_GENERATOR MATCHES "Visual Studio")
		set (CTEST_BUILD_FLAGS "/m /v:minimal")
	elseif (CTEST_CMAKE_GENERATOR MATCHES "Xcode")
		set (CTEST_BUILD_FLAGS "-jobs ${NCPU} -parallelizeTargets" )
	endif()
endif()

## remove old build dirs
string(REPLACE "Release" "" _cur_build "${CTEST_BINARY_DIRECTORY}")
string(REPLACE "Debug" "" _cur_build "${_cur_build}")
string(REPLACE "${CTEST_CONFIGURATION_TYPE}" "" _cur_build "${_cur_build}")
file(GLOB _build_dirs "build*/")
foreach(_build_path ${_build_dirs})
	get_filename_component(_build_dir "${_build_path}" NAME)
	string(REPLACE "${CTEST_CONFIGURATION_TYPE}" "" _build_dir "${_build_dir}")
	string(REPLACE "Debug" "" _build_dir "${_build_dir}")
	string(REPLACE "Release" "" _build_dir "${_build_dir}")
	#
	if( "${_cur_build}" STREQUAL "${_build_dir}" )
		message("NOT removing ${_build_path}")
	else()
		message("Removing old build folder: ${_build_path}")
		file(REMOVE_RECURSE "${_build_path}")
	endif()
endforeach()

# ctest -DTEST_CLEAN=ON -S path/to/this/file
get_time()
if (TEST_CLEAN)
	file (REMOVE_RECURSE "${CTEST_BINARY_DIRECTORY}")
	# the following does not seem to work?
	ctest_empty_binary_directory ("${CTEST_BINARY_DIRECTORY}")
else()
	file (REMOVE_RECURSE "${CTEST_BINARY_DIRECTORY}/Testing")
	file (REMOVE_RECURSE "${CTEST_BINARY_DIRECTORY}/**/*.gcda") # coverage files
	file (REMOVE_RECURSE "${CTEST_BINARY_DIRECTORY}/**/*.gcno") # coverage files
endif()
#####################################################################################

# ctest_start makes CTEST_BINARY_DIRECTORY/Testing,
# and reads CTEST_SOURCE_DIRECTORY/CtestConfig.cmake
ctest_start (${TEST_MODEL}) # TRACK ${TEST_MODEL}-ATrack)
message ("Project: ${CTEST_PROJECT_NAME}, Build: ${CTEST_BUILD_NAME}, Model: "
	"${TEST_MODEL}, Parallel: ${NCPU}, on ${_time}")
# execute_process (COMMAND cmake -E environment)

if (TEST_UPDATE)
	message ("Updating source in ${CTEST_SOURCE_DIRECTORY}")
	ctest_update (RETURN_VALUE retval)
	message ("   result: ${retval}")
endif()

get_time()
message ("Configuring ${CTEST_SOURCE_DIRECTORY} in ${CTEST_BINARY_DIRECTORY}" )
message ("   with ${CTEST_CMAKE_GENERATOR} ${conf_opts} on ${_time}")
ctest_configure (OPTIONS "${conf_opts}" RETURN_VALUE retval)
string (TIMESTAMP _time)
message ("   Result: ${retval} on ${_time}")

if (retval EQUAL 0)
	string (TIMESTAMP _time)
	message ("Building with: ${CTEST_BUILD_FLAGS} on ${_time}")
	ctest_read_custom_files ("${CTEST_BINARY_DIRECTORY}")
	ctest_build (RETURN_VALUE retval NUMBER_ERRORS nerr NUMBER_WARNINGS nwarn)
	string (TIMESTAMP _time)
	message ("   wanrings: ${nwarn},  errors: ${nerr},  result: ${retval} "
		"on ${_time}")
	if (nwarn GREATER 10)
		message ("   ERROR! Too many build warnings!")
		set (retval -1)
	elseif (retval EQUAL 0 AND nerr EQUAL 0)
		string (TIMESTAMP _time)
		if (TEST_CLEAN)
			message ("Testing all tests with ${NCPU} cpus, on ${_time}")
			ctest_test (RETURN_VALUE retval PARALLEL_LEVEL ${NCPU})
		else()
			message ("Testing, excluding intense time-consuming tests, ${NCPU} cpus")
			ctest_test (RETURN_VALUE retval EXCLUDE_LABEL "intense"
				PARALLEL_LEVEL ${NCPU})
		endif()
		string (TIMESTAMP _time)
		message ("   Result: ${retval}, on ${_time}")
		if (retval EQUAL 0)
			if (TEST_COVER)
				find_program (CTEST_COVERAGE_COMMAND NAMES gcov) # lcov does html
				if (CTEST_COVERAGE_COMMAND AND CTEST_CONFIGURATION_TYPE MATCHES
						Debug)
					message ("Coverage analysis")
					ctest_coverage (RETURN_VALUE retval LABELS coverage)
					message ("   result: ${retval}")
				endif()
			endif()
			if (TEST_MEMCHECK)
				find_program (CTEST_MEMORYCHECK_COMMAND NAMES valgrind)
				if (CTEST_MEMORYCHECK_COMMAND)
					set (CTEST_MEMORYCHECK_TYPE Valgrind)
					# set(CTEST_MEMORYCHECK_COMMAND_OPTIONS "--error-exitcode=-314")
					if (UNIX) # CMAKE_HOST_UNIX
						# only for Linux
						if (NOT CTEST_MEMORYCHECK_SUPPRESSIONS_FILE)
							set (CTEST_MEMORYCHECK_SUPPRESSIONS_FILE
								"${CTEST_SOURCE_DIRECTORY}/valgrind_linux.supp")
						endif()
					endif()
					set (_exclude "bla-bla-bla|bad-bad-bad")
					string (TIMESTAMP _time)
					message ("Memory check with valgrind, on ${_time}")
					message ("   suppress: ${CTEST_MEMORYCHECK_SUPPRESSIONS_FILE}")
					ctest_memcheck (RETURN_VALUE retval
						PARALLEL_LEVEL ${NCPU} EXCLUDE ${_exclude} DEFECT_COUNT ndef)
					string (TIMESTAMP _time)
					message ("   defects: ${ndef}, result: ${retval}, on ${_time}")
					if (NOT ndef EQUAL 0)
						set (retval -1)
					endif()
				endif()
			endif()
		endif()
	endif()
	# message("Building package")
	# this overwrites all Build_*.log and Build.xml files
	# @todo: use file(COPY ...) ?
	# ctest_build(TARGET package RETURN_VALUE buildres)
endif()

if (CTEST_DROP_SITE)
	ctest_upload (FILES blabla.tar.xz)
endif()

if (CTEST_DROP_SITE)
	message ("Submitting to ${CTEST_DROP_SITE}")
	unset (submitres)
	ctest_submit (RETURN_VALUE submitres)
	if (NOT submitres EQUAL 0)
		message("   errors occurred in submission")
	endif()
else()
	message ("NOT submitting results, no server in: CTEST_DROP_SITE")
endif()
string (TIMESTAMP _time)
message ("Done at ${_time}")
if (retval EQUAL 0)
	message ("Ok!")
else()
	message (FATAL_ERROR "Failed!")
endif()
