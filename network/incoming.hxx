#ifndef NETWORK_INCOMING_HXX
#define NETWORK_INCOMING_HXX

#include <memory>
#include <cassert>

#include <network/timeout.hxx>
#include <network/packet.hxx>


namespace network {

class abs_listener;

class incoming : public timeout
{
public:

	virtual void respond(const packet &) = 0;

	virtual void close() { this->timeout::stop(); assert( this->is_closed() ); }

	// Call to virtual function during destruction will not dispatch to derived class
	virtual ~incoming() {this->incoming::close();}

	const packet &message() const {assert(message_ptr_); return *message_ptr_;}

	const std::unique_ptr<packet> &message_ptr() const {return message_ptr_;}

	packet &modify_message() {assert(message_ptr_); return *message_ptr_;}

	void replace_message(std::unique_ptr<packet> &&q)
	{
		assert(q);
		message_ptr_ = std::move(q);
	}

	bool is_closed() { return !this->timeout::is_active(); }

	std::shared_ptr<class abs_listener> listener_ptr() const
	{
		assert( !listener_ptr_.expired() );
		return std::shared_ptr<class abs_listener>(listener_ptr_);
	}

protected:

	explicit incoming (std::weak_ptr<class abs_listener> &&);

	std::unique_ptr<packet> message_ptr_;

	std::weak_ptr<class abs_listener> listener_ptr_;
};

} // namespace network
#endif
