#ifndef DNS_RESPONDER_HXX
#define DNS_RESPONDER_HXX

#include <iosfwd>
#include <vector>
#include <memory>
#include <cassert>

#include <network/responder.hxx>
#include <network/constants.hxx>
#include <network/fwd.hxx>
#include <dns/dll.hxx>

namespace dns {

class query;
class filter;
class cache;
class hosts;
class responder_parameters;

class DNS_API responder : public network::responder
{
public:

	typedef ::dns::responder_parameters parameters;

	responder();

	explicit responder (const parameters &);

	virtual ~responder() = default; // because of virtual functions

	static std::vector<network::provider> from_file (std::istream &text_stream,
		bool noipv6, network::proto);

	short pre_filter(query &dns_query);

	void respond (std::shared_ptr<network::incoming> &) override;

	void store(const query &msg);

	void add_provider(std::shared_ptr<network::provider> &&p)
	{
		assert( p );
		dns_providers_.emplace_back(std::move(p));
	}

	std::size_t q_count() const {return upstream_requests_.size();}

	std::size_t processed_count() const {return processed_count_;}
	std::size_t blacklisted_count() const {return blacklisted_count_;}
	std::size_t cached_count() const {return cached_count_;}

	void process(std::shared_ptr <network::incoming> &&) override;

	std::unique_ptr <network::packet> new_packet() const override;

	void reload(const parameters &);

	//! Remove closed upstream requests
	void collect_garbage();

	const std::string &cache_dir() const {return cache_dir_;}

	const class cache &cache() const {return *cache_ptr_;}

protected:

	std::shared_ptr<network::provider> zone_provider (const query &) const;

	virtual std::shared_ptr<network::provider> random_provider (const query &) const;

private:

	void select_random_provider() const;

	std::shared_ptr<filter> whitelist_ptr_, blacklist_ptr_;
	std::shared_ptr<class cache> cache_ptr_;
	std::shared_ptr<hosts> hosts_ptr_;
	std::vector< std::shared_ptr<network::provider> > dns_providers_;
	std::vector< std::shared_ptr<network::upstream> > upstream_requests_;
	std::shared_ptr<network::provider> onion_provider_ptr_;
	mutable unsigned random_provider_ = 99999999U;
	std::size_t blacklisted_count_, processed_count_, cached_count_;
	std::string cache_dir_;
	bool noipv6_ = false;
};

} // namespace dns
#endif
