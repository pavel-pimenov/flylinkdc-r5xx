﻿/*
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


#ifndef DCPLUSPLUS_DCPP_UTIL_H
#define DCPLUSPLUS_DCPP_UTIL_H

#include <wininet.h>
#ifndef _CONSOLE
#include <atlcomtime.h>
#endif

#include <map>

#include "Text.h"
#include "CFlyThread.h"
#include "MerkleTree.h"
#include "LogManager.h"

# define PATH_SEPARATOR '\\'
# define PATH_SEPARATOR_STR "\\"
# define PATH_SEPARATOR_WSTR L"\\"
#define URI_SEPARATOR '/'
#define URI_SEPARATOR_STR "/"
#define URI_SEPARATOR_WSTR L"/"

#define FLYLINKDC_REGISTRY_PATH _T("SOFTWARE\\FlylinkDC++")
#define FLYLINKDC_REGISTRY_MEDIAINFO_FREEZE_KEY _T("MediaFreezeInfo")
#define FLYLINKDC_REGISTRY_MEDIAINFO_CRASH_KEY  _T("MediaCrashInfo")
#define FLYLINKDC_REGISTRY_SQLITE_ERROR  _T("SQLiteError")
#define FLYLINKDC_REGISTRY_LEVELDB_ERROR  _T("LevelDBError")

#define PLAY_SOUND(sound_key) Util::playSound(SOUND_SETTING(sound_key))
#define PLAY_SOUND_BEEP(sound_key) { if (SOUND_BEEP_BOOLSETTING(sound_key)) Util::playSound(SOUND_SETTING(SOUND_BEEPFILE), true); }

const string getFlylinkDCAppCaption();
const tstring getFlylinkDCAppCaptionT();
const string getFlylinkDCAppCaptionWithVersion();
const tstring getFlylinkDCAppCaptionWithVersionT();

class IDateReceiveReporter
{
	public:
		virtual bool ReportResultWait(DWORD totalDataWait) = 0;
		virtual bool ReportResultReceive(DWORD currentDataReceive) = 0;
		virtual bool SetCurrentStage(std::string& value) = 0;
};


class CInternetHandle
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		explicit CInternetHandle(HINTERNET p_hInternet): m_hInternet(p_hInternet)
		{
		}
		~CInternetHandle()
		{
			if (m_hInternet)
			{
#ifdef _DEBUG
				BOOL l_res =
#endif
				    ::InternetCloseHandle(m_hInternet);
				dcassert(l_res);
			}
		}
		operator const HINTERNET() const
		{
			return m_hInternet;
		}
	protected:
		const HINTERNET m_hInternet;
};

class CFlyHTTPDownloader
{
		DWORD m_inet_flag;
		DWORD m_last_error_code;
		string m_error_message;
		bool switchMirrorURL(string& p_url, int p_mirror);
	public:
		static int g_last_stable_mirror;
		static void nextMirror();
		bool m_is_add_url;
		bool m_is_use_cache;
		tstring m_user_agent;
		CFlyHTTPDownloader() : m_inet_flag(0), m_last_error_code(0), m_is_add_url(true), m_is_use_cache(false)
		{
		}
		std::vector<string> m_get_http_header_item;
		uint64_t getBinaryDataFromInetSafe(const string& p_url, std::vector<unsigned char>& p_dataOut, LONG p_time_out = 0, IDateReceiveReporter* p_reporter = NULL);
		uint64_t getBinaryDataFromInet(const string& p_url, std::vector<unsigned char>& p_dataOut, LONG p_time_out = 0, IDateReceiveReporter* p_reporter = NULL);
		struct CFlyUrlItem
		{
			string m_url;
			std::vector<unsigned char> p_body;
		};
		typedef std::vector<CFlyUrlItem> CFlyUrlItemArray;
		
		uint64_t getBinaryDataFromInetArray(CFlyUrlItemArray& p_url_array, LONG p_time_out = 0, IDateReceiveReporter* p_reporter = NULL);
		void clear()
		{
			m_get_http_header_item.clear();
		}
		void setInetFlag(DWORD p_inet_flag)
		{
			m_inet_flag = p_inet_flag;
		}
		const string& getErroMessage() const
		{
			return m_error_message;
		}
		DWORD getLastErrorCode() const
		{
			return m_last_error_code;
		}
		void create_error_message(const char* p_type, const string& p_url);
};

template <class T>
void AppendPathSeparator(T& p_path)
{
	if (!p_path.empty())
	{
		const auto l_last_char = p_path[ p_path.length() - 1 ];
		if (l_last_char != PATH_SEPARATOR && l_last_char != URI_SEPARATOR)
		{
			p_path += PATH_SEPARATOR;
		}
		else
		{
			dcassert(l_last_char != URI_SEPARATOR)
		}
	}
}

template <class T>
static void AppendUriSeparator(T& p_path)
{
	if (!p_path.empty())
	{
		const auto l_last_char = p_path[ p_path.length() - 1 ];
		if (l_last_char != URI_SEPARATOR && l_last_char != PATH_SEPARATOR)
		{
			p_path += URI_SEPARATOR;
		}
		else
		{
			dcassert(l_last_char != PATH_SEPARATOR)
		}
	}
}
static void ReplaceAllUriSeparatorToPathSeparator(string& p_InOutData)
{
	std::replace(p_InOutData.begin(), p_InOutData.end(), URI_SEPARATOR, PATH_SEPARATOR);
}
static void ReplaceAllUriSeparatorToPathSeparator(wstring& p_InOutData)
{
	std::replace(p_InOutData.begin(), p_InOutData.end(), _T(URI_SEPARATOR), _T(PATH_SEPARATOR));
}
static void ReplaceAllPathSeparatorToUriSeparator(wstring& p_InOutData)
{
	std::replace(p_InOutData.begin(), p_InOutData.end(), _T(PATH_SEPARATOR), _T(URI_SEPARATOR));
}
static void ReplaceAllPathSeparatorToUriSeparator(string& p_InOutData)
{
	std::replace(p_InOutData.begin(), p_InOutData.end(), PATH_SEPARATOR, URI_SEPARATOR);
}

template <class STR>
static STR RemovePathSeparator(const STR& p_path)
{
	return (p_path.length() < 4/* Drive letter, like "C:\" */ || p_path[p_path.length() - 1] != PATH_SEPARATOR) ? p_path : p_path.substr(0, p_path.length() - 1); //-V112
}

