#ifndef _DNS_CACHE_HXX_
#define _DNS_CACHE_HXX_ 1

#include <cinttypes>
#include <unordered_set>
#include <vector>
#include <ctime>

#include <dns/dll.hxx>
#include <dns/fwd.hxx>

namespace dns {

typedef std::pair<std::string,std::uint16_t> k_t;
// typedef std::string cache_key;
typedef k_t cache_key;

namespace detail {
// from boost/functional/hash/hash.hpp
inline void hash_combine (std::size_t& seed, std::size_t value) noexcept
{
	seed ^= value + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

} // namespace dns::detail

struct su_hash
{
	std::size_t operator() (const cache_key &h) const noexcept
	{
		std::size_t seed = 0;
		detail::hash_combine (seed, std::hash <std::string>() (h.first));
		detail::hash_combine (seed, std::hash <std::uint16_t>() (h.second));
		return seed;
	}
};

struct cache_entry : public cache_key
{
	mutable std::time_t deadline;
	mutable std::vector<std::uint8_t> response;
};

typedef std::unordered_set<cache_entry, su_hash> cache_list_t;
// typedef std::unordered_map<cache_key,cache_response, su_hash> cache_list_t;

class DNS_API cache
{
public:
	struct DNS_NO_EXPORT defaults
	{
		// seconds
		// DNS_NO_EXPORT static constexpr const int max_ttl = 86400, min_ttl = 60;
		static inline constexpr int max_ttl() noexcept {return 86400;}
		static inline constexpr int min_ttl() noexcept {return 60;}
		static inline constexpr unsigned max_size() noexcept {return 50;}
	};

	explicit cache (unsigned short minttl);

	time_t min_ttl() const noexcept {return min_ttl_;}

	std::size_t size() const {return cache_entries_.size();}

	const cache_entry * find(const std::string &qname, rr_type qtype) const;

	void replace_entry(const std::string &wire_qname,
		const std::vector<uint8_t>  &wire_data, std::int32_t ttl, rr_type qtype);

	/*! @return -1 if error, 1 if cached entry was retrieved, 0 if not
	 */
	bool retrieve(query &q);

	//! Store DNS query in cache if necessary
	void store(const query &q);

	static cache load (const std::string & file_name, unsigned short minttl);

	void save_as (const std::string & file_name) const;

	//! Remove expired entries
	void collect_garbage();

private:

	cache_list_t cache_entries_;
	//! @todo unused
	std::size_t cache_entries_max;
	time_t min_ttl_, now_;
};

} // namespace dns

#endif
