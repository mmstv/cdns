#ifndef __DNS_CRYPT_MESSAGE_HEADER_HXX
#define __DNS_CRYPT_MESSAGE_HEADER_HXX  2

#include <cinttypes>
#include <array>

#include "crypt/crypt.hxx"
#include "dns/crypt/constants.hxx"

namespace dns { namespace crypt {

namespace detail {

class query_header
{
public:
	std::array <std::uint8_t, magic_size> magic;
	::crypt::pubkey pubkey;
	::crypt::nonce nonce;
};
static_assert( 8u + 32u + 12u == sizeof(query_header), "size" );

class response_header
{
public:
	std::array <std::uint8_t, magic_size> magic;
	::crypt::nonce client_nonce, server_nonce;
};
static_assert( 8u + 2u*12u == sizeof(response_header), "size" );

inline constexpr std::array <std::uint8_t, 8> magic_response()
{
	static_assert (8u == magic_size, "size");
	static_assert (1u + magic_size == sizeof (magic_ucstr), "size");

	constexpr const std::uint8_t (&cm)[9] = magic_ucstr;
	constexpr const std::array <std::uint8_t, magic_size> amagic {{cm[0], cm[1],
		cm[2], cm[3], cm[4], cm[5], cm[6], cm[7]}};
	return amagic;
}

inline unsigned find_end(const std::uint8_t *const buf, unsigned size)
{
	while (size > 0u && 0 == buf[--size]) {;}
	if (0x80 != buf[size])
		throw std::runtime_error("no end");
	return size;
}

} // namespace detail

}} // dns::crypt
#endif
