/* $Id: miniupnpcstrings.h.in,v 1.4 2011/01/04 11:41:53 nanard Exp $ */
/* Project: miniupnp
 * http://miniupnp.free.fr/ or http://miniupnp.tuxfamily.org/
 * Author: Thomas Bernard
 * Copyright (c) 2005-2011 Thomas Bernard
 * This software is subjects to the conditions detailed
 * in the LICENCE file provided within this distribution */
#ifndef __MINIUPNPCSTRINGS_H__
#define __MINIUPNPCSTRINGS_H__

#ifdef _WIN32
#define OS_STRING "MSWindows/6.1.7601"
#define MINIUPNPC_VERSION_STRING "1.9"

#if 0
/* according to "UPnP Device Architecture 1.0" */
#define UPNP_VERSION_STRING "UPnP/1.0"
#else
/* according to "UPnP Device Architecture 1.1" */
#define UPNP_VERSION_STRING "UPnP/1.1"
#endif

#else
#error TODO gen from the makefile
#endif

#endif

