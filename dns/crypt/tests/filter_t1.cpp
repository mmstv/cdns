#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <sstream>
#include <fstream>

#include "backtrace/catch.hxx"
#include "dns/filter.hxx"

using namespace std;
constexpr const char nl = '\n';

void tst_0()
{
	cout << "\n==== Testing filter" << nl;
	stringstream str;
	str << "*.blacklisted.domain";
	dns::filter filter (move(str));
	cout << "count: " << filter.count() << nl;
	assert( 1u == filter.count() );

	assert (!filter.match ("domain"));
	assert (!filter.match (".domain"));
	assert (!filter.match ("domain."));
	assert (!filter.match (".domain."));
	assert (!filter.match ("lacklisted.domain"));
	assert (!filter.match ("bblacklisted.domain"));

	assert (filter.match ("haha.blacklisted.domain"));
	assert (filter.match ("haha.Blacklisted.domain"));
	assert (filter.match (".blacklisteD.domain."));
	assert (filter.match (".blackListed.domain"));
	assert (filter.match ("blackLiSted.domain."));
	assert (!filter.match ("haha.notblacklisted.domain"));
	assert (!filter.match ("blacklisted.domainn"));
}

void tst_1()
{
	cout << "\n==== Testing filter" << nl;
	stringstream str;
	str << "*.blacklisted.domain\n";
	str << "*.other.black.dom";
	dns::filter filter (move(str));
	cout << "count: " << filter.count() << nl;
	assert( 2u == filter.count() );
	assert (filter.match ("other.black.dom"));
	assert (filter.match (".other.black.dom"));
	assert (filter.match (".other.black.dom."));
	assert (filter.match ("z.other.black.dom."));
	assert (filter.match (".y.other.black.dOm"));
	assert (!filter.match ("black.dom"));
	assert (!filter.match ("ther.black.dom"));
	assert (!filter.match (".dom"));
	assert (!filter.match ("dom"));
	assert (!filter.match ("domain"));
	assert (!filter.match ("black.match"));
	assert (!filter.match ("blacklisted.dom"));
	assert (filter.match ("haha.bLacKlisted.domain"));
	assert (filter.match (".blacklisteD.domain."));
	assert (filter.match (".blackListed.dOmain"));
	assert (filter.match ("blackLiSted.domAin."));
	assert (!filter.match ("haha.notblacklisted.domain"));
	assert (!filter.match ("blacklisted.domainn"));
}

void tst_2()
{
	cout << "\n==== Testing filter" << nl;
	stringstream str;
	str << "*.some.black-listed.doma *.other.listed.doma .xxx.listed.doma"
		" .yyy.listed.doma .listed.doma .zzz.listed.doma"
	;
	dns::filter filter (move(str));
	cout << "count: " << filter.count() << nl;
	assert (2u == filter.count());
	cout << "Filter:\n" << filter << nl;
	assert (!filter.match ("other.black.dom"));
	assert (filter.match ("her.listed.doma"));
	assert (filter.match ("hey.some.black-listed.doma"));
	assert (filter.match ("some.black-listed.doma"));
	assert (filter.match ("listed.doma"));
	assert (!filter.match ("llisted.doma"));
	assert (!filter.match ("isted.doma"));
}

void tst_3()
{
	cout << "\n=== Prefix filter test\n";
	stringstream str;
	str << "level1.level2.level3.* level1.level2.other.* level1.level2.three.*"
		" level1.level2.four.* level1.level2.* level1.level2.five.*"
		" level1.level.level3.*";
	dns::filter filter (move (str));
	cout << "count: " << filter.count() << nl;
	assert (2 == filter.count());
	cout << "Filter:\n" << filter << nl;
	assert (filter.match ("level1.level2.blabla"));
	assert (filter.match ("level1.level.level3.blablabla"));
	assert (filter.match ("level1.level2"));
	assert (filter.match ("level1.level2.level3.some"));
	assert (!filter.match ("level1.level.level"));
	assert (!filter.match ("level1.level"));
	assert (!filter.match ("level1.level2x"));
}

dns::filter tst_4()
{
	cout << "\n=== Combined filter test\n";
	stringstream str;
	str <<
		"prefix1.prefix2.prefix3.* prefix1.prefix2.other.* prefix1.prefix2.three.*"
		" prefix1.prefix2.* prefix1.prefix2.fi5ve.*"
		" prefix1.prefix.prefix3.* prefix.xYsubstr1gA.xxx.*"
		" *.a.suffix2.suffix1 .b.suffix2.suffix1 .suffix2.suffix1 .prefix1.prefix2 "
		"exact2.exact1 exact3.exact2.exact1 exact.suffix2.suffix1 a.XnotexactY.exact"
		" suffix1 prefix1 prefix1.prefix prefix1.prefix2 prefix1.prefix2.some"
		" suffix2.suffix1"
		" *substr1* *substr2* *notexact* *notexac* *notexacto*"
		;
	dns::filter filter (move (str));
	cout << "count: " << filter.count() << nl;
	cout << "Filter:\n" << filter << nl;
	assert (12 == filter.count());
	assert (filter.match ("z.Xsubstr1x.site"));
	assert (filter.match ("cc.a-notexacdament.site"));
	assert (filter.match ("prefix1.prefix2.site"));
	assert (filter.match ("prefix1.prefix2"));
	assert (filter.match ("host.suffix2.suffix1"));
	assert (filter.match ("exact3.exact2.exact1"));
	assert (filter.match ("prefix1.prefix.prefix3"));
	assert (!filter.match ("prefix1.prefix.suffix1"));
	assert (!filter.match ("prefix1.prefix2a.bsuffix2.suffix1"));
	assert (filter.match ("prefix1.prefix2.suffix2.suffix1"));
	assert (filter.match ("exact2.exact1"));
	assert (!filter.match ("exact.exact2.exact1"));
	assert (!filter.match ("exact2.exact1.suffix1"));
	assert (filter.match ("suffix1"));
	assert (!filter.match ("uffix2.suffix1"));
	assert (filter.match ("prefix1"));
	assert (filter.match ("prefix1.prefix"));
	assert (!filter.match ("prefix1.suffix1"));
	assert (filter.match ("site.prefix1.prefix2"));
	return filter;
}

