#undef NDEBUG

#include <iostream>
#include <cassert>
#include <array>

#include <sodium.h>

#include "backtrace/catch.hxx"
#include "../srcz/pad_buffer.hxx"

using namespace std;

void run()
{
	uint8_t buf[1000];
	for(unsigned i=0; i<1000; ++i)
		buf[i] = 0x11;
	unsigned n = dns::crypt::detail::pad_buffer (buf, 100, 1000,
		randombytes_uniform);
	cout << "n: " << n << endl;
	assert (n <= 1000u);
	assert (0x80 == buf[100]);
	assert (0 == (n % 64u) || (1000u == n));
	for(unsigned i=101; i<n; ++i)
		assert (0 == buf[i]);
	for(unsigned i=n; i<1000; ++i)
		assert (0x11 == buf[i]);
	for(unsigned i=0; i<100; ++i)
		assert (0x11 == buf[i]);
}

int main()
{
	return trace::catch_all_errors(run);
}
