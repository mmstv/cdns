#ifndef DNS_DAEMON_HXX__
#define DNS_DAEMON_HXX__ 1

#include <vector>
#include <deque>
#include <memory>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cassert>

#include <ev++.h>

#include <network/address.hxx>
#include <network/fwd.hxx>

#include <dns/options.hxx>
#include <dns/dll.hxx>

namespace dns {

class responder;

class DNS_API daemon
{
public:

	explicit daemon(std::shared_ptr<detail::options> &&ops);

	daemon() : daemon(std::make_shared<detail::options>()) {}

	virtual ~daemon();

	virtual void reload();

	bool set_options(int argc, char *argv[]);

	void set_options(std::shared_ptr<detail::options> &&opts);

	void execute();

	const detail::options &options() const {return *options_ptr_;}

	void report_stats() const;

protected:

	void setup(std::shared_ptr<responder> &&);

	void setup0();

	std::shared_ptr<responder> responder_ptr() const {return responder_ptr_;}

	const std::shared_ptr<detail::options> options_ptr() const {return options_ptr_;}

	network::udp::udplistener &modify_udp_listener() {return *udp_listener_;}

	network::tcp::tcplistener &modify_tcp_listener() {return *tcp_listener_;}

	const network::udp::udplistener &udp_listener() const {return *udp_listener_;}

	const network::tcp::tcplistener &tcp_listener() const {return *tcp_listener_;}

private:

	const network::address &address() const {return local_sockaddr_;}

	void set_address(const network::address &addr) { local_sockaddr_ = addr; }

	void init_descriptors_from_systemd();

	///// options
	network::proto tcp_only() const {return this->options().net_proto; }

	unsigned int connections_count_max() const
	{
		return this->options().connections_count_max;
	}

	void start_listeners();

private:

	std::shared_ptr<detail::options> options_ptr_;

	ev::sig interrupt_, terminate_, reload_;
	ev::timer testing_timeout_;

	network::address local_sockaddr_;

	std::shared_ptr<responder> responder_ptr_;

	bool listeners_started = false;

	std::shared_ptr<network::udp::udplistener> udp_listener_;
	std::shared_ptr<network::tcp::tcplistener> tcp_listener_;
	int udp_listener_systemd_handle_, tcp_listener_systemd_handle_;
};

} // namespace dns

#endif
