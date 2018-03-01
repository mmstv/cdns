#ifndef _DNS_CRYPT_DAEMON_HXX
#define _DNS_CRYPT_DAEMON_HXX 2

#include <memory>

#include <dns/daemon.hxx>
#include <dns/crypt/resolver.hxx>
#include <dns/crypt/crypt_options.hxx>
#include <dns/crypt/cresponder.hxx>
#include <dns/crypt/server_responder.hxx>
#include <dns/crypt/dll.hxx>

namespace dns { namespace crypt {

class certifier;

class DNS_CRYPT_API cdaemon : public dns::daemon
{
public:

	explicit cdaemon(std::shared_ptr<crypt::options> &&);

	~cdaemon();

	cdaemon(int argc, char *argv[]);

	void execute();

	void reload() override;

private:

};

}}
#endif
