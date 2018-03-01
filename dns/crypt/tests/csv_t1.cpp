#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cassert>
#include <cstdlib>

#include "dns/crypt/resolver.hxx"

static int run(int argc, char *argv[])
{
	const char *fn = nullptr;
	if( argc>1 )
		fn = argv[1];
	else
		fn = std::getenv("DNS_CRYPT_TEST_FILE");
	assert (nullptr != fn);
	auto resolvers = dns::crypt::parse_resolvers_list(fn, network::proto::udp);
	std::cout << "Resolver count: " << resolvers.size() << std::endl;
	for(const auto &a_resolver : resolvers)
		std::cout << a_resolver.ip_port() << ' ' << a_resolver.id()
			<< ' ' << a_resolver.hostname() << '\n';
	assert( 117U==resolvers.size() );
	//! @todo temporary file name
	const char sfn[] = "/tmp/tst-tmp-dns-rslv.csv";
	dns::crypt::save_resolvers (resolvers, sfn);
	auto resolvers2 = dns::crypt::parse_resolvers_list (sfn, network::proto::udp);
	assert( 117U==resolvers2.size() );
	return 0;
}

int main(int argc, char *argv[])
{
	try { return run(argc, argv); }
	catch(std::exception &e)
	{
		std::cerr << "\nERROR! " << e.what() << std::endl;
	}
	return 3;
}
