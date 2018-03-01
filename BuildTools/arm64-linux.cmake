# This file (arm64-linux.cmake) is for cross-compiling on Linux for ARMv8
# Use it like this:
#    export LDFLAGS='-Wl,-rpath-link,/tmp/dns/lib'
#    cmake  -DCMAKE_TOOLCHAIN_FILE=path-to/arm64-linux.cmake  path-to/Project
# Determine DLL dependencies with one of:
#    objdump -p exefile | grep DLL
#    strings exefile | grep -i '\.dll$'
set (CMAKE_SYSTEM_NAME Linux)
set (CMAKE_SYSTEM_VERSION 4)
set (CMAKE_SYSTEM_PROCESSOR aarch64)

set (base "aarch64-linux-gnu")
set (usrb "/usr/bin/${base}")
set (CMAKE_C_COMPILER         ${usrb}-gcc)
set (CMAKE_CXX_COMPILER       ${usrb}-g++)
set (CMAKE_Fortran_COMPILER   ${usrb}-g77)
set (CMAKE_AR                 ${usrb}-ar)
set (CMAKE_CXX_COMPILER_AR    ${usrb}-ar)
set (CMAKE_RANLIB             ${usrb}-ranlib)
set (CMAKE_CXX_COMPILER_RANLIB ${usrb}-ranlib)
set (CMAKE_STRIP              ${usrb}-strip)
set (CMAKE_LINKER             ${usrb}-ld)
set (CMAKE_NM                 ${usrb}-nm)
set (CMAKE_OBJCOPY            ${usrb}-objcopy)
set (CMAKE_OBJDUMP            ${usrb}-objdump)

# C/CXX_FLAGS might need to be set in the override file:
#     set (CMAKE_USER_MAKE_RULES_OVERRIDE full-path-to/override_mingw.cmake)

# where to look for executables, libraries and headers
set (CMAKE_FIND_ROOT_PATH  /usr/${base})
# set (CMAKE_SYSROOT  /usr/${base})

# set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
# set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
