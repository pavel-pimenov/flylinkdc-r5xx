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

#ifndef DCPLUSPLUS_DCPP_STDINC_H
#define DCPLUSPLUS_DCPP_STDINC_H

#ifndef TORRENT_USE_BOOST_DATE_TIME
#define TORRENT_USE_BOOST_DATE_TIME
#endif
#ifndef TORRENT_BUILDING_SHARED
#define TORRENT_BUILDING_SHARED
#endif
#ifndef TORRENT_LINKING_SHARED
#define TORRENT_LINKING_SHARED
#endif
#ifndef BOOST_ASIO_HEADER_ONLY
#define BOOST_ASIO_HEADER_ONLY
#endif
#ifndef BOOST_ASIO_ENABLE_CANCELIO
#define BOOST_ASIO_ENABLE_CANCELIO
#endif
#ifndef BOOST_ASIO_SEPARATE_COMPILATION
#define BOOST_ASIO_SEPARATE_COMPILATION
#endif
#ifndef TORRENT_DISABLE_GEO_IP
#define TORRENT_DISABLE_GEO_IP
#endif
#ifndef TORRENT_DISABLE_ENCRYPTION
#define TORRENT_DISABLE_ENCRYPTION
#endif
#ifndef BOOST_ASIO_HAS_STD_CHRONO
#define BOOST_ASIO_HAS_STD_CHRONO
#endif

// - Используем VLD http://vld.codeplex.com/
// #define _CRTDBG_MAP_ALLOC
// #include <crtdbg.h>

#include "util_flylinkdc.h"

// --- Shouldn't have to change anything under here...

#ifndef BZ_NO_STDIO
#define BZ_NO_STDIO 1
#endif

#include "w.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#ifdef _DEBUG
#include <boost/noncopyable.hpp>
#endif

using std::vector;
using std::string;
using std::wstring;
using std::unique_ptr;
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
	return _strnicmp(a, b, n);
}
inline int stricmp(const wstring& a, const wstring& b)
{
	return _wcsicmp(a.c_str(), b.c_str());
}
inline int stricmp(const wchar_t* a, const wchar_t* b)
{
	return _wcsicmp(a, b);
}
inline int strnicmp(const wstring& a, const wstring& b, size_t n)
{
	return _wcsnicmp(a.c_str(), b.c_str(), n);
}
inline int strnicmp(const wchar_t* a, const wchar_t* b, size_t n)
{
	return _wcsnicmp(a, b, n);
}

#endif // !defined(DCPLUSPLUS_DCPP_STDINC_H)

/**
 * @file
 * $Id: stdinc.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
