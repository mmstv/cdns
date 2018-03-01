#ifndef DNS_HOSTS_HXX_
#define DNS_HOSTS_HXX_

#include <unordered_map>
#include <string>
#include <dns/dll.hxx>

namespace dns {

class DNS_API hosts
{
public:

	hosts() = default;

	hosts (const std::string &file_name, bool disable_ipv6);

	const std::string response(const std::string &ip) const;

	std::size_t count() const {return host_ip_map_.size();}

	bool empty() const {return host_ip_map_.empty();}

private:

	std::unordered_map <std::string, std::string> host_ip_map_;
};

} // namespace dns
#endif
