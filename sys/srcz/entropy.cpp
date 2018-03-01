#include "sys/sysunix.hxx"

#include "sys/preconfig.h"

#if defined (__linux__) && defined (HAVE_LINUX_RANDOM_H)

#ifndef HAVE_UNISTD_H
#  error "need close"
#endif

#include <unistd.h> // for close

#if !defined (HAVE_SYS_IOCTL_H) || !defined (HAVE_SYS_STAT_H) || !defined \
	(HAVE_FCNTL_H)
#  error "need ioctl, and open"
#endif
#include <sys/ioctl.h>

#include <sys/stat.h> // open
#include <fcntl.h>
#include <linux/random.h>

#if !defined (RNDGETENTCNT)
#  error "rndgetentcnt"
#endif

namespace sys {

unsigned entropy()
{
	int c = 0;
	const int fd = ::open ("/dev/random", O_RDONLY);
	if (-1 != fd)
	{
		if (0 != ::ioctl (fd, RNDGETENTCNT, &c))
			c = 0;
		if (0 > c)
			c = 0;
		::close (fd);
	}
	return static_cast <unsigned> (c);
}

}
#else

#include <random>
namespace sys {

unsigned entropy()
{
	unsigned ee = 0;
	try
	{
		// random_defice.entropy() returns 0. on Linux, which means it is not a
		// random device, but a pseudo-random generator
		std::random_device rnd;
		const double e = rnd.entropy();
		if (0.5 <= e && e < std::numeric_limits <unsigned>::max())
			ee = static_cast <unsigned> (std::lrint (e));
	}
	catch (const std::runtime_error &)
	{
		ee = 0;
	}
	return ee;
}

} // namespace sys
#endif

