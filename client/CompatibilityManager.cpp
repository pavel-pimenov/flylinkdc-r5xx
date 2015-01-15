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

#  if defined(_MSC_VER) && _MSC_VER >= 1800 && defined(_M_X64)
#    include <math.h> /* needed for _set_FMA3_enable */
#  endif

#include <WinError.h>
#include <winnt.h>
#include <ImageHlp.h>
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
#define PSAPI_VERSION 1
#endif
#include <psapi.h>
#if _MSC_VER >= 1700
#include <atlbase.h>
#include <atlapp.h>
#endif
#include "Util.h"
#include "Socket.h"
#include "CompatibilityManager.h"
#include "CFlylinkDBManager.h"
#include "ShareManager.h"
#include "../FlyFeatures/flyServer.h"
#include <iphlpapi.h>

#pragma comment(lib, "Imagehlp.lib")

string CompatibilityManager::g_incopatibleSoftwareList;
string CompatibilityManager::g_startupInfo;
DWORDLONG CompatibilityManager::g_TotalPhysMemory;
OSVERSIONINFOEX CompatibilityManager::g_osvi = {0};
SYSTEM_INFO CompatibilityManager::g_sysInfo = {0};
bool CompatibilityManager::g_supports[LAST_SUPPORTS];
LONG CompatibilityManager::g_comCtlVersion = 0;
DWORD CompatibilityManager::g_oldPriorityClass = 0;
string CompatibilityManager::g_upnp_router_model;
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
#if _MSC_VER >= 1800
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
	(getOsMajor() >= future_major_version)
		
#define FUTURE_MINOR_VER(current_major_version, future_minor_version) \
	(getOsMajor() == current_major_version && getOsMinor() >= future_minor_version)
		
#define CURRENT_VER(current_major_version, current_minor_version) \
	(getOsMajor() == current_major_version && getOsMinor() == current_minor_version)
		
#define CURRENT_VER_SP(current_major_version, current_minor_version, current_sp_version) \
	(getOsMajor() == current_major_version && getOsMinor() == current_minor_version && getOsSpMajor() == current_sp_version)
		
	if (FUTURE_VER(7) || // future version
	        FUTURE_MINOR_VER(6, 3) || // future version
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
	    !CURRENT_VER_SP(6, 3, 0))   // Windows 8.1 & Windows Server 2012 R2 http://en.wikipedia.org/wiki/Windows_8.1 http://ru.wikipedia.org/wiki/Windows_Server_2012
// TODO - Windows 10
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

// [+] PPA
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

static const DllInfo g_IncompatibleDll[] =
{
	_T("nvappfilter.dll"), "NVIDIA ActiveArmor (NVIDIA Firewall)",
	_T("nvlsp.dll"), "NVIDIA Application Filter",
	_T("nvlsp64.dll"), "NVIDIA Application Filter",
	_T("adguard.dll"), "Adguard", // Latest verion 5.1 not bad. TODO: http://code.google.com/p/flylinkdc/issues/detail?id=606
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
	// TODO http://code.google.com/p/flylinkdc/issues/detail?id=606 http://stackoverflow.com/questions/420185/how-to-get-the-version-info-of-a-dll-in-c
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
				l_OS += "Server 2012 R2";
		}
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
					l_OS += "Enterprise Edition";
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
				if (getOsSuiteMask() & VER_SUITE_DATACENTER)
					l_OS += "Datacenter Server";
				else if (getOsSuiteMask() & VER_SUITE_ENTERPRISE)
					l_OS += "Advanced Server";
				else
					l_OS += "Server";
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
	g_startupInfo =
	    A_APPNAME_WITH_VERSION " "
#ifdef FLYLINKDC_HE
	    "HE "
#endif
	    "startup on machine with:\r\n"
	    "\tNumber of processors: " + Util::toString(getProcessorsCount()) + ".\r\n" +
#ifdef FLYLINKDC_HE
	    + "\tPage size: " + Util::toString(getPageSize()) + " Bytes.\r\n" +
#endif
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
		g_startupInfo += "\r\n\r\nFor maximal performance needs to update your FlylinkDC to x64 version!";
	}
	else
#endif
		g_startupInfo += "Windows native ";
		
	g_startupInfo += "\r\n\r\nOS: ";
	g_startupInfo += getWindowsVersionName();   //getFormatedOsVersion();
	
