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

#include "stdafx.h"
#include <delayimp.h>
#include <signal.h>

#ifdef _DEBUG
#ifndef _WIN64
// #define USE_FLYLINKDC_VLD // 3>LINK : fatal error LNK1104: cannot open file 'vld.lib' VLD качать тут http://vld.codeplex.com/
#endif
#endif
#ifdef USE_FLYLINKDC_VLD
//[!] ¬ключать только при наличии VLD и только в _DEBUG
#define VLD_DEFAULT_MAX_DATA_DUMP 1
//#define VLD_FORCE_ENABLE // Uncoment this define to enable VLD in release
#include "C:\Program Files (x86)\Visual Leak Detector\include\vld.h" // VLD качать тут http://vld.codeplex.com/
#endif

#include "Resource.h"
#include "MainFrm.h"
#include "ResourceLoader.h" // [+] InfinitySky. PNG Support from Apex 1.3.8.
#include "ChatBot.h" // !SMT!-CB
#include "PopupManager.h"
#include "ToolbarManager.h"
#include "../client/MappingManager.h"
#include "../client/CompatibilityManager.h" // [+] IRainman
#include "../client/ThrottleManager.h"
#include "../FlyFeatures/AutoUpdate.h" // [+] SSA
#include "../FlyFeatures/flyfeatures.h" // [+] SSA

#if !defined(_DEBUG)
#include "DbgHelp.h"
#include "../crash-server/CrashHandler.h"
CrashHandler g_crashHandler(
#ifdef FLYLINKDC_HE
    "C9149EA6-612E-4038-A557-D62DF22755CD"
#else
#ifdef FLYLINKDC_BETA
    "910F4B4D-C71C-45BC-A88D-F59FE022525B"
#else
    "D7F972BA-ACF7-451E-88D5-FF0B98BD085D"
#endif
#endif
    ,                                   // GUID assigned to this application.
    "flylinkdc-r5xx"
#ifdef FLYLINKDC_HE
    "-he"
#endif
    ,                                   // Prefix that will be used with the dump name: YourPrefix_v1.v2.v3.v4_YYYYMMDD_HHMMSS.mini.dmp.
    L"FlylinkDC++ r5xx"
#ifdef FLYLINKDC_HE
    L" HE"
#endif
    ,                                   // Application name that will be used in message box.
    L"FlylinkDC++ Team"                     // Company name that will be used in message box.
);
#endif

//#if defined(_DEBUG)
//#define OVERRIDE_NEW_DELETE
//#include "../vmem/MemPro.cpp"

//#define OVERRIDE_NEW_DELETE
// #define OVERRIDE_MALLOC
//#define VMEM_STATS
//#define VMEM_FULL_CHECKING
//#include "../vmem/VMem.cpp"
//#endif

// #pragma comment(linker, "/ignore:4099") // fix - 6>ssleay32.lib(ssl_err.obj) : warning LNK4099: PDB 'lib.pdb' was not found with 'ssleay32.lib(ssl_err.obj)' or at 'C:\vc10\r5xx-trunk\compiled\lib.pdb'; linking object as if no debug info

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
#ifdef FLYLINKDC_SUPPORT_WIN_2000
static void checkCommonControls()
{
	// [-] IRainman: http://msdn.microsoft.com/en-us/library/windows/desktop/bb776779(v=vs.85).aspx
#define PACKVERSION(major,minor) MAKELONG(minor,major)
	
	HINSTANCE hinstDll;
	DWORD dwVersion = 0;
	
	hinstDll = LoadLibrary(_T("comctl32.dll"));
	
	if (hinstDll)
	{
		DLLGETVERSIONPROC pDllGetVersion;
		
		pDllGetVersion = (DLLGETVERSIONPROC) GetProcAddress(hinstDll, "DllGetVersion");
		
		if (pDllGetVersion)
		{
			DLLVERSIONINFO dvi = {0} ;
			HRESULT hr;
			
			dvi.cbSize = sizeof(dvi);
			
			hr = (*pDllGetVersion)(&dvi);
			
			if (SUCCEEDED(hr))
			{
				dwVersion = PACKVERSION(dvi.dwMajorVersion, dvi.dwMinorVersion);
			}
		}
		
		FreeLibrary(hinstDll);
	}
	
	if (dwVersion < PACKVERSION(5, 80))
	{
		MessageBox(NULL,
		           _T("Your version of windows common controls is too old for FlylinkDC++ to run correctly, and you will most probably experience problems with the user interface. You should download version 5.80 or higher from the DC++ homepage or from Microsoft directly."),
		           _T("User Interface Warning"), MB_OK);
	}
}
#endif // FLYLINKDC_SUPPORT_WIN_2000

static tstring g_sSplashText;
static const int g_splash_size_x = 347;
static const int g_splash_size_y = 93;

