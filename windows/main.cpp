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

#include "stdafx.h"
#include <delayimp.h>
#include <signal.h>

#ifdef _DEBUG
#ifndef _WIN64
// #define FLYLINKDC_USE_VLD // 3>LINK : fatal error LNK1104: cannot open file 'vld.lib' VLD качать тут https://kinddragon.github.io/vld/
#endif
#endif
#ifdef FLYLINKDC_USE_VLD
//[!] Включать только при наличии VLD и только в _DEBUG
#define VLD_DEFAULT_MAX_DATA_DUMP 1
//#define VLD_FORCE_ENABLE // Uncoment this define to enable VLD in release
#include "C:\Program Files (x86)\Visual Leak Detector\include\vld.h" // VLD качать тут https://kinddragon.github.io/vld/
#endif

#include "Resource.h"
#include "MainFrm.h"
#include "ResourceLoader.h"
#include "PopupManager.h"
#include "ToolbarManager.h"
#include "../client/MappingManager.h"
#include "../client/CompatibilityManager.h" // [+] IRainman
#include "../client/ThrottleManager.h"
#include "../FlyFeatures/AutoUpdate.h" // [+] SSA
#include "../FlyFeatures/flyfeatures.h" // [+] SSA


#ifndef _DEBUG
#include "DbgHelp.h"
#include "../doctor-dump/CrashRpt.h"

template<typename T>
static T getFilePath(const T& path)
{
	const auto i = path.rfind('\\');
	return (i != string_t::npos) ? path.substr(0, i + 1) : path;
}

crash_rpt::ApplicationInfo* GetApplicationInfo()
{
	static crash_rpt::ApplicationInfo appInfo;
	appInfo.ApplicationInfoSize = sizeof(appInfo);
	appInfo.ApplicationGUID =
#ifdef FLYLINKDC_BETA
	    "9B9D2DBC-80E9-40FF-9801-52E1F52E5EC0";
#else
	    "45A84685-77E6-4F01-BF10-C698B811087F";
#endif
	    
#ifdef _WIN64
	appInfo.Prefix = "flylinkdc-x64";             // Prefix that will be used with the dump name: YourPrefix_v1.v2.v3.v4_YYYYMMDD_HHMMSS.mini.dmp.
	appInfo.AppName = L"FlylinkDC++ x64";         // Application name that will be used in message box.
#else
	appInfo.Prefix = "flylinkdc-x86";             // Prefix that will be used with the dump name: YourPrefix_v1.v2.v3.v4_YYYYMMDD_HHMMSS.mini.dmp.
	appInfo.AppName = L"FlylinkDC++";             // Application name that will be used in message box.
#endif
	appInfo.Company = L"FlylinkDC++ developers";  // Company name that will be used in message box.
	appInfo.V[0] = 7;
	appInfo.V[1] = 7;
	appInfo.V[2] = VERSION_NUM;
	appInfo.V[3] = REVISION_NUM;
	return &appInfo;
}

/*
TODO - рассмотреть возможность использовать штатную dbghelp.dll
чтобы не забивать дистр лишней dll-кой

 WCHAR dbghelpPath[MAX_PATH];
ExpandEnvironmentStringsW(L"%SystemRoot%\\System32\\dbghelp.dll", dbghelpPath, _countof(dbghelpPath));

printf("%ls\n", dbghelpPath);

if (NULL == LoadLibraryW(dbghelpPath))
printf("failed\n");
else
printf("succeeded\n");

TODO-2 // Добавить атрибуты по mediainfo + что+то еще
    g_crashRpt.AddUserInfoToReport(L"Test-key",L"Test-Value");
*/

