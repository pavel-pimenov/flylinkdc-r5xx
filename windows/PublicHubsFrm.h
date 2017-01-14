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

#if !defined(PUBLIC_HUBS_FRM_H)
#define PUBLIC_HUBS_FRM_H

#pragma once

#include "FlatTabCtrl.h"
#include "ExListViewCtrl.h"
#include "Resource.h"

#include "../client/FavoriteManager.h"
#include "../client/StringSearch.h"

#define HUB_FILTER_MESSAGE_MAP 8
#define HUB_TREE_MESSAGE_MAP 9
#define HUB_LIST_MESSAGE_MAP 10
class PublicHubsFrame : public MDITabChildWindowImpl < PublicHubsFrame, RGB(0, 0, 0), IDR_INTERNET_HUBS > ,
	public StaticFrame<PublicHubsFrame, ResourceManager::PUBLIC_HUBS, ID_FILE_CONNECT>,
	public CSplitterImpl<PublicHubsFrame>,
	private SettingsManagerListener
{
	public:
		PublicHubsFrame() : users(0), m_hubs(0), visibleHubs(0), m_ISPRootItem(0), m_PublicListRootItem(0)
			, m_filterContainer(WC_EDIT, this, HUB_FILTER_MESSAGE_MAP)
			, m_treeContainer(WC_TREEVIEW, this, HUB_TREE_MESSAGE_MAP)
			, m_listContainer(WC_LISTVIEW, this, HUB_LIST_MESSAGE_MAP)
		{
		}
		
		~PublicHubsFrame() { }
		enum CFlyISPType
		{
			e_ISPCountry = 1,
			e_ISPCity = 2,
			e_ISPProvider = 3,
			e_ISPNetwork = 4,
			e_ISPHub = 5,
			e_ISPRoot = 6,
			e_HubListRoot = 7,
			e_HubListItem = 8
		};
		
		DECLARE_FRAME_WND_CLASS_EX(_T("PublicHubsFrame"), IDR_INTERNET_HUBS, 0, COLOR_3DFACE);
		
		typedef MDITabChildWindowImpl < PublicHubsFrame, RGB(0, 0, 0), IDR_INTERNET_HUBS > baseClass;
		BEGIN_MSG_MAP(PublicHubsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		COMMAND_ID_HANDLER(IDC_FILTER_FOCUS, onFilterFocus)
		COMMAND_ID_HANDLER(IDC_ADD, onAdd)
		COMMAND_ID_HANDLER(IDC_REM_AS_FAVORITE, onRemoveFav)
		COMMAND_ID_HANDLER(IDC_CONNECT, onClickedConnect)
		COMMAND_ID_HANDLER(IDC_COPY_HUB, onCopyHub);
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_COLUMNCLICK, onColumnClickHublist)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_RETURN, onEnter)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_DBLCLK, onDoubleClickHublist)
		
		NOTIFY_HANDLER(IDC_ISP_TREE, TVN_SELCHANGED, onSelChangedISPTree);
		
		//NOTIFY_HANDLER(IDC_HUBLIST, NM_CUSTOMDRAW, m_ctrlHubs.onCustomDraw) // [+] IRainman
		NOTIFY_HANDLER(IDC_HUBLIST, NM_CUSTOMDRAW, onCustomDraw)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(CSplitterImpl<PublicHubsFrame>)
		ALT_MSG_MAP(HUB_TREE_MESSAGE_MAP)
		ALT_MSG_MAP(HUB_LIST_MESSAGE_MAP)
		ALT_MSG_MAP(HUB_FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
		END_MSG_MAP()
		
		LRESULT onFilterChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDoubleClickHublist(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onEnter(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onFilterFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemoveFav(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onClickedConnect(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onCopyHub(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSelChangedISPTree(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		bool checkNick();
		
		void loadISPHubs();
		void loadPublicListHubs();
		void parseISPHubsLine(const string& p_line, CFlyLog& p_log);
		static int calcISPCountryIconIndex(tstring& p_country);
		LRESULT onCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		
		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			m_ctrlHubs.SetFocus();
			return 0;
		}
		
	private:
		// TODO - возможно эта хитрая цепочка мапок не нужна.
		struct CFlyTreeItemBase
		{
			HTREEITEM m_tree_item;
			CFlyTreeItemBase(): m_tree_item(0)
			{
			}
		};
		struct CFlyISPHubItem : public CFlyTreeItemBase
		{
			boost::unordered_map<string, CFlyTreeItemBase> m_hubs;
		};
		struct CFlyISPNetworkItem : public CFlyTreeItemBase
		{
			boost::unordered_map<string, CFlyISPHubItem> m_network;
		};
		struct CFlyISPProviderItem : public CFlyTreeItemBase
		{
			boost::unordered_map<string, CFlyISPNetworkItem> m_isp;
		};
		struct CFlyISPCityItem : public CFlyTreeItemBase
		{
			boost::unordered_map<string, CFlyISPProviderItem> m_city;
		};
		struct CFlyISPCountryItem
		{
			boost::unordered_map<string, CFlyISPCityItem> m_country;
		};
		CFlyISPCountryItem m_country_map;
		string m_isp_raw_data;
		enum
		{
			COLUMN_FIRST,
			COLUMN_NAME = COLUMN_FIRST,
			COLUMN_DESCRIPTION,
			COLUMN_USERS,
			COLUMN_SERVER,
			COLUMN_COUNTRY,
			COLUMN_SHARED,
			COLUMN_MINSHARE,
			COLUMN_MINSLOTS,
			COLUMN_MAXHUBS,
			COLUMN_MAXUSERS,
			COLUMN_RELIABILITY,
			COLUMN_RATING,
			COLUMN_LAST
		};
		
		enum
		{
			SET_TEXT
		};
		
		enum FilterModes
		{
			NONE,
			EQUAL,
			GREATER_EQUAL,
			LESS_EQUAL,
			GREATER,
			LESS,
			NOT_EQUAL
		};
		
		int visibleHubs;
		int users;
		CStatusBarCtrl ctrlStatus;
		CButton ctrlFilterDesc;
		CEdit ctrlFilter;
		CMenu hubsMenu;
		
		CContainedWindow m_filterContainer;
		CComboBox ctrlFilterSel;
		ExListViewCtrl m_ctrlHubs;
		
		typedef boost::unordered_map<string, HubEntryList> PubListMap;
		PubListMap m_publicListMatrix;
		
		
		CContainedWindow        m_treeContainer;
		CTreeViewCtrl           m_ctrlTree;
		HTREEITEM               m_ISPRootItem;
		HTREEITEM               m_PublicListRootItem;
		
		typedef boost::unordered_set<tstring> TStringSet;
		
		HTREEITEM LoadTreeItems(TStringSet& l_hubs, HTREEITEM hRoot);
		CContainedWindow        m_listContainer;
		
		HubEntry::List m_hubs;
		string m_filter;
		
		
		StringSet m_onlineHubs;
		bool isOnline(const string& p_hubUrl)
		{
			return m_onlineHubs.find(p_hubUrl) != m_onlineHubs.end();
		}
		static int columnIndexes[];
		static int columnSizes[];
		
		
		void speak(int x, const tstring& l)
		{
			safe_post_message(*this, x, new tstring(l));
		}
		
		void updateStatus();
		void updateList();
		void updateTree();
		
		const string getPubServer(int pos) const
		{
			return m_ctrlHubs.ExGetItemText(pos, COLUMN_SERVER);
		}
		void openHub(int ind); // [+] IRainman fix.
		
		bool parseFilter(FilterModes& mode, double& size);
		bool matchFilter(const HubEntry& entry, const int& sel, bool doSizeCompare, const FilterModes& mode, const double& size);
		
		void on(SettingsManagerListener::Repaint) override;
		
};

#endif // !defined(PUBLIC_HUBS_FRM_H)

/**
 * @file
 * $Id: PublicHubsFrm.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
