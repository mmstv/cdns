#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <iostream>
#include <cassert>

#include <sodium/crypto_box.h>

#include "dns/crypt/resolver.hxx"
#include "dns/crypt/certificate.hxx"
#include "backtrace/catch.hxx"

constexpr const char key[]= "50F6:F917:30EB:FF8F:24A4:69E6:A79C:A2AD:F757:9722:844C"
	":C1C1:80E9:4C38:CEBE:E07C";
constexpr const char zkey[]= "0000:0000:0000:0000:0000:0000:0000:0000:0000:0000"
	":0000:0000:0000:0000:0000:0000";


static void resolver_test(dns::crypt::resolver &resolver)
{
	std::cout << "resolver test\n";
	assert( resolver.hostname() == std::string("2.dnscrypt-cert") );
	assert( resolver.id() == std::string("2.dnscrypt-cert") );
	assert( resolver.address().ip_port() == std::string("127.0.0.1:53") );
	assert( !resolver.is_ready() );
	const std::string pk = resolver.public_key().fingerprint();
	std::cout << "finger: " << pk << std::endl;
	assert( pk != zkey );
	assert( key == pk );
	const std::string lk = resolver.resolver_public_key().fingerprint();
	std::cout << "local : " << lk << std::endl;
	assert( zkey == lk );
	const std::string ek = resolver.encryptor().public_key().fingerprint();
	std::cout << "encryp: " << ek << std::endl;
	assert( zkey == ek );
}

static int run(int , char *[])
{
	dns::crypt::resolver resolver (std::string ("2.dnscrypt-cert"),
		std::string ("2.dnscrypt-cert"), crypt::pubkey (std::string (key)),
		network::address ("127.0.0.1", 53), network::proto::udp);
	resolver_test(resolver);
#if 0
	@todo rewrite with libev
	struct event_base *event_loop = event_base_new();
	assert( nullptr!=event_loop );
	struct event *timer = evtimer_new(event_loop, cbfn, event_loop);
	timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	evtimer_add(timer, &tv);
	const bool tcp_only = true;
	dns::crypt::CertUpdater cert(event_loop, std::move(resolver), tcp_only);
	cert.start();
	int dr = event_base_dispatch(event_loop);
	std::cout << "Event loop ended: " << dr << '\n';
	cert.stop();
	event_free(timer);
	event_base_free(event_loop);
	return dr;
#else
	return 0;
#endif
}

int main(int argc, char *argv[])
{
	return trace::catch_all_errors(run, argc, argv);
}
