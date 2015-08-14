// (c) Brain RIPper

#pragma once

#include "TextFrame.h"

// https://flylinkdc.googlecode.com/svn/extension/PortalBrowser/PortalBrowser/PortalBrowser.h

struct SEARCH_DATA_STRUCT
{
	enum DefinedTypes {SEARCH_NAME, SEARCH_TTH};
	enum DefinedFlags {FLAG_NONE, FLAG_DEFAULT};
	
	DefinedTypes type;
	DWORD dwFlags;
};

class IPortalList
{
	public:
		enum DefinedPortalFlags
		{
			F_NONE, F_HOST_TOOLBAR_ICON_FIRST = 1,
			F_HOST_TOOLBAR_ICON_NONE = 2,
			F_HOST_MENU_NONE = 4 //-V112
		};
		struct PORTAL_DATA
		{
			DWORD dwSize;
			DWORD dwFlags;
		};
		
		virtual size_t GetCount() const = 0;
		virtual LPCWSTR GetName(size_t indPortal) const = 0;
		virtual LPCWSTR GetHubUrl(size_t indPortal) const = 0;
		virtual PORTAL_DATA const *GetData(size_t indPortal) const = 0;
		
		virtual size_t GetSearchCount(size_t indPortal) const = 0;
		virtual LPCWSTR GetSearchName(size_t indPortal, size_t indSearch) const = 0;
		virtual SEARCH_DATA_STRUCT const *GetSearchData(size_t indPortal, size_t indSearch) const = 0;
		
		virtual HBITMAP GetBitmap(size_t indPortal, bool bSmall) const = 0;
};

// Creates/Closes Browser window. Have to be called GetMessage loop from calling thread
typedef HANDLE(__cdecl*CREATEBROWSER)(HWND hParentWnd, int iCallbackMessage, LPARAM lCallbackParam, LPCWSTR pszPortalName, LPCWSTR pszLogin, LPCWSTR pszPassword);
typedef bool (__cdecl*DESTROYBROWSER)(HANDLE hBrowser);
typedef HWND (__cdecl*GETBROWSERWND)(HANDLE hBrowser);
typedef IPortalList const *(__cdecl*GETPORTALLIST)();
typedef void (__cdecl *FREEPORTALLIST)(IPortalList const *pPortalList);
typedef  BOOL (__cdecl *TRANSLATEBROWSERACCELERATOR)(HANDLE hBrowser, MSG *pMsg);

// Follow messages passed as wParam with iCallbackMessage message
enum DefinedPortalBrowserMessages
{
	MSG_MAGNET_LINK,
	MSG_FAILED_TO_LOAD,
	MSG_WINDOW_TEXT_CHANGE,
};

enum DefinedErrorValues {ERROR_UNKNOWN, ERROR_XML_PARSE, ERROR_XML_VERSION, ERROR_IE_INIT};

enum DefinedMagnetAction {MAGNET_DEFAULT, MAGNET_ASK, MAGNET_DOWNLOAD, MAGNET_SEARCH};

// sends as lParam with MSG_MAGNET_LINK message
struct MAGNET_LINK_STRUCT
{
	LPARAM lParam;
	WCHAR *pszURL;
	DefinedMagnetAction Action;
};


// obsolete stuff

// Shows Browser and block current thread, until Browser is closed
//typedef void (*SHOWMODALBROWSER)(HWND hParentWnd, int iCallbackMessage, LPCWSTR pszPortalName, LPCWSTR pszLogin, LPCWSTR pszPassword);

// Creates modal Browser window in own thread and returns control.
// On Browser closing post iCallbackMessage to parent window. iCallbackMessage must be > WM_USER
//typedef  bool (*SHOWMODALBROWSERASYNC)(HWND hParentWnd, int iCallbackMessage, LPCWSTR pszPortalName, LPCWSTR pszLogin, LPCWSTR pszPassword);

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
		
		static std::map<std::wstring, PortalBrowserFrame*> g_portal_frames;
		static FastCriticalSection g_cs; // [!] IRainman opt: use spin lock here.
		
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