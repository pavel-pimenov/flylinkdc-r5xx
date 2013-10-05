#pragma once
#ifdef RIP_USE_SKIN

class CBannerCtrl: public CSkinableCmdBar
{
		std::vector<std::wstring> m_strURLs;
	public:
		DWORD AddBanner(int iImageID, LPCWSTR pszURL)
		{
			m_strURLs.push_back(pszURL);
			return AddItem(iImageID, 0, BS_PUSHBUTTON, 0, (LPARAM)m_strURLs.size() - 1);
		}
		
		void OnCommand(DWORD dwItem, ITEM_STRUCT &is)
		{
			if ((size_t)is.lParam < m_strURLs.size())
				ShellExecute(NULL, NULL, m_strURLs[is.lParam].c_str(), NULL, NULL, SW_SHOW);
		}
};

class CSkinManager
{
		CSkinableCmdBar m_ctrlToolbar;
		CBannerCtrl m_ctrlBannerbar;
		
	public:
		bool LoadSkin(HWND hParentWnd, LPCWSTR pszSkinXML);
		void OnSize(RECT *rect);
		
		bool HaveToolbar();
		HWND GetToolbarHandle();
		
		bool HaveBannerbar();
		HWND GetBannerHandle();
		
		bool HaveTabbar();
		ITabCtrl *GetTabbarCtrl();
};
#endif