#include <string>
#include <cassert>

#include "network/socket.hxx"
#include "network/address.hxx"
#include "network/net_error.hxx"
#include "network/preconfig.h"

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> // for getsockopt
#elif defined (HAVE_WINSOCK2_H)
#include <winsock2.h> // for getsockopt
#else
#error "need ::getsockopt"
#endif
#if defined (HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#if !defined (HAVE_FCNTL) && !defined (_WIN32)
#error "UNIX has fcntl"
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
// to disable crypt function, which conflicts with crypt namespace
#undef _GNU_SOURCE
#undef __USE_XOPEN
#include <unistd.h> // for ::close
#elif defined (_WIN32)
#include <io.h>
#else
#error "need ::close"
#endif

#ifndef HAVE_SYS_STAT_H
#error "need ::fstat"
#endif
#include <sys/stat.h> // for ::fstat

namespace network {

static_assert (std::is_move_assignable <socket <proto::tcp>>::value, "move");
static_assert (!std::is_nothrow_move_assignable <socket <proto::tcp>>::value, "");
static_assert (std::is_nothrow_move_constructible <socket <proto::tcp>>::value, "");
static_assert (!std::is_copy_constructible <socket <proto::tcp>>::value, "no copy");
static_assert (!std::is_copy_assignable <socket <proto::tcp>>::value, "no copy");
static_assert (!std::is_constructible <system::socket>::value, "abstract");
static_assert (!std::is_destructible <system::socket>::value, "abstract");
static_assert (std::is_final <tcp::socket_t>::value, "final");

#ifdef _WIN32
static_assert (std::is_same <SOCKET, system::socket_handle_t>::value, "socket");
#endif
#ifdef _WIN64
static_assert (std::is_same <SOCKET, unsigned long long>::value, "socket");
#elif defined (_WIN32)
static_assert (std::is_same <SOCKET, unsigned>::value, "socket");
#endif

namespace detail
{

class socket_store
{
public:
	system::socket_handle_t socket_;
#ifdef _WIN32
	int file_descriptor_;
#endif
};

static_assert (sizeof (socket_store) == sizeof (network::socket <proto::tcp>), "");
static_assert (sizeof (network::socket <proto::tcp>) <= 2 * sizeof (long long), "");

template <proto TcpUdp>
inline socket_store &cast (socket <TcpUdp> &s) noexcept
{
	return reinterpret_cast <socket_store&> (s);
}

template <proto TcpUdp>
inline const socket_store &c_cast (const socket <TcpUdp> &s) noexcept
{
	return reinterpret_cast <const socket_store&> (s);
}

inline int af_family (inet af)
{
	return (inet::ipv6 == af) ? AF_INET6 : AF_INET;
}

inline bool is_valid (system::socket_handle_t s) noexcept
{
	return
#ifdef _WIN32
		INVALID_SOCKET != s;
#else
		0 <= s;
#endif
}

} // namespace detail

#if !defined (_WiN32) && !defined (INVALID_SOCKET)
constexpr const int INVALID_SOCKET = -1;
#endif

namespace system
{

socket::socket () noexcept : socket_ (INVALID_SOCKET)
#ifdef _WIN32
	, file_descriptor_ (-1)
#endif
{}

extern socket::socket (socket &&other) noexcept : socket_ (std::move (other.socket_))
#ifdef _WIN32
	, file_descriptor_ (std::move (other.file_descriptor_))
#endif
{
	other.socket_ = INVALID_SOCKET;
#ifdef _WIN32
	other.file_descriptor_ = -1;
#endif
}

socket &socket::operator= (socket &&other)
{
	this->close();
	new (this) socket (std::move (other)); //! @todo is this safe?
	return *this;
}

void socket::close()
{
	auto &self = *this;
	if (detail::is_valid (self.handle()))
	{
#ifdef _WIN32
		// also closes self.file_descriptor_
		const int r = ::closesocket (self.handle());
		self.file_descriptor_ = -1;
#else
		const int r = ::close (self.handle());
#endif
		self.socket_ = INVALID_SOCKET;
		if (0 != r)
			throw error (get_errno(), "Can't close network connection");
	}
	assert (!detail::is_valid (self.handle()));
}

int socket::self_error_code() const
{
	const auto &self = *this;
	union {
		int e = 0;
		char ch[sizeof (int)]; // Windows wants char*, Linux void*
	};
	socklen_t elen = sizeof (e);
	if (0 == ::getsockopt (self.handle(), SOL_SOCKET, SO_ERROR, ch, &elen))
		return e;
	throw std::system_error (get_errno(), error_category(), "Could not get network "
		"error code");
}

void socket::unblock() const
{
	const auto &self = *this;
#ifdef _WIN32
	{
		u_long nonblocking = 1;
		if (ioctlsocket (self.handle(), FIONBIO, &nonblocking) == SOCKET_ERROR)
			throw net_error (get_errno(), "can't unblock network socket");
	}
#elif defined(FIONBIO)
	{
		int nonblocking = 1;
		if (ioctl (self.handle(), FIONBIO, &nonblocking) != 0)
			throw net_error ("can't unblock network socket", errno);
	}
#else
	// Linux
	{
		// ATTN! fcntl might crash if socket is closed
		const int issock = isfdtype (self.handle(), S_IFSOCK);
		if (0 == issock)
			throw std::logic_error ("socket is not a socket");
		else if (0 > issock)
			throw std::system_error (errno, std::system_category(), "Bad socket");
		const int flags = fcntl (self.handle(), F_GETFL, nullptr);
		if (flags < 0)
			throw net_error (get_errno(), "can't unblock network socket");
		if (fcntl (self.handle(), F_SETFL, flags | O_NONBLOCK) == -1)
			throw net_error (get_errno(), "can't unblock network socket");
	}
#endif
}

//! @todo this makes sense only for TCP connection
bool socket::is_connected() const
{
	const auto &self = *this;
	if (!detail::is_valid (self.handle()))
		throw std::logic_error ("network connection is already closed");

	const int e = this->self_error_code();
	if (0 != e)
	{
#ifdef _WIN32
		if (WSAEWOULDBLOCK == e || WSAEINPROGRESS == e)
			return false;
#else
		if (EINTR == e || EINPROGRESS == e)
			return false;
#endif
		throw net_error (e, "socket is in failed state");
	}
	return true;
}

void socket::reuse_listener_port () const
{
#ifdef __linux__
	const auto &self = *this;
	int o = 1;
	/* REUSEPORT on Linux 3.9+ means, "Multiple servers (processes or
	 * threads) can bind to the same port if they each set the option. */
	if (0 != ::setsockopt (self.handle(), SOL_SOCKET, SO_REUSEPORT, &o, sizeof (o)))
		throw net_error (get_errno(), "Can't reuse network port");
#endif
}

int socket::file_descriptor () const noexcept
{
	return this->
#ifdef _WIN32
		file_descriptor_;
#else
		socket_;
#endif
}

std::string socket::to_string () const
{
	return std::to_string (this->handle());
}

void socket::bind_listener (const address &a) const
{
	assert (detail::is_valid (this->handle()));
	if (0 != ::bind (this->handle(), a.addr(), a.len()))
		throw net_error (get_errno(), "Failed to bind to " + a.ip_port() + " #"
			+ this->to_string ());
}

address socket::get_address () const
{
	if (!detail::is_valid (this->handle()))
		throw std::logic_error ("Invalid socket");
	struct sockaddr_storage laddr;
	socklen_t len = sizeof (laddr);
	std::memset (&laddr, 0, sizeof laddr);
	static_assert (sizeof (laddr) >= sizeof (sockaddr), "size");
	if (0 != ::getsockname (this->handle(), reinterpret_cast <sockaddr*> (&laddr),
			&len))
		throw error (get_errno(), "Failed to get internet IP address from socket");
	assert (0 < len && sizeof (sockaddr_storage) >= len);
	return address (&laddr, static_cast <unsigned short> (len));
}

inline socket::socket (inet af, int type, int protocol)
{
	auto &self = *this;
	self.socket_ = ::socket (detail::af_family (af), type, protocol);
	auto sock_err = get_errno();
	if (!detail::is_valid (self.handle()))
		throw error (sock_err, "Failed to create network socket");
#ifdef _WIN32
	static_assert (sizeof (SOCKET) == sizeof (std::intptr_t), "size");
	self.file_descriptor_ = _open_osfhandle (static_cast <std::intptr_t>
		(self.handle()), 0);
	//! @todo check of error
#endif
}

} // namespace network::system

template <proto TcpUdp>
socket <TcpUdp>::socket (inet af) : system::socket (af,
	((proto::udp == TcpUdp)?SOCK_DGRAM:SOCK_STREAM),
	((proto::udp == TcpUdp)?IPPROTO_UDP:IPPROTO_TCP))
{}

namespace detail
{

#if 0
void tcp_tune(evutil_socket_t handle)
{
	if (handle == -1)
		return;
#ifdef TCP_QUICKACK
	setsockopt(handle, IPPROTO_TCP, TCP_QUICKACK, (void *) (int []) { 1 }, sizeof
		(int));
#else
	setsockopt(handle, IPPROTO_TCP, TCP_NODELAY, (void *) (int []) { 1 }, sizeof
		(int));
#endif
}

setsockopt(sock, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
#endif

#if 0
// not used
void socket::reuse_listener_address() const
{
#ifndef _WIN32
	const int o = 1;
	/* REUSEADDR on Unix means, "don't hang on to this address after the
	 * listener is closed."  On Windows, though, it means "don't keep other
	 * processes from binding to this address while we're using it. */
	if (0 != ::setsockopt (this->socket(), SOL_SOCKET, SO_REUSEADDR, &o, sizeof (o)))
		throw net_error ("Can't reuse network address", errno);
#endif
}

//! @todo: what is this
void socket::closeonexec (void) const
{
#if !defined(_WIN32)
# ifdef FIOCLEX
	if (0 != ::ioctl (this->socket(), FIOCLEX, NULL))
		throw std::sytem_error (errno, std::system_category());
# elif defined (HAVE_SETFD)
	if (0 > ::fcntl (this->socket(), F_GETFD, NULL))
		throw std::sytem_error (errno, std::system_category());
	if (-1 == ::fcntl (this->socket(), F_SETFD, flags | FD_CLOEXEC))
		throw std::sytem_error (errno, std::system_category());
# endif
#endif
}
#endif
} // mamespace detail

namespace udp
{
ssize_t send_to (const socket <proto::udp> &s, const std::uint8_t *msg,
	unsigned short msglen, int flags, const address &a)
{
	assert (0u < msglen);
	assert (detail::is_valid (detail::c_cast (s).socket_));
	return ::sendto (detail::c_cast (s).socket_,
#ifdef _WIN32
		reinterpret_cast <const char*> (msg),
#else
		msg,
#endif
		msglen, flags, a.addr(), a.len());
}

ssize_t receive_from (const socket <proto::udp> &s, std::uint8_t *buf,
	unsigned short bufsize, int flags, address *_address)
{
	assert (0u < bufsize);
	assert (detail::is_valid (detail::c_cast (s).socket_));
	static_assert (sizeof (sockaddr_storage) >= sizeof (sockaddr), "size");
	struct sockaddr_storage caddr;
	::socklen_t clen = sizeof (caddr);
	const ::ssize_t n = ::recvfrom (detail::c_cast (s).socket_,
#ifdef _WIN32
		reinterpret_cast <char*> (buf),
#else
		buf,
#endif
		bufsize, flags, reinterpret_cast <sockaddr*> (&caddr), &clen);
	if (nullptr != _address && 0 < n)
	{
		if (0 < clen)
			*_address = address (&caddr, static_cast <unsigned short > (clen));
		else
			throw std::logic_error ("UDP address not received");
	}
	return n;
}
} // namespace network::udp

namespace tcp
{
ssize_t send (const socket <proto::tcp> &s, const std::uint8_t *msg,
	unsigned short msglen, const int flags)
{
	assert (0u < msglen);
	assert (detail::is_valid (detail::c_cast (s).socket_));
	return ::send (detail::c_cast (s).socket_,
#ifdef _WIN32
		reinterpret_cast <const char*> (msg),
#else
		msg,
#endif
		msglen, flags);
}

ssize_t receive (const socket <proto::tcp> &s, std::uint8_t *buf,
	unsigned short bufsize, const int flags)
{
	assert (0u < bufsize);
	assert (detail::is_valid (detail::c_cast (s).socket_));
	return ::recv (detail::c_cast (s).socket_,
#ifdef _WIN32
		reinterpret_cast <char*> (buf),
#else
		buf,
#endif
		bufsize, flags);
}

void connect (const socket <proto::tcp> &s, const address &a)
{
	assert (detail::is_valid (detail::c_cast (s).socket_));
	const int r = ::connect (detail::c_cast (s).socket_, a.addr(), a.len());
	if (0 != r)
	{
		const auto sock_err = get_errno();
		if (sock_err !=
#ifdef _WIN32
			WSAEWOULDBLOCK)
#else
			EINPROGRESS)
#endif
			throw net_error (sock_err, "Failed to connect[TCP] to " + a.ip_port());
	}
}

void listen (const socket <proto::tcp> &s, int backlog)
{
	assert (detail::is_valid (detail::c_cast (s).socket_));
	if (0 != ::listen (detail::c_cast (s).socket_, backlog))
		throw net_error (get_errno(), "Failed to listen on TCP socket");
}

std::pair <socket <proto::tcp>, address>
accept_new (const socket <proto::tcp> &tcp_listener_sock)
{
	static_assert (sizeof (sockaddr) == sizeof (sockaddr_in), "size");
	static_assert (sizeof (sockaddr_storage) >= sizeof (sockaddr), "size");
	static_assert (sizeof (sockaddr_storage) >= sizeof (sockaddr_in6), "size");
	assert (detail::is_valid (detail::c_cast (tcp_listener_sock).socket_));
	struct sockaddr_storage caddr;
	::socklen_t clen = sizeof(caddr);
	std::memset (&caddr, 0, sizeof (caddr));
	const auto accepted = ::accept (detail::c_cast (tcp_listener_sock).socket_,
		reinterpret_cast <sockaddr*> (&caddr), &clen);
	if (!detail::is_valid (accepted))
		throw net_error (get_errno(), "Failed to accept[TCP] on "
			+ tcp_listener_sock.to_string());
	struct stat sbuf;
	socket <proto::tcp> sock;
	detail::cast (sock).socket_ = accepted;
#ifdef _WIN32
	detail::cast (sock).file_descriptor_ = _open_osfhandle (static_cast
		<std::intptr_t> (accepted), 0);
	//! @todo: check for error
#endif
	//! @todo this is for user access control, makes sense only for TCP
	if (0 != ::fstat (sock.file_descriptor(), &sbuf))
		throw std::system_error (errno, std::system_category(), "Bad file "
			"descriptor");
	return std::make_pair (std::move (sock), network::address (&caddr,
			static_cast <unsigned short> (clen)));
}

} // namespace netowork::tcp

template <proto Proto>
void bind_listener (const socket <Proto> &s, const address &a)
{
	constexpr const char *const strproto = (proto::tcp == Proto)?"[TCP]: ":"[UDP]: ";
	assert (detail::is_valid (detail::c_cast (s).socket_));
	if (0 != ::bind (detail::c_cast (s).socket_, a.addr(), a.len()))
		throw net_error (get_errno(), "Failed to bind to " + std::string (strproto)
			+ a.ip_port() + " #" + s.to_string ());
}

template <proto Proto> socket <Proto> bound_listener (system::socket_handle_t handle)
{
	assert (detail::is_valid (handle));
#ifndef _WIN32
	// linux only?
	constexpr const char *const str = (proto::tcp == Proto)?"[TCP]: ":"[UDP]: ";
		// ATTN! fcntl might crash if socket is closed
	const int issock = isfdtype (handle, S_IFSOCK);
	if (0 == issock)
		throw std::logic_error ("socket is not a socket");
	else if (0 > issock)
		throw std::system_error (errno, std::system_category(), std::string (str));
#endif
	socket <Proto> sock;
	detail::cast (sock).socket_ = handle;
#ifdef _WIN32
	detail::cast (sock).file_descriptor_ = _open_osfhandle (static_cast
		<std::intptr_t> (handle), 0);
#endif
	return sock;
}

// explicit instantiations
template socket <proto::udp>::socket (inet);
template socket <proto::tcp>::socket (inet);
template void NETWORK_API bind_listener <proto::udp> (const socket <proto::udp>&,
	const address&);
template void NETWORK_API bind_listener <proto::tcp> (const socket <proto::tcp>&,
	const address&);
template socket <proto::udp> NETWORK_API bound_listener <proto::udp>
	(system::socket_handle_t handle);
template socket <proto::tcp> NETWORK_API bound_listener <proto::tcp>
	(system::socket_handle_t handle);

} // namespace network
