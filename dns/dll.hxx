#ifndef _DNS_API_H
#define _DNS_API_H

#include "sys/dllexport.hxx"

#ifdef DNS_STATIC_DEFINE
#  define DNS_API
#  define DNS_NO_EXPORT
#else
#  ifdef DNS_DLL_EXPORTS
#    define DNS_API SYS_SYMBOL_EXPORT
#  else
#    define DNS_API SYS_SYMBOL_IMPORT
#  endif
#  define DNS_NO_EXPORT SYS_SYMBOL_NO_EXPORT
#endif

#endif