template <class STR>
static void AppendQuotsToPath(STR& p_path)
{
	if (p_path.length() < 1)
		return;
		
	if (p_path[0] != '"')
		p_path = '\"' + p_path;
		
	if (p_path[p_path.length() - 1] != '"')
		p_path += '\"';
}

template <class STR>
static void RemoveQuotsFromPath(STR& p_path)
{
	if (p_path.length() < 1)
		return;
		
	if (p_path[0] == '"' && p_path[p_path.length() - 1] == '"')
		p_path = p_path.substr(1, p_path.length() - 2);
}

template <class T> class FlyLinkVector : public std::vector<T>
{
		typedef std::vector<T> inherited;
	public:
		void erase_and_check(const T& p_Value)
		{
			/*
			#ifdef _DEBUG
			      dcassert(find(begin(), end(), p_Value) != end());
			#else
			      if(find(begin(), end(), p_Value) != end())
			#endif
			*/
			inherited::erase(std::remove(inherited::begin(), inherited::end(), p_Value), inherited::end());
		}
};


template<typename T, bool flag> struct ReferenceSelector
{
	typedef T ResultType;
};
template<typename T> struct ReferenceSelector<T, true>
{
	typedef const T& ResultType;
};

template<typename T> class IsOfClassType
{
	public:
		template<typename U> static char check(int U::*);
		template<typename U> static float check(...);
	public:
		enum { Result = sizeof(check<T>(0)) };
};

template<typename T> struct TypeTraits
{
	typedef IsOfClassType<T> ClassType;
	typedef ReferenceSelector < T, ((ClassType::Result == 1) || (sizeof(T) > sizeof(char*))) > Selector;
	typedef typename Selector::ResultType ParameterType;
};

#define GETSET(type, name, name2) \
	private: type name; \
	public: TypeTraits<type>::ParameterType get##name2() const { return name; } \
	void set##name2(TypeTraits<type>::ParameterType a##name2) { name = a##name2; }
#define GETSET_BOOL(type, name, name2) \
	private: type name; \
	public: TypeTraits<type>::ParameterType get##name2() const { return name; } \
	bool set##name2(TypeTraits<type>::ParameterType a##name2) { if(name == a##name2) return false; name = a##name2; return true; }

#define GETM(type, name, name2) \
	private: type name; \
	public: TypeTraits<type>::ParameterType get##name2() const { return name; }

#define GETC(type, name, name2) \
	private: const type name; \
	public: TypeTraits<type>::ParameterType get##name2() const { return name; }

/** Evaluates op(pair<T1, T2>.second, compareTo) */
#if 0
template < class T1, class T2, class op = equal_to<T2> >
class CompareSecond
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		CompareSecond(const T2& compareTo) : a(compareTo) { }
		bool operator()(const pair<T1, T2>& p)
		{
			return op()(p.second, a);
		}
	private:
		const T2& a;
};

/** Evaluates op(pair<T1, T2>.second, compareTo) */
template < class T1, class T2, class T3, class op = equal_to<T2> >
class CompareSecondFirst
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		CompareSecondFirst(const T2& compareTo) : a(compareTo) { }
		bool operator()(const pair<T1, pair<T2, T3>>& p)
		{
			return op()(p.second.first, a);
		}
	private:
		const T2& a;
};

#endif
/**
 * Compares two values
 * @return -1 if v1 < v2, 0 if v1 == v2 and 1 if v1 > v2
 */
template<typename T1>
inline int compare(const T1& v1, const T1& v2)
{
	return (v1 < v2) ? -1 : ((v1 == v2) ? 0 : 1);
}

template<typename T>
class AutoArray
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
		typedef T* TPtr;
	public:
#ifdef _DEBUG
		explicit AutoArray(size_t size, char p_fill) : p(new T[size])
		{
			memset(p, p_fill, size);
		}
#endif
		explicit AutoArray(size_t size) : p(new T[size]) { }
		~AutoArray()
		{
			delete[] p;
		}
		operator TPtr()
		{
			return p;
		}
		TPtr data()
		{
			return p;
		}
	private:
		TPtr p;
};

template<class T, size_t N>
class LocalArray
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		T m_data[N];
		LocalArray()
		{
			m_data[0]   = 0;
		}
		static size_t size()
		{
			return N;
		}
		T& operator[](size_t p_pos)
		{
			dcassert(p_pos < N);
			return m_data[p_pos];
		}
		const T* data() const
		{
			return m_data;
		}
		T* data()
		{
			return m_data;
		}
		void init()
		{
			memzero(m_data, sizeof(m_data));
		}
};

class MD5Calc;
class Util
{
	public:
		static const char* getCountryShortName(uint16_t p_flag_index);
		static int getFlagIndexByCode(uint16_t p_countryCode);
		static const tstring emptyStringT;
		static const string emptyString;
		static const wstring emptyStringW;
		
		static const std::vector<uint8_t> emptyByteVector;
		
