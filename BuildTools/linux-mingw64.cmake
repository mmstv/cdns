# This file (linux-mingw64.cmake) is for cross-compiling on Linux using
# MinGW compiler suite.
# Use it like this:
#    cmake  -DCMAKE_TOOLCHAIN_FILE=path-to/linux-mingw64.cmake  path-to/Project
# Determine DLL dependencies with one of:
#    objdump -p exefile | grep DLL
#    strings exefile | grep -i '\.dll$'

set (CMAKE_SYSTEM_NAME Windows)
set (CMAKE_SYSTEM_VERSION 6)
set (CMAKE_SYSTEM_PROCESSOR x86_64)

set (base "x86_64-w64-mingw32")
set (usrb "/usr/bin/${base}")
set (CMAKE_C_COMPILER         ${usrb}-gcc)
set (CMAKE_CXX_COMPILER       ${usrb}-g++)
set (CMAKE_Fortran_COMPILER   ${usrb}-g77)
set (CMAKE_RC_COMPILER        ${usrb}-windres)
set (CMAKE_AR                 ${usrb}-ar)
set (CMAKE_STRIP              ${usrb}-strip)
set (CMAKE_LINKER             ${usrb}-ld)
set (CMAKE_NM                 ${usrb}-nm)
set (CMAKE_OBJCOPY            ${usrb}-objcopy)
set (CMAKE_OBJDUMP            ${usrb}-objdump)

# C/CXX_FLAGS might need to be set in the override file:
#     set (CMAKE_USER_MAKE_RULES_OVERRIDE full-path-to/override_mingw.cmake)
# POSIX is for "%zu" in printf, SIGHUP in <csignal>, ::gmtime_r, etc
# -D_POSIX -D_POSIX_ -D_POSIX_C_SOURCE
# 0x0600 == Windows Vista?
set (CMAKE_C_FLAGS_INIT "-D_WIN32_WINNT=0x0600 -D_POSIX -D_POSIX_C_SOURCE")
set (CMAKE_CXX_FLAGS_INIT "-D_WIN32_WINNT=0x0600 -D_POSIX -D_POSIX_C_SOURCE")

# where to look for executables, libraries and headers
set (CMAKE_FIND_ROOT_PATH  /usr/${base})
# set (CMAKE_SYSROOT  /usr/${base})

# set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
# set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
