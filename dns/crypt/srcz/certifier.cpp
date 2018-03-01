#include <stdexcept>

#include <sodium.h>

#include "dns/query.hxx"
#include "network/tcp/upstream.hxx"
#include "network/udp/upstream.hxx"
#include "dns/crypt/certifier.hxx"
#include "dns/crypt/certificate.hxx"
#include "sys/logger.hxx"

namespace dns { namespace crypt {

namespace retry
{
constexpr const unsigned short min_delay = 17,
	max_delay = 5 * 60,
	steps = 100,
	delay_after_success_min_delay = 60 * 60,
	delay_after_success_jitter = 100;
}

namespace log = process::log;

void certifier::reschedule_query(double query_retry_delay)
{
	if (timer_.is_pending())
		return;
	ev::tstamp after = query_retry_delay; // seconds
	timer_.set(after, after);
}

void certifier::reschedule_query_after_failure()
{
	if (timer_.is_pending())
		return;
	double query_retry_delay = (retry::min_delay +
		this->query_retry_step() *
		(retry::max_delay - retry::min_delay) /
		retry::steps);
	if (this->query_retry_step() < retry::steps)
		this->inc_query_retry_step();
	log::debug ("after failure retry: ", query_retry_delay);
	this->reschedule_query(query_retry_delay);
}

void certifier::reschedule_query_after_success()
{
	if (timer_.is_pending())
		return;
	this->reschedule_query (retry::delay_after_success_min_delay
		+ randombytes_uniform (retry::delay_after_success_jitter));
}

void certifier::process_response(query &q)
{
	log::debug ("Certificate: ", q.host_type(), ", size: ", q.size());
	std::vector<std::vector<std::uint8_t> > txt_data;
	q.get_txt_answer(txt_data);
	auto &provider = this->modify_provider();
	if( provider.find_valid_certificate( txt_data ) )
	{   // success
		this->reset_query_retry_step();
		this->reschedule_query_after_success();
	}
	else
		this->reschedule_query_after_failure();
	upstream_.reset();
}

template<network::proto TCP>
class request : public std::conditional <static_cast <bool> (TCP),
	network::tcp::upstream, network::udp::out>::type
{
	typedef typename std::conditional<static_cast<bool>(TCP), network::tcp::upstream,
		network::udp::out>::type base_t;

	std::weak_ptr<certifier> certifier_;

	void pass_answer_downstream() override
	{
		assert( ! certifier_.expired() );
		assert (typeid (this->message_mod()) == typeid (query));
		certifier_.lock()->process_response (dynamic_cast <query&>
			(this->message_mod()));
	}

public:

	// pass dns::provider copy of dns::crypt::resolver
	// because certifier should not encrypt and decrypt DNS queries
	request(std::weak_ptr<certifier> &&c, std::unique_ptr <query> &&q, double tsec)
		: base_t (std::make_shared<network::provider>(*c.lock()->provider_ptr()),
			std::move (q), tsec),
		certifier_ (std::move(c)) {}
};

void certifier::update()
{
	try
	{
		if( upstream_ && upstream_->is_active() )
		{
			log::debug ("Certificate request is still alive");
			return;
		}
		if (4 < this->attempts_)
		{
			this->upstream_.reset();
			this->stop();
			log::debug ("Too many certificate requests for: ",
				this->provider().ip_port());
			return;
		}
		constexpr const bool txt = true;
		auto q = std::make_unique <query> (this->provider().hostname().c_str(), txt);
		assert (q);
		log::debug ("Certificate request, size: ", q->size(), ", to: ",
			this->provider().ip_port(), ", for: ", this->provider().hostname());
		this->upstream_.reset(); // because of timeout, upstream could be alive
		std::shared_ptr<certifier> nptr = this->shared_from_this();
		this->upstream_ = (network::proto::tcp == this->tcp_only()) ?
			static_cast<std::unique_ptr<network::upstream> >(
				std::make_unique< request<network::proto::tcp> >(nptr, std::move (q),
					timeout_seconds_))
			: static_cast<std::unique_ptr<network::upstream> >(
				std::make_unique< request<network::proto::udp> >(nptr, std::move (q),
					timeout_seconds_));
		assert( this->upstream_ );
		++attempts_;
		return;
	}
	catch(std::runtime_error &e)
	{
		log::error ("CERTIFICATE ", e.what());
	}
	catch(std::exception &e)
	{
		log::alert ("CERTIFICATE FATAL: ", e.what());
		throw;
	}
	this->upstream_.reset();
	this->reschedule_query_after_failure();

}

void certifier::retry_timer_callback(ev::timer &t, int)
{
	auto* const cert = reinterpret_cast<certifier* >(t.data);
	log::debug ("CB retry timer  at: ", t.at, ",  repeat: ", t.repeat, ", remain: ",
		t.remaining());
	try {
		cert->update();
	}
	catch(std::exception &e)
	{
		log::critical (e.what());
	}
}

certifier::certifier (resolver &&r, network::proto tcp, double tsec)
	: query_retry_step_ (0U), resolver_ (std::make_shared <resolver> (std::move(r))),
	tcp_only_ (tcp), timeout_seconds_ (tsec)
{
	auto dl = ev::get_default_loop();
	timer_.set(dl);
	timer_.set<certifier::retry_timer_callback>();
	ev::tstamp after = retry::min_delay;
	timer_.set(after, after);
	timer_.data = this;
	timer_.start();
}

std::string certifier::start()
{
	try {
		this->update();
	}
	catch(std::runtime_error &e)
	{
		return e.what();
	}
	return "";
}

void certifier::stop()
{
	timer_.stop();
}

class invalid_certificate : public std::runtime_error
{
public:
	invalid_certificate() : std::runtime_error("invalid certificate") {}
	invalid_certificate(const char *msg) : std::runtime_error(msg) {}
};

bool resolver::find_valid_certificate (std::vector <std::vector <std::uint8_t>> &trv)
{
	this->set_ready(false);
	assert (trv.size() < 100U);
	std::unique_ptr <certificate> cert;
	log::debug ("Certificate count: ", trv.size());
	for (unsigned i=0; i < trv.size(); ++i)
	{
		try
		{
			auto ci = std::make_unique <certificate> (this->public_key(), trv[i]);
			if ((cert && ci->is_preferred_over (*cert)) || (!cert))
				cert = std::move (ci);
		}
		catch (invalid_certificate &err) // ignore, try next certificate
		{
			log::error ("Invalid certificate: ", err.what());
		}
	}
	if (!cert)
	{
		log::error ("No useable certificates found among: ", trv.size());
		// cert_reschedule_query_after_failure(&cert);
		return false;
	}
	dns::crypt::cipher the_cipher = cert->cipher();
	if (cipher::undefined == the_cipher)
	{
		// cert_reschedule_query_after_failure(&cert);
		return false;
	}
	this->set_resolver_public_key(cert->server_public_key());
	//! @todo seems to be unused
	this->set_magic_query (cert->magic_message());
	cert->print_info();
	cert->check_key_rotation_period();
	this->modify_encryptor().set_magic_query(
		cert->magic_message(), the_cipher);
	cert.reset();
	if (this->modify_encryptor().set_resolver_publickey (this->resolver_public_key())
		!= 0)
	{
		log::error ("Suspicious public key");
		throw std::runtime_error("Suspicious public key");
	}
	log::debug ("DNScrypt: ", this->ip_port(), " ready, public key: ",
		this->fingerprint());
	this->set_ready(true);

	// cert.reset_query_retry_step();
	// cert_reschedule_query_after_success(&cert);
	return true;
}

}} // namespace dns::crypt


