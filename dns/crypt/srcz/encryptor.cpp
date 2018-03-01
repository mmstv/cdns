#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sodium.h>

#include "crypt/crypt.hxx"
#include "dns/crypt/encryptor.hxx"
#include "dns/crypt/message_header.hxx"
#include "sys/logger.hxx"

#include "pad_buffer.hxx"

namespace dns { namespace crypt {

DNS_CRYPT_API std::ostream &operator<< (std::ostream &text_stream, enum cipher c)
{
	switch (c)
	{
		case cipher::undefined: (text_stream << "UNDEFINED"); break;
		case cipher::xsalsa20poly1305: (text_stream << "SALSA"); break;
		case cipher::xchacha20poly1305: (text_stream << "CHACHA"); break;
	}
	return text_stream;
}

namespace log = process::log;

namespace detail {

static_assert( sizeof(query_header) == 8U + crypto_box_PUBLICKEYBYTES
	+ (crypto_box_NONCEBYTES / 2), "crypt size");
static_assert( sizeof(query_header) == 8U + 32U + 12U, "size");

static_assert (sizeof (response_header) == magic_size + crypto_box_NONCEBYTES, "s");
static_assert (sizeof (magic_ucstr) == 1u + magic_size, "size");

static_assert (sizeof (query_header) == magic_size
	+ crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES, "size");

static_assert (sizeof (response_header) == (8u + crypto_box_NONCEBYTES), "size");
static_assert (32u == sizeof (response_header), "size");

static_assert (std::is_trivial <response_header>::value, "trivial");
static_assert (std::is_trivial <query_header>::value, "trivial");
static_assert (std::is_pod     <response_header>::value, "pod");
static_assert (std::is_pod     <query_header>::value, "pod");

} // detail


//! @todo: sodium_mlock causes VERY weired problems: the whole system hangs
//! @todo: remove logger_ calls
encryptor::encryptor (bool ephemeral) noexcept
{
	sodium_memzero(&data_, sizeof(data_));
#if 0
	sodium_mlock(&data_, sizeof(data_));
#endif
	data_.ephemeral_keys = ephemeral;
	if (data_.ephemeral_keys)
	{
		log::debug ("Ephemeral keys enabled - generating a new seed");
		assert(data_.ephemeral_keys != 0);
		randombytes_buf(data_.nonce_pad.modify_bytes(), data_.nonce_pad.size);
		randombytes_buf(data_.secretkey.modify_bytes(), data_.secretkey.size);
		randombytes_stir();
	}
	else
	{
		log::info ("Generating a new session key pair");
		assert(data_.ephemeral_keys == 0);
		crypto_box_keypair (data_.publickey.modify_bytes(),
			data_.secretkey.modify_bytes());
	}
}

//! @todo remove logger_ calls
encryptor::encryptor (::crypt::pubkey &&pk, ::crypt::secretkey &&sk) noexcept
{
	sodium_memzero(&data_, sizeof(data_));
#if 0
	sodium_mlock(&data_, sizeof(data_));
#endif
	data_.ephemeral_keys = false;
	log::info ("Persistent key pair");
	data_.publickey = std::move(pk);
	data_.secretkey = std::move(sk);
}


encryptor::encryptor(const char *
#ifndef NDEBUG
	client_key_file
#endif
	)
{
	sodium_memzero(&data_, sizeof(data_));
#if 0
	sodium_mlock(data_, sizeof(data_));
#endif
	data_.ephemeral_keys = false;
	assert( client_key_file != nullptr);
	//! @todo: implement properly, see options.cpp:options_use_client_key_file
	log::info ("Using a user-supplied client secret key");
	if (0 != crypto_scalarmult_base(data_.publickey.modify_bytes(),
			data_.secretkey.bytes()))
		throw std::runtime_error("failed generating encryption keys");
}

encryptor::encryptor (encryptor &&other) noexcept
{
	if (this != &other)
	{
#if 0
		sodium_mlock(data_, sizeof(data_));
#endif
		data_ = std::move (other.data_);
		other.~encryptor();
	}
}

#if 0
encryptor& encryptor::operator= (encryptor &&other) noexcept
{
	if (this != &other)
	{
		data_ = std::move (other.data_);
		other.~encryptor();
	}
	return *this;
}
#endif

encryptor::encryptor (const encryptor &other) noexcept
{
	if( this!=&other )
	{
#if 0
		sodium_mlock(data_, sizeof(data_));
#endif
		data_ = other.data_;
	}
}

encryptor::~encryptor() noexcept
{
	sodium_memzero (&data_, sizeof(data_));
#if 0
	sodium_munlock(data_, sizeof(data_)); // also zeros memory?
#endif
}

void encryptor::set_magic_query(
	const std::array <std::uint8_t, magic_size> &magic_query,
	enum dns::crypt::cipher a_cipher)

{
	static_assert (magic_size == sizeof data_.magic_query, "crypto");
	data_.magic_query = magic_query;
	data_.cipher = a_cipher;
}

int encryptor::set_resolver_publickey(const ::crypt::pubkey &resolver_publickey)
{
	int res = -1;
	static_assert (crypto_box_BEFORENMBYTES == crypto_box_PUBLICKEYBYTES, "crypto");
	static_assert (::crypt::pubkey::size == crypto_box_PUBLICKEYBYTES, "crypto");

	if (data_.ephemeral_keys == 0)
	{
		if (data_.cipher == cipher::xsalsa20poly1305)
		{
			res = crypto_box_beforenm (data_.nmkey.modify_bytes(),
				resolver_publickey.bytes(), data_.secretkey.bytes());
		}
		else if (data_.cipher == cipher::xchacha20poly1305)
		{
			res = crypto_box_curve25519xchacha20poly1305_beforenm(
				data_.nmkey.modify_bytes(), resolver_publickey.bytes(),
				data_.secretkey.bytes());
		}
	}
	else
	{
		data_.publickey = resolver_publickey;
		res = 0;
	}
	return res;
}

class crypt_failure : public std::runtime_error
{
public:
	crypt_failure(const char *msg)
		: std::runtime_error(std::string("Cryptography failure: ")+msg) {}
};

namespace detail {

::crypt::pubkey curve_uncurve(const encryptor_data &keys,
	const ::crypt::full_nonce &fullnonce, uint8_t * const output,
	const std::uint8_t *input, size_t len, const bool e);
}

//  8 bytes: magic_query
// 32 bytes: the client's public key (crypto_box_PUBLICKEYBYTES)
// 12 bytes: a client-selected nonce for this packet (crypto_box_NONCEBYTES / 2)
// 16 bytes: Poly1305 MAC (crypto_box_MACBYTES)

void encryptor::curve(::crypt::nonce &session_nonce, query &msg) const
{
	const detail::encryptor_data &enc = this->data_;
	unsigned len = msg.size();

	constexpr const unsigned mxx = query::max_size; // otherwise linking error
	constexpr const unsigned query_head_size = sizeof (detail::query_header)
		+ (crypto_box_MACBYTES);
	const unsigned max_len = std::min (3u*(msg.size() + query_head_size), mxx);

	if (max_len < len || max_len - len < query_head_size)
		throw crypt_failure ("not enough space allocated to encrypt DNS message");
	assert(max_len > query_head_size);
	uint8_t *const buf = msg.modify_bytes();
	uint8_t *const boxed = buf + sizeof (detail::query_header);
	static_assert (sizeof (detail::query_header) == sizeof enc.magic_query
		+ crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES, "size");

	// shift message leaving space for header
	std::memmove (boxed, buf, len);
	// also adjust for TCP 2 bytes for message size
	len = detail::pad_buffer (boxed, len, max_len - query_head_size - 2u,
		randombytes_uniform);
	session_nonce = ::crypt::nonce::make_random();
	const ::crypt::full_nonce fullnonce(session_nonce); //! @todo fill with zeros

	// in-place encryption
	::crypt::pubkey eph_publickey = curve_uncurve (enc, fullnonce, boxed,
		const_cast <const std::uint8_t*> (boxed), len, false);

	static_assert( std::is_trivial <detail::query_header>::value, "trivial" );
	static_assert( std::is_pod <detail::query_header>::value, "pod" );

	auto *head = reinterpret_cast <detail::query_header *> (buf);
	head->magic = enc.magic_query;
	head->pubkey = eph_publickey;
	head->nonce = session_nonce;
	assert (head->nonce == fullnonce.first_half());
	const unsigned curved_size = len + query_head_size;
	if (curved_size <= 0u || curved_size > 65534u)
		throw crypt_failure("failed to encrypt");
	msg.set_size(static_cast<unsigned short>(curved_size));
}

//  8 bytes: the string r6fnvWj8
// 12 bytes: the client's nonce (crypto_box_NONCEBYTES / 2)
// 12 bytes: a server-selected nonce extension (crypto_box_NONCEBYTES / 2)
// 16 bytes: Poly1305 MAC (crypto_box_MACBYTES)
void encryptor::uncurve(const ::crypt::nonce &session_nonce, query &q) const
{
	const unsigned len = q.size();
	if (len <= (sizeof (detail::response_header) + (crypto_box_MACBYTES)))
		throw crypt_failure("too short encrypted message");
	uint8_t * const buf = q.modify_bytes();
	const auto *const head = reinterpret_cast <const detail::response_header*> (buf);
	if (head->magic != detail::magic_response() )
		throw crypt_failure ("message is not DNScrypt");
	if( session_nonce != head->client_nonce )
		throw crypt_failure ("wrong nonce");
	const ::crypt::full_nonce fullnonce(head->client_nonce, head->server_nonce);
	const unsigned ciphertext_len = len - static_cast <unsigned short> (sizeof
		(detail::response_header));
	(void) curve_uncurve (this->data_, fullnonce, buf,
		const_cast <const std::uint8_t*>(buf + sizeof (detail::response_header)),
		ciphertext_len, true);

	unsigned dec_size = detail::find_end(buf, ciphertext_len - crypto_box_MACBYTES);

	assert( dec_size < q.max_size );
	assert( dec_size <= q.size());
	if (dec_size < sizeof (dns::message_header))
		throw crypt_failure ("decrypted message is not DNS");
	//! @todo message type size, numeric_cast
	q.set_size (static_cast <query::size_type> (dec_size));
}

namespace detail {

::crypt::pubkey curve_uncurve(const encryptor_data &keys,
	const ::crypt::full_nonce &fullnonce, uint8_t *const output,
	const std::uint8_t *const input, const size_t len, const bool e)
{
	if (keys.cipher != cipher::xsalsa20poly1305
		&& keys.cipher != cipher::xchacha20poly1305)
		throw std::logic_error("unsupported cipher");

	const auto afternm = e ? crypto_box_open_easy_afternm : crypto_box_easy_afternm ;
	const auto chacha_afternm = e
		? crypto_box_curve25519xchacha20poly1305_open_easy_afternm
		: crypto_box_curve25519xchacha20poly1305_easy_afternm;
	const auto easy = e ? crypto_box_open_easy : crypto_box_easy;
	const auto chacha_easy = e ? crypto_box_curve25519xchacha20poly1305_open_easy
		: crypto_box_curve25519xchacha20poly1305_easy;

	const auto do_afternm = (keys.cipher == cipher::xchacha20poly1305)
		? chacha_afternm : afternm;
	const auto do_easy = (keys.cipher == cipher::xchacha20poly1305)
		? chacha_easy : easy;

	int res = -300;
	::crypt::pubkey eph_publickey; // only used for encryption
	if (keys.ephemeral_keys == 0)
	{
		res = do_afternm (output, input, len, fullnonce.bytes(),
			keys.nmkey.bytes());
		eph_publickey = keys.publickey;
	}
	else
	{
		uint8_t eph_secretkey[crypto_box_SECRETKEYBYTES];
		static_assert (crypto_stream_NONCEBYTES == ::crypt::full_nonce::size, "sz");
		const ::crypt::full_nonce eph_fullnonce (fullnonce.first_half(),
			keys.nonce_pad);
		crypto_stream(eph_secretkey, sizeof eph_secretkey, eph_fullnonce.bytes(),
			keys.secretkey.bytes());
		crypto_scalarmult_base (eph_publickey.modify_bytes(),
			const_cast<const std::uint8_t *>(eph_secretkey));
		res = do_easy (output, input, len, fullnonce.bytes(), keys.publickey.bytes(),
			eph_secretkey);
		sodium_memzero(eph_secretkey, sizeof eph_secretkey);
	}
	if (0 != res)
		throw crypt_failure("-6");
	return eph_publickey; // only used for encryption
}

} // namespace detail

}} // dns::crypt
