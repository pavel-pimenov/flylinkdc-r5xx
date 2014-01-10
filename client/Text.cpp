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

#include "stdinc.h"
#include <boost/algorithm/string.hpp>

#ifndef _WIN32
#include <errno.h>
#include <iconv.h>
#include <langinfo.h>

#ifndef ICONV_CONST
#define ICONV_CONST
#endif

#endif

#include "Util.h"

namespace Text
{

const string g_utf8 = "utf-8"; // optimization
const string g_code1251 = "Russian_Russia.1251"; //[+]FlylinkDC++ Team optimization
const string g_code1252 = "English_United Kingdom.1252"; //[+]FlylinkDC++ Team optimization

string systemCharset;

void initialize()
{
	setlocale(LC_ALL, "");
#ifdef _WIN32
	char *ctype = setlocale(LC_CTYPE, NULL);
	if (ctype)
	{
		systemCharset = string(ctype);
	}
	else
	{
		dcdebug("Unable to determine the program's locale");
	}
#else
	systemCharset = string(nl_langinfo(CODESET));
#endif
}

#ifdef _WIN32
int getCodePage(const string& p_charset)
{
	if (p_charset.empty())
		return CP_ACP;
	//[+]FlylinkDC++ Team optimization
	if (p_charset.size() >= 14 && p_charset[14] == '.')
		return 1251;
	if (p_charset.size() >= 22 && p_charset[22] == '.')
		return 1252;
	//[~]FlylinkDC++ Team optimization
	string::size_type pos = p_charset.find('.');
	if (pos == string::npos)
		return CP_ACP;
	return atoi(p_charset.c_str() + pos + 1);
}
#endif

bool isAscii(const char* str) noexcept
{
	for (const uint8_t* p = reinterpret_cast<const uint8_t*>(str); *p; ++p)
	{
		if (*p & 0x80)
			return false;
	}
	return true;
}

bool isAscii(const string& p_str) noexcept // [+] IRainman fix.
{
	for (auto p = p_str.cbegin(); p != p_str.cend(); ++p)
	{
		if (*p & 0x80)
			return false;
	}
	return true;
}

int utf8ToWc(const char* str, wchar_t& c)
{
	uint8_t c0 = (uint8_t)str[0];
	if (c0 & 0x80)                                  // 1xxx xxxx
	{
		if (c0 & 0x40)                              // 11xx xxxx
		{
			if (c0 & 0x20)                          // 111x xxxx
			{
				if (c0 & 0x10)                      // 1111 xxxx
				{
					int n = -4;
					if (c0 & 0x08)                  // 1111 1xxx
					{
						n = -5;
						if (c0 & 0x04)              // 1111 11xx
						{
							if (c0 & 0x02)          // 1111 111x
							{
								return -1;
							}
							n = -6;
						}
					}
					int i = -1;
					while (i > n && (str[abs(i)] & 0x80) == 0x80)
						--i;
					return i;
				}
				else        // 1110xxxx
				{
					uint8_t c1 = (uint8_t)str[1];
					if ((c1 & (0x80 | 0x40)) != 0x80)
						return -1;
						
					uint8_t c2 = (uint8_t)str[2];
					if ((c2 & (0x80 | 0x40)) != 0x80)
						return -2;
						
					// Ugly utf-16 surrogate catch
					if ((c0 & 0x0f) == 0x0d && (c1 & 0x3c) >= (0x08 << 2))
						return -3;
						
					// Overlong encoding
					if (c0 == (0x80 | 0x40 | 0x20) && (c1 & (0x80 | 0x40 | 0x20)) == 0x80)
						return -3;
						
					c = (((wchar_t)c0 & 0x0f) << 12) |
					    (((wchar_t)c1 & 0x3f) << 6) |
					    ((wchar_t)c2 & 0x3f);
					    
					return 3;
				}
			}
			else                // 110xxxxx
			{
				uint8_t c1 = (uint8_t)str[1];
				if ((c1 & (0x80 | 0x40)) != 0x80)
					return -1;
					
				// Overlong encoding
				if ((c0 & ~1) == (0x80 | 0x40))
					return -2;
					
				c = (((wchar_t)c0 & 0x1f) << 6) |
				    ((wchar_t)c1 & 0x3f);
				return 2;
			}
		}
		else                    // 10xxxxxx
		{
			return -1;
		}
	}
	else                        // 0xxxxxxx
	{
		c = (unsigned char)str[0];
		return 1;
	}
}

static inline void wcToUtf8(wchar_t c, string& str)
{
	if (c >= 0x0800)
	{
		str += (char)(0x80 | 0x40 | 0x20 | (c >> 12));
		str += (char)(0x80 | ((c >> 6) & 0x3f));
		str += (char)(0x80 | (c & 0x3f));
	}
	else if (c >= 0x0080)
	{
		str += (char)(0x80 | 0x40 | (c >> 6));
		str += (char)(0x80 | (c & 0x3f));
	}
	else
	{
		str += (char)c;
	}
}

const string& acpToUtf8(const string& str, string& tmp, const string& fromCharset) noexcept
{
	wstring wtmp;
//[+]PPA    dcdebug("acpToUtf8: %s\n", str.c_str());
	return wideToUtf8(acpToWide(str, wtmp, fromCharset), tmp);
}

const wstring& acpToWide(const string& str, wstring& tmp, const string& fromCharset) noexcept
{
	if (str.empty())
		return Util::emptyStringW;
#ifdef _WIN32
	const int l_code_page = getCodePage(fromCharset); //[+]FlylinkDC++ Team
	int n = MultiByteToWideChar(l_code_page, MB_PRECOMPOSED, str.c_str(), (int)str.length(), NULL, 0);
	if (n == 0)
	{
		return Util::emptyStringW;
	}
	tmp.resize(n);
	n = MultiByteToWideChar(l_code_page, MB_PRECOMPOSED, str.c_str(), (int)str.length(), &tmp[0], n); //TODO
	if (n == 0)
	{
		return Util::emptyStringW;
	}
//[+]PPA    dcdebug("acpToWide: %s\n", str.c_str());
	return tmp;
#else
	size_t rv;
	wchar_t wc;
	const char *src = str.c_str();
	size_t n = str.length() + 1;
	
	tmp.clear();
	tmp.reserve(n);
	
	while (n > 0)
	{
		rv = mbrtowc(&wc, src, n, NULL);
		if (rv == 0 || rv == (size_t) - 2)
		{
			break;
		}
		else if (rv == (size_t) - 1)
		{
			tmp.push_back(L'_');
			++src;
			--n;
		}
		else
		{
			tmp.push_back(wc);
			src += rv;
			n -= rv;
		}
	}
	return tmp;
#endif
}

const string& wideToUtf8(const wstring& str, string& tgt) noexcept
{
	if (str.empty())
		return Util::emptyString;
#ifdef _WIN32
	wstring::size_type size = 0;
	tgt.resize(str.length() * 2);
	while ((size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), str.length(), &tgt[0], tgt.length(), NULL, NULL)) == 0)
	{
//[+]PPA        dcdebug("wideToUtf8-[error]: %s str.length() = %d\n", tgt.c_str(),str.length());
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			tgt.resize(tgt.size() * 2);
		else
			break;
	}
	tgt.resize(size);
