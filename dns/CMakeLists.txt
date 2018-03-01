cmake_minimum_required (VERSION 3.9)

set (pkgdatadir "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}")
add_definitions (-DHAVE_CONFIG_H -DPKGDATADIR="${pkgdatadir}")

set (interface
	query.hxx
	constants.hxx
	fwd.hxx
	filter.hxx
	hosts.hxx
	daemon.hxx
	"cache.hxx"
	responder.hxx
)

set (sources
	srcz/query.cpp
	srcz/filter.cpp
	"srcz/cache.cpp"
	srcz/hosts.cpp
	srcz/daemon.cpp
	srcz/options.cpp
	srcz/responder.cpp
)

add_shared_lib(dns interface sources)
target_include_directories(dns PUBLIC
	"$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>")

target_link_libraries (dns PUBLIC network sys ev PRIVATE backtrace)

if (WIN32)
	target_link_libraries (dns PUBLIC ws2_32)
endif()
add_exe (dns-example "srcz/dns-example.cpp")
target_link_libraries (dns-example dns backtrace)

if (BUILD_TESTING)
	add_1sec_test (srcz/tests/resolv_t1.cpp dns backtrace)
	add_1sec_test (srcz/tests/hosts_t1.cpp dns backtrace)
	set_property (TEST hosts_t1_dns APPEND PROPERTY ENVIRONMENT
		"DNS_TEST_FILE=${PROJECT_SOURCE_DIR}/data/hosts")
	check_include_file_cxx ("ldns/ldns.h" HAVE_LDNS_LDNS_H)
	if (HAVE_LDNS_LDNS_H)
		check_library_exists (ldns ldns_wire2pkt "" HAVE_LDNS)
	endif()
	if (NOT HAVE_LDNS)
		message (FATAL_ERROR "need ldns")
	endif()
	add_1sec_test (srcz/tests/msg_ldns_t0.cpp dns backtrace ldns)
	add_1sec_test (srcz/tests/msg_t2.cpp dns backtrace)
endif()
