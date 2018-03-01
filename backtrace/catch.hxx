#ifndef BACKTRACE_CATCH_HXX
#define BACKTRACE_CATCH_HXX

#include "backtrace/dll.hxx"

namespace trace {

typedef int (*main_t)(int argc, char *argv[]);
typedef int (*main0_t)(void);
typedef void (*void_main0_t)(void);
typedef void (*void_main_t)(int argc, char *argv[]);

//! Catches and prints specific messages to std::cerr for most exceptions
int BACKTRACE_API catch_all_errors(main_t main_func, int argc, char *argv[],
	bool enable_fp_exceptions=true);

int BACKTRACE_API catch_all_errors(main0_t main_func, bool enalbe_fp_exceptions=true);

int BACKTRACE_API catch_all_errors(void_main0_t main_func, bool fp=true);

int BACKTRACE_API catch_all_errors(void_main_t main_func, int argc, char *argv[],
	bool enable_fp_exceptions=true);

}
#endif
