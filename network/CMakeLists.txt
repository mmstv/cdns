cmake_minimum_required (VERSION 3.9)

include (CheckCXXSymbolExists)
include (CheckTypeSize)
include (CheckStructHasMember)

set (headers fcntl.h unistd.h sys/ioctl.h sys/socket.h arpa/inet.h sys/stat.h)

if (WIN32)
	list (APPEND headers winsock2.h ws2tcpip.h)
endif()

foreach (head ${headers})
	string(REPLACE "/" "_" uhead "${head}")
	string(REPLACE "." "_" uhead "${uhead}")
	string(TOUPPER "${uhead}" uhead)
	check_include_file_cxx(${head} HAVE_${uhead})
endforeach()

check_cxx_symbol_exists(F_SETFD fcntl.h HAVE_SETFD)

unset(CMAKE_EXTRA_INCLUDE_FILES)

if(HAVE_SYS_SOCKET_H)
	set(CMAKE_EXTRA_INCLUDE_FILES "sys/socket.h")
endif()
if (HAVE_ARPA_INET_H)
	list(APPEND CMAKE_EXTRA_INCLUDE_FILES "arpa/inet.h")
endif()
if (HAVE_FCNTL_H)
	list(APPEND CMAKE_EXTRA_INCLUDE_FILES "fcntl.h")
endif()
if (HAVE_WINSOCK2_H)
	list(APPEND CMAKE_EXTRA_INCLUDE_FILES "winsock2.h")
endif()
if (HAVE_WS2TCPIP_H)
	list(APPEND CMAKE_EXTRA_INCLUDE_FILES "ws2tcpip.h")
endif()

set (typs "struct in6_addr" "struct sockaddr_in6" "struct sockaddr_storage")
foreach(typ ${typs})
	string(REPLACE " " "_" utyp "${typ}")
	string(TOUPPER "${utyp}" utyp)
	check_type_size(${typ} ${utyp} LANGUAGE CXX)
endforeach()

if (NOT HAVE_STRUCT_SOCKADDR_STORAGE)
	message (FATAL_ERROR "Need sockaddr_storage")
endif()
if (NOT HAVE_STRUCT_SOCKADDR_IN6)
	message (FATAL_ERROR "Need sockaddr_in6")
endif()
if (NOT HAVE_STRUCT_IN6_ADDR)
	message (FATAL_ERROR "Need in6_addr")
endif()

check_struct_has_member("struct sockaddr_in" sin_len "${CMAKE_EXTRA_INCLUDE_FILES}"
	HAVE_STRUCT_SOCKADDR_IN_SIN_LEN LANGUAGE CXX)
check_struct_has_member("struct sockaddr_in6" sin6_len "${CMAKE_EXTRA_INCLUDE_FILES}"
	HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN LANGUAGE CXX)
check_struct_has_member("struct sockaddr_storage" ss_family
	"${CMAKE_EXTRA_INCLUDE_FILES}" HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY LANGUAGE
	CXX)
if (NOT HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY)
	message (FATAL_ERROR "Need sockaddr_storage.ss_family")
endif()

if (WIN32)
	set (CMAKE_REQUIRED_LIBRARIES ws2_32)
endif()

set (funcs fcntl inet_ntop inet_pton)
foreach(func ${funcs})
	string(TOUPPER "${func}" ufunc)
	check_cxx_symbol_exists(${func} "${CMAKE_EXTRA_INCLUDE_FILES}" HAVE_${ufunc})
endforeach()
unset (CMAKE_EXTRA_INCLUDE_FILES)
unset (CMAKE_REQUIRED_LIBRARIES)
if (NOT (HAVE_INET_NTOP AND HAVE_INET_PTON))
	message (FATAL_ERROR "Need inet_ntop and inet_pton")
endif()

configure_file(net_config.h.in "${PROJECT_BINARY_DIR}/include/network/net_config.h")
add_definitions (-DHAVE_CONFIG_H)

set(interface
	address.hxx
	connection.hxx
	provider.hxx
	packet.hxx
)

set(sources
	srcz/address.cpp
	srcz/timeout.cpp
	srcz/connection.cpp
	srcz/socket.cpp
	srcz/provider.cpp
	srcz/upstream.cpp
	srcz/listener.cpp
	srcz/incoming.cpp
	srcz/net_error.cpp
	srcz/packet.cpp
)

add_shared_lib (network interface sources)
target_include_directories(network PUBLIC
	"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_link_libraries(network PRIVATE sys backtrace ev)
# target_include_directories (network PUBLIC ev)

if (WIN32)
	target_link_libraries (network PUBLIC ws2_32) # PRIVATE
endif()

if (BUILD_TESTING)
	add_1sec_test (srcz/tests/pkt_t1.cpp network backtrace)
	add_1sec_test (srcz/tests/addr_t0.cpp network backtrace)
	add_1sec_test (srcz/tests/sock_t0.cpp network backtrace)
	if (WIN32)
		# fails because ::listen called on non-bound socket
		# probably should fail on Linux as well
		set (_e "SYSTEM ERROR! \\(network\\) Failed to listen on TCP socket:")
		set (_e "${_e} Invalid argument \\[WSAEINVAL\\]")
		set_tests_properties (sock_t0_network PROPERTIES PASS_REGULAR_EXPRESSION
			"${_e}")
		unset (_e)
	endif()
	add_1sec_test (srcz/tests/sock_addr_t1.cpp network backtrace)
endif()
