#ifndef _DNS_CRYPT_API_H
#define _DNS_CRYPT_API_H

#include "sys/dllexport.hxx"

#ifdef DNSCRYPT_STATIC_DEFINE
#  define DNS_CRYPT_API
#  define DNS_CRYPT_NO_EXPORT
#else
#  ifdef DNSCRYPT_DLL_EXPORTS
#    define DNS_CRYPT_API SYS_SYMBOL_EXPORT
#  else
#    define DNS_CRYPT_API SYS_SYMBOL_IMPORT
#  endif
#  define DNS_CRYPT_NO_EXPORT SYS_SYMBOL_NO_EXPORT
#endif

#endif
