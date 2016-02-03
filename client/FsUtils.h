
#pragma once

#ifndef FS_UTILS_H
#define FS_UTILS_H

#include "CFlyThread.h"

namespace FsUtils
{

//#define FS_UTILS_UNICODE

#ifdef FS_UTILS_UNICODE
#define FsUtils_LPCtSTR LPCWSTR
#define FsUtils_str std::wstring
#define FsUtils_slen wcslen
#define FsUtils_sncmp wcsncmp
#define FsUtils_schr wcschr
#define FsUtils_L(a) L##a
#define FsUtils_CHAR WCHAR
#define FsUtils_GetVolumeInformation GetVolumeInformationW
#define FsUtils_CreateFile CreateFileW
#else
#define FsUtils_LPCtSTR LPCSTR
#define FsUtils_str std::string
#define FsUtils_slen strlen
#define FsUtils_sncmp strncmp
#define FsUtils_schr strchr
#define FsUtils_L(a) a
#define FsUtils_CHAR CHAR
#define FsUtils_GetVolumeInformation GetVolumeInformationA
#define FsUtils_CreateFile CreateFileA
#endif

class CFsTypeDetector
{
	public:
		explicit CFsTypeDetector(HWND hWnd = NULL):
			m_hWnd(hWnd)
		{
		}
		
		bool IsStreamSupported(FsUtils_LPCtSTR pwcFileName);
		LRESULT OnDeviceChange(LPARAM lParam, WPARAM wParam);
		void SetNotifyWnd(HWND hWnd)
		{
			m_hWnd = hWnd;
		}
		
		// Follow function work only since Vista.
		// Can implement it dynamically, if needed
		// bool IsStreamSupported(HANDLE hFile);
		
	private:
		struct VOL_STRUCT
		{
			bool bSupportStream;
			HDEVNOTIFY hNotify;
			explicit VOL_STRUCT(bool p_SupportStream = false): bSupportStream(p_SupportStream), hNotify(nullptr)
			{
			}
		};
		
		typedef std::unordered_map<FsUtils_str, VOL_STRUCT> tCACHE;
		
		//FastCriticalSection m_cs; // [+] IRainman fix.
		tCACHE m_cache;
		HWND m_hWnd;
};
#endif

} // FsUtils