		static const string m_dot;
		static const string m_dot_dot;
		static const tstring m_dotT;
		static const tstring m_dot_dotT;
		static NUMBERFMT g_nf;
		static void initialize();
		static void loadCustomlocations();
		static void loadGeoIp();
		static void loadP2PGuard();
		static void loadIBlockList();
		
		static bool isNmdc(const tstring& p_HubURL);
		static bool isNmdcS(const tstring& p_HubURL);
		static bool isAdc(const tstring& p_HubURL);
		static bool isAdcS(const tstring& p_HubURL);
		static bool isNmdc(const string& p_HubURL);
		static bool isNmdcS(const string& p_HubURL);
		static bool isAdc(const string& p_HubURL);
		static bool isAdcS(const string& p_HubURL);
		
		template<typename string_t>
		static bool isAdcHub(const string_t& p_HubURL)
		{
			return isAdc(p_HubURL) || isAdcS(p_HubURL);
		}
		template<typename string_t>
		static bool isNmdcHub(const string_t& p_HubURL)
		{
			return isNmdc(p_HubURL) || isNmdcS(p_HubURL);
		}
		template<typename string_t>
		static bool isDcppHub(const string_t& p_HubURL)
		{
			return isNmdc(p_HubURL) || isAdcHub(p_HubURL);
		}
		// Identify magnet links.
		static bool isMagnetLink(const char* p_URL);
		static bool isMagnetLink(const string& p_URL);
		static bool isMagnetLink(const wchar_t* p_URL);
		static bool isMagnetLink(const tstring& p_URL);
#if 0
		static bool isImageLink(const tstring& p_URL)
		{
			return strnicmp(p_URL.c_str(), _T("http://"), 7) == 0
			       && strnicmp(p_URL.c_str() + (p_URL.length() - 4), _T(".jpg"), 4) == 0; //-V112
		}
#endif
		static bool isTorrentLink(const tstring& sFileName);
		static bool isHttpLink(const tstring& p_url);
		static bool isHttpLink(const string& p_url);
		static bool isValidIP(const string& p_ip);
		static bool isHttpsLink(const tstring& p_url);
		static bool isHttpsLink(const string& p_url);
		
		// From RSSManager.h
		static string ConvertFromHTML(const string &htmlString);
		static string ConvertFromHTMLSymbol(const string &htmlString, const string &findString, const string &changeString);
		
		// Erase all HTML tags from string
		static tstring eraseHtmlTags(tstring && p_desc);
		
		// File Extension comparsing
		template<typename string_t>
		static bool isSameFileExt(const string_t& fileName, const string_t& ext)
		{
			return fileName.length() > ext.length() &&
			       !strnicmp(fileName.c_str() + fileName.length() - ext.length(), ext.c_str(), ext.length());
		}
		template<typename string_t>
		static bool isSameFileExt(const string_t& fileName, const string_t& ext, const bool lower)
		{
			if (lower)
				return isSameFileExt(fileName, ext);
			else
			{
				const string_t l_file = Text::toLower(fileName);
				return isSameFileExt(l_file, ext);
			}
		}
		static bool isTorrentFile(const tstring& file);
		static bool isDclstFile(const tstring& file);
		static bool isDclstFile(const string& file);
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		static bool isStreamingVideoFile(const string& p_file);
#endif // SSA_VIDEO_PREVIEW_FEATURE
		
		/** Path of temporary storage */
		static string getTempPath();
		
		/** Migrate from pre-localmode config location */
		static void migrate(const string& file);
		
		/** Path of file lists */
		static const string& getListPath()
		{
			return getPath(PATH_FILE_LISTS);
		}
		/** Path of hub lists */
		static const string& getHubListsPath()
		{
			return getPath(PATH_HUB_LISTS);
		}
		/** Notepad filename */
		static const string& getNotepadFile()
		{
			return getPath(PATH_NOTEPAD);
		}
		/** Path of settings */
		static const string& getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
		    bool p_AllUsers = false
#endif
		)
		{
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
			if (p_AllUsers)
				return getPath(PATH_ALL_USER_CONFIG);
			else
#endif
				return getPath(PATH_USER_CONFIG);
		}
		/** Path of program file */
		static const string& getDataPath()
		{
			return getPath(PATH_GLOBAL_CONFIG);
		}
		/** Path of localisation file */
		static const string& getLocalisationPath()
		{
			return getPath(PATH_LANGUAGES);
		}
#ifdef FLYLINKDC_USE_EXTERNAL_MAIN_ICON
		/** Path of icon */
		static const string getICOPath()
		{
			return getPath(PATH_EXTERNAL_ICO);
		}
