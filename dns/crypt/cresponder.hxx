#ifndef DNS_CRYPT_CRESPONDER_HXX
#define DNS_CRYPT_CRESPONDER_HXX

#include <memory>
#include <map>

#include <dns/responder.hxx>

#include <ev++.h>
#include <dns/crypt/dll.hxx>

namespace dns { namespace crypt {

class certifier;

namespace server {
class responder;
}

class DNS_CRYPT_API cresponder : public dns::responder
{
public:

	cresponder (const dns::responder::parameters &,
		const std::string &dnscrypt_resolvers,
		std::unique_ptr<server::responder> s = nullptr);

	void process(std::shared_ptr<network::incoming> &&) override;

	void respond (std::shared_ptr<network::incoming> &req) override;

	// std::unique_ptr <network::packet> new_packet() const override;

	void update_certificates();

	void reload(const dns::responder::parameters &,
		const std::string &dnscrypt_resolvers_file);

	void save_providers (const std::string &filename) const;

private:

	void load (const std::string &dnscrypt_resolvers_file, bool noipv6,
		network::proto tcponly, double timeout);

	std::shared_ptr <network::provider> random_provider (const query &) const
		override;

	void select_random_provider() const;

	std::map <std::string, std::shared_ptr <certifier>> dnscrypt_providers_;

	std::unique_ptr< server::responder > server_ptr_;

	// copy constructor is deleted in ev::timer, and move does not work
	ev::timer random_timer_;

	mutable std::string random_dnscrypt_provider_;
};

}}

#endif
