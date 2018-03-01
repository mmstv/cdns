#include <fstream>
#include <algorithm>

#include "network/incoming.hxx"
#include "network/upstream.hxx"
#include "dns/responder_parameters.hxx"
#include "dns/cache.hxx"
#include "dns/constants.hxx"
#include "dns/crypt/cresponder.hxx"
#include "dns/crypt/certifier.hxx"
#include "sys/logger.hxx"
#include "dns/crypt/server_crypt.hxx"
#include "dns/crypt/server_responder.hxx"
#include "backtrace/backtrace.hxx"

namespace dns { namespace crypt {

namespace log = process::log;

void random_callback(ev::timer &t, int)
{
	assert (nullptr != t.data);
	const auto &cr = *reinterpret_cast <const cresponder *> (t.data);
	log::notice ("RND! processed: ", cr.processed_count(), ", cached: ",
		cr.cached_count(), ", queued: ", cr.q_count(), ", blacklisted: ",
		cr.blacklisted_count());
	if (!cr.cache_dir().empty())
	{
		const std::string fn1 = cr.cache_dir() + "/ready_dnscrypt_providers.csv";
		cr.save_providers (fn1);
		const std::string fn2 =  cr.cache_dir() + "/dnscache.bin";
		cr.cache().save_as (fn2);
		log::notice ("Saved providers: ", fn1, ", cache: ", fn2);
	}
	// cr.select_random_provider();
}

cresponder::cresponder (const dns::responder::parameters &params,
	const std::string &dnscrypt_resolvers_file,
	std::unique_ptr<server::responder> srv)
	: dns::responder (params), server_ptr_ (std::move(srv))
{
	this->load (dnscrypt_resolvers_file, params.noipv6, params.net_proto,
		params.timeout);
	this->random_timer_.set <random_callback>();
	this->random_timer_.set (ev::get_default_loop());
	this->random_timer_.data = this;
	constexpr const unsigned update = 7U; // minutes
	this->random_timer_.repeat = update*60.; // seconds
	this->random_timer_.at = 0;
	//! @todo ATTN! ev_timer is not properly initialized in <ev++.h>
	this->random_timer_.pending = 0;
	this->random_timer_.priority = 0;
	log::info ("DNScrypt random update: ", update, " sec");
	this->random_timer_.start();
}

void cresponder::load(const std::string &dnscrypt_resolvers_file, bool noipv6,
	network::proto tcponly, double timeout)
{
	try
	{
		//! @todo use std::string exe_path = trace::this_executable_path();
		//! to find resolvers file
		try
		{
			log::info ("DNScrypt resolvers from: ", dnscrypt_resolvers_file);
			auto resolvers = parse_resolvers_list (dnscrypt_resolvers_file.c_str(),
				tcponly);
			std::size_t used = 0;
			for(auto &resolver : resolvers)
			{
				const auto &addr = resolver.address();
				const std::string ipp = addr.ip_port();
				const auto it = this->dnscrypt_providers_.find (ipp);
				const bool use = !(noipv6 && network::inet::ipv6 == addr.inet());
				const bool found = (this->dnscrypt_providers_.end() != it);
				used += use;
				if (use && !found)
					this->dnscrypt_providers_.emplace (ipp,
						std::make_shared<certifier> (std::move (resolver),
							tcponly, timeout));
				if (!use && found)
					this->dnscrypt_providers_.erase (it);
			}
			for (auto pp = this->dnscrypt_providers_.begin();
				pp != this->dnscrypt_providers_.end();)
			{
				auto next = pp;
				++next;
				//! @todo inefficient, use std::set ?
				if (resolvers.cend() == std::find_if (resolvers.cbegin(),
						resolvers.cend(), [pp] (const resolver &p)
						{
							return p.address().ip_port() == pp->first;
						}))
					this->dnscrypt_providers_.erase (pp);
				pp = next;
			}
			assert (this->dnscrypt_providers_.size() <= resolvers.size());
			if (this->dnscrypt_providers_.size() != used)
				throw std::logic_error ("DNScrypt resolvers merging failed");
		}
		catch (std::runtime_error &e) //! @todo: catch only file errors?
		{
			log::error (e.what());
		}
		log::info ("DNScrypt resolvers: ", this->dnscrypt_providers_.size());
	}
	catch(std::exception &e)
	{
		log::error (e.what());
		throw;
	}
	catch(...)
	{
		log::error ("failed to start certificate updater");
		throw;
	}
}

void cresponder::reload(const dns::responder::parameters &p,
	const std::string &dnscrypt_resolvers_file)
{
	this->dns::responder::reload (p);
	this->load (dnscrypt_resolvers_file, p.noipv6, p.net_proto, p.timeout);
}

void cresponder::update_certificates()
{
	try
	{
		for(auto &resolver : this->dnscrypt_providers_)
		{
			auto &prov = *resolver.second;
			std::string errmsg = prov.start();
			if( !errmsg.empty() )
			{
				log::error (errmsg);
				prov.stop();
			}
			if( prov.provider().is_ready() )
				log::debug ("provider ready");
		}
		constexpr const unsigned update = 7U; // minutes
		this->random_timer_.repeat = update*60.; // seconds
		log::info ("Resolvers: ", this->dnscrypt_providers_.size(), ", update: ",
			update, " sec");
	}
	catch(std::exception &e)
	{
		log::error (e.what());
		throw;
	}
	catch(...)
	{
		log::error ("failed to start certificate updater");
		throw;
	}
}

struct emessage : public query
{
	typedef std::pair <::crypt::pubkey, ::crypt::nonce> keys_t;

