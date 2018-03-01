#ifndef DNS_CRYPT__OPTIONS_HXX__
#define DNS_CRYPT__OPTIONS_HXX__ 1

#include "dns/options.hxx"
#include <dns/crypt/dll.hxx>

namespace dns {
namespace crypt {

struct DNS_CRYPT_API options : public dns::detail::options
{
	std::string dnscrypt_resolvers_file, server_pubkey_file, server_secretkey_file,
		server_certificate_file, server_hostname;

	options();

private:

	virtual bool apply_option(int opt_flag, const char *optarg);

};

}} // namespace dns::crypt
#endif
