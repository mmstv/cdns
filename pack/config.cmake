set (_ignore_files "${CPACK_PACKAGE_NAME}-.*\\.tar\\."
	"${CPACK_PACKAGE_NAME}-.*\\.zip\$")
# list (APPEND CPACK_SOURCE_IGNORE_FILES ${_ignore_files})
# The follwoing is used for both source and binary packages
list (APPEND CPACK_IGNORE_FILES  ${_ignore_files})
unset (_ignore_files)

if (CPACK_GENERATOR MATCHES DragNDrop)
	set(tmppkg "/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}")
	set(CPACK_PACKAGING_INSTALL_PREFIX "/${tmppkg}.${CPACK_PACKAGE_VERSION_MINOR}")
	# set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON) # packaging will fail
elseif (CPACK_GENERATOR MATCHES STGZ)
	# set(CPACK_PACKAGING_INSTALL_PREFIX "/opt")
elseif (CPACK_GENERATOR MATCHES Bundle)
	#set(tmppkg "/${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}")
	#set(CPACK_PACKAGING_INSTALL_PREFIX "/${tmppkg}.${CPACK_PACKAGE_VERSION_MINOR}")
	#set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
elseif (CPACK_GENERATOR MATCHES DEB)
	# cmake -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc
	#       -DCPACK_GENERATOR=DEB
	if (CPACK_PACKAGING_INSTALL_PREFIX)
		set (_pref "${CPACK_PACKAGING_INSTALL_PREFIX}")
	else()
		set (_pref "/usr") # default prefix for DEB packager
	endif()
	if (NOT ("${_pref}" STREQUAL "${CPACK_INSTALL_PREFIX}"))
		message (FATAL_ERROR "Installation and packaging prefixes differ: "
			"${_pref} != ${CPACK_INSTALL_PREFIX}")
	endif()
	unset (_pref)
elseif (CPACK_GENERATOR MATCHES TBZ2 OR CPACK_GENERATOR MATCHES ZIP)
	# Use local directory when packing, because of ABSOLUTE installation path: /etc
	# This will use CMAKE_INSTALL_PREFIX instead of CPACK_PACKAGING_INSTALL_PREFIX
	set (CPACK_SET_DESTDIR TRUE)
endif()
