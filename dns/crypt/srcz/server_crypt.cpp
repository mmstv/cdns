#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>

#include <sodium.h>
#if SODIUM_LIBRARY_VERSION_MAJOR < 7
# error "outdated libsodium"
#endif

#include "crypt/crypt.hxx"
#include "sys/logger.hxx"
#include "dns/crypt/server_crypt.hxx"
#include "dns/crypt/message_header.hxx"

#include "pad_buffer.hxx"

namespace dns { namespace crypt { namespace detail {

inline bool is_chacha(const server_encryptor_data &cert)
{
	return 0 == cert.es_version[0] && 2 == cert.es_version[1];
}

static_assert( 8u + 32u + 12u == sizeof(query_header), "size" );
static_assert( sizeof (detail::query_header) == (magic_size
	+ crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES), "size");
static_assert( crypto_box_BEFORENMBYTES == ::crypt::pubkey::size, "size" );
static_assert( crypto_box_BEFORENMBYTES == ::crypt::secretkey::size, "size" );
static_assert(crypto_box_BEFORENMBYTES == crypto_box_PUBLICKEYBYTES, "size");
static_assert( sizeof (detail::query_header) == (magic_size
	+ crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES), "crypto size");
static_assert (sizeof (response_header) == crypto_box_HALF_NONCEBYTES * 2
	+ magic_size, "size");
static_assert (crypto_box_BEFORENMBYTES == ::crypt::pubkey::size, "size");

constexpr const unsigned short query_head_size = sizeof(query_header)
	+ (crypto_box_MACBYTES);

inline std::uint32_t crypto_random(const ::crypt::nonce &halfnonce,
	const ::crypt::secretkey &secretkey)
{
	const ::crypt::full_nonce nonce2(halfnonce, halfnonce);
	assert(nonce2.bytes()[crypto_box_HALF_NONCEBYTES] == nonce2.bytes()[0]);
	std::uint32_t rnd;
	crypto_stream(reinterpret_cast <unsigned char * > (&rnd), sizeof(rnd),
		nonce2.bytes(), secretkey.bytes());
	return rnd;
}

} // detail

namespace server {

//  8 bytes: magic_query
// 32 bytes: the client's public key (crypto_box_PUBLICKEYBYTES)
// 12 bytes: a client-selected nonce (crypto_box_HALF_NONCEBYTES)
// 16 bytes: Poly1305 MAC (crypto_box_MACBYTES)
std::pair<::crypt::pubkey,::crypt::nonce> encryptor::uncurve(query &q)
{
	static_assert( crypto_box_BEFORENMBYTES == ::crypt::pubkey::size, "size");

	const detail::server_encryptor_data &keys = this->keys_;
	const auto beforenm = detail::is_chacha(keys)
		? crypto_box_curve25519xchacha20poly1305_beforenm : crypto_box_beforenm;
	const unsigned msg_size = q.size();
	if (msg_size <= detail::query_head_size)
		throw std::runtime_error("decryption failed: too short message");

	uint8_t *const buf = q.modify_bytes();
	auto *const head = reinterpret_cast <const detail::query_header *> (buf);
	//! @todo compare head->magic with expected one from the certificate
	::crypt::pubkey nmkey = head->pubkey;
	if (beforenm (nmkey.modify_bytes(), nmkey.bytes(), keys.crypt_secretkey.bytes())
		!= 0)
		throw std::runtime_error("decryption failed: beforenm");

	::crypt::full_nonce fullnonce(head->nonce);

	// mac and encrypted message
	const std::uint8_t *const encrypted = buf + sizeof (detail::query_header);
	const auto easy_afternm = detail::is_chacha(keys)
		? crypto_box_curve25519xchacha20poly1305_open_easy_afternm
		: crypto_box_open_easy_afternm;
	if (0 != easy_afternm (buf, encrypted, msg_size - sizeof (detail::query_header),
			fullnonce.bytes(), nmkey.bytes()))
		throw std::runtime_error("decryption failed: afternm");

	unsigned dec_size = detail::find_end (buf, msg_size - detail::query_head_size);
	if (dec_size < sizeof (dns::message_header))
		throw std::runtime_error ("too short decrypted DNS message");
	//! @todo numeric_cast
	q.set_size (static_cast <query::size_type> (dec_size));
	static_assert(crypto_box_BEFORENMBYTES == crypto_box_PUBLICKEYBYTES, "size");
	return std::make_pair (nmkey, fullnonce.first_half());
}

//  8 bytes: magic header (CERT_MAGIC_HEADER)
// 12 bytes: the client's nonce
// 12 bytes: server nonce extension
// 16 bytes: Poly1305 MAC (crypto_box_MACBYTES)
void encryptor::curve(const ::crypt::nonce &client_nonce,
	const ::crypt::pubkey &nmkey, query &q)
{
	static_assert( crypto_box_HALF_NONCEBYTES == ::crypt::nonce::size, "crypto");
	static_assert (crypto_box_BEFORENMBYTES == ::crypt::pubkey::size, "crypto");

	uint8_t *const buf = q.modify_bytes();
	uint8_t *const boxed = buf + sizeof (detail::response_header);
	std::uint8_t *const shifted_message = boxed + crypto_box_MACBYTES;
	const unsigned msg_size = q.size();
	// shift message leaving space for the header and MAC
	std::memmove(shifted_message, buf, msg_size);
	// Add random padding to a buffer, according to a client nonce.
	// The length has to depend on the query in order to avoid reply attacks.
	// also adjust for TCP 2 bytes for message size
	const ::crypt::secretkey &crypt_secretkey = this->keys_.crypt_secretkey;
	constexpr const unsigned short response_head_size = sizeof
		(detail::response_header) + (crypto_box_MACBYTES);
	assert (q.reserved_size() > 120u && q.reserved_size() < 66000);
	const unsigned mxsz = std::min (static_cast <unsigned> (q.reserved_size()),
		(msg_size + std::max(response_head_size, detail::query_head_size))*3u);
	assert (mxsz > 120u && mxsz < 66000u );
	const unsigned padded_msg_size = detail::pad_buffer (shifted_message, msg_size,
		mxsz - std::max(response_head_size, detail::query_head_size) - 2u,
		[client_nonce, crypt_secretkey](std::uint32_t ub)
		{
			return detail::crypto_random (client_nonce, crypt_secretkey) % ub;
		});
	assert (padded_msg_size > 32u && padded_msg_size < 66000u);
	// add server nonce extension
	const ::crypt::full_nonce fullnonce(client_nonce, ::crypt::nonce::make_random());

	const auto easy_afternm = is_chacha (this->keys_)
		? crypto_box_curve25519xchacha20poly1305_easy_afternm
		: crypto_box_easy_afternm;
	if (0 != easy_afternm (boxed, const_cast<const std::uint8_t *>(shifted_message),
			padded_msg_size, fullnonce.bytes(), nmkey.bytes()))
		throw std::runtime_error("encryption failed");

	auto *const head = reinterpret_cast <detail::response_header*> (buf);
	head->magic = detail::magic_response();
	//! @todo full_nonce = union{nonce}
	head->client_nonce = fullnonce.first_half();
	head->server_nonce = fullnonce.second_half();
	//! @todo numeric_cast
	q.set_size (static_cast <query::size_type> (padded_msg_size
		+ response_head_size));
}

encryptor::encryptor()
{
	int r = crypto_box_keypair(keys_.crypt_publickey.modify_bytes(),
		keys_.crypt_secretkey.modify_bytes());
	if( 0 != r )
		throw std::runtime_error("Failed to generate key pair");
	keys_.es_version[0] = 0;
	keys_.es_version[1] = 0;
}

encryptor::encryptor(::crypt::pubkey &&pk, ::crypt::secretkey &&sk)
{
	keys_.es_version[0] = 0;
	keys_.es_version[1] = 0;
	keys_.crypt_publickey = pk;
	keys_.crypt_secretkey = sk;
}

} // namespace dns::crypt::server

}} // namespace dns  crypt detail

