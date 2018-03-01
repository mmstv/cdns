#include "network/udp/upstream.hxx"
#include "network/tcp/upstream.hxx"
#include "network/net_error.hxx"
#include "sys/logger.hxx"

namespace network {

namespace log = process::log;

void upstream::pass_failure_downstream()
{
	this->close();
	// question here could be folded/encrypted
	//! @todo what if expecting encrypted answer?
	if( this->question().size() > 0u && this->question().is_dns() )
	{
		this->question_mod().mark_servfail();
		assert (this->provider_ptr_);
		this->provider_ptr_->increment_failures();
		log::debug ("passing failure down: ", this->question().size(),
			", failures: ", this->provider_ptr()->failures());
		this->pass_answer_downstream();
		//! @todo: try another povider
	}
	else
		log::error ("not passing failure  down: ", this->question().size());
}

void upstream::start(int sock)
{
	this->event_.start(sock, ev::READ);
}

void upstream::receive_callback (ev::io &w, int
#ifndef NDEBUG
	revs
#endif
)
{
	assert( 0 != (ev::READ & revs) );
	assert( 0 == (ev::WRITE & revs) );
	assert( nullptr != w.data );
	reinterpret_cast<upstream *>( w.data )->on_receive();
}

void upstream::send_callback (ev::io &w, int
#ifndef NDEBUG
	revents
#endif
)
{
	assert( 0 == (revents & ev::READ) );
	assert( 0 != (revents&ev::WRITE) );
	auto *self = reinterpret_cast<upstream *>( w.data );
	assert (nullptr != self);
	self->on_send();
}

void upstream::close()
{
	event_.stop();
	event_send_.stop();
	this->timeout::stop();
}

upstream::upstream (std::shared_ptr <provider> &&p, std::unique_ptr <packet> &&pkt,
	double tsec) : timeout (tsec), question_ptr_ (std::move (pkt)),
	provider_ptr_(std::move(p))
{
	assert( provider_ptr_ );
	assert( question_ptr_ );
	assert (0 < this->question().size());
	event_.set(ev::get_default_loop());
	event_.set<receive_callback>(); // ATTN! clears event_.data
	event_.data = this; // ATTN! must be set after callback

	event_send_.set(ev::get_default_loop());
	event_send_.set<send_callback> (this); // also sets event_.data = 0
	assert (this == event_send_.data);
	event_send_.data = this;
}

void upstream::on_send()
{
	const auto &conn = dynamic_cast <abs_connection &> (*this);
	const auto *tcp_up = dynamic_cast <tcp::upstream *> (this);
	const bool is_tcp = (nullptr != tcp_up);
	const char *const proto = is_tcp ? "TCP" : "UDP";
	try
	{
		const bool connected = (!is_tcp) || tcp_up->is_connected();
		if (connected)
		{

			if( this->question().size() > 0U )
			{
				try
				{
					log::debug ("sending[", proto, "]: ", this->question().size(),
						" to: ", conn.address().ip_port());
					this->provider_ptr()->fold(this->question_mod());
					conn.send (this->question());
					// ATTN! shutdown(SHUT_WR) only works on local 127.0.0.1
					// addresses. It causes subsequent ::recv return 0 (connection
					// reset by peer) on the external addresses
					//! @todo keep question for failed responses?
					this->question_mod().set_size(0);
				}
				catch(net_error &e)
				{
					log::warning ("Network failure: ", e.what());
					this->pass_failure_downstream();
				}

				if( this->question().size() <= 0U ) // done sending
				{
					this->start (conn.file_descriptor()); // start receiving
					this->event_send_.stop(); // stop sending
				}
			}
			else
				this->event_send_.stop();
		}
	}
	catch (net_error &e)
	{
		log::error ("SEND[", proto, "] to: ", conn.address().ip_port(),
			", not connected: " , e.what());
		this->pass_failure_downstream();
	}
#ifndef NDEBUG
	if( this->event_send_.loop.iteration() > 1000 )
	{
		this->event_send_.stop();
		log::alert ("DEVELOPMENT INTERACTIONS LIMIT");
		this->event_send_.loop.break_loop (ev::ALL);
	}
#endif
}


namespace udp {

//! @todo this socket can be global
out::out (std::shared_ptr <provider> &&p, std::unique_ptr <packet> &&q, double tsec)
	: network::upstream (std::move(p), std::move (q), tsec),
	udp_connection (this->provider_ptr()->address())
{
	this->unblock();
	event_send_.start (this->file_descriptor(), ev::WRITE);
}

} // network::udp

namespace tcp {

void upstream::close()
{
	this->network::upstream::close();
	this->tcp_connection::close();
}

upstream::upstream (std::shared_ptr<provider> &&p, std::unique_ptr <packet> &&q,
	double tsec) : network::upstream (std::move(p), std::move (q), tsec),
	tcp_connection (this->provider_ptr()->address())
{
	this->unblock();
	event_send_.start(this->file_descriptor(), ev::WRITE);
	this->tcp_connect();
}

} // namespace network::tcp

void upstream::on_receive()
{
	const auto &conn = dynamic_cast <abs_connection &> (*this);
	const auto *tcp_up = dynamic_cast <tcp::upstream *> (this);
	const char *const proto = (nullptr != tcp_up) ? "TCP" : "UDP";
	try
	{
		if ((nullptr == tcp_up) || tcp_up->is_connected())
		{
			try
			{
				this->question_mod().set_size(0); // clearing question
				conn.receive (this->question_mod());
				log::debug ("response[", proto, "]: ", this->question().size(),
					" from ", conn.address().ip_port());
				this->close();
				this->unfold();
				this->pass_answer_downstream();
			}
			catch(net_error &e)
			{
				log::error ("Failed[", proto, "]: ", e.what());
				this->pass_failure_downstream();
			}
		}
		else
		{
			//! @todo max times to try
			log::debug ("upstream ", proto, " not connected on recv: ",
				conn.to_string());
		}
	}
	catch(net_error &e)
	{
		log::error ("RECV upstream[", proto, "] not connected: ", e.what());
		this->pass_failure_downstream();
	}
}

} // namespace network