crash_rpt::HandlerSettings* GetHandlerSettings()
{
	static TCHAR g_path_sender[MAX_PATH] = {0};
	static TCHAR g_path_dbhelp[MAX_PATH] = {0};
	::GetModuleFileName(NULL, g_path_sender, MAX_PATH);
	wcscpy(g_path_dbhelp, g_path_sender);
	auto l_tslash = wcsrchr(g_path_sender, '\\');
	if (l_tslash)
	{
		l_tslash++;
#ifdef _WIN64
		wcscpy(l_tslash, L"sendrpt-x64.exe");
#else
		wcscpy(l_tslash, L"sendrpt-x86.exe");
#endif
		l_tslash = wcsrchr(g_path_dbhelp, '\\');
		l_tslash++;
#ifdef _WIN64
		wcscpy(l_tslash, L"dbghelp-x64.dll");
#else
		wcscpy(l_tslash, L"dbghelp-x86.dll");
#endif
	}
	static crash_rpt::HandlerSettings g_handlerSettings;
	g_handlerSettings.HandlerSettingsSize = sizeof(g_handlerSettings);
	g_handlerSettings.OpenProblemInBrowser = TRUE;
	g_handlerSettings.SendRptPath = g_path_sender;
	g_handlerSettings.DbgHelpPath = g_path_dbhelp;
	return &g_handlerSettings;
}

crash_rpt::CrashRpt g_crashRpt(
#ifdef _WIN64
    L"crashrpt-x64.dll",
#else
    L"crashrpt-x86.dll",
#endif
    GetApplicationInfo(),
    GetHandlerSettings());

#endif

#ifdef FLYLINKDC_BETA
bool g_UseCSRecursionLog = false;
#endif
bool g_UseStrongDCTag = false;

CAppModule _Module;
static void sendCmdLine(HWND hOther, LPTSTR lpstrCmdLine)
{
	const tstring cmdLine = lpstrCmdLine;
	COPYDATASTRUCT cpd = {0};
	cpd.cbData = sizeof(TCHAR) * (cmdLine.length() + 1); //-V103
	cpd.lpData = (void *)cmdLine.c_str();
	SendMessage(hOther, WM_COPYDATA, NULL, (LPARAM) & cpd);
}

BOOL CALLBACK searchOtherInstance(HWND hWnd, LPARAM lParam)
{
	DWORD_PTR result;
	LRESULT ok = ::SendMessageTimeout(hWnd, WMU_WHERE_ARE_YOU, 0, 0,
	                                  SMTO_BLOCK | SMTO_ABORTIFHUNG, 5000, &result);
	if (ok == 0)
		return TRUE;
	if (result == WMU_WHERE_ARE_YOU) //-V104
	{
		// found it
		HWND *target = (HWND *)lParam;
		*target = hWnd;
		return FALSE;
	}
	return TRUE;
}

static tstring g_sSplashText;
static int g_splash_size_x = 347;
static int g_splash_size_y = 93;
ExCImage* g_splash_png = nullptr;

LRESULT CALLBACK splashCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_PAINT && g_splash_png)
	{
	
		// Get some information
		HDC dc = GetDC(hwnd);
		RECT rc;
		GetWindowRect(hwnd, &rc);
		OffsetRect(&rc, -rc.left, -rc.top);
		RECT rc2 = rc;
		RECT rc3 = rc;
		rc2.top = 2;
		rc2.right = rc2.right - 5;
		::SetBkMode(dc, TRANSPARENT);
		rc3.top = rc3.bottom - 15;
		rc3.left = rc3.right - 120;
		
		{
			HDC l_comp = CreateCompatibleDC(dc); // [+]
			if (g_splash_png)
			{
				SelectObject(l_comp, *g_splash_png); // [+]
			}
			
			BitBlt(dc, 0, 0, g_splash_size_x, g_splash_size_y, l_comp, 0, 0, SRCCOPY);
			DeleteDC(l_comp);
		}
		LOGFONT logFont = {0};
		GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(logFont), &logFont);
		lstrcpy(logFont.lfFaceName, TEXT("Tahoma"));
		logFont.lfHeight = 11;
		const HFONT hFont = CreateFontIndirect(&logFont);
		CSelectFont l_font(dc, hFont); //-V808
		//::SetTextColor(dc, RGB(179, 179, 179)); // [~] Sergey Shushkanov
		::SetTextColor(dc, RGB(255, 255, 255)); // [~] Sergey Shushkanov
		const tstring l_progress = g_sSplashText;
		::DrawText(dc, l_progress.c_str(), l_progress.length(), &rc2, DT_CENTER); //-V107
		//::SetTextColor(dc, RGB(50, 50, 50));
		const tstring l_version = T_VERSIONSTRING;
		::DrawText(dc, l_version.c_str(), l_version.length(), &rc3, DT_CENTER);
		DeleteObject(hFont);
		int l_res = ReleaseDC(hwnd, dc);
		dcassert(l_res);
	}
	
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void splash_callBack(void* p_x, const tstring& p_a)
{
	g_sSplashText = p_a; // [!]NightOrion(translate)
	SendMessage((HWND)p_x, WM_PAINT, 0, 0);
}

