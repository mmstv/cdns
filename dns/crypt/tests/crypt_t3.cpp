#undef NDEBUG

#include <iostream>
#include <cassert>
#include <sstream>
#include <iomanip>

#include "backtrace/catch.hxx"

#include "crypt/crypt.hxx"

using namespace std;

static void run()
{
	crypt::nonce n0 = crypt::nonce::make_random ();
	cout << "n0: " << n0.fingerprint() << '\n';
	crypt::nonce n1 = crypt::nonce::make_random ();
	cout << "n1: " << n1.fingerprint() << '\n';
	assert (n1 != n0);
	////////////
	const std::string pk1str ("10F6:F907:300B:0F0C:04A4:69E6:A79C:A2AD:F757:9722:"
		"844C:C1B3:40E9:4CA8:CEBE:907F");
	crypt::pubkey pk1 (pk1str);
	assert (pk1.fingerprint() == pk1str);
	std::ostringstream ostr;
	assert (0 == (ostr.flags() & std::ios::right));
	assert (0 == (ostr.flags() & std::ios::left));
	assert (0 == (ostr.flags() & std::ios::unitbuf));
	ostr << std::oct << std::scientific << std::showbase << std::showpoint
		<< std::showpos << std::nouppercase << std::left << std::boolalpha
		<< std::unitbuf;
	ostr.precision (11);
	ostr.fill ('%');
	ostr.width (9);
	const auto old_flags = ostr.flags();
	assert (0 != (ostr.flags() & std::ios::left));
	assert (0 != (ostr.flags() & std::ios::unitbuf));
	ostr << pk1;
	std::cout << "PK1: " << ostr.str() << '\n';
	assert (ostr.str() == pk1str);
	assert (79 == ostr.str().length());
	assert (ostr.flags() == old_flags);
	assert ('%' == ostr.fill());
	assert (0 != (ostr.flags() & std::ios::showbase));
	assert (0 == (ostr.flags() & std::ios::uppercase));
	assert (0 != (ostr.flags() & std::ios::oct));
	assert (11 == ostr.precision());
	/////////////////
	pk1.clear();
	std::cout << "Cleared: " << pk1 << '\n';
	assert (pk1.fingerprint() == "0000:0000:0000:0000:0000:0000:0000:0000:0000:0000:"
		"0000:0000:0000:0000:0000:0000");
	std::cout << std::endl;
}

int main()
{
	return trace::catch_all_errors(run);
}
