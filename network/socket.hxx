#ifndef __NETWORK_SOCKET_HXX__
#define __NETWORK_SOCKET_HXX__

#include <string>

#include <network/dll.hxx>
#include <network/constants.hxx>
#include <network/fwd.hxx>

namespace network {

namespace system
{

typedef
#ifdef _WIN64
	unsigned long long
#elif defined (_WIN32)
	unsigned
#else
	int
#endif
	socket_handle_t;

class socket
{
private:

	socket_handle_t socket_;
#ifdef _WIN32
	int file_descriptor_;
#endif

protected:

	inline socket (inet af, int type, int protocol);

	NETWORK_API socket () noexcept;

	NETWORK_API socket (socket &&other) noexcept;

	// destructors are implicityly noexcept in C++11
	~socket() {try {this->close();} catch (...) {}}

	NETWORK_API socket &operator= (socket &&other);

	socket_handle_t handle() const noexcept {return socket_;}

public:

	socket (const socket &) = delete;

	int NETWORK_API self_error_code() const;

	//! to be used with libev for example
	int NETWORK_API file_descriptor() const noexcept;

	std::string NETWORK_API to_string () const;

	void NETWORK_API unblock() const;

	// only for listener
	void NETWORK_API reuse_listener_port () const;

	void NETWORK_API close();

	bool NETWORK_API is_connected() const; //! @todo for tcp only?

	void NETWORK_API bind_listener (const address &) const;

	address NETWORK_API get_address () const;

	// only for listener
	// not used void reuse_listener_address() const;
};

} // namespace network::system

//! TCP/UDP socket
template <proto TcpUdp>
class socket final : public system::socket
{
public:

	explicit NETWORK_API socket (inet);

	socket () noexcept = default;

	socket (socket &&other) noexcept = default;

	socket &operator= (socket &&other) = default;
};

template <proto Proto>
void NETWORK_API bind_listener (const socket <Proto> &, const address &);

template <proto Proto>
socket <Proto> NETWORK_API bound_listener (system::socket_handle_t);

namespace udp
{

using socket_t = class socket <proto::udp>;

ssize_t NETWORK_API send_to (const socket_t&, const std::uint8_t *msg,
	unsigned short msglen, int flags, const address &);

ssize_t NETWORK_API receive_from (const socket_t&, std::uint8_t *buf,
	unsigned short bufsize, int flags, address *);

} // namespace network::udp

namespace tcp
{

using socket_t = class socket <proto::tcp>;

ssize_t NETWORK_API send (const socket_t&, const std::uint8_t *msg,
	unsigned short msglen, const int flags);

ssize_t NETWORK_API receive (const socket_t&, std::uint8_t *buf,
	unsigned short bufsize, const int flags);

void NETWORK_API connect (const socket_t&, const address &);

void NETWORK_API listen (const socket_t&, int backlog);

std::pair <socket_t, address> NETWORK_API accept_new (const socket_t&);

} // namespace network::tcp

} // network

#endif
