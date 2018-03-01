# Use it like this:
#   cmake  -DCMAKE_TOOLCHAIN_FILE=path-to/toolchain_ios.cmake \
#      -DCMAKE_USER_MAKE_RULES_OVERRIDE=full-path-to/override_ios.cmake \
#      path-to/Project

if(NOT (CMAKE_GENERATOR MATCHES Xcode) )
	if(IOS_PLATFORM MATCHES "SIMULATOR")
		set (CMAKE_C_FLAGS_INIT "-mios-simulator-version-min=8.2")
		set (CMAKE_CXX_FLAGS_INIT "-mios-simulator-version-min=8.2")
	else()
		set (CMAKE_C_FLAGS_INIT "-arch armv7 -arch arm64 -mios-version-min=8.2")
		set (CMAKE_CXX_FLAGS_INIT "-arch armv7 -arch arm64 -mios-version-min=8.2")
	endif()
endif()
