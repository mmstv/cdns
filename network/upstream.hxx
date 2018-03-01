#ifndef NETWORK_UPSTREAM_HXX
#define NETWORK_UPSTREAM_HXX

#include <cassert>

#include <network/packet.hxx>
#include <network/timeout.hxx>
#include <network/provider.hxx>
#include <network/dll.hxx>

namespace network {

class NETWORK_API upstream : public timeout
{
public:

	// Call to virtual function during destruction will not dispatch to derived class
	virtual ~upstream() {this->upstream::close();}

	const packet &message() const {assert (question_ptr_); return *question_ptr_;}

	const std::unique_ptr <packet> &message_ptr() const
	{
		assert (question_ptr_);
		return question_ptr_;
	}

	//! @todo: obsolete
	const packet &question() const {assert( question_ptr_); return *question_ptr_;}

	virtual void pass_answer_downstream() {}

	virtual void close();

	void unfold() { this->provider_ptr_->unfold(this->message_mod()); }

protected:

	upstream (std::shared_ptr <provider> &&p, std::unique_ptr <packet> &&pkt,
		double tsec);

	void start(int sock);

	void pass_failure_downstream();

	packet &message_mod() {assert(question_ptr_); return *question_ptr_;}

	packet &question_mod() {assert(question_ptr_); return *question_ptr_;}

	const std::shared_ptr<provider> & provider_ptr() const {return provider_ptr_;}

	ev::io event_send_;

private:

	static void receive_callback (ev::io &, int );

	virtual void on_receive();

	ev::io event_;

	std::unique_ptr <packet> question_ptr_;

	std::shared_ptr<provider> provider_ptr_;

	virtual void on_send();

	static void send_callback(ev::io &, int);
};

} // namespace network
#endif
