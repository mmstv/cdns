
#ifndef __CRYPT_CRYPT_HXX__
#define __CRYPT_CRYPT_HXX__ 1

#include <cstdint>
#include <array>
#include <cstring>
#include <string>

#include <sodium/crypto_box.h>
#include <sodium/crypto_sign.h>

#include <dns/crypt/dll.hxx>

constexpr const short crypto_box_HALF_NONCEBYTES = (crypto_box_NONCEBYTES / 2);
static_assert (2*crypto_box_HALF_NONCEBYTES == crypto_box_NONCEBYTES, "crypto");

// conflicts with 'crypt' function from <unistd.h>
namespace crypt {

namespace detail {

DNS_CRYPT_API std::string bytes_to_hex(const uint8_t * const key, unsigned short n);

template<unsigned short Size> class key
{
	static_assert( Size>=12U && Size < 8000U, "unsupported key size");

public:
	typedef key<Size> key_t;

	static constexpr const unsigned short size = Size;

protected:
	typedef std::array<std::uint8_t, size> bytes_t;
	typedef std::uint8_t raw_bytes_t[size];

public:

	const std::uint8_t *bytes() const noexcept {return bytes_.data();}

	std::uint8_t *modify_bytes() noexcept {return bytes_.data();}

	std::string fingerprint() const
	{
		static_assert( 0 == size %2, "even size");
		return bytes_to_hex(this->bytes(), this->size);
	}

#if 1
	//! @todo remove
	explicit key (const std::uint8_t other[size]) noexcept : bytes_()
	{
		std::memcpy (bytes_.data(), other, size);
	}
#endif

	explicit key (const raw_bytes_t &other) noexcept : bytes_()
	{
		std::memcpy (bytes_.data(), other, size);
	}

	explicit key (const bytes_t &other) noexcept : bytes_(other) {}

	explicit key (bytes_t &&other) noexcept : bytes_(std::move(other)) {}

	key() noexcept = default;

	void clear() noexcept
	{
		this->bytes_.fill (0);
	}

protected:
public:
	//! @todo: instantiation in the .cpp file
	static key<Size> load_binary_file(const std::string &file_name);

private:
	bytes_t bytes_;
	// static_assert( size == bytes_t::size(), "size" );

};
} // namespace detail

class nonce : public detail::key<crypto_box_HALF_NONCEBYTES>
{
public:

	DNS_CRYPT_API bool operator== (const nonce &other) const noexcept;

	bool operator!= (const nonce &other) const noexcept {return !(*this == other);}

	explicit nonce (const std::uint8_t other[size]) noexcept : key_t(other) {}

	explicit nonce(int dummy);

	explicit nonce (bytes_t &&other) noexcept : key<size> (std::move(other)) {}

	DNS_CRYPT_API static ::crypt::nonce make_random (void);

	nonce() noexcept = default;
};

class full_nonce : public detail::key<crypto_box_NONCEBYTES>
{
public:

	explicit full_nonce (const std::uint8_t other[size]) noexcept : key_t(other) {}

	full_nonce() noexcept = default;

	full_nonce (const nonce &first, const nonce &second) noexcept
	{
		*reinterpret_cast <nonce*> (this->modify_bytes()) = first;
		*reinterpret_cast <nonce*> (this->modify_bytes()+sizeof(nonce)) = second;
	}

	explicit full_nonce (const nonce &first) noexcept
	{
		this->clear();
		*reinterpret_cast <nonce*> (this->modify_bytes()) = first;
	};

	const nonce &first_half() const noexcept
	{
		return *reinterpret_cast <const nonce*> (this->bytes());
	}

	const nonce &second_half() const noexcept
	{
		return *reinterpret_cast <const nonce*> (sizeof(nonce)+this->bytes());
	}
};

class pubkey : public detail::key<std::max(crypto_sign_ed25519_PUBLICKEYBYTES,
	crypto_box_PUBLICKEYBYTES)>
{
	static_assert( size >= 32U && size < 2048U, "size");

	explicit pubkey (detail::key<size> &&k) noexcept : detail::key<size> (std::move
		(k)) {}

public:

	pubkey() noexcept = default;

	DNS_CRYPT_API explicit pubkey(const char *fingerprint);

	DNS_CRYPT_API explicit pubkey (const std::string &fingerprint);

	explicit pubkey (const std::uint8_t other[size]) noexcept : key_t(other) {}

	static pubkey load_binary_file(const std::string &file_name)
	{
		return pubkey(detail::key<size>::load_binary_file(file_name));
	}
};

DNS_CRYPT_API std::ostream &operator<< (std::ostream &text_stream, const pubkey &);

class secretkey : public detail::key<crypto_box_SECRETKEYBYTES>
{
	explicit secretkey (detail::key<size> &&k) noexcept : detail::key <size>
		(std::move (k)) {}

public:

	secretkey() noexcept = default;

	DNS_CRYPT_API explicit secretkey (const std::string &hexstr);

	explicit secretkey (const std::uint8_t other[size]) noexcept : key_t (other) {}

	static secretkey load_binary_file(const std::string &file_name)
	{
		return secretkey(detail::key<size>::load_binary_file(file_name));
	}
};

} // namesapce crypt
#endif
