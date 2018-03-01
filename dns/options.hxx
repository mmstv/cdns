#ifndef DNS_DETAIL_OPTIONS_HXX__
#define DNS_DETAIL_OPTIONS_HXX__ 1

#include <string>
#include <memory>

#include <dns/responder_parameters.hxx>
#include <dns/dll.hxx>

namespace dns {

namespace detail {

class flags;

struct DNS_API options : public responder_parameters
{
	unsigned int  connections_count_max;

	unsigned short max_log_level;
	bool syslog;
	bool disable_exceptions;
	bool daemonize;
	std::string listener_ip;
	std::string log_file;
	std::string user_name;

	options();
	virtual ~options() = default; // because of virtual function
	bool parse(int argc, char *argv[]);

protected:

	void add(const char *name, char symbol, unsigned short nparam = 0);
	virtual bool apply_option(int opt_flag, const char *optarg);

private:

	void options_apply();
	void parse_file(const char *fn);

	std::shared_ptr<flags> flags_ptr_;
};

}} // namespace dns::detail

#endif
