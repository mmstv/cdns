#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cassert>
#include <sstream>

#include "backtrace/catch.hxx"
#include "dns/responder.hxx"
#include "network/provider.hxx"

const char valid_resolv_t1[] =
"\n"
"\n"
"\t\t\n"
"domain home\n"
"search \t\tbla\n"
"nameserver 127.0.0.3:1\n"
"nameserver 127.0.0.7   # comment    \n"
"8.8.8.8 8.8.4.4       \n"
"100.100.100.100 2.2.2.2 3.3.3.3:7 4.4.4.4\n"
"options";

const char valid_resolv_t2[] =
"\n"
"##### ###### bla bla bla yada yada yada\n"
"\t#\t\n"
"domain home # ha\n"
"search \t\tbla oo ee yy qq tt oo\n"
"nameserver 126.0.0.3    \t   \n"
"nameserver 126.0.0.7:533   # comment, 1.2.3.4 5.6.7.8  more comments   \n"
"8.8.8.8:65535 8.8.4.4   7.7.7.7    \n"
"#100.100.100.100 2.2.2.2 3.3.3.3:99 4.4.4.4\n"
"options opt1 opt2 opt3 opt4\n"
"1::1 2::2 3::3 [4::5.6.7.8]:8080 # fancy name\n";

const char invalid_resolv_t3[] =
"\n"
"##### ## bla\n"
"nameserver 1::fefe # ok \n"
"  255.255.255.255\t # ok\n"
"nameserver 1::fge1 # not ok\n";

const char invalid_resolv_t4[] =
"1.1.1.1:53 [2::3]:443 6.6.7.8 # Ok! \n"
"    #\n"
"1.9.4.1:53 [1::7]:453 nameservers\n";

using namespace std::literals::string_literals;

static int run()
{
	std::istringstream istr (valid_resolv_t1);
	auto r = dns::responder::from_file (istr, false, network::proto::udp);
	for (const auto &a : r)
	{
		std::cout << "ADDR: " << a.address().ip_port() << '\n';
	}
	assert (8u == r.size());

	std::istringstream istr2 (valid_resolv_t2);
	r = dns::responder::from_file (istr2, false, network::proto::tcp);
	std::cout << '\n';
	for (const auto &a : r)
	{
		std::cout << "ADDR: " << a.address().ip_port() << '\n';
	}
	assert (9u == r.size());

	std::istringstream istr3 (invalid_resolv_t3);
	bool failed = false;
	try
	{
		r = dns::responder::from_file (istr3, false, network::proto::udp);
	}
	catch (std::runtime_error &e)
	{
		failed = true;
		std::cout << "\nErr: " << e.what() << '\n';
		assert ("invalid address: 1::fge1"s == e.what());
	}
	assert (failed);

	std::istringstream istr4 (invalid_resolv_t4);
	failed = false;
	try
	{
		r = dns::responder::from_file (istr4, false, network::proto::udp);
	}
	catch (std::runtime_error &e)
	{
		failed = true;
		std::cout << "\nErr: " << e.what() << '\n';
		assert ("invalid address: nameservers"s == e.what());
	}
	assert (failed);

	std::cout << std::endl;
	return 0;
}

int main()
{
	return trace::catch_all_errors (run);
}
