/*
 * Copyright (C) 2011-2013 Alexey Solomin, a.rainman on gmail pt com
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

#  if defined(_MSC_VER) && _MSC_VER >= 1800 && _MSC_VER < 1900 && defined(_M_X64)
#    include <math.h> /* needed for _set_FMA3_enable */
#  endif

#define FLYLINKDC_USE_DISK_SPACE_INFO

#include <ImageHlp.h>

#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
#define PSAPI_VERSION 1
#endif
#include <psapi.h>
#if _MSC_VER >= 1700
#include <atlbase.h>
#include <atlapp.h>
#endif
#include "Socket.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "CompatibilityManager.h"
#include "ShareManager.h"
#include "../FlyFeatures/flyServer.h"

#include <iphlpapi.h>
#include <direct.h>

#pragma comment(lib, "Imagehlp.lib")

string CompatibilityManager::g_incopatibleSoftwareList;
string CompatibilityManager::g_startupInfo;
DWORDLONG CompatibilityManager::g_TotalPhysMemory;
DWORDLONG CompatibilityManager::g_FreePhysMemory;
OSVERSIONINFOEX CompatibilityManager::g_osvi = {0};
SYSTEM_INFO CompatibilityManager::g_sysInfo = {0};
bool CompatibilityManager::g_supports[LAST_SUPPORTS];
LONG CompatibilityManager::g_comCtlVersion = 0;
DWORD CompatibilityManager::g_oldPriorityClass = 0;
bool CompatibilityManager::g_is_teredo = false;
bool CompatibilityManager::g_is_ipv6_enabled = false;
FINDEX_INFO_LEVELS CompatibilityManager::g_find_file_level = FindExInfoStandard;
// http://msdn.microsoft.com/ru-ru/library/windows/desktop/aa364415%28v=vs.85%29.aspx
// FindExInfoBasic
//     The FindFirstFileEx function does not query the short file name, improving overall enumeration speed.
//     The data is returned in a WIN32_FIND_DATA structure, and the cAlternateFileName member is always a NULL string.
//     Windows Server 2008, Windows Vista, Windows Server 2003, and Windows XP:  This value is not supported until Windows Server 2008 R2 and Windows 7.
DWORD CompatibilityManager::g_find_file_flags = 0;
// http://msdn.microsoft.com/ru-ru/library/windows/desktop/aa364419%28v=vs.85%29.aspx
// Uses a larger buffer for directory queries, which can increase performance of the find operation.
// Windows Server 2008, Windows Vista, Windows Server 2003, and Windows XP:  This value is not supported until Windows Server 2008 R2 and Windows 7.


void CompatibilityManager::init()
{
#ifdef _WIN64
	// https://code.google.com/p/chromium/issues/detail?id=425120
	// FMA3 support in the 2013 CRT is broken on Vista and Windows 7 RTM (fixed in SP1). Just disable it.
	// fix crash https://drdump.com/Problem.aspx?ProblemID=102616
	//           https://drdump.com/Problem.aspx?ProblemID=102601
#if _MSC_VER >= 1800 && _MSC_VER < 1900
	_set_FMA3_enable(0);
#endif
#endif
	setWine(detectWine());
	
	if (!isWine() && getFromSystemIsAppRunningIsWow64())
		set(RUNNING_IS_WOW64);
		
	detectOsSupports();
	getSystemInfoFromOS();
	generateSystemInfoForApp();
	if (CompatibilityManager::isWin7Plus())
	{
		g_find_file_level = FindExInfoBasic;
		g_find_file_flags = FIND_FIRST_EX_LARGE_FETCH;
	}
	g_is_teredo = checkTeredo();
}

void CompatibilityManager::getSystemInfoFromOS()
{
	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms724958(v=vs.85).aspx
	GetSystemInfo(&g_sysInfo);
}

void CompatibilityManager::detectOsSupports()
{
	g_osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	
	if (!GetVersionEx((OSVERSIONINFO*)&g_osvi))
		memzero(&g_osvi, sizeof(OSVERSIONINFOEX));
		
#define FUTURE_VER(future_major_version) \
	(getOsMajor() >= (future_major_version))
		
#define FUTURE_MINOR_VER(current_major_version, future_minor_version) \
	(getOsMajor() == (current_major_version) && getOsMinor() >= (future_minor_version))
		
#define CURRENT_VER(current_major_version, current_minor_version) \
	(getOsMajor() == (current_major_version) && getOsMinor() == (current_minor_version))
		
#define CURRENT_VER_SP(current_major_version, current_minor_version, current_sp_version) \
	(getOsMajor() == (current_major_version) && getOsMinor() == (current_minor_version) && getOsSpMajor() == (current_sp_version))
		
	if (FUTURE_VER(8) || // future version
	        FUTURE_MINOR_VER(10, 1) || // Windows 10 and newer
	        CURRENT_VER(10, 0)) // Windows 10
		set(OS_WINDOWS10_PLUS);
		
	if (FUTURE_VER(7) || // future version
	        FUTURE_MINOR_VER(6, 3) || // Windows 8.1
	        CURRENT_VER(6, 2)) // Windows 8
		set(OS_WINDOWS8_PLUS);
		
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
	if (FUTURE_VER(7) || // future version
	        FUTURE_MINOR_VER(6, 2) || // Windows 8 and newer
	        CURRENT_VER(6, 1)) // Windows 7
		set(OS_WINDOWS7_PLUS);
#endif
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (FUTURE_VER(7) || // future version
	        FUTURE_MINOR_VER(6, 1) || // Windows 7 and newer
	        CURRENT_VER(6, 0)) // Windows Vista
		set(OS_VISTA_PLUS);
		
	if (FUTURE_VER(6) || // Windows Vista and newer
	        CURRENT_VER_SP(5, 2, 2) || // Windows Server 2003 SP2
	        CURRENT_VER_SP(5, 1, 3)) // Windows XP SP3
		set(OS_XP_SP3_PLUS);
#endif
		
	if (
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	    !CURRENT_VER_SP(5, 1, 3) && // Windows XP SP3 http://ru.wikipedia.org/wiki/Windows_XP
	    !CURRENT_VER_SP(5, 2, 2) && // Windows Server 2003 SP2  http://ru.wikipedia.org/wiki/Windows_Server_2003
#endif
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
	    !CURRENT_VER_SP(6, 0, 2) && // Windows Vista SP2 & Windows Server 2008 SP2 // http://ru.wikipedia.org/wiki/Windows_Vista http://en.wikipedia.org/wiki/Windows_Server_2008
#endif
	    !CURRENT_VER_SP(6, 1, 1) && // Windows 7 SP1 & Windows Server 2008 R2 SP1  http://en.wikipedia.org/wiki/Windows_7 http://en.wikipedia.org/wiki/Windows_Server_2008_R2
	    !CURRENT_VER_SP(6, 2, 0) &&   // Windows 8 & Windows Server 2012 http://en.wikipedia.org/wiki/Windows_8 http://ru.wikipedia.org/wiki/Windows_Server_2012
	    !CURRENT_VER_SP(6, 3, 0) &&  // Windows 8.1 & Windows Server 2012 R2 http://en.wikipedia.org/wiki/Windows_8.1 http://ru.wikipedia.org/wiki/Windows_Server_2012
	    !CURRENT_VER_SP(10, 0, 0)) // https://msdn.microsoft.com/ru-ru/library/windows/desktop/ms724833(v=vs.85).aspx  мануал (ну как и раньше, только дополнен Windows 10/0)
	{
#ifdef FLYLINKDC_USE_CHECK_OLD_OS
		set(RUNNING_AN_UNSUPPORTED_OS);
#endif
	}
	
#undef FUTURE_VER
#undef FUTURE_MINOR_VER
#undef CURRENT_VER
#undef CURRENT_VER_SP
	
	g_comCtlVersion = getComCtlVersionFromOS();
}

