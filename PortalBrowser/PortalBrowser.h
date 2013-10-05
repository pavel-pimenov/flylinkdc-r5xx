#pragma once
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