#endif
		static const string getExternalLogoPath()
		{
			return getPath(PATH_EXTERNAL_LOGO);
		}
		/** Path of local mode */
		static const string& getLocalPath()
		{
			return getPath(PATH_USER_LOCAL);
		}
		/** Path of downloads dir */
		static const string& getDownloadsPath()
		{
			return getPath(PATH_DOWNLOADS);
		}
		/** Path of EmoPacks */
		static const string& getEmoPacksPath()
		{
			return getPath(PATH_EMOPACKS);
		}
		static const string& getThemesPath()
		{
			return getPath(PATH_THEMES);
		}
		static const string& getExePath()
		{
			return getPath(PATH_EXE);
		}
		static const string& getSoundPath()
		{
			return getPath(PATH_SOUNDS);
		}
		
		static const string& getGPGPUPath()
		{
			return getPath(PATH_GPGPU);
		}
		
		static string getIETFLang();
		
		static string translateError(DWORD aError);
		static string translateError()
		{
			return translateError(GetLastError());
		}
		
		static TCHAR* strstr(const TCHAR *str1, const TCHAR *str2, int *pnIdxFound);
		
		static time_t getStartTime()
		{
			return g_startTime;
		}
		static time_t getUpTime()
		{
			return time(nullptr) - getStartTime();
		}
		
		template<typename string_t>
		static string_t getFilePath(const string_t& path)
		{
			const auto i = path.rfind(PATH_SEPARATOR);
			return (i != string_t::npos) ? path.substr(0, i + 1) : path;
		}
		template<typename string_t>
		static string_t getFileName(const string_t& path)
		{
			const auto i = path.rfind(PATH_SEPARATOR);
			return (i != string_t::npos) ? path.substr(i + 1) : path;
		}
		static string getFileExtWithoutDot(const string& path)
		{
			const auto i = path.rfind('.');
			return i != string::npos ? path.substr(i + 1) : Util::emptyString;
		}
		static string getFileDoubleExtWithoutDot(const string& path)
		{
			auto i = path.rfind('.');
			if (i != string::npos && i)
			{
				i = path.rfind('.', i - 1);
				if (i != string::npos)
				{
					const auto l_res_2exe = path.substr(i + 1);
					return l_res_2exe;
				}
			}
			return Util::emptyString;
		}
		/*
		template <class T> static inline void check_path(const T& path)
		        {
		#ifdef _DEBUG
		            dcassert(!path.empty());
		            dcassert(path[path.size() - 1] != '/' && path[path.size() - 1] != '\\');
		#endif
		        }
		        */
		static wstring getFileExtWithoutDot(const wstring& path)
		{
			//check_path(path);
			const auto i = path.rfind('.');
			if (i != wstring::npos)
			{
				const auto l_res = path.substr(i + 1);
				if (l_res.rfind(PATH_SEPARATOR) == string::npos)
					return l_res;
			}
			return Util::emptyStringW;
		}
		static string getFileExt(const string& path)
		{
			//check_path(path);
			const auto i = path.rfind('.');
			if (i != string::npos)
			{
				const auto l_res = path.substr(i);
				if (l_res.rfind(PATH_SEPARATOR) == string::npos)
					return l_res;
			}
			return Util::emptyString;
		}
		static wstring getFileExt(const wstring& path)
		{
			//check_path(path);
			const auto i = path.rfind(L'.');
			if (i != string::npos)
			{
				const auto l_res = path.substr(i);
				if (l_res.rfind(PATH_SEPARATOR) == string::npos)
					return l_res;
			}
			return Util::emptyStringW;
		}
		static string getLastDir(const string& path)
		{
			const auto i = path.rfind(PATH_SEPARATOR);
			if (i == string::npos)
				return Util::emptyString;
				
			const auto j = path.rfind(PATH_SEPARATOR, i - 1);
			return j != string::npos ? path.substr(j + 1, i - j - 1) : path;
		}
		static wstring getLastDir(const wstring& path)
		{
			const auto i = path.rfind(PATH_SEPARATOR);
			if (i == wstring::npos)
				return Util::emptyStringW;
				
			const auto j = path.rfind(PATH_SEPARATOR, i - 1);
			return j != wstring::npos ? path.substr(j + 1, i - j - 1) : path;
		}
		
		template<typename string_t>
		static void replace(const string_t& search, const string_t& replacement, string_t& str)
		{
			string_t::size_type i = 0;
			while ((i = str.find(search, i)) != string_t::npos)
			{
				str.replace(i, search.size(), replacement);
				i += replacement.size();
			}
		}
		template<typename string_t>
		static inline void replace(const typename string_t::value_type* search, const typename string_t::value_type* replacement, string_t& str)
		{
			replace(string_t(search), string_t(replacement), str);
		}
		
		static void decodeUrl(const string& aUrl, string& protocol, string& host, uint16_t& port, string& path, string& query, string& fragment)
		{
			bool isSecure;
			decodeUrl(aUrl, protocol, host, port, path, isSecure, query, fragment);
		}
		static void parseIpPort(const string& aIpPort, string& ip, uint16_t& port);
		static bool isValidSearch(const string& p_search);
		static void decodeUrl(const string& aUrl, string& protocol, string& host, uint16_t& port, string& path, bool& isSecure, string& query, string& fragment);
		static std::map<string, string> decodeQuery(const string& query);
		
		static string validateFileName(string aFile);
		static string cleanPathChars(string aNick);
		static void fixFileNameMaxPathLimit(string& p_File);
		
		static string addBrackets(const string& s)
		{
			return '<' + s + '>';
		}
		static string formatBytes(const string& aString)
		{
			return formatBytes(toInt64(aString));
		}
		
		static wstring formatBytesW(const wstring& aString)
		{
			return formatBytesW(toInt64(aString));
		}
		
		static string getShortTimeString(time_t t = time(NULL));
		
		static string toAdcFile(const string& file);
		static string toNmdcFile(const string& file);
		
		static string formatBytes(int64_t aBytes); // TODO - template?
		static string formatBytes(uint32_t aBytes)
		{
			return formatBytes(int64_t(aBytes));
		}
		static string formatBytes(double aBytes); // TODO - template?
		static string formatBytes(uint64_t aBytes)
		{
			return formatBytes(double(aBytes));
		}
		static string formatBytes(unsigned long aBytes)
		{
			return formatBytes(double(aBytes));
		}
		static wstring formatBytesW(int64_t aBytes);
		
		static wstring formatExactSize(int64_t aBytes);
		
		static wstring formatSecondsW(int64_t aSec, bool supressHours = false);
		static string formatSeconds(int64_t aSec, bool supressHours = false);
		static string formatParams(const string& msg, const StringMap& params, bool filter, const time_t p_t = time(NULL));
		static string formatTime(const string& msg, const time_t p_t);
		static string formatTime(uint64_t rest, const bool withSecond = true);
		static string formatDigitalClockGMT(const time_t& p_t);
		static string formatDigitalClock(const time_t& p_t);
		static string formatDigitalDate();
		static string formatDigitalClock(const string &p_msg, const time_t& p_t, bool p_is_gmt);
		static string formatRegExp(const string& msg, const StringMap& params);
		
		static inline int64_t roundDown(int64_t size, int64_t blockSize)
		{
			return ((size + blockSize / 2) / blockSize) * blockSize;
		}
		
		static inline int64_t roundUp(int64_t size, int64_t blockSize)
		{
			return ((size + blockSize - 1) / blockSize) * blockSize;
		}
		
		static inline int roundDown(int size, int blockSize)
		{
			return ((size + blockSize / 2) / blockSize) * blockSize;
		}
		
		static inline int roundUp(int size, int blockSize)
		{
			return ((size + blockSize - 1) / blockSize) * blockSize;
		}
		
		static int64_t toInt64(const string& aString)
		{
			//if(aString.size() == 1 && aString[0] == '0')
			//  return 0;
			//else
			return toInt64(aString.c_str());
		}
		
		static int64_t toInt64(const char* aString)
		{
#ifdef _WIN32
			return _atoi64(aString);
#else
			return strtoll(aString, (char **)NULL, 10);
#endif
		}
		
		static int64_t toInt64(const wstring& aString)
		{
			//if(aString.size() == 1 && aString[0] == L'0')
			//  return 0;
			//else
			return toInt64(aString.c_str());
		}
		
		static int64_t toInt64(const wchar_t* aString)
		{
#ifdef _WIN32
			return _wtoi64(aString);
#else
			// TODO return strtoll(aString, (char **)NULL, 10);
#endif
		}
		
		static int toInt(const string& aString)
		{
			if (aString.empty())
				return 0;
			else
				return toInt(aString.c_str());
		}
		
		static int toInt(const char* aString)
		{
			return atoi(aString);
		}
		
		static int toInt(const wstring& aString)
		{
			if (aString.empty())
				return 0;
			else
				return toInt(aString.c_str());
		}
		
		static int toInt(const wchar_t* aString)
		{
			return _wtoi(aString);
		}
		
		static uint32_t toUInt32(const string& aString)
		{
			if (aString.empty())
				return 0;
			else
				return toUInt32(aString.c_str());
		}
		static uint32_t toUInt32(const char* c)
		{
#ifdef _MSC_VER
			/*
			* MSVC's atoi returns INT_MIN/INT_MAX if out-of-range; hence, a number
			* between INT_MAX and UINT_MAX can't be converted back to uint32_t.
			*/
			uint32_t ret = atoi(c);
			if ((ret == INT_MAX || ret == INT_MIN) && errno == ERANGE)
			{
				ret = (uint32_t)_atoi64(c);
			}
			return ret;
#else
			return (uint32_t)atoi(c);
#endif
		}
		
		static double toDouble(const string& aString);
		
		static float toFloat(const string& aString)
		{
			return (float)toDouble(aString);
		}
		
		static string toString(short val)
		{
			char buf[8];
			_snprintf(buf, _countof(buf), "%d", (int)val);
			return buf;
		}
		static string toString(uint16_t val)
		{
			char buf[8];
			_snprintf(buf, _countof(buf), "%u", (unsigned int)val);
			return buf;
		}
		static string toString(int val)
		{
			char buf[16];
			_snprintf(buf, _countof(buf), "%d", val);
			return buf;
		}
		static string toStringPercent(int val)
		{
			char buf[16];
			_snprintf(buf, _countof(buf), "%d%%", val);
			return buf;
		}
		static string toString(unsigned int val)
		{
			char buf[16];
			_snprintf(buf, _countof(buf), "%u", val);
			return buf;
		}
		static string toString(long val)
		{
			char buf[24]; //-V112
			_snprintf(buf, _countof(buf), "%ld", val);
			return buf;
		}
		static string toHexString(long val)
		{
			char buf[24];
			_snprintf(buf, _countof(buf), "%X", (unsigned long)val);
			return buf;
		}
		static string toString(unsigned long val)
		{
			char buf[24]; //-V112
			_snprintf(buf, _countof(buf), "%lu", val);
			return buf;
		}
		static string toString(long long val)
		{
			char buf[24];
			_snprintf(buf, _countof(buf), I64_FMT, val);
			return buf;
		}
		static string toString(unsigned long long val)
		{
			char buf[24];
			_snprintf(buf, _countof(buf), U64_FMT, val);
			return buf;
		}
		static string toString(void* val)
		{
			return toString((unsigned long long) val);
		}
		
		static string toString(double val)
		{
			char buf[24];
			_snprintf(buf, _countof(buf), "%0.2f", val);
			return buf;
		}
		
		static string toString(const char* p_sep, const StringList& p_lst);
		static string toString(char p_sep, const StringList& p_lst);
		static string toString(char p_sep, const StringSet& p_set);
		static string toString(const StringList& p_lst);
		static const string& toString(const string& p_str)
		{
			return p_str;
		}
		
		static string toSupportsCommand(const StringList& p_feat)
		{
			const string l_result = "$Supports " + toString(' ', p_feat) + '|';
			return  l_result;
		}
		
		static wstring toStringW(int32_t val)
		{
			wchar_t buf[32];
			_snwprintf(buf, _countof(buf), L"%d", val);
			return buf;
		}
		
		static wstring toStringW(uint32_t val)
		{
			wchar_t buf[32];
			_snwprintf(buf, _countof(buf), L"%u", val);
			return buf;
		}
		
		static wstring toStringW(int64_t val)
		{
			wchar_t buf[64];
			_snwprintf(buf, _countof(buf), _T(I64_FMT), val);
			return buf;
		}
		
		static wstring toStringW(uint64_t val)
		{
			wchar_t buf[64];
			_snwprintf(buf, _countof(buf), _T(U64_FMT), val);
			return buf;
		}
		
		static wstring toStringW(double val)
		{
			wchar_t buf[20];
			_snwprintf(buf, _countof(buf), L"%0.2f", val);
			return buf;
		}
		
		static string toHexEscape(char val)
		{
			char buf[sizeof(int) * 2 + 1 + 1];
			_snprintf(buf, _countof(buf), "%%%X", val & 0x0FF);
			return buf;
		}
		static char fromHexEscape(const string &aString)
		{
			unsigned int res = 0;
			if (sscanf(aString.c_str(), "%X", &res) == EOF)
			{
				// TODO log error!
			}
			return static_cast<char>(res);
		}
		template<typename T, class NameOperator>
		static string listToStringT(const T& lst, bool forceBrackets, bool squareBrackets)
		{
			string tmp;
			if (lst.empty())
				return tmp;
			if (lst.size() == 1 && !forceBrackets)
				return NameOperator()(*lst.begin());
				
			tmp.push_back(squareBrackets ? '[' : '(');
			for (auto i = lst.begin(), iend = lst.end(); i != iend; ++i)
			{
				tmp += NameOperator()(*i);
				tmp += ", ";
			}
			
			if (tmp.length() == 1)
			{
				tmp.push_back(squareBrackets ? ']' : ')');
			}
			else
			{
				tmp.pop_back();
				tmp[tmp.length() - 1] = squareBrackets ? ']' : ')';
			}
			return tmp;
		}
		struct StrChar
		{
			const char* operator()(const string& u)
			{
				dcassert(!u.empty()) // FlylinkDC++
				if (!u.empty())
					return u.c_str();
				else
					return "";
			}
		};
		
		template<typename ListT>
		static string listToString(const ListT& lst)
		{
			return listToStringT<ListT, StrChar>(lst, false, true);
		}
		template<typename T>
		static tstring toStringT(const T& p_val)
		{
			return Text::toT(toString(p_val));
		}
		
		
