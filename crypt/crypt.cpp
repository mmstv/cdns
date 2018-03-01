#include <fstream>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <type_traits>
#include <iomanip>

#include <sodium.h>

#include "crypt/crypt.hxx"

namespace crypt {

static_assert (std::is_trivial <pubkey>::value, "trivial");
static_assert (std::is_trivial <secretkey>::value, "trivial");
static_assert (std::is_trivial <nonce>::value, "trivial");
static_assert (std::is_trivial <full_nonce>::value, "trivial");
static_assert (std::is_nothrow_move_assignable <pubkey>::value, "move");
static_assert (std::is_nothrow_copy_assignable <pubkey>::value, "move");
static_assert (std::is_nothrow_copy_assignable <secretkey>::value, "move");
static_assert (std::is_nothrow_move_assignable <secretkey>::value, "move");

namespace detail {

template<unsigned short Size>
key<Size> key<Size>::load_binary_file(const std::string &file_name)
{
	std::ifstream f(file_name, std::ios::binary);
	key<Size> result;
	auto nr = f.readsome (reinterpret_cast<char *>(result.modify_bytes()),
		result.size);
	if( nr != result.size )
		throw std::runtime_error("invalid encryption in key in: " + file_name);
	return result;
}

// explicit instantiation
template key <32u> key <32u>::load_binary_file (const std::string &);

//! @todo use sodium_bin2hex?
std::ostream& bin2hex (std::ostream &text_stream, const std::uint8_t * const key,
	unsigned short n)
{
	assert( 0 == n%2 );
	assert (1 < n);
	const auto keep_flags = text_stream.flags (std::ios::hex | std::ios::uppercase);
	const auto keep_fill = text_stream.fill ('0');
	size_t key_pos = 0UL;
	for (;;)
	{
		text_stream
			<< std::setw (2) << static_cast <unsigned short> (key[key_pos])
			<< std::setw (2) << static_cast <unsigned short> (key[key_pos + 1]);
		key_pos += 2U;
		if (key_pos >= n)
			break;
		text_stream << ':';
	}
	text_stream.fill (keep_fill);
	text_stream.flags (keep_flags);
	return text_stream;
}

std::string bytes_to_hex (const std::uint8_t * const key, unsigned short n)
{
	std::ostringstream ostr;
	bin2hex (ostr, key, n);
	return ostr.str();
}

inline void hex2bin (const char *hex, std::size_t len,
	std::uint8_t key[crypto_box_PUBLICKEYBYTES])
{
	std::size_t key_len = 0;
	if (nullptr == hex || 8 > len || 0 != sodium_hex2bin (key,
			crypto_box_PUBLICKEYBYTES, hex, len, ": \t\r\n", &key_len, nullptr)
		|| crypto_box_PUBLICKEYBYTES != key_len)
		throw std::runtime_error (std::string ("Invalid crypto key: ") + hex);
}

inline std::uint64_t hrtime (void)
{
	//! @todo do we have to use high_resolution_clock instead of steady?
	auto now = std::chrono::steady_clock::now().time_since_epoch();
	auto us = std::chrono::duration_cast<std::chrono::microseconds>(now);
	return static_cast <std::uint64_t> (us.count()); // long
}

} // namespace detail

bool nonce::operator==(const nonce &other) const noexcept
{
	return 0 == sodium_memcmp (this->bytes(), other.bytes(), size);
}

nonce::nonce(int)
{
	//! @todo use cryptographic random numbers
	for(unsigned i=0; i<size; ++i)
	{
		this->modify_bytes()[i] = static_cast<std::uint8_t>(std::rand()%256);
	}
}

::crypt::nonce nonce::make_random(void)
{
	uint64_t ts = detail::hrtime();
	uint64_t tsn = (ts << 10) | (randombytes_random() & 0x3ff);
	// uint64_t tsn = (ts << 32) | (randombytes_random());
	static_assert(::crypt::nonce::size == 8U + 4U, "crypto size");
	::crypt::nonce result;
	std::memcpy(result.modify_bytes(), &tsn, 8U);
	uint32_t suffix = randombytes_random();
	std::memcpy(result.modify_bytes() + 8U, &suffix, 4U);
	return result;
}

pubkey::pubkey (const char *hex)
{
	static_assert( size == crypto_box_PUBLICKEYBYTES, "size");
	detail::hex2bin (hex, std::strlen (hex), this->modify_bytes());
}

pubkey::pubkey (const std::string &hex)
{
	static_assert( size == crypto_box_PUBLICKEYBYTES, "size");
	detail::hex2bin (hex.c_str(), hex.length(), this->modify_bytes());
}

std::ostream &operator<< (std::ostream &text_stream, const pubkey &key)
{
	return detail::bin2hex (text_stream, key.bytes(), key.size);
}

secretkey::secretkey (const std::string &hex)
{
	detail::hex2bin (hex.c_str(), hex.length(), this->modify_bytes());
}

} // namespace ::crypt
