/*
 * Copyright (C) 2001-2016 Jacek Sieka, arnetheduck on gmail point com
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


#ifndef DCPLUSPLUS_DCPP_STDINC_H
#define DCPLUSPLUS_DCPP_STDINC_H

// - Используем VLD http://vld.codeplex.com/
// #define _CRTDBG_MAP_ALLOC
// #include <crtdbg.h>

#include "util_flylinkdc.h"

// --- Shouldn't have to change anything under here...

#ifndef BZ_NO_STDIO
#define BZ_NO_STDIO 1
#endif

#include "w.h"

#include <wchar.h>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <sys/types.h>
#include <time.h>
#include <locale.h>
#ifndef _MSC_VER
#include <stdint.h>
#endif

#include <algorithm>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <numeric>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _DEBUG
#include <boost/noncopyable.hpp>
#endif

#include <boost/static_assert.hpp> // [+]IRainman
/*
#ifdef _WIN32
//#define IRAINMAN_BOOST_REGEX_FAIL_FASTER // TODO use it!
#endif
//#define BOOST_NO_FUNCTION_TEMPLATE_ORDERING
// http://www.solarix.ru/for_developers/cpp/boost/regex/ru/configuration.shtml
#ifdef IRAINMAN_BOOST_REGEX_FAIL_FASTER
#define BOOST_REGEX_RECURSIVE 1
#else

#define BOOST_REGEX_NON_RECURSIVE 1
#ifdef BOOST_REGEX_NON_RECURSIVE
#define BOOST_REGEX_BLOCKSIZE 8096
#define BOOST_REGEX_MAX_CACHE_BLOCKS 128
#endif

#endif
*/
#include <regex>
using std::vector;
using std::list;
using std::deque;
using std::pair;
using std::string;
using std::wstring;
using std::unique_ptr;
using std::min;
using std::max;

using std::make_pair;

inline int stricmp(const string& a, const string& b)
{
	return _stricmp(a.c_str(), b.c_str());
}
inline int strnicmp(const string& a, const string& b, size_t n)
{
	return _strnicmp(a.c_str(), b.c_str(), n);
}
inline int strnicmp(const char* a, const char* b, size_t n)
{
	return _strnicmp(a, b, n); // [+] IRainman opt
}
inline int stricmp(const wstring& a, const wstring& b)
{
	return _wcsicmp(a.c_str(), b.c_str());
}
inline int stricmp(const wchar_t* a, const wchar_t* b)
{
	return _wcsicmp(a, b); // [+] IRainman opt
}
inline int strnicmp(const wstring& a, const wstring& b, size_t n)
{
	return _wcsnicmp(a.c_str(), b.c_str(), n);
}
inline int strnicmp(const wchar_t* a, const wchar_t* b, size_t n)
{
	return _wcsnicmp(a, b, n); // [+] IRainman opt
}

#endif // !defined(STDINC_H)

/**
 * @file
 * $Id: stdinc.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