#if 0
		template<typename T>
		static T& intersect(T& t1, const T& t2)
		{
			for (auto i = t1.cbegin(); i != t1.cend();)
			{
				if (find_if(t2.begin(), t2.end(), bind1st(equal_to<typename T::value_type>(), *i)) == t2.end())
					i = t1.erase(i);
				else
					++i;
			}
			return t1;
		}
#endif
		static string encodeURI(const string& /*aString*/, bool reverse = false);
		static string getLocalOrBindIp(const bool p_check_bind_address);
		static bool isPrivateIp(const string& p_ip);
		static bool isNotPrivateIpAndNot169(const string& p_ip)
		{
			return !isPrivateIp(p_ip) && strncmp(p_ip.c_str(), "169", 3) != 0;
		}
		static bool isPrivateIp(uint32_t p_ip)
		{
			return ((p_ip & 0xff000000) == 0x0a000000 || // 10.0.0.0/8
			        (p_ip & 0xff000000) == 0x7f000000 || // 127.0.0.0/8
			        (p_ip & 0xffff0000) == 0xa9fe0000 || // 169.254.0.0/16
			        (p_ip & 0xfff00000) == 0xac100000 || // 172.16.0.0/12
			        (p_ip & 0xffff0000) == 0xc0a80000);  // 192.168.0.0/16
		}
		/**
		 * Case insensitive substring search.
		 * @return First position found or string::npos
		 */
		static string::size_type findSubString(const string& aString, const string& aSubString, string::size_type start = 0) noexcept;
		static wstring::size_type findSubString(const wstring& aString, const wstring& aSubString, wstring::size_type start = 0) noexcept;
		
		static int DefaultSort(const wchar_t* a, const wchar_t* b, bool noCase = true);
		
		static bool getAway()
		{
			return g_away;
		}
		static void setAway(bool aAway, bool notUpdateInfo = false);
		static string getAwayMessage(StringMap& params);
		static void setAwayMessage(const string& p_Msg)
		{
			g_awayMsg = p_Msg;
		}
		
		static uint64_t getDirSize(const string &sFullPath);
		static bool validatePath(const string &sPath);
		static string getFilenameForRenaming(const string& p_filename);
		
		template<typename T>
		static void shrink_to_fit(T* start, const T* stop)
		{
			for (; start != stop; ++start)
				start->shrink_to_fit();
		}
		
		static uint32_t rand();
		static uint32_t rand(uint32_t high)
		{
			dcassert(high > 0);
			return rand() % high;
		}
		static uint32_t rand(uint32_t low, uint32_t high)
		{
			return rand(high - low) + low;
		}
		static string getRandomNick(size_t iNickLength = 20);
		
		static string getRegistryCommaSubkey(const tstring& p_key);
		static string getRegistryValueString(const TCHAR* p_key, bool p_is_path = false);
		static bool setRegistryValueString(const TCHAR* p_key, const tstring& p_value);
		static bool deleteRegistryValue(const TCHAR* p_key);
		
		static bool setRegistryValueInt(const TCHAR* p_key, DWORD p_value);
		static DWORD getRegistryValueInt(const TCHAR* p_key);
		
		static string getWANIP(const string& p_url, LONG p_timeOut = 500);
		
		static size_t getDataFromInet(bool p_is_use_cache, const string& url, string& data, LONG timeOut = 0, IDateReceiveReporter* reporter = NULL);
		static size_t getDataFromInetSafe(bool p_is_use_cache, const string& url, string& data, LONG timeOut = 0, IDateReceiveReporter* reporter = NULL);
		
		
