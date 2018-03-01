#ifndef NETWORK_TCP_INCOMING_HXX
#define NETWORK_TCP_INCOMING_HXX

#include <network/incoming.hxx>
#include <network/connection.hxx>
#include <network/dll.hxx>

namespace network {

namespace tcp {

class NETWORK_API in : public network::incoming, public network::tcp_connection
{
public:

	explicit in (std::weak_ptr<class tcplistener> &&);

	// Call to virtual function during destruction will not dispatch to derived class
	~in() override {this->in::close();}

	void respond(const packet &) override;

	void close() override {receive_event_.stop(); this->incoming::close();}

private:

	void on_receive();

	std::shared_ptr<class tcplistener> listener_ptr() const;

	static void receive_callback(ev::io &w, int revents);

	ev::io receive_event_;
};

}} // namespace network::tcp
#endif
