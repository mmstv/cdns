#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include "dns/options.hxx"
#include "sys/logger.hxx"
#include "sys/str.hxx"

#include "package.h"

#ifndef HAVE_GETOPT_H
#error "linux has getopt.h"
#endif
#include <getopt.h>

#ifndef DEFAULT_RESOLVERS_LIST
# ifdef _WIN32
#  define DEFAULT_RESOLVERS_LIST "dns-resolvers.txt"
# else
#  define DEFAULT_RESOLVERS_LIST PKGDATADIR "/dns-resolvers.txt"
# endif
#endif

namespace dns {

namespace detail {

static const struct option _long_options_[] = {
	{ "local-address", 1, NULL, 'a' },
#ifndef _WIN32
	{ "daemonize", 0, NULL, 'd' },
#endif
	{ "resolvers-list", 1, NULL, 'L' },
	{ "logfile", 1, NULL, 'l' },
	{ "loglevel", 1, NULL, 'm' },
	{ "exceptions", 0, NULL, 'x' },
#ifndef _WIN32
	{ "syslog", 0, NULL, 's' },
#endif
	{ "max-active-requests", 1, NULL, 'n' },
	{ "user", 1, NULL, 'u' },
	{ "tcp-only", 0, NULL, 't' },

	{ "min-ttl", 1, NULL, 'M' },
	{ "onion", 1, NULL, 'O' },
	{ "no-ipv6", 0, NULL, '6' },
	{ "blacklist", 1, nullptr, 'B' },
	{ "whitelist", 1, NULL, 'W' },
	{ "hosts", 1, nullptr, 'H' },
	{ "timeout", 1, nullptr, 'T'},
	{ "cachedir", 1, nullptr, 'E'},

	{ "version", 0, NULL, 'V' },
	{ "help", 0, NULL, 'h' },
	{ NULL, 0, NULL, 0 }
};
#ifndef _WIN32
static const char _options_str_[] = "a:dhL:l:m:n:su:tVxM:O:6B:W:H:T:E:";
#else
static const char _options_str_[] = "a:hL:l:m:n:u:tVxM:O:6B:W:H:T:E:";
#endif

void normalize(std::string &word)
{
	std::size_t pos = word.find_first_of("-_");
	if( word.npos != pos )
		word = word.substr(0, pos) + word.substr(1U+pos);
	if (!word.empty())
		sys::ascii_tolower (word);
}

class flags
{
	std::vector<struct option> long_options_;
	std::string short_options_;

public:

	const std::string &options() const {return short_options_;}

	const std::vector<struct option> &long_options() const {return long_options_;}

	flags()
	{
		constexpr const unsigned n = sizeof(_long_options_) / sizeof(struct option);
		static_assert( 8 < n && 40 > n, "size" );
		for(unsigned i=0; i<n; ++i)
		{
			const struct option &opt = _long_options_[i];
			long_options_.emplace_back(opt);
			if( nullptr == opt.name )
				break;
		}
		short_options_ = _options_str_;
	}

	void add(const struct option &o)
	{
		if( nullptr != o.name )
		{
			long_options_.back() = o;
			struct option null{NULL, 0, NULL, 0};
			long_options_.emplace_back(null);
			if( std::isalnum(o.val) )
			{
				short_options_.push_back(static_cast<char>(o.val));
				if( 0 != o.has_arg )
					short_options_.push_back(':');
			}
		}
	}