#ifdef _WIN32
#ifndef _CONSOLE
		static tstring getCompileDate(const LPCTSTR& p_format = _T("%Y-%m-%d"))
		{
			COleDateTime tCompileDate;
			tCompileDate.ParseDateTime(_T(__DATE__), LOCALE_NOUSEROVERRIDE, 1033);
			return tCompileDate.Format(p_format).GetString();
		}
		
		static tstring getCompileTime(const LPCTSTR& p_format = _T("%H-%M-%S"))
		{
			COleDateTime tCompileDate;
			tCompileDate.ParseDateTime(_T(__TIME__), LOCALE_NOUSEROVERRIDE, 1033);
			return tCompileDate.Format(p_format).GetString();
		}
#endif // _CONSOLE
#endif // _WIN32
		static void setLimiter(bool aLimiter);
		
		static bool getTTH_MD5(const string& p_filename, const size_t p_buffSize, std::unique_ptr<TigerTree>* p_tth, std::unique_ptr<MD5Calc>* p_md5 = 0, bool p_isAbsPath = true);
		static void BackupSettings();
		static string formatDchubUrl(const string& DchubUrl);
		
		static string getMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize);
		static string getMagnetByPath(const string& aFile);
		
		static string getWebMagnet(const TTHValue& aHash, const string& aFile, int64_t aSize);
		
	private:
	
		enum Paths
		{
			/** Global configuration */
			PATH_GLOBAL_CONFIG,
			/** Per-user configuration (queue, favorites, ...) */
			PATH_USER_CONFIG,
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
			/** All-user local data (GeoIP, custom location, portal browser settings, ...) */
			PATH_ALL_USER_CONFIG,
#endif
			/** Per-user local data (cache, temp files, ...) */
			PATH_USER_LOCAL,
			/** Translations */
			/** Various resources (help files etc) */
//			PATH_RESOURCES,
			/** Default download location */
			PATH_DOWNLOADS,
			/** Default file list location */
			PATH_FILE_LISTS,
			/** Default hub list cache */
			PATH_HUB_LISTS,
			/** Where the notepad file is stored */
			PATH_NOTEPAD,
			/** Folder with emoticons packs*/
			PATH_EMOPACKS,
			/** Languages files location*/
			PATH_LANGUAGES,
#ifdef FLYLINKDC_USE_EXTERNAL_MAIN_ICON
			/** External app icon path*/
			PATH_EXTERNAL_ICO,
#endif
			/** Themes and resources */
			PATH_THEMES,
			/** Executable path **/
			PATH_EXE,
			/** Sounds path **/
			PATH_SOUNDS,
			/** Files for GPGPU **/
			PATH_GPGPU,
			PATH_EXTERNAL_LOGO,
			PATH_LAST
		};
	public:
		enum SysPaths
		{
#ifdef _WIN32
			WINDOWS,
			PROGRAM_FILESX86,
			PROGRAM_FILES,
			APPDATA,
			LOCAL_APPDATA,
			COMMON_APPDATA,
			PERSONAL,
#endif
			SYS_PATH_LAST
		};
	private:
	
		/** In local mode, all config and temp files are kept in the same dir as the executable */
		static bool g_localMode;
		static string g_paths[PATH_LAST];
		static string g_sysPaths[SYS_PATH_LAST];
		static bool g_away;
		static string g_awayMsg;
		static time_t g_awayTime;
		static const time_t g_startTime;
		
		
	public:
		static const tstring getModuleFileName();
		static const string getModuleCustomFileName(const string& p_file_name);
		
		struct CustomNetworkIndex
		{
			public:
				explicit CustomNetworkIndex() :
					m_location_cache_index(-1),
					m_country_cache_index(-1)
				{
				}
				explicit CustomNetworkIndex(int32_t p_location_cache_index, int16_t p_country_cache_index) :
					m_location_cache_index(p_location_cache_index),
					m_country_cache_index(p_country_cache_index)
				{
				}
				bool isNew() const
				{
					return m_location_cache_index == -1 && m_country_cache_index == -1;
				}
				bool isKnown() const
				{
					return m_location_cache_index >= 0 || m_country_cache_index >= 0 ;
				}
				tstring getDescription() const;
				tstring getCountry() const;
				int32_t getFlagIndex() const;
				int16_t getCountryIndex() const;
			private:
				int16_t m_country_cache_index;
				int32_t m_location_cache_index;
		};
		
		static CustomNetworkIndex getIpCountry(uint32_t p_ip, bool p_is_use_only_cache = false);
		static CustomNetworkIndex getIpCountry(const string& p_ip, bool p_is_use_only_cache = false);
		
	private:
	
		static void loadBootConfig();
		/** Path of program configuration files */
		static const string& getPath(Paths p_path)
		{
			dcassert(!g_paths[p_path].empty());
			return g_paths[p_path];
		}
	public:
		/** Path of system folder */
		static const string& getSysPath(SysPaths p_path)
		{
			dcassert(!g_sysPaths[p_path].empty());
			return g_sysPaths[p_path];
		}
		static bool locatedInSysPath(SysPaths path, const string& p_path);
		static bool checkForbidenFolders(const string& p_path);
	private:
		static void intiProfileConfig();
		static void moveSettings();
