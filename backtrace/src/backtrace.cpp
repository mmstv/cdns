#include "backtrace/config.h"

#include <string>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <array>
#include <system_error>

#include <cstdlib>
#include <csignal>
#include <cstring>

#ifdef HAVE_UNISTD_H
#include <unistd.h> // write, STDERR_FILENO
#elif defined(_WIN32)
#include <io.h> // write
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif
#endif

#ifdef HAVE_CXXABI_H
#include <cxxabi.h> // Demangling
#endif

#ifdef HAVE_XMMINTRIN_H
#include <xmmintrin.h>
#endif

#if defined(_WIN32) && defined(HAVE_DBGHELP_LIB)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#include <cfloat> // for _controlfp
#endif

#if defined(__linux) || defined(__linux__) || defined(__gnu_linux__) \
	|| defined(linux) || defined(__FreeBSD__)
//! @todo what about MinGW?
#include <fenv.h>
#if !defined (__GNUC__)
//! @todo: what is this for?
#pragma STDC FENV_ACCESS ON
#endif
#define HAVE_FE_EXCEPT 1
#endif

#include "backtrace/backtrace.hxx"

namespace trace {

#ifdef HAVE_CXXABI_H
static void demangle(std::string & frame)
{
#if defined( __APPLE__ )
	// 0   module   0x000000010ed8a8a2 func + 50
	std::size_t naddr = frame.find("0x");
	if( naddr != std::string::npos )
	{
		naddr = frame.find(" ", naddr);
		if( naddr != std::string::npos )
		{
			naddr = frame.find_first_not_of(" ", naddr);
			if( naddr != std::string::npos )
			{
				std::size_t nend = frame.find_first_of(" +", naddr);
				if( nend==std::string::npos )
					return;
				std::string func = frame.substr(naddr, nend-naddr);
				std::string prefix = frame.substr(0, naddr);
				std::string suffix = frame.substr(nend);
				int status = -10;
				char *demangled=nullptr;
				demangled = abi::__cxa_demangle(func.c_str(), 0, 0, &status);
				if( status==0 && demangled!=nullptr )
				{
					frame = prefix;
					frame += demangled;
					frame += suffix;
				}
				if( demangled!=nullptr )
					free(demangled);
			}
		}
	}
#else
	// Linux
	// find parentheses and +address offset surrounding the mangled name:
	// ./module(function+0x15c) [0x8048a6d]
	std::size_t naddr = frame.find('(');
	if( naddr != std::string::npos )
	{
		++naddr;
		std::size_t nfend = frame.find('+',naddr);
		if( nfend == std::string::npos )
			return;
		std::string func = frame.substr(naddr,nfend-naddr);
		if( !func.empty() )
		{
			std::string prefix = frame.substr(0,naddr),
				suffix = frame.substr(nfend);
			int status = -10;
			char *demangled=NULL;
			demangled = abi::__cxa_demangle(func.c_str(), 0, 0, &status);
			if( 0==status && nullptr!=demangled )
			{
				frame = prefix;
				frame += demangled;
				frame += suffix;
			}
			if( nullptr!=demangled )
				std::free(demangled);
		}
	}
#endif
} // demangle
#endif

// from: http://stackoverflow.com/questions/5693192/win32-backtrace-from-c-code
// Needs /Oy- Visual Studio C flag to work, which can be disabled by optimization
struct bt
{
	std::array<void*, 25> bt_;
	unsigned short n;
#if defined(_WIN32) && defined(HAVE_DBGHELP_LIB) && !defined(HAVE_BACKTRACE)
	HANDLE process_;
	static std::mutex mutex_;
	static bool initialized_;
#endif

