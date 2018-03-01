#ifndef DNS_FILTER_HXX_
#define DNS_FILTER_HXX_

#include <iosfwd>
#include <set>
#include <unordered_set>
#include <string>

#include <dns/dll.hxx>

namespace dns {

struct comp
{
	bool operator() (const std::string &a, const std::string &b) const
	{
		// ads.* < ads.bla.bla.
		return std::lexicographical_compare (a.cbegin(), a.cend(), b.cbegin(),
			b.cend());
	}
};

struct rev_comp
{
	bool operator() (const std::string &a, const std::string &b) const
	{
		// *.ads.com < bla.bla.ads.com
		return std::lexicographical_compare (a.crbegin(), a.crend(), b.crbegin(),
			b.crend());
	}
};

class DNS_API filter
{
public:

	explicit filter (const std::string &file_name);

	filter() = default;

	explicit filter (std::istream &&text_stream);

	bool match (const std::string &hostname) const;

	std::size_t count() const
	{
		return suffix_.size() + prefix_.size() + substrings_.size() + exact_.size();
	}

	bool empty() const
	{
		return suffix_.empty() && prefix_.empty() && substrings_.empty()
			&& exact_.empty();
	}

	void merge (filter &&);

	void write (std::ostream &text_stream) const;

	void optimize (void);

private:

	std::set <std::string, comp> prefix_;
	std::set <std::string, rev_comp> suffix_;
	std::unordered_set <std::string> substrings_, exact_;
};

inline std::ostream& operator<< (std::ostream &text_stream, const filter &f)
{
	f.write (text_stream);
	return text_stream;
}

} // namespace dns
#endif
