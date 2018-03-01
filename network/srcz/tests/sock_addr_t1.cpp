#undef NDEBUG
#include <cassert>
#include <iostream>
#include <cstring>

#include "network/socket.hxx"
#include "network/net_error.hxx"
#include "network/address.hxx"
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
	network::address addr ("127.0.0.1", 65534);
	network::bind_listener (sock4, addr);
	network::tcp::listen (sock4, 0);
	const auto got_addr = sock4.get_address();
	std::cout << "TCP listen addr: " << got_addr.ip_port() << std::endl;
	assert (got_addr == addr);
	std::uint8_t msg[1000] = "Hello";
	network::socket <network::proto::tcp> send_sock (network::inet::ipv4);
	network::tcp::connect (send_sock, addr);
	ssize_t n = network::tcp::send (send_sock, msg, 100, MSG_NOSIGNAL);
	int keep_err = network::get_errno();
	int err = sock4.self_error_code();
	std::cout << "TCP Sock4 sent: " << n << ", ERR: " << std::strerror (keep_err)
		<< ", " << keep_err << ", self err: " << err << std::endl;
	assert (0 < n);
	assert (0 == keep_err);
	assert (0 == err);
	send_sock.close();
	auto conn = network::tcp::accept_new (sock4);
	std::cout << "Accepted from: " << conn.second.ip_port() << std::endl;
	assert (conn.first.is_connected());
	n = network::tcp::receive (conn.first, msg, 200, MSG_NOSIGNAL);
	keep_err = network::get_errno();
	err = sock4.self_error_code();
	std::cout << "TCP Sock4 recv: " << n << ", ERR: " << std::strerror (keep_err)
		<< ", " << keep_err << ", self: " << err << std::endl;
	assert (0 < n);
	assert (0 == keep_err);
	assert (0 == err);
	conn.first.close();
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
	network::address addr ("127.0.0.1", 65533);
	network::bind_listener (sock4, addr);
	const auto got_addr = sock4.get_address();
	std::cout << "UDP listen addr: " << got_addr.ip_port() << std::endl;
	assert (got_addr == addr);
	std::uint8_t msg[1000] = "Hello";
	network::socket <network::proto::udp> send_sock (network::inet::ipv4);
	ssize_t n = network::udp::send_to (send_sock, msg, 100, 0, addr);
	int keep = network::get_errno();
	std::cout << "UDP sent: " << n << ", " << std::strerror (keep) << ", err: "
		<< network::get_errno() << std::endl;
	assert (100 == n);
	assert (0 == keep);
	send_sock.close();
	network::address recv_addr;
	n = network::udp::receive_from (sock4, msg, 200, 0, &recv_addr);
	keep = network::get_errno();
	std::cout << "UDP received: " << n << ", " << std::strerror (keep) << ", err: "
		<< network::get_errno() << std::endl;
	assert (100 == n);
	assert (0 == keep);
}

void run()
{
	ipv4_run();
	udp_ipv4_run();
	std::cout << "\nOkOk!" << std::endl;
}

int main()
{
	return trace::catch_all_errors (run);
}
