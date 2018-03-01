#include "network/provider.hxx"
#include "network/packet.hxx"

namespace network {

std::unique_ptr<packet> provider::adapt_message (const std::unique_ptr<packet> &other)
	const
{
	assert (other);
	return other->clone();
}

}
