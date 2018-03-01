#ifndef BACKTRACE_EXCEPTIONS_HXX
#define BACKTRACE_EXCEPTIONS_HXX

#include <string>
#include <stdexcept>

#if 0
#include "util/push_disable_warnings.hxx"
#include <boost/preprocessor/stringize.hpp>
#include "util/pop_enable_warnings.hxx"
#endif

#include "backtrace/backtrace.hxx"

#if 0
#ifndef INSIST
#define INSIST( f ) \
	do \
	{ \
		if( !(f) ) \
			throw trace::logic_error("Assertion failure! \n" \
			BOOST_STRINGIZE(f) " \nat: " __FILE__ ":" BOOST_STRINGIZE(__LINE__)); \
	} while (0)
#endif
#endif

#ifdef _WIN32
#define BACKTRACE_NO_HIDE
#else
// Mac OS X clang converts __hidden__ exceptions to their base classes
#define BACKTRACE_NO_HIDE BACKTRACE_API
#endif

//! Backtrace for exceptions, floating point errors and segfaults
namespace trace
{

// user errors

//! Same as std::runtime_error, but with a callstack backtrace
class BACKTRACE_NO_HIDE runtime_error : public std::runtime_error
{
	std::string backtrace_;
public:
	explicit runtime_error(const std::string &message) :
		std::runtime_error(message), backtrace_(add_backtrace_string("")) {}
	const std::string &backtrace() const {return backtrace_;}
};

//! Same thing as std::range_error, but with a callstack backtrace
class BACKTRACE_NO_HIDE range_error : public std::range_error
{
public:
	explicit range_error(const std::string &message) :
		std::range_error(add_backtrace_string(message))
	{}
};


class BACKTRACE_NO_HIDE overflow_error : public std::overflow_error
{
public:
	explicit overflow_error(const std::string &message) :
		std::overflow_error(add_backtrace_string(message))
	{}
};

class BACKTRACE_NO_HIDE underflow_error : public std::underflow_error
{
public:
	explicit underflow_error(const std::string &message) :
		std::underflow_error(add_backtrace_string(message))
	{}
};


// logic, programmer errors
class BACKTRACE_NO_HIDE logic_error : public std::logic_error
{
	std::string backtrace_;
public:
	explicit logic_error(const std::string &message) :
		std::logic_error(message), backtrace_(add_backtrace_string(""))
	{}
	const std::string &backtrace() const {return backtrace_;}
};

class BACKTRACE_NO_HIDE length_error : public std::length_error
{
public:
	explicit length_error(const std::string &message) :
		std::length_error(add_backtrace_string(message))
	{}
};


class BACKTRACE_NO_HIDE out_of_range : public std::out_of_range
{
public:
	explicit out_of_range(const std::string &message) :
		std::out_of_range(add_backtrace_string(message))
	{}
};

class BACKTRACE_NO_HIDE domain_error : public std::domain_error
{
public:
	explicit domain_error(const std::string &message) :
		std::domain_error(add_backtrace_string(message))
	{}
};


class BACKTRACE_NO_HIDE invalid_argument : public std::invalid_argument
{
public:
	explicit invalid_argument(const std::string &message) :
		std::invalid_argument(add_backtrace_string(message))
	{}
};

} // namespace util

#undef BACKTRACE_NO_HIDE

#endif
