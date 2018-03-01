#include <cassert>
#include <stdexcept>

#include "network/timeout.hxx"
#include "sys/logger.hxx"

namespace network {

void timeout::callback (ev::timer &w, int)
{
	process::log::debug ("timeout, at: ", w.at, ", repeat: ", w.repeat,
		", remaining: ", w.remaining());
	reinterpret_cast<timeout *>( w.data )->close();
	w.stop();
	assert( !w.is_active() );
}

timeout::timeout(double secs)
{
	if( 1.E-6 > secs )
		throw std::logic_error ("Too low timeout: " + std::to_string(secs)
			+ " seconds");
	// simple non-repeating timeout
	timeout_.set(ev::get_default_loop());
	timeout_.set<callback>();
	timeout_.data = this; // ATTN! must be set after callback
	ev::tstamp after = secs; // seconds
	timeout_.set(after);
	timeout_.start();
	assert( timeout_.is_active() );
}

}