bool CompatibilityManager::detectWine()
{
	const HMODULE module = GetModuleHandle(_T("ntdll.dll"));
	if (!module)
		return false;
		
	const bool ret = GetProcAddress(module, "wine_server_call") != nullptr;
	FreeLibrary(module);
	return ret;
}

struct DllInfo
{
	TCHAR* dllName;
	char* info;
};

// TODO - config
static const DllInfo g_IncompatibleDll[] =
{
	_T("nvappfilter.dll"), "NVIDIA ActiveArmor (NVIDIA Firewall)",
	_T("nvlsp.dll"), "NVIDIA Application Filter",
	_T("nvlsp64.dll"), "NVIDIA Application Filter",
	_T("chtbrkg.dll"), "Adskip or Internet Download Manager integration",
	_T("adguard.dll"), "Adguard", // Latest verion 5.1 not bad.
	_T("pcapwsp.dll"), "ProxyCap",
	_T("NetchartFilter.dll"), "NetchartFilter",
	_T("vlsp.dll"), "Venturi Wireless Software",
	_T("radhslib.dll"), "Naomi web filter",
	_T("AmlMaple.dll"), "Aml Maple",
	_T("sdata.dll"), "Trojan-PSW.Win32.LdPinch.ajgw",
	_T("browsemngr.dll"), "Browser Manager",
	_T("owexplorer_10616.dll"), "Overwolf Overlay",
	_T("owexplorer_10615.dll"), "Overwolf Overlay",
	_T("owexplorer_20125.dll"), "Overwolf Overlay",
	_T("owexplorer_2006.dll"), "Overwolf Overlay",
	_T("owexplorer_20018.dll"), "Overwolf Overlay",
	_T("owexplorer_2000.dll"), "Overwolf Overlay",
	_T("owexplorer_20018.dll"), "Overwolf Overlay",
	_T("owexplorer_20015.dll"), "Overwolf Overlay",
	_T("owexplorer_1069.dll"), "Overwolf Overlay",
	_T("owexplorer_10614.dll"), "Overwolf Overlay",
	_T("am32_33707.dll"), "Ad Muncher",
	_T("am32_32562.dll"), "Ad Muncher",
	_T("am64_33707.dll"), "Ad Muncher x64",
	_T("am64_32130.dll"), "Ad Muncher x64",
	_T("browserprotect.dll"), "Browserprotect",
	_T("searchresultstb.dll"), "DTX Toolbar",
	_T("networx.dll"), "NetWorx",
	nullptr, nullptr, // termination
};

void CompatibilityManager::detectUncompatibleSoftware()
{
	const DllInfo* currentDllInfo = g_IncompatibleDll;
	// TODO http://stackoverflow.com/questions/420185/how-to-get-the-version-info-of-a-dll-in-c
	for (; currentDllInfo->dllName; ++currentDllInfo)
	{
		const HMODULE hIncompatibleDll = GetModuleHandle(currentDllInfo->dllName);
		if (hIncompatibleDll)
		{
			g_incopatibleSoftwareList += "\r\n";
			const auto l_dll = Text::fromT(currentDllInfo->dllName);
			g_incopatibleSoftwareList += l_dll;
			if (currentDllInfo->info)
			{
				g_incopatibleSoftwareList += " - ";
				g_incopatibleSoftwareList += currentDllInfo->info;
			}
		}
	}
	g_incopatibleSoftwareList.shrink_to_fit();
}

string CompatibilityManager::getIncompatibleSoftwareMessage()
{
	dcassert(isIncompatibleSoftwareFound());
	if (isIncompatibleSoftwareFound())
	{
		string temp;
		temp.resize(32768);
		temp.resize(sprintf_s(&temp[0], temp.size() - 1, CSTRING(INCOMPATIBLE_SOFTWARE_FOUND), g_incopatibleSoftwareList.c_str())); //-V111
		temp += ' ' + Util::getWikiLink() + "incompatiblesoftware";
		return temp;
	}
	return Util::emptyString;
}

string CompatibilityManager::getFormatedOsVersion()
{
	string l_OS = Util::toString(getOsMajor()) + '.' + Util::toString(getOsMinor());
	if (getOsType() == VER_NT_SERVER)
	{
		l_OS += " Server";
	}
	if (getOsSpMajor() > 0)
	{
		l_OS += " SP" + Util::toString(getOsSpMajor());
	}
	if (isWine())
	{
		l_OS += " (Wine)";
	}
	if (runningIsWow64())
	{
		l_OS += " (WOW64)";
	}
	return l_OS;
}

