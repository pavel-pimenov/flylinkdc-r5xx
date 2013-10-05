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

#ifndef COMPATIBILITY_MANGER_H
#define COMPATIBILITY_MANGER_H

#ifdef _WIN32
#include "typedefs.h"

// http://code.google.com/p/flylinkdc/issues/detail?id=574
class CompatibilityManager
{
	public:
		// Call this function as soon as possible (immediately after the start of the program).
		static void init();
		
		static bool isWine()
		{
			return isSet(IS_WINE);
		}
		static bool runningAnOldOS()
		{
			return isSet(RUNNING_AN_UNSUPPORTED_OS);
		}
		static bool runningIsWow64()
		{
			return isSet(RUNNING_IS_WOW64);
		}
#ifdef FLYLINKDC_SUPPORT_WIN_2000
		static bool IsXPPlus()
		{
			return isSet(OS_XP_PLUS);
		}
#endif
#ifdef FLYLINKDC_SUPPORT_WIN_XP
		static bool IsXPSP3AndHigher()
		{
			return isSet(OS_XP_SP3_PLUS);
		}
		static bool isOsVistaPlus()
		{
			return isSet(OS_VISTA_PLUS);
		}
#endif
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
		static bool isWin7Plus()
		{
			return isSet(OS_WINDOWS7_PLUS);
		}
#endif
		static bool isWin8Plus()
		{
			return isSet(OS_WINDOWS8_PLUS);
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
		static OSVERSIONINFOEX& getVersionInfo() // [+] SSA Get System Info
		{
			return g_osvi;
		}
		static bool isIncompatibleSoftwareFound()
		{
			return !g_incopatibleSoftwareList.empty();
		}
		static string getIncompatibleSoftwareMessage();
		static string generateGlobalMemoryStatusMessage();
		static string generateFullSystemStatusMessage();
		static string generateProgramStats();
		static string getDefaultPath()
		{
			const char* homePath = isWine() ? getenv("HOME") : getenv("SystemDrive");
			string defaultPath = homePath ? homePath : (isWine() ? "\\tmp" : "C:"); // TODO - убрать это под wine не должен быть доступен tmp!
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
		static size_t getPageSize()
		{
			return max(g_sysInfo.dwPageSize, g_sysInfo.dwAllocationGranularity);
		}
		static DWORDLONG getTotalPhysMemory()
		{
			return g_TotalPhysMemory;
		}
		
		static float ProcSpeedCalc();
		static WORD getDllPlatform(const string& fullpath);
		static void reduceProcessPriority();
		static void restoreProcessPriority();
	private:
		static DWORD g_oldPriorityClass;
		static string g_incopatibleSoftwareList;
		static string g_startupInfo;
		static OSVERSIONINFOEX g_osvi;
		static SYSTEM_INFO g_sysInfo;
		
		enum Supports
		{
			IS_WINE = 0,
#ifdef FLYLINKDC_SUPPORT_WIN_2000
			OS_XP_PLUS,
#endif
#ifdef FLYLINKDC_SUPPORT_WIN_XP
			OS_XP_SP3_PLUS,
			OS_VISTA_PLUS,
#endif
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
			OS_WINDOWS7_PLUS,
#endif
			OS_WINDOWS8_PLUS,
			RUNNING_IS_WOW64,
			RUNNING_AN_UNSUPPORTED_OS,
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
		
		static void detectOsSupports();
		static bool detectWine();// [+] PPA
		static void detectUncompatibleSoftware();
		static LONG getComCtlVersionFromOS();
		static void getSystemInfoFromOS();
		static void generateSystemInfoForApp();
		static bool getFromSystemIsAppRunningIsWow64();
		static bool getGlobalMemoryStatusFromOS(MEMORYSTATUSEX* MsEx);
};

#endif // _WIN32

#endif // COMPATIBILITY_MANGER_H