void tst_5 (dns::filter &&otherfilter)
{
	cout << "\n=== Merge filter test\n";
	stringstream str;
	str <<
		"prefix1.prefix2.prefix3.* prefix1.prefix2.other.* prefix1.prefix2.three.*"
		" prefix1.prefix.* prefix.xYsubstr1gA.xxx.*"
		" *.a.suffix2.suffix1 .b.suffix2.suffix1 .prefix2 "
		"exact2.exact1 exact3.exact2.exact1 exact.suffix2.suffix1 a.XnotexactY.exact"
		" suffix1 prefix1 prefix1.prefix prefix1.prefix2 prefix1.prefix2.some"
		" suffix2.suffix1"
		" *substr1x* *substr2y* *notexa*"
		;

	dns::filter filter (move (str));
	cout << "count: " << filter.count() << nl;
	assert (18 == filter.count());
	filter.merge (move (otherfilter));
	cout << "Merged count: " << filter.count() << nl;
	cout << "Merged Filter:\n" << filter << nl;
	assert (11 == filter.count());
	assert (filter.match ("zzzzz.prefix2"));
	assert (filter.match ("yyy.suffix2.suffix1"));
	assert (filter.match ("prefix1.prefix.ggggggggg"));
	assert (filter.match ("prefix1.prefix2.aaaaaa"));
	assert (filter.match ("bla.blaNotexAnono.com"));
}

void tst_n()
{
	const char *const dirn = getenv ("DNS_TEST_DIR");
	assert (nullptr != dirn);
	{
		const string wfn = string(dirn) + "/whitelist.txt";
		dns::filter white(wfn);
		cout << "white size: " << white.count() << nl;
		assert (706u == white.count());
		assert (white.match ("twitter.com"));
		assert (white.match ("www.twitter.com"));
		assert (white.match ("1.1.1.1.in-addr.arpa"));
		assert (!white.match ("1.1.1.1in-addr.arpa"));
		assert (!white.match ("twitter.co"));
		assert (!white.match ("google.com"));
		assert (!white.match ("www.google.com"));
		assert (!white.match ("www.google.com"));
		assert (white.match ("android.googlesource.com"));
		assert (white.match ("www.android.googlesource.com"));
		assert (!white.match ("droid.googlesource.com"));
		assert (!white.match ("googlesource.com"));
		assert (white.match ("reqrypt.org"));
		assert (!white.match ("ww.reqrypt.org"));
		assert (!white.match ("wwww.reqrypt.org"));
		assert (white.match ("www.reqrypt.org"));
	}

	{
		const string bfn = string(dirn) + "/blacklist.txt";
		dns::filter black(bfn);
		cout << "black size: " << black.count() << nl;
		assert (19000u < black.count() && 23000u >= black.count());
		assert (21622u == black.count());
		assert (!black.match ("twitter.com"));
		assert (!black.match ("www.twitter.com"));
		assert (black.match ("l.googlE.coM"));
		assert (black.match ("aPis.goOgle.com"));
		assert (black.match ("host.apis.google.com"));
		assert (!black.match ("pis.google.com"));
		assert (!black.match ("google.com"));
		assert (black.match ("down.hit020.com"));
		assert (black.match ("dOwn.HiT020.cOm"));

		assert (!black.match (".down.hit020.com"));
		assert (black.match ("down.hit020.com"));

		assert (!black.match ("w.down.hit020.com"));
		assert ( black.match ("www.google----xx.com"));
		assert (!black.match ("www.google---xx.com"));
		assert (black.match ("ad.some.site.com"));
		assert (black.match ("no.such.uTelemetry-site.com"));
		assert (!black.match ("sucks.telemetr---.net"));
		assert (black.match ("mbn.cOm.ua"));
		assert (black.match ("120.mBN.com.ua"));
		assert (!black.match ("a120.mbn.com.ua"));
		assert (black.match ("classic.mbn.com.ua"));
		assert (!black.match ("lassic.mbn.com.ua"));
		assert (!black.match ("cclassic.mbn.com.ua"));
		assert (black.match ("bla-bla-bla.stats.esomniture.com"));
		assert (black.match ("stats.esomniture.com"));
		assert (black.match ("stats.whatever"));
	}
}

void run()
{
	tst_0();
	tst_1();
	tst_2();
	tst_3();
	tst_5 (tst_4());
	tst_n();
}

int main()
{
	return trace::catch_all_errors (run);
}
