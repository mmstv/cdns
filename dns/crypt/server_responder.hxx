#ifndef __DNS_CRYPT_SERVER_RESPONDER_HXX
#define __DNS_CRYPT_SERVER_RESPONDER_HXX

#include "dns/crypt/server_crypt.hxx"
#include "dns/crypt/certificate.hxx"

namespace dns { namespace crypt { namespace server {

class responder
{
public:

	responder (std::string &&n, ::crypt::pubkey &&pk, ::crypt::secretkey &&sk,
		certificate &&bc)
		: decryptor_(std::move(pk), std::move(sk)), certificate_(std::move(bc)),
		hostname_(std::move(n))
	{}

	const class encryptor &decryptor() const noexcept {return decryptor_;}

	class encryptor &decryptor_mod() noexcept {return decryptor_;}

	const std::string &hostname() const noexcept {return hostname_;}

	const class certificate &certificate() const noexcept {return certificate_;}

private:

	encryptor decryptor_;
	class certificate certificate_;
	std::string hostname_;
	// client keys
};

}}} // namespace dns::crypt::server
#endif
