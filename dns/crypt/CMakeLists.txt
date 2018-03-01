cmake_minimum_required (VERSION 3.9)

set(pkgdatadir "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
add_definitions( -DHAVE_CONFIG_H -DPKGDATADIR="${pkgdatadir}" )

set(main_src main.cpp)

set(interface resolver.hxx
	daemon.hxx
	certifier.hxx
	certificate.hxx
	server_crypt.hxx
	crypt_options.hxx
	constants.hxx
	csv.hxx
)

set(sources
	${PROJECT_SOURCE_DIR}/crypt/crypt.hxx
	"${PROJECT_SOURCE_DIR}/crypt/crypt.cpp"
	srcz/resolver.cpp
	srcz/encryptor.cpp
	srcz/csv.cpp
	srcz/daemon.cpp
	srcz/certifier.cpp
	srcz/certificate.cpp
	srcz/server_crypt.cpp
	srcz/crypt_options.cpp
	srcz/cresponder.cpp
	srcz/pad_buffer.cpp srcz/pad_buffer.hxx
)

add_shared_lib(dnscrypt interface sources)
target_include_directories(dnscrypt PUBLIC
	"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")
target_link_libraries (dnscrypt PRIVATE sodium sys ev PUBLIC dns)
if (WIN32)
	target_link_libraries (dnscrypt PRIVATE ws2_32)
endif()
# find_package (Threads)
# if (THREADS_FOUND)
#   target_link_libraries (dnscrypt PRIVATE Threads::Threads) # because of <atomic>
# endif()

add_exe (${PROJECT_NAME} srcz/dns-main.cpp) # installs in bin, should it be sbin?
target_link_libraries (${PROJECT_NAME} dnscrypt backtrace)

add_exe (cdrill srcz/cdrill.cpp)
target_link_libraries (cdrill dnscrypt dns backtrace)