// [-] DWORD g_ID_SPLASH = IDR_SPLASH;
LRESULT CALLBACK splashCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_PAINT)
	{
		// Get some information
		HDC dc = GetDC(hwnd);
		RECT rc;
		GetWindowRect(hwnd, &rc);
		OffsetRect(&rc, -rc.left, -rc.top);
		RECT rc2 = rc;
		RECT rc3 = rc;
		//rc2.top = rc2.bottom - 20;
		rc2.top = 7; //[+] PPA
		rc2.right = rc2.right - 5;
		::SetBkMode(dc, TRANSPARENT);
		rc3.top = rc3.bottom - 20;
		ExCImage hi;
		// TODO. Ќужно реализовать прозрачность из WinUtil.cpp.
		
		//ResourceLoader::LoadImageList(IDR_SPLASH, hi, 354, 370);
		static int l_month = 0;
		static int l_day = 0;
		if (l_month == 0)
		{
			time_t currentTime;
			time(&currentTime);
			tm* l_lt = localtime(&currentTime);
			l_month = l_lt->tm_mon + 1;
			l_day   = l_lt->tm_mday;
		}
		
#define LOAD_SPLASH(res) hi.LoadFromResource(res, _T("PNG"), _Module.get_m_hInst())
#define DAY(m, d) (l_month == m && l_day == d)
#define DAYS(m, d1, d2) (l_month == m && l_day >= d1 && l_day <= d2)
		
		if (DAYS(5, 8, 10))
			LOAD_SPLASH(IDR_SPLASH_9MAY);
		else if (DAYS(12, 1, 30) ||
		         DAYS(1, 10, 31) ||
		         DAYS(2, 1, 28))
			LOAD_SPLASH(IDR_SPLASH_WINTER);
		else if (DAY(12, 31))
			LOAD_SPLASH(IDR_SPLASH_NY1);
		else if (DAYS(1, 1, 9))
			LOAD_SPLASH(IDR_SPLASH_NY2);
		else if (DAY(4, 1))
			LOAD_SPLASH(IDR_SPLASH_FOOLS_DAY);
		else
			LOAD_SPLASH(IDR_SPLASH);  // [+] InfinitySky. PNG Support from Apex 1.3.8.
			
#undef LOAD_SPLASH
#undef DAY
#undef DAYS
			
		HDC comp = CreateCompatibleDC(dc); // [+]
		SelectObject(comp, hi); // [+]
		
		BitBlt(dc, 0, 0 , g_splash_size_x, g_splash_size_y, comp, 0, 0, SRCCOPY);
		hi.Destroy();   // [+]
		DeleteDC(comp); // [+]
		DeleteObject(hi);
		LOGFONT logFont = {0};
		GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(logFont), &logFont);
		lstrcpy(logFont.lfFaceName, TEXT("Tahoma"));
		logFont.lfHeight = 13;
		const HFONT hFont = CreateFontIndirect(&logFont);
		CSelectFont l_font(dc, hFont); //-V808
		::SetTextColor(dc, RGB(179, 179, 179)); // [~] Sergey Shushkanov
		const tstring l_progress = g_sSplashText;
		::DrawText(dc, l_progress.c_str(), l_progress.length(), &rc2, DT_CENTER); //-V107
		::SetTextColor(dc, RGB(120, 120, 120));
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
bool g_DisableSplash = false;
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
		
		g_dummy.Create(NULL, rc, T_APPNAME_WITH_VERSION, WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
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
	if (!g_DisableSplash && g_splash)
	{
		DestroyAndDetachWindow(g_splash);
		DestroyAndDetachWindow(g_dummy);
		g_sSplashText.clear();
	}
}

void GuiInit(void* pParam)
{
	UNREFERENCED_PARAMETER(pParam);
	ToolbarManager::newInstance();
	createFlyFeatures(); // [+] SSA
}

void GuiUninit(void* pParam)
{
	UNREFERENCED_PARAMETER(pParam);
	deleteFlyFeatures(); // [+] SSA
	PopupManager::deleteInstance();
	ToolbarManager::deleteInstance();
}

static int Run(LPTSTR /*lpstrCmdLine*/ = NULL, int nCmdShow = SW_SHOWDEFAULT)
{
#ifdef IRAINMAN_INCLUDE_GDI_INIT
	// Initialize GDI+.
	static Gdiplus::GdiplusStartupInput g_gdiplusStartupInput;
	static ULONG_PTR g_gdiplusToken = 0;
	Gdiplus::GdiplusStartup(&g_gdiplusToken, &g_gdiplusStartupInput, NULL);
#endif // IRAINMAN_INCLUDE_GDI_INIT
	
#ifdef FLYLINKDC_SUPPORT_WIN_2000
	checkCommonControls();
#endif
	
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);
	
	ChatBot::newInstance(); // !SMT!-CB
	
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
		
			rc.left = SETTING(MAIN_WINDOW_POS_X);
			rc.top = SETTING(MAIN_WINDOW_POS_Y);
			rc.right = rc.left + SETTING(MAIN_WINDOW_SIZE_X);
			rc.bottom = rc.top + SETTING(MAIN_WINDOW_SIZE_Y);
			// Now, let's ensure we have sane values here...
			if ((rc.left < 0) || (rc.top < 0) || (rc.right - rc.left < 10) || ((rc.bottom - rc.top) < 10))
			{
				rc = wndMain.rcDefault;
			}
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
				
			if (BOOLSETTING(MINIMIZE_ON_STARTUP))
			{
				wndMain.ShowWindow(SW_SHOWMINIMIZED);
			}
			else
			{
				wndMain.ShowWindow(((nCmdShow == SW_SHOWDEFAULT) || (nCmdShow == SW_SHOWNORMAL)) ? SETTING(MAIN_WINDOW_STATE) : nCmdShow);
			}
			
			DestroySplash();
			
			//[-]PPA        AutoUpdateGUIMethod* l_guiDelegate = dynamic_cast<AutoUpdateGUIMethod*>(&wndMain);
			AutoUpdateGUIMethod* l_guiDelegate = &wndMain;
			
			loadingAfterGuiFlyFeatures(wndMain, l_guiDelegate);
			
			nRet = theLoop.Run(); // [2] https://www.box.net/shared/e198e9df5044db2a40f4
			
			_Module.RemoveMessageLoop();
		}  // [!] IRainman fix: correct unload.
	} // !SMT!-fix
	
	ChatBot::deleteInstance(); // !SMT!-CB
	shutdown(GuiUninit, NULL/*, true*/); // TODO http://code.google.com/p/flylinkdc/issues/detail?id=1245