string CompatibilityManager::getProcArchString()
{
	string l_ProcArch;
	//int l_ProcLevel = g_sysInfo.wProcessorLevel;
	switch (g_sysInfo.wProcessorArchitecture)
	{
		case PROCESSOR_ARCHITECTURE_AMD64:
			l_ProcArch = " x86-x64";
			break;
		case PROCESSOR_ARCHITECTURE_INTEL:
			l_ProcArch = " x86";
			break;
		case PROCESSOR_ARCHITECTURE_IA64:
			l_ProcArch = " Intel Itanium-based";
			break;
		default: // PROCESSOR_ARCHITECTURE_UNKNOWN
			l_ProcArch = " Unknown";
			break;
	};
	return l_ProcArch;
}
string CompatibilityManager::getWindowsVersionName()
{
	string l_OS = "Microsoft Windows ";
	DWORD dwType;
	
// Check for unsupported OS
//	if (VER_PLATFORM_WIN32_NT != g_osvi.dwPlatformId || getOsMajor() <= 4)
//	{
//		return false;
//	}

	// Test for the specific product.
	// https://msdn.microsoft.com/ru-ru/library/windows/desktop/ms724833(v=vs.85).aspx
	if (getOsMajor() == 6 || getOsMajor() == 10)
	{
		if (getOsMajor() == 10)
		{
			if (getOsMinor() == 0)
			{
				if (getOsType() == VER_NT_WORKSTATION)
					l_OS += "10 ";
				else
					l_OS += "Windows Server 2016 ";
			}
			// check type for Win10: Desktop, Mobile, etc...
		}
		if (getOsMajor() == 6)
		{
			if (getOsMinor() == 0)
			{
				if (getOsType() == VER_NT_WORKSTATION)
					l_OS += "Vista ";
				else
					l_OS += "Server 2008 ";
			}
			if (getOsMinor() == 1)
			{
				if (getOsType() == VER_NT_WORKSTATION)
					l_OS += "7 ";
				else
					l_OS += "Server 2008 R2 ";
			}
			if (getOsMinor() == 2)
			{
				if (getOsType() == VER_NT_WORKSTATION)
					l_OS += "8 ";
				else
					l_OS += "Server 2012 ";
			}
			if (getOsMinor() == 3)
			{
				if (getOsType() == VER_NT_WORKSTATION)
					l_OS += "8.1 ";
				else
					l_OS += "Server 2012 R2 ";
			}
		}
		// Product Info  https://msdn.microsoft.com/en-us/library/windows/desktop/ms724358(v=vs.85).aspx
		typedef BOOL(WINAPI * PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);
		PGPI pGPI = (PGPI)GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "GetProductInfo");
		if (pGPI)
		{
			pGPI(getOsMajor(), getOsMinor(), 0, 0, &dwType);
			switch (dwType)
			{
				case PRODUCT_ULTIMATE:
					l_OS += "Ultimate Edition";
					break;
				case PRODUCT_PROFESSIONAL:
					l_OS += "Professional";
					break;
				case PRODUCT_PROFESSIONAL_N:
					l_OS += "Professional N";
					break;
				case PRODUCT_HOME_PREMIUM:
					l_OS += "Home Premium Edition";
					break;
				case PRODUCT_HOME_BASIC:
					l_OS += "Home Basic Edition";
					break;
				case PRODUCT_ENTERPRISE:
					l_OS += "Enterprise Edition";
					break;
				case PRODUCT_BUSINESS:
					l_OS += "Business Edition";
					break;
				case PRODUCT_STARTER:
					l_OS += "Starter Edition";
					break;
#if 0
				case PRODUCT_CLUSTER_SERVER:
					l_OS += "Cluster Server Edition";
					break;
				case PRODUCT_DATACENTER_SERVER:
					l_OS += "Datacenter Edition";
					break;
				case PRODUCT_DATACENTER_SERVER_CORE:
					l_OS += "Datacenter Edition (core installation)";
					break;
				case PRODUCT_ENTERPRISE_SERVER:
					l_OS += "Enterprise Edition (server)";
					break;
				case PRODUCT_ENTERPRISE_SERVER_CORE:
					l_OS += "Enterprise Edition (core installation)";
					break;
				case PRODUCT_ENTERPRISE_SERVER_IA64:
					l_OS += "Enterprise Edition for Itanium-based Systems";
					break;
				case PRODUCT_SMALLBUSINESS_SERVER:
					l_OS += "Small Business Server";
					break;
				case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
					l_OS += "Small Business Server Premium Edition";
					break;
				case PRODUCT_STANDARD_SERVER:
					l_OS += "Standard Edition";
					break;
				case PRODUCT_STANDARD_SERVER_CORE:
					l_OS += "Standard Edition (core installation)";
					break;
				case PRODUCT_WEB_SERVER:
					l_OS += "Web Server Edition";
					break;
#endif
				case PRODUCT_UNLICENSED:
					l_OS += "Unlicensed";
					break;
					// Only Windows 10 support:
					// VC 2015 not supported these defines :(
					/*
					case PRODUCT_MOBILE_CORE:
					l_OS += "Mobile";
					break;
					case PRODUCT_MOBILE_ENTERPRISE:
					l_OS += "Mobile Enterprise";
					break;
					case PRODUCT_IOTUAP:
					l_OS += "IoT Core";
					break;
					case PRODUCT_IOTUAPCOMMERCIAL:
					l_OS += "IoT Core Commercial";
					break;
					case PRODUCT_EDUCATION:
					l_OS += "Education";
					break;
					case PRODUCT_ENTERPRISE_S:
					l_OS += "Enterprise 2015 LTSB";
					break;
					case PRODUCT_CORE:
					l_OS += "Home";
					break;
					case PRODUCT_CORE_SINGLELANGUAGE:
					l_OS += "Home Single Language";
					break;
					*/
			}
		}
	}
	if (getOsMajor() == 5)
	{
		if (getOsMinor() == 2)
		{
			if (GetSystemMetrics(SM_SERVERR2))
				l_OS += "Server 2003 R2, ";
			else if (getOsSuiteMask() & VER_SUITE_STORAGE_SERVER)
				l_OS += "Storage Server 2003";
			else if (getOsSuiteMask() & VER_SUITE_WH_SERVER)
				l_OS += "Home Server";
			else if (getOsType() == VER_NT_WORKSTATION && getProcArch() == PROCESSOR_ARCHITECTURE_AMD64)
			{
				l_OS += "XP Professional x64 Edition";
			}
			else
				l_OS += "Server 2003, ";  // Test for the server type.
#if 0
			if (getOsType() != VER_NT_WORKSTATION)
			{
				if (getProcArch() == PROCESSOR_ARCHITECTURE_IA64)
				{
					if (getOsSuiteMask() & VER_SUITE_DATACENTER)
						l_OS += "Datacenter Edition for Itanium-based Systems";
					else if (getOsSuiteMask() & VER_SUITE_ENTERPRISE)
						l_OS += "Enterprise Edition for Itanium-based Systems";
				}
				else if (getProcArch() == PROCESSOR_ARCHITECTURE_AMD64)
				{
					if (getOsSuiteMask() & VER_SUITE_DATACENTER)
						l_OS += "Datacenter x64 Edition";
					else if (getOsSuiteMask() & VER_SUITE_ENTERPRISE)
						l_OS += "Enterprise x64 Edition";
					else
						l_OS += "Standard x64 Edition";
				}
				else
				{
					if (getOsSuiteMask() & VER_SUITE_COMPUTE_SERVER)
						l_OS += "Compute Cluster Edition";
					else if (getOsSuiteMask() & VER_SUITE_DATACENTER)
						l_OS += "Datacenter Edition";
					else if (getOsSuiteMask() & VER_SUITE_ENTERPRISE)
						l_OS += "Enterprise Edition";
					else if (getOsSuiteMask() & VER_SUITE_BLADE)
						l_OS += "Web Edition";
					else
						l_OS += "Standard Edition";
				}
			}
#endif
		}
		if (getOsMinor() == 1)
		{
			l_OS += "XP ";
			if (getOsSuiteMask() & VER_SUITE_PERSONAL)
				l_OS += "Home Edition";
			else
				l_OS += "Professional";
		}
		if (getOsMinor() == 0)
		{
			l_OS += "2000 ";
			if (getOsType() == VER_NT_WORKSTATION)
			{
				l_OS += "Professional";
			}
			else
			{
#if 0
				if (getOsSuiteMask() & VER_SUITE_DATACENTER)
					l_OS += "Datacenter Server";
				else if (getOsSuiteMask() & VER_SUITE_ENTERPRISE)
					l_OS += "Advanced Server";
				else
					l_OS += "Server";
#endif
			}
		}
	}
	// Include service pack (if any) and build number.
	if (g_osvi.szCSDVersion[0] != 0)
	{
		l_OS += ' ' + Text::fromT(g_osvi.szCSDVersion);
	}
	/*  l_OS = l_OS + " (build " + g_osvi.dwBuildNumber + ")";*/
	if (g_osvi.dwMajorVersion >= 6)
	{
		if ((getProcArch() == PROCESSOR_ARCHITECTURE_AMD64) || runningIsWow64())
			l_OS += ", 64-bit";
		else if (getProcArch() == PROCESSOR_ARCHITECTURE_INTEL)
			l_OS += ", 32-bit";
	}
	return l_OS;
}
void CompatibilityManager::generateSystemInfoForApp()
{
	g_startupInfo = getFlylinkDCAppCaptionWithVersion();
	
	g_startupInfo += " startup on machine with:\r\n"
	                 "\tNumber of processors: " + Util::toString(getProcessorsCount()) + ".\r\n" +
	                 + "\tProcessor type: ";
	g_startupInfo += getProcArchString();
	g_startupInfo += ".\r\n";
	
	g_startupInfo += generateGlobalMemoryStatusMessage();
	g_startupInfo += "\r\n";
	
	g_startupInfo += "\tRunning in ";
#ifndef _WIN64
	if (runningIsWow64())
	{
		g_startupInfo += "Windows WOW64 ";
		g_startupInfo += "\r\n\r\n\tFor maximal performance needs to update your FlylinkDC to x64 version!";
	}
	else
#endif
		g_startupInfo += "Windows native ";
		
	g_startupInfo += "\r\n\t";
	g_startupInfo += getWindowsVersionName();   //getFormatedOsVersion();
	g_startupInfo += "  (" + CompatibilityManager::getFormatedOsVersion() + ")";
	
#ifdef FLYLINKDC_USE_CHECK_OLD_OS
	if (runningAnOldOS())
		g_startupInfo += " - incompatible OS!";
#endif
	g_startupInfo += "\r\n\r\n";
	
	// g_startupInfo.shrink_to_fit(); // странно падение https://drdump.com/DumpGroup.aspx?DumpGroupID=464486
}

