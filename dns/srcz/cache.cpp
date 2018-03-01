#include <fstream>
#include <cassert>

#include "dns/cache.hxx"
#include "dns/query.hxx"
#include "dns/constants.hxx"
#include "sys/logger.hxx"
#include "sys/str.hxx"

namespace dns {

cache::cache(unsigned short minttl)
	: cache_entries_max (defaults::max_size()), min_ttl_ (minttl)
{}

const cache_entry * cache::find(const std::string &qname, const rr_type qtype) const
{
	// cache_key k;
	cache_entry k;
	//! @todo DNS uses limited ASCI encoding: letter, number, dash: [a-z,0-9,-],
	//! libidn, punycode
	std::string lower_qname = sys::ascii_tolower_copy (qname);
	//! @todo: adjust byte preceding the last one back to UPPERCASE
	//! in some wheired OPT ttl case? But it is not done during cache::store
#if 0
	if (wire_data[11] == 1)
	{
		uint16_t opt_type;
		uint32_t opt_ttl;

		if (next_rr(wire_data, wire_data_len, 0, NULL, &i,
					&opt_type, NULL, &opt_ttl) != 0) {
			throw std::runtime_error ("bad dns query");
		}
		if (opt_type != DNS_TYPE_OPT)
			return false; // query not found
		if ((opt_ttl & 0x8000) == 0x8000) {
			if (qname_len >= 2) {
				qname[qname_len - 2] = static_cast <uint8_t>
					(std::toupper(qname[qname_len - 2]));
			}
		}
	}
#endif
	k.first = std::move(lower_qname);
	assert (lower_qname.empty());
	k.second = static_cast <std::uint16_t> (qtype);
	const auto itr = cache_entries_.find(k);
	if( cache_entries_.end() != itr )
		return &*itr;
	return nullptr;
}

void cache::replace_entry(const std::string &wire_qname,
	const std::vector<uint8_t>  &wire_data, const std::int32_t ttl,
	const rr_type qtype)
{
	const cache_entry *scanned_cache_entry = this->find(wire_qname, qtype);
	std::time_t tnow = std::time (nullptr);
	if (scanned_cache_entry != nullptr)
	{
		scanned_cache_entry->response = wire_data;
		scanned_cache_entry->deadline = tnow + static_cast <std::time_t> (ttl);
	}
	else
	{
		cache_entry cache_entry;
		cache_entry.first = wire_qname;
		cache_entry.second = static_cast <std::uint16_t> (qtype);
		cache_entry.response = wire_data;
		cache_entry.deadline = tnow + static_cast <std::time_t> (ttl);
		this->cache_entries_.insert (std::move (cache_entry));
	}
}

void cache::store(const query &msg)
{
	assert (msg.is_dns());
	//! @todo is TTL time_t or uint32 ?
	const std::uint8_t *const ub = msg.bytes();
	//! @todo what is this?
	if (! (((ub[2] & 2) != 0) || ((ub[3] & 0xf) != 0 && (ub[3] & 0xf) != 3)) )
	{
		const auto qq = msg.get_question();
		if( rr_class::internet == std::get <rr_class> (qq) )
		{
			const std::int32_t min_ttl = std::min (defaults::max_ttl(), std::max
				(msg.answer_min_ttl(), static_cast<std::int32_t> (this->min_ttl())));
			const std::vector <std::uint8_t> wire (msg.bytes(), msg.bytes()
				+ msg.size());
			auto hostname = sys::ascii_tolower_copy (std::get <std::string> (qq));
			process::log::info ("Caching: ", hostname, " size: ", msg.size(),
				", TTL: ", min_ttl);
			//! @todo not adjusting here byte preceding the last one?
			//! see cache::find
			this->replace_entry (hostname, wire, min_ttl, std::get <rr_type> (qq));
		}
	}
}

bool cache::retrieve(query &msg)
{
	auto q = msg.get_question();
	const auto hn = std::get<std::string>(q);
	const cache_entry *ent = this->find (hn, std::get <rr_type> (q));
	std::time_t tnow = std::time (nullptr);
	process::log::debug ("CACHE size: ", this->cache_entries_.size(), ", entry: ",
		ent);
	if (nullptr != ent && ent->response.size() <= msg.max_size &&
		tnow < ent->deadline)
	{
		const std::uint16_t tid = msg.header().id;
		std::int32_t ttl = static_cast <std::int32_t> (ent->deadline - tnow);
		//! @todo numeric_cast
		msg = query (ent->response.data(), static_cast <query::size_type>
			(ent->response.size()));
		//! @todo replace query with the one from the question
		//! or merge question and answer
		msg.adjust_id_and_ttl (tid, ttl);
		return true;
	}
	return false;
}

template <typename T> inline void wrt (std::ostream &of, const T *d, std::size_t s)
{
	//! @todo numeric_cast
	of.write (reinterpret_cast <const char*> (d), static_cast <std::streamsize> (s));
}

template <typename T> inline void wrt (std::ostream &of, const T &d)
{
	of.write (reinterpret_cast <const char*> (&d), sizeof (T));
}

void cache::save_as (const std::string &filename) const
{
	std::ofstream ofile (filename, std::ios::binary);
	wrt (ofile, "mgck", 4);
	constexpr const std::int32_t magick = 1'023'456'789;
	wrt (ofile, magick);
	wrt (ofile, "size", 4);
	wrt (ofile, this->cache_entries_.size());
	for (const auto & ent : this->cache_entries_)
	{
		wrt (ofile, "entr", 4);
		wrt (ofile, ent.first.size());
		wrt (ofile, ent.first.c_str(), ent.first.size()); // name
		wrt (ofile, ent.second); // type
		wrt (ofile, ent.deadline);
		wrt (ofile, "dns:", 4);
		wrt (ofile, ent.response.size());
		//! @todo: assert (is_dns (ent.response));
		wrt (ofile, ent.response.data(), ent.response.size());
	}
	if (!ofile)
		throw std::runtime_error ("Failed to save DNS cache into: " + filename);
}

template <typename T> inline void rd (std::ifstream &ifile, T &d)
{
	ifile.read (reinterpret_cast <char *> (&d), sizeof (T));
}

template <typename T> inline T crd (std::ifstream &ifile)
{
	T d=0;
	ifile.read (reinterpret_cast <char *> (&d), sizeof (T));
	if (!ifile)
		throw std::runtime_error ("wrong file");
	return d;
}

template <typename T> inline void crd (std::ifstream &ifile, T *d, std::size_t s)
{
	//! @todo numeric_cast
	ifile.read (reinterpret_cast <char *> (d), static_cast <std::streamsize> (s));
	if (!ifile)
		throw std::runtime_error ("wrong file");
}

template <std::uint8_t N> inline std::array <char, N> crd (std::istream &ifile)
{
	std::array <char, N> d;
	ifile.read (&d[0], N);
	if (!ifile && !ifile.eof())
		throw std::runtime_error ("Can't read file");
	return d;
}

typedef std::array <char, 4> key_t;

cache cache::load (const std::string &filename, unsigned short minttl)
{
	std::ifstream ifile (filename, std::ios::binary);
	if (crd<4> (ifile) != key_t{{'m', 'g', 'c', 'k'}})
		throw std::runtime_error ("Not a DNS cache (wrong magick): " + filename);
	if (1023456789 != crd <std::int32_t> (ifile))
		throw std::runtime_error ("Not a DNS cache (wrong byte order): "
			+ filename);
	if (crd<4> (ifile) != key_t{{'s','i','z','e'}})
		throw std::runtime_error ("Not a DNS cache (no size): " + filename);
	const auto nq = crd <std::size_t> (ifile);
	std::size_t n=0;
	cache result (minttl);
	while (ifile)
	{
		const key_t key = crd<4> (ifile);
		if (ifile.eof())
			break;
		if (key != key_t{{'e','n','t','r'}})
			throw std::runtime_error ("corrupt DNS cache file: " + filename);
		auto l = crd <std::size_t> (ifile);
		if (1u > l || 300u < l)
			throw std::runtime_error ("corrupt DNS name in: " + filename);
		std::string name(l, '\0');
		crd (ifile, &name[0], name.size()); // name
		const auto qtype = crd <unsigned short> (ifile);
		const auto deadline = crd <std::time_t> (ifile);
		if (crd<4> (ifile) != key_t {{'d','n','s',':'}})
			throw std::runtime_error ("corrupt DNS entry in: " + filename);
		l = crd <std::size_t> (ifile);
		query msg;
		if (12u > l || query::max_size < l || l > msg.reserved_size())
			throw std::runtime_error ("invalid DNS message in: " + filename);
		crd (ifile, msg.modify_bytes(), l);
		msg.set_size (static_cast <query::size_type> (l));
		++n;
		if (n > nq)
			throw std::runtime_error ("Too many entries in DNS cache: " + filename);
		// assert (std::get <std::string> (msg.get_question()) == name);
		cache_entry entry;
		entry.first = std::move (name);
		entry.second = qtype;
		entry.response = std::vector <std::uint8_t> (msg.bytes(), msg.bytes()
			+ msg.size());
		entry.deadline = deadline;
		result.cache_entries_.insert (std::move (entry));
	}
	if (result.size() < nq)
		throw std::runtime_error ("too few entries");
	return result;
}

void cache::collect_garbage()
{
	std::time_t tnow = std::time (nullptr);
	for (auto e = cache_entries_.begin(); e != cache_entries_.end();)
	{
		if (e->deadline < tnow)
			e = cache_entries_.erase (e);
		else
			e = std::next (e);
	}
}

} // namespace dns
