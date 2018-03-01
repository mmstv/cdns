#ifndef NETWORK_TCP_LISTENER_HXX
#define NETWORK_TCP_LISTENER_HXX

#include <network/listener.hxx>
#include <network/connection.hxx>
#include <network/dll.hxx>

namespace network { namespace tcp {

class NETWORK_API tcplistener : public network::abs_listener, public tcp_connection,
	public std::enable_shared_from_this<class tcplistener>
{
public:
	virtual ~tcplistener() = default;

	static std::shared_ptr<class tcplistener> make_new(
		std::weak_ptr<network::responder> &&r, const class address &a)
	{
		// can't use make_shared because of private constructor
		return std::shared_ptr<class tcplistener>(new tcplistener(std::move(r),a));
	}

	static std::shared_ptr<class tcplistener> make_new(
		std::weak_ptr<network::responder> &&r, socket_t &&systemd_bound)
	{
		// can't use make_shared because of private constructor
		return std::shared_ptr<class tcplistener> (new tcplistener (std::move(r),
			std::move (systemd_bound)));
	}

private:

	void make_incoming() override;

	tcplistener(std::weak_ptr<network::responder> &&, const class address &);

	// for socket from systemd
	tcplistener (std::weak_ptr<network::responder> &&, socket_t &&systemd_bound);

	void init (bool do_bind);
};

using listener = tcplistener;

}} // namespace network::tcp
#endif