#if defined(__PROFILER_ENABLED__)
	Profiler::dumphtml();
#endif
#ifdef IRAINMAN_INCLUDE_GDI_INIT
	Gdiplus::GdiplusShutdown(g_gdiplusToken);
#endif
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
	// TODO - flush file
#endif
	return nRet;
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
	bool multipleInstances = false;
	bool magnet = false;
	bool delay = false;
	bool openfile = false;
	bool sharefolder = false;
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
	if (_tcsstr(lpstrCmdLine, _T("/nowal")) != NULL)
		g_DisableSQLJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_use_memory")) != NULL)
		g_DisableSQLJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_use_wal")) != NULL)
		g_UseWALJournal = true;
	if (_tcsstr(lpstrCmdLine, _T("/sqlite_synchronous_off")) != NULL)
		g_UseSynchronousOff = true;
		
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
	if (_tcsstr(lpstrCmdLine, _T("/q")) != NULL)
		multipleInstances = true;
	if (_tcsstr(lpstrCmdLine, _T("/nowine")) != NULL)
		CompatibilityManager::setWine(false);
	if (_tcsstr(lpstrCmdLine, _T("/forcewine")) != NULL)
		CompatibilityManager::setWine(true);
	if (_tcsstr(lpstrCmdLine, _T("/magnet")) != NULL)
		magnet = true;
	if (_tcsstr(lpstrCmdLine, _T("/c")) != NULL)
	{
		multipleInstances = true;
		delay = true;
	}
#ifdef SSA_WIZARD_FEATURE
	if (_tcsstr(lpstrCmdLine, _T("/wizard")) != NULL)
		g_ShowWizard = true;
#endif
	if (_tcsstr(lpstrCmdLine, _T("/open")) != NULL)
		openfile = true;
	if (_tcsstr(lpstrCmdLine, _T("/share")) != NULL)
		sharefolder = true;
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
	//if (_tcsstr(lpstrCmdLine, _T("/restart")) != NULL) // [+] IRainman TODO
	//{
	//  // http://code.google.com/p/flylinkdc/issues/detail?id=211
	//  // —истема сама нас перезапустила со всеми параметрами прошлого запуска
	//  // можем что нибудь при этом сделать
	//  // Ќапример спросить пользовател€ нужно ли нас запускать :)
	//  // дополнительные сведени€ в MainFrame::onQueryEndSession
	//}
	
	Util::initialize();
	ThrottleManager::newInstance();
	
	// First, load the settings! Any code running before will not get the value of SettingsManager!
	SettingsManager::newInstance();
	SettingsManager::getInstance()->load();
	const bool l_is_create_wide = SettingsManager::getInstance()->LoadLanguage();
	ResourceManager::startup(l_is_create_wide);
	SettingsManager::getInstance()->setDefaults(); // !SMT!-S: allow localized defaults in string settings
	LogManager::newInstance();
	CreateSplash(); //[+]PPA
	
	g_fly_server_config.loadConfig();
	TimerManager::newInstance();
	ClientManager::newInstance();
	CompatibilityManager::detectUncompatibleSoftware();
	
	ThrottleManager::getInstance()->startup(); // [+] IRainman fix.
	
	if (dcapp.IsAnotherInstanceRunning())
	{
		// Allow for more than one instance...
		bool multiple = false;
		if (multipleInstances == false && magnet == false && openfile == false && sharefolder == false)
		{
			if (MessageBox(NULL, CTSTRING(ALREADY_RUNNING), T_APPNAME_WITH_VERSION, MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST) == IDYES)   // [~] Drakon.
			{
				multiple = true;
			}
		}
		else
		{
			multiple = true;
		}
		
		if (delay == true)
		{
			Thread::sleep(2500);        // let's put this one out for a break
		}
		
		if (multiple == false || magnet == true || openfile == true || sharefolder == true)
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
	return nRet;
}

/**
 * @file
 * $Id: main.cpp,v 1.65 2006/11/04 14:08:28 bigmuscle Exp $
 */