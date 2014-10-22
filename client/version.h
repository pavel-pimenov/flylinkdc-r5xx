/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#ifndef FLY_VERSION_H
#define FLY_VERSION_H

#pragma once

#include "../revision.h"  // call UpdateRevision.bat (fatal error C1083: Cannot open include file: 'revision.h': No such file or directory)

#ifdef _WIN64
#define A_POSTFIX64 "-x64"
#define L_POSTFIX64 L"-x64"
#else
#define A_POSTFIX64 ""
#define L_POSTFIX64 L""
#endif

//#define FLYLINKDC_HE 1 // Enable to use FlylinkDC-Hedgehog edition version of FlylinkDC (a.rainman on gmail pt com). When compiling the release you just need to use the configuration ReleaseHE.

//#define NIGHT_BUILD 1 // Enable to use fail-save, and potential unstable features.

// [!] FlylinkDC++ compilation note:
// To enable FLYLINKDC_SUPPORT_WIN_2000 please use the compiler that supports C++11 features,
// and at the same compiles the code for Windows 2000,
// in addition to this you have to add support for the compiler to a "compiler.h".
// P.s: please send this patch to FlylinkDC++ team https://code.google.com/p/flylinkdc/

#ifdef FLYLINKDC_HE

# ifndef _WIN64
#  define FLYLINKDC_SUPPORT_WIN_VISTA 1
#  ifdef FLYLINKDC_SUPPORT_WIN_VISTA
#   define FLYLINKDC_SUPPORT_WIN_XP 1
#   ifdef FLYLINKDC_SUPPORT_WIN_XP
//#   define FLYLINKDC_SUPPORT_WIN_2000 1
#   endif // FLYLINKDC_SUPPORT_WIN_XP
#  endif // FLYLINKDC_SUPPORT_WIN_VISTA
# endif // _WIN64

#else // FLYLINKDC_HE

# define FLYLINKDC_SUPPORT_WIN_VISTA 1
# ifdef FLYLINKDC_SUPPORT_WIN_VISTA
#  define FLYLINKDC_SUPPORT_WIN_XP 1
#  ifdef FLYLINKDC_SUPPORT_WIN_XP
//#   define FLYLINKDC_SUPPORT_WIN_2000 1
#  endif // FLYLINKDC_SUPPORT_WIN_XP
# endif // FLYLINKDC_SUPPORT_WIN_VISTA

#endif // FLYLINKDC_HE

#define APPNAME "FlylinkDC++"
#define PPA_INCLUDE_SVN_REVISION

// Macros utils
#define VER_STRINGIZE1(a) #a
#define VER_STRINGIZE(a) VER_STRINGIZE1(a)
#define VER_LONGIZE1(a) L##a
#define VER_LONGIZE(a) VER_LONGIZE1(a)

#define REV_STRINGIZE1(a) #a
#define REV_STRINGIZE(a) REV_STRINGIZE1(a)
#define REV_LONGIZE1(a) L##a
#define REV_LONGIZE(a) REV_LONGIZE1(a)

// Auto definitions
#define A_REVISION_NUM_STR  REV_STRINGIZE(REVISION_NUM)
#define L_REVISION_NUM_STR  REV_LONGIZE(A_REVISION_NUM_STR)

#ifdef FLYLINKDC_BETA
# define A_BETA_NUM_STR VER_STRINGIZE(BETA_NUM)
# define L_BETA_NUM_STR  VER_LONGIZE(A_BETA_NUM_STR)
#endif
#define A_VERSION_NUM_STR  VER_STRINGIZE(VERSION_NUM)
#define L_VERSION_NUM_STR  VER_LONGIZE(A_VERSION_NUM_STR)

#ifdef FLYLINKDC_BETA
# define L_SHORT_VERSIONSTRING L"r" L_VERSION_NUM_STR L"-rc" L_BETA_NUM_STR L_POSTFIX64
# define A_SHORT_VERSIONSTRING  "r" A_VERSION_NUM_STR  "-rc" A_BETA_NUM_STR A_POSTFIX64
#else
# define L_SHORT_VERSIONSTRING L"r" L_VERSION_NUM_STR   L_POSTFIX64
# define A_SHORT_VERSIONSTRING  "r" A_VERSION_NUM_STR   A_POSTFIX64
#endif

#define L_VERSIONSTRING L_SHORT_VERSIONSTRING L" build " L_REVISION_NUM_STR
#define A_VERSIONSTRING A_SHORT_VERSIONSTRING  " build " A_REVISION_NUM_STR

#ifdef UNICODE
# define T_VERSIONSTRING L_VERSIONSTRING
# define T_REVISION_NUM_STR L_REVISION_NUM_STR
#else
# define T_VERSIONSTRING A_VERSIONSTRING
# define T_REVISION_NUM_STR A_REVISION_NUM_STR
#endif
// [+] IRainman: copy-past fix.
#define T_APPNAME_WITH_VERSION _T(APPNAME) _T(" ") T_VERSIONSTRING
#define A_APPNAME_WITH_VERSION APPNAME " " A_VERSIONSTRING
// [~] IRainman: copy-past fix.

#define DCVERSIONSTRING "0.785"

#define HOMEPAGE "http://flylinkdc.com/"
#define HELPPAGE "http://flylinkdc.com/dokuwiki/doku.php?id="
#define HOMEPAGERU "http://flylinkdc.ru/"
#define HELPPAGE2 "flylinkdc"

#define HOMEPAGE_BLOG "http://flylinkdc.blogspot.com/"
#define DISCUSS "http://flylinkdc.com/forum/"
#define SITES_FLYLINK_TRAC "http://code.google.com/p/flylinkdc/"


#endif // FLY_VERSION_H

/* Update the .rc file as well... */
