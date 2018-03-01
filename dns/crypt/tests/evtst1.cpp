#include <iostream>

#include <ev++.h>

#include "dns/responder.hxx"
#include "network/address.hxx"
#include "network/provider.hxx"
#include "network/udp/listener.hxx"
#include "network/tcp/listener.hxx"
#include "backtrace/catch.hxx"

namespace dns { namespace tests {

void tcb (ev::timer &w, int )
{
	std::cout << "\nTimeout!" << std::endl;
	w.loop.break_loop(ev::ONE);
}

void interrupt(ev::sig &s, int)
{
	s.loop.break_loop(ev::ALL);
}

void tstev_main(int argc, char *argv[])
{
	int timeout = (argc > 1) ? std::atoi( argv[1] ) : 12;
	if( timeout <= 0 || timeout > 3600 )
		timeout = 12;
	auto port = static_cast <std::uint16_t> ((argc>2) ? std::atoi( argv[2] ) : 1053);
	int pport = (argc>3) ? std::atoi( argv[3] ) : 2053;
	std::cout << "TIMEOUT: " << timeout << " seconds"
		<< " PORT: " << port << "  PROVIDER PORT: " << pport
		<< std::endl;
	auto dl = ev::get_default_loop();

	network::address addr("127.0.0.1", port);
	network::address paddr("127.0.0.1", static_cast <std::uint16_t>
		(pport>0?pport:53));
	auto prov = std::make_shared<network::provider>(paddr, network::proto::udp);
	auto rptr = std::make_shared<dns::responder>();
	auto provo = prov; // keep a copy
	provo->set_tcp(network::proto::tcp);
	prov->set_tcp(network::proto::udp);
	if( pport > 0 )
	{
		rptr->add_provider(std::move(provo));
		rptr->add_provider(std::move(prov));
	}
	assert (rptr);
	std::cout << "Making UDP listener" << std::endl;
	auto udp_listener = network::udp::listener::make_new (std::shared_ptr
		<dns::responder> (rptr), addr);
	assert( rptr );
	std::cout << "Making TCP listener" << std::endl;
	auto  tcp_listener = network::tcp::listener::make_new(std::move(rptr), addr);
	//! @todo: assert( !rptr );
	// simple non-repeating 'timeout' seconds timeout
	ev::timer timeout_watcher;
	timeout_watcher.set<tcb>();
	timeout_watcher.set(dl);
	ev::tstamp after = timeout;
	timeout_watcher.priority = 0;
	timeout_watcher.set(after);
	std::cout << "STARTING timeout watcher" << std::endl;
	timeout_watcher.start();
	ev::sig s;
	s.set(dl);
	s.priority = 0;
	s.set<interrupt>();
	s.start(SIGINT);
	std::cout << "STARTING main event loop" << std::endl;
	dl.run();
	timeout_watcher.stop();
	std::cout << "\nFinished, request count: " << udp_listener->count()
		<< ", tcp: " << tcp_listener->count()
		<< "; queued: " << udp_listener->q_count()
		<<", tcp: " << tcp_listener->q_count()
		<< ", iterations: " << dl.iteration()
		<< std::endl;
	std::atexit (ev_default_destroy);
}

}}

int main (int argc, char *argv[])
{
	return trace::catch_all_errors(dns::tests::tstev_main, argc, argv);
}
