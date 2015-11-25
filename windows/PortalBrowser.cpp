#include "stdafx.h"
#include "PortalBrowser.h"
#include "MainFrm.h"
#include "WinUtil.h"
#include <fstream>

std::map<std::wstring, PortalBrowserFrame*> PortalBrowserFrame::g_portal_frames;
FastCriticalSection PortalBrowserFrame::g_cs;

static LPCWSTR s_ErrorStringTable[] =
{
	L"unknown error",
	L"error to parse XML",
	L"wrong XML version number. Please update your DC client",
	L"error to initialize IE Web Browser",
};

PortalBrowserFrame::PortalBrowserFrame(LPCWSTR pszName, WORD wID):
	m_hLib(nullptr),
	m_hBrowser(nullptr),
	m_bSizing(false),
	m_bActive(false),
	m_DestroyBrowser(nullptr),
	m_GetBrowserWnd(nullptr),
	m_TranslateBrowserAccelerator(nullptr),
	m_wID(wID),
	m_LastSizeState(0xffffffff)
{
	static bool g_is_first = false;
	if (g_is_first == false)
	{
		g_is_first = true;
		extern tstring g_full_user_agent;
		const auto l_agent = Text::fromT(g_full_user_agent);
		UrlMkSetSessionOption(URLMON_OPTION_USERAGENT, (LPVOID)l_agent.c_str(), l_agent.length(), 0);
	}
}

PortalBrowserFrame::~PortalBrowserFrame()
{
	{
		CFlyFastLock(g_cs);
		auto i = g_portal_frames.begin();
		
		while (i != g_portal_frames.end())
		{
			if (i->second == this)
			{
				g_portal_frames.erase(i);
				break;
			}
			++i;
		}
	}
	
	Cleanup();
}

BOOL PortalBrowserFrame::PreTranslateMessage(MSG* pMsg)
{
	BOOL bRet = FALSE;
	if (!g_portal_frames.empty())
	{
		CFlyFastLock(g_cs);
		auto i = g_portal_frames.begin();
		while (i != g_portal_frames.end())
		{
			bRet |= i->second->PreTranslateMessage_ByInstance(pMsg);
			++i;
		}
	}
	return bRet;
}

BOOL PortalBrowserFrame::PreTranslateMessage_ByInstance(MSG* pMsg)
{
	BOOL bRet = FALSE;
	
	if (m_hBrowser && m_TranslateBrowserAccelerator)
	{
		__try
		{
			bRet = m_TranslateBrowserAccelerator(m_hBrowser, pMsg);
		}
		__except(1)
		{
		}
	}
	
	return bRet;
}