CEdit g_dummy;
CWindow g_splash;
bool g_DisableSplash  = false;
bool g_DisableGDIPlus = false;
#ifdef _DEBUG
bool g_DisableTestPort = false;
#else
bool g_DisableTestPort = false;
#endif
#ifdef SSA_WIZARD_FEATURE
bool g_ShowWizard = false;
#endif
void CreateSplash()
{
	if (!g_DisableSplash)
	{
		CRect rc;
		rc.bottom = GetSystemMetrics(SM_CYFULLSCREEN);
		rc.top = (rc.bottom / 2) - 80;
		
		rc.right = GetSystemMetrics(SM_CXFULLSCREEN);
		rc.left = rc.right / 2 - 85;
		
		g_dummy.Create(NULL, rc, getFlylinkDCAppCaptionWithVersionT().c_str(), WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		               ES_CENTER | ES_READONLY, WS_EX_STATICEDGE);
		g_splash.Create(_T("Static"), GetDesktopWindow(), g_splash.rcDefault, NULL, WS_POPUP | WS_VISIBLE | SS_USERITEM | WS_EX_TOOLWINDOW);
		/*
		Error #365: LEAK 40 direct bytes 0x027d7f78-0x027d7fa0 + 0 indirect bytes
		# 0 COMCTL32.dll!DPA_Grow                 +0xf5     (0x71ec0d15 <COMCTL32.dll+0x30d15>)
		# 1 COMCTL32.dll!GetWindowSubclass        +0x5235   (0x71eabb74 <COMCTL32.dll+0x1bb74>)
		# 2 USER32.dll!gapfnScSendMessage         +0x331    (0x766b62fa <USER32.dll+0x162fa>)
		# 3 USER32.dll!GetDC                      +0x51     (0x766b7316 <USER32.dll+0x17316>)
		# 4 USER32.dll!GetThreadDesktop           +0x184    (0x766b6de8 <USER32.dll+0x16de8>)
		# 5 USER32.dll!UnregisterClassW           +0x7bb    (0x766ba740 <USER32.dll+0x1a740>)
		# 6 ntdll.dll!KiUserCallbackDispatcher    +0x2d     (0x7761010a <ntdll.dll+0x1010a>)
		# 7 USER32.dll!UnregisterClassW           +0xab7    (0x766baa3c <USER32.dll+0x1aa3c>)
		# 8 USER32.dll!CreateWindowExW            +0x32     (0x766b8a5c <USER32.dll+0x18a5c>)
		# 9 ATL::CWindow::Create                   [c:\program files (x86)\microsoft visual studio 10.0\vc\atlmfc\include\atlwin.h:818]
		#10 CreateSplash                           [c:\vc10\r5xx\windows\main.cpp:274]
		#11 Run                                    [c:\vc10\r5xx\windows\main.cpp:340]
		*/
		
		if (!g_splash_png)
		{
			g_splash_png = new ExCImage;
			const string l_custom_splash = Util::getModuleCustomFileName("splash.png");
			if (File::isExist(l_custom_splash))
			{
				g_splash_png->Load(Text::toT(l_custom_splash).c_str());
			}
			else
			{
				static int g_month = 0;
				static int g_day = 0;
				if (g_month == 0)
				{
					time_t currentTime;
					time(&currentTime);
					tm* l_lt = localtime(&currentTime);
					if (l_lt)
					{
						g_month = l_lt->tm_mon + 1;
						g_day   = l_lt->tm_mday;
					}
				}
				auto load_splash = [](int p_res) -> void
				{
					g_splash_png->LoadFromResource(p_res, _T("PNG"), _Module.get_m_hInst());
				};
				
				bool is_found = false;
				
#ifdef FLYLINKDC_USE_WINTER_SPLASH
				auto isDAYS = [](int m1, int d1, int m2, int d2) -> bool
				{
					if (m2 == 0 && d2 == 0)
						return g_month == m1 && g_day == d1;
					return g_month >= m1 && g_month <= m2 && g_day >= d1 && g_day <= d2;
				};
				
				typedef struct tagSplashMonths
				{
					int m_st;
					int d_st;
					int m_en;
					int d_en;
					int p_res;
				} SplashMonths;
				static const int n_day = 5;
				SplashMonths g_dates[n_day] =
				{
					{12, 1, 12, 30, IDR_SPLASH_WINTER},     // zima
					{12, 31, 0, 0,  IDR_SPLASH_NY1},        // new year
					{1, 1, 1, 9,    IDR_SPLASH_NY1},        // new year
					{1, 10, 2, 29,  IDR_SPLASH_WINTER},     // zima
					{5, 8, 5, 10,   IDR_SPLASH_9MAY},       // 9 may
				};
				for (int i = 0; i < n_day; i++)
				{
					if (isDAYS(g_dates[i].m_st, g_dates[i].d_st, g_dates[i].m_en, g_dates[i].d_en))
					{
						load_splash(g_dates[i].p_res);
						d_found = true;
					}
				}
#endif
				if (!is_found)
				{
					load_splash(IDR_SPLASH);
				}
			}
		}
		g_splash_size_x = g_splash_png->GetWidth();
		g_splash_size_y = g_splash_png->GetHeight();
		
		g_splash.SetFont((HFONT)GetStockObject(DEFAULT_GUI_FONT));
		
		HDC dc = g_splash.GetDC();
		rc.right = rc.left + g_splash_size_x;
		rc.bottom = rc.top + g_splash_size_y;
		int l_res = g_splash.ReleaseDC(dc);
		dcassert(l_res);
		g_splash.HideCaret();
		g_splash.SetWindowPos(NULL, &rc, SWP_SHOWWINDOW);
		g_splash.SetWindowLongPtr(GWLP_WNDPROC, (LONG_PTR)&splashCallback);
		g_splash.CenterWindow();
		g_splash.SetFocus();
		g_splash.RedrawWindow();
	}
}

