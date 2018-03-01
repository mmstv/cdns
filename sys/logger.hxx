#ifndef _SYS_LOGGER_HXX_
#define _SYS_LOGGER_HXX_ 1

#include <iosfwd>
#include <sstream>
#include <cstring>

#include <sys/dll.hxx>

namespace sys {

int SYS_API logger_close();

void SYS_API logger_init(unsigned short loglevel, bool syslog);

} // namespace sys

namespace process {

SYS_API void self_test (void);

namespace log {

enum class severity : unsigned short
{
	emergency = 0,
	alert =     1,
	critical =  2,
	error =     3,
	warning =   4,
	notice =    5,
	info =      6,
	debug =     7
};

void SYS_API println (severity, const char *line);

severity SYS_API get_severity (void);

void SYS_API set_severity (severity);

void SYS_API set_system (bool);

namespace detail
{

template <typename T> inline void print (std::ostream &text_stream, const T& t)
{
	text_stream << t;
}

template <typename T, typename... Types> inline void print
	(std::ostream &text_stream, const T& t, const Types& ...values)
{
	text_stream << t;
	print (text_stream, values...);
}

template <typename T, typename... Types> inline void slog
	(severity a_severity, const T& t, const Types& ...values)
{
	if (a_severity <= log::get_severity())
	{
		//! @todo optimize: too many 'malloc's
		std::ostringstream ostr;
		print (ostr, t, values...);
		println (a_severity, ostr.str().c_str());
	}
}

} // namespace process::log::detail

template <typename T, typename... Types> inline void emergency
	(const T& t, const Types& ...values)
{
	detail::slog (severity::emergency, t, values...);
}

template <typename T, typename... Types> inline void alert
	(const T& t, const Types& ...values)
{
	detail::slog (severity::alert, t, values...);
}

template <typename T, typename... Types> inline void critical
	(const T& t, const Types& ...values)
{
	detail::slog (severity::critical, t, values...);
}

template <typename T, typename... Types> inline void error
	(const T& t, const Types& ...values)
{
	detail::slog (severity::error, t, values...);
}

template <typename T, typename... Types> inline void warning
	(const T& t, const Types& ...values)
{
	detail::slog (severity::warning, t, values...);
}

template <typename T, typename... Types> inline void notice
	(const T& t, const Types& ...values)
{
	detail::slog (severity::notice, t, values...);
}

template <typename T, typename... Types> inline void info
	(const T& t, const Types& ...values)
{
	detail::slog (severity::info, t, values...);
}

template <typename T, typename... Types> inline void debug
	(const T& t, const Types& ...values)
{
	detail::slog (severity::debug, t, values...);
}

}} // namespace process::log

namespace dns { using namespace sys; }

#endif
