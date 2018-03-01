#ifndef SYS_SYSUNIX_HXX
#define SYS_SYSUNIX_HXX

#include <string>
#include <array>

#include <sys/dll.hxx>

namespace sys
{

SYS_API void revoke_privileges(const std::string &uname);

SYS_API void do_daemonize(void);

SYS_API void systemd_send_pid (void);

SYS_API void systemd_notify(const char *msg);

SYS_API std::array<int, 2u> systemd_init_descriptors();

SYS_API unsigned entropy();

} // namespace sys
#endif
