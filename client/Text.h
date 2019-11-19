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


#ifndef DCPLUSPLUS_DCPP_TEXT_H
#define DCPLUSPLUS_DCPP_TEXT_H

#include "typedefs.h"
#include <cctype>
#include <tchar.h>

/**
 * Text handling routines for DC++. DC++ internally uses UTF-8 for
 * (almost) all string:s, hence all foreign text must be converted
 * appropriately...
 * acp - ANSI code page used by the system
 * wide - wide unicode string
 * utf8 - UTF-8 representation of the string
 * t - current GUI text format
 * string - UTF-8 string (most of the time)
 * wstring - Wide string
 * tstring - GUI type string (acp string or wide string depending on build type)
 */

namespace Text
{
extern const string g_utf8;
extern const string g_code1251;
extern const string g_code1252;
extern string g_systemCharset;

void initialize();

const string& acpToUtf8(const string& str, string& tmp, const string& fromCharset = "") noexcept;
inline string acpToUtf8(const string& str, const string& fromCharset = "") noexcept
{
	string tmp;
	return acpToUtf8(str, tmp, fromCharset);
}

const wstring& acpToWide(const string& str, wstring& tmp, const string& fromCharset = "") noexcept;
inline wstring acpToWide(const string& str, const string& fromCharset = "") noexcept
{
	wstring tmp;
	return acpToWide(str, tmp, fromCharset);
}

const string& utf8ToAcp(const string& str, string& tmp, const string& toCharset = "") noexcept;
inline string utf8ToAcp(const string& str, const string& toCharset = "") noexcept
{
	string tmp;
	return utf8ToAcp(str, tmp, toCharset);
}

const wstring& utf8ToWide(const string& str, wstring& tmp) noexcept;
inline wstring utf8ToWide(const string& str) noexcept
{
	wstring tmp;
	return utf8ToWide(str, tmp);
}

const string& wideToAcp(const wstring& str, string& tmp, const string& toCharset = "") noexcept;
inline string wideToAcp(const wstring& str, const string& toCharset = "") noexcept
{
	string tmp;
	return wideToAcp(str, tmp, toCharset);
}
const string& wideToUtf8(const wstring& str, string& tmp) noexcept;
inline string wideToUtf8(const wstring& str) noexcept
{
	string tmp;
	return wideToUtf8(str, tmp);
}

int utf8ToWc(const char* str, wchar_t& c);

inline const tstring lowercase(tstring p_str) noexcept
{
	transform(p_str.begin(), p_str.end(), p_str.begin(), towlower);
	return p_str;
}
inline const string lowercase(string p_str) noexcept
{
	transform(p_str.begin(), p_str.end(), p_str.begin(), tolower);
	return p_str;
}
inline bool is_sub_tstring(const tstring& p_str1, const tstring& p_str2)
{
	return lowercase(p_str1).find(lowercase(p_str2)) != tstring::npos;
}
inline const tstring& toT(const string& str, tstring& tmp) noexcept
{
	return utf8ToWide(str, tmp);
}
inline tstring toT(const string& str) noexcept
{
	return utf8ToWide(str);
}

inline const string& fromT(const tstring& str, string& tmp) noexcept
{
	return wideToUtf8(str, tmp);
}
inline string fromT(const tstring& str) noexcept
{
	return wideToUtf8(str);
}
inline string fromT(const TCHAR* str) noexcept
{
	return fromT(tstring(str));
}

bool isAscii(const string& p_str) noexcept;
bool isAscii(const char* str) noexcept;
bool validateUtf8(const string& p_str, size_t p_pos = 0) noexcept;

inline char asciiToLower(uint8_t p_c)
{
	dcassert((p_c & 0x80) == 0);
	return std::tolower(int(p_c));
}

inline wchar_t toLower(wchar_t c) noexcept
{
#ifdef _WIN32
	return static_cast<wchar_t>(reinterpret_cast<ptrdiff_t>(CharLowerW((LPWSTR)c)));
#else
	return (wchar_t)towlower(c);
#endif
}

const wstring& toLower(const wstring& str, wstring& tmp) noexcept;
inline wstring toLower(const wstring& str) noexcept
{
	wstring tmp;
	return toLower(str, tmp);
}
inline string toLowerFast(string p_str) noexcept
{
	std::transform(p_str.begin(), p_str.end(), p_str.begin(), ::tolower);
	return p_str;
}

const string& toLower(const string& str, string& tmp) noexcept;
inline string toLower(const string& str) noexcept
{
	string tmp;
	return toLower(str, tmp);
}

const string& toLabel(const string& str, string& tmp) noexcept;
inline string toLabel(const string& str) noexcept
{
	string tmp;
	return toLabel(str, tmp);
}
const wstring& toLabel(const wstring& str, wstring& tmp) noexcept;
inline wstring toLabel(const wstring& str) noexcept
{
	wstring tmp;
	return toLabel(str, tmp);
}

inline bool isUTF8(const string& p_Charset, string& p_LowerCharset)
{
	if (g_utf8.size() == p_Charset.size() &&
	        (p_Charset == g_utf8 || toLower(p_Charset, p_LowerCharset) == g_utf8))
		return true;
	else
		return false;
}
#ifdef FLYLINKDC_USE_DEAD_CODE
const string& convert(const string& str, string& tmp, const string& fromCharset, const string& toCharset) noexcept;
inline string convert(const string& str, const string& fromCharset, const string& toCharset) noexcept
{
	string tmp;
	return convert(str, tmp, fromCharset, toCharset);
}
#endif

const string& toUtf8(const string& str, const string& fromCharset, string& tmp) noexcept;
inline string toUtf8(const string& str, const string& fromCharset = g_systemCharset)
{
	string tmp;
	return toUtf8(str, fromCharset, tmp);
}

const string& fromUtf8(const string& str, const string& toCharset, string& tmp) noexcept;
inline string fromUtf8(const string& str, const string& toCharset = g_systemCharset) noexcept
{
	string tmp;
	return fromUtf8(str, toCharset, tmp);
}

string toDOS(string tmp);
wstring toDOS(wstring tmp);

inline tstring CropStrLength(tstring p_Text, size_t p_len = 40)
{
	if (p_Text.length() > p_len)
	{
		p_Text.erase(p_len - 1, p_Text.length());
		p_Text += _T('Е');
	}
	return p_Text;
}

inline tstring CropStrLength(const string& p_str)
{
	return CropStrLength(Text::toT(p_str));
}

template<typename T>
inline tstring tformat(const tstring& src, T t)
{
	tstring ret(src.size() + 64, _T('\0'));
	int n = _sntprintf(&ret[0], ret.size(), src.c_str(), t);
	if (n != -1 && n < static_cast<int>(ret.size()))
	{
		ret.resize(static_cast<tstring::size_type>(n));
	}
	return ret;
}
template<typename T, typename T2, typename T3>
inline tstring tformat(const tstring& src, T t, T2 t2, T3 t3)
{
	tstring ret(src.size() + 128, _T('\0'));
	int n = _sntprintf(&ret[0], ret.size(), src.c_str(), t, t2, t3);
	if (n != -1 && n < static_cast<int>(ret.size()))
	{
		ret.resize(static_cast<tstring::size_type>(n));
	}
	return ret;
}

bool safe_strftime_translate(string& p_value);

inline bool isEqualsSubstringIgnoreCase(const string& p_sub_string, const string& p_str)
{
	dcassert(!p_sub_string.empty());
	dcassert(!p_str.empty());
	const bool l_res = _strnicmp(p_sub_string.c_str(), p_str.c_str(), p_str.size()) == 0;
#ifdef _DEBUG
//	string temp = p_sub_string.substr(0, p_str.size()); // TODO не делать копию заменить на _strnicmp
//	const bool l_res2 =  stricmp(temp, p_str) == 0;
//	dcassert(l_res == l_res2);
#endif
	return l_res;
}

void normalizeStringEnding(tstring& p_text);
void removeString_rn(string& p_text);
void replace_all(string& p_str, const string& p_from, const string& p_to);
void trim(string& p_str);
void replace_all(tstring& p_str, const tstring& p_from, const tstring& tp_to);

} // namespace Text

#endif // DCPLUSPLUS_DCPP_TEXT_H
