#include <fstream>

#include "sys/logger.hxx"
#include "sys/sysunix.hxx"
#include "dns/crypt/daemon.hxx"
#include "dns/crypt/certificate.hxx"

#include <sodium.h>

#include "package.h"

namespace dns { namespace crypt {

namespace log = process::log;

void cdaemon::execute()
{
	auto cr = std::dynamic_pointer_cast<cresponder> (this->responder_ptr());
	cr->update_certificates();
	this->dns::daemon::execute();
	if (cr && !cr->cache_dir().empty())
	{
		const std::string fn = cr->cache_dir() + "/ready_dnscrypt_providers.csv";
		cr->save_providers (fn);
		log::notice ("Saved dnscrypt providers: ", fn);
	}
}

cdaemon::cdaemon (std::shared_ptr <crypt::options> &&opts)
	: dns::daemon (std::move (opts))
{
	const unsigned entropy = sys::entropy();
	log::notice ("Entropy: ", entropy);
	if (160u > entropy)
	{
		log::warning
			("This system doesn't provide enough entropy to quickly generate "
			"high-quality random numbers");
		log::warning
			("Installing the rng-utils/rng-tools or haveged packages may help.");
		log::warning
			("On virtualized Linux environments, also consider using virtio-rng.");
		log::warning
			("The service will not start until enough entropy has been collected.");
	}

	if (0 != sodium_init())
	{
		const char msg[] = "Unable to setup encryption library.";
		log::critical (msg);
		throw std::logic_error(msg);
	}
	::randombytes_set_implementation(&randombytes_salsa20_implementation);
}

cdaemon::~cdaemon()
{
	randombytes_close();
}

cdaemon::cdaemon (int argc, char *argv[])
	: cdaemon (std::make_shared <crypt::options>())
{
	const auto o = std::dynamic_pointer_cast<crypt::options>(this->options_ptr());
	if( o->parse(argc, argv) )
	{
		this->setup0();
		std::unique_ptr<server::responder> srv;
		if( !o->server_pubkey_file.empty() )
		{
			::crypt::pubkey pk = ::crypt::pubkey::load_binary_file(
				o->server_pubkey_file);
			log::debug ("pubkey: ", pk.fingerprint());
			::crypt::secretkey sk = ::crypt::secretkey::load_binary_file(
				o->server_secretkey_file);
			log::debug ("seckey: ", sk.fingerprint());
			certificate bc = certificate::load_binary_file
				(o->server_certificate_file, pk);
			bc.print_info();
			log::debug ("server key: ", bc.server_public_key().fingerprint(),
				", cipher: ", bc.cipher());
			srv = std::make_unique<server::responder>(
				std::string(o->server_hostname),
				std::move(pk), std::move(sk), std::move(bc));
		}
		auto r = std::make_shared<cresponder> (*o, o->dnscrypt_resolvers_file,
			std::move(srv));
		this->setup(std::move(r));
		this->execute();
	}
}


void cdaemon::reload()
{
#ifdef HAVE_LIBSYSTEMD
	sys::systemd_notify ("RELOADING=1");
#endif
	log::notice ("Reloading " PACKAGE_STRING " ...");
	const auto o = std::dynamic_pointer_cast <crypt::options> (this->options_ptr());
	auto cr = std::dynamic_pointer_cast <cresponder> (this->responder_ptr());
	assert (o && cr);
	cr->reload (*o, o->dnscrypt_resolvers_file);
#ifdef HAVE_LIBSYSTEMD
	sys::systemd_notify ("READY=1");
#endif
}

}} // namespace dns::crypt