//[+]PPA    dcdebug("wideToUtf8: %s\n", tgt.c_str());
	return tgt;
#else
	const string::size_type n = str.length();// [!] IRainman add const
	tgt.clear();
	for (string::size_type i = 0; i < n; ++i)
	{
		wcToUtf8(str[i], tgt);
	}
	return tgt;
#endif
}

const string& wideToAcp(const wstring& str, string& tmp, const string& toCharset) noexcept
{
	if (str.empty())
		return Util::emptyString;
#ifdef _WIN32
	const int l_code_page = getCodePage(toCharset); //[+]FlylinkDC++ Team
	int n = WideCharToMultiByte(l_code_page, 0, str.c_str(), (int)str.length(), NULL, 0, NULL, NULL);
	if (n == 0)
	{
		return Util::emptyString;
	}
	tmp.resize(n);
	n = WideCharToMultiByte(l_code_page, 0, str.c_str(), (int)str.length(), &tmp[0], n, NULL, NULL);
	if (n == 0)
	{
		return Util::emptyString;
	}
//[+]PPA    dcdebug("wideToAcp: %s\n", tmp.c_str());
	return tmp;
#else
	const wchar_t* src = str.c_str();
	int n = wcsrtombs(NULL, &src, 0, NULL);
	if (n < 1)
	{
		return Util::emptyString;
	}
	src = str.c_str();
	tmp.resize(n);
	n = wcsrtombs(&tmp[0], &src, n, NULL);
	if (n < 1)
	{
		return Util::emptyString;
	}
	return tmp;
#endif
}

