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

#pragma once


#ifndef COMPATIBILITY_MANGER_H
#define COMPATIBILITY_MANGER_H

#ifdef _WIN32
#include "typedefs.h"

class CompatibilityManager
{
	public:
		// Call this function as soon as possible (immediately after the start of the program).
		static void init();
		
		static bool isWine()
		{
			return isSet(IS_WINE);
		}
#ifdef FLYLINKDC_USE_CHECK_OLD_OS
		static bool runningAnOldOS()
		{
			return isSet(RUNNING_AN_UNSUPPORTED_OS);
		}
#endif
		static bool runningIsWow64()
		{
			return isSet(RUNNING_IS_WOW64);
		}
		static bool isOsVistaPlus()
		{
			return isSet(OS_VISTA_PLUS);
		}
		static bool isWin7Plus()
		{
			return isSet(OS_WINDOWS7_PLUS);
		}
		static bool isWin8Plus()
		{
			return isSet(OS_WINDOWS8_PLUS);
		}
		static bool isWin10Plus()
		{
			return isSet(OS_WINDOWS10_PLUS);
		}
		static void setWine(bool p_wine)
		{
			set(IS_WINE, p_wine);
		}
		static LONG getComCtlVersion()
		{
			return g_comCtlVersion;
		}
		static string getFormatedOsVersion();
		static string getWindowsVersionName();
		static DWORD getOsMajor()
		{
			return g_osvi.dwMajorVersion;
		}
		static DWORD getOsMinor()
		{
			return g_osvi.dwMinorVersion;
		}
		static WORD getOsSpMajor()
		{
			return g_osvi.wServicePackMajor;
		}
		static BYTE getOsType()
		{
			return g_osvi.wProductType;
		}
		static BYTE getOsSuiteMask()
		{
			return g_osvi.wSuiteMask;
		}
		static OSVERSIONINFOEX& getVersionInfo()
		{
			return g_osvi;
		}
		static bool isIncompatibleSoftwareFound()
		{
			return !g_incopatibleSoftwareList.empty();
		}
		static const string& getIncompatibleSoftwareList()
		{
			return g_incopatibleSoftwareList;
		}
		static string getIncompatibleSoftwareMessage();
		static string generateGlobalMemoryStatusMessage();
		static string generateFullSystemStatusMessage();
		static string generateProgramStats();
		static string generateNetworkStats();
		static string getDefaultPath()
		{
			const char* homePath = isWine() ? getenv("HOME") : getenv("SystemDrive");
			string defaultPath = homePath ? homePath : (isWine() ? "\\tmp" : "C:"); // TODO - ������ ��� ��� wine �� ������ ���� �������� tmp!
			defaultPath += '\\';
			return defaultPath;
		}
		static string& getStartupInfo()
		{
			return g_startupInfo;
		}
		static size_t getProcessorsCount()
		{
			return g_sysInfo.dwNumberOfProcessors;
		}
		static WORD getProcArch()
		{
			return g_sysInfo.wProcessorArchitecture;
		}
		static DWORDLONG getTotalPhysMemory()
		{
			return g_TotalPhysMemory;
		}
		static DWORDLONG getFreePhysMemory()
		{
			return g_FreePhysMemory;
		}
		static void caclPhysMemoryStat();
		
		static float ProcSpeedCalc();
		static WORD getDllPlatform(const string& fullpath);
		static void reduceProcessPriority();
		static void restoreProcessPriority();
		
		static FINDEX_INFO_LEVELS g_find_file_level;
		static DWORD g_find_file_flags;
		static bool g_is_teredo;
		static bool g_is_ipv6_enabled;
		static bool checkTeredo();
	private:
		static DWORD g_oldPriorityClass;
		static string g_incopatibleSoftwareList;
		static string g_startupInfo;
		static OSVERSIONINFOEX g_osvi;
		static SYSTEM_INFO g_sysInfo;
		
		enum Supports
		{
			IS_WINE = 0,
			OS_VISTA_PLUS,
			OS_WINDOWS7_PLUS,
			OS_WINDOWS8_PLUS,
			OS_WINDOWS10_PLUS,
			RUNNING_IS_WOW64,
#ifdef FLYLINKDC_USE_CHECK_OLD_OS
			RUNNING_AN_UNSUPPORTED_OS,
#endif
			LAST_SUPPORTS
		};
		static void set(Supports index, bool val = true)
		{
			g_supports[index] = val;
		}
		static bool isSet(Supports index)
		{
			return g_supports[index];
		}
		static bool g_supports[LAST_SUPPORTS];
		static LONG g_comCtlVersion;
		static DWORDLONG g_TotalPhysMemory;
		static DWORDLONG g_FreePhysMemory;
		
		static void detectOsSupports();
		static bool detectWine();
		static LONG getComCtlVersionFromOS();
		static void getSystemInfoFromOS();
		static string getProcArchString();
		static void generateSystemInfoForApp();
		static bool getFromSystemIsAppRunningIsWow64();
		static bool getGlobalMemoryStatusFromOS(MEMORYSTATUSEX* MsEx);
	public:
		static void detectUncompatibleSoftware();
		
		// AirDC++ code
		static string Speedinfo();
		static string DiskSpaceInfo(bool onlyTotal = false);
		static tstring diskInfo();
		static TStringList FindVolumes();
		static string CPUInfo();
		static string getSysUptime();
};

#endif // _WIN32

#endif // COMPATIBILITY_MANGER_H