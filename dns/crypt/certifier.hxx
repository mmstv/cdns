#ifndef __DNS_CRYPT_CERTIFIER_HXX__
#define __DNS_CRYPT_CERTIFIER_HXX__ 1

#include <memory>

#include <ev++.h>

#include <network/fwd.hxx>
#include <dns/crypt/resolver.hxx>
#include <dns/crypt/dll.hxx>

namespace dns {

namespace crypt {

//! 'certifier' does not exist in the English language according to `hunspell`
class DNS_CRYPT_API certifier : public std::enable_shared_from_this<certifier>
{
public:

	//! @todo unused constexpr static const unsigned  test_cert_margin = -1;

	certifier(resolver &&, network::proto, double tout);

	const resolver &provider() const {return *resolver_;}

	const std::shared_ptr<resolver> provider_ptr() const {return resolver_;}

	std::string start();

	void stop();

	void process_response(query &q);

private:
	// template<bool T> friend class request;

	network::proto tcp_only() const {return tcp_only_;}

	static void retry_timer_callback(ev::timer &t, int);

	void update();

	resolver &modify_provider() {return *resolver_;}

	unsigned query_retry_step() const {return query_retry_step_;}

	void inc_query_retry_step() {++query_retry_step_;}

	void reset_query_retry_step() {query_retry_step_=0;}

	void reschedule_query(double query_retry_delay);

	void reschedule_query_after_success();

	void reschedule_query_after_failure();

	ev::timer timer_;

	unsigned int             query_retry_step_;
	std::shared_ptr<resolver>     resolver_;
	network::proto tcp_only_;

	std::unique_ptr<network::upstream> upstream_;
	double timeout_seconds_ = 10.5;
	std::size_t attempts_ = 0;
};

}} // namespace dns::crypt

#endif
