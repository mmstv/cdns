#ifndef NETWORK_UDP_LISTENER_HXX
#define NETWORK_UDP_LISTENER_HXX

#include <network/listener.hxx>
#include <network/connection.hxx>
#include <network/dll.hxx>

namespace network { namespace udp {

class NETWORK_API udplistener : public network::abs_listener, public udp_connection,
	public std::enable_shared_from_this<udplistener>
{
public:

	static std::shared_ptr<class udplistener> make_new(
		std::weak_ptr<network::responder> &&r, const class address &a)
	{
		// can't use make_shared because of private constructor
		return std::shared_ptr<class udplistener>(new udplistener(std::move(r), a));
	}

	static std::shared_ptr<class udplistener> make_new(
		std::weak_ptr<network::responder> &&r, socket_t &&s)
	{
		// can't use make_shared because of private constructor
		return std::shared_ptr<class udplistener> (new udplistener (std::move(r),
				std::move (s)));
	}

private:

	void make_incoming() override;

	udplistener(std::weak_ptr<network::responder> &&, const class address &);

	// Use created and bound systemd socket
	udplistener (std::weak_ptr<network::responder> &&, socket_t &&systemd_bound);

	void init (bool do_bind);
};

using listener = udplistener;

}} // namespace network::udp
#endif
