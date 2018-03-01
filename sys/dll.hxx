#ifndef SYS_API_H
#define SYS_API_H

#include "sys/dllexport.hxx"

#ifdef SYS_STATIC_DEFINE
#  define SYS_API
#  define SYS_NO_EXPORT
#else
#  ifdef SYS_DLL_EXPORTS
#    define SYS_API SYS_SYMBOL_EXPORT
#  else
#    define SYS_API SYS_SYMBOL_IMPORT
#  endif
#  define SYS_NO_EXPORT SYS_SYMBOL_NO_EXPORT
#endif

#endif
