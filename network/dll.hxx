#ifndef _NETWORK_API_H
#define _NETWORK_API_H

#include "sys/dllexport.hxx"

#ifdef NETWORK_STATIC_DEFINE
#  define NETWORK_API
#  define NETWORK_NO_EXPORT
#else
#  ifdef NETWORK_DLL_EXPORTS
#    define NETWORK_API SYS_SYMBOL_EXPORT
#  else
#    define NETWORK_API SYS_SYMBOL_IMPORT
#  endif
#  define NETWORK_NO_EXPORT SYS_SYMBOL_NO_EXPORT
#endif

#endif
