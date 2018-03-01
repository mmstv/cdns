#include <iostream>
#include <string>
#include <cassert>
#include <fstream>
#include <ctime>
#include <typeinfo>
#include <random>

#include <ev++.h>

#include "dns/query.hxx"
#include "network/provider.hxx"
#include "dns/crypt/resolver.hxx"
#include "dns/crypt/certifier.hxx"
#include "network/udp/upstream.hxx"
#include "network/tcp/upstream.hxx"
#include "backtrace/catch.hxx"
#include "sys/logger.hxx"

#include "network/net_config.h"
// for htons
#if defined (HAVE_ARPA_INET_H)
#   include <arpa/inet.h>
#elif defined (_WIN32)
#   include <winsock2.h>
#else
#   error "Need htons"
#endif

namespace logg = process::log;

//! @todo: this is ugly
template<network::proto TCP>
class cert_request : public std::conditional<static_cast<bool>(TCP),
	network::tcp::upstream, network::udp::out >::type
{
	std::shared_ptr <dns::crypt::resolver> crypt_provider_ptr_;
	dns::query question_;

	typedef typename std::conditional<static_cast<bool>(TCP), network::tcp::upstream,
		network::udp::out>::type base_t;

	std::unique_ptr<base_t> dns_request_;

	void pass_answer_downstream() override
	{
		dns::query &q = dynamic_cast <dns::query &> (this->message_mod());
		logg::debug ("processing TCP certificate reply, ", this);
		logg::debug ("ht: ", q.host_type(), ", size: ", q.size());
		std::vector<std::vector<std::uint8_t> > txt_data;
		q.get_txt_answer(txt_data);
		if( this->crypt_provider_ptr_->find_valid_certificate( txt_data ) )
		{   // success
			logg::debug ("CERT SUCC sending DNS Query: ", this->question_.size());
			this->message_mod().set_size(0);
			// this->message_ptr();
			auto qptr = std::make_unique <dns::query> (this->question_);
			this->dns_request_ = std::make_unique<base_t> (this->crypt_provider_ptr_,
				std::move (qptr), 5.5);
		}
		else
			logg::error ("certificate request failed");
	}


public:

	bool succ() const {return nullptr != this->dns_request_;}

	const dns::query &message() const
	{
		assert (dns_request_);
		return dynamic_cast <const dns::query &> (dns_request_->message());
	}

	// pass network::provider copy of dns::crypt::resolver
	// because certifier should not encrypt and decrypt DNS queries
	cert_request(std::shared_ptr<dns::crypt::resolver> &&c, const dns::query &q)
		: base_t (std::make_shared<network::provider>(*c),
			std::make_unique <dns::query> (c->hostname().c_str(), true), 5.5),
		crypt_provider_ptr_ (std::move(c)), question_ (q) {}
};


void run(int argc, char *argv[])
{
	// random_defice.entropy() returns 0. on Linux, which means it is not a random
	// device, but a pseudo-random generator
	std::random_device rd;
	std::srand (rd());
	network::proto tcp = network::proto::udp;
	bool txt = false, save = false;
	std::string question, ip_port("127.0.0.1:53"), host_key;
	for(int i=1; i<argc; ++i)
	{
		const std::string s(argv[i]);
		if( "-t" == s )
			tcp = network::proto::tcp;
		else if( "-txt" == s )
			txt = true;
		else if( "-d" == s )
			sys::logger_init (static_cast <int> (process::log::severity::debug),
				false);
		else if ("-s" == s)
			save = true;
		else if( '@' == s.at(0) )
			ip_port = s.substr(1U);
		else if( '+' == s.at(0) )
			host_key = s.substr(1u);
		else
			question = s;
	}
	auto make_up = [host_key, save] (const network::address &addr,
		const std::string &q, network::proto tt, bool tx)
	{
		auto msgptr = std::make_unique <dns::query> (q.c_str(), tx);
		const auto &msg = *msgptr;
		if (save)
		{
			std::ofstream tmpf("/tmp/ccdrill-tmp-udp.bin", std::ios::binary);
			if( tmpf )
			{
				std::cout << "\nSaved query into /tmp/ccdrill-tmp-udp.bin\n";
				tmpf.write (reinterpret_cast <const char*> (msg.bytes()),
					msg.size());
			}
		}
		if (save)
		{
			std::ofstream tmpf("/tmp/ccdrill-tmp-tcp.bin", std::ios::binary);
			if( tmpf )
			{
				std::cout << "\nSaved query into /tmp/ccdrill-tmp-tcp.bin\n";
				dns::query qt;
				union
				{
					std::uint16_t u16;
					std::uint8_t u8[2];
				} sz;
				//! @todo numeric cast
				sz.u16 = htons (static_cast <std::uint16_t> (msg.size()));
				qt.append (sz.u8, 2u);
				qt.append (msg);
				tmpf.write (reinterpret_cast <const char*> (qt.bytes()),
					qt.size());
			}
		}
		std::unique_ptr<network::upstream> req;
		if( !host_key.empty() )
		{
			std::string key = host_key, host = host_key;
			auto pos = host_key.rfind('_');
			if( std::string::npos != pos )
			{
				key = host_key.substr (1u+pos);
				host = host_key.substr (0, pos);
			}
			if( key.empty() || host.empty() )
				throw std::runtime_error ("bad DNScrypt host key: " + host_key);
			auto p = std::make_shared <dns::crypt::resolver> (host, addr,
				::crypt::pubkey (key.c_str()), tt);
			if( network::proto::tcp == tt )
			{
				auto tr = std::make_unique <cert_request <network::proto::tcp>>
					(std::move (p), msg);
				req = std::move(tr);
			}
			else
			{
				auto tr = std::make_unique <cert_request <network::proto::udp>>
					(std::move (p), msg);
				req = std::move(tr);
			}
		}
		else
		{
			auto prov = std::make_shared<network::provider>(addr, tt);
			if( network::proto::tcp == tt )
			{
				auto t = std::make_unique<network::tcp::upstream> (std::move(prov),
					std::move (msgptr), 5.5);
				req = std::move(t);
			}
			else
			{
				auto u = std::make_unique<network::udp::out>( std::move(prov),
					std::move (msgptr), 5.5);
				req = std::move(u);
			}
		}
		assert( req );
		return req;
	};
	std::cout << "Q: " << question << "; TXT: " << txt << "; TCP: "
		<< static_cast<bool>(tcp)
		<< "; IP:PORT: " << ip_port << std::endl;
	if( !question.empty() )
	{
		network::address addrr (ip_port, dns::crypt::defaults::port);
		auto req = make_up(addrr, question, tcp, txt);
		assert( req );
		auto dl = ev::get_default_loop();

		dl.run();
		assert (typeid (req->message()) == typeid (dns::query));
		dns::query ans = dynamic_cast <const dns::query&> (req->message());
		auto *ptcp = dynamic_cast <cert_request<network::proto::tcp>*> (req.get());
		if( ptcp && ptcp->succ() )
			ans = ptcp->message();
		else
		{
			auto *pudp = dynamic_cast <cert_request<network::proto::udp>*>
				(req.get());
			if( pudp && pudp->succ() )
				ans = pudp->message();
		}
		if( ans.size() > 0U )
		{
			std::cout << std::endl;
			ans.print (std::cout);
		}
	}
}

int main(int argc, char *argv[])
{
	constexpr const bool enable_fp = true;
	return trace::catch_all_errors(run, argc, argv, enable_fp);
}
