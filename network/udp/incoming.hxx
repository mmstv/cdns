#ifndef NETWORK_UDP_INCOMING_HXX
#define NETWORK_UDP_INCOMING_HXX

#include <network/incoming.hxx>
#include <network/dll.hxx>

namespace network {
namespace udp {

class NETWORK_API in : public network::incoming
{
public:

	explicit in (std::weak_ptr<class udplistener> &&);

	~in() override {}

	void respond(const packet &) override;

private:

	std::shared_ptr<class udplistener> listener_ptr() const;

	const class address &address() {return address_;}

	class address address_;
};

}} // namespace udp

#endif
