#ifndef _BACKTRACE_API_H
#define _BACKTRACE_API_H

#include "sys/dllexport.hxx"

#ifdef BACKTRACE_STATIC_DEFINE
#  define BACKTRACE_API
#  define BACKTRACE_NO_EXPORT
#else
#  ifdef BACKTRACE_DLL_EXPORTS
#    define BACKTRACE_API SYS_SYMBOL_EXPORT
#  else
#    define BACKTRACE_API SYS_SYMBOL_IMPORT
#  endif
#  define BACKTRACE_NO_EXPORT SYS_SYMBOL_NO_EXPORT
#endif

#endif