void DestroySplash() // [+] IRainman
{
	safe_delete(g_splash_png);
	if (!g_DisableSplash && g_splash)
	{
		DestroyAndDetachWindow(g_splash);
		DestroyAndDetachWindow(g_dummy);
		g_sSplashText.clear();
	}
}

void GuiInit(void*)
{
	createFlyFeatures(); // [+] SSA
}

void GuiUninit(void*)
{
	deleteFlyFeatures(); // [+] SSA
	PopupManager::deleteInstance();
}
#ifdef SCALOLAZ_MANY_MONITORS
int ObtainMonitors() // Count of a Display Devices
{
	DWORD i = 0;
	int count = 0;
	//DWORD j = 0;
	DISPLAY_DEVICE dc = {0};
	
	dc.cb = sizeof(dc);
	while (EnumDisplayDevices(NULL, i, &dc, EDD_GET_DEVICE_INTERFACE_NAME) != 0)
		// Read More: https://msdn.microsoft.com/ru-ru/library/windows/desktop/dd162609(v=vs.85).aspx
	{
		if ((dc.StateFlags & DISPLAY_DEVICE_ACTIVE) && !(dc.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
		{
			// Read More: https://msdn.microsoft.com/ru-ru/library/windows/desktop/dd183569(v=vs.85).aspx
			/*  // Этот кусок до счётчика Реальных Дисплеев нам не нужен, но оставлю. Вдруг приспичит разрешение дисплеев и их имена брать.
			DEVMODE dm;
			j = 0;
			while (EnumDisplaySettings(dc.DeviceName, j, &dm) != 0)
			{
			    //Запоминаем DEVMODE dm, чтобы потом мы могли его найти и использовать
			    //в ChangeDisplaySettings, когда будем инициализировать окно
			    ++j;
			}
			*/
			++count;    // Count for REAL Devices
		}
		++i;
	}
	return count;
}
#endif
static int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
#ifdef IRAINMAN_INCLUDE_GDI_INIT
	// Initialize GDI+.
	static Gdiplus::GdiplusStartupInput g_gdiplusStartupInput;
	static ULONG_PTR g_gdiplusToken = 0;
	Gdiplus::GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);
#endif // IRAINMAN_INCLUDE_GDI_INIT
	
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);
	
	startup(splash_callBack, g_DisableSplash ? (void*)0 : (void*)g_splash.m_hWnd, GuiInit, NULL);
	startupFlyFeatures(splash_callBack, g_DisableSplash ? (void*)0 : (void*)g_splash.m_hWnd); // [+] SSA
	WinUtil::initThemeIcons();
	static int nRet;
	{
		// !SMT!-fix this will ensure that GUI (wndMain) destroyed before client library shutdown (gui objects may call lib)
		MainFrame wndMain;
#ifdef SSA_WIZARD_FEATURE
		if (g_ShowWizard)
		{
			wndMain.SetWizardMode();
		}
#endif
		
		CRect rc = wndMain.rcDefault;
		
		if ((SETTING(MAIN_WINDOW_POS_X) != CW_USEDEFAULT) &&
		        (SETTING(MAIN_WINDOW_POS_Y) != CW_USEDEFAULT) &&
		        (SETTING(MAIN_WINDOW_SIZE_X) != CW_USEDEFAULT) &&
		        (SETTING(MAIN_WINDOW_SIZE_Y) != CW_USEDEFAULT))
		{
		
			/*
			
			Пока не работает на мульти-мониках
			            RECT l_desktop = {0,0,1024,600};
			            GetWindowRect(GetDesktopWindow(), &l_desktop);
			            rc.left = SETTING(MAIN_WINDOW_POS_X);
			            bool l_is_skip = false;
			            if (rc.left < 0 || rc.left >= l_desktop.left - 50)
			            {
			                rc.left = 0;
			                rc.right = l_desktop.right;
			                l_is_skip = true;
			            }
			            rc.top = SETTING(MAIN_WINDOW_POS_Y);
			            if (rc.top < 0 || rc.top >= l_desktop.top - 50)
			            {
			                rc.top = 0;
			                rc.bottom = l_desktop.bottom;
			                l_is_skip = true;
			            }
			            if (l_is_skip == false)
			            {
			                rc.right = rc.left + SETTING(MAIN_WINDOW_SIZE_X);
			                if (rc.right < 0 || rc.right >= l_desktop.right - l_desktop.left)
			                    rc.right = l_desktop.right;
			                rc.bottom = rc.top + SETTING(MAIN_WINDOW_SIZE_Y);
			                if (rc.bottom < 0 || rc.bottom >= l_desktop.bottom - l_desktop.top)
			                    rc.bottom = l_desktop.bottom;
			            }
			*/
			rc.left = SETTING(MAIN_WINDOW_POS_X);
			rc.top = SETTING(MAIN_WINDOW_POS_Y);
			rc.right = rc.left + SETTING(MAIN_WINDOW_SIZE_X);
			rc.bottom = rc.top + SETTING(MAIN_WINDOW_SIZE_Y);
		}
		
		// Now, let's ensure we have sane values here...
#ifdef SCALOLAZ_MANY_MONITORS
		const int l_mons = ObtainMonitors();    // Get  Phisical Display Drives Count: 1 or more
#endif
		if (((rc.left < -10) || (rc.top < -10) || (rc.right - rc.left < 500) || ((rc.bottom - rc.top) < 300))
#ifdef SCALOLAZ_MANY_MONITORS
		        && l_mons < 2
		        || ((rc.left < -2048) || (rc.right > 5000) || (rc.top < -10) || (rc.bottom > 4000))
#endif
		   )
		{
			rc = wndMain.rcDefault;
			// nCmdShow - по идее, команда при запуске флая с настройками. Но мы её заюзаем как флаг
			// ДА для установки дефолтного стиля для окна.
			// ниже по коду видно, что если в этой команде есть что-то, то будет заюзан стиль из Настроек клиента.
			if (!nCmdShow)
				nCmdShow = SW_SHOWDEFAULT;  // SW_SHOWNORMAL
		}
		const int rtl = /*ResourceManager::getInstance()->isRTL() ? WS_EX_RTLREADING :*/ 0; // [!] IRainman fix.
		if (wndMain.CreateEx(NULL, rc, 0, rtl | WS_EX_APPWINDOW | WS_EX_WINDOWEDGE) == NULL)
		{
			ATLTRACE(_T("Main window creation failed!\n"));
			DestroySplash();
			nRet = 0; // [!] IRainman fix: correct unload.
		}
		else // [!] IRainman fix: correct unload.
		{
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
			HashManager::getInstance()->SetFsDetectorNotifyWnd(MainFrame::getMainFrame()->m_hWnd);
#endif
			
			// Backup & Archive Settings at Starup!!! Written by NightOrion.
			if (BOOLSETTING(STARTUP_BACKUP))
			{
				Util::BackupSettings();
			}
			// End of BackUp...
			
			if (/*SETTING(PROTECT_PRIVATE) && */SETTING(PROTECT_PRIVATE_RND))
				SET_SETTING(PM_PASSWORD, Util::getRandomNick()); // Generate a random PM password
				
			wndMain.ShowWindow(((nCmdShow == SW_SHOWDEFAULT) || (nCmdShow == SW_SHOWNORMAL)) ? SETTING(MAIN_WINDOW_STATE) : nCmdShow);
			if (BOOLSETTING(MINIMIZE_ON_STARTUP))
			{
				wndMain.ShowWindow(SW_SHOWMINIMIZED);
			}
			
			DestroySplash();
			
			AutoUpdateGUIMethod* l_guiDelegate = &wndMain;
			
			loadingAfterGuiFlyFeatures(wndMain, l_guiDelegate);
			
			nRet = theLoop.Run(); // [2] https://www.box.net/shared/e198e9df5044db2a40f4
			
			_Module.RemoveMessageLoop();
		}  // [!] IRainman fix: correct unload.
	} // !SMT!-fix
	
	shutdown(GuiUninit, NULL/*, true*/);
