#ifndef __NETWORK_CONNECTION_HXX__
#define __NETWORK_CONNECTION_HXX__

#include <stdexcept>
#include <cassert>

#include <network/address.hxx>
#include <network/dll.hxx>
#include <network/fwd.hxx>
#include <network/socket.hxx>

namespace network {

class NETWORK_API abs_connection
{
public:

	const class address &address() const noexcept {return address_;}

	int self_error_code() const {return this->sys_socket().self_error_code();}

	bool is_connected() const {return this->sys_socket().is_connected();}

	abs_connection (abs_connection &&other) noexcept = default;

	abs_connection &operator= (abs_connection &&other) = default;

	abs_connection (const abs_connection &) = delete;

	virtual void send (const packet &) const = 0;

	virtual void receive (packet &) const = 0;

	// for libev
	int file_descriptor () const
	{
		if (this->sys_socket().file_descriptor() < 0)
			throw std::logic_error ("network connection is closed");
		return this->sys_socket().file_descriptor();
	}

	std::string to_string () const;

	virtual const system::socket &sys_socket() const = 0;

protected:

	void unblock() const {this->sys_socket().unblock();}

	void reuse_listener_port () const {this->sys_socket().reuse_listener_port();}

	explicit abs_connection (class address &&a) : address_ (std::move (a))
	{
#ifndef NDEBUG
		const auto af = address_.inet();
		assert (inet::ipv4 == af || inet::ipv6 == af);
#endif
	}

	void close() {const_cast <system::socket&> (this->sys_socket()).close();}

	// destructors are implicityly noexcept in C++11
	virtual ~abs_connection() = default;

	void bind_listener() const // used only by UDP/TCP listener
	{
		this->sys_socket().bind_listener (this->address());
	}

private:

	class address address_;
};

// dllexport is needed here for VPTR
// It is not enough to mark just the virtual functions dllexport.
// This issue is only caught with -fsanitize
class NETWORK_API udp_connection : public abs_connection
{
public:

	const system::socket &sys_socket() const override final {return sock_;}

	const udp::socket_t &udp_socket() const {return this->sock_;}

	void udp_receive_from (packet &message, class address &_address) const
	{
		this->udp_receive (message, &_address);
	}

	void udp_receive (packet &message) const
	{
		this->udp_receive (message, nullptr);
	}

	void udp_send_to (const packet &message, const class address &) const;

	void send (const packet &msg) const override {this->udp_send (msg);}

	void receive (packet &m) const override {this->udp_receive (m);}

protected:

	explicit udp_connection (const class address &a)
		: abs_connection (network::address (a)), sock_ (a.inet())
	{}

	explicit udp_connection (udp::socket_t &&systemd_bound)
		: abs_connection (systemd_bound.get_address()),
			sock_ (std::move (systemd_bound)) {}

	void udp_send(const packet &msg) const {this->udp_send_to(msg, this->address());}

private:

	void udp_receive (packet &message, class address *address) const;

	udp::socket_t sock_;
};

class NETWORK_API tcp_connection : public abs_connection
{
	tcp_connection (tcp::socket_t &&s, class address &&a) noexcept
		: abs_connection (std::move (a)), sock_ (std::move (s)) {}

	static tcp_connection accept_new (const tcp::tcplistener &lstn);

protected:

	explicit tcp_connection (const class address &a)
		: abs_connection (network::address (a)), sock_ (a.inet())
	{}

	explicit tcp_connection (const tcp::tcplistener &lstn) : tcp_connection
		(tcp_connection::accept_new (lstn)) {}

	explicit tcp_connection (tcp::socket_t &&systemd_bound)
		: abs_connection (systemd_bound.get_address()),
			sock_ (std::move (systemd_bound)) {}

	void tcp_listen() const; // used only by TCP listener

	void tcp_connect() const;

	void send (const packet &msg) const override {this->tcp_send (msg);}

	void receive (packet &m) const override {this->tcp_receive (m);}

	//! @todo buffered multi-transmission messages
	void tcp_send(const packet &message) const;

	//! @todo buffered multi-transmission messages
	void tcp_receive (packet &message) const;

public:

	const system::socket &sys_socket() const override final {return sock_;}

	const tcp::socket_t &tcp_socket() const {return sock_;}

private:

	tcp::socket_t sock_;
};

} // network

#endif