	bt()
	{
#ifdef HAVE_BACKTRACE
		const int nn = backtrace (bt_.data(), static_cast <int> (bt_.size()));
		this->n = (nn<0 || nn>1000) ? 0u : static_cast <unsigned short> (nn);
#elif defined(_WIN32) && defined(HAVE_DBGHELP_LIB)
		// process_ = ::GetCurrentProcess();
		// SymInitialize(process_, nullptr, TRUE );
		const ULONG frames_to_skip =0,
			  frames_to_capture = static_cast<DWORD>(bt_.size()-1);
		this->n = ::CaptureStackBackTrace(frames_to_skip, frames_to_capture,
			bt_.data(), nullptr);
#else
#  error "Can't create backtrace"
#endif
	}

#if defined(_WIN32) && defined(HAVE_DBGHELP_LIB)
	void write_symbol(const SYMBOL_INFO &s, unsigned i, int fd) const
	{
		char buf[512]="";
		buf[0] = '\0';
		std::snprintf(buf, 500, "%d : %s - %p\n", (n-i-1), s.Name,
			reinterpret_cast<const void* >(s.Address));
		_write(fd, buf, std::min(500U,static_cast<unsigned>(std::strlen(buf))));
	}
	void write_symbol(const SYMBOL_INFO &s, unsigned i, std::ostream &stream) const
	{
		stream << (n-i-1) << ": " << s.Name << " - "
			<< reinterpret_cast<const void* >(s.Address)
			<< " == " << bt_.at(i)
			<< '\n';
	}

	template<typename Output> void write_(Output out)
	{
		process_ = ::GetCurrentProcess();
		std::lock_guard<std::mutex> lock_sym(this->mutex_);
		SetLastError(0);
		if(!initialized_)
		{
			int rs = SymInitialize(process_, nullptr, TRUE);
			if(FALSE==rs)
			{
				std::cerr << "WinERR, sym init failed" << std::endl;
				DWORD dw = GetLastError();
				TCHAR msgbuf[1024];
				msgbuf[0]='\0';
				FormatMessage(
					FORMAT_MESSAGE_FROM_SYSTEM
					| FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					msgbuf,
					200, NULL);
				std::cerr << "WinERR: " << msgbuf << std::endl;
				std::_Exit(99);
			}
			initialized_ = true;
		}
		if (0u != this->n)
		{
			constexpr const unsigned nlen = 255;
			struct sym_info
			{
				SYMBOL_INFO  sym;
				TCHAR name[nlen+1];
			} symbol;
			static_assert(sizeof(symbol) > (sizeof(SYMBOL_INFO)+200*sizeof(TCHAR)),
				"size");
			std::memset(&symbol, 0, sizeof(symbol));
			symbol.sym.MaxNameLen = nlen-10;
			symbol.sym.SizeOfStruct = sizeof(SYMBOL_INFO);
			for(unsigned i = 0; i < n && i<bt_.size(); ++i)
			{
				SetLastError(0);
				int rs = SymFromAddr(process_, reinterpret_cast<DWORD64>(bt_[i]), 0,
					&symbol.sym);
				if(FALSE==rs)
				{
					DWORD dw = GetLastError();
					TCHAR msgbuf[1024];
					msgbuf[0]='\0';
					FormatMessage(
						FORMAT_MESSAGE_FROM_SYSTEM
						| FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
						msgbuf,
						200, NULL);
					strncpy_s(symbol.sym.Name, 200, msgbuf, 190);
					symbol.sym.Address = 0;
				}
				this->write_symbol (symbol.sym, i, out);
			}
		}
	}
#endif

	void symbols_fd(int fd = STDERR_FILENO)
	{
#ifdef HAVE_BACKTRACE
		// Link with -rdyanmic on Linux to make function names available
		// See `man 3 backtrace`
		// Demangle names using standalone bundled in usr/bin/demangle tool
		backtrace_symbols_fd (bt_.data(), this->n, fd);
#elif defined(_WIN32) && defined(HAVE_DBGHELP_LIB)
		this->write_<int>(fd);
#else
#  error "Can't create backtrace"
#endif
	}


