#ifndef NETWORK_RESPONDER_HXX_
#define NETWORK_RESPONDER_HXX_

#include <memory>

#include <network/fwd.hxx>
#include <network/dll.hxx>

namespace network {

// dllexport is needed here for VPTR
// It is not enough to mark just the virtual functions dllexport.
// This issue is only caught with -fsanitize
class NETWORK_API responder
{
public:
	//! @todo rename to receive
	virtual void process(std::shared_ptr<incoming> &&) = 0;
	virtual void respond (std::shared_ptr<incoming> &) = 0;
	double timeout_seconds() const {return timeout_seconds_;}

	virtual std::unique_ptr <packet> new_packet() const = 0;

protected:

	explicit responder (double t) : timeout_seconds_(t) {}

	virtual ~responder() = default; // because of virtual functions

private:
	double timeout_seconds_ = 7.5;
};

} // namespace network
#endif
