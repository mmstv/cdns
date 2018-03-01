#ifndef _CERTIFICATE_HXX__
#define _CERTIFICATE_HXX__ 1

#include <vector>
#include <array>
#include <cinttypes>
#include <iosfwd>

#include <sodium/crypto_box.h>

#include <crypt/crypt.hxx>
#include <dns/crypt/constants.hxx>
#include <network/fwd.hxx>

#include <dns/crypt/dll.hxx>

namespace dns {

class query;

namespace crypt {

class version
{
public:
	std::array <std::uint8_t, 4> magic;
	std::array <std::uint8_t, 2> major, minor;
};

static_assert( sizeof(version) == 4U + 2U*2U, "size" );

class certificate_data
{
public:
	::crypt::pubkey server_publickey;
	std::array <std::uint8_t, 8> magic_query;
	std::array <std::uint8_t, 4> serial;
	std::array <std::uint8_t, 4> ts_begin;
	std::array <std::uint8_t, 4> ts_end;
};

static_assert( sizeof (certificate_data) == 32U + 8U + 4U*3U, "size" );

class DNS_CRYPT_API certificate
{
public:

	certificate (const ::crypt::pubkey &signing_pubkey,
		const std::uint8_t *signed_certificate, const std::size_t size);

	certificate (const ::crypt::pubkey &signing_pubkey,
		const std::vector<std::uint8_t> &signed_certificate);

	~certificate();

	bool is_preferred_over(const certificate &other) const;

	enum dns::crypt::cipher cipher() const;

	std::array<std::uint32_t,2> ts() const;

	const ::crypt::pubkey &encrypting_public_key() const
	{
		return data_.server_publickey;
	}

	//! @todo obsolete
	::crypt::pubkey server_public_key() const;

	const std::array <std::uint8_t, 8> &magic_message() const
	{
		return data_.magic_query;
	}

	void print_info() const;

	void check_key_rotation_period() const;

	static certificate load_binary_file (const std::string &file_name,
		const ::crypt::pubkey &provider_pubkey);

	const std::vector<std::uint8_t> &raw_signed() const {return raw_signed_;}

	bool is_dnscrypt_message(const network::packet &q) const;

	std::uint32_t version() const;

	std::uint32_t serial() const;

private:

	void clear();
	class version version_;
	certificate_data data_;
	std::vector<std::uint8_t> raw_signed_;
};

}}
#endif