bool validateUtf8(const string& str) noexcept
{
	string::size_type i = 0;
	while (i < str.length())
	{
		wchar_t dummy = 0;
		int j = utf8ToWc(&str[i], dummy);
		if (j < 0)
			return false;
		i += j;
	}
	return true;
}

const string& utf8ToAcp(const string& str, string& tmp, const string& toCharset) noexcept
{
	wstring wtmp;
	return wideToAcp(utf8ToWide(str, wtmp), tmp, toCharset);
}

const wstring& utf8ToWide(const string& str, wstring& tgt) noexcept
{
#ifdef _WIN32
	wstring::size_type size = 0;
	tgt.resize(str.length() + 1); // 2012-04-29_13-46-19_ZB4DIS35ERNZ4WVNI7Y32THCOYPXEXPADDKUGBY_22E0B325_crash-stack-r501-x64-build-9869.dmp
	while ((size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), str.length(), &tgt[0], (int)tgt.length())) == 0)
		// 2012-05-11_23-57-17_H6LZ4GA3P2232TLWUOMIEM72RWRDPDCPIB6JE7Y_B3194916_crash-stack-r502-beta26-x64-build-9946.dmp
	{
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			tgt.resize(tgt.size() * 2);
		}
		else
		{
			break;
		}
		
	}
	tgt.resize(size);
	return tgt;
#else
	tgt.reserve(str.length());
	string::size_type n = str.length();
	for (string::size_type i = 0; i < n;)
	{
		wchar_t c = 0;
		int x = utf8ToWc(str.c_str() + i, c);
		if (x < 0)
		{
			tgt += '_';
			i += abs(x);
		}
		else
		{
			i += x;
			tgt += c;
		}
	}
	return tgt;
#endif
}

const wstring& toLower(const wstring& str, wstring& tmp) noexcept
{
	if (str.empty())
		return Util::emptyStringW;
	tmp.clear();
	tmp.reserve(str.length() + 2);
	for (auto i = str.cbegin(); i != str.cend(); ++i)
	{
		tmp += toLower(*i);
	}
	return tmp;
}

const string& toLower(const string& str, string& tmp) noexcept
{
	if (str.empty())
		return Util::emptyString;
		
	tmp.clear();
	tmp.reserve(str.length() + 1);
	const char* end = &str[0] + str.length();
	for (const char* p = &str[0]; p < end;)
	{
		wchar_t c = 0;
		int n = utf8ToWc(p, c);
		if (n < 0)
		{
			tmp += '_';
			p += abs(n);
		}
		else
		{
			p += n;
			wcToUtf8(toLower(c), tmp);
		}
	}
	return tmp;
}
const string& toLabel(const string& str, string& tmp)
{
	tmp = str;
	boost::replace_all(tmp, "&", "&&");
	return tmp;
}
const wstring& toLabel(const wstring& str, wstring& tmp)
{
	tmp = str;
	boost::replace_all(tmp, L"&", L"&&");
	return tmp;
}
const string& toUtf8(const string& str, const string& fromCharset, string& tmp) noexcept
{
	if (str.empty())
		return str;
	if (isUTF8(fromCharset, tmp)) //[+]FlylinkDC++ Team
		return str;
#ifdef _WIN32
	return acpToUtf8(str, tmp, fromCharset);
#else
	return convert(str, tmp, fromCharset, utf8);
#endif
}

const string& fromUtf8(const string& str, const string& toCharset, string& tmp) noexcept
{
	if (str.empty())
		return str;
	if (isUTF8(toCharset, tmp)) //[+]FlylinkDC++ Team
		return str;
#ifdef _WIN32
	return utf8ToAcp(str, tmp, toCharset);
#else
	return convert(str, tmp, utf8, toCharset);
#endif
}

