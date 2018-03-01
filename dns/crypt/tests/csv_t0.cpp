#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "dns/crypt/csv.hxx"
#include "backtrace/catch.hxx"

const char valid_t1[] =
"   Col1,Col2  ,Col3,   Co4   ,\"Col5\",\"Col6\",Col7,\"C o l  8 \",   cl9\r\n";

const char valid_t2[] =
"\"   C ol1 ,\",\",C,o,l2  \", C ol3,\"   Col4\",\r\v\n";

const char valid_t7[] = "\"C ol1,\",,Col3,,\", Col5\",\"   Col6\",\n";

const char valid_t8[] = "C ol 1,,,\"Col 4\",,coL6,Col7,\n";

const char valid_t9[] = "Col1";

const char valid_t0[] = "cOlumn1\n";

const char invalid_t3[] =
"\"   C ol1 ,\",\",C,o,l2  \" \t\r \v , C ol3, \"   Col4\",\r\v\n";

const char invalid_t4[] = " \"Col1\"\n";

const char invalid_t5[] = "Col1,\"Col2\", col3 , \"col4\"\n";

const char invalid_t6[] = "coL1,cOl2, col3, col4 \t,\"cOluMn5\n";

void test_t1()
{
	std::istringstream istr (valid_t1);
	const auto words = csv::parse_line (istr);
	std::cout << "CSV1 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV1 column: " << w << ":::\n";
	assert (9u == words.size());
	assert ("   Col1" == words.at (0));
	assert ("   cl9" == words.at (8));
}

void test_t2 ()
{
	std::istringstream istr (valid_t2);
	const auto words = csv::parse_line (istr);
	std::cout << "CSV2 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV2 column: " << w << ":::\n";
	assert (5u == words.size());
	assert ("   C ol1 ," == words.at (0));
	assert ("   Col4" == words.at (3));
	assert (1 == words.at (4).length());
	assert ("\v" == words.at (4));
}

void test_t7 ()
{
	std::istringstream istr (valid_t7);
	const auto words = csv::parse_line (istr);
	std::cout << "CSV7 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV7 column: " << w << ":::\n";
	assert (7u == words.size());
	assert ("C ol1," == words.at (0));
	assert ("" == words.at (3));
	assert (words.at (3).empty());
	assert (", Col5" == words.at (4));
	assert (words.back().empty());
}

void test_t8 ()
{
	std::istringstream istr (valid_t8);
	const auto words = csv::parse_line (istr);
	std::cout << "CSV8 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV8 column: " << w << ":::\n";
	assert (8u == words.size());
	assert ("C ol 1" == words.at (0));
	assert ("Col 4" == words.at (3));
	assert (0 == words.at (4).length());
	assert ("" == words.at (4));
	assert (words.back().empty());
}

void test_t9 ()
{
	std::istringstream istr (valid_t9);
	const auto words = csv::parse_line (istr);
	std::cout << "CSV9 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV9 column: " << w << ":::\n";
	assert (1u == words.size());
	assert ("Col1" == words.at (0));
}

void test_t0 ()
{
	std::istringstream istr (valid_t0);
	const auto words = csv::parse_line (istr);
	std::cout << "CSV0 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV0 column: " << w << ":::\n";
	assert (1u == words.size());
	assert ("cOlumn1" == words.at (0));
}

void test_t11 ()
{
	const std::string str9 (valid_t9);
	assert ('\0' != str9.back());
	assert ('\n' != str9.back());
	assert ('\r' != str9.back());
	std::string str11 (str9);
	str11.push_back('\0');
	assert (str11.length() == str9.length() + 1);
	std::istringstream istr (std::move (str11));
	const auto words = csv::parse_line (istr);
	std::cout << "CSV11 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV11 column: " << w << ":::\n";
	assert (1u == words.size());
	assert (5 == words.at(0).size());
	assert ("Col1" == words.at (0).substr(0,4));
	assert ('\0' == words.at (0).back());
}

void test_t12 ()
{
	const std::string str9 (valid_t9);
	assert ('\0' != str9.back());
	assert ('\n' != str9.back());
	assert ('\r' != str9.back());
	std::string str12 (str9);
	str12.push_back ('\0');
	str12.push_back ('\0');
	str12.push_back ('\n');
	assert (str12.length() == str9.length() + 3);
	std::istringstream istr (std::move (str12));
	const auto words = csv::parse_line (istr);
	std::cout << "CSV12 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV12 column: " << w << ":::\n";
	assert (1u == words.size());
	assert (6 == words.at(0).size());
	assert ("Col1" == words.at (0).substr(0,4));
	assert ('\0' == words.at (0).at(5));
	assert ('\0' == words.at (0).at(4));
}

void test_t13 ()
{
	const std::string str13 ("\n");
	assert (str13.length() == 1);
	assert ('\n' == str13.back());
	std::istringstream istr (std::move (str13));
	auto words = csv::parse_line (istr);
	std::cout << "CSV13 nw: " << words.size() << '\n';
	assert (0 == words.size());
}

void test_t14 ()
{
	std::string str14 ("Column 1, linE1coL2");
	str14.push_back ('\0');
	str14.push_back ('\0');
	str14.push_back ('\n');
	str14.append ("Column1 line2,Col2line2");
	str14.push_back ('\0');
	str14.push_back ('\n');
	assert (str14.length() == 19 + 3  + 23 + 2);
	std::istringstream istr (std::move (str14));
	auto words = csv::parse_line (istr);
	std::cout << "CSV14_1 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV14_1 column: " << w << ":::" << w.size() << '\n';
	assert (2u == words.size());
	assert ("Column 1" == words.at (0));
	assert (12 == words.at(1).size());
	assert (" linE1coL2" == words.at (1).substr (0,10));
	assert ('\0' == words.at (1).at (10));
	assert ('\0' == words.at (1).at (11));
	//////
	csv::parse_line (istr, words);
	std::cout << "CSV14_2 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV14_2 column: " << w << ":::" << w.size() << '\n';
	assert (2 == words.size());
	assert (10 == words.at (1).length());
	assert ("Col2line2" == words.at (1).substr (0,9));
	assert ('\0' == words.at (1).at(9));
	/////
	csv::parse_line (istr, words);
	std::cout << "CSV14_3 nw: " << words.size() << '\n';
	for (const auto &w : words)
		std::cout << "CSV14_3 column: " << w << ":::" << w.size() << '\n';
	assert (0 == words.size());
}

void test_invalid (const char *const line, const std::string &expected)
{
	bool failed = false;
	try
	{
		std::istringstream istr (line);
		const auto words = csv::parse_line (istr);
		std::cout << "CSV invalid nw: " << words.size() << '\n';
	}
	catch (const std::runtime_error &e)
	{
		failed = true;
		std::cout << "Expected Failure! " << e.what()
			<< "\n................! " << expected << '\n';
		assert (expected == std::string (e.what()).substr (0, expected.length()));
	}
	assert (failed);
}

static void run ()
{
	test_t1();
	test_t2 ();
	/////
	test_invalid (invalid_t3, "CSV column: 2, has text after closing quote (5):");
	test_invalid (invalid_t4, "CSV column: 1 has qutation mark after (1):");
	test_invalid (invalid_t5, "CSV column: 4 has qutation mark after (1):");
	test_invalid (invalid_t6, "Unclosed quote in CSV column: 5, at word(7) cOluMn5");
	/////
	test_t7();
	test_t8();
	test_t9();
	test_t0();
	test_t11();
	test_t12();
	test_t13();
	test_t14();
}

int main ()
{
	return trace::catch_all_errors (run);
}