	void write(std::ostream &stream)
	{
#ifdef HAVE_BACKTRACE
		char ** symbols = backtrace_symbols (bt_.data(), n);
		stream << "\nCall stack backtrace:\n";
		const int stack_start = 1;
		for(int i = stack_start; i < n; i++)
		{
			std::string frame = symbols[i];
			demangle(frame);
			stream << '\t' << frame << "\n";
		}
		if( nullptr!=symbols )
			std::free(symbols);

#elif defined(_WIN32) && defined(HAVE_DBGHELP_LIB)
		this->write_<std::ostream & >(stream);
#else
#  error "Can't create backtrace"
#endif
	}

}; // class bt

#if defined(_WIN32) && defined(HAVE_DBGHELP_LIB) && !defined(HAVE_BACKTRACE)
std::mutex bt::mutex_;
bool bt::initialized_ = false;
#endif

void print_backtrace(std::ostream &stream)
{
#if defined(HAVE_BACKTRACE) || (defined(_WIN32) && defined(HAVE_DBGHELP_LIB))
	bt b;
	b.write(stream);
#else
	ostr << "NO BACKTRACE! DbgHelp.lib not available" << std::endl;
#endif
}

std::string add_backtrace_string(const std::string& msg)
{
	std::string result;
	try {
		std::ostringstream stream;
		stream << msg << '\n';
		print_backtrace(stream);
		result = stream.str();
	}
	catch( ... )
	{
		std::terminate();
	}
	return result;
}


/*!  Speeds up computation with denormal floating point numbers, i.e.
 * very small numbers, by flushing them to zero. May affect accuracy.
 */
bool set_flush_denormals_to_zero()
{
	// http://stackoverflow.com/questions/9314534/
	// why-does-changing-0-1f-to-0-slow-down-performance-by-10x
#ifdef HAVE_XMMINTRIN_H
	// speeds up computations with very small numbers, making them inaccurate
	_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
	// needs pmmintrin.h ?
	// _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
#endif
#ifdef _WIN32
	// http://habrahabr.ru/post/267155/
	// improve malloc performance int multithreaded apps
	// Low Fragmentation Heap, enabled by default since Vista ?
	// https://msdn.microsoft.com/en-us/library/aa366750.aspx
	// https://github.com/shines77/jemalloc-win32
	// ULONG HeapInformation = 2;  // HEAP_LFH;
	// HeapSetInformation(GetProcessHeap(),  HeapCompatibilityInformation,
	//   &HeapInformation, sizeof(HeapInformation));
#endif
	return true;
}


#ifndef _WIN32
#define POSIX_SIGNALS 1
#endif

extern "C" void backtrace_on_signal(int sig
#ifdef POSIX_SIGNALS
	, siginfo_t *, void *
#endif
)
{
	static volatile sig_atomic_t segv_count = 0;
	int sc = segv_count++;
	if( 0!=sc )
	{
		// ignore all signals but first
		if(SIGABRT!=sig)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			std::abort();
		}
	}
	else
	{
		// only print backtrace for the first thread raising signal
		bt b;
		int ret = 100;
		const char *ssig = "SIGNAL ERROR! (UNKN)\n";
		switch(sig)
		{
			case SIGSEGV:
				ret = 200;
				ssig = "\nSIGNAL ERROR! (SEGV)\n";
				break;
#ifndef _WIN32
			case SIGBUS:
				ret = 300;
				ssig = "\nSIGNAL ERROR! (BUS )\n";
				break;
#endif
			case SIGILL:
				ret = 400;
				ssig = "\nSIGNAL ERROR! (ILL )\n";
				break;
			case SIGFPE:
				ret = 500;
				ssig = "\nSIGNAL ERROR! (FPE )\n";
				break;
			case SIGABRT:
				ssig = "\nSIGNAL ERROR! (ABRT)\n";
		}
		const auto r =
#ifdef _WIN32
			::_write
#else
			::write
#endif
				(STDERR_FILENO, ssig, 22);
		if (0 <= r)
			// Symbols will not be demangled here to avoid malloc in signal
			// processing. Use bundled `demangle` tool.
			b.symbols_fd (STDERR_FILENO);
		// do not use std::exit here, it may call std::atexit registered functions
		// and deadlock
		if (SIGABRT != sig)
			std::_Exit (ret / 100 + b.n * 10);
	}
}


static void on_terminate()
{
	static std::mutex cout_mutex;
	{
		std::lock_guard<std::mutex> lock_cout(cout_mutex);
		std::flush(std::cerr);
		std::cerr << "\n\nFATAL ERROR(termination)! "
			<< " thread: " << std::this_thread::get_id() << "\n";
		print_backtrace(std::cerr);
		std::cerr << std::endl;
	}
	std::abort();
}