void PortalBrowserFrame::openWindow(WORD wID)
{
	PORTAL_BROWSER_ITEM_STRUCT const *pPortal = GetPortalBrowserListData(wID - IDC_PORTAL_BROWSER);
	PortalBrowserFrame::INIT_STRUCT is = {0};
	
	std::wstring strPassword;
	std::wstring strLogin;
	if (pPortal)
	{
		if (pPortal->strName.size())
		{
			if (pPortal->strHubUrl.size())
			{
				std::string str;
				str.resize(pPortal->strHubUrl.size());
				
				WideCharToMultiByte(CP_ACP, 0, pPortal->strHubUrl.c_str(), pPortal->strHubUrl.size(), &str.at(0), str.size(), NULL, NULL);
				FavoriteHubEntry* fhub = FavoriteManager::getFavoriteHubEntry(str.c_str());
				
				if (fhub)
				{
					const auto& l_password = fhub->getPassword(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'fhub->getPassword()' expression repeatedly. portalbrowser.cpp 103
					if (l_password.size())
					{
						strPassword.resize(l_password.size());
						MultiByteToWideChar(CP_ACP, 0, l_password.c_str(), l_password.size(), &strPassword.at(0), strPassword.size());
						is.pszPortalPassword = strPassword.c_str();
					}
					
					const auto& l_nick = fhub->getNick(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'fhub->getNick()' expression repeatedly. portalbrowser.cpp 110
					if (l_nick.size())
					{
						strLogin.resize(l_nick.size());
						MultiByteToWideChar(CP_ACP, 0, l_nick.c_str(), l_nick.size(), &strLogin.at(0), strLogin.size());
						is.pszPortalLogin = strLogin.c_str();
					}
				}
			}
		}
	}
	else
		return;
		
	CFlyFastLock(g_cs);
	auto i = g_portal_frames.find(pPortal->strName);
	
	if (i == g_portal_frames.end())
	{
		PortalBrowserFrame *pFrame = new PortalBrowserFrame(pPortal->strName.c_str(), wID);
		g_portal_frames[pPortal->strName] = pFrame;
		
		pFrame->CreateEx(WinUtil::g_mdiClient, pFrame->rcDefault, pPortal->strName.c_str(), 0, 0, (LPVOID)&is);
		WinUtil::setButtonPressed(wID, true);
	}
	else
	{
		// match the behavior of MainFrame::onSelected()
		HWND hWnd = i->second->m_hWnd;
		
		if (isMDIChildActive(hWnd))
		{
			::PostMessage(hWnd, WM_CLOSE, NULL, NULL);
		}
		else if (i->second->MDIGetActive() != hWnd)
		{
			MainFrame::getMainFrame()->MDIActivate(hWnd);
			WinUtil::setButtonPressed(wID, true);
		}
		else if (BOOLSETTING(TOGGLE_ACTIVE_WINDOW))
		{
			::SetWindowPos(hWnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
			i->second->MDINext(hWnd);
			hWnd = i->second->MDIGetActive();
			WinUtil::setButtonPressed(wID, true);
		}
		if (::IsIconic(hWnd))
			::ShowWindow(hWnd, SW_RESTORE);
	}
}

LRESULT PortalBrowserFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	WinUtil::setButtonPressed(m_wID, false);
	
	bHandled = FALSE;
	return TRUE;
}

bool PortalBrowserFrame::isMDIChildActive(HWND hWnd)
{
	HWND wnd = MainFrame::getMainFrame()->MDIGetActive();
	dcassert(wnd != NULL);
	return (hWnd == wnd);
}

void PortalBrowserFrame::closeWindow(LPCWSTR pszName)
{
	CFlyFastLock(g_cs);
	auto i = g_portal_frames.find(pszName);
	
	if (i != g_portal_frames.end())
	{
		::PostMessage(i->second->m_hWnd, WM_CLOSE, NULL, NULL);
	}
}

void PortalBrowserFrame::Cleanup()
{
	__try
	{
		if (m_hBrowser && m_DestroyBrowser)
		{
			m_DestroyBrowser(m_hBrowser);
			m_hBrowser = NULL;
		}
	}
	__finally
	{
		if (m_hLib)
			FreeLibrary(m_hLib);
	}
}
static void LoadExternalPortalBrowserXML()
{
	const string l_url = SETTING(PORTAL_BROWSER_UPDATE_URL);
	if (l_url.empty())
		return;
		
	const tstring l_filename = Text::toT(Util::getConfigPath(
#ifndef USE_SETTINGS_PATH_TO_UPDATA_DATA
	                                         true
#endif
	                                     )) + _T("PortalBrowser\\PortalBrowser.xml");
	string l_data;
	const string l_log_message = STRING(DOWNLOAD) + ": " + l_url;
	int l_size = Util::getDataFromInet(l_url, l_data, 0);
	if (l_size == 0)
	{
		LogManager::message(l_log_message + " [" + STRING(ERROR_STRING) + ']');
	}
	else
	{
		LogManager::message(l_log_message + " [Ok]");
		std::ofstream l_file_out(l_filename.c_str());
		if (!l_file_out.is_open())
		{
			LogManager::message("Error create PortalBrowser.xml file: " + Text::fromT(l_filename));
		}
		l_file_out.write(l_data.c_str(), l_data.size());
		LogManager::message(Text::fromT(l_filename) + " [" + STRING(SAVE) + ']');
	}
}
LRESULT PortalBrowserFrame::onCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bool bRet = false;
	
	baseClass::onCreate(uMsg, wParam, lParam, bHandled);
	
	if (!lParam)
		return TRUE;
		
	__try
	{
#ifdef _WIN64
		m_hLib = LoadLibrary(L"PortalBrowser\\PortalBrowser_x64.dll");
#else
		m_hLib = LoadLibrary(L"PortalBrowser\\PortalBrowser.dll");
#endif
		
		if (!m_hLib)
		{
			RECT rect;
			GetClientRect(&rect);
			
#ifdef _WIN64
			CreateWindowEx(0, WC_STATIC, L"Failed to load PortalBrowser\\PortalBrowser_x64.dll", WS_VISIBLE | WS_CHILD, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, m_hWnd, NULL, (HINSTANCE)GetWindowLongPtr(GWLP_HINSTANCE), NULL);
#else
			CreateWindowEx(0, WC_STATIC, L"Failed to load PortalBrowser\\PortalBrowser.dll", WS_VISIBLE | WS_CHILD, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, m_hWnd, NULL, (HINSTANCE)GetWindowLongPtr(GWLP_HINSTANCE), NULL);
#endif
			__leave;
		}
		CREATEBROWSER CreateBrowser = (CREATEBROWSER)GetProcAddress(m_hLib, "CreateBrowser");
		m_DestroyBrowser = (DESTROYBROWSER)GetProcAddress(m_hLib, "DestroyBrowser");
		m_GetBrowserWnd = (GETBROWSERWND)GetProcAddress(m_hLib, "GetBrowserWnd");
		m_TranslateBrowserAccelerator = (TRANSLATEBROWSERACCELERATOR)GetProcAddress(m_hLib, "TranslateBrowserAccelerator");
		
		if (!CreateBrowser || !m_DestroyBrowser || !m_GetBrowserWnd)
		{
			RECT rect;
			GetClientRect(&rect);
			
#ifdef _WIN64
			CreateWindowEx(0, WC_STATIC, L"Can't find needed functions in PortalBrowser\\PortalBrowser_x64.dll", WS_VISIBLE | WS_CHILD, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, m_hWnd, NULL, (HINSTANCE)GetWindowLongPtr(GWLP_HINSTANCE), NULL);
#else
			CreateWindowEx(0, WC_STATIC, L"Can't find needed functions in PortalBrowser\\PortalBrowser.dll", WS_VISIBLE | WS_CHILD, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, m_hWnd, NULL, (HINSTANCE)GetWindowLongPtr(GWLP_HINSTANCE), NULL);
#endif
			__leave;
		}
		
		if (CreateBrowser)
		{
			CREATESTRUCT *pcs = (CREATESTRUCT*)lParam;
			MDICREATESTRUCT *pmcs = (MDICREATESTRUCT *)pcs->lpCreateParams;
			
			if (pmcs)
			{
				INIT_STRUCT *pis = (INIT_STRUCT *)pmcs->lParam;
				
				if (pis)
				{
					PORTAL_BROWSER_ITEM_STRUCT const *pPortal = GetPortalBrowserListData(m_wID - IDC_PORTAL_BROWSER);
					
					if (pPortal)
					{
						LoadExternalPortalBrowserXML(); //[+]PPA
						m_hBrowser = CreateBrowser(m_hWnd, WM_PORTAL_BROWSER_EVENT, m_wID - IDC_PORTAL_BROWSER, pPortal->strName.c_str(), pis->pszPortalLogin, pis->pszPortalPassword);
						setCustomIcon(pPortal->hIcon);
						setDirty(0);
					}
				}
			}
		}
		
		if (m_hBrowser)
			bRet = true;
	}
	__finally
	{
		if (!bRet)
		{
			if (m_hLib)
			{
				FreeLibrary(m_hLib);
				m_hLib = NULL;
			}
		}
	}
	
	bHandled = TRUE;
	return bRet;
}

void PortalBrowserFrame::Resize()
{
	__try
	{
		if (m_hBrowser && m_GetBrowserWnd)
		{
			HWND hBrowserWnd = m_GetBrowserWnd(m_hBrowser);
			RECT rect;
			GetClientRect(&rect);
			::SetWindowPos(hBrowserWnd, NULL, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);
		}
	}
	__finally
	{
	}
}

LRESULT PortalBrowserFrame::onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{

	if (m_LastSizeState != wParam)
	{
		// If maximized/minimized state changed
		
		if (m_bActive)
		{
			// use m_bSizing, to avoid nesting WM_SIZE processing.
			// calling ViewTransferView causes window change size
			
			bool bActive = false;
			bool bHostSizeChanged = false;
			
			if (wParam == SIZE_MAXIMIZED)
			{
				bActive = true;
				bHostSizeChanged = true;
			}
			else if (wParam == SIZE_MINIMIZED || wParam == SIZE_RESTORED)
			{
				bActive = wParam == SIZE_RESTORED;
				bHostSizeChanged = true;
			}
			
			if (bHostSizeChanged)
			{
				//HandleFullScreen(bActive);
				
				// N.B. HandleFullScreen should be called outside
				// of WM_SIZE processing, to avoid bugs in frame size
				PostMessage(WM_QUEUE_HANDLE_FULL_SCREEN, bActive);
			}
		}
	}
	
	Resize();
	
	m_LastSizeState = wParam;
	
	bHandled = FALSE;
	return FALSE;
}

void PortalBrowserFrame::CloseBrowserWindow()
{
	__try
	{
		if (m_GetBrowserWnd && m_hBrowser)
			::DestroyWindow(m_GetBrowserWnd(m_hBrowser));
			
		Cleanup();
	}
	__except(1)
	{
	}
}

LRESULT PortalBrowserFrame::onEvent(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	switch (wParam)
	{
		case MSG_MAGNET_LINK:
		{
			MAGNET_LINK_STRUCT *pml = (MAGNET_LINK_STRUCT*)lParam;
			WinUtil::DefinedMagnetAction Action = WinUtil::MA_DEFAULT;
			
			if (pml)
			{
				switch (pml->Action)
				{
					case MAGNET_DEFAULT:
						Action = WinUtil::MA_DEFAULT;
						break;
						
					case MAGNET_SEARCH:
						Action = WinUtil::MA_SEARCH;
						break;
						
					case MAGNET_DOWNLOAD:
						Action = WinUtil::MA_DOWNLOAD;
						break;
						
					case MAGNET_ASK:
						Action = WinUtil::MA_ASK;
						break;
				}
				
				WinUtil::parseMagnetUri(pml->pszURL, Action);
			}
		}
		break;
		
		case MSG_FAILED_TO_LOAD:
		{
			CloseBrowserWindow();
			
			std::wstring str(L"Failed to create PortalBrowser.\r\nReason: ");
			
			if (lParam >= _countof(s_ErrorStringTable))
				lParam = 0;
				
			str += s_ErrorStringTable[lParam];
			
			RECT rect;
			GetClientRect(&rect);
			
			CreateWindowEx(0, WC_STATIC, str.c_str(), WS_VISIBLE | WS_CHILD, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, m_hWnd, NULL, (HINSTANCE)GetWindowLongPtr(GWLP_HINSTANCE), NULL);
		}
		break;
	}
	
	return TRUE;
}

LRESULT PortalBrowserFrame::onMDIActivate(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	m_bActive = (HWND)lParam == m_hWnd;
	HandleFullScreen(WM_QUEUE_HANDLE_FULL_SCREEN, m_bActive, 0, bHandled);
	
	bHandled = FALSE;
	
	return TRUE;
}

LRESULT PortalBrowserFrame::HandleFullScreen(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bool bActive = wParam != 0;
	
	extern bool g_isShutdown;
	if (!g_isShutdown)
	{
		if (bActive && IsZoomed())
		{
			MainFrame::getMainFrame()->ViewTransferView(FALSE);
			
			DWORD dwState = MainFrame::getMainFrame()->UIGetState(ID_VIEW_TRANSFER_VIEW);
			MainFrame::getMainFrame()->UISetState(ID_VIEW_TRANSFER_VIEW, dwState | CUpdateUIBase::UPDUI_DISABLED);
		}
		else
		{
			if (BOOLSETTING(SHOW_TRANSFERVIEW))
				MainFrame::getMainFrame()->ViewTransferView(TRUE);
				
			DWORD dwState = MainFrame::getMainFrame()->UIGetState(ID_VIEW_TRANSFER_VIEW);
			MainFrame::getMainFrame()->UISetState(ID_VIEW_TRANSFER_VIEW, dwState & (~CUpdateUIBase::UPDUI_DISABLED));
		}
	}
	return TRUE;
}

static std::vector<PORTAL_BROWSER_ITEM_STRUCT> g_PortalList;

static HICON IconFromBitmap(HBITMAP hBMP)
{
	HICON hIcon = NULL;
	
	if (hBMP)
	{
		BITMAP bmp = {0};
		GetObject(hBMP, sizeof(bmp), &bmp);
		
		HIMAGELIST hImageList = ImageList_Create(bmp.bmWidth, bmp.bmHeight, ILC_COLOR32 | ILC_MASK, 1, 0);
		
		if (hImageList)
		{
			ImageList_Add(hImageList, hBMP, NULL);
			
			hIcon = ImageList_GetIcon(hImageList, 0, ILD_TRANSPARENT);
			
			ImageList_Destroy(hImageList);
		}
	}
	
	return hIcon;
}

static bool FillMenu(CMenuHandle &menu, IPortalList const *pPortalList)
{
	const size_t szCount = pPortalList->GetCount();
	bool bRet = false;
	
	if (szCount)
	{
		if (!menu.m_hMenu)
			menu.CreatePopupMenu();
			
		g_PortalList.reserve(szCount);
		for (size_t i = 0; i < szCount; i++)
		{
			const IPortalList::PORTAL_DATA* l_data = pPortalList->GetData(i);
			if (l_data && l_data->dwSize >= offsetof(IPortalList::PORTAL_DATA, dwFlags) + sizeof(l_data->dwFlags)) // Check if Data includes dwFlags member
			{
				if (l_data->dwFlags & IPortalList::F_HOST_MENU_NONE)
					continue;
			}
			
			bRet |= menu.AppendMenu(MF_STRING, IDC_PORTAL_BROWSER + i, pPortalList->GetName(i)) != FALSE;
			
			HBITMAP hSmallBMP = pPortalList->GetBitmap(i, true);
			HBITMAP hLargeBMP = pPortalList->GetBitmap(i, false);
			g_PortalList.push_back(PORTAL_BROWSER_ITEM_STRUCT(pPortalList->GetName(i), pPortalList->GetHubUrl(i), l_data, hLargeBMP, hSmallBMP, IconFromBitmap(hSmallBMP)));
		}
		g_PortalList.shrink_to_fit();
	}
	
	return bRet;
}

bool InitPortalBrowserMenuItems(CMenuHandle &menu)
{
	bool bRet = false;
	HMODULE hLib = nullptr;
	__try
	{
#ifdef _WIN64
		hLib = LoadLibrary(L"PortalBrowser\\PortalBrowser_x64.dll");
#else
		hLib = LoadLibrary(L"PortalBrowser\\PortalBrowser.dll");
#endif
		
		if (!hLib)
			__leave;
			
		GETPORTALLIST GetPortalList = (GETPORTALLIST)GetProcAddress(hLib, "GetPortalList");
		FREEPORTALLIST FreePortalList = (FREEPORTALLIST)GetProcAddress(hLib, "FreePortalList");
		
		if (!GetPortalList || !FreePortalList)
			__leave;
			
		IPortalList const *pPortalList = GetPortalList();
		
		if (!pPortalList)
			__leave;
			
		bRet = FillMenu(menu, pPortalList);
		
		FreePortalList(pPortalList);
	}
	__finally
	{
		if (hLib)
		{
			FreeLibrary(hLib);
		}
	}
	return bRet;
}

bool InitPortalBrowserMenuImages(CImageList &images, CMDICommandBarCtrl &CmdBar)
{
	bool bRet = false;
	size_t PortalsCount = GetPortalBrowserListCount();
	
	for (size_t i = 0; i < PortalsCount; i++)
	{
		const PORTAL_BROWSER_ITEM_STRUCT *pPortalItem = GetPortalBrowserListData(i);
		
		if (pPortalItem && pPortalItem->hSmallBMP)
		{
			images.Add(pPortalItem->hSmallBMP);
			CmdBar.m_arrCommand.Add(i + IDC_PORTAL_BROWSER);
			
			bRet = true;
		}
	}
	
	return bRet;
}

size_t InitPortalBrowserToolbarImages(CImageList &largeImages, CImageList &largeImagesHot)
{
	PORTAL_BROWSER_ITEM_STRUCT *pPBItem = nullptr;
	size_t i = 0, added = 0;
	while ((pPBItem = const_cast<PORTAL_BROWSER_ITEM_STRUCT*>(GetPortalBrowserListData(i++))) != NULL)
	{
		if (pPBItem->hLargeBMP)
		{
			pPBItem->iLargeImageInd = largeImages.Add(pPBItem->hLargeBMP);
			int imageHotInd = largeImagesHot.Add(pPBItem->hLargeBMP);
			
			dcassert(imageHotInd == pPBItem->iLargeImageInd);
			UNREFERENCED_PARAMETER(imageHotInd);
			
			added++;
		}
	}
	
	return added;
}

size_t InitPortalBrowserToolbarItems(CImageList &largeImages, CImageList &largeImagesHot, CToolBarCtrl &ctrlToolbar, bool bBeginOfToolbar)
{
	const size_t PortalsCount = GetPortalBrowserListCount();
	size_t added = 0;
	TBBUTTON nTB = {0};
	
	if (PortalsCount
	        && ctrlToolbar.IsWindow()
	   )
	{
		int iBtnCount = ctrlToolbar.GetButtonCount();
		
		const PORTAL_BROWSER_ITEM_STRUCT *pPBItem = nullptr;
		size_t i = 0;
		while ((pPBItem = GetPortalBrowserListData(i)) != NULL)
		{
			bool bWantBeFirst = false;
			
			if (pPBItem->Data.dwSize >= offsetof(IPortalList::PORTAL_DATA, dwFlags) + sizeof(pPBItem->Data.dwFlags))    // Check if Data includes dwFlags member
			{
				// Check if we don't want this portal on toolbar
				if (pPBItem->Data.dwFlags & IPortalList::F_HOST_TOOLBAR_ICON_NONE)
				{
					i++;
					continue;
				}
				
				bWantBeFirst = (pPBItem->Data.dwFlags & IPortalList::F_HOST_TOOLBAR_ICON_FIRST);
			}
			
			if (bBeginOfToolbar == bWantBeFirst)
			{
				memzero(&nTB, sizeof(nTB));
				nTB.iBitmap = pPBItem->iLargeImageInd;
				nTB.idCommand = IDC_PORTAL_BROWSER + i;
				nTB.fsStyle = TBSTYLE_CHECK;
				nTB.fsState = TBSTATE_ENABLED;
				nTB.iString = (INT_PTR)(pPBItem->strName.c_str());
				ctrlToolbar.AddButtons(1, &nTB);
				added++;
			}
			i++;
		}
		
		if (added)
		{
			memzero(&nTB, sizeof(nTB));
			if (bBeginOfToolbar)
			{
				nTB.fsStyle = TBSTYLE_SEP;
				ctrlToolbar.AddButtons(1, &nTB);
			}
			else
			{
				nTB.fsStyle = TBSTYLE_SEP;
				ctrlToolbar.InsertButton(iBtnCount, &nTB);
			}
		}
	}
	
	return added;
}

typedef bool (*ENUMVISIBLEPORTALSCALLBACK)(size_t ind, const PORTAL_BROWSER_ITEM_STRUCT *is, LPARAM lParam);

static bool EnumVisiblePortals(ENUMVISIBLEPORTALSCALLBACK Callback, LPARAM lParam)
{
	if (!Callback)
		return false;
		
	const size_t PortalsCount = GetPortalBrowserListCount();
	
	if (PortalsCount)
	{
		const PORTAL_BROWSER_ITEM_STRUCT *pPBItem = nullptr;
		size_t i = 0;
		
		while ((pPBItem = GetPortalBrowserListData(i)) != nullptr)
		{
			if (pPBItem->Data.dwSize >= offsetof(IPortalList::PORTAL_DATA, dwFlags) + sizeof(pPBItem->Data.dwFlags))    // Check if Data includes dwFlags member
			{
				// Check if we don't want this portal on toolbar
				if ((pPBItem->Data.dwFlags & IPortalList::F_HOST_TOOLBAR_ICON_NONE) == 0 ||
				        (pPBItem->Data.dwFlags & IPortalList::F_HOST_MENU_NONE) == 0)
				{
					if (!Callback(i, pPBItem, lParam))
						return false;
				}
			}
			
			i++;
		}
	}
	
	return true;
}

static bool IfHaveVisiblePortalsCallback(size_t ind, const PORTAL_BROWSER_ITEM_STRUCT *is, LPARAM lParam)
{
	LPDWORD pdwHaveVisiblePortals = reinterpret_cast<LPDWORD>(lParam);
	
	*pdwHaveVisiblePortals = 1;
	return false;
}

bool IfHaveVisiblePortals()
{
	DWORD dwHaveVisiblePortals = 0;
	EnumVisiblePortals(IfHaveVisiblePortalsCallback, reinterpret_cast<LPARAM>(&dwHaveVisiblePortals));
	
	return dwHaveVisiblePortals != 0;
}

bool OpenVisiblePortalsCallback(size_t ind, const PORTAL_BROWSER_ITEM_STRUCT *is, LPARAM lParam)
{
	PostMessage((HWND)lParam, WM_COMMAND, IDC_PORTAL_BROWSER + ind, 0);
	return true;
}

void OpenVisiblePortals(HWND hWnd)
{
	EnumVisiblePortals(OpenVisiblePortalsCallback, (LPARAM)hWnd);
}

PORTAL_BROWSER_ITEM_STRUCT const *GetPortalBrowserListData(size_t ind)
{
	PORTAL_BROWSER_ITEM_STRUCT *p = nullptr;
	
	if (ind < g_PortalList.size())
		p = &g_PortalList[ind];
		
	return p;
}

size_t GetPortalBrowserListCount()
{
	return g_PortalList.size();
}