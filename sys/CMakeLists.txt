cmake_minimum_required (VERSION 3.9)

include(CheckCXXSymbolExists)
include(CheckIncludeFileCXX)

##! @todo: where is sandbox.h?
set (heads "paths.h" pwd.h grp.h linux/random.h unistd.h sys/stat.h sys/types.h
	sys/ioctl.h fcntl.h)

foreach(head ${heads})
	string(REPLACE "/" "_" uhead "${head}")
	string(REPLACE "." "_" uhead "${uhead}")
	string(REPLACE "+" "p" uhead "${uhead}")
	string(TOUPPER "${uhead}" uhead)
	check_include_file_cxx(${head} HAVE_${uhead})
	if (HAVE_${uhead})
		list (APPEND _incs ${head})
	endif()
endforeach()

# gmtime_r is thread safe version of std::gmtime
set (funcs getpwnam gmtime_r initgroups)

list (APPEND _incs ctime)
foreach(func ${funcs})
	string(TOUPPER "${func}" ufunc)
	check_cxx_symbol_exists(${func} "${_incs}" HAVE_${ufunc})
endforeach()
unset (_incs)

configure_file(sysconfig.h.in "${PROJECT_BINARY_DIR}/include/sysconfig.h")
add_definitions (-DHAVE_CONFIG_H)

set(interface
	logger.hxx
	sysunix.hxx
	str.hxx
)

set(sources
	srcz/logger.cpp
	srcz/entropy.cpp
	srcz/str.cpp
)

if (UNIX AND NOT MINGW)
	list (APPEND sources srcz/sysunix.cpp)
endif()

add_shared_lib(sys interface sources)
target_include_directories(sys PUBLIC
	"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_link_libraries (sys PRIVATE ${SYSTEMD_LIBRARIES})
# find_package (Threads)
# if (THREADS_FOUND)
#    target_link_libraries (sys PRIVATE Threads::Threads) # because of <atomic>
# endif()

if (BUILD_TESTING)
	add_1sec_test (srcz/tests/log_t0.cpp sys backtrace)
endif()