LONG CompatibilityManager::getComCtlVersionFromOS()
{
	DWORD dwMajor = 0, dwMinor = 0;
	if (SUCCEEDED(ATL::AtlGetCommCtrlVersion(&dwMajor, &dwMinor)))
		return MAKELONG(dwMinor, dwMajor);
	return 0;
}

bool CompatibilityManager::getFromSystemIsAppRunningIsWow64()
{
	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms684139(v=vs.85).aspx
	
	auto kernel32 = GetModuleHandle(_T("kernel32"));
	
	if (kernel32)
	{
		typedef BOOL (WINAPI * LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
		LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(kernel32, "IsWow64Process");
		BOOL bIsWow64;
		if (fnIsWow64Process)
		{
			if (fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
			{
				return bIsWow64 != FALSE;
			}
		}
	}
	return false;
}

bool CompatibilityManager::getGlobalMemoryStatusFromOS(MEMORYSTATUSEX* p_MsEx)
{
	// http://msdn.microsoft.com/en-us/library/windows/desktop/aa366770(v=vs.85).aspx
	
	auto kernel32 = GetModuleHandle(_T("kernel32"));
	if (kernel32)
	{
		typedef BOOL (WINAPI * LPFN_GLOBALMEMORYSTATUSEX)(LPMEMORYSTATUSEX);
		
		LPFN_GLOBALMEMORYSTATUSEX fnGlobalMemoryStatusEx;
		
		fnGlobalMemoryStatusEx = (LPFN_GLOBALMEMORYSTATUSEX) GetProcAddress(kernel32, "GlobalMemoryStatusEx");
		
		if (fnGlobalMemoryStatusEx != nullptr)
		{
			return fnGlobalMemoryStatusEx(p_MsEx) != FALSE;
		}
	}
	return false;
}

string CompatibilityManager::generateGlobalMemoryStatusMessage()
{
	MEMORYSTATUSEX curMemoryInfo = {0};
	curMemoryInfo.dwLength = sizeof(curMemoryInfo);
	
	if (getGlobalMemoryStatusFromOS(&curMemoryInfo))
	{
		g_TotalPhysMemory = curMemoryInfo.ullTotalPhys;
		string memoryInfo = "\tMemory config:\r\n";
		memoryInfo += "\t\tThere is\t" + Util::toString(curMemoryInfo.dwMemoryLoad) + " percent of memory in use.\r\n";
		memoryInfo += "\t\tThere are\t" + Util::formatBytes(curMemoryInfo.ullTotalPhys) + " total of physical memory.\r\n";
		memoryInfo += "\t\tThere are\t" + Util::formatBytes(curMemoryInfo.ullAvailPhys) + " free of physical memory.\r\n";
		return memoryInfo;
	}
	return Util::emptyString;
}

float CompatibilityManager::ProcSpeedCalc() // moved form WinUtil.
{
#if (defined(_M_ARM) || defined(_M_ARM64))
	return 0;
#else
	__int64 cyclesStart = 0, cyclesStop = 0;
	unsigned __int64 nCtr = 0, nFreq = 0, nCtrStop = 0;
	if (!QueryPerformanceFrequency((LARGE_INTEGER *) &nFreq)) return 0;
	QueryPerformanceCounter((LARGE_INTEGER *) &nCtrStop);
	nCtrStop += nFreq;
	cyclesStart = __rdtsc();
	do
	{
		QueryPerformanceCounter((LARGE_INTEGER *) &nCtr);
	}
	while (nCtr < nCtrStop);
	cyclesStop = __rdtsc();
	return float(cyclesStop - cyclesStart) / 1000000.0;
#endif
}

string CompatibilityManager::generateFullSystemStatusMessage()
{
	return
	    getStartupInfo() +
	    STRING(CURRENT_SYSTEM_STATE) + ":\r\n" + generateGlobalMemoryStatusMessage() +
	    '\t' + STRING(CPU_SPEED) + ": " + Util::toString(ProcSpeedCalc()) + " MHz" +
	    getIncompatibleSoftwareMessage() + "\r\n" +
	    CFlylinkDBManager::get_db_size_info();
}

string CompatibilityManager::generateNetworkStats()
{
	std::vector<char> l_buf(1024);
	sprintf_s(l_buf.data(), l_buf.size(),
	          "-=[ TCP: Downloaded: %s. Uploaded: %s ]=-\r\n"
	          "-=[ UDP: Downloaded: %s. Uploaded: %s ]=-\r\n"
	          // TODO "-=[ Torrent: Downloaded: %s. Uploaded: %s ]=-\r\n"
	          "-=[ SSL: Downloaded: %s. Uploaded: %s ]=-\r\n",
	          Util::formatBytes(Socket::g_stats.m_tcp.totalDown).c_str(), Util::formatBytes(Socket::g_stats.m_tcp.totalUp).c_str(),
	          Util::formatBytes(Socket::g_stats.m_udp.totalDown).c_str(), Util::formatBytes(Socket::g_stats.m_udp.totalUp).c_str(),
	          // TODO Util::formatBytes(Socket::g_stats.m_dht.totalDown).c_str(), Util::formatBytes(Socket::g_stats.m_dht.totalUp).c_str(),
	          Util::formatBytes(Socket::g_stats.m_ssl.totalDown).c_str(), Util::formatBytes(Socket::g_stats.m_ssl.totalUp).c_str()
	         );
	return l_buf.data();
}
void CompatibilityManager::caclPhysMemoryStat()
{
	// Total RAM
	MEMORYSTATUSEX curMem = {0};
	curMem.dwLength = sizeof(curMem);
	g_FreePhysMemory = 0;
	g_TotalPhysMemory = 0;
	if (getGlobalMemoryStatusFromOS(&curMem))
	{
		g_TotalPhysMemory = curMem.ullTotalPhys;
		g_FreePhysMemory = curMem.ullAvailPhys;
	}
}
string CompatibilityManager::generateProgramStats() // moved form WinUtil.
{
	std::vector<char> l_buf(1024 * 2);
	{
		const HINSTANCE hInstPsapi = LoadLibrary(_T("psapi"));
		if (hInstPsapi)
		{
			typedef bool (CALLBACK * LPFUNC)(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);
			LPFUNC _GetProcessMemoryInfo = (LPFUNC)GetProcAddress(hInstPsapi, "GetProcessMemoryInfo");
			if (_GetProcessMemoryInfo)
			{
				PROCESS_MEMORY_COUNTERS l_pmc = {0};
				l_pmc.cb = sizeof(l_pmc);
				_GetProcessMemoryInfo(GetCurrentProcess(), &l_pmc, sizeof(l_pmc));
				extern int g_RAM_PeakWorkingSetSize;
				extern int g_RAM_WorkingSetSize;
				g_RAM_WorkingSetSize = l_pmc.WorkingSetSize / 1024 / 1024;
				g_RAM_PeakWorkingSetSize = l_pmc.PeakWorkingSetSize / 1024 / 1024;
				caclPhysMemoryStat();
				string l_procs;
				const int digit_procs = getProcessorsCount();
				if (digit_procs > 1)
					l_procs = " x" + Util::toString(getProcessorsCount()) + " core(s)";
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
				dcassert(CFlylinkDBManager::isValidInstance());
				if (CFlylinkDBManager::isValidInstance())
				{
					CFlylinkDBManager::getInstance()->load_global_ratio();
				}
#endif
				sprintf_s(l_buf.data(), l_buf.size(),
				          "\r\n\t-=[ %s " //-V111
				          "Compiled on: %s ]=-\r\n"
				          "\t-=[ OS: %s ]=-\r\n"
				          "\t-=[ CPU Clock: %.1f MHz%s. Memory (free): %s (%s) ]=-\r\n"
				          "\t-=[ Uptime: %s. Client Uptime: %s ]=-\r\n"
				          "\t-=[ RAM (peak): %s (%s). Virtual (peak): %s (%s) ]=-\r\n"
				          "\t-=[ GDI units (peak): %d (%d). Handle (peak): %d (%d) ]=-\r\n"
				          "\t-=[ Share: %s. Files in share: %u. Total users: %u on hubs: %u ]=-\r\n"
				          "\t-=[ TigerTree cache: %u Search not exists cache: %u Search exists cache: %u]=-\r\n"
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
				          "\t-=[ Total download: %s. Total upload: %s ]=-\r\n"
#endif
				          "%s"
				          ,
				          getFlylinkDCAppCaptionWithVersion().c_str(),
				          Text::fromT(Util::getCompileDate()).c_str(),
				          CompatibilityManager::getWindowsVersionName().c_str(),
				          CompatibilityManager::ProcSpeedCalc(),
				          l_procs.c_str(),
				          Util::formatBytes(g_TotalPhysMemory).c_str(),
				          Util::formatBytes(g_FreePhysMemory).c_str(),
				          Util::formatTime(
#ifdef FLYLINKDC_SUPPORT_WIN_XP
				              GetTickCount()
#else
				              GetTickCount64()
#endif
				              / 1000).c_str(),
				          Util::formatTime(Util::getUpTime()).c_str(),
				          Util::formatBytes(l_pmc.WorkingSetSize).c_str(),
				          Util::formatBytes(l_pmc.PeakWorkingSetSize).c_str(),
				          Util::formatBytes(l_pmc.PagefileUsage).c_str(),
				          Util::formatBytes(l_pmc.PeakPagefileUsage).c_str(),
				          GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS),
				          GetGuiResources(GetCurrentProcess(), 2/* GR_GDIOBJECTS_PEAK */),
				          GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS),
				          GetGuiResources(GetCurrentProcess(), 4 /*GR_USEROBJECTS_PEAK*/),
				          Util::formatBytes(ShareManager::getShareSize()).c_str(),
				          ShareManager::getLastSharedFiles(),
				          ClientManager::getTotalUsers(),
				          Client::getTotalCounts(),
				          CFlylinkDBManager::get_tth_cache_size(),
				          ShareManager::get_cache_size_file_not_exists_set(),
				          ShareManager::get_cache_file_map(),
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
				          Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_download()).c_str(),
				          Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_upload()).c_str(),
#endif
				          generateNetworkStats().c_str()
				         );
			}
			FreeLibrary(hInstPsapi);
		}
	}
	return l_buf.data();
}