#ifdef FLYLINKDC_USE_CHECK_OLD_OS
	if (runningAnOldOS())
		g_startupInfo += " - incompatible OS!";
#endif
	if (!CompatibilityManager::g_upnp_router_model.empty())
	{
		g_startupInfo += "Router model: " + CompatibilityManager::g_upnp_router_model;
	}
	g_startupInfo += ").\r\n\r\n";
	
	g_startupInfo.shrink_to_fit();
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
}

string CompatibilityManager::generateFullSystemStatusMessage()
{
	return
	    getStartupInfo() +
	    STRING(CURRENT_SYSTEM_STATE) + ":\r\n" + generateGlobalMemoryStatusMessage() +
	    '\t' + STRING(CPU_SPEED) + ": " + Util::toString(ProcSpeedCalc()) + " MHz" +
	    getIncompatibleSoftwareMessage() + "\r\n" +
	    CFlylinkDBManager::getDBSizeInfo();
}

string CompatibilityManager::generateNetworkStats()
{
	std::vector<char> l_buf(1024);
	sprintf_s(l_buf.data(), l_buf.size(),
	          "-=[ TCP: Downloaded: %s. Uploaded: %s ]=-\r\n"
	          "-=[ UDP: Downloaded: %s. Uploaded: %s ]=-\r\n"
	          "-=[ DHT: Downloaded: %s. Uploaded: %s ]=-\r\n"
	          "-=[ SSL: Downloaded: %s. Uploaded: %s ]=-\r\n"
	          "-=[ Router: %s ]=-",
	          Util::formatBytes(Socket::g_stats.m_tcp.totalDown).c_str(), Util::formatBytes(Socket::g_stats.m_tcp.totalUp).c_str(),
	          Util::formatBytes(Socket::g_stats.m_udp.totalDown).c_str(), Util::formatBytes(Socket::g_stats.m_udp.totalUp).c_str(),
	          Util::formatBytes(Socket::g_stats.m_dht.totalDown).c_str(), Util::formatBytes(Socket::g_stats.m_dht.totalUp).c_str(),
	          Util::formatBytes(Socket::g_stats.m_ssl.totalDown).c_str(), Util::formatBytes(Socket::g_stats.m_ssl.totalUp).c_str(),
	          g_upnp_router_model.c_str()
	         );
	return l_buf.data();
}

