#ifndef NETWORK_TIMEOUT_HXX
#define NETWORK_TIMEOUT_HXX

#include <ev++.h>
#include <network/dll.hxx>

namespace network {

// dllexport is needed here for VPTR.
// It is not enough to mark just the virtual functions dllexport.
// This issue is only caught with -fsanitize.
class NETWORK_API timeout
{
public:

	explicit timeout (double seconds);

	virtual ~timeout() = default; // because of virtual functions

	virtual void close() = 0; // {}

	void stop() {timeout_.stop(); assert(!timeout_.is_active());}

	bool is_active() const {return timeout_.is_active();}

	void *event_loop_handle() const noexcept {return timeout_.loop.raw_loop;}

private:

	ev::timer timeout_;

	static void callback (ev::timer &, int);
};

} // namespace network

#endif
