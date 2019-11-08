/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#pragma once

#ifndef DCPLUSPLUS_DCPP_COMPILER_H
#define DCPLUSPLUS_DCPP_COMPILER_H

# ifndef _DEBUG
#  ifndef NDEBUG
#   error define NDEBUG in Release configuration! (assert runtime bug!)
#  endif
# endif

#if defined(__GNUC__)
# if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 5)
# error GCC 4.5 is required

# endif

#elif defined(_MSC_VER)
# if _MSC_VER < 1600
#  error MSVC 10 (2010) is required
# endif

# ifndef _DEBUG
/*
#  if _MSC_VER <= 1500
#   define _SECURE_SCL  0
#   define _SECURE_SCL_THROWS 0
#  else
*/
#  define _ITERATOR_DEBUG_LEVEL 0 // VC++ 2010
//#  endif
#  define BOOST_DISABLE_ASSERTS 1
#  define _HAS_ITERATOR_DEBUGGING 0
# else
/*
#  if _MSC_VER <= 1500
#   define _SECURE_SCL  1
#   define _SECURE_SCL_THROWS 1
#  else
*/
#  define _ITERATOR_DEBUG_LEVEL 2 // VC++ 2010 
//#  endif
#  define _HAS_ITERATOR_DEBUGGING 1
# endif

# define memzero(dest, n) memset(dest, 0, n)

//disable the deprecated warnings for the CRT functions.
#ifndef _CRT_SECURE_NO_DEPRECATE
# define _CRT_SECURE_NO_DEPRECATE 1
#endif
# define _ATL_SECURE_NO_DEPRECATE 1
# define _CRT_NON_CONFORMING_SWPRINTFS 1
# ifndef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#  define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
# endif

# define strtoll _strtoi64

#else
# error No supported compiler found

#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
# define _LL(x) x##ll
# define _ULL(x) x##ull
# define I64_FMT "%I64d"
# define U64_FMT "%I64u" // [PVS-Studio] V576. Incorrect format. Consider checking the N actual argument of the 'Foo' function

#elif defined(SIZEOF_LONG) && SIZEOF_LONG == 8
# define _LL(x) x##l
# define _ULL(x) x##ul
# define I64_FMT "%ld"
# define U64_FMT "%lu" // [PVS-Studio] V576. Incorrect format. Consider checking the N actual argument of the 'Foo' function
#else
# define _LL(x) x##ll
# define _ULL(x) x##ull
# define I64_FMT "%lld"
# define U64_FMT "%llu" // [PVS-Studio] V576. Incorrect format. Consider checking the N actual argument of the 'Foo' function
#endif

#ifndef _REENTRANT
# define _REENTRANT 1
#endif

#include "compiler_flylinkdc.h"

#endif // DCPLUSPLUS_DCPP_COMPILER_H
