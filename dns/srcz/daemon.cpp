#include <vector>
#include <stdexcept>
#include <cassert>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <sstream>
#include <random>

#include "backtrace/backtrace.hxx"
#include "dns/daemon.hxx"
#include "dns/responder.hxx"
#include "dns/cache.hxx"
#include "dns/constants.hxx"
#include "network/udp/listener.hxx"
#include "network/tcp/listener.hxx"
#include "sys/logger.hxx"
#include "sys/sysunix.hxx"

#include "package.h"

#include "sys/preconfig.h"

namespace dns {

namespace log = process::log;

using sys::systemd_notify;

void daemon::start_listeners()
{
	assert( !this->listeners_started );
	if (this->listeners_started )
		return;
	auto rc1 = this->responder_ptr_;
	if (0 <= this->udp_listener_systemd_handle_ )
	{
		this->udp_listener_ = network::udp::listener::make_new (std::move(rc1),
			network::bound_listener <network::proto::udp>
			(this->udp_listener_systemd_handle_));
		this->set_address (this->udp_listener_->udp_socket().get_address());
	}
	else
		this->udp_listener_ = network::udp::listener::make_new(std::move(rc1),
			this->address());
	auto rc2 = this->responder_ptr_;
	if (0 <= this->tcp_listener_systemd_handle_ )
	{
		this->tcp_listener_ = network::tcp::listener::make_new (std::move(rc2),
			network::bound_listener <network::proto::tcp>
			(this->tcp_listener_systemd_handle_));
	}
	else
		this->tcp_listener_ = network::tcp::listener::make_new(std::move(rc2),
			this->address());
	log::notice ("Listening on ", this->address().ip_port());
	this->listeners_started = true;
#ifdef HAVE_LIBSYSTEMD
	sys::systemd_notify("READY=1");
#endif
}

void daemon::init_descriptors_from_systemd()
{
#ifdef HAVE_LIBSYSTEMD
	std::array <int, 2u> sds = sys::systemd_init_descriptors();
	if( sds[0] >= 0 )
	{
		assert (sds[1] >= 0);
		this->udp_listener_systemd_handle_ = sds[0];
		this->tcp_listener_systemd_handle_ = sds[1];
		log::notice ("Using systemd sockets: ", this->udp_listener_systemd_handle_,
			" ", this->tcp_listener_systemd_handle_);
	}
#endif
}

//! @todo: too complicated destructor, prone for exceptions
daemon::~daemon()
{
	log::notice ("Stopping daemon ...");
	reload_.stop();
#ifdef HAVE_LIBSYSTEMD
	systemd_notify("STOPPING=1");
#endif
	log::notice (" ... stopped");
}

void daemon::report_stats() const
{
	std::size_t reqcount = 0UL, recv_count = 0UL;
	if( this->tcp_listener_ )
	{
		reqcount += this->tcp_listener().q_count();
		recv_count += this->tcp_listener().count();
	}
	if( this->udp_listener_ )
	{
		reqcount += this->udp_listener().q_count();
		recv_count += this->udp_listener().count();
	}
	auto dl = ev::get_default_loop();
	if( this->responder_ptr() )
	{
		const auto &r = *(this->responder_ptr());
		log::notice ("Requests total: ", recv_count, ", processed: ",
			r.processed_count(), ", blacklisted: ", r.blacklisted_count(), ","
			" cached: ", r.cached_count());
		log::debug ("Requests still queued: ", reqcount, ", upstream: ",
			this->responder_ptr()->q_count(), ", iterations: ", dl.iteration());
	}
}

void interrupt_signal(ev::sig &s, int)
{
	s.loop.break_loop(ev::ALL);
}

//! @todo: how much can one actually do during SIGHUP, can one output to syslog?
void daemon::reload()
{
#ifdef HAVE_LIBSYSTEMD
	sys::systemd_notify ("RELOADING=1");
#endif
	log::notice ("Reloading " PACKAGE_STRING " ...");
	assert( responder_ptr_ );
	responder_ptr_->reload (this->options());
	this->report_stats();
#ifdef HAVE_LIBSYSTEMD
	sys::systemd_notify ("READY=1");
#endif
}

void reload_signal(ev::sig &s, int)
{
	assert( nullptr != s.data ); //! @todo: can one use assert in SIGHUP
	auto *dem = reinterpret_cast<daemon *> (s.data);
	//! @todo: how much can one actually do during SIGHUP, can one output to syslog?
	dem->reload();
}

void testing_timeout (ev::timer &w, int )
{
	log::notice ("TESTING FINISHED!");
	w.loop.break_loop(ev::ALL);
}

daemon::daemon(std::shared_ptr<detail::options> &&ops) : options_ptr_(std::move(ops))
{
	std::setlocale (LC_CTYPE, "C");
	std::setlocale (LC_COLLATE, "C");
	//! @todo what is this for?
	std::setvbuf(stdout, NULL, _IOLBF, BUFSIZ);
	try
	{
		// random_device is not always available, and might be pseudo-random
		std::random_device rnd;
		std::srand (rnd());
	}
	catch (const std::runtime_error &)
	{
		std::srand (static_cast <unsigned> (std::time (nullptr)
			% std::numeric_limits <int>::max()));
	}
	this->udp_listener_systemd_handle_ = -1;
	this->tcp_listener_systemd_handle_ = -1;
	this->listeners_started = false;
#ifdef SIGPIPE
	std::signal(SIGPIPE, SIG_IGN);
#endif
	if (0 == ev::supported_backends())
		throw std::logic_error ("libev does not provide any backends");
	auto dl = ev::get_default_loop();
	//! @todo: quick_exit? clean up after all events referencing main loop
	std::atexit (ev_default_destroy);

	interrupt_.set(dl);
	interrupt_.set<interrupt_signal>();
	interrupt_.priority = 0;
	interrupt_.start (SIGINT);
	terminate_.set(dl);
	terminate_.set<interrupt_signal>();
	terminate_.priority = 0;

	terminate_.start(SIGTERM);
#ifndef SIGHUP
#error "I want SIGHUP"
#endif
	reload_.set(dl);
	reload_.set<reload_signal>();
	reload_.data = this; //! @todo is this safe?
	reload_.priority = 0;
	reload_.start(SIGHUP);

	const char *t = std::getenv (PACKAGE "_TESTING_TIMEOUT");
	if( nullptr != t )
	{
		std::istringstream istr(t);
		ev::tstamp tout = 5.0; // seconds
		istr >> tout;
		if( !istr || tout < 0.001 || tout > 100. )
			tout =  5.;
		process::log::notice ("TESTING, TESTING, TESTING for ", tout, " secs");
		testing_timeout_.set<testing_timeout>();
		testing_timeout_.set(dl);
		testing_timeout_.set(tout);
		testing_timeout_.start();
	}
}

bool daemon::set_options(int argc, char *argv[])
{
	if (!options_ptr_->parse(argc, argv))
		return false;
	this->setup0();
	this->setup(nullptr);
	return true;
}

void daemon::set_options(std::shared_ptr<detail::options> &&opts)
{
	options_ptr_ = std::move(opts);
	this->setup0();
	this->setup(nullptr);
}

void daemon::setup0()
{
	const detail::options &opts = this->options();
	if (!opts.log_file.empty())
	{
		assert( !opts.syslog );
		FILE *logfp = std::fopen(opts.log_file.c_str(), "a");
		if( nullptr == logfp )
			throw std::runtime_error("Unable to open log file");
		//! @todo: sys::logger_init(opts.max_log_level, logfp);
	}
	else
		sys::logger_init(opts.max_log_level, opts.syslog);

	if( !opts.disable_exceptions )
		trace::enable_fp_exceptions();

#ifndef _WIN32
	if (opts.daemonize)
	{
		assert( opts.syslog || !opts.log_file.empty() );
		log::notice ("Running in daemon mode!");
		sys::do_daemonize();
	}
#endif

}

void daemon::setup(std::shared_ptr<responder> &&_responder_ptr)
{
	const detail::options &opts = this->options();
	if( _responder_ptr )
		this->responder_ptr_ = std::move(_responder_ptr);
	else
		this->responder_ptr_ = std::make_shared<responder>(opts);
	assert (this->responder_ptr_);
	this->set_address (network::address (opts.listener_ip.c_str(), defaults::port));

	log::notice ("Starting " PACKAGE_STRING " ...");
	log::info ("LogLevel: ", opts.max_log_level);

	this->init_descriptors_from_systemd();

#ifdef HAVE_LIBSYSTEMD
	sys::systemd_send_pid();
#endif
	// starting listeners here, so that all objects inside daemon are initialized
	this->start_listeners();

#ifndef _WIN32
	// downgrading user from root, AFTER listeners had been started
	if (!opts.user_name.empty())
		sys::revoke_privileges (opts.user_name);
#endif
}

void daemon::execute()
{
	log::info ("  ... started");
	auto dl = ev::get_default_loop();
	dl.run();
	this->report_stats();
	if (this->responder_ptr())
	{
		const auto &cr = *this->responder_ptr();
		if (!cr.cache_dir().empty())
		{
			const std::string fn2 =  cr.cache_dir() + "/dnscache.bin";
			cr.cache().save_as (fn2);
			log::notice ("Saved cache: ", fn2);
		}
	}
}

} // namespace dns
