#undef NDEBUG
#include <cassert>
#include <iostream>
#include <cstring>

#include "network/socket.hxx"
#include "network/net_error.hxx"
#include "backtrace/catch.hxx"

#include "network/preconfig.h"
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#elif defined (_WIN32)
constexpr const int MSG_NOSIGNAL = 0;
#else
#error "need MSG_NOSIGNAL"
#endif

void ipv4_run()
{
	network::socket <network::proto::tcp> sock4 (network::inet::ipv4);
	std::cout << "TCP Sock4: " << sock4.to_string() << std::endl;
	assert (sock4.is_connected());
	sock4.unblock();
	sock4.reuse_listener_port();
	std::cout << "TCP Sock4 fd: " << sock4.file_descriptor() << std::endl;
	assert (sock4.is_connected());
	//! @todo: this will fail with MINGW, because socket is not bound to an address
	network::tcp::listen (sock4, 0);
	std::uint8_t msg[1000] = "Hello";
	ssize_t n = network::tcp::send (sock4, msg, 100, MSG_NOSIGNAL);
	int keep_err = network::get_errno();
	int err = sock4.self_error_code();
	std::cout << "TCP Sock4 sent: " << n << ", ERR: " << std::strerror (keep_err)
		<< ", " << keep_err << ", self err: " << err << std::endl;
	assert (0 > n);
	assert (EPIPE == keep_err);
	assert (0 == err);
	n = network::tcp::receive (sock4, msg, 200, MSG_NOSIGNAL);
	keep_err = network::get_errno();
	err = sock4.self_error_code();
	std::cout << "TCP Sock4 recv: " << n << ", ERR: " << std::strerror (keep_err)
		<< ", " << keep_err << ", self: " << err << std::endl;
	assert (0 > n);
	assert (ENOTCONN == keep_err);
	assert (0 == err);
	network::socket <network::proto::tcp> moved_sock (std::move (sock4));
	network::socket <network::proto::tcp> moved_a_sock;
	moved_a_sock = std::move (moved_sock);
	sock4.close();
}

void udp_ipv4_run()
{
	network::socket <network::proto::udp> sock4 (network::inet::ipv4);
	std::cout << "UDP Sock4: " << sock4.to_string() << std::endl;
	assert (sock4.is_connected());
	sock4.unblock();
	sock4.reuse_listener_port();
	std::cout << "UDP Sock4 fd: " << sock4.file_descriptor() << std::endl;
	assert (sock4.is_connected());
	const int err = sock4.self_error_code();
	std::cout << "UDP Sock4 " << ", ERR: " << err << std::endl;
	assert (0 == err);

	network::socket <network::proto::udp> moved_sock (std::move (sock4));
	network::socket <network::proto::udp> moved_a_sock;
	moved_a_sock = std::move (moved_sock);

	sock4.close();
}

void ipv6_run()
{
	network::socket <network::proto::tcp> sock6 (network::inet::ipv6);
	std::cout << "Sock6: " << sock6.to_string() << std::endl;
	assert (sock6.is_connected());
}

void run()
{
	ipv4_run();
	udp_ipv4_run();
	try
	{
		ipv6_run();
	}
	catch (const std::system_error &e)
	{
		std::cout << "ERR! " << e.what() << std::endl;
#ifndef _WIN32
		// on mingw network error codes are commented out in std::errc enum
		assert (e.code().value() == int (std::errc::address_family_not_supported));
#endif
	}
}

int main()
{
	return trace::catch_all_errors (run);
}
