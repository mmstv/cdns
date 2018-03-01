#include <stdexcept>
#include <string>
#include <cstring>
#include <cerrno>
#include <system_error>
#include <cassert>
#include <limits>

#include "sys/preconfig.h"

#ifndef HAVE_GETPWNAM
#error "I want getpwnam"
#endif

#ifndef HAVE_INITGROUPS
#error "I want initgroups"
#endif

#ifdef _WIN32
#error "This is for UNIX/Linux only"
#endif

#if !defined (HAVE_UNISTD_H) || !defined (HAVE_PWD_H) || !defined (HAVE_GRP_H) \
	|| !defined (HAVE_SYS_TYPES_H)
#error "need this stuff"
#endif

#include <sys/types.h> // open, getpwnam, initgroups
#include <fcntl.h> // open
#include <pwd.h> // getpwnam
// chdir, chroot, setgid, setuid, close, fork, dup2, setsid, isatty, sysconf
#include <unistd.h>

#include <grp.h> // initgroups

#ifdef HAVE_PATHS_H
# include <paths.h> // for _PATH_DEVNULL
#endif

#ifndef _PATH_DEVNULL
# define _PATH_DEVNULL "/dev/null"
#endif

#ifdef HAVE_LIBSYSTEMD
//! @todo HAVE_SYS_SOCKET ? or move into network/
# include <sys/socket.h> // for AF_INET
# include <systemd/sd-daemon.h>
#endif

#include "sys/sysunix.hxx"
#include "sys/logger.hxx"

namespace sys
{

///// systemd stuff

void systemd_send_pid (void)
{
#if defined( HAVE_LIBSYSTEMD )
	sd_notifyf(0, "MAINPID=%lu", static_cast<unsigned long> (getpid()));
#endif
}

void systemd_notify(const char * const msg)
{
#ifdef HAVE_LIBSYSTEMD
	const int err = sd_notify(0, msg);

	if (err < 0)
		process::log::debug ("sd_notify failed: ", std::strerror(-err));
#endif
}

std::array<int, 2u> systemd_init_descriptors()
{
#ifdef HAVE_LIBSYSTEMD
	const int num_sd_fds = sd_listen_fds(0);
	if (0 == num_sd_fds)
		return std::array<int,2u>{{-1, -1}}; // success, not using systemd sockets
	//! @todo create systemd error category
	if( 0 > num_sd_fds )
		throw std::system_error (-num_sd_fds, std::generic_category());
	if (2 != num_sd_fds)
		throw std::runtime_error ("Wrong number of systemd sockets: "
			+ std::to_string (num_sd_fds) + " - should be 2");
	static_assert (2 <= std::numeric_limits <int>::max() - SD_LISTEN_FDS_START, "");
	int udp_socket = -1, tcp_socket = -1;
	for (int sock = SD_LISTEN_FDS_START; sock < SD_LISTEN_FDS_START + num_sd_fds;
		sock++)
	{
		assert (0 <= sock);
		//! @todo handle negative returns as errors
		if (sd_is_socket(sock, AF_INET, SOCK_DGRAM, 0) > 0 ||
			sd_is_socket(sock, AF_INET6, SOCK_DGRAM, 0) > 0)
			udp_socket = sock;
		if (sd_is_socket(sock, AF_INET, SOCK_STREAM, 1) > 0 ||
			sd_is_socket(sock, AF_INET6, SOCK_STREAM, 1) > 0)
			tcp_socket = sock;
	}
	if (udp_socket < 0)
		throw std::runtime_error ("No systemd UDP socket passed in");
	if (tcp_socket < 0)
		throw std::runtime_error ("No systemd TCP socket passed in");
	return std::array<int,2u>{{udp_socket, tcp_socket}};
#else
	return std::array<int,2u>{{-1, -1}};
#endif
}


struct user
{
	std::string name;
	uid_t id;
	gid_t group_id;
	std::string dir;
};

user get_user(const std::string &uname)
{
	if (uname.empty())
		throw std::logic_error("User name is blank");
	errno = 0; // see `man 3 getpwnam`
	const struct passwd *const pw = ::getpwnam(uname.c_str());
	if (nullptr == pw)
	{
		if (0 != errno)
			throw std::system_error (errno, std::system_category(), "Bad user: "
				+ uname);
		else
			throw std::runtime_error ("Unknown user: " + uname);
	}
	user result;
	result.name = pw->pw_name;
	result.id = pw->pw_uid;
	result.group_id = pw->pw_gid;
	result.dir = pw->pw_dir;
	return result;
}

void revoke_privileges(const std::string &uname)
{
	const user u = get_user (uname);
	if (static_cast <uid_t> (0) != u.id)
	{
		if (::initgroups(u.name.c_str(), u.group_id) != 0)
			throw std::system_error(errno, std::system_category(), "Unable to "
				"initialize groups for: " + u.name);
		// @TODO: change group as well?
		if (::setuid (u.id) != 0 || ::seteuid (u.id) != 0)
			throw std::system_error (errno, std::system_category(), "Can't switch"
				" user to: " + std::to_string (u.id));
	}
}

unsigned int open_max(void)
{
	//! @todo what is this?
	const long z = ::sysconf (_SC_OPEN_MAX);
	if (0L > z)
	{
		process::log::error ("_SC_OPEN_MAX: ", std::strerror (errno));
		return 2U;
	}
	return static_cast <unsigned int> (z);
}

void closedesc_all()
{
	(void) ::close(0);
	int fodder = ::open(_PATH_DEVNULL, O_RDONLY);
	if (-1 == fodder)
		throw std::system_error (errno, std::system_category(), _PATH_DEVNULL
			" duplication");
	(void) ::dup2(fodder, 0);
	if (fodder > 0)
		(void) ::close(fodder);
	fodder = ::open(_PATH_DEVNULL, O_WRONLY);
	if (-1 == fodder)
		throw std::system_error (errno, std::system_category(),
			_PATH_DEVNULL " duplication");
	(void) ::dup2(fodder, 1);
	(void) ::dup2(1, 2);
	if (fodder > 2)
		(void) ::close(fodder);
}

void do_daemonize (void)
{
	const pid_t child = ::fork();
	if (static_cast <pid_t> (-1) == child)
		throw std::system_error (errno, std::system_category(),
			"Unable to fork() in order to daemonize");
	else if (static_cast<pid_t> (0) != child)
		_exit(0); //! @todo std::Exit
	if (::setsid() == static_cast <pid_t> (-1))
		throw std::system_error (errno, std::system_category(),
			"Unable to setsid()");
	//! @todo what is this for?
	unsigned int i = open_max();
	do
	{
		if (::isatty( static_cast <int> (i)))
			(void) close(static_cast <int> (i));
		i--;
	}
	while (i > 2U);
	closedesc_all();
}

} // namespace sys
