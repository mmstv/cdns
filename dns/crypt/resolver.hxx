#ifndef _DNS_CRYPT_RESOLVER_HXX
#define _DNS_CRYPT_RESOLVER_HXX

#include <string>
#include <vector>

#include <dns/crypt/encryptor.hxx>
#include <network/provider.hxx>
#include <dns/crypt/dll.hxx>

namespace dns { namespace crypt {

namespace defaults {
constexpr const unsigned short port = 443;
}

class DNS_CRYPT_API resolver : public network::provider
{
public:

	resolver(const std::string &hn, const network::address &addr,
		const ::crypt::pubkey &signing_pubkey, network::proto t);

	resolver (std::string &&_name, std::string &&hn, ::crypt::pubkey &&ks,
		network::address &&, network::proto t, bool dnssec = false,
		bool namecoin = false, bool logg = false) noexcept;

	std::string fingerprint() const;

	std::string local_fingerprint() const;

	std::string encryptor_fingerprint() const;

	const ::crypt::pubkey &public_key() const {return pubkey_;}

	const ::crypt::pubkey &resolver_public_key() const {return resolver_pubkey_;}

	void set_resolver_public_key(const ::crypt::pubkey &k) {resolver_pubkey_=k;}

	void set_resolver_public_key (::crypt::pubkey &&k)
	{
		resolver_pubkey_=std::move(k);
	}

	std::string ip_port() const {return this->address().ip_port();}

	const std::string &id() const {return name_;}

	const std::string &hostname() const {return host_name_;}

	bool is_ready() const {return ready_;}

	void set_ready(bool b) {ready_=b;}

	void set_magic_query (const std::array <std::uint8_t, magic_size> &q) noexcept
	{
		this->dnscrypt_magic_query_ = q;
	}

	const class ::dns::crypt::encryptor &encryptor() const
	{
		return this->dnscrypt_client_;
	}

	::dns::crypt::encryptor &modify_encryptor() {return this->dnscrypt_client_;}

	bool find_valid_certificate( std::vector< std::vector<std::uint8_t> >  &trv);

	void unfold(network::packet &q) override;

	void fold(network::packet &q) override;

	std::unique_ptr <network::packet> adapt_message (const std::unique_ptr
		<network::packet> &other) const override;

private:
	::crypt::pubkey pubkey_, // provider public key
		resolver_pubkey_;// local resolver/client?/daemon? public key (generated)

	std::string name_, host_name_;
	bool dnssec_, namecoin_, logging_, ready_;
	std::array <std::uint8_t, magic_size> dnscrypt_magic_query_; //! @todo unused

	class ::dns::crypt::encryptor dnscrypt_client_;
};

DNS_CRYPT_API std::vector<resolver> parse_resolvers_list(const char *file_name,
	network::proto);

DNS_CRYPT_API void save_resolvers (const std::vector <resolver> &rs,
	const std::string &filename);

}} // namespace dns::crypt
#endif //__DNS_CRYPT_RESOLER_HXX
