#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cstring>
#include <cassert>
#include <tuple>

#include "backtrace/catch.hxx"
#include "dns/query.hxx"
#include "dns/constants.hxx"

#include "dns_test.hxx"

using namespace std;

inline void print(const std::string &str, std::ostream &text_stream)
{
	for(char ch : str)
	{
		if( std::isprint(ch) )
			text_stream << ch;
		else
			text_stream << "[0x" << std::hex << static_cast<unsigned>(ch)
				<< std::dec << ']';
	}
}

void tst_0()
{
	cout << "\n==== Testing DNS query" << endl;
	const char hn0[] = "dummy.host.name";
	const dns::query msg(hn0);
	cout << "Message size: " << msg.size() << endl;
	assert( msg.is_dns() );
	assert( msg.size() > 20U && msg.size() < 100U );
	assert (12 + 1 + 15 + 1 + 2*2 == msg.size());
	const auto q = msg.get_question();
	const string hn =  get<string>(q);
	cout << "Q. hostname size: " << hn.size() << endl;
	cout << "Q. hostname: ";
	print (hn, cout);
	cout << '\n';
	cout
		<< ", class: " << static_cast <unsigned> (get <dns::rr_class> (q))
		<< ", type: " << static_cast <unsigned> (get<dns::rr_type>(q))
		<< '\n' << endl;
	const std::string hnm =  msg.hostname();
	cout << "hostname: " << hnm << endl;
	assert( 'd' == hn.at(0) );
	assert( sizeof(hn0) == hn.size() );
	assert( dns::rr_class::internet == get<dns::rr_class>(q) );
	assert( dns::rr_type::a == get<dns::rr_type>(q) );
	assert( sizeof(hn0) == 1 + hnm.size() );
	assert( '.' != hnm.back() );
	const auto min_ttl =  msg.answer_min_ttl();
	cout << "min ttl: " << min_ttl << endl;
	assert( 0 == min_ttl ); // not a response
	assert (!msg.header().qr);
	assert (!msg.header().tc);
	assert (!msg.header().aa);
	assert (!msg.header().ra);
	assert (!msg.header().rd);
	assert (!has_flags_qr (msg));
	assert (!has_flags_tc (msg));
	assert (!has_flags_ra (msg));
}

void tst_1()
{
	cout << "\n==== Testing DNS answer" << endl;
	const char hn0[] = "asking.ip.of.dummy.host.name.";
	dns::query msg(hn0);
	msg.set_answer("reply.host.name", "0.0.0.1");
	cout << "answered query size: " << msg.size() << endl;
	const auto min_ttl = msg.answer_min_ttl();
	cout << "min ttl: " << min_ttl << endl;
	assert( 10 == min_ttl );
	assert( msg.is_dns() );
	const auto q = msg.get_question();
	const auto hn = get <string> (q);
	print (hn, cout);
	cout << '\n';
	assert (dns::rr_class::internet == get <dns::rr_class> (q));
	assert( dns::rr_type::a == get<dns::rr_type> (q) );
	assert (!hn.empty() && 'a' == hn.front());
}

void run()
{
	tst_0();
	tst_1();
}

int main()
{
	return trace::catch_all_errors(run);
}