WORD CompatibilityManager::getDllPlatform(const string& fullpath)
{
	WORD bRet = IMAGE_FILE_MACHINE_UNKNOWN;
	PLOADED_IMAGE imgLoad = ::ImageLoad(Text::fromUtf8(fullpath).c_str(), Util::emptyString.c_str());
	if (imgLoad && imgLoad->FileHeader)
	{
		bRet = imgLoad->FileHeader->FileHeader.Machine;
	}
	if (imgLoad)
	{
		::ImageUnload(imgLoad);
	}
	
	return bRet;
}

void CompatibilityManager::reduceProcessPriority()
{
	if (!g_oldPriorityClass || g_oldPriorityClass == GetPriorityClass(GetCurrentProcess()))
	{
		g_oldPriorityClass = GetPriorityClass(GetCurrentProcess());
		SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
	}
}

void CompatibilityManager::restoreProcessPriority()
{
	if (g_oldPriorityClass)
		SetPriorityClass(GetCurrentProcess(), g_oldPriorityClass);
}

bool CompatibilityManager::checkTeredo()
{
	// http://blog.cherepovets.ru/serovds/2013/07/30/teredo-winxp/
	g_is_ipv6_enabled = false;
	{
		SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		if (INVALID_SOCKET != s)
		{
			closesocket(s);
			g_is_ipv6_enabled = true;
		}
	}
	bool l_result = false;
	// https://msdn.microsoft.com/en-us/library/aa365915.aspx
#define MALLOC(x) HeapAlloc(GetProcessHeap(), 0, (x))
#define FREE(x) HeapFree(GetProcessHeap(), 0, (x))
	
	const unsigned WORKING_BUFFER_SIZE = 15000;
	const unsigned MAX_TRIES = 3;
	
	DWORD dwRetVal = 0;
	
	// Set the flags to pass to GetAdaptersAddresses
	ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
	
	// default to unspecified address family (both)
	ULONG family = AF_UNSPEC;
	//        family = AF_INET;
	//        family = AF_INET6;
	
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;
	ULONG outBufLen = 0;
	ULONG Iterations = 0;
	
	PIP_ADAPTER_ADDRESSES pCurrAddresses = NULL;
	//PIP_ADAPTER_UNICAST_ADDRESS pUnicast = NULL;
	//PIP_ADAPTER_ANYCAST_ADDRESS pAnycast = NULL;
	//PIP_ADAPTER_MULTICAST_ADDRESS pMulticast = NULL;
	//IP_ADAPTER_DNS_SERVER_ADDRESS *pDnServer = NULL;
	//IP_ADAPTER_PREFIX *pPrefix = NULL;
	
	
	/*
	printf("Calling GetAdaptersAddresses function with family = ");
	if (family == AF_INET)
	printf("AF_INET\n");
	if (family == AF_INET6)
	printf("AF_INET6\n");
	if (family == AF_UNSPEC)
	printf("AF_UNSPEC\n\n");
	*/
	// Allocate a 15 KB buffer to start with.
	outBufLen = WORKING_BUFFER_SIZE;
	
	do
	{
	
		pAddresses = (IP_ADAPTER_ADDRESSES *)MALLOC(outBufLen);
		if (pAddresses == NULL)
		{
			dcassert(0);
			return false;
		}
		
		dwRetVal =
		    GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);
		    
		if (dwRetVal == ERROR_BUFFER_OVERFLOW)
		{
			FREE(pAddresses);
			pAddresses = NULL;
		}
		else
		{
			break;
		}
		
		Iterations++;
		
	}
	while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_TRIES));
	
	if (dwRetVal == NO_ERROR)
	{
		// If successful, output some information from the data we received
		pCurrAddresses = pAddresses;
		while (pCurrAddresses)
		{
			//printf("\tLength of the IP_ADAPTER_ADDRESS struct: %ld\n",
			//       pCurrAddresses->Length);
			//printf("\tIfIndex (IPv4 interface): %u\n", pCurrAddresses->IfIndex);
			/*
			printf("\tAdapter name: %s\n", pCurrAddresses->AdapterName);
			
			pUnicast = pCurrAddresses->FirstUnicastAddress;
			if (pUnicast != NULL) {
			for (i = 0; pUnicast != NULL; i++)
			pUnicast = pUnicast->Next;
			printf("\tNumber of Unicast Addresses: %d\n", i);
			} else
			printf("\tNo Unicast Addresses\n");
			
			pAnycast = pCurrAddresses->FirstAnycastAddress;
			if (pAnycast) {
			for (i = 0; pAnycast != NULL; i++)
			pAnycast = pAnycast->Next;
			printf("\tNumber of Anycast Addresses: %d\n", i);
			} else
			printf("\tNo Anycast Addresses\n");
			
			pMulticast = pCurrAddresses->FirstMulticastAddress;
			if (pMulticast) {
			for (i = 0; pMulticast != NULL; i++)
			pMulticast = pMulticast->Next;
			printf("\tNumber of Multicast Addresses: %d\n", i);
			} else
			printf("\tNo Multicast Addresses\n");
			
			pDnServer = pCurrAddresses->FirstDnsServerAddress;
			if (pDnServer) {
			for (i = 0; pDnServer != NULL; i++)
			pDnServer = pDnServer->Next;
			printf("\tNumber of DNS Server Addresses: %d\n", i);
			} else
			printf("\tNo DNS Server Addresses\n");
			*/
			//TOOD printf("\tDNS Suffix: %wS\n", pCurrAddresses->DnsSuffix);
			
			//if (pCurrAddresses->Ipv6Enabled)
			//{
			//  g_is_ipv6_enabled |= true;
			//}
			if (wcscmp(pCurrAddresses->FriendlyName, _T("Teredo Tunneling Pseudo-Interface")) == 0)
			{
				l_result = true;
				break;
			}
			
			//printf("\tDescription: %wS\n", pCurrAddresses->Description);
			//printf("\tFriendly name: %wS\n", pCurrAddresses->FriendlyName);
			
			/*
			if (pCurrAddresses->PhysicalAddressLength != 0) {
			printf("\tPhysical address: ");
			for (i = 0; i < (int) pCurrAddresses->PhysicalAddressLength;
			i++) {
			if (i == (pCurrAddresses->PhysicalAddressLength - 1))
			printf("%.2X\n",
			(int) pCurrAddresses->PhysicalAddress[i]);
			else
			printf("%.2X-",
			(int) pCurrAddresses->PhysicalAddress[i]);
			}
			}
			*/
			// printf("\tFlags: %ld\n", pCurrAddresses->Flags);
			// printf("\tFlags.Ipv6Enabled: %d\n", int(pCurrAddresses->Ipv6Enabled));
			// TODO printf("\tMtu: %lu\n", pCurrAddresses->Mtu);
			// TODO printf("\tIfType: %ld\n", pCurrAddresses->IfType);
			// TODO printf("\tOperStatus: %ld\n", pCurrAddresses->OperStatus);
			// TODO printf("\tIpv6IfIndex (IPv6 interface): %u\n",
			// TODO        pCurrAddresses->Ipv6IfIndex);
			/*
			printf("\tZoneIndices (hex): ");
			for (i = 0; i < 16; i++)
			printf("%lx ", pCurrAddresses->ZoneIndices[i]);
			printf("\n");
			
			printf("\tTransmit link speed: %I64u\n", pCurrAddresses->TransmitLinkSpeed);
			printf("\tReceive link speed: %I64u\n", pCurrAddresses->ReceiveLinkSpeed);
			
			pPrefix = pCurrAddresses->FirstPrefix;
			if (pPrefix) {
			for (i = 0; pPrefix != NULL; i++)
			pPrefix = pPrefix->Next;
			printf("\tNumber of IP Adapter Prefix entries: %d\n", i);
			} else
			printf("\tNumber of IP Adapter Prefix entries: 0\n");
			*/
			//printf("\n");
			
			pCurrAddresses = pCurrAddresses->Next;
		}
	}
	else
	{
		//printf("Call to GetAdaptersAddresses failed with error: %d\n",dwRetVal);
		if (dwRetVal == ERROR_NO_DATA)
		{
			//  printf("\tNo addresses were found for the requested parameters\n");
		}
		else
		{
			dcassert(0);
			//LPVOID lpMsgBuf = NULL;
			
			/*
			if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, dwRetVal, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			// Default language
			(LPTSTR)& lpMsgBuf, 0, NULL))
			{
			//printf("\tError: %s", lpMsgBuf);
			LocalFree(lpMsgBuf);
			}
			*/
		}
	}
	
	if (pAddresses)
	{
		FREE(pAddresses);
	}
	return l_result;
}

