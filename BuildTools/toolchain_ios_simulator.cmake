# cmake -G Xcode -DCMAKE_TOOLCHAIN_FILE=path/to/this/file  path/to/source/dir
set (CMAKE_SYSTEM_NAME Darwin)
set (CMAKE_SYSTEM_VERSION 8)
# set(CMAKE_SYSTEM_PROCESSOR armv7)
set (UNIX TRUE)
set (APPLE TRUE)
set (IOS TRUE)

if(CMAKE_GENERATOR MATCHES Xcode)
	set(CMAKE_MACOSX_BUNDLE YES)
	set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_REQUIRED "NO")
endif()

# set(CMAKE_GENERATOR_PLATFORM "iPhoneSimulator")
set(triple  "x86_64-apple-ios")
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})
set(CMAKE_OSX_ARCHITECTURES x86_64)
unset(triple)


get_filename_component(cursrcdir "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
get_filename_component(srcdir "${cursrcdir}/" REALPATH)
set(CMAKE_USER_MAKE_RULES_OVERRIDE "${srcdir}/override_ios.cmake")
unset(cursrcdir)
unset(srcdir)

set(CMAKE_OSX_DEPLOYMENT_TARGET "")

set(devroot /Applications/Xcode.app/Contents/Developer)
set(toolchain ${devroot}/Toolchains/XcodeDefault.xctoolchain)

set(CMAKE_C_COMPILER ${toolchain}/usr/bin/clang)
set(CMAKE_CXX_COMPILER ${toolchain}/usr/bin/clang++)
if(CMAKE_GENERATOR MATCHES Xcode)
	set (CMAKE_CXX_COMPILER_WORKS TRUE)
	set (CMAKE_C_COMPILER_WORKS TRUE)
endif()


# Setup iOS platform unless specified manually with IOS_PLATFORM
set (IOS_PLATFORM "SIMULATOR64")

set (SIMULATOR_MODE true)
set (IOS_PLATFORM_LOCATION "iPhoneSimulator.platform")
# set (CMAKE_XCODE_EFFECTIVE_PLATFORMS "-iphonesimulator;-iphoneos")

set (IOS_SDKS_ROOT "${devroot}/Platforms/${IOS_PLATFORM_LOCATION}/Developer")

file (GLOB _IOS_SDKS "${IOS_SDKS_ROOT}/SDKs/*")
if (NOT _IOS_SDKS)
	message (FATAL_ERROR "No iOS SDK's found in default search path"
		" ${IOS_SDKS_ROOT}.")
endif ()
list (SORT _IOS_SDKS)
list (REVERSE _IOS_SDKS)
list (GET _IOS_SDKS 0 IOS_SDK_ROOT)
message (STATUS "Toolchain using default iOS SDK: ${IOS_SDK_ROOT}")

set(CMAKE_OSX_SYSROOT ${IOS_SDK_ROOT})
# set(CMAKE_SYSROOT ${IOS_SDKS_ROOT})
set(CMAKE_FIND_ROOT_PATH "${IOS_SDK_ROOT};${devroot}")

set (CMAKE_FIND_FRAMEWORK FIRST)
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
