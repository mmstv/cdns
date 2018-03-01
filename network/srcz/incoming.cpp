#include <typeinfo>

#include "network/net_error.hxx"
#include "network/packet.hxx"
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

/* PROPOSED CLASS HIERARCHY

udp::ref_sock
-> udp::in

own_sock
-> tcp::in
-> tcp::upstream
-> udp::upstream
-> listener

incoming : listener
-> udp::in
-> tcp::in

udp::in : udp_listener
<-- incoming
<-- udp::ref_sock

tcp::in : tcp_listener
<-- incoming
<-- tcp::own_sock
	<-- own_sock

tcp::up
<-- upstream
<-- tcp::own_sock
	<-- own_sock

udp::up
<-- upstream
<-- udp::own_sock
	<-- own_sock

udp::listener
<-- listener
<-- udp::own_sock
	<-- own_sock

own_sock
-> udp::own_sock
-> tcp::own_sock

*/

incoming::incoming (std::weak_ptr<class abs_listener> &&l)
	: timeout (l.lock()->responder_ptr()->timeout_seconds()),
	listener_ptr_(std::move(l))
{
	assert( !listener_ptr_.expired() );
	const auto r = this->listener_ptr_.lock()->responder_ptr();
	assert (r);
	this->message_ptr_ = r->new_packet();
}

namespace udp {

in::in (std::weak_ptr<class udplistener> &&_listener)
	: network::incoming (std::move(_listener))
{
	auto lstn = this->listener_ptr();
	//! @todo: where are exceptions caught?
	lstn->udp_receive_from (this->modify_message(), this->address_);
	log::debug ("packet[UDP]: ", this->message().size(), " from: ",
		address_.ip_port());
}

void in::respond(const packet &message)
{
	try
	{
		this->listener_ptr()->udp_send_to(message, this->address());
		log::debug ("response[UDP]: ", message.size(), " to client: ",
			this->address().ip_port());
		this->close();
	}
	catch(net_error &e)
	{
		log::alert ("Failed to respond[UDP]: ", e.what());
		this->close(); //! @todo: should we try again?
	}
}

std::shared_ptr<class udplistener> in::listener_ptr() const
{
	assert( !listener_ptr_.expired() );
	return std::dynamic_pointer_cast< class udplistener >( listener_ptr_.lock() );
}

} // namespace network::udp

namespace tcp {

void in::respond(const packet &message)
{
	try
	{
		this->tcp_send(message);
		log::debug ("response[TCP]: ", message.size(), " to client: ",
			this->address().ip_port());
		this->close(); // done responding
	}
	catch(net_error &e)
	{
		log::alert ("failed responding[TCP]: ", e.what());
		this->close();
	}
}

in::in (std::weak_ptr<class tcplistener> &&_listener)
	: network::incoming (std::move (_listener)),
	network::tcp_connection (*this->listener_ptr())
{
	this->unblock();
	receive_event_.set (ev::get_default_loop());
	receive_event_.set<receive_callback>(); // ATTN! clears event_.data
	receive_event_.data = this; // ATTN! must be set after callback
	receive_event_.start(this->file_descriptor(), ev::READ );
}

void in::receive_callback(ev::io &w, int
#ifndef NDEBUG
	revents
#endif
)
{
	assert( 0 == (ev::WRITE & revents ) );
	assert( 0 != (ev::READ & revents) );
	in *self = reinterpret_cast<in *>(w.data);
	assert (nullptr != self);
	self->on_receive();
}

void in::on_receive()
{
	try
	{
		this->tcp_receive(this->modify_message());
		log::debug ("packet[TCP]: ", this->message().size(), " from: ",
			this->address().ip_port());
		this->receive_event_.stop();
		this->listener_ptr()->create_response(this);
		// not closing this connection, waiting for response
	}
	catch(net_error &e)
	{
		log::alert ("Failed to receive incoming request[TCP]: ", e.what());
		//! @todo: should we try to receive again, depending on the error type?
		this->close();
	}
#ifndef NDEBUG
	if( this->receive_event_.loop.iteration() > 1000 )
		throw std::logic_error("DEVELOPMENT LIMIT: too many loop iterations");
#endif
}

std::shared_ptr<class tcplistener> in::listener_ptr() const
{
	assert( !listener_ptr_.expired() );
	return std::dynamic_pointer_cast< class tcplistener >( listener_ptr_.lock() );
}

} // namespace tcp

} // namespace network