#ifdef _WIN32
		
		static string getDownloadPath(const string& def);
		
#endif
	public:
		static void playSound(const string& p_sound, const bool p_beep = false);
		
		static StringList splitSettingAndReplaceSpace(string patternList);
		static string toSettingString(const StringList& patternList);
		static StringList splitSettingAndLower(const string& patternList);
		
		static string getLang();
		
		static string getWikiLink();
		
#ifdef _WIN32
		static DWORD GetTextResource(const int p_res, LPCSTR& p_data);
		static void WriteTextResourceToFile(const int p_res, const tstring& p_file);
#endif
		
};

/** Case insensitive hash function for strings */
struct noCaseStringHash
{
	size_t operator()(const string* s) const
	{
		return operator()(*s);
	}
	
	size_t operator()(const string& s) const
	{
		size_t x = 0;
		const char* end = s.data() + s.size();
		for (const char* str = s.data(); str < end;)
		{
			wchar_t c = 0;
			int n = Text::utf8ToWc(str, c);
			if (n < 0)
			{
				x = x * 32 - x + '_'; //-V112
				str += static_cast<size_t>(abs(n));
			}
			else
			{
				x = x * 32 - x + (size_t)Text::toLower(c); //-V112
				str += static_cast<size_t>(n);
			}
		}
		return x;
	}
	