	const char *dummy() const override {return "enc";}

	std::unique_ptr <packet> clone() const override
	{
		return std::make_unique <emessage> (*this);
	}

	emessage(keys_t &&pknc, query &&q) :
		query(std::move(q)), session_(std::move(pknc)) {}

	const ::crypt::pubkey &pubkey() const {return std::get <::crypt::pubkey>
		(session_);}

	const ::crypt::nonce &nonce() const {return std::get <::crypt::nonce>
		(session_);}

private:

	keys_t session_;
};

void cresponder::respond (std::shared_ptr <network::incoming> &req)
{
	auto &msg = req->modify_message();
	if( this->server_ptr_ )
	{
		auto *emsg = dynamic_cast< emessage* > ( &msg );
		if( nullptr != emsg )
		{
			log::debug ("encrypting response: ", emsg->size());
			if (emsg->size() > 0u && emsg->is_dns()
				&& (pkt_rcode::noerror != emsg->rcode()))
				log::debug ("sending failure");
			this->server_ptr_->decryptor_mod().curve (emsg->nonce(), emsg->pubkey(),
				*emsg);
			log::debug ("encrypted response: ", emsg->size());
		}
	}
	this->dns::responder::respond(req);
}

void cresponder::process(std::shared_ptr<network::incoming> &&q)
{
	if( this->server_ptr_ )
	{
		assert( q );
		const auto &msg = q->message();
		log::debug ("analyzing server: ", msg.size());
		if (this->server_ptr_->certificate().is_dnscrypt_message (msg))
		{
			log::debug ("encrypted request: ", msg.size());
			assert (typeid (msg) == typeid (query));
			//! @todo upcast to query should not be necessary
			query &qr = dynamic_cast <query &> (q->modify_message());
			auto pknc = this->server_ptr_->decryptor_mod().uncurve(qr);
			log::debug ("decrypted request: ", qr.size());
			auto eptr = std::make_unique<emessage>(std::move(pknc), std::move(qr));
			q->replace_message(std::move(eptr));
		}
		else if (msg.is_dns())
		{
			assert (typeid (msg) == typeid (query));
			if (dynamic_cast <const query&> (msg).is_dnscrypt_cert_request
				(this->server_ptr_->hostname()))
			{
				log::debug ("certificate request: ", q->message().size());
				query cert = dynamic_cast <const query&> (q->message());
				//! @todo use raw DNS owner name or proper string?
				if (!cert.set_txt_answer (std::get <std::string>
						(cert.get_question()),
						this->server_ptr_->certificate().raw_signed()) )
					throw std::logic_error("failed to create certificae response");
				q->respond(cert);
				return;
			}
		}
	}
	this->dns::responder::process(std::move(q));
}

void cresponder::select_random_provider() const
{
	const auto &providers = this->dnscrypt_providers_;
	std::vector <std::string> ready;
	ready.reserve (providers.size());
	for(const auto &pp : providers)
	{
		const auto &r = *pp.second->provider_ptr();
		if( r.is_ready() )
			ready.emplace_back (pp.first);
	}
	if( !ready.empty() )
	{
		//! @todo numeric_cast
		unsigned mx = static_cast<unsigned>(ready.size());
		if( mx > RAND_MAX )
			mx = RAND_MAX;
		if( 0==mx )
			mx = 1;
		//! @todo numeric_cast
		this->random_dnscrypt_provider_ = ready.at (static_cast<unsigned>
			(std::rand()) % mx);
	}
	else
	{
		this->random_dnscrypt_provider_.clear();
		log::warning ("No ready DNScrypt providers");
	}
	log::debug ("Random DNScrypt provider: ", this->random_dnscrypt_provider_,
		"  (", ready.size(), '/', providers.size(), ')');
}

//! @todo: code duplication, dns::responder::random_provider
std::shared_ptr <network::provider> cresponder::random_provider (const query &inmsg)
	const
{
	{
		auto r = this->zone_provider (inmsg);
		if( r )
			return r;
	}
	if( this->dnscrypt_providers_.empty() )
		return this->responder::random_provider (inmsg);
	this->select_random_provider();
	if (this->random_dnscrypt_provider_.empty())
		return std::shared_ptr <network::provider> (nullptr);
	const auto it = this->dnscrypt_providers_.find (this->random_dnscrypt_provider_);
	if (this->dnscrypt_providers_.end() != it)
		return it->second->provider_ptr();
	else
		return std::shared_ptr <network::provider> (nullptr);
}

//! @todo code duplication?, see resolver.cpp:save_resolvers
void cresponder::save_providers (const std::string &filename) const
{
	std::ofstream ofile (filename);
	ofile << "Resolver address,Provider name,Provider public key\n";
	const auto &providers = this->dnscrypt_providers_;
	for (const auto &pp : providers)
	{
		const auto &r = *pp.second->provider_ptr();
		if (r.is_ready())
		{
			ofile
				<< r.address() << ','
				<< r.hostname() << ','
				<< r.public_key() << '\n'
			;
		}
	}
	if (!ofile)
		throw std::runtime_error ("Failed saving resolvers into: " + filename);
}

}} // namespace dns::crypt
