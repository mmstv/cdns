#include <map>
#include <mutex>
#include <cassert>

#include "network/net_error.hxx"

#if defined (_WIN32)
#include <winsock2.h>
#endif

namespace network {

#ifdef _WIN32
namespace { namespace detail
{
// For error code meanings see:
// /usr/include/boost/system/detail/error_code.ipp
// /usr/include/boost/system/error_code.hpp
// /usr/include/asm-generic/errno.h

// Windows socket error codes are in:
// /usr/x86_64-w64-mingw32/include/psdk_inc/_wsa_errnos.h

// from libevent/evutil.c
#define E(code, s) { code, (s " [" #code "]") }

static constexpr const struct
{
	const int code;
	const char *const msg;
}
windows_socket_errors[] =
{
	E(WSAEINTR, "Interrupted function call"),
	E (WSAEBADF, "Bad file descriptor"),
	E(WSAEACCES, "Permission denied"),
	E(WSAEFAULT, "Bad address"),
	E(WSAEINVAL, "Invalid argument"),
	E(WSAEMFILE, "Too many open files"),

	E(WSAEWOULDBLOCK,  "Resource temporarily unavailable"),
	E(WSAEINPROGRESS, "Operation now in progress"),
	E(WSAEALREADY, "Operation already in progress"),
	E(WSAENOTSOCK, "Socket operation on nonsocket"),
	E(WSAEDESTADDRREQ, "Destination address required"),
	E(WSAEMSGSIZE, "Message too long"),
	E(WSAEPROTOTYPE, "Protocol wrong for socket"),
	E(WSAENOPROTOOPT, "Bad protocol option"),
	E(WSAEPROTONOSUPPORT, "Protocol not supported"),
	E(WSAESOCKTNOSUPPORT, "Socket type not supported"),
	E(WSAEOPNOTSUPP, "Operation not supported"),
	E(WSAEPFNOSUPPORT,  "Protocol family not supported"),
	E(WSAEAFNOSUPPORT, "Address family not supported by protocol family"),
	E(WSAEADDRINUSE, "Address already in use"),
	E(WSAEADDRNOTAVAIL, "Cannot assign requested address"),
	E(WSAENETDOWN, "Network is down"),
	E(WSAENETUNREACH, "Network is unreachable"),
	E(WSAENETRESET, "Network dropped connection on reset"),
	E(WSAECONNABORTED, "Software caused connection abort"),
	E(WSAECONNRESET, "Connection reset by peer"),
	E(WSAENOBUFS, "No buffer space available"),
	E(WSAEISCONN, "Socket is already connected"),
	E(WSAENOTCONN, "Socket is not connected"),
	E(WSAESHUTDOWN, "Cannot send after socket shutdown"),
	E (WSAETOOMANYREFS, "Too many references: cannot splice"),
	E(WSAETIMEDOUT, "Connection timed out"),
	E(WSAECONNREFUSED, "Connection refused"),
	E (WSAELOOP, "Too many symbolic links"),
	E (WSAENAMETOOLONG, "File name too long"),
	E(WSAEHOSTDOWN, "Host is down"),
	E(WSAEHOSTUNREACH, "No route to host"),
	E (WSAENOTEMPTY, "Directory not empty"),
	E(WSAEPROCLIM, "Too many processes"),
	E (WSAEUSERS, "Too many users"),
	E (WSAEDQUOT, "Quota exceeded"),
	E (WSAESTALE, "Stale file handle"),
	E (WSAEREMOTE, "Object is remote"),

	/* Yes, some of these start with WSA, not WSAE. No, I don't know why. */
	E(WSASYSNOTREADY, "Network subsystem is unavailable"),
	E(WSAVERNOTSUPPORTED, "Winsock.dll out of range"),
	E(WSANOTINITIALISED, "Successful WSAStartup not yet performed"),
	E(WSAEDISCON, "Graceful shutdown now in progress"),
	E(WSATYPE_NOT_FOUND, "Class type not found"),
	E(WSAHOST_NOT_FOUND, "Host not found"),
	E(WSATRY_AGAIN, "Nonauthoritative host not found"),
	E(WSANO_RECOVERY, "This is a nonrecoverable error"),
	E(WSANO_DATA, "Valid name, no data record of requested type)"),

	/* There are some more error codes whose numeric values are marked
	* <b>OS dependent</b>. They start with WSA_, apparently for the same
	* reason that practitioners of some craft traditions deliberately
	* introduce imperfections into their baskets and rugs "to allow the
	* evil spirits to escape."  If we catch them, then our binaries
	* might not report consistent results across versions of Windows.
	* Thus, I'm going to let them all fall through.
	*/

	//// winsock2
	E (WSAENOMORE, "No more?"),
	E (WSAECANCELLED, "Operation cacelled"),
	E (WSAEINVALIDPROCTABLE, "Invalid procedure table?"),
	E (WSAEINVALIDPROVIDER, "Invalid provider?"),
	E (WSAEPROVIDERFAILEDINIT, "Provider failed to initialize?"),
	E (WSASYSCALLFAILURE, "Failed system call?"),
	E (WSASERVICE_NOT_FOUND, "Unknown service?"),

	E (WSA_E_NO_MORE, "No more?"),
	E (WSA_E_CANCELLED, "Cancelled?"),
	E (WSAEREFUSED, "Refused?"),

	//// winsock2 QoS
	E (WSA_QOS_RECEIVERS, ""),
	E (WSA_QOS_SENDERS, ""),
	E (WSA_QOS_NO_SENDERS, ""),
	E (WSA_QOS_NO_RECEIVERS, ""),
	E (WSA_QOS_REQUEST_CONFIRMED, ""),
	E (WSA_QOS_ADMISSION_FAILURE, ""),
	E (WSA_QOS_POLICY_FAILURE, ""),
	E (WSA_QOS_BAD_STYLE, ""),
	E (WSA_QOS_BAD_OBJECT, ""),
	E (WSA_QOS_TRAFFIC_CTRL_ERROR, ""),
	E (WSA_QOS_GENERIC_ERROR, ""),
	E (WSA_QOS_ESERVICETYPE, ""),
	E (WSA_QOS_EFLOWSPEC, ""),
	E (WSA_QOS_EPROVSPECBUF, ""),
	E (WSA_QOS_EFILTERSTYLE, ""),
	E (WSA_QOS_EFILTERTYPE, ""),
	E (WSA_QOS_EFILTERCOUNT, ""),
	E (WSA_QOS_EOBJLENGTH, ""),
	E (WSA_QOS_EFLOWCOUNT, ""),
	E (WSA_QOS_EUNKNOWNPSOBJ, ""),
	E (WSA_QOS_EPOLICYOBJ, ""),
	E (WSA_QOS_EFLOWDESC, ""),
	E (WSA_QOS_EPSFLOWSPEC, ""),
	E (WSA_QOS_EPSFILTERSPEC, ""),
	E (WSA_QOS_ESDMODEOBJ, ""),
	E (WSA_QOS_ESHAPERATEOBJ, ""),
	E (WSA_QOS_RESERVED_PETYPE, ""),

	{ -1, NULL },
};
#undef E

constexpr const unsigned nerr = 45 + 10 + 7 + 27;
static_assert (sizeof (windows_socket_errors) ==
	  (nerr + 1) * sizeof (windows_socket_errors[0]), "size");

static std::map <int, const char *> errors;

static inline void cleanup (void) {WSACleanup();}

static void initialize (void)
{
	for (int i=0; windows_socket_errors[i].code >= 0; ++i)
	{
		const auto &we = windows_socket_errors[i];
		auto ins = errors.insert (std::make_pair (we.code, we.msg));
		assert (std::get <bool> (ins));
	}
	WSADATA wsa_data;
	WSAStartup (MAKEWORD(2, 2), &wsa_data);
	std::atexit (cleanup);
}

static bool initialize_once (void)
{
	static std::once_flag init_flag;
	std::call_once (init_flag, initialize);
	assert (83 < errors.size());
	assert (90 > errors.size());
	assert (errors.size() == nerr);
	return true;
}

static bool initialized = initialize_once();

}} // unnamed::detail

NETWORK_API int get_errno (void)
{
	return WSAGetLastError();
}
#endif // _WIN32

namespace { namespace detail {

class net_error_category : public std::error_category
{
public:

	const char *name() const noexcept override {return "network";}

	std::string message (int m) const override
	{
#ifdef _WIN32
		const auto we = errors.find (m);
		if (we != errors.end())
			return we->second;
		else
			return "Unknown failure (" + std::to_string (m) + ')';
#else
		return std::system_category().message (m);
#endif
	}
};

static net_error_category cat;

}} // unnamed::detail

const std::error_category &error_category() {return detail::cat;}

} // namespace network