	size_t operator()(const wstring* s) const
	{
		return operator()(*s);
	}
	size_t operator()(const wstring& s) const
	{
		size_t x = 0;
		const wchar_t* y = s.data();
		wstring::size_type j = s.size();
		for (wstring::size_type i = 0; i < j; ++i)
		{
			x = x * 31 + (size_t)Text::toLower(y[i]);
		}
		return x;
	}
	
	bool operator()(const string& a, const string& b) const
	{
		return stricmp(a.c_str(), b.c_str()) < 0;
	}
};

/** Case insensitive string comparison */
struct noCaseStringEq
{
	bool operator()(const string* a, const string* b) const
	{
		return a == b || stricmp(a->c_str(), b->c_str()) == 0;
	}
	bool operator()(const string& a, const string& b) const
	{
		return stricmp(a.c_str(), b.c_str()) == 0;
	}
};

inline bool __fastcall EqualD(double A, double B)
{
	return fabs(A - B) <= 1e-6;
}

// parent class for objects with a lot of empty columns in list
template<int C> class ColumnBase
#ifdef _DEBUG
	: private boost::noncopyable
#endif
{
	public:
		virtual ~ColumnBase() {}
		const tstring& getText(const int col) const
		{
			dcassert(col >= 0 && col < C);
			return m_info[col];
		}
		void setText(const int col, const tstring& val)
		{
			dcassert(col >= 0 && col < C);
			m_info[col] = val;
		}
	private:
		tstring m_info[C];
};

template<typename TASK_TYPE, const unsigned int PAUSE_IN_MILLS_BEFORE_THREAD_DEADS = 1000>
class BackgroundTaskExecuter : public Thread, protected CFlyStopThread
{
	public:
		explicit BackgroundTaskExecuter() :  m_active(false)
#ifdef _DEBUG
			, m_runningThreadId(DWORD(-1))
#endif
		{
		}
		virtual ~BackgroundTaskExecuter()
		{
			dcassert(m_runningThreadId == DWORD(-1));
			dcassert(!m_active);
			//dcassert(m_stop);
			stopThread();
			join();
		}
		
		void addTask(const TASK_TYPE& toAdd, bool p_is_check_shutdown = true)
		{
			if (p_is_check_shutdown == true)
			{
				dcassert(!isShutdown());
			}
			if (m_stop == false)
			{
				{
					CFlyFastLock(m_csTasks);
					m_tasks.push_front(toAdd);
					if (m_active)
						return;
					m_active = true;
				}
				startThread();
			}
		}
		void forceStop()
		{
			stopThread();
		}
		
		void waitShutdown(const bool exit = true)
		{
			if (exit)
			{
				forceStop();
			}
			while (m_active)
			{
				sleep(10);
			}
		}
		
	private:
		void startThread()
		{
			try
			{
				start(64);
			}
			catch (const ThreadException& e)
			{
				{
					CFlyFastLock(m_csTasks);
					m_active = false;
				}
				LogManager::message("BackgroundTaskExecuter::startThread failed: " + e.getError());
			}
		}
		
		int run()
		{
#ifdef _DEBUG
			m_runningThreadId = ::GetCurrentThreadId();
#endif
			for (;;)
			{
				TASK_TYPE next;
				{
					for (uint64_t lbefore = 0;;)
					{
						if (m_tasks.empty())
						{
							if (lbefore >= PAUSE_IN_MILLS_BEFORE_THREAD_DEADS || isShutdown())
							{
								CFlyFastLock(m_csTasks);
								if (m_tasks.empty())
								{
									m_active = false;
#ifdef _DEBUG
									m_runningThreadId = DWORD(-1);
#endif
									return 0;
								}
							}
							else
							{
								const int STEP_FOR_SLEEP = 250;
								sleep(STEP_FOR_SLEEP);
								lbefore += STEP_FOR_SLEEP;
								continue;
							}
						}
						else
						{
							break;
						}
					}
					
					CFlyFastLock(m_csTasks);
					std::swap(next, m_tasks.back());
					m_tasks.pop_back();
				}
				execute(next);
			}
		}
		virtual void execute(const TASK_TYPE & toExecute) = 0;
		typedef std::list<TASK_TYPE> TaskList;
		volatile bool m_active;
		TaskList m_tasks;
		FastCriticalSection m_csTasks;
#ifdef _DEBUG
		DWORD m_runningThreadId;
#endif
};

#endif // !defined(UTIL_H)

/**
 * @file
 * $Id: Util.h 575 2011-08-25 19:38:04Z bigmuscle $
 */
