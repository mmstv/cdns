#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <vector>
#include <cassert>
#include <ctime>

#include "backtrace/catch.hxx"
#include "dns/crypt/certificate.hxx"

#include "data/test_signed_certificate.h"
#include "data/test_public_key.h"

using namespace std;

constexpr const unsigned sz = sizeof(test_signed_certificate);
static_assert( 124U == sz, "size" );
static_assert( 32U == sizeof(test_public_key), "size");

constexpr static const char ekey[] =
	"2F01:B9D9:518D:4A1B:872B:DE6C:FF20:F47B:CEA4:2FE6:4EB6:BB05:B067:8EEB:D468:FC2A"
	;

constexpr static const char skey[] =
	"BFA7:C101:B941:0A88:339D:42BA:38E1:0646:5B33:7384:9D6C:02D9:9FA6:E755:6A62:CB50"
	;


static void run()
{
	crypt::pubkey signing_public_key(test_public_key);
	assert( skey == signing_public_key.fingerprint() );
	dns::crypt::certificate cert(signing_public_key, test_signed_certificate, sz);
	cert.print_info();
	assert( ekey == cert.encrypting_public_key().fingerprint());
	assert (cert.cipher() == dns::crypt::cipher::xsalsa20poly1305);
	assert( 1496175122u == cert.serial() );
	const auto ts = cert.ts();

	const time_t ts_begin = static_cast<time_t> (get<0>(ts));
	assert(ts_begin > 0);

	const time_t ts_end = static_cast<time_t>( get<1>(ts) );
	assert(ts_end > 0);

	cout << "ts_begin: " << ts_begin << ",  ts_end: " << ts_end << endl;
	assert( 1496175122 == ts_begin);
	assert( 1527711122 == ts_end );
	/*
		certificate is valid from [2017-05-30] to [2018-05-30]
	*/

	const array <uint8_t, 8> expected_magic{{47, 1, 185, 217, 81, 141, 74, 27}};
	assert( cert.magic_message() == expected_magic );
	cout << "certificate version: " << cert.version() << endl;
	assert( 65536u == cert.version() );
}

int main()
{
	return trace::catch_all_errors(run);
}
