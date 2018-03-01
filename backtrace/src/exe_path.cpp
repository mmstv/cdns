#include <string>
#include <stdexcept>
#include <climits>
#include <cstdint>

#include "backtrace/backtrace.hxx"

/*
http://stackoverflow.com/questions/1023306/
		finding-current-executables-path-without-proc-self-exe
Mac OS X: _NSGetExecutablePath() (man 3 dyld)
Linux: readlink /proc/self/exe
Solaris: getexecname()
FreeBSD: sysctl CTL_KERN KERN_PROC KERN_PROC_PATHNAME -1
FreeBSD if it has procfs: readlink /proc/curproc/file (FreeBSD doesn't have
	procfs by default)
NetBSD: readlink /proc/curproc/exe
DragonFly BSD: readlink /proc/curproc/file
Windows: GetModuleFileName() with hModule = NULL
*/

#ifdef _WIN32

#include <windows.h>
namespace trace {

std::string this_executable_path()
{
	char path[MAX_PATH+1]="";
	::GetModuleFileName(NULL, path, MAX_PATH);
	return std::string(path);
}

} // namespace trace

#elif defined(__linux__)||defined(__linux)||defined(linux)||defined(__gnu_linux__)

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

namespace trace {

std::string this_executable_path()
{
	char dest[PATH_MAX+1]="";
	if (readlink("/proc/self/exe", dest, PATH_MAX) == -1 || ('\0'==dest[0]) )
		throw std::runtime_error("readlink failed");
	//! @todo: realpath()
	return std::string(dest);
}

}

#elif defined(__APPLE__)

// iOS ?

#include <sys/param.h>
#include <mach-o/dyld.h>

namespace trace {

std::string this_executable_path()
{
	std::uint32_t bufsize = 0;
	{
		char buf[100];
		_NSGetExecutablePath(buf, &bufsize);
	}
	if(0==bufsize)
		throw std::logic_error("exe path failed");
	std::unique_ptr<char[]> buf(new char[bufsize+2]);
	if(0!=_NSGetExecutablePath(buf.get(), &bufsize))
		throw std::runtime_error("exe path failed");
	return std::string(buf.get());
}

}

#elif defined(__FreeBSD__)

#include <sys/types.h>
#include <sys/sysctl.h>

namespace trace {

std::string this_executable_path()
{
	char buf[1024];
	{
		int mib[4];
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = KERN_PROC_PATHNAME;
		mib[3] = -1;
		size_t cb = sizeof(buf);
		sysctl(mib, 4, buf, &cb, NULL, 0);
		//! @todo: realpath()
	}
	return std::string(buf);
}

}

#else

// see comments at the top of this file
#error "unsupported operating system, for executable path determination"

#endif