	const struct option *find_option(const std::string &optname)
	{
		for(const auto &opt : long_options_)
		{
			if( nullptr != opt.name )
			{
				std::string n(opt.name);
				normalize(n);
				if( optname == n )
					return &opt;
			}
		}
		return nullptr;
	}
};

void options_version (void)
{
#ifdef PACKAGE_VENDOR
	puts(PACKAGE_STRING "-" PACKAGE_VENDOR);
#else
	puts(PACKAGE_STRING);
#endif
	puts("");
	printf("Compilation date: %s\n", __DATE__);
#ifdef HAVE_LIBSYSTEMD
	puts("Support for systemd socket activation: present");
#endif
	puts("Support for the XChaCha20-Poly1305 cipher: present");
}

void options_usage(const struct option *options)
{
	options_version();
	std::puts ("\nOptions:\n");
	do {
		if (options->val < 256) {
			std::printf ("  -%c\t--%s%s\n", options->val, options->name,
				options->has_arg ? "=..." : "");
		} else {
			std::printf ("\t--%s%s\n", options->name,
				options->has_arg ? "=..." : "");
		}
		options++;
	} while (options->name != NULL);
#ifndef _WIN32
	std::puts ("\nPlease consult the " PACKAGE " man page for details.\n");
#endif
}

options::options()
{
	this->noipv6 = false;
	this->disable_exceptions = false;
	this->log_file.clear();
	this->max_log_level = static_cast <int> (process::log::severity::info);
	this->connections_count_max = 250u; //! @todo unused
	this->listener_ip = "127.0.0.1:53";
	this->log_file.clear();
	this->resolvers = DEFAULT_RESOLVERS_LIST;
	this->syslog = false;
	this->daemonize = false;
	this->net_proto = network::proto::udp;
	this->min_ttl = 60; // seconds
	this->timeout = 7.5; // seconds
	flags_ptr_ = std::make_shared<flags>();
}

void options::add(const char *name, char symbol, unsigned short nparam)
{
	struct option opt{name, nparam, NULL, symbol};
	this->flags_ptr_->add(opt);
}

} // namespace detail

const char *
options_get_col(char * const * const headers, const size_t headers_count,
				char * const * const cols, const size_t cols_count,
				const char * const header)
{
	size_t i = 0UL;
	while (i < headers_count) {
		if (strcmp(header, headers[i]) == 0) {
			if (i < cols_count) {
				return cols[i];
			}
			break;
		}
		i++;
	}
	return nullptr;
}

namespace detail {

void options::options_apply()
{
	if (this->daemonize)
	{
		if (this->log_file.empty())
			this->syslog = true;
	}
	if (!this->log_file.empty() && this->syslog)
		throw std::runtime_error("--logfile and --syslog are mutually exclusive");
}

void options::parse_file(const char *fn)
{
	std::ifstream file(fn);
	while( file )
	{
		std::string line;
		std::getline(file, line);
		if( !line.empty() && line.at(0) != '#' && line.length() < 512UL )
		{
			std::istringstream istr(line);
			std::string word;
			istr >> word;
			if( istr && !word.empty() )
			{
				normalize(word);
				const struct option *opt = this->flags_ptr_->find_option(word);
				if( nullptr!=opt )
				{
					istr >> word;
					if( !istr )
						word = "";
					if( !(0==opt->has_arg && ("no" == word || "off" == word
						|| "false" == word)) )
					{
						if( !this->apply_option(opt->val, word.c_str()) )
							process::log::warning ("Help or version options"
								" are not supported in configuration file");
					}
				}
			}
		}
	}
}

bool options::parse (int argc, char *argv[])
{
	if (argc == 2 && argv[1][0] != '-' && argv[1][0] != '\0')
		this->parse_file(argv[1]);
	else
	{
		int opt_flag;
		int option_index = 0;
		while ((opt_flag = getopt_long(argc, argv,
									   flags_ptr_->options().c_str(),
									   flags_ptr_->long_options().data(),
									   &option_index)) != -1)
		{
			if( !this->apply_option(opt_flag, optarg) )
				return false;
		}
	}
	this->options_apply();
	return true;
}

bool options::apply_option(int opt_flag, const char *optarg)
{

	switch (opt_flag) {
	case 'a':
		this->listener_ip = optarg;
		break;
	case 'd':
		this->daemonize = 1;
		break;
	case 'h':
		options_usage(flags_ptr_->long_options().data());
		return false;
	case 'l':
		this->log_file = optarg;
		break;
	case 'L':
		this->resolvers = optarg;
		break;
#ifndef _WIN32
	case 's':
		this->syslog = 1;
		break;
#endif
	case 'm': {
		char *endptr;
		const long log_level = strtol(optarg, &endptr, 10);

		if (*optarg == 0 || *endptr != 0 || log_level < 0)
			throw std::runtime_error ("Invalid max log level");
		//! @todo numeric_cast
		this->max_log_level = static_cast<unsigned short>(log_level);
		break;
	}
	case 'n': {
		char *endptr;
		const unsigned long connections_max = strtoul(optarg, &endptr, 10);

		if (*optarg == 0 || *endptr != 0 || connections_max <= 0U ||
			connections_max > UINT_MAX)
			throw std::runtime_error("Invalid max number of active request");
		this->connections_count_max = static_cast <unsigned int> (connections_max);
		break;
	}
	case 'u': {
		this->user_name = optarg;
		break;
	}
	case 't':
		this->net_proto = network::proto::tcp;
		break;
	case 'V':
		options_version();
		return false;
	case 'x':
		this->disable_exceptions = true;
		process::log::notice ("Floating point exceptions disabled");
		break;
	case 'M':
		//! @todo numeric_cast
		this->min_ttl = static_cast<unsigned short>(std::atoi(optarg));
		break;
	case 'O':
		this->onion = optarg;
		break;
	case '6':
		this->noipv6 = true;
		break;
	case 'B':
		this->blacklists.emplace (optarg);
		break;
	case 'W':
		this->whitelists.emplace (optarg);
		break;
	case 'H':
		this->hosts = optarg;
		break;
	case 'T':
		this->timeout = std::atof (optarg);
		break;
	case 'E':
		this->cachedir = optarg;
		break;
	default:
		// fprintf(stderr, "Unknown option: '%c' (%d)\n", (char)opt_flag, opt_flag);
		fprintf(stderr, "\nUse -h or --help to get list of options\n\n");
		// options_usage();
		throw std::runtime_error("Unknown option!");
	}
	return true;
}

} // namespace detail

} // namespace dns
