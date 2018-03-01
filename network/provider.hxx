#ifndef NETWORK_PROVIDER_HXX
#define NETWORK_PROVIDER_HXX

#include <vector>
#include <memory>

#include <network/address.hxx>
#include <network/dll.hxx>
#include <network/fwd.hxx>

namespace network {

class NETWORK_API provider
{
public:

	provider() noexcept = default;

	provider (const class address &a, proto _tcp) noexcept : address_(a), tcp_(_tcp)
		{}

	provider (class address &&a, proto t) noexcept : address_ (std::move (a)),
		tcp_(t) {}

	virtual ~provider() noexcept = default; // because of virtual functions

	const class address &address() const noexcept {return address_;}

	proto net_proto() const noexcept {return tcp_;}

	void set_tcp(proto t) noexcept {tcp_ = t;}

	bool is_ready() const noexcept {return true;}

	virtual void fold(packet &) {}

	virtual void unfold(packet &) {}

	virtual std::unique_ptr <packet> adapt_message (const std::unique_ptr <packet> &)
		const;

	void increment_failures() noexcept {++failures_;}

	std::size_t failures() const noexcept {return failures_;}

private:

	class address address_;
	proto tcp_;
	std::size_t failures_ = 0;
};

} // namespace network

#endif
