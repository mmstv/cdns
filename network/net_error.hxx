#ifndef _NETWORK_NET_ERROR_HXX_
#define _NETWORK_NET_ERROR_HXX_

#include <stdexcept>
#include <cstring>
#include <string>
#include <system_error>
#include <cerrno>

#include <network/dll.hxx>

namespace network {

NETWORK_API const std::error_category &error_category();

class error : public std::system_error
{
public:

	error (int system_errno, const char *msg)
	   : std::system_error (system_errno, error_category(), msg) {}

	error (int system_errno, const std::string &msg)
	   : error (system_errno, msg.c_str()) {}
};

using net_error = error;

#ifdef _WIN32
NETWORK_API int get_errno (void);
#else
inline int get_errno (void) noexcept {return errno;}
#endif

} // namespace network

#endif