string CompatibilityManager::generateProgramStats() // moved form WinUtil.
{
	std::vector<char> l_buf(2048);
	{
		const HINSTANCE hInstPsapi = LoadLibrary(_T("psapi"));
		if (hInstPsapi)
		{
			typedef bool (CALLBACK * LPFUNC)(HANDLE Process, PPROCESS_MEMORY_COUNTERS ppsmemCounters, DWORD cb);
			LPFUNC _GetProcessMemoryInfo = (LPFUNC)GetProcAddress(hInstPsapi, "GetProcessMemoryInfo");
			if (_GetProcessMemoryInfo)
			{
				PROCESS_MEMORY_COUNTERS pmc = {0};
				pmc.cb = sizeof(pmc);
				_GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
				// Total RAM
				MEMORYSTATUSEX curMem = {0};
				curMem.dwLength = sizeof(curMem);
				DWORDLONG l_FreePhysMemory = 0;
				if (getGlobalMemoryStatusFromOS(&curMem))
				{
					g_TotalPhysMemory = curMem.ullTotalPhys;
					l_FreePhysMemory = curMem.ullAvailPhys;
				}
				FILETIME tmpa = {0};
				FILETIME tmpb = {0};
				FILETIME kernelTimeFT = {0};
				FILETIME userTimeFT = {0};
				GetProcessTimes(GetCurrentProcess(), &tmpa, &tmpb, &kernelTimeFT, &userTimeFT);
				const int64_t kernelTime = kernelTimeFT.dwLowDateTime | (((int64_t)kernelTimeFT.dwHighDateTime) << 32); //-V112
				const int64_t userTime = userTimeFT.dwLowDateTime | (((int64_t)userTimeFT.dwHighDateTime) << 32); //-V112
				string l_procs;
				const int digit_procs = getProcessorsCount();
				if (digit_procs > 1)
					l_procs = " x" + Util::toString(getProcessorsCount()) + " core(s)";
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
				dcassert(CFlylinkDBManager::isValidInstance());
				if (CFlylinkDBManager::isValidInstance())
				{
					CFlylinkDBManager::getInstance()->load_global_ratio(); // fix http://code.google.com/p/flylinkdc/issues/detail?id=1363
				}
#endif
				sprintf_s(l_buf.data(), l_buf.size(), "\r\n-=[ FlylinkDC++ %s " //-V111
#ifdef FLYLINKDC_HE
				          "HE "
#endif
				          "Compiled on: %s ]=-\r\n"
				          "-=[ OS: %s ]=-\r\n"
				          "-=[ CPU Clock: %.1f MHz%s. Memory (free): %s (%s) ]=-\r\n"
				          "-=[ Uptime: %s. Cpu time: %s. System Uptime: %s ]=-\r\n"
				          "-=[ Memory usage (peak): %s (%s). Virtual (peak): %s (%s) ]=-\r\n"
				          "-=[ GDI units (peak): %d (%d). Handle (peak): %d (%d) ]=-\r\n"
				          "-=[ Public share: %s. Files in share: %u ]=-\r\n"
				          "-=[ Total users: %u on hubs: %u ]=-\r\n"
				          "%s\r\n"
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
				          "\r\n-=[ Total download: %s. Total upload: %s ]=-"
#endif
				          "\r\n",
				          A_VERSIONSTRING, Text::fromT(Util::getCompileDate()).c_str(),
				          CompatibilityManager::getWindowsVersionName().c_str(),
				          //CompatibilityManager::getProcArchString().c_str(),
				          CompatibilityManager::ProcSpeedCalc(), l_procs.c_str(), Util::formatBytes(g_TotalPhysMemory).c_str(), Util::formatBytes(l_FreePhysMemory).c_str(),
				          Util::formatTime(Util::getUpTime()).c_str(), Util::formatSeconds((kernelTime + userTime) / (10I64 * 1000I64 * 1000I64)).c_str(),
				          Util::formatTime(
#ifdef FLYLINKDC_SUPPORT_WIN_XP
				              GetTickCount()
#else
				              GetTickCount64()
#endif
				              / 1000).c_str(),
				          Util::formatBytes(pmc.WorkingSetSize).c_str(),
				          Util::formatBytes(pmc.PeakWorkingSetSize).c_str(),
				          Util::formatBytes(pmc.PagefileUsage).c_str(),
				          Util::formatBytes(pmc.PeakPagefileUsage).c_str(),
				          GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS),
				          GetGuiResources(GetCurrentProcess(), 2/* GR_GDIOBJECTS_PEAK */),
				          GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS),
				          GetGuiResources(GetCurrentProcess(), 4 /*GR_USEROBJECTS_PEAK*/),
				          Util::formatBytes(ShareManager::getInstance()->getSharedSize()).c_str(),
				          ShareManager::getInstance()->getSharedFiles(),
				          ClientManager::getTotalUsers(),
				          Client::getTotalCounts(),
				          generateNetworkStats().c_str()
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
				          , Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.m_download).c_str(), Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.m_upload).c_str()
#endif
				         );
			}
			FreeLibrary(hInstPsapi);
		}
	}
	return l_buf.data();
}

WORD CompatibilityManager::getDllPlatform(const string& fullpath)
{
	PLOADED_IMAGE imgLoad = nullptr;
	WORD bRet = IMAGE_FILE_MACHINE_UNKNOWN;
	try
	{
		imgLoad = ::ImageLoad(Text::fromUtf8(fullpath).c_str(), Util::emptyString.c_str()); // TODO: IRainman: I don't know to use unicode here, Windows sucks.
		if (imgLoad && imgLoad->FileHeader)
			bRet = imgLoad->FileHeader->FileHeader.Machine;
	}
	catch (...)
	{
	}
	if (imgLoad)
		::ImageUnload(imgLoad);
		
	return bRet;
}

void CompatibilityManager::reduceProcessPriority()
{
	if (!g_oldPriorityClass || g_oldPriorityClass == GetPriorityClass(GetCurrentProcess()))
	{
		// TODO: refactoring this code to use step-up and step-down of the process priority change.
		g_oldPriorityClass = GetPriorityClass(GetCurrentProcess());
		SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
		// [-] SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1); [-] IRainman: this memory "optimization" for lemmings only.
	}
}

void CompatibilityManager::restoreProcessPriority()
{
	if (g_oldPriorityClass)
		SetPriorityClass(GetCurrentProcess(), g_oldPriorityClass);
}

