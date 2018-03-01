#ifndef DNS_RESPONDER_PARAMETERS_HXX
#define DNS_RESPONDER_PARAMETERS_HXX

#include <string>
#include <unordered_set>

#include <network/constants.hxx>

namespace dns {

class responder_parameters
{
public:
	std::string resolvers, hosts, onion, cachedir;
	bool noipv6;
	network::proto net_proto;
	unsigned short min_ttl;
	double timeout;
	std::unordered_set <std::string> whitelists, blacklists;
};

} // namespace dns
#endif
