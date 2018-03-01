#include <cassert>
#include <stdexcept>
#include <vector>
#include <istream>
#include <string>

#include "dns/crypt/csv.hxx"

namespace csv
{

constexpr const char separator = ',', quote = '"';

inline std::string *cur_word (std::vector <std::string> &words, std::size_t &i)
{
	assert (i<=words.size());
	if (i >= words.size())
		words.emplace_back (std::string());
	assert (i<words.size());
	auto &w = words.at (i);
	w.clear();
	return &w;
}

void parse_line (std::istream &text_stream, std::vector <std::string> &words)
{
	char ch = '\0';
	if (16 > words.capacity())
		words.reserve (16);
	bool quote_flag = false, closing_quote_flag = false;
	std::size_t i = 0;
	std::string *word_ptr = cur_word (words, i);
	while (text_stream.get (ch) && '\n' != ch)
	{
		if ((separator != ch && quote != ch && '\r' != ch)
			|| ((separator == ch) && quote_flag))
		{
			if (32000u < word_ptr->length())
				throw std::runtime_error ("Too long word in column");
			// reducing number of memory allocations, at the expence of memory size
			const auto cap = word_ptr->capacity();
			if (word_ptr->length() >= cap && 95 > cap) // 95 = 64 + 32 - 1
				word_ptr->reserve (95); // default is to double the capacity
			word_ptr->push_back (ch);
		}
		else if (quote == ch)
		{
			if (quote_flag)
			{
				quote_flag = false; // closing quote
				closing_quote_flag = true;
				++i;
				word_ptr = cur_word (words, i);
			}
			else
			{
				const std::string &word = *word_ptr;
				if (!word.empty())
					throw std::runtime_error ("CSV column: " + std::to_string
						(words.size()) + " has qutation mark after ("
						+ std::to_string (word.length()) + "): " + word);
				quote_flag = true;
			}
		}
		else if (separator == ch)
		{
			if (!closing_quote_flag)
			{
				++i;
				word_ptr = cur_word (words, i);
			}
			else
			{
				const std::string &word = *word_ptr;
				if (!word.empty())
					throw std::runtime_error ("CSV column: " + std::to_string
						(i) + ", has text after closing quote ("
						+ std::to_string (word.length()) + "): " + word);
				closing_quote_flag = false;
			}
		}
	}
	const std::string &word = *word_ptr;
	if (quote_flag)
		throw std::runtime_error ("Unclosed quote in CSV column: "
			+ std::to_string (words.size()) + ", at word("
			+ std::to_string (word.length()) + ") " + word);
	if (!closing_quote_flag && (0 < i || !words.at(0).empty()))
		++i;
	assert (words.size() >= i);
	if (words.size() > i)
		words.resize (i);
	assert (words.size() == i);
}

std::vector <std::string> parse_line (std::istream &text_stream)
{
	std::vector <std::string> words;
	parse_line (text_stream, words);
	return words;
}

} // namespace csv
