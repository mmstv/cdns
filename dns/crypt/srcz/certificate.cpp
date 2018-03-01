#include <stdexcept>
#include <memory>
#include <sstream>
#include <fstream>
#include <type_traits>
#include <iomanip>

#include <cassert>
#include <cinttypes>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include <sys/types.h>
#include <sodium.h>

#include "sys/preconfig.h" // for gmtime_r
#ifndef HAVE_GMTIME_R
#error "linux has gmtime_r"
#endif

#include "network/net_config.h" // for arpa/inet

// for htonl
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#elif defined (HAVE_WINSOCK2_H)
#include <winsock2.h>
#else
#error "htonl"
#endif

#include "dns/crypt/certificate.hxx"
#include "dns/query.hxx"
#include "sys/logger.hxx"
#include "crypt/crypt.hxx"
#include "dns/crypt/server_crypt.hxx"
#include "dns/crypt/message_header.hxx"

namespace dns { namespace crypt {

namespace log = process::log;

bool certificate::is_dnscrypt_message(const network::packet &q) const
{
	log::debug ("is dnscrypt: ", q.size());
	if (q.size() <= sizeof (detail::query_header) + crypto_box_MACBYTES)
		return false;
	auto *head = reinterpret_cast <const detail::query_header *>
		(q.bytes());
	assert (magic_size == this->magic_message().size());
	return head->magic == this->magic_message();
}

namespace detail {

constexpr const char cert_magic_cert[] = "DNSC";
// original constexpr const unsigned recommended_key_rotation_period = 86400; // day
constexpr const unsigned recommended_key_rotation_period = 7 * 86400; // one week

bool cert_parse_version(const version &ver)
{
	if (0 != std::memcmp (ver.magic.data(), cert_magic_cert, ver.magic.size()))
	{
		log::debug ("TXT record with no certificates received");
		return false;
	}
	if (ver.major[0] != 0U || (ver.major[1] != 1U && ver.major[1] != 2U))
	{
		log::error ("Unsupported certificate version: ", ver.major[1], ".",
			ver.major[0]);
		return false;
	}
	return true;
}

} // namespace dns::crypt::detail

std::uint32_t certificate::serial() const
{
	std::uint32_t userial = 0;
	static_assert (sizeof (userial) == sizeof (data_.serial), "size");
	std::memcpy (&userial, &data_.serial, sizeof userial);
	return htonl (userial);
}

std::uint32_t certificate::version() const
{
	uint32_t version =
		(static_cast <std::uint32_t> (version_.major[0]) << 24) +
		(static_cast <std::uint32_t> (version_.major[1]) << 16) +
		(static_cast <std::uint32_t> (version_.minor[0]) << 8) +
		static_cast <std::uint32_t> (version_.minor[1]);
	return version;
}

bool certificate::is_preferred_over (const certificate &previous) const
{
	const std::uint32_t serial = this->serial(),
		version = this->version(),
		previous_version = previous.version(),
		previous_serial = previous.serial();

	if (previous_version > version)
	{
		log::info ("Keeping certificate #", previous_serial,
			" which is for a more recent version than #", serial);
		return false;
	}
	else if (previous_version < version)
	{
		log::info ("Favoring version #", version, " over version #",
			previous_version);
		return true;
	}
	if (previous_serial > serial)
	{
		log::info ("Certificate #", previous_serial,
			" has been superseded by certificate #", serial);
		return false;
	}
	log::info ("This certificate supersedes certificate #", previous_serial);
	return true;
}

class invalid_certificate : public std::runtime_error
{
public:
	invalid_certificate() : std::runtime_error("invalid certificate") {}
	invalid_certificate(const char *msg) : std::runtime_error(msg) {}
	invalid_certificate(const std::string &msg) : std::runtime_error(msg) {}
};

certificate certificate::load_binary_file(const std::string &file_name,
	const ::crypt::pubkey &provider_pubkey)
{
	std::ifstream fc(file_name, std::ios::binary);
	if( fc )
	{
		constexpr const unsigned short nb = 1024U;
		static_assert( nb > sizeof(class version), "size" );
		std::uint8_t buf[nb+256];
		auto nr = fc.readsome(reinterpret_cast <char*> (buf), nb);
		if( fc && nr > static_cast<short>(sizeof(class version)) && nr <= nb )
			return certificate(provider_pubkey, buf, static_cast<std::size_t>(nr));
	}
	throw invalid_certificate("Failed to load certificate from binary file: "
		+ file_name);
}

certificate::certificate (const ::crypt::pubkey &provider_pubkey,
	const std::vector<std::uint8_t> &signed_certificate)
	: certificate (provider_pubkey, signed_certificate.data(),
		signed_certificate.size())
{}

certificate::certificate(const ::crypt::pubkey &provider_pubkey,
	const std::uint8_t *const signed_certificate,
	const std::size_t signed_bincert_len)
{
	const auto &signed_ver = *reinterpret_cast <const class version*>
		(signed_certificate);
	if( 65000U < signed_bincert_len || sizeof(class version) > signed_bincert_len )
		throw invalid_certificate();
	if (!detail::cert_parse_version (signed_ver))
		throw invalid_certificate();
	static_assert( 4U + 2U+2U == sizeof(class version), "size" );
	static_assert( std::is_pod<class version>::value, "pod" );
	static_assert( std::is_trivially_copyable<class version>::value, "trivial" );
	this->version_ = signed_ver;
	// See sodium/crypto_sign_ed25519.h,
	// it does not really specify expected array sizes, need docs.
	static_assert( ::crypt::pubkey::size == crypto_sign_ed25519_PUBLICKEYBYTES,
		"crypto size");
	unsigned long long bincert_data_len_ul = 0ULL;
	std::vector<std::uint8_t> buf (signed_bincert_len + 128U, 0);
	const std::uint8_t *const signed_data = signed_certificate + sizeof
		(class version);
	const size_t signed_data_len = signed_bincert_len - sizeof(class version);
	int sr = crypto_sign_ed25519_open (buf.data(), &bincert_data_len_ul,
			signed_data, signed_data_len, provider_pubkey.bytes());
	if( bincert_data_len_ul >= sizeof(certificate_data) )
		this->data_ = *reinterpret_cast <const certificate_data*> (buf.data());
	else
		throw invalid_certificate();
	if (0 != sr)
	{
		this->clear();
		throw invalid_certificate ("Suspicious certificate. Fingerprints mismatch: "
			+ provider_pubkey.fingerprint() + " != "
			+ data_.server_publickey.fingerprint());
	}
	assert( bincert_data_len_ul == crypto_box_PUBLICKEYBYTES + 8U + 3U * 4U );
	const auto t = this->ts();
	if ( std::get<1>(t) <= std::get<0>(t))
	{
		this->clear();
		throw invalid_certificate ("This certificate has a bogus validity period");
	}
	const uint32_t now_u32 = static_cast<uint32_t> ( std::time(NULL) );
	if (now_u32 < std::get<0>(t))
	{
		this->clear();
		throw invalid_certificate ("Certificate has not been activated yet");
	}
	if (now_u32 > std::get<1>(t))
	{
		this->clear();
		throw invalid_certificate ("Certificate has expired");
	}
	raw_signed_.assign (signed_certificate, signed_bincert_len + signed_certificate);
	assert( raw_signed_.size() == signed_bincert_len );
}

certificate::~certificate()
{
	this->clear();
}

void certificate::clear()
{
	sodium_memzero(&data_, sizeof data_);
	if( !raw_signed_.empty() )
	{
		sodium_memzero(raw_signed_.data(), raw_signed_.size());
		raw_signed_.clear();
		raw_signed_.shrink_to_fit();
	}
}

inline struct std::tm gm_time (std::time_t t)
{
#ifdef HAVE_GMTIME_R
	struct std::tm result;
	const auto * gm = ::gmtime_r (&t, &result);
#else
#  ifdef __linux__
#    error "Linux has gmtime_r"
#  endif
	//! @todo not thread safe, uses system static 'struct tm' variable
	const auto * gm = std::gmtime (&t);
#endif
	if (nullptr == gm)
		throw std::system_error (errno, std::system_category(), "Failed to get UTC"
			" system time");
#ifdef HAVE_GMTIME_R
	return result;
#else
	return *gm;
#endif
}

void certificate::print_info() const
{
	const auto tsbe = this->ts();
	const std::time_t ts_begin_t = static_cast<time_t> (std::get<0>(tsbe)),
		ts_end_t = static_cast<time_t>( std::get<1>(tsbe) );
	assert(ts_begin_t > 0);
	assert(ts_end_t > 0);
	assert(ts_end_t >= ts_begin_t);
	const std::tm ts_begin_tm = gm_time (ts_begin_t), ts_end_tm = gm_time (ts_end_t);
	const uint32_t  serial = this->serial();
	// %F == %Y-%m-%d, see `man strftime`
	log::info ("Certificate #", serial, ", key: ", this->server_public_key(),
		" is valid from [",
		std::put_time (&ts_begin_tm, "%F"), "] to [",
		std::put_time (&ts_end_tm, "%F"), ']');
	if (this->version_.major[1] > 1U)
	{
		const unsigned int version_minor =
			this->version_.minor[0] * 256U + this->version_.minor[1];
		log::info ("Using version ", this->version_.major[1], '.', version_minor,
			   " of the DNSCrypt protocol");
	}
}

void certificate::check_key_rotation_period() const
{
	const auto t = this->ts();
	assert(std::get<1>(t) > std::get<0>(t));
	if (std::get<1>(t) - std::get<0>(t) > detail::recommended_key_rotation_period)
		log::warning ("The key rotation period for this server may "
			"exceed the recommended value. This is bad for forward secrecy.");
}

enum cipher certificate::cipher() const
{
	switch (this->version_.major[1])
	{
		case 1: return cipher::xsalsa20poly1305;
		case 2: return cipher::xchacha20poly1305;
	}
	log::error ("Unsupported certificate version");
	return cipher::undefined;
}

std::array<std::uint32_t,2> certificate::ts() const
{
	std::array<std::uint32_t, 2> result;
	assert( data_.ts_begin.size() == sizeof result[0] );
	std::memcpy(&result[0], &data_.ts_begin[0], sizeof result[0]);
	result[0] = htonl(result[0]);
	std::memcpy(&result[1], &data_.ts_end[0], sizeof result[1]);
	result[1] = htonl(result[1]);
	return result;
}

::crypt::pubkey certificate::server_public_key() const
{
	return data_.server_publickey;
}

}} // namespace dns::crypt
