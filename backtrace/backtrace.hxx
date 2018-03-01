#ifndef BACKTRACE_HXX
#define BACKTRACE_HXX

#include <iosfwd>
#include <string>

#include "backtrace/dll.hxx"

//! Cross-platform operating system related functions
namespace trace {

//! Prints callstack backtrace to stream, if available in OS
void print_backtrace(std::ostream &);

//! Returns callstack backtrace appended to input string.
/*! \param[in] msg The string prepended to backtrace.
 */
std::string BACKTRACE_API add_backtrace_string(const std::string& msg);

//! Enables floating point exceptions
bool BACKTRACE_API  enable_fp_exceptions();

//! Speeds up computations with very small numbers by reducing accuracy
bool BACKTRACE_API  set_flush_denormals_to_zero();

//! Returns path name of the current executable
BACKTRACE_API std::string this_executable_path();

}

#endif
