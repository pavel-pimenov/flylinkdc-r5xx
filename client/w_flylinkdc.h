#pragma once

#ifndef DCPLUSPLUS_DCPP_W_FLYLINKDC_H
#define DCPLUSPLUS_DCPP_W_FLYLINKDC_H

#include "version.h"

// https://msdn.microsoft.com/ru-ru/library/windows/desktop/aa383745%28v=vs.85%29.aspx
#if defined(_M_ARM) || defined (_M_ARM64)
# define _WIN32_WINNT _WIN32_WINNT_WINBLUE
#else
#ifndef _WIN32_WINNT
# ifdef FLYLINKDC_SUPPORT_WIN_XP
#  define _WIN32_WINNT _WIN32_WINNT_WINXP
# elif FLYLINKDC_SUPPORT_WIN_VISTA
#  define _WIN32_WINNT _WIN32_WINNT_VISTA
# else // Win7+
#  define _WIN32_WINNT _WIN32_WINNT_WIN7
# endif
#endif
#endif // _WIN32_WINNT

#ifndef _WIN32_IE
# ifdef FLYLINKDC_SUPPORT_WIN_XP
#  define _WIN32_IE _WIN32_IE_IE80
# endif
#endif // _WIN32_IE

#endif // DCPLUSPLUS_DCPP_COMPILER_FLYLINKDC_H
