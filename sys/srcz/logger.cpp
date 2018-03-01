#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <ctime>

#include <iostream>
#include <sstream>
#include <atomic>
#include <iomanip>

#include "sys/logger.hxx"

#include "sys/preconfig.h" // for localtime_r
#ifndef HAVE_GMTIME_R
#error "linux has gmtime_r"
#endif

#ifndef _WIN32
#  include <syslog.h> // POSIX
#endif

namespace sys {

//! @todo deprecated
static unsigned short _max_log_level_ = static_cast <int>
	(process::log::severity::info); // LOG_CRIT; // LOG_ERR
static bool _syslog_ = false;
static FILE *_log_fp_ = nullptr;


void logger_open_syslog()
{
	assert( _syslog_ );
#ifndef _WIN32
	::openlog(nullptr, LOG_NDELAY | LOG_PID, LOG_DAEMON);
#endif
}

void logger_init(unsigned short loglevel, bool syslog)
{
	_max_log_level_ = loglevel;
	_syslog_ = syslog;
	if( _syslog_ )
		logger_open_syslog();
	process::log::set_system (syslog);
	process::log::set_severity (static_cast <process::log::severity> (loglevel));
}

#if 0
#ifndef MAX_LOG_LINE
# define MAX_LOG_LINE 1024U
#endif

#ifndef LOGGER_DELAY_BETWEEN_IDENTICAL_LOG_ENTRIES
# define LOGGER_DELAY_BETWEEN_IDENTICAL_LOG_ENTRIES 60
#endif
#ifndef LOGGER_ALLOWED_BURST_FOR_IDENTICAL_LOG_ENTRIES
# define LOGGER_ALLOWED_BURST_FOR_IDENTICAL_LOG_ENTRIES 5U
#endif


int loggery (const int crit, const char * const format, ...)
{
	//! @TODO thread safe?
	static char previous_line[MAX_LOG_LINE];
	static std::time_t last_log_ts = static_cast<time_t>( 0) ;
	static unsigned int burst_counter = 0U;

	char line[MAX_LOG_LINE];
	const char *urgency;
	va_list va;
	time_t now = time(NULL);
	size_t len;

	if( crit>_max_log_level_ )
		return 0;
	switch (crit) {
	case LOG_INFO:
		urgency = "[INFO] ";
		break;
	case LOG_WARNING:
		urgency = "[WARNING] ";
		break;
	case LOG_ERR:
		urgency = "[ERROR] ";
		break;
	case LOG_NOTICE:
		urgency = "[NOTICE] ";
		break;
	case LOG_DEBUG:
		urgency = "[DEBUG] ";
		break;
	case LOG_CRIT:
		urgency = "[CRITICAL] "; break;
	case LOG_ALERT:
		urgency = "[ALERT] "; break;
	case LOG_EMERG:
		urgency = "[EMERGENCY] "; break;
	default:
		urgency = "";
	}
	va_start(va, format);
	len = static_cast <size_t> (std::vsnprintf(line, sizeof line, format, va));
	va_end(va);

	if (len >= sizeof line) {
		assert(sizeof line >  0UL);
		len = sizeof line - 1UL;
	}
	line[len++] = 0;
#ifndef _WIN32
	if ( nullptr == _log_fp_ && _syslog_)
	{
		syslog(crit, "%s", line);
		return 0;
	}
#endif
	if (std::memcmp(previous_line, line, len) == 0) {
		burst_counter++;
		if (burst_counter > LOGGER_ALLOWED_BURST_FOR_IDENTICAL_LOG_ENTRIES &&
			now - last_log_ts < LOGGER_DELAY_BETWEEN_IDENTICAL_LOG_ENTRIES) {
			return 1;
		}
	} else {
		burst_counter = 0U;
	}
	last_log_ts = now;
	assert(sizeof previous_line >= sizeof line);
	std::memcpy(previous_line, line, len);
	FILE *log_fp = nullptr;
	if (_log_fp_ == nullptr)
		log_fp = crit >= LOG_NOTICE ? stdout : stderr;
	else
		log_fp = _log_fp_;
	timestamp_fprint(log_fp);
	std::fprintf(log_fp, "%s%s\n", urgency, line);
	std::fflush(log_fp);

	return 0;
}

int logger_noformaty(const int crit, const char * const msg)
{
	return loggery(crit, "%s", msg);
}
#endif

int logger_close()
{
#ifndef _WIN32
	if (_syslog_)
		::closelog();
#endif
	if( nullptr!=_log_fp_ )
	{
		int r = std::fclose(_log_fp_);
		_log_fp_ = nullptr;
		return r;
	}
	return 0;
}

} // namespace sys


