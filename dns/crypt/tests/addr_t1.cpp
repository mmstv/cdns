#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cassert>
#include <cstring>

#include "backtrace/catch.hxx"
#include "network/address.hxx"

#include "network/net_config.h"
#if defined (HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif
#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif
#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

using namespace std;

constexpr const char nl = '\n';
constexpr const char caddr[] = "127.0.0.1:55";
static int run()
{
	network::address addr (caddr, 0);
	const auto ss = addr.af_family();
	const auto sa = addr.addr()->sa_family;
	cout << "addr: " << addr.ip() << ", port: " << addr.port()
		<< ", ss family: " << ss
		<< ", sa family: " << sa
		<< "\nINET=" << AF_INET << ", INET6=" << AF_INET6
		<< "\nip-port: " << addr.ip_port()
		<< "\nlen: " << addr.len()
		<< nl;

	assert( "127.0.0.1" == addr.ip() );
	assert( 55 == addr.port() );
	assert( AF_INET6 != AF_INET );
	assert( ss == sa );
	assert( 0 != ss );
	assert( AF_INET == ss );
	assert( caddr == addr.ip_port() );
	assert (addr.len() >= sizeof (sockaddr));
	assert (addr.len() <= sizeof (sockaddr_storage));
	return 0;
}

int main()
{
	return trace::catch_all_errors(run);
}
