#ifndef NETWORK_UDP_UPSTREAM_HXX_
#define NETWORK_UDP_UPSTREAM_HXX_ 2

#include <network/connection.hxx>
#include <network/upstream.hxx>
#include <network/dll.hxx>

namespace network { namespace udp {

class NETWORK_API out : public ::network::upstream, public network::udp_connection
{
public:

	out (std::shared_ptr <provider> &&p, std::unique_ptr <packet> &&pkt,
		double tsec);

	virtual ~out() = default; // because of virtual functions

	void close() override {this->upstream::close(); this->udp_connection::close();}
};

}} // namespace network::udp
#endif
