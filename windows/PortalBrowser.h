#pragma once
#include "../PortalBrowser/PortalBrowser.h"

#include "TextFrame.h"

class PortalBrowserFrame : public MDITabChildWindowImpl < PortalBrowserFrame, RGB(0, 0, 0), IDR_PORTAL_BROWSER >
{
		enum DefinedWindowMessages {WM_PORTAL_BROWSER_EVENT = WM_USER + 1, WM_QUEUE_HANDLE_FULL_SCREEN};
		
	public:
		typedef MDITabChildWindowImpl < PortalBrowserFrame, RGB(0, 0, 0), IDR_PORTAL_BROWSER > baseClass;
		
		DECLARE_FRAME_WND_CLASS_EX(_T("PortalBrowserFrame"), IDR_PORTAL_BROWSER, 0, COLOR_3DFACE);
		
		BEGIN_MSG_MAP(PortalBrowserFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_PORTAL_BROWSER_EVENT, onEvent)
		MESSAGE_HANDLER(WM_QUEUE_HANDLE_FULL_SCREEN, HandleFullScreen)
		MESSAGE_HANDLER(WM_MDIACTIVATE, onMDIActivate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSize(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onSizing(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onEvent(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onMDIActivate(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT HandleFullScreen(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		
		PortalBrowserFrame(LPCWSTR pszName, WORD wID);
		~PortalBrowserFrame();
		
		struct INIT_STRUCT
		{
			LPCWSTR pszPortalLogin;
			LPCWSTR pszPortalPassword;
		};
		
		static void openWindow(WORD wID);
		static bool isMDIChildActive(HWND hWnd);
		static void closeWindow(LPCWSTR pszName);
		
		static BOOL PreTranslateMessage(MSG* pMsg);
		
	private:
		HMODULE m_hLib;
		
		DESTROYBROWSER m_DestroyBrowser;
		GETBROWSERWND m_GetBrowserWnd;
		TRANSLATEBROWSERACCELERATOR m_TranslateBrowserAccelerator;
		HANDLE m_hBrowser;
		
		static std::map<std::wstring, PortalBrowserFrame*> m_Frames;
		static FastCriticalSection m_cs; // [!] IRainman opt: use spin lock here.
		
		std::wstring m_strName;
		bool m_bSizing;
		bool m_bActive;
		
		WORD m_wID;
		WPARAM m_LastSizeState;
		
		void Cleanup();
		void CloseBrowserWindow();
		BOOL PreTranslateMessage_ByInstance(MSG* pMsg);
		void Resize();
};

struct PORTAL_BROWSER_ITEM_STRUCT
{
	PORTAL_BROWSER_ITEM_STRUCT(LPCWSTR pszName, LPCWSTR pszHubUrl, IPortalList::PORTAL_DATA const *pData, HBITMAP _hLargeBMP, HBITMAP _hSmallBMP, HICON _hIcon):
		hLargeBMP(_hLargeBMP),
		hSmallBMP(_hSmallBMP),
		hIcon(_hIcon)
	{
		if (pszName)
			strName = pszName;
		if (pszHubUrl)
			strHubUrl = pszHubUrl;
			
		if (pData)
			Data = *pData;
		else
			Data.dwSize = 0;
			
		iLargeImageInd = -1;
	}
	
	std::wstring strName;
	std::wstring strHubUrl;
	IPortalList::PORTAL_DATA Data;
	HBITMAP hLargeBMP;
	HBITMAP hSmallBMP;
	
	HICON hIcon;
	
	int iLargeImageInd; // inited while BMP image added to ImageList
};

bool InitPortalBrowserMenuItems(CMenuHandle &menu);
bool InitPortalBrowserMenuImages(CImageList &images, CMDICommandBarCtrl &CmdBar);
size_t InitPortalBrowserToolbarItems(CImageList &largeImages, CImageList &largeImagesHot, CToolBarCtrl &ctrlToolbar, bool bBeginOfToolbar);
size_t InitPortalBrowserToolbarImages(CImageList &largeImages, CImageList &largeImagesHot);

PORTAL_BROWSER_ITEM_STRUCT const *GetPortalBrowserListData(size_t ind);
size_t GetPortalBrowserListCount();
bool IfHaveVisiblePortals();
void OpenVisiblePortals(HWND hWnd);