// AirDC++ code
// ToDo:  Move all functions into Util
string CompatibilityManager::Speedinfo()
{
	string result = "\r\n\t";
	result += "-=[ ";
	result += "Dn. speed: ";
	result += Util::formatBytes(DownloadManager::getRunningAverage()) + "/s  (";
	result += Util::toString(DownloadManager::getDownloadCount()) + " fls.)";
	//result += " =- ";
	//result += " -= ";
	result += ". ";
	result += "Upl. speed: ";
	result += Util::formatBytes(UploadManager::getRunningAverage()) + "/s  (";
	result += Util::toString(UploadManager::getUploadCount()) + " fls.)";
	result += " ]=-";
	return result;
}

string CompatibilityManager::DiskSpaceInfo(bool onlyTotal /* = false */)
{
	string ret;
#ifdef FLYLINKDC_USE_DISK_SPACE_INFO
	int64_t free = 0, totalFree = 0, size = 0, totalSize = 0, netFree = 0, netSize = 0;
	const TStringList volumes = FindVolumes();
	for (auto i = volumes.cbegin(); i != volumes.cend(); ++i)
	{
		const auto l_drive_type = GetDriveType((*i).c_str());
		if (l_drive_type == DRIVE_CDROM || l_drive_type == DRIVE_REMOVABLE) // Not score USB flash, SD, SDMC, DVD, CD
			continue;
		if (GetDiskFreeSpaceEx((*i).c_str(), NULL, (PULARGE_INTEGER)&size, (PULARGE_INTEGER)&free))
		{
			totalFree += free;
			totalSize += size;
		}
	}
	
	//check for mounted Network drives
	ULONG drives = _getdrives();
	TCHAR drive[3] = { _T('C'), _T(':'), _T('\0') }; // TODO фиксануть copy-paste
	while (drives != 0)
	{
		const auto l_drive_type = GetDriveType(drive);
		if (drives & 1 && (l_drive_type != DRIVE_CDROM && l_drive_type != DRIVE_REMOVABLE && l_drive_type == DRIVE_REMOTE)) // TODO
		{
			if (GetDiskFreeSpaceEx(drive, NULL, (PULARGE_INTEGER)&size, (PULARGE_INTEGER)&free))
			{
				netFree += free;
				netSize += size;
			}
			
		}
		++drive[0];
		drives = (drives >> 1);
	}
	if (totalSize != 0)
		if (!onlyTotal)
		{
			ret += "\r\n\t-=[ All HDD space (free/total): " + Util::formatBytes(totalFree) + "/" + Util::formatBytes(totalSize) + " ]=-";
			if (netSize != 0)
			{
				ret += "\r\n\t-=[ Network space (free/total): " + Util::formatBytes(netFree) + "/" + Util::formatBytes(netSize) + " ]=-";
				ret += "\r\n\t-=[ Network + HDD space (free/total): " + Util::formatBytes((netFree + totalFree)) + "/" + Util::formatBytes(netSize + totalSize) + " ]=-";
			}
		}
		else
		{
			ret += Util::formatBytes(totalFree) + "/" + Util::formatBytes(totalSize);
		}
#endif // FLYLINKDC_USE_DISK_SPACE_INFO
	return ret;
}
TStringList CompatibilityManager::FindVolumes()
{
	TCHAR   buf[MAX_PATH];
	buf[0] = 0;
	TStringList volumes;
	HANDLE hVol = FindFirstVolume(buf, MAX_PATH);
	if (hVol != INVALID_HANDLE_VALUE)
	{
		volumes.push_back(buf);
		BOOL found = FindNextVolume(hVol, buf, MAX_PATH);
		//while we find drive volumes.
		while (found)
		{
			volumes.push_back(buf);
			found = FindNextVolume(hVol, buf, MAX_PATH);
		}
		found = FindVolumeClose(hVol);
	}
	return volumes;
}
tstring CompatibilityManager::diskInfo()
{
	tstring result;
#ifdef FLYLINKDC_USE_DISK_SPACE_INFO
	int64_t free = 0, size = 0, totalFree = 0, totalSize = 0;
	int disk_count = 0;
	std::vector<tstring> results; //add in vector for sorting, nicer to look at :)
	// lookup drive volumes.
	TStringList volumes = FindVolumes();
	for (auto i = volumes.cbegin(); i != volumes.cend(); ++i)
	{
		const auto l_drive_type = GetDriveType((*i).c_str());
		if (l_drive_type == DRIVE_CDROM || l_drive_type == DRIVE_REMOVABLE) // Not score USB flash, SD, SDMC, DVD, CD
			continue;
		TCHAR   buf[MAX_PATH];
		buf[0] = 0;
		if ((GetVolumePathNamesForVolumeName((*i).c_str(), buf, 256, NULL) != 0) &&
		        (GetDiskFreeSpaceEx((*i).c_str(), NULL, (PULARGE_INTEGER)&size, (PULARGE_INTEGER)&free) != 0))
		{
			const tstring mountpath = buf;
			if (!mountpath.empty())
			{
				totalFree += free;
				totalSize += size;
				results.push_back((_T("\t-=[ Disk ") + mountpath + _T(" space (free/total): ") + Util::formatBytesW(free) + _T("/") + Util::formatBytesW(size) + _T(" ]=-")));
			}
		}
	}
	
	// and a check for mounted Network drives, todo fix a better way for network space
	ULONG drives = _getdrives();
	TCHAR drive[3] = { _T('C'), _T(':'), _T('\0') };
	
	while (drives != 0)
	{
		const auto l_drive_type = GetDriveType(drive);
		if (drives & 1 && (l_drive_type != DRIVE_CDROM && l_drive_type != DRIVE_REMOVABLE && l_drive_type == DRIVE_REMOTE)) // TODO
		{
			if (GetDiskFreeSpaceEx(drive, NULL, (PULARGE_INTEGER)&size, (PULARGE_INTEGER)&free))
			{
				totalFree += free;
				totalSize += size;
				results.push_back((_T("\t-=[ Network ") + (tstring)drive + _T(" space (free/total): ") + Util::formatBytesW(free) + _T("/") + Util::formatBytesW(size) + _T(" ]=-")));
			}
		}
		++drive[0];
		drives = (drives >> 1);
	}
	
	sort(results.begin(), results.end()); //sort it
	for (auto i = results.cbegin(); i != results.end(); ++i)
	{
		disk_count++;
		result += _T("\r\n ") + *i;
	}
	result += _T("\r\n\r\n\t-=[ All HDD space (free/total): ") + Util::formatBytesW((totalFree)) + _T("/") + Util::formatBytesW(totalSize) + _T(" ]=-");
	result += _T("\r\n\t-=[ Total Drives count: ") + Text::toT(Util::toString(disk_count)) + _T(" ]=-");
	results.clear();
#endif // FLYLINKDC_USE_DISK_SPACE_INFO
	return result;
}

