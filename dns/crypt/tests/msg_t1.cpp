#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cstring>
#include <cassert>

#include "backtrace/catch.hxx"
#include "dns/query.hxx"

void copy_message(dns::query &q)
{
	static_assert( dns::query::max_size < 65535U, "size" );
	static_assert( dns::query::max_size > 10000U, "size" );
	std::uint8_t buf[] = "abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra"
		" abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra abrakadabra";
	static_assert( sizeof(buf) > 120U && sizeof(buf) < 220U, "size" );
	const dns::query orig_q(buf, sizeof(buf));
	std::cout << "original message size: " << orig_q.size() << std::endl;
	q = orig_q;
	std::cout << "copied message size: " << q.size() << std::endl;
	assert( 132U == q.size() );
}

void tst_copy0()
{
	std::cout << "==== Testing Copy" << std::endl;
	const std::uint8_t buf0[] = "buf0 buf0 buf0 buf0 buf0 buf0";
	dns::query q(buf0, sizeof(buf0));
	std::cout << "Message size: " << q.size() << std::endl;
	assert( q.size() > 20U && q.size() < 100U );
	assert( 0 == std::memcmp (q.bytes(), buf0, q.size()) );
	copy_message(q);
}

void run()
{
	tst_copy0();
}

int main()
{
	return trace::catch_all_errors(run);
}
