#include <sstream>
#include <algorithm>
#include <chrono>

#include "network/address.hxx"
#include "network/udp/incoming.hxx"
#include "network/tcp/incoming.hxx"
#include "network/udp/listener.hxx"
#include "network/tcp/listener.hxx"
#include "network/responder.hxx"
#include "sys/logger.hxx"

#include <ev++.h>

namespace network {

namespace log = process::log;

abs_listener::abs_listener(std::weak_ptr<network::responder> &&r) : count_(0UL),
	responder_ptr_(std::move(r)), t0_(std::chrono::high_resolution_clock::now())
{
	assert( ! responder_ptr_.expired() );
	event_.set(ev::get_default_loop());
	event_.set<callback>();
	event_.data = this;
}

void abs_listener::on_message()
{
	auto t = std::chrono::high_resolution_clock::now();
	auto td = std::chrono::duration_cast<std::chrono::nanoseconds>(t-t0_).count();
	++count_;
	//! @todo: implement throttling, and parameterize
	if( td > 0 || (0 == count_ % 1000) )
	{
		long double rate = 1e9;
		rate *= count_;
		rate /= td;
		if( rate > 32. )
			log::notice ("Message rate: ", rate, "/sec, count: ", count_);
	}
	//! @todo: code duplication, see dns::responder::collect_garbage
	if( !requests_.empty() )
	{
		// clean up requests
		std::size_t last = 0;
		for(std::size_t i=0; i<requests_.size(); ++i)
		{
			auto &qreq = requests_[i];
			if( qreq && qreq->is_closed() )
			{
				qreq.reset();
				requests_.erase(requests_.begin() + static_cast <::ssize_t> (i));
			}
			else
				last = i + 1U;
		}
		requests_.resize(last);
	}
	if (requests_.size() > 8U)
	{
		log::debug ("active requests: ", this->requests_.size(), ", capacity: ",
			requests_.capacity(), ", total: ", count_);
	}
	this->make_incoming();
}

//! @todo replace raw pointer with smart one
//! @todo pointers are not reliable for comparision, use some sort of id?
void abs_listener::create_response(incoming *inreq)
{
	assert( nullptr != inreq );
	auto itr = std::find_if(requests_.begin(), requests_.end(),
		[inreq] (const auto &p) { return &*p == inreq; });
	if( requests_.end() == itr )
		throw std::logic_error("Creating response for non-queued incoming request");
	std::shared_ptr<incoming> req = *itr;
	assert( ! responder_ptr_.expired() );
	this->responder_ptr_.lock()->process(std::move(req));
}

void abs_listener::callback (ev::io &w, int )
{
	auto *self = reinterpret_cast<abs_listener *>( w.data );
	self->on_message();
}

namespace udp {

void udplistener::make_incoming()
{
	// shared_from_this() throws if 'this' is not shared_ptr
	std::weak_ptr<udplistener> self(this->shared_from_this());
	assert( !self.expired() );
	//! @todo: catch exception here ?
	std::shared_ptr <incoming> req (new udp::in (std::move(self)));
	if( !req->is_closed() )
	{
		this->requests_.emplace_back( std::move(req) );
		// UDP, so asking for response from upstream provider
		this->create_response(&*requests_.back());
	}
}

// see daemon::udp_listener_bind
udplistener::udplistener(std::weak_ptr<network::responder> &&r,
	const class address &a) : abs_listener(std::move(r)), udp_connection (a)
{
	this->init (true); // do bind
}

// Use created and bound systemd socket
udplistener::udplistener (std::weak_ptr<network::responder> &&r,
	socket_t &&systemd_bound_socket) : abs_listener (std::move (r)), udp_connection
	(std::move (systemd_bound_socket))
{
	this->init (false); // do not bind
}


void udplistener::init (bool do_bind)
{
	this->reuse_listener_port();
	if (do_bind)
		this->bind_listener();
	this->unblock(); //! @todo before or after bind?
	this->start (this->file_descriptor());
	log::notice ("UDP listener on ", this->to_string());
}

} // namespace network::udp


namespace tcp {

void tcplistener::make_incoming()
{
	// shared_from_this() throws if 'this' is not shared_ptr
	std::weak_ptr<tcplistener> self(this->shared_from_this());
	assert( !self.expired() );
	auto req = std::make_shared<tcp::in> (std::move (self));
	if( !req->is_closed() )
		this->requests_.emplace_back( std::move(req) );
}

tcplistener::tcplistener (std::weak_ptr<network::responder> &&r,
	const class address &a) : abs_listener (std::move(r)), tcp_connection (a)
{
	this->init (true); // do bind
}

// for socket from systemd
tcplistener::tcplistener (std::weak_ptr<network::responder> &&r,
	socket_t &&systemd_bound) : abs_listener (std::move(r)), tcp_connection
								(std::move (systemd_bound))
{
	this->init (false); // do not bind
}

void tcplistener::init (bool do_bind)
{
	this->reuse_listener_port();
	{
		;
		// const int pq[] = { TCP_FASTOPEN_QUEUES };
		// ::setsockopt(this->socket(), IPPROTO_TCP, TCP_FASTOPEN, pq, sizeof (int));
	}
	if (do_bind)
		this->bind_listener();
	this->tcp_listen();
	this->unblock(); //! @todo is it necessary?
	this->start(this->file_descriptor());
	log::notice ("TCP listener started on ", this->to_string());
}

} // namespace network::tcp

} // namespace network
