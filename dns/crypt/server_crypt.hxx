#ifndef DNS_CRYPT_SERVER_HXX
#define DNS_CRYPT_SERVER_HXX

#include <dns/crypt/dll.hxx>
#include <dns/query.hxx>
#include <crypt/crypt.hxx>

namespace dns { namespace crypt { namespace detail {

class server_encryptor_data
{
public:
	std::array <std::uint8_t, 2> es_version;
	::crypt::pubkey crypt_publickey;
	::crypt::secretkey crypt_secretkey;
};

} // namespace dns crypt detail

namespace server {

class DNS_CRYPT_API encryptor
{
public:

	encryptor();

	encryptor(::crypt::pubkey &&, ::crypt::secretkey &&);

	std::pair<::crypt::pubkey,::crypt::nonce> uncurve(query &q);

	void curve (const ::crypt::nonce &client_nonce, const ::crypt::pubkey &nmkey,
		query &q);

	::crypt::pubkey public_key() const {return keys_.crypt_publickey;}

	::crypt::secretkey secret_key() const
	{
		return keys_.crypt_secretkey;
	}

private:

	detail::server_encryptor_data keys_;
};

} // dns::crypt::server

}} // namespace dns::crypt

#endif
