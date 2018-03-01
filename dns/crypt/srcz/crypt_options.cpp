#include "dns/crypt/crypt_options.hxx"

#ifndef DEFAULT_RESOLVERS
# ifdef _WIN32
#  define DEFAULT_RESOLVERS "dnscrypt-resolvers.csv"
# else
#  define DEFAULT_RESOLVERS PKGDATADIR "/dnscrypt-resolvers.csv"
# endif
#endif

namespace dns { namespace crypt {

options::options()
{
	this->dnscrypt_resolvers_file = DEFAULT_RESOLVERS;
	this->add("crypt-resolvers", 'D', 1);
	this->add("server-pubkey", 'P', 1);
	this->add("server-secretkey", 'S', 1);
	this->add("server-certificate", 'C', 1);
	this->add("server-name", 'N', 1);
}

bool options::apply_option(int opt_flag, const char *optarg)
{
	switch (opt_flag) {
	case 'D':
		this->dnscrypt_resolvers_file = optarg;
		return true;
	case 'P':
		this->server_pubkey_file = optarg;
		return true;
	case 'S':
		this->server_secretkey_file = optarg;
		return true;
	case 'C':
		this->server_certificate_file = optarg;
		return true;
	case 'N':
		this->server_hostname = optarg;
		return true;
	}
	return this->dns::detail::options::apply_option(opt_flag, optarg);
}

}} // namespace dns::crypt
