#include <fstream>
#include <stdexcept>
#include <sstream>
#include <cstring>
#include <limits>

#include "dns/crypt/csv.hxx"
#include "crypt/crypt.hxx"
#include "dns/crypt/resolver.hxx"
#include "sys/logger.hxx"
#include "sys/str.hxx"

namespace dns { namespace crypt {

constexpr const unsigned short csv_max_columns = 50;

static_assert (std::is_nothrow_move_constructible <resolver>::value, "move");
static_assert (std::is_nothrow_move_assignable <resolver>::value, "move");
static_assert (std::is_copy_constructible <resolver>::value, "copy");
static_assert (std::is_copy_assignable <resolver>::value, "copy");
static_assert (!std::is_nothrow_copy_assignable <resolver>::value, "copy");
static_assert (!std::is_nothrow_copy_assignable <std::string>::value, "copy");

static_assert (std::is_nothrow_move_constructible <network::provider>::value, "mov");
static_assert (std::is_nothrow_move_assignable <network::provider>::value, "move");
static_assert (std::is_nothrow_copy_assignable <network::provider>::value, "copy");
static_assert (std::is_nothrow_copy_constructible <network::provider>::value, "cop");

std::size_t find_col (const std::vector <std::string> &headers,
	const std::vector <std::string> &cols, const char * const header)
{
	size_t i = 0UL;
	while (i < headers.size())
	{
		if (header == headers[i])
		{
			if (i < cols.size())
			{
				return i;
			}
			break;
		}
		i++;
	}
	return std::numeric_limits <std::size_t>::max();
}

using ::crypt::pubkey;
using ::crypt::nonce;

resolver parse_resolver (const std::vector <std::string> &headers,
	const std::vector <std::string> &cols, network::proto defp)
{
	const std::size_t max_cols = cols.size();
	const std::size_t provider_name_i = find_col (headers, cols, "Provider name");
	if (provider_name_i >= max_cols || cols[provider_name_i].empty())
		throw std::runtime_error ("Resolvers list is missing a provider name");
	std::size_t resolver_name_i = find_col (headers, cols, "Name");
	if (max_cols <= resolver_name_i)
		resolver_name_i = provider_name_i;
	const std::size_t provider_publickey_i = find_col (headers, cols,
		"Provider public key");
	const std::size_t resolver_ip_i = find_col (headers, cols, "Resolver address");
	if (provider_publickey_i >= max_cols || cols[provider_publickey_i].empty())
		throw std::runtime_error ("Public key is missing for ["
			+ cols[resolver_name_i] + ']');
	if (resolver_ip_i >= max_cols || cols[resolver_ip_i].empty())
		throw std::runtime_error ("Resolver address is missing for ["
			+ cols[resolver_name_i] + "]");

	const std::size_t dnssec_i = find_col (headers, cols, "DNSSEC validation");
	const bool dnssecb = !(dnssec_i < max_cols && sys::ascii_strcasecmp
		(cols[dnssec_i].c_str(), "yes") != 0);
	std::size_t namecoin_i = find_col (headers, cols, "Namecoin");
	bool namecoinb = (namecoin_i < max_cols && sys::ascii_strcasecmp
		(cols[namecoin_i].c_str(), "yes") == 0);
	const std::size_t nologs_i = find_col (headers, cols, "No logs");
	bool loggb = (nologs_i < max_cols && sys::ascii_strcasecmp
		(cols[nologs_i].c_str(), "no") == 0);

	// std::string copy constructor sets capacity == length for long strings
	return resolver (std::string (cols[resolver_name_i]),
		std::string (cols[provider_name_i]),
		::crypt::pubkey (cols[provider_publickey_i]),
		network::address (cols[resolver_ip_i], dns::crypt::defaults::port),
		defp, dnssecb, namecoinb, loggb);
}

std::vector<resolver> parse_resolvers_list (const char *file_name,
	network::proto defproto)
{
	std::vector<resolver> resolvers;
	resolvers.reserve (256);
	std::ifstream csv_text_file (file_name);
	if (!csv_text_file)
		throw std::runtime_error ("Failed to read resolvers from: "
			+ std::string(file_name));
	std::vector <std::string> headers, cols;
	csv::parse_line (csv_text_file, headers);
	const size_t headers_count = headers.size();
	if (headers_count < 3U || headers_count > csv_max_columns)
		throw std::runtime_error ("Invalid number of columns in the CSV resolver "
			"file");
	std::size_t lines = 1;
	do
	{
		++lines;
		csv::parse_line (csv_text_file, cols);
		const size_t cols_count = cols.size();
		if (0 < cols_count)
		{
			if (cols_count != headers_count)
				throw std::runtime_error ("CSV line: " + std::to_string (lines)
					+ ", columns: " + std::to_string (cols_count)
					+ " != " + std::to_string (headers_count) + " (headers)");
			resolvers.emplace_back (parse_resolver (headers, cols, defproto));
		}
	}
	while (csv_text_file);
	return resolvers;
}

void save_resolvers (const std::vector <resolver> &rs, const std::string &filename)
{
	std::ofstream ofile (filename);
	ofile << "Resolver address,Provider name,Provider public key\n";
	for (const auto &r : rs)
		ofile << r.address() << ',' << r.hostname() << ','
			<< r.public_key() << '\n';
	if (!ofile)
		throw std::runtime_error ("Failed saving resolvers into: " + filename);
}

resolver::resolver (const std::string &hn, const network::address &addr,
	const ::crypt::pubkey &pubkey, network::proto t) : network::provider (addr, t),
	pubkey_(pubkey), resolver_pubkey_(), name_("_dummy_"),
	host_name_(hn), dnssec_(false),
	namecoin_(false), logging_(false), ready_(false), dnscrypt_client_(true)
{
}

resolver::resolver (std::string &&_name, std::string &&hn, ::crypt::pubkey &&ks,
	network::address &&a, network::proto t, bool dnssec, bool namecoin,
	bool logg) noexcept : network::provider (std::move (a), t),
	pubkey_ (std::move (ks)), resolver_pubkey_(),
	name_ (std::move (_name)), host_name_ (std::move (hn)),
	dnssec_(dnssec), namecoin_(namecoin),
	logging_(logg), ready_(false), dnscrypt_client_(true)
{
}

std::string resolver::fingerprint() const
{
	return pubkey_.fingerprint();
}

std::string resolver::local_fingerprint() const
{
	return resolver_pubkey_.fingerprint();
}

std::string resolver::encryptor_fingerprint() const
{
	return this->encryptor().public_key().fingerprint();
}

struct message : public query
{
	const char *dummy() const override {return "dnscrypt";}

	std::unique_ptr <packet> clone() const override
	{
		return std::make_unique <message> (*this);
	}

private:
public:

	message() = default;

	//! @todo numeric_cast
	explicit message (const network::packet &other) : query (other.bytes(),
		other.size()) {}

	nonce session_nonce_;
};

std::unique_ptr<network::packet> resolver::adapt_message (const std::unique_ptr
	<network::packet> &other) const
{
	assert (other);
	return std::make_unique <message> (*other);
}

void resolver::unfold(network::packet &q)
{
	assert (typeid (q) == typeid (message));
	auto &msg = dynamic_cast<message & > (q);
	this->encryptor().uncurve(msg.session_nonce_, msg);
	process::log::debug ("Decrypted response, size: ", q.size());
	msg.session_nonce_.clear();
}

void resolver::fold(network::packet &q)
{
	assert (typeid (q) == typeid (message));
	auto &msg = dynamic_cast<message & > (q);
	this->encryptor().curve(msg.session_nonce_, msg);
	process::log::debug ("Encrypted size: ", msg.size());
	//! @todo should I store produced nonce in query?
}

}} // namespace dns::crypt
