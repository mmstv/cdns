message(STATUS "GIT_EXE: ${GIT_EXECUTABLE}")
message(STATUS "CUR_SRC: ${srcdir}")
message(STATUS "CUR_BIN: ${builddir}")

execute_process(
	COMMAND "${GIT_EXECUTABLE}" show -s  --format="DATE: %ai   TAG: %h" HEAD
	WORKING_DIRECTORY ${srcdir}
	OUTPUT_VARIABLE gitdate
)
message(STATUS "git HEAD ${gitdate}")

if( gitdate STREQUAL "" )
	file(WRITE ${builddir}/version.txt "VERSION Unknown")
else()
	file(WRITE ${builddir}/version.txt "VERSION ${gitdate}")
endif()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" status --porcelain
	WORKING_DIRECTORY ${srcdir}
	OUTPUT_VARIABLE gitstatus
)
message(STATUS "git status:\n${gitstatus}")
if( gitstatus STREQUAL "" )
	file(APPEND ${builddir}/version.txt "STATUS Unknown\n")
else()
	file(APPEND ${builddir}/version.txt "STATUS:\n${gitstatus}\n")
endif()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" log  -n 1 "--pretty=%w(80)%B" HEAD
	WORKING_DIRECTORY ${srcdir}
	OUTPUT_VARIABLE gitlog
)

message(STATUS "git log:  ${gitlog}")
if( gitlog STREQUAL "" )
	file(APPEND ${builddir}/version.txt "LOG Unknown\n")
else()
	file(APPEND ${builddir}/version.txt "LOG:  ${gitlog}\n")
endif()

execute_process(
	COMMAND "${GIT_EXECUTABLE}" log  "--pretty=--- %ai %h%n%w(80)%B" HEAD
	WORKING_DIRECTORY ${srcdir}
	OUTPUT_FILE "${builddir}/ChangeLog.txt"
)
