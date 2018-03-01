#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cstring>
#include <cassert>
#include <ctime>

#include "backtrace/catch.hxx"
#include "dns/cache.hxx"
#include "dns/query.hxx"
#include "dns/constants.hxx"

using namespace std;
constexpr const char nl = '\n';
void tst_0()
{
	cout << "\n==== Testing cache" << endl;
	const char hn0[] = "dummy.nohost.NoName";
	const dns::query msg (hn0);
	dns::cache cache (60); // 60 sec ttl
	assert (60 == cache.min_ttl());
	assert (msg.is_dns());
	assert (12 + 1 + 19 + 1 + 2*2 == msg.size());
	assert (5u == msg.bytes()[12]);
	dns::query smsg (hn0);
	std::vector <std::uint8_t> dummy (10UL, 'a');
	smsg.set_txt_answer (hn0, dummy, 999999);
	{
		smsg.print (cout);
		const std::uint8_t *const ub = smsg.bytes();
		cout << "\nub2: " << std::hex << unsigned (ub[2])
			<< " :: " << unsigned (ub[2]&2)
			<< ", ub3: " << unsigned (ub[3]) << " :: " << unsigned (ub[3]&0xf)
			<< '\n';
		assert ( ((ub[2] & 2) == 0) && ((ub[3] & 0xf) == 0 || (ub[3] & 0xf) == 3) );
	}

	cache.store (smsg);
	assert (1ul == cache.size());
	const std::time_t cnow = std::time (nullptr);
	const uint16_t id = msg.header().id;
	cout << "now: " << cnow << ", msg id: " << id << ", " << smsg.header().id << nl;
	assert( cnow > 10000 );
	assert( cnow > 1499188064 );
	assert (id != smsg.header().id);

	const dns::cache_entry *ent = cache.find (hn0, dns::rr_type::a);
	assert (nullptr == ent);
	ent = cache.find ("dummy.nohost.NoName.", dns::rr_type::a);
	assert (nullptr != ent);
	auto q = msg.get_question();
	string hn = get<string>(q);
	cout << "hn: " << hn << '\n';
	ent = cache.find (hn, get<dns::rr_type>(q));
	assert (nullptr != ent);
	assert (ent->deadline > cnow);
	cout << "deadline: " << ent->deadline << ", dif: " << (ent->deadline - cnow)
		<< '\n';
	assert (ent->deadline == cnow + 86400); // 24 hrs default MAX_TTL
	auto r_msg = msg;
	cout << "Retrieving, id: " << r_msg.header().id << '\n';
	assert (id == r_msg.header().id);
	bool gr = cache.retrieve (r_msg);
	cout << "Retrieved: " << boolalpha << gr << ", id: " << r_msg.header().id << nl;
	assert (gr);
	assert (r_msg.is_dns());
	assert (id == r_msg.header().id);
	assert (86400 == r_msg.answer_min_ttl());
	assert (ent->deadline > std::time(nullptr));
	const auto rq = r_msg.get_question();
	const auto rq_hn = get <string> (rq);
	cout << "Retrieved HN: " << rq_hn << '\n';
	assert (dns::rr_type::a == get <dns::rr_type> (rq));
	/////
	const char hn1[] = "dummy-1.host2.com";
	const dns::query msg1 (hn1, dns::rr_type::a);
	assert (msg1.is_dns());
	assert (12u + 1u + 17u + 1u + 2*2u == msg1.size());
	assert (7u == msg1.bytes()[12]);
	smsg = msg1;
	smsg.add_answer (dummy, dns::rr_type::any, 1234);
	assert (smsg.is_dns());
	assert (smsg.size() == msg1.size() + 2U + sizeof(dns::rr_header) + dummy.size());

	{
		smsg.print (cout);
		const std::uint8_t *const ub = smsg.bytes();
		cout << "\nub2: " << std::hex << unsigned (ub[2])
			<< " :: " << unsigned (ub[2]&2)
			<< ", ub3: " << unsigned (ub[3]) << " :: " << unsigned (ub[3]&0xf)
			<< '\n';
		assert ( ((ub[2] & 2) == 0) && ((ub[3] & 0xf) == 0 || (ub[3] & 0xf) == 3) );
	}

	assert (1u == cache.size());
	cache.store (smsg);
	assert (2u == cache.size());
	r_msg = msg1;
	gr = cache.retrieve (r_msg);
	assert (gr);
	assert (r_msg.is_dns());
	assert (msg1.header().id == r_msg.header().id);
	assert (1234 == r_msg.answer_min_ttl());
	const auto rq1 = r_msg.get_question();
	const auto rq1_hn = get <string> (rq1);
	cout << "Retrieved HN: " << rq1_hn << '\n';
	assert (dns::rr_type::a == get <dns::rr_type> (rq1));
	/////
	const char fn[] = "tst-tmp-dns-cache.bin";
	cout << "Saving cache into: " << fn << '\n';
	cache.save_as (fn);
	cout << "Loading cache from: " << fn << '\n';
	dns::cache loaded = dns::cache::load (fn, 0);
	assert (loaded.size() == cache.size());
}

void run()
{
	tst_0();
}

int main()
{
	return trace::catch_all_errors (run);
}
