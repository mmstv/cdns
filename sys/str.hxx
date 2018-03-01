#ifndef _SYS_STR_HXX
#define _SYS_STR_HXX

#include <sys/dll.hxx>

#include <string>

namespace sys
{

SYS_API void ascii_tolower (char *);

//! @todo use C locale
SYS_API int ascii_strcasecmp(const char *s1, const char *s2);

SYS_API std::string ascii_tolower_copy (const char *);

SYS_API void ascii_tolower (std::string &);

SYS_API std::string ascii_tolower_copy (const std::string &);

SYS_API std::string ascii_tolower_copy (std::string &&);

} // namespace sys
#endif