#ifdef PPA_INCLUDE_DEAD_CODE
const string& convert(const string& str, string& tmp, const string& fromCharset, const string& toCharset) noexcept
{
	if (str.empty())
		return str;
		
#ifdef _WIN32
	if (stricmp(fromCharset, toCharset) == 0)
		return str;
	if (isUTF8(toCharset, tmp)) //[+]FlylinkDC++ Team
		return acpToUtf8(str, tmp);
	if (isUTF8(fromCharset, tmp)) //[+]FlylinkDC++ Team
		return utf8ToAcp(str, tmp);
		
	// We don't know how to convert arbitrary charsets
	dcdebug("Unknown conversion from %s to %s\n", fromCharset.c_str(), toCharset.c_str());
	return str;
#else
	
	// Initialize the converter
	iconv_t cd = iconv_open(toCharset.c_str(), fromCharset.c_str());
	if (cd == (iconv_t) - 1)
		return str;
	
	size_t rv;
	size_t len = str.length() * 2; // optimization
	size_t inleft = str.length();
	size_t outleft = len;
	tmp.resize(len);
	const char *inbuf = str.data();
	char *outbuf = (char *)tmp.data();
	
	while (inleft > 0)
	{
		rv = iconv(cd, (ICONV_CONST char **) & inbuf, &inleft, &outbuf, &outleft);
		if (rv == (size_t) - 1)
		{
			size_t used = outbuf - tmp.data();
			if (errno == E2BIG)
			{
				len *= 2;
				tmp.resize(len);
				outbuf = (char *)tmp.data() + used;
				outleft = len - used;
			}
			else if (errno == EILSEQ)
			{
				++inbuf;
				--inleft;
				tmp[used] = '_';
			}
			else
			{
				tmp.replace(used, inleft, string(inleft, '_'));
				inleft = 0;
			}
		}
	}
	iconv_close(cd);
	if (outleft > 0)
	{
		tmp.resize(len - outleft);
	}
	return tmp;
#endif
}
#endif

string toDOS(string tmp)
{
	if (tmp.empty())
		return Util::emptyString;
		
	if (tmp[0] == '\r' && (tmp.size() == 1 || tmp[1] != '\n'))
	{
		tmp.insert(1, "\n");
	}
	for (string::size_type i = 1; i < tmp.size() - 1; ++i)
	{
		if (tmp[i] == '\r' && tmp[i + 1] != '\n')
		{
			// Mac ending
			tmp.insert(i + 1, "\n");
			i++;
		}
		else if (tmp[i] == '\n' && tmp[i - 1] != '\r')
		{
			// Unix encoding
			tmp.insert(i, "\r");
			i++;
		}
	}
	return tmp;
}

wstring toDOS(wstring tmp)
{
	if (tmp.empty())
		return Util::emptyStringW;
		
	if (tmp[0] == L'\r' && (tmp.size() == 1 || tmp[1] != L'\n'))
	{
		tmp.insert(1, L"\n");
	}
	for (string::size_type i = 1; i < tmp.size() - 1; ++i)
	{
		if (tmp[i] == L'\r' && tmp[i + 1] != L'\n')
		{
			// Mac ending
			tmp.insert(i + 1, L"\n");
			i++;
		}
		else if (tmp[i] == L'\n' && tmp[i - 1] != L'\r')
		{
			// Unix encoding
			tmp.insert(i, L"\r");
			i++;
		}
	}
	return tmp;
}

// [+] FlylinkDC++

bool safe_strftime_translate(string& p_value)
{
	if (p_value.empty())
		return false;
	// Попытка заткунть проблему падения при кривой маске для strftime  http://www.rsdn.ru/forum/cpp.applied/5047749.1
	boost::algorithm::trim(p_value);
	const auto l_input_size = p_value.size();
	// 1. Убираем повторяющиеся %%
	size_t l_last_size;
	do
	{
		l_last_size = p_value.size();
		boost::replace_all(p_value, "%%", "%");
	}
	while (l_last_size != p_value.size());
	// 2. Запрещаем в конце символ %
	if (!p_value.empty() && p_value[p_value.size() - 1] == '%')
	{
		p_value.resize(p_value.size() - 1);
	}
	return l_input_size != p_value.size();
}

// [+] IRainman fix.
void normalizeStringEnding(tstring& p_text)
{
	boost::replace_all(p_text, _T("\r\n"), _T("\n"));
	boost::replace_all(p_text, _T("\n\r"), _T("\n"));
	std::replace(p_text.begin(), p_text.end(), _T('\r'), _T('\n'));
}

// [~] FlylinkDC++

}// namespace Text


