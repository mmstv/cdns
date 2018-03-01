cmake_minimum_required (VERSION 3.9)

add_1sec_test(csv_t1.cpp dnscrypt dns)
set_property (TEST csv_t1_dnscrypt APPEND PROPERTY ENVIRONMENT
	"DNS_CRYPT_TEST_FILE=${PROJECT_SOURCE_DIR}/data/dnscrypt-resolvers.csv")

add_1sec_test (csv_t0.cpp dnscrypt backtrace)
set_property (TEST csv_t0_dnscrypt APPEND PROPERTY ENVIRONMENT
	"DNS_CRYPT_TEST_FILE=${PROJECT_SOURCE_DIR}/data/dnscrypt-resolvers.csv")

add_1sec_test(arr_t0.cpp backtrace)
add_1sec_test(arr_t0_clang_fails.cpp backtrace)
if (CMAKE_SYSTEM_NAME MATCHES Linux AND CMAKE_CXX_COMPILER_ID MATCHES Clang
	AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "5.0"
	AND ((CMAKE_BUILD_TYPE MATCHES Release)
	OR (CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)))
	# test fails with Clang-3.9.1 on Linux-4.9 with -O2 compilation flag
	set_tests_properties(arr_t0_clang_fails_backtrace PROPERTIES WILL_FAIL TRUE)
endif()
add_1sec_test(err_t1.cpp  dns backtrace)
add_1sec_test(msg_t1.cpp  dns backtrace)
add_1sec_test(rnd_t1.cpp  dnscrypt backtrace sodium)
add_1sec_test(crypt_t3.cpp  dnscrypt dns backtrace)
add_1sec_test(crypt_t0.cpp  dnscrypt dns backtrace sodium)
add_1sec_test(crypt_t1.cpp  dnscrypt dns backtrace sodium)
add_1sec_test(crypt_t2.cpp  dnscrypt dns backtrace sodium)
add_1sec_test(addr_t1.cpp dnscrypt backtrace)
add_3sec_test(cert_t0.cpp dnscrypt backtrace)
add_3sec_test(cert_t1.cpp dnscrypt backtrace)
add_15sec_test (evtst1.cpp dnscrypt backtrace ev)
add_15sec_test (evudptst2.cpp dnscrypt backtrace ev)
add_1sec_test (evtst000.cpp ev) # very slow with wine under ctest
add_1sec_test (evtst001.cpp ev)
# 'select' in winsock2.h works only with socket handles, it can't be used for
# general file descriptor. File descriptors can't be converted to sockets
add_1sec_test (seltst0.cpp)
if (WIN32)
	target_link_libraries (evtst000_ev ws2_32)
	target_link_libraries (evtst001_ev ws2_32)
	target_link_libraries (seltst0_${PROJECT_NAME} ws2_32)
endif()
add_1sec_test(prov_t1.cpp  dnscrypt dns backtrace)
add_1sec_test(cache_t1.cpp  dns backtrace)
add_1sec_test(filter_t1.cpp  dns backtrace)
set_property (TEST filter_t1_dns APPEND PROPERTY ENVIRONMENT
	"DNS_TEST_DIR=${PROJECT_SOURCE_DIR}/data")

exe_timed_test (${PROJECT_NAME} version 4 -V)
exe_timed_test (${PROJECT_NAME} help 4 --help)
exe_timed_test (${PROJECT_NAME} fail 4 --Version)
set_tests_properties (${PROJECT_NAME}_fail PROPERTIES PASS_REGULAR_EXPRESSION
	"RUNTIME ERROR! Unknown option!")

add_executable (srvclitst srvcli-all.cpp)
find_package (Threads)
if (THREADS_FOUND)
	target_link_libraries (srvclitst Threads::Threads)
endif()
target_is_test (srvclitst)

foreach (proto udp tcp tcpudp)
	exe_timed_test (srvclitst ${proto} 8 $<TARGET_FILE:${PROJECT_NAME}> $<TARGET_FILE:cdrill>
		"${PROJECT_SOURCE_DIR}/data" "${CMAKE_CURRENT_SOURCE_DIR}/data" ${proto})
	set_property (TEST srvclitst_${proto} APPEND PROPERTY DEPENDS
		"evtst1_dnscrypt;evudptst2_dnscrypt")
	set_property (TEST srvclitst_${proto} APPEND PROPERTY ENVIRONMENT
		"${PROJECT_NAME}_TESTING_TIMEOUT=3")
endforeach()
unset (_p)
set_property (TEST srvclitst_tcp APPEND PROPERTY DEPENDS srvclitst_udp)
set_property (TEST srvclitst_tcpudp APPEND PROPERTY DEPENDS
	"srvclitst_udp;srvclitst_tcp")
