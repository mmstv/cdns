#include <cstdlib>
#include <cstring>
#include <cassert>
#include <sstream>
#include <system_error>
#include <array>
#include <limits>
#include <climits>
#include <type_traits>

#include "network/address.hxx"
#include "sys/logger.hxx"
#include "backtrace/backtrace.hxx"

#include "network/preconfig.h"

#if defined (HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#endif

#if defined (HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif

#ifdef HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#ifndef AF_INET6
#error "INET6 is missing"
#endif

#if defined (HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN) && defined (__linux__)
#error "sin6_len is present on linux: unexpected"
#endif

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE
#error "sockaddr_storage is missing"
#endif

#ifndef HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY
#error "sockaddr_storage.ss_family is missing"
#endif

namespace network {

static_assert (std::is_nothrow_move_assignable <address>::value, "move");
static_assert (std::is_nothrow_move_constructible <address>::value, "move");
static_assert (std::is_nothrow_copy_constructible <address>::value, "nothrow copy");
static_assert (std::is_nothrow_copy_assignable <address>::value, "nothrow copy");

namespace detail {

class address_storage
{
public:
	struct sockaddr_storage storage_;
	unsigned short length_;
};

static_assert (sizeof (address_storage) <= sizeof (address), "size");
static_assert (sizeof (address_storage) <= sizeof (struct sockaddr_storage) + 8, "");
static_assert (alignof (address_storage) >= alignof (struct sockaddr_storage), "");
static_assert (alignof (address) >= alignof (struct sockaddr_storage), "align");
static_assert (alignof (address_storage) == alignof (address), "align");
static_assert (8 == alignof (struct sockaddr_storage), "align");

inline address_storage &cast (address *a)
{
	return *reinterpret_cast <address_storage*> (a);
}

inline const address_storage &c_cast (const address *a)
{
	return *reinterpret_cast <const address_storage*> (a);
}

template <inet Inet> struct addr
{
	typedef sockaddr_in a_t;
	static constexpr const int family = AF_INET;
	static_assert (sizeof (sockaddr_storage) >= sizeof (sockaddr_in), "");
	static in_addr *a (a_t &sin) {return &sin.sin_addr;}
#ifdef _WIN32
	static in_addr *no_const_a (const a_t &sin)
	{
		return const_cast <in_addr*> (&sin.sin_addr);
	}
#else
	static const in_addr *no_const_a (const a_t &sin) {return &sin.sin_addr;}
#endif
	static const a_t &ap (const sockaddr_storage &sa)
	{
		return reinterpret_cast <const a_t &> (sa);
	}
	static a_t &ap (sockaddr_storage &sa)
	{
		return reinterpret_cast <a_t &> (sa);
	}
	static std::uint16_t &port (a_t &s) {return s.sin_port;}
	static std::uint16_t port (const a_t &s) {return s.sin_port;}
};

template <> struct addr <inet::ipv6>
{
	typedef sockaddr_in6 a_t;
	static constexpr const int family = AF_INET6;
	static_assert (sizeof (sockaddr_storage) >= sizeof (sockaddr_in6), "");
	static in6_addr *a (a_t &sin) {return &sin.sin6_addr;}
#ifdef _WIN32
	static in6_addr *no_const_a (const a_t &sin)
	{
		return const_cast <in6_addr*> (&sin.sin6_addr);
	}
#else
	static const in6_addr *no_const_a (const a_t &sin) {return &sin.sin6_addr;}
#endif
	static const a_t &ap (const sockaddr_storage &sa)
	{
		return reinterpret_cast <const a_t &> (sa);
	}
	static a_t &ap (sockaddr_storage &sa)
	{
		return reinterpret_cast <a_t &> (sa);
	}
	static std::uint16_t &port (a_t &s) {return s.sin6_port;}
	static std::uint16_t port (const a_t &s) {return s.sin6_port;}
};

typedef char char128_t[128];

template <inet Inet> inline void to_string (const struct sockaddr_storage &sa,
	char128_t &b)
{
	static_assert (sizeof (struct sockaddr_storage) >= (4u*4u + 2), "size");
	static_assert (sizeof (b) >= INET_ADDRSTRLEN && (sizeof b) >= INET6_ADDRSTRLEN
		&& sizeof (b) >= 120, "s");
	typedef struct addr <Inet> at;
	assert (at::family == sa.ss_family);
	auto &s = at::ap (sa);
	if (nullptr == ::inet_ntop (at::family, at::no_const_a (s), b, sizeof (b)))
		//! @todo on Windows errno or WSAGetLastError ?
		throw std::system_error (errno, std::system_category(), "failed to convert"
			" address to string");
}

template <inet Inet> inline std::string to_string (const struct sockaddr_storage &sa)
{
	static_assert (sizeof (struct sockaddr_storage) >= (4u*4u + 2), "size");
	char b[128];
	to_string <Inet> (sa, b);
	return b;
}

template <inet Inet> inline struct sockaddr_storage make_address
	(const std::string &ip_port, std::uint16_t port)
{
	static_assert (std::is_trivial <struct sockaddr_storage>::value, "trivial");
	struct sockaddr_storage result;
	std::memset (&result, 0, sizeof result);
	typedef addr <Inet> at;
	result.ss_family = at::family;
	auto &s = at::ap (result);
	const auto r = ::inet_pton (at::family, ip_port.c_str(), at::a (s));
	if (1 == r)
	{
		at::port (s) = htons (port);
		return result;
	}
	else if (0 == r)
		throw std::runtime_error ("invalid address: " + ip_port);
	//! @todo on Windows errno or WSAGetLastError ?
	throw std::system_error (errno, std::system_category(), "can't convert string "
		"to address");
#if 0
	struct sockaddr_in6 sin6;

	std::memset(&sin6, 0, sizeof(sin6));
	sin6.
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN
	sin6.sin6_len = sizeof(sin6);
#endif
	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = htons(port);

	struct sockaddr_in sin;
	std::memset(&sin, 0, sizeof(sin));
#ifdef HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_family = AF_INET;
#endif

}

struct ip_port_t
{
	std::string ip, port;
	bool ipv6;
};

ip_port_t split (const std::string &ip_port)
{
	const auto nsr = ip_port.rfind (':');
	const auto ns = ip_port.find (':');
	const bool bracket = ('[' == ip_port.at(0));
	const bool ipv6 = ((std::string::npos != nsr && std::string::npos != ns
		&& ns != nsr) || bracket);
	if (ipv6)
	{
		if (bracket)
		{
			const auto ne = ip_port.find (']');
			if (std::string::npos == ne || 1 == ne)
				throw std::runtime_error ("invalid IPv6 address:port");
			std::string ip = ip_port.substr (1, ne-1), port;
			if (std::string::npos != nsr && (nsr == 1 + ne))
				port = ip_port.substr (1+nsr);
			return ip_port_t{ip,port,ipv6};
		}
		return ip_port_t{ip_port,"",ipv6};
	}
	else
	{
		if (std::string::npos != nsr)
		{
			std::string port = ip_port.substr (1+nsr), ip = ip_port.substr (0,nsr);
			return ip_port_t{ip,port,ipv6};
		}
		return ip_port_t{ip_port,"",ipv6};
	}
}

inline std::uint16_t str2u16 (const char *str)
{
	char *end = nullptr;
	long val = std::strtol (str, &end, 10);
	if (LONG_MIN == val || LONG_MAX == val)
		throw std::system_error (errno, std::system_category(), "Failed to convert"
			" string to number");
	if ('\0' != *end && ' ' != *end)
		throw std::runtime_error ("invalid number syntax");
	if (0 > val || std::numeric_limits <std::uint16_t>::max() < val)
		throw std::runtime_error ("number out of range");
	return static_cast <std::uint16_t> (val);
}

} // namespace detail

inet NETWORK_API is_ip_address (const char *str)
{
	struct sockaddr_storage addr;
	if (1 == ::inet_pton (AF_INET, str, &addr))
		return inet::ipv4;
	if (1 == ::inet_pton (AF_INET6, str, &addr))
		return inet::ipv6;
	throw std::runtime_error ("Invalid IP address: " + std::string (str));
}

std::ostream &operator<< (std::ostream &o, inet i)
{
	o << ((inet::ipv6==i)?"IPv6":"IPv4");
	return o;
}

enum inet address::inet() const
{
	switch (this->storage()->ss_family)
	{
		case AF_INET6:
			return inet::ipv6;
		case AF_INET:
			return inet::ipv4;
	}
	throw std::logic_error("bad inet");
}

//! @todo code duplication
address::address (const std::string &_ip_port, std::uint16_t _port)
	: dummy_storage_()
{
	// In C++11 std::string uses stack if its length <= 15
	const auto a = detail::split (_ip_port);
	if (!a.port.empty())
		_port = detail::str2u16 (a.port.c_str());
	auto &self = detail::cast (this);
	self.storage_ = a.ipv6 ?  detail::make_address <inet::ipv6> (a.ip, _port)
		: detail::make_address <inet::ipv4> (a.ip, _port);
	self.length_ = sizeof (sockaddr_storage);
}

address::address (const sockaddr_storage *a, unsigned short l) : dummy_storage_()
{
	if (sizeof (sockaddr) > l || sizeof (sockaddr_storage) < l)
		throw std::logic_error ("Invalid network address size");
	assert (nullptr != a);
	assert (AF_INET == a->ss_family || AF_INET6 == a->ss_family);
	this->clear();
	auto &self = detail::cast (this);
	std::memcpy(&self.storage_, a, l);
	self.length_ = l;
	auto str = this->ip_port();
	if (str.length() < 3u || str.length() > 128u)
		throw std::logic_error ("Wrong internet IP address: " + str);
}

address::address (enum inet af, const std::vector <std::uint8_t> &addr,
	std::uint16_t _port)
{
	if (sizeof (struct in_addr) > addr.size() || sizeof (sockaddr_storage)
		< addr.size())
		throw std::logic_error ("Invalid network address size");
	struct sockaddr_storage sa;
	std::memset (&sa, 0, sizeof (sa));
	if (inet::ipv4 == af)
	{
		struct sockaddr_in a;
		a.sin_family = AF_INET;
		a.sin_port = htons (_port);
		std::memcpy (&a.sin_addr.s_addr, addr.data(), addr.size());
		std::memcpy (&sa, &a, sizeof (a));
	}
	else
	{
		struct sockaddr_in6 a;
		a.sin6_family = AF_INET6;
		a.sin6_port = htons (_port);
		std::memcpy (&a.sin6_addr.s6_addr, addr.data(), addr.size());
		std::memcpy (&sa, &a, sizeof (a));
	}
	//! @todo is this safe?
	new (this) address (&sa, sizeof (sa));
}


std::string address::ip_port() const
{
	std::string a = this->ip();
	if (inet::ipv6 == this->inet())
		a = '[' + a + ']';
	return a + ':' + std::to_string (this->port());
}

std::string address::ip() const
{
	const auto &sa = *this->storage();
	if (AF_INET == sa.ss_family)
		return detail::to_string <inet::ipv4> (sa);
	else if (AF_INET6 == sa.ss_family)
		return detail::to_string <inet::ipv6> (sa);
	throw std::logic_error (trace::add_backtrace_string ("invalid address family"));
}

std::ostream &operator<< (std::ostream &o, const address &a)
{
	const auto &sa = *a.storage();
	char buf[128];
	if (AF_INET == sa.ss_family)
	{
		detail::to_string <inet::ipv4> (sa, buf);
		o << buf;
	}
	else if (AF_INET6 == sa.ss_family)
	{
		detail::to_string <inet::ipv6> (sa, buf);
		o << '['  << buf << ']';
	}
	else
		throw std::logic_error (trace::add_backtrace_string
			("invalid address family"));
	o << ':' << a.port();
	return o;
}

std::uint16_t address::port() const
{
	const auto &sa = *this->storage();
	return ntohs ((AF_INET6==sa.ss_family)?
		detail::addr <inet::ipv6>::ap (sa).sin6_port
		:
		detail::addr <inet::ipv4>::ap (sa).sin_port);
}

bool address::operator== (const address &other) const noexcept
{
	const auto af = this->storage()->ss_family;
	if (other.storage()->ss_family != af)
		return false;

	if (AF_INET == af)
	{
		const auto &sin1 = *reinterpret_cast <const struct sockaddr_in *>
			(this->storage()),
			&sin2 = *reinterpret_cast <const struct sockaddr_in *> (other.storage());
		return (sin1.sin_addr.s_addr) == (sin2.sin_addr.s_addr)
			&& (sin1.sin_port == sin2.sin_port);
	}
	else if (AF_INET6 == af)
	{
		const auto
			&s1 = *reinterpret_cast <const struct sockaddr_in6 *> (this->storage()),
			&s2 = *reinterpret_cast <const struct sockaddr_in6 *> (other.storage());
		return (s1.sin6_addr.s6_addr == s2.sin6_addr.s6_addr)
			&& (s1.sin6_port == s2.sin6_port);
	}
	return false;
}

const struct sockaddr *address::addr() const
{
	return reinterpret_cast <const sockaddr*> (&detail::c_cast (this).storage_);
}

const struct sockaddr_storage *address::storage() const
{
	return &detail::c_cast (this).storage_;
}

unsigned short address::len() const
{
	return detail::c_cast (this).length_;
}

int address::af_family() const {return this->storage()->ss_family;}

} // namespace network
