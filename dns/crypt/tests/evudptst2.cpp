#undef NDEBUG
#include <cassert>

#include <iostream>
#include <typeinfo>

#include "network/udp/upstream.hxx"
#include "network/tcp/upstream.hxx"
#include "sys/logger.hxx"

#include "backtrace/catch.hxx"

namespace dns { namespace tests {

static std::size_t tcount = 0;
void timeoutcb (ev::timer &t, int)
{
	++tcount;
	std::cerr << "Timeout, loop: " << t.loop.raw_loop << std::endl;
	t.stop();
}

void tst_udp_main(int argc, char *argv[])
{
	sys::logger_init (static_cast <int> (process::log::severity::debug), false);
	auto backs = ev::supported_backends();
	std::cout << "Supported BACKENDS: " << backs << std::endl;
	assert (0 != backs);
	bool tcp = false;
	unsigned short port = 2053;
	std::string question("2.dnscrypt-cert"), ip("127.0.0.1");
	for(int i=1; i<argc; ++i)
	{
		const std::string s(argv[i]);
		if( "-t" == s )
			tcp = true;
		else
		{
			int p = std::atoi(s.c_str());
			if( p>0 && p <= 65535 )
				port = static_cast<unsigned short>(p);
			else if( !s.empty() && '@' == s.at(0) )
				ip = s.substr(1U);
			else
				question = s;
		}
	}
	std::cout << "Q: " << question << "; TCP: " << tcp
		<< "; IP: " << ip << "; PORT: " << port << std::endl;
	network::address addrr (ip, port);
	auto make_up = [](const network::address &addr, const std::string &q, bool tt)
	{
		auto prov = std::make_shared<network::provider>(addr, network::proto::udp);
		assert (prov);
		std::cout << "Provider: " << prov.get() << ", ready: " << prov->is_ready()
			<< std::endl;
		auto pkt = std::make_unique <network::packet> (reinterpret_cast
			<const std::uint8_t*> (q.data()), q.length());
		assert (pkt);
		std::cout << "Packet: " << pkt.get() << ", size: " << pkt->size()
			<< std::endl;
		std::unique_ptr<network::upstream> req;
		constexpr double timeout = 5.5; // seconds
		if( tt )
		{
			auto t = std::make_unique <network::tcp::upstream> (std::move(prov),
				std::move (pkt), timeout);
			assert( t );
			req = std::move(t);
		}
		else
		{
			auto u = std::make_unique <network::udp::out> (std::move (prov),
				std::move (pkt), timeout);
			assert (u);
			std::cout << "UDP upstream socket, fd: " << u->file_descriptor()
				<< ", handle: " << u->sys_socket().to_string()
				<< ", connected: " << u->sys_socket().is_connected()
				<< ", active: " << u->is_active()
				<< std::endl;
			req = std::move(u);
		}
		assert( req );
		return req;
	};
	std::cout << "Address: " << addrr << std::endl;
	auto req = make_up (addrr, question, tcp);
	std::cout << "Request ptr: " << req.get() << std::endl;
	assert (req);
	assert (req->timeout::is_active());
	std::cout << "Upstream loop: " << req->event_loop_handle() << std::endl;
	ev::timer timeout;
	timeout.set <timeoutcb> ();
	timeout.priority = 0;
	timeout.start (2.);

	auto dl = ev::get_default_loop();
	std::cout << "Pending count: " << ev_pending_count (dl) << std::endl;
	unsigned be = dl.backend();
	std::cout << "Backend: " << be << ", POLL: " <<
		(0!=(be&ev::POLL)) << ", EPOLL: " << (0!=(be&ev::EPOLL))
		<< ", SELECT: " << (0 != (be & ev::SELECT))
		<< ", Main Loop: " << dl.raw_loop
		<< std::endl;
	// ATTN libev must be DLL, to avoid multiple default loops
	assert (dl.raw_loop == req->event_loop_handle());
	dl.run();
	const auto &msg = req->message();
	std::cout << "\nAfter sending msg size: " << msg.size()
		<< ", pending: " << ev_pending_count (dl)
		<< std::endl;
	if( msg.size() > 0U )
	{
		std::cout << "Msg type: " << typeid (msg).name();
		assert (typeid (msg) == typeid (network::packet));
		std::cout << std::endl;
	}

	std::cout << "Iterations: " << dl.iteration()
		<< ", depth: " << dl.depth()
		<< ", is default: " << dl.is_default()
		<< ", tcount: " << tcount
		<< std::endl;
	assert (dl.is_default());
	assert (0 == dl.depth());
	assert (6 > dl.iteration());
	assert (0 < dl.iteration());
	assert (1 == tcount);
	assert (0 == msg.size());
	std::atexit (ev_default_destroy);
}

}}

int main (int argc, char *argv[])
{
	return trace::catch_all_errors(dns::tests::tst_udp_main, argc, argv);
}
