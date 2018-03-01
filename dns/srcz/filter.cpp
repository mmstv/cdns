#include <cassert>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <type_traits>

#include "sys/str.hxx"
#include "dns/filter.hxx"

namespace dns {

static_assert (std::is_nothrow_move_assignable <filter>::value, "move");
static_assert (std::is_nothrow_move_constructible <filter>::value, "move");

enum class filters : unsigned short
{
	undefined,
	exact,
	prefix,
	suffix,
	substring
};

constexpr const unsigned short MAX_QNAME_LENGTH = 1024;

namespace detail {

template <typename Set> inline bool substr_match (const Set &subs,
	const std::string &str)
{
	return std::find_if (subs.cbegin(), subs.cend(), [&str] (const std::string &s)
		{return str.length() >= s.length() && std::string::npos != str.find (s);})
		!= subs.cend();
}

#if 1
template <typename Set> void substr_insert (Set &subs, const std::string &w)
{
	for (auto i = subs.cbegin(); i != subs.cend(); ++i)
	{
		if (std::string::npos != w.find (*i))
			return;
		if (std::string::npos != i->find (w))
		{
			for (auto j = subs.erase (i); j != subs.end();)
			{
				if (std::string::npos != j->find (w))
					j = subs.erase (j);
				else
					++j;
			}
			subs.insert (w);
			return;
		}
	}
	subs.insert (w);
}
#else
template <typename Set> void substr_insert (Set &subs, const std::string &w)
{
	if (!substr_match (subs, w))
	{
		for (auto it = subs.begin(); it != subs.end();)
		{
			if (std::string::npos != it->find (w))
				it = subs.erase (it);
			else
				++it;
		}
		subs.insert (w);
	}
}
#endif

template <typename Comp>
bool contains (const std::string &longer, const std::string &shorter);

template <>
inline bool contains <comp> (const std::string &longer, const std::string &shorter)
{
	return shorter.length() <= longer.length() && 0 == longer.compare
		(0, shorter.length(), shorter);
}

template <> inline bool contains <rev_comp> (const std::string &longer,
	const std::string &shorter)
{
	return shorter.length() <= longer.length() && 0 == longer.compare
		(longer.length() - shorter.length(), shorter.length(), shorter);
}

template <typename Cmp> inline bool words_match (const std::set <std::string, Cmp>
	&words, const std::string &str)
{
	// ATTN! performance, 'const char*' in upper_bound creates temporary std::string
	const auto si = words.upper_bound (str);
	return words.cbegin() != si && contains <Cmp> (str, *std::prev (si));
}

template <typename Cmp> void words_insert (std::set <std::string, Cmp> &words,
	const std::string &w)
{
#if 0
	////////////////// more comparisions, less mallocs
	const auto ub = words.upper_bound (w);
	if (words.cbegin() == ub || !contains <Cmp> (w, *std::prev (ub)))
	{
		auto i = words.insert (ub, w);
		auto m = std::next (i);
		// auto m = ub;
		while (words.cend() != m && contains <Cmp> (*m, w))
			m = words.erase (m);
	}
#else
	////////////////// less comparisons, more mallocs
	// Emplace uses more malloc's than insert
	const auto i = words.insert (w);
	if (i.second) // inserted
	{
		assert (words.cend() != i.first);
		if (words.cbegin() != i.first && contains <Cmp> (w, *std::prev (i.first)))
			words.erase (i.first);
		else
		{
			auto n = std::next (i.first);
			while (words.cend() != n && contains <Cmp> (*n, w))
				n = words.erase (n);
		}
	}
#endif
}

} // namespace dns::detail

bool filter::match (const std::string &hostname) const
{
	const std::size_t len = hostname.length();
	if (len > MAX_QNAME_LENGTH) //! @todo this is after puny encoding?
		throw std::runtime_error ("too long host name: " + hostname);
	const auto end = (len > 1UL && '.' == hostname.back()) ?
		std::prev (hostname.cend()) : hostname.cend();
	if (hostname.cbegin() == end)
		throw std::runtime_error ("too short host name: " + hostname);
	// In C++11 string is local if it is shorter than 15 chars
	std::string low (hostname.cbegin(), end);
	sys::ascii_tolower (low); //! @todo libidn punycode?
	if (exact_.end() != exact_.find (low))
		return true;
	if ('.' != low.front())
		low.insert (low.begin(), '.');
	assert ('.' == low.front());
	if (detail::words_match (suffix_, low))
		return true;
	if ('.' != low.back())
		low.push_back ('.');
	assert ('.' == low.front());
	if (detail::substr_match (substrings_, low))
		return true;
	if ('.' == low.front())
		low.erase (low.begin());
	assert ('.' != low.front());
	assert ('.' == low.back());
	return detail::words_match (prefix_, low);
}

filter::filter (std::istream &&text_stream)
{
	std::string word; // local string (less than 15 chars), no malloc
	word.reserve (127); // first allocation
	while (text_stream >> word)
	{
		assert (!word.empty());
		if ('#' == word.front())
		{
			text_stream.ignore (10000000, '\n');
			continue;
		}
		if (word.length() > MAX_QNAME_LENGTH)
			throw std::runtime_error ("Too long filter entry: " + word);
		char *word_cstr = &word[0];
		filters filter_type = filters::undefined;
		if ('*' == word.front() && '*' == word.back())
		{
			word.pop_back();
			assert (word_cstr == &word[0]);
			word_cstr++;
			filter_type = filters::substring;
		}
		else if ('*' == word.back())
		{
			word.pop_back();
			assert (word_cstr == &word[0]);
			filter_type = filters::prefix;
		}
		else
		{
			filter_type = filters::exact;
			if ('*' == word.front())
			{
				word_cstr++;
				assert ('.' == *word_cstr);
			}
			if ('.' == word_cstr[0])
				filter_type = filters::suffix;
		}
		// assert ('\0' != *word_cstr);
		if ('\0' == *word_cstr)
			continue;
		sys::ascii_tolower (word_cstr); //! @todo punycode, libidn
		word = word_cstr;
		if (filters::exact == filter_type)
		{
			assert ('.' != word.front());
			this->exact_.insert (word);
		}
		else if (filters::suffix == filter_type)
		{
			assert ('.' == word.front());
			detail::words_insert (suffix_, word);
		}
		else if (filters::prefix == filter_type)
		{
			detail::words_insert (prefix_, word);
		}
		else if (filters::substring == filter_type)
		{
			detail::substr_insert (substrings_, word);
		}
	}
	this->optimize();
}

filter::filter (const std::string &file_name) : filter (std::ifstream (file_name)) {}

void filter::merge (filter &&other)
{
	if (prefix_.empty())
		prefix_.swap (other.prefix_);
	else
	{
		for (const auto &w : other.prefix_)
			detail::words_insert (prefix_, w);
	}
	if (suffix_.empty())
		suffix_.swap (other.suffix_);
	else
	{
		for (const auto &w : other.suffix_)
			detail::words_insert (suffix_, w);
	}
	if (substrings_.empty())
		substrings_.swap (other.substrings_);
	else
	{
		for (const auto &w : other.substrings_)
			detail::substr_insert (substrings_, w);
	}
	if (exact_.empty())
		exact_.swap (other.exact_);
	else
		exact_.insert (other.exact_.begin(), other.exact_.end());
	//std::set_union (exact_.begin(), exact_.end(), other.exact_.begin(),
	// other.exact_.end(), std::inserter (exact_, exact_.end()));
	// std::merge
	this->optimize();
}

void filter::write (std::ostream &text_stream) const
{
	if (!substrings_.empty())
		text_stream << "\n# filter substring domains\n";
	std::size_t line_len = 0;
	constexpr const unsigned max_len = 85;
	for (const auto &d : substrings_)
	{
		line_len += (d.length()+3);
		if (line_len > max_len)
		{
			text_stream << '\n';
			line_len = d.length() + 2;
		}
		else if (d.length()+3 != line_len)
			text_stream << ' ';
		text_stream << '*' << d << '*';
	}
	if (!prefix_.empty())
		text_stream << "\n\n# prefix domains\n";
	line_len = 0;
	for (const auto &d : prefix_)
	{
		line_len += d.length() + 2;
		if (line_len > max_len)
		{
			text_stream << '\n';
			line_len = d.length() + 1;
		}
		else
			text_stream << ' ';
		text_stream << d << '*';
	}
	if (!suffix_.empty())
		text_stream << "\n\n# suffix domains\n";
	line_len = 0;
	for (const auto &d : suffix_)
	{
		line_len += (d.length() + 1);
		if (line_len > max_len)
		{
			text_stream << '\n';
			line_len = d.length();
		}
		else
			text_stream << ' ';
		text_stream << d;
	}
	if (!exact_.empty())
		text_stream << "\n\n# exact hosts\n";
	line_len = 0;
	for (const auto &d : exact_)
	{
		line_len += (d.length() + 1);
		if (line_len > max_len)
		{
			text_stream << '\n';
			line_len = d.length();
		}
		else
			text_stream << ' ';
		text_stream << d;
	}
}

void filter::optimize (void)
{
	std::string str;
	for (auto di = suffix_.begin(); di != suffix_.end();)
	{
		const auto &d = *di;
		assert ('.' == d.front());
		str.clear();
		str.append (d);
		if ('.' != str.back())
			str.push_back ('.');
		if (detail::substr_match (substrings_, str))
			di = suffix_.erase (di);
		else
			++di;
	}
	for (auto pi = prefix_.begin(); pi != prefix_.end();)
	{
		const auto &d = *pi;
		str.clear();
		if ('.' != d.front())
			str.push_back ('.');
		str.append (d);
		assert ('.' == str.back());
		if (detail::substr_match (substrings_, str))
			pi = prefix_.erase (pi);
		else
			++pi;
	}
	for (auto ei = exact_.begin(); ei != exact_.end();)
	{
		const auto &w = *ei;
		str.clear();
		if ('.' != w.front())
			str.push_back ('.');
		str.append (w);
		if (detail::words_match (suffix_, str))
			ei = exact_.erase (ei);
		else
		{
			if ('.' != str.back())
				str.push_back ('.');
			if (detail::substr_match (substrings_, str))
				ei = exact_.erase (ei);
			else
			{
				if ('.' == str.front())
					str.erase (str.begin());
				if (detail::words_match (prefix_, str))
					ei = exact_.erase (ei);
				else
					++ei;
			}
		}
	}
}

} // namespace dns
