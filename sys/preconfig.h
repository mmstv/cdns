#ifndef _SYS_PRECOFNIG_H_
#define _SYS_PRECOFNIG_H_

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#elif defined (_WIN32)
#include "win_conf.h"
#elif defined (__linux__)
#include "linux_conf.h"
#else
#error "Unsupported system"
#endif

#endif
