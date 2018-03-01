#include <fstream>
#include <sstream>
#include <algorithm>

#include "network/net_error.hxx"
#include "network/provider.hxx"
#include "dns/responder.hxx"
#include "dns/query.hxx"
#include "dns/filter.hxx"
#include "dns/cache.hxx"
#include "dns/hosts.hxx"
#include "dns/constants.hxx"
#include "network/udp/upstream.hxx"
#include "network/tcp/upstream.hxx"
#include "network/incoming.hxx"
#include "sys/logger.hxx"
#include "network/listener.hxx"
#include "dns/responder_parameters.hxx"

namespace log = process::log;

namespace dns {

responder::responder() : network::responder(7.5), blacklisted_count_(0),
	processed_count_(0), cached_count_(0)
{
	this->cache_ptr_ = std::make_shared <class cache> (cache::defaults::min_ttl());
	this->whitelist_ptr_ = std::make_shared<filter>();
	this->blacklist_ptr_ = std::make_shared<filter>();
	this->hosts_ptr_ = std::make_shared<::dns::hosts>();
}

responder::responder (const responder::parameters &params)
	: network::responder (params.timeout), blacklisted_count_(0),
	processed_count_(0), cached_count_(0), cache_dir_ (params.cachedir),
	noipv6_ (params.noipv6)
{
	bool has_cache = false;
	if (!this->cache_dir().empty())
	{
		const std::string fn = this->cache_dir() + "/dnscache.bin";
		try
		{
			this->cache_ptr_ = std::make_shared <class cache> (dns::cache::load (fn,
				params.min_ttl));
			log::debug ("Cache entries: ", this->cache().size());
			this->cache_ptr_->collect_garbage();
			has_cache = true;
			log::notice ("Loaded ", this->cache().size(), " entries from cache: ",
				fn);
		}
		catch (const std::runtime_error &e)
		{
			log::error ("Failed to load cache: ", e.what());
		}
	}
	if (!has_cache)
		this->cache_ptr_ = std::make_shared<class cache> (params.min_ttl);
	assert (this->cache_ptr_);
	this->reload (params);
}

std::unique_ptr <network::packet> responder::new_packet() const
{
	return std::make_unique<query>();
}

void responder::reload (const parameters &params)
{
	try
	{
		log::info ("TCPonly: ", static_cast<int>(params.net_proto), ", noIPV6: ",
			 params.noipv6);
		log::info ("DNS resolvers from: ", params.resolvers);
		std::ifstream dnsf (params.resolvers);
		auto rslvs = responder::from_file (dnsf, params.noipv6, params.net_proto);
		this->dns_providers_.clear();
		this->dns_providers_.reserve (rslvs.size());
		for( auto &p : rslvs )
			this->dns_providers_.emplace_back (std::make_shared <network::provider>
				(std::move(p)));
	}
	catch(std::runtime_error &e) //! @todo only ignore file operations
	{
		log::warning (e.what());
	}
	this->noipv6_ = params.noipv6;

	this->whitelist_ptr_ = std::make_shared<filter>();
	for (const auto &fn : params.whitelists)
		whitelist_ptr_->merge (filter (fn));
	this->blacklist_ptr_ = std::make_shared <filter>();
	for (const auto &fn : params.blacklists )
		this->blacklist_ptr_->merge (filter (fn));
	if( !params.onion.empty() )
	{
		network::address onion_addr (params.onion, 5353);
		// Tor DNS does not support TCP ?
		this->onion_provider_ptr_ = std::make_shared <network::provider> (onion_addr,
			network::proto::udp);
	}
	else
		this->onion_provider_ptr_.reset();
	if( !params.hosts.empty() )
		this->hosts_ptr_ = std::make_shared<::dns::hosts>(params.hosts.c_str(),
			params.noipv6);
	else
		this->hosts_ptr_ = std::make_shared<::dns::hosts>();
	log::info ("DNS resolvers: ", dns_providers_.size(), ", Blacklist: ",
		blacklist_ptr_->count(), ", Whitelist: ", whitelist_ptr_->count(),
		", Onion: ", params.onion, ", Hosts: ", hosts_ptr_->count(),
		", Cache: ", cache_ptr_->size());
}

std::vector<network::provider> responder::from_file (std::istream &f,
	const bool noipv6, const network::proto p)
{
	// See `man resolv.conf`
	constexpr static const char *const key[4] = {"search", "domain", "sortlist",
		"options"};
	std::string word;
	word.reserve (64);
	std::vector<network::provider> result;
	while (f>>word)
	{
		assert (!word.empty());
		if ('#' == word.front() || &key[4] != std::find (std::begin (key), std::end
			(key), word))
		{
			f.ignore (100000, '\n'); // skip to the end of line
			continue;
		}
		else if ("nameserver" == word)
		{
			f >> word;
			f.ignore (100000000000, '\n'); // skip to the end of line
		}
		if (f && !word.empty())
		{
			network::address addr (word, dns::defaults::port);
			if (addr.inet() == network::inet::ipv4 || !noipv6)
				result.emplace_back (network::provider (std::move (addr), p));
		}
	}
	return result;
}

short responder::pre_filter (query &dns_query)
{
	auto q = dns_query.get_question();
	std::string &owner = std::get <std::string> (q);
	if (!owner.empty() && '.' == owner.back())
		owner.pop_back();
	const bool pass = (!this->whitelist_ptr_ || this->whitelist_ptr_->empty()
		|| this->whitelist_ptr_->match (owner))
		&& !this->blacklist_ptr_->match (owner)
		&& (!(this->noipv6_ && (rr_type::aaaa == std::get <rr_type> (q))));
	const char *const msgt = pass ? "Query: [" : "Blacklisted: [";
	log::info (msgt, std::get <rr_class> (q), ' ', std::get <rr_type> (q),
		"] ", owner, '.');
	if (!pass)
	{
		++blacklisted_count_;
		dns_query.mark_refused();
		return 1; // respond directly, no caching
	}
	const bool cr = this->cache_ptr_->retrieve(dns_query);
	if( cr )
	{
		log::info ("Cached: ", owner);
		++cached_count_;
		return 1; // response must be sent to the 'incoming' peer directly
	}
	// parse hosts database
	std::string ip = this->hosts_ptr_->response(owner);
	if( !ip.empty() )
	{
		log::info ("Hosts: ", owner, " = ", ip);
		if( dns_query.set_answer(owner, ip) )
			return 2; // send response to the 'incoming' peer, post-filter, and cache
	}
	return 0; // ask for answer upstream
}

void responder::store(const query &msg)
{
	this->cache_ptr_->store (msg);
	if (!this->cache_dir().empty() && 0 == (this->cache().size() % 16))
	{
		std::string cfn = this->cache_dir() + "/dnscache.bin";
		log::debug ("saving cache into: ", cfn);
		this->cache().save_as (cfn);
	}
}

void responder::respond(std::shared_ptr<network::incoming> &req)
{
	req->respond(req->message());
}

template<network::proto TCP>
class _upstream_incoming_ : public std::conditional< static_cast<bool>(TCP),
	network::tcp::upstream, network::udp::out >::type
{
	typedef typename std::conditional <static_cast<bool>(TCP),
		network::tcp::upstream, network::udp::out>::type base_t;
public:

	_upstream_incoming_ (std::shared_ptr<network::provider> &p,
		std::shared_ptr <network::incoming> &&inptr, double tsec)
		: base_t (std::shared_ptr <network::provider> (p), p->adapt_message
			(inptr->message_ptr()), tsec), inptr_ (std::move(inptr))
	{}

	void pass_answer_downstream() override
	{
		auto r = std::dynamic_pointer_cast<responder>(
			inptr_->listener_ptr()->responder_ptr());
		assert( r );
		inptr_->replace_message (this->message_ptr()->clone());
		assert (this->message_ptr());
		assert (typeid (*this->message_ptr()).before (typeid (query))
			|| typeid (*this->message_ptr()) == typeid (query));

		const query &answer = dynamic_cast <const query&> (*this->message_ptr());
		log::info ("Reply: ", answer.short_info(), "; From: ",
			this->address().ip_port());
		if (pkt_rcode::noerror == answer.rcode()
			|| pkt_rcode::nxdomain == answer.rcode())
		{
			r->store (answer); // cache
			r->respond (inptr_);
		}
		else
			r->respond (inptr_); // do not store failures in the cache
	}

private:

	std::shared_ptr<network::incoming> inptr_;
};

//! @todo: code duplication, see network::abs_listener::on_message
void responder::collect_garbage()
{
	auto &reqs = this->upstream_requests_;
	if (!reqs.empty())
	{
		// clean up requests
		std::size_t last = 0;
		for (std::size_t i=0; i<reqs.size(); ++i)
		{
			auto &qreq = reqs[i];
			if( qreq && !qreq->is_active() ) // is_closed
			{
				qreq.reset();
				reqs.erase(reqs.begin() + static_cast <::ssize_t> (i));
			}
			else
				last = i + 1U;
		}
		reqs.resize(last);
	}
	if (reqs.size() > 2U)
	{
		log::debug ("active requests: ", reqs.size(), ", capacity: ",
			reqs.capacity(), ", total: ", this->processed_count_);
	}
}

void responder::process (std::shared_ptr<network::incoming> &&req)
{
	assert( req );
	assert (req->message_ptr());

	assert (typeid (*req->message_ptr()).before (typeid (dns::query))
		|| typeid (*(req->message_ptr())) == typeid (dns::query));
	const query &origmsg = dynamic_cast <const query&> (*(req->message_ptr()));
	if( !origmsg.is_dns() )
	{
		req->close(); // silently drop bad connections
		return;
	}
	++(this->processed_count_);
	auto resp = req->message_ptr()->clone(); // original unfolded message
	auto fmsg = std::make_unique <query> (origmsg); // filtered DNS message
	short fr = this->pre_filter (*fmsg);
	if( 0 == processed_count_ % 1000 ) //! @todo parameter
		log::notice ("Queries: ", processed_count_, ", blacklisted: ",
			blacklisted_count_, ", from cache: ", cached_count_, ", cache size: ",
			cache_ptr_->size());
	if( 1==fr )
	{
		// no need to create upstream request or post-filter or cache
		req->replace_message (std::move (fmsg));
		this->respond(req); // respond directly no caching
		return;
	}
	else if( 2==fr )
	{
		// no need to create upstream request
		this->store (*fmsg); // store in cache
		req->replace_message (std::move (fmsg));
		this->respond (req);
		return;
	}
	auto prov = this->random_provider (origmsg);
	if( prov )
	{
		this->collect_garbage();
		try
		{
			if( network::proto::tcp == prov->net_proto() )
			{
				this->upstream_requests_.emplace_back(
					std::make_shared<_upstream_incoming_ <network::proto::tcp>>
					(prov, std::move (req), this->timeout_seconds()));
				assert( !req );
			}
			else
			{
				assert( req && prov );
				this->upstream_requests_.emplace_back(
					std::make_shared<_upstream_incoming_<network::proto::udp> >
					(prov, std::move(req), this->timeout_seconds()) );
			}
		}
		catch(network::error &e)
		{
			assert( prov );
			const auto addr = prov->address();
			resp->mark_servfail(); // failing original unfolded message
			assert( req );
			req->replace_message (std::move (resp));
			//! @todo: try another povider
			prov->increment_failures();
			log::error ("Request to: ", addr.ip_port(), " failed(", prov->failures(),
				"): ", e.what());
			this->respond(req);
		}
		assert( prov );
	}
	else
	{
		assert (req);
		resp->mark_servfail(); // failing original unfolded message
		log::error ("no resolvers: ", resp->size());
		req->replace_message (std::move (resp));
		this->respond (req);
	}
}

void responder::select_random_provider() const
{
	const auto &providers = this->dns_providers_;
	unsigned mx = static_cast<unsigned>(providers.size());
	if( mx > RAND_MAX )
		mx = RAND_MAX;
	if( 0==mx )
		mx = 1;
	this->random_provider_ = static_cast<unsigned>(std::rand())%mx;
	log::debug ("Random provider: ", random_provider_, '/', providers.size());
}

std::shared_ptr <network::provider> responder::zone_provider (const query &dns_query)
	const
{
	if( this->onion_provider_ptr_ )
	{
		const char onion[] = ".onion";
		constexpr const unsigned onionlen = 6;
		static_assert( onionlen + 1U == sizeof(onion), "size");
		const std::string owner = dns_query.hostname();
		if (owner.length() > onionlen && onion == owner.substr (owner.length()
			- onionlen))
		{
			log::info ("forwarding: ", owner, " to: ",
				onion_provider_ptr_->address().ip_port());
			return this->onion_provider_ptr_;
		}
	}
	return nullptr;
}

//! @todo: code duplication, dns::crypt::responder::random_provider
std::shared_ptr<network::provider> responder::random_provider(const query &dns_query)
	const
{
	{
		auto r = this->zone_provider (dns_query);
		if( r )
			return r;
	}
	if( dns_providers_.empty() )
		return std::shared_ptr<network::provider>(nullptr);
	this->select_random_provider();
	if( this->random_provider_>= this->dns_providers_.size() )
		throw std::logic_error("random DNS provider is not in the list");
	return this->dns_providers_.at(this->random_provider_);
}

} // namespace dns
