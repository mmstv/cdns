#ifndef _NETWORK_PRECONFIG_H_
#define _NETWORK_PRECONFIG_H_

#ifdef HAVE_CONFIG_H
#include "network/net_config.h"
#elif defined (_WIN32)
#include "network/win_netconf.h"
#elif defined (__linux__)
#include "network/linux_netconf.h"
#else
#error "Unsupported system"
#endif

#endif