void catch_system_signals (void)
{
	std::set_terminate (on_terminate);
	constexpr const int signals[] = {SIGFPE, SIGSEGV, SIGILL, SIGABRT
#if defined (SIGBUS) && !defined (_WIN32)
		, SIGBUS
#endif
	};

#ifdef POSIX_SIGNALS
	struct sigaction act;
	::sigemptyset (&act.sa_mask);
	act.sa_flags = static_cast <int> (SA_NODEFER | SA_ONSTACK | SA_RESETHAND
		| SA_SIGINFO);
	act.sa_sigaction = backtrace_on_signal;
#endif

	for (auto s : signals)
	{
#ifdef POSIX_SIGNALS
		if (0 != ::sigaction (s, const_cast <const struct sigaction*> (&act),
			nullptr))
#else
		if (SIG_ERR == std::signal (s, backtrace_on_signal))
#endif
			// In MinGW both errno and GetLastError might be 0 here
			throw std::system_error (errno, std::system_category(), "Failed to set "
				"signal(" + std::to_string (s) + ") handler: ");
	}
}

/*! _EM_INVALID or _MM_MASK_INVALID flags are supposed to catch errors
 * such as sqrt(-1.). But they also raise 'inexact exception' on sqrt(0.) when
 * -ffast-math is enabled with Clang-3.8 and AppleClang-7.3. See implementation
 * details at: http://www.fortran-2000.com/ArnaudRecipes/CompilerTricks.html
 * and: boost/test/impl/execution_monitor.impl
 * All FP exceptions: INVALID DIV_ZERO OVERFLOW UNDERFLOW DENORM INEXACT
 */
bool enable_fp_exceptions()
{
	catch_system_signals();

#if defined(__FAST_MATH__) && defined(__clang__)
#if defined(__APPLE_CC__) && (100*(__clang_major__)+(__clang_minor__)) >= 703
		/* Mac OSX 10.11 El Capitan clang-7.3.0 with -ffast-math raise inexact
		 * floating point exception with FPE exceptions turned on, when computing
		 * sqrt(0.)
		 */
#error "OSX 10.11 clang-7.3.0 -ffast-math and FPE exceptions fail with sqrt(0)"
#elif !defined(__APPLE_CC__) && (100*(__clang_major__)+(__clang_minor__)) >= 308
		/* Linux clang-3.8 with -ffast-math raise inexact
		 * floating point exception with FPE exceptions turned on, when computing
		 * sqrt(0.)
		 */
#error "Linux clang-3.8 -ffast-math and FPE exceptions fail with sqrt(0)"
#endif
#endif
#ifdef _WIN32
	// _controlfp affects both x87 and SSE/SSE2 ?
	// http://www.fortran-2000.com/ArnaudRecipes/CompilerTricks.html
	unsigned int cw=0; // = _controlfp(0,0) & MCW_EM;
	_controlfp_s(&cw, 0, 0);
	cw = cw & _MCW_EM;
	constexpr const unsigned cfl = (_EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW
		| _EM_UNDERFLOW);
	cw &= ~cfl;
	// _EM_INEXACT : causes too many failures in standard library
	unsigned int unused=0;
	_controlfp_s(&unused, cw, _MCW_EM);
#endif
#ifdef HAVE_FE_EXCEPT
	// feenableexcept affects both x87 and SSE ?
	feenableexcept(FE_INVALID|FE_DIVBYZERO|FE_OVERFLOW|FE_UNDERFLOW);
	// |__FE_DENORM);
#endif
#if defined(HAVE_XMMINTRIN_H)
	// SSE exceptions
	constexpr unsigned except =
		  (~static_cast<unsigned>(_MM_MASK_INVALID))
		& (~static_cast<unsigned>(_MM_MASK_DIV_ZERO))
		& (~static_cast<unsigned>(_MM_MASK_OVERFLOW))
		& (~static_cast<unsigned>(_MM_MASK_UNDERFLOW));
		// & (~static_cast<unsigned>(_MM_MASK_DENORM));
		// & (~_MM_MASK_INEXACT) this causes failure with: cout << 0.5 << endl;
	_MM_SET_EXCEPTION_MASK(_MM_GET_EXCEPTION_MASK() & except);
	return true;
#else
	return false;
#endif
}

} // namespace trace
