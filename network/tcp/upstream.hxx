#ifndef NETWORK_TCP_UPSTREAM_HXX_
#define NETWORK_TCP_UPSTREAM_HXX_ 2

#include <memory>

#include <network/connection.hxx>
#include <network/upstream.hxx>
#include <network/dll.hxx>

namespace network { namespace tcp {

class NETWORK_API upstream : public ::network::upstream, public tcp_connection
{
public:

	upstream (std::shared_ptr <provider> &&p, std::unique_ptr <packet> &&pkt,
		double tsec);

	// Call to virtual function during destruction will not dispatch to derived class
	~upstream() override {this->upstream::close();}

	void close() override;
};

}} // namespace network::tcp
#endif