string CompatibilityManager::CPUInfo()
{
	tstring result;
	CRegKey key;
	ULONG len = 255;
	if (key.Open(HKEY_LOCAL_MACHINE, _T("Hardware\\Description\\System\\CentralProcessor\\0"), KEY_READ) == ERROR_SUCCESS)
	{
		TCHAR buf[255];
		buf[0] = 0;
		if (key.QueryStringValue(_T("ProcessorNameString"), buf, &len) == ERROR_SUCCESS)
		{
			tstring tmp = buf;
			result = tmp.substr(tmp.find_first_not_of(_T(' ')));
		}
		DWORD speed;
		if (key.QueryDWORDValue(_T("~MHz"), speed) == ERROR_SUCCESS)
		{
			result += _T(" (");
			result += Util::toStringW((uint32_t)speed);
			result += _T(" MHz)");
		}
	}
	return result.empty() ? "Unknown" : Text::fromT(result);
}
string CompatibilityManager::getSysUptime()
{

	static HINSTANCE kernel32lib = NULL;
	if (!kernel32lib)
		kernel32lib = LoadLibrary(_T("kernel32"));
		
	//apexdc
	typedef ULONGLONG(CALLBACK * LPFUNC2)(void);
	LPFUNC2 _GetTickCount64 = (LPFUNC2)GetProcAddress(kernel32lib, "GetTickCount64");
	time_t sysUptime = (_GetTickCount64 ? _GetTickCount64() : GetTickCount()) / 1000;
	
	return Util::formatTime(sysUptime, false);
	
}