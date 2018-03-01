#ifndef NETWORK_CONSTANTS_HXX
#define NETWORK_CONSTANTS_HXX

namespace network {

enum class inet : bool {ipv4=false, ipv6=true};

enum class proto : bool {udp = false, tcp = true};

} // namespace network

#endif