namespace process
{

namespace log
{

namespace detail
{

#ifndef _WIN32
static_assert (static_cast <int> (severity::emergency) == LOG_EMERG, "syslog");
static_assert (static_cast <int> (severity::debug) == LOG_DEBUG, "syslog");
static_assert (static_cast <int> (severity::info) == LOG_INFO, "syslog");
#endif

static std::atomic <severity> _log_severity_ (severity::info);

static std::atomic <bool> _syslog_ (false);
static std::atomic <std::ostream *> _log_stream_ (nullptr);

inline const char *severity_cstr (severity p)
{
	switch (p)
	{
		case severity::emergency: return "EMERGENCY";
		case severity::alert: return "ALERT";
		case severity::critical: return "CRITICAL";
		case severity::error: return "ERROR ";
		case severity::warning: return "WARNING";
		case severity::notice: return "NOTICE";
		case severity::info: return "INFO";
		case severity::debug: return "DEBUG";
	}
	return "[UNKNOWN]";
}

inline struct std::tm local_time (std::time_t t)
{
#ifdef HAVE_GMTIME_R
	struct std::tm result;
	const auto * ts = ::localtime_r (&t, &result);
#else
#  ifdef __linux__
#    error "Linux has localtime_r"
#  endif
	//! @todo not thread safe, uses system static 'struct tm' variable
	const auto * ts = std::localtime (&t);
#endif
	if (nullptr == ts)
		throw std::system_error (errno, std::system_category(), "Failed to get "
			"system time");
#ifdef HAVE_GMTIME_R
	return result;
#else
	return *ts;
#endif
}

} // namespace process::log::detail

void SYS_API println (severity a_severity, const char *line)
{
	if (a_severity <= detail::_log_severity_.load())
	{
#ifndef _WIN32
		if (nullptr == detail::_log_stream_ && detail::_syslog_)
			::syslog (static_cast <unsigned short> (a_severity), "%s", line);
		else
#endif
		{
			const std::time_t now = std::time (nullptr);
			const struct std::tm t = detail::local_time (now);
			std::ostream *stream = detail::_log_stream_.load();
			if (nullptr == stream)
				stream = (a_severity >= severity::notice) ? &std::cout : &std::cerr;
			// See `man strftime` for time formats
			*stream << std::put_time (&t, "%c") << " [" << detail::severity_cstr
				(a_severity) << "] " << line << std::endl;
		}
	}
}

severity SYS_API get_severity (void)
{
	return detail::_log_severity_.load();
}

void SYS_API set_severity (severity p)
{
	detail::_log_severity_.store (p);
}

void SYS_API set_system (bool v)
{
	detail::_syslog_.store (v);
}

} // namespace process::log

void SYS_API self_test (void)
{
	log::info ("--str1", " test log::info1, ", 3.1415);
	log::debug ("--str2", " test log::debug1, ", -3.1415);
	log::notice ("--str3", " test log::notice1, ", 3.1415);
	log::error ("--str4", " test log::error, ", 3.1415);
	log::critical ("--str5", " test log::crit, ", 3.1415);
	log::emergency ("--str6", " test log::emerg, ", 3.1415);
	log::alert ("--str7", " test log::alert, ", 3.1415);
	log::warning ("--str8", " test log::warn, ", "Pi: ", 3.1415, ", 123: ", 123,
		", True: ", true, ", False: ", false);
}

} // namespace process
