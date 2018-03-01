#include <fstream>

#include "dns/hosts.hxx"
#include "network/address.hxx"

namespace dns {

//! @todo: is hosts file in utf8?
hosts::hosts (const std::string &file_name, bool disable_ipv6)
{
	std::ifstream f(file_name);
	if (!f)
		throw std::runtime_error ("failed to open file: " + file_name);
	std::string ip;
	std::string word;
	ip.reserve (511);
	word.reserve (511);
	bool no_end = true;
	do
	{
		bool line_break, word_break;
		char ch;
		no_end = static_cast <bool> (f.get (ch));
		// bool end = f.eof() || !f.good() || f.fail() || f.bad();
		if (!no_end || '\n' == ch || '#' == ch)
		{
			line_break = true;
			word_break = true;
			if ('#' == ch)
				f.ignore (100000, '\n'); // skip to the end of line
		}
		// else if (std::isspace (ch)) what about '\0'?
		else if ('\r' == ch || '\t' == ch || '\v' == ch || ' ' == ch)
		{
			word_break = true;
			line_break = false;
		}
		else
		{
			word_break = false;
			line_break = false;
			word.push_back (ch);
			if (2048 <= word.length())
				throw std::runtime_error ("Too long word: " + word
					+ " in hosts file: " + file_name);
		}
		if (word_break && !word.empty())
		{
			if (ip.empty())
			{
				if (!line_break)
				{
					const network::inet af = network::is_ip_address (word.c_str());
					if (network::inet::ipv4 == af || !disable_ipv6)
						ip = word;
					else
						f.ignore (100000, '\n'); // skip to the end of line
				}
			}
			else
			{
				std::string &str = this->host_ip_map_[word];
				if (str.empty())
					str = ip;
			}
			word.clear();
		}
		if (line_break)
			ip.clear();
	}
	while (no_end);
}

const std::string hosts::response(const std::string &ip) const
{
	const auto itr = host_ip_map_.find(ip);
	if( host_ip_map_.end() != itr )
		return itr->second;
	else
		return std::string("");
}

} // namespace dns
