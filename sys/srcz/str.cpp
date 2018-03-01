#include <locale>
#include <algorithm>
#include <cassert>

#include "sys/str.hxx"

namespace sys {

SYS_API void ascii_tolower (char *str)
{
	const std::locale &cloc = std::locale::classic();
	assert (nullptr != str);
	while ('\0' != *str)
	{
		*str = std::tolower <char> (*str, cloc);
		str++;
	}
}

SYS_API int ascii_strcasecmp (const char *s1, const char *s2)
{
	const std::locale &cloc = std::locale::classic();
	while (true)
	{
		char c1, c2;
		c1 = std::tolower <char> (*s1++, cloc);
		c2 = std::tolower <char> (*s2++, cloc);
		if (c1 < c2)
			return -1;
		else if (c1 > c2)
			return 1;
		else if (c1 == 0 || c2 == 0)
			return 0;
	}
}

SYS_API std::string ascii_tolower_copy (const char *str)
{
	const std::locale &cloc = std::locale::classic();
	std::string result;
	assert (nullptr != str);
	while ('\0' != *str)
	{
		result.push_back (std::tolower <char> (*str, cloc));
		++str;
	}
	return result;
}

namespace detail {
template <typename Iterator> inline void ascii_tolower (const std::string &str,
	Iterator out)
{
	const std::locale &cloc = std::locale::classic();
	std::transform (std::begin (str), std::end (str), out, [cloc] (char ch)
		{
			return std::tolower <char> (ch, cloc);
		});
}
}

SYS_API void ascii_tolower (std::string &str)
{
	detail::ascii_tolower (str, std::begin (str));
}

SYS_API std::string ascii_tolower_copy (const std::string &str)
{
	std::string result;
	result.reserve (str.size());
	detail::ascii_tolower (str, std::back_inserter (result));
	return result;
}

SYS_API std::string ascii_tolower_copy (std::string &&str)
{
	ascii_tolower (str);
	return std::move (str);
}

} // namespace sys
