#ifndef NETWORK_LISTENER_HXX_
#define NETWORK_LISTENER_HXX_

#include <vector>
#include <memory>
#include <chrono>

#include <ev++.h>

#include <network/fwd.hxx>
#include <network/dll.hxx>

namespace network {

class NETWORK_API abs_listener // : public std::enable_shared_from_this<abs_listener>
{
public:

	//! Total incoming requests count
	std::size_t count() const {return count_;}

	//! Queued incoming requests count
	std::size_t q_count() const {return requests_.size();}

	//! @todo: use smart pointer
	void create_response(incoming *); // used by incoming

	void start(int sock) {event_.start(sock, ev::READ);}

	std::shared_ptr<network::responder> responder_ptr() const
	{
		assert( !this->responder_ptr_.expired() );
		return std::shared_ptr<network::responder>(this->responder_ptr_);
	}

protected:

	explicit abs_listener (std::weak_ptr<network::responder> &&);

	virtual ~abs_listener() = default; // because of virtual functions

	std::vector< std::shared_ptr<incoming> > requests_;

private:

	virtual void make_incoming() = 0;

	static void callback (ev::io &, int);

	void on_message();

	std::size_t count_;
	ev::io event_;

	std::weak_ptr<network::responder> responder_ptr_;

	std::chrono::high_resolution_clock::time_point t0_;
};

} // namespace network
#endif
