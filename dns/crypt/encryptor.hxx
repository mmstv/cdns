#ifndef _DNS_CRYPT_ENCRYPTOR_HXX_
#define _DNS_CRYPT_ENCRYPTOR_HXX_ 1

#include <type_traits>
#include <cstdint>

#include <crypt/crypt.hxx>
#include <dns/crypt/constants.hxx>
#include <dns/query.hxx>
#include <dns/crypt/dll.hxx>

namespace dns { namespace crypt {

DNS_CRYPT_API std::ostream &operator<< (std::ostream &text_stream, enum cipher);

namespace detail {

struct encryptor_data
{
	std::array <std::uint8_t, magic_size> magic_query;
	::crypt::pubkey publickey;
	::crypt::secretkey secretkey;
	static_assert (crypto_box_BEFORENMBYTES == ::crypt::secretkey::size, "size");
	::crypt::secretkey nmkey;
	::crypt::nonce nonce_pad;

	enum dns::crypt::cipher cipher;
	bool  ephemeral_keys;

	static_assert( 8U == sizeof magic_query, "size" );
};

static_assert(std::is_trivially_move_constructible<encryptor_data>::value, "trivia");
static_assert(std::is_trivially_move_assignable<encryptor_data>::value, "trivial");
static_assert (std::is_trivial <encryptor_data>::value, "trivial");

} // namespace detail

class DNS_CRYPT_API encryptor
{
	detail::encryptor_data data_;

public:

	explicit encryptor (bool ephemeral ) noexcept;

	encryptor (::crypt::pubkey &&pk, ::crypt::secretkey &&sk) noexcept;

	//! @TODO: implement properly, see options.cpp:options_use_client_key_file
	explicit encryptor(const char *client_key_file);

	encryptor (encryptor &&) noexcept;
	encryptor (const encryptor &other) noexcept;

	encryptor& operator= (encryptor &&other) noexcept = default;
	encryptor& operator= (const encryptor &other) noexcept = default;

	~encryptor() noexcept;

	void curve(::crypt::nonce &session_nonce, query &q) const;

	void uncurve(const ::crypt::nonce &session_nonce, query &q) const;

	void set_magic_query(
		const std::array <std::uint8_t, magic_size> &magic_query,
		dns::crypt::cipher);

	enum dns::crypt::cipher cipher() const {return data_.cipher;}

	const std::array <std::uint8_t, 8> &magic() const {return data_.magic_query;}

	int set_resolver_publickey(const ::crypt::pubkey &resolver_publickey);

	::crypt::pubkey public_key() const {return data_.publickey;}

	::crypt::secretkey secret_key() const {return data_.secretkey;}

	::crypt::secretkey nm_key() const {return data_.nmkey;}

	::crypt::nonce nonce_pad() const {return data_.nonce_pad;}

	bool ephemeral() const {return data_.ephemeral_keys;}
};

static_assert( std::is_nothrow_move_constructible <encryptor>::value, "move");
static_assert (std::is_nothrow_move_assignable <encryptor>::value, "move");
static_assert( std::is_nothrow_copy_constructible <encryptor>::value, "copy");
static_assert( std::is_nothrow_copy_assignable <encryptor>::value, "copy");

}} // namespace dns::crypt

#endif
