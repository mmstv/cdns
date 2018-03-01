#include <stdexcept>
#include <cerrno>
#include <string>
#include <cassert>
#include <sstream>
#include <system_error>
#include <limits>

#include "network/connection.hxx"
#include "network/packet.hxx"
#include "network/net_error.hxx"
#include "network/tcp/listener.hxx"
#include "sys/logger.hxx"

#include "network/preconfig.h"
// for htons and MSG_NOSIGNAL
#if defined (HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#elif defined (HAVE_WINSOCK2_H)
#include <winsock2.h>
#else
#error "need htons"
#endif

namespace network {

static_assert (std::is_move_assignable <tcp_connection>::value, "move");
static_assert (!std::is_nothrow_move_assignable <tcp_connection>::value, "move");
static_assert (std::is_nothrow_move_constructible <tcp_connection>::value, "move");
static_assert (!std::is_copy_constructible <tcp_connection>::value, "no copy");
static_assert (!std::is_copy_assignable <tcp_connection>::value, "no copy");

void udp_connection::udp_send_to(const packet &message,
	const class address &_address) const
{
	assert (0u < message.size());
	constexpr const int flags = 0;
	::ssize_t n = udp::send_to (this->udp_socket(), message.bytes(), message.size(),
		flags, _address);
	if( n < 0L )
	{
		const int send_errno = get_errno();
		std::ostringstream ostr;
		ostr << "Failed send[UDP] " << message.size() << " bytes to: " << _address;
		throw net_error (send_errno, ostr.str());
	}
	if( 0L == n )
		throw net_error (0, "Connection reset by peer? [UDP send]");
	// ATTN can't shutdown here, because socket may be a listener
	// still do not understand how ::shutdown works
}

void udp_connection::udp_receive (packet &message, class address *_address) const
{
	if (_address == &(this->address()))
		throw std::logic_error ("UDP recvfrom for self-address");
	assert (0u == message.size());
	assert (0u < message.reserved_size());
	constexpr const int flags = 0; // MSG_WAITALL; // MSG_PEEK;
	const ssize_t n = udp::receive_from (this->udp_socket(), message.modify_bytes(),
		message.reserved_size(), flags, _address);
	if( 0 > n )
		throw net_error (get_errno(), "Failed to receive[UDP]");
	if( 0 == n )
		throw net_error (0, "Connection reset by peer? [UDP recv]");
	assert (std::numeric_limits <packet::size_type>::max() >= n);
	message.set_size (static_cast <packet::size_type> (n));
	//! @todo what is this:
#if 0
	if (nullptr == _address)
	{
		if (this->address() != *_address)
			throw std::logic_error ("UDP recvfrom wrong address");
	}
	else
		*_address = std::move (taddr);
#endif
}

void tcp_connection::tcp_send(const packet &message) const
{
	assert (0u < message.size());
	packet q;
	// prepending TCP message with its total size, because it could be delivered
	// in pieces. The receiver needs to know how many pieces to wait for.
	// See tcp_receive
	//! @todo fix message size type, use numeric_cast
	const std::uint16_t sz = htons(static_cast<std::uint16_t>(message.size()));
	q.append(reinterpret_cast<const std::uint8_t *>(&sz), 2U);
	q.append(message);
	assert (q.size() == 2u + message.size());
	constexpr const int flags =
#if defined (__linux__) || defined (MSG_NOSIGNAL)
		MSG_NOSIGNAL; // | MSG_EOR; // do not trigger SIGPIPE;
#else
		0; // Windows has MSG_INTERRUPT, MSG_PARTIAL
#endif
	const ::ssize_t n = tcp::send (this->tcp_socket(), q.bytes(), q.size(), flags);
	if( n < 0L )
	{
		const auto sock_err = get_errno();
		std::ostringstream ostr;
		ostr << "Failed to TCP-send: " << message.size() << " bytes, on socket: "
			<< this->sys_socket().to_string();
		throw net_error (sock_err, ostr.str());
	}
	if (0 == n)
		throw net_error (0, "connection[TCP,send] reset by peer?");
	// can't shutdown here
	//! @todo implement bufferred send
	if( q.size() != static_cast <std::size_t> (n) )
		throw std::logic_error("buffered TCP is not supported");
}

void tcp_connection::tcp_receive(packet &message) const
{
	assert( 0U == message.size() );
	assert (0u < message.reserved_size());
	packet q;
	q.reserve (message.reserved_size());
	//! @todo implement bufferred receive
	constexpr const int flags = 0; // MSG_WAITALL; // MSG_PEEK;
	::ssize_t n = tcp::receive (this->tcp_socket(), q.modify_bytes(),
		q.reserved_size(), flags);
	if( n < 0L )
		throw net_error (get_errno(), "Failed to receive[TCP]");
	if (0L == n)
		// socket may be a listener, so can't shutdown here
		throw net_error (this->self_error_code(), "No data in TCP stream");
	if( n < 2L )
		throw std::runtime_error("not enough data in TCP message");
	// first two bytes contain total TCP message size, introduced by tcp_send
	const auto *sz = reinterpret_cast <const std::uint16_t *>(q.bytes());
	const std::uint16_t msg_size = ntohs (*sz);
	if (0 == msg_size)
		throw std::runtime_error ("zero length TCP message");
	const unsigned pkt_size = msg_size + 2U;
	if (std::numeric_limits <packet::size_type>::max() < pkt_size)
		throw std::runtime_error ("Too large TCP message");
	const auto tcp_n = static_cast <packet::size_type> (pkt_size);
	if (tcp_n > q.reserved_size())
		throw std::runtime_error ("too long TCP message");
	//! @todo implement bufferred input, to allow for other events between ::recv
	while (n < tcp_n)
	{
		//! @todo numeric_cast
		ssize_t n2 = tcp::receive (this->tcp_socket(), q.modify_bytes() + n,
			static_cast <packet::size_type> (q.reserved_size() - static_cast
				<std::size_t> (n)), flags);
		//! @todo error could be because socket is not ready to rceive
		//! @todo !!! implement bufferred input
		if( n2 <= 0)
			throw net_error (get_errno(), "tcp receive failure");
		n += n2;
		process::log::debug ("recv again: ", n2, ", expected: ", tcp_n, ", total: ",
			n);
	}
	if (tcp_n != n)
	{
		std::ostringstream ostr;
		ostr << "TCP message size mismatch, received: " << n << " != " << tcp_n;
		throw std::logic_error (ostr.str());
	}
	assert (std::numeric_limits <packet::size_type>::max() >= n);
	q.set_size (static_cast <packet::size_type> (n));
	//! @todo fix size type, use numeric_cast?
	message.append (q.bytes()+2L, static_cast <packet::size_type> (q.size()-2U));
}

void tcp_connection::tcp_connect() const
{
	tcp::connect (this->tcp_socket(), this->address());
}

void tcp_connection::tcp_listen() const
{
	const int backlog = 0; //! @todo what is this
	tcp::listen (this->tcp_socket(), backlog);
}

std::string abs_connection::to_string () const
{
	return this->address().ip_port() + " #" + this->sys_socket().to_string ();
}

tcp_connection tcp_connection::accept_new (const tcp::tcplistener &lstn)
{
	auto sa = tcp::accept_new (lstn.tcp_socket());
	return tcp_connection (std::move (sa.first), std::move (sa.second));
}

} // namespace network