#if defined(__PROFILER_ENABLED__)
	Profiler::dumphtml();
#endif
#ifdef IRAINMAN_INCLUDE_GDI_INIT
	Gdiplus::GdiplusShutdown(g_gdiplusToken);
#endif
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	// TODO - flush file
#endif
#ifdef FLYLINKDC_USE_PROFILER_CS
	CFlyLockProfiler::print_stat();
#endif
	DirectoryListing::print_stat();
	return nRet;
}

#ifdef FLYLINKDC_BETA
static void crash_test_doctor_dump()
{
#ifndef _DEBUG
	*((int*)0) = 0;
#endif
}
#endif
namespace leveldb
{
//extern void LevelDBDestoyModule();
}

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow)
{
#ifdef _DEBUG
// [-] VLD  _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	// [+] IRainman Initialize manager of compatibility for the correction of program options depending on the environment, as well to prevent errors related to conflicts with other software.
	CompatibilityManager::init();
#ifdef _DEBUG
	static uint8_t l_data[24];
	Encoder::fromBase32("LVAFZ2YD2GY47TLWPQXZVPQKTEAQ7EIGTXJJEIQ", l_data, 24);
#endif
	
#ifndef _DEBUG
	SingleInstance dcapp(_T("{FLYDC-AEE8350A-B49A-4753-AB4B-E55479A48351}"));
#else
	SingleInstance dcapp(_T("{FLYDC-AEE8350A-B49A-4753-AB4B-E55479A48350}"));
#endif
	bool l_is_multipleInstances = false;
	bool l_is_magnet = false;
	bool l_is_delay = false;
	bool l_is_openfile = false;
	bool l_is_sharefolder = false;
#ifdef _DEBUG
	// [!] brain-ripper
	// Dear developer, if you don't want to see Splash screen in debug version,
	// please specify parameter "/nologo" in project's
	// "Properties/Configuration Properties/Debugging/Command Arguments"
	
	//g_DisableSplash = true;
#endif
	extern bool g_DisableSQLJournal;
	extern bool g_UseWALJournal;
	extern bool g_EnableSQLtrace;
	extern bool g_UseSynchronousOff;
	extern bool g_UseCSRecursionLog;
	extern bool g_UseStrongDCTag;
	extern bool g_DisableUserStat;
	if (_tcsstr(lpstrCmdLine, _T("/nowal")) != NULL)
		g_DisableSQLJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_use_memory")) != NULL)
		g_DisableSQLJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_use_wal")) != NULL)
		g_UseWALJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_synchronous_off")) != NULL)
		g_UseSynchronousOff = true;
	if (_tcsstr(lpstrCmdLine, _T("/hfs_ignore_file_size")) != NULL)
		ShareManager::setIgnoreFileSizeHFS();
	//if (_tcsstr(lpstrCmdLine, _T("/disable_users_stats")) != NULL)
	//  g_DisableUserStat = true;
	
	const auto l_debug_fly_server_url = _tcsstr(lpstrCmdLine, _T("/debug_fly_server_url="));
	if (l_debug_fly_server_url != NULL)
	{
		extern string g_debug_fly_server_url;
		g_debug_fly_server_url = Text::fromT(l_debug_fly_server_url + 22);
	}
	if (_tcsstr(lpstrCmdLine, _T("/sqltrace")) != NULL)
		g_EnableSQLtrace = true;
	if (_tcsstr(lpstrCmdLine, _T("/nologo")) != NULL)
		g_DisableSplash = true;
	if (_tcsstr(lpstrCmdLine, _T("/nogdiplus")) != NULL)
		g_DisableGDIPlus = true;
	if (_tcsstr(lpstrCmdLine, _T("/notestport")) != NULL)
		g_DisableTestPort = true;
	if (_tcsstr(lpstrCmdLine, _T("/q")) != NULL)
		l_is_multipleInstances = true;
	if (_tcsstr(lpstrCmdLine, _T("/nowine")) != NULL)
		CompatibilityManager::setWine(false);
	if (_tcsstr(lpstrCmdLine, _T("/forcewine")) != NULL)
		CompatibilityManager::setWine(true);
	if (_tcsstr(lpstrCmdLine, _T("/magnet")) != NULL)
		l_is_magnet = true;
		
#ifdef FLYLINKDC_BETA
	if (_tcsstr(lpstrCmdLine, _T("/strongdc")) != NULL)
		g_UseStrongDCTag = true;
	//if (_tcsstr(lpstrCmdLine, _T("/critical_section_log")) != NULL)
	//  g_UseCSRecursionLog = true;
	if (_tcsstr(lpstrCmdLine, _T("/crash-test-doctor-dump")) != NULL)
	{
		crash_test_doctor_dump();
	}
#endif
	if (_tcsstr(lpstrCmdLine, _T("/c")) != NULL)
	{
		l_is_multipleInstances = true;
		l_is_delay = true;
	}
#ifdef SSA_WIZARD_FEATURE
	if (_tcsstr(lpstrCmdLine, _T("/wizard")) != NULL)
		g_ShowWizard = true;
#endif
	if (_tcsstr(lpstrCmdLine, _T("/open")) != NULL)
		l_is_openfile = true;
	if (_tcsstr(lpstrCmdLine, _T("/share")) != NULL)
		l_is_sharefolder = true;
#ifdef SSA_SHELL_INTEGRATION
	if (_tcsstr(lpstrCmdLine, _T("/installShellExt")) != NULL)
	{
		WinUtil::makeShellIntegration(false);
		return 0;
	}
	if (_tcsstr(lpstrCmdLine, _T("/uninstallShellExt")) != NULL)
	{
		WinUtil::makeShellIntegration(true);
		return 0;
	}
#endif
	if (_tcsstr(lpstrCmdLine, _T("/installStartup")) != NULL)
	{
		WinUtil::AutoRunShortCut(true);
		return 0;
	}
	if (_tcsstr(lpstrCmdLine, _T("/uninstallStartup")) != NULL)
	{
		WinUtil::AutoRunShortCut(false);
		return 0;
	}
	
	Util::initialize();
	ThrottleManager::newInstance();
	
	// First, load the settings! Any code running before will not get the value of SettingsManager!
	SettingsManager::newInstance();
	SettingsManager::getInstance()->load();
	const bool l_is_create_wide = SettingsManager::LoadLanguage();
	ResourceManager::startup(l_is_create_wide);
	SettingsManager::getInstance()->setDefaults(); // !SMT!-S: allow localized defaults in string settings
	LogManager::init();
	CreateSplash(); //[+]PPA
	
	g_fly_server_config.loadConfig();
	TimerManager::newInstance();
	ClientManager::newInstance();
	CompatibilityManager::detectUncompatibleSoftware();
	ThrottleManager::getInstance()->startup(); // [+] IRainman fix.
	CompatibilityManager::caclPhysMemoryStat();
	if (dcapp.IsAnotherInstanceRunning())
	{
		// Allow for more than one instance...
		bool multiple = false;
		if (l_is_multipleInstances == false && l_is_magnet == false && l_is_openfile == false && l_is_sharefolder == false)
		{
			if (MessageBox(NULL, CTSTRING(ALREADY_RUNNING), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST) == IDYES)   // [~] Drakon.
			{
				multiple = true;
			}
		}
		else
		{
			multiple = true;
		}
		
		if (l_is_delay == true)
		{
			Thread::sleep(2500);        // let's put this one out for a break
		}
		
		if (multiple == false || l_is_magnet == true || l_is_openfile == true || l_is_sharefolder == true)
		{
			HWND hOther = NULL;
			EnumWindows(searchOtherInstance, (LPARAM)&hOther);
			
			if (hOther != NULL)
			{
				// pop up
				::SetForegroundWindow(hOther);
				
				/*if( IsIconic(hOther)) {
				    //::ShowWindow(hOther, SW_RESTORE); // !SMT!-f - disable, it unlocks password-protected instance
				}*/
				sendCmdLine(hOther, lpstrCmdLine);
			}
			return FALSE;
		}
	}
	
	// For SHBrowseForFolder, UPnP_COM
	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	ATLASSERT(SUCCEEDED(hRes)); // [+] IRainman
	
	
#ifdef _DEBUG
	// It seamed not to be working for stack-overflow catch'n'process.
	// I'll research it further
	
	// [+] brain-ripper
	// Try to reserve some space for stack on stack-overflow event,
	// to successfully make crash-dump.
	// Note SetThreadStackGuarantee available on Win2003+ (including WinXP x64)
	HMODULE hKernelLib = LoadLibrary(L"kernel32.dll");
	
	if (hKernelLib)
	{
		typedef BOOL (WINAPI * SETTHREADSTACKGUARANTEE)(PULONG StackSizeInBytes);
		SETTHREADSTACKGUARANTEE SetThreadStackGuarantee = (SETTHREADSTACKGUARANTEE)GetProcAddress(hKernelLib, "SetThreadStackGuarantee");
		
		if (SetThreadStackGuarantee)
		{
			ULONG len = 1024 * 8;
			SetThreadStackGuarantee(&len);
		}
		
		FreeLibrary(hKernelLib);
	}
#endif
	
	AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES |
	                      ICC_TAB_CLASSES | ICC_UPDOWN_CLASS | ICC_USEREX_CLASSES);   // add flags to support other controls
	                      
	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));
	
	HINSTANCE hInstRich = ::LoadLibrary(_T("RICHED20.DLL"));
	if (!hInstRich)
		hInstRich = ::LoadLibrary(_T("RICHED32.DLL"));
		
	const int nRet = Run(lpstrCmdLine, nCmdShow);
	
	if (hInstRich)
	{
		::FreeLibrary(hInstRich);
	}
	
	_Module.Term();
	::CoUninitialize();
	DestroySplash();
	LogManager::flush_all_log();
	//leveldb::LevelDBDestoyModule();
	return nRet;
}
/**
 * @file
 * $Id: main.cpp,v 1.65 2006/11/04 14:08:28 bigmuscle Exp $
 */