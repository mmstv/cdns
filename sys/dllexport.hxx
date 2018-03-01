#if !(defined (_SYS_DLLEXPORT_HXX_) || defined (SYS_SYMBOL_IMPORT))
#define _SYS_DLLEXPORT_HXX_

// See boost/config

#if defined (_WIN32) || defined (WIN32) || defined (_MSC_VER)
#   define SYS_SYMBOL_EXPORT __declspec (dllexport)
#   define SYS_SYMBOL_IMPORT __declspec (dllimport)
#   define SYS_SYMBOL_NO_EXPORT
#elif defined (__INTEL_COMPILER) || defined (__ICL) || defined (__ICC) \
	|| defined (__ECC) || defined (__clang__) || defined (__GNUC__)
#   define SYS_SYMBOL_EXPORT __attribute__ ((__visibility__ ("default")))
#   define SYS_SYMBOL_IMPORT
#   define SYS_SYMBOL_NO_EXPORT __attribute__ ((visibility ("hidden")))
#else
#   define SYS_SYMBOL_EXPORT
#   define SYS_SYMBOL_IMPORT
#   define SYS_SYMBOL_NO_EXPORT
#endif

#endif // _SYS_DLLEXPORT_HXX_
