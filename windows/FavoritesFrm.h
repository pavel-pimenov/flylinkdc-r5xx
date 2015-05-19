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

#if !defined(FAVORITE_HUBS_FRM_H)
#define FAVORITE_HUBS_FRM_H

#include "FlatTabCtrl.h"
#include "ExListViewCtrl.h"

#include "../client/FavoriteManager.h"

#define SERVER_MESSAGE_MAP 7

class FavoriteHubsFrame : public MDITabChildWindowImpl < FavoriteHubsFrame, RGB(0, 0, 0), IDR_FAVORITES > , public StaticFrame<FavoriteHubsFrame, ResourceManager::FAVORITE_HUBS, IDC_FAVORITES>,
	private FavoriteManagerListener,
	private ClientManagerListener,
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
	private CFlyTimerAdapter,
#endif
	private SettingsManagerListener
{
	public:
		typedef MDITabChildWindowImpl < FavoriteHubsFrame, RGB(0, 0, 0), IDR_FAVORITES > baseClass;
		
		FavoriteHubsFrame() : CFlyTimerAdapter(m_hWnd), m_nosave(true) { }
		~FavoriteHubsFrame() { }
		
		DECLARE_FRAME_WND_CLASS_EX(_T("FavoriteHubsFrame"), IDR_FAVORITES, 0, COLOR_3DFACE);
		
		BEGIN_MSG_MAP(FavoriteHubsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		MESSAGE_HANDLER(WM_TIMER, onTimer);
#endif
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		COMMAND_ID_HANDLER(IDC_CONNECT, onClickedConnect)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_NEWFAV, onNew)
		COMMAND_ID_HANDLER(IDC_MOVE_UP, onMoveUp);
		COMMAND_ID_HANDLER(IDC_MOVE_DOWN, onMoveDown);
		COMMAND_ID_HANDLER(IDC_OPEN_HUB_LOG, onOpenHubLog)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		COMMAND_ID_HANDLER(IDC_MANAGE_GROUPS, onManageGroups)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_CUSTOMDRAW, ctrlHubs.onCustomDraw) // [+] IRainman
		NOTIFY_HANDLER(IDC_HUBLIST, NM_DBLCLK, onDoubleClickHublist)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_HUBLIST, LVN_COLUMNCLICK, onColumnClickHublist)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onNew(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onMoveUp(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onMoveDown(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onManageGroups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
#endif
		
		bool checkNick();
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
		LRESULT onEnter(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
		{
			openSelected();
			return 0;
		}
		
		LRESULT onClickedConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			openSelected();
			return 0;
		}
		
		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlHubs.SetFocus();
			return 0;
		}
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
	private:
	
		enum
		{
			COLUMN_FIRST,
			COLUMN_NAME = COLUMN_FIRST,
			COLUMN_DESCRIPTION,
			COLUMN_NICK,
			COLUMN_PASSWORD,
			COLUMN_SERVER,
			COLUMN_USERDESCRIPTION,
			COLUMN_EMAIL,
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
			COLUMN_HIDESHARE,
#endif
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
			COLUMN_CONNECTION_STATUS,
			COLUMN_LAST_SUCCESFULLY_CONNECTED,
#endif
			COLUMN_LAST
		};
		
		enum
		{
			HUB_CONNECTED,
			HUB_DISCONNECTED
		};
		struct StateKeeper
		{
				StateKeeper(ExListViewCtrl& hubs_, bool ensureVisible_ = true);
				~StateKeeper();
				
				const FavoriteHubEntryList& getSelection() const;
				
			private:
				ExListViewCtrl& hubs;
				bool ensureVisible;
				FavoriteHubEntryList selected;
				int scroll;
		};
		
		CButton ctrlConnect;
		CButton ctrlRemove;
		CButton ctrlNew;
		CButton ctrlProps;
		CButton ctrlUp;
		CButton ctrlDown;
		CButton ctrlManageGroups;
		OMenu hubsMenu;
		CImageList m_onlineStatusImg;
		StringSet m_onlineHubs;
		bool isOnline(const string& p_hubUrl)
		{
			return m_onlineHubs.find(p_hubUrl) != m_onlineHubs.end();
		}
		
		ExListViewCtrl ctrlHubs;
		
		bool m_nosave;
		
		static int columnSizes[COLUMN_LAST];
		static int columnIndexes[COLUMN_LAST];
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		tstring getLastAttempts(const ConnectionStatus& connectionStatus, const time_t curTime);
		tstring getLastSucces(const ConnectionStatus& connectionStatus, const time_t curTime);
#endif // IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		void addEntry(const FavoriteHubEntry* entry, int pos, int groupIndex);
		void handleMove(bool up);
		TStringList getSortedGroups() const;
		void fillList();
		void openSelected();
		
		void on(FavoriteAdded, const FavoriteHubEntry* /*e*/)  noexcept override
		{
			StateKeeper keeper(ctrlHubs);
			fillList();
		}
		void on(FavoriteRemoved, const FavoriteHubEntry* e) noexcept override
		{
			ctrlHubs.DeleteItem(ctrlHubs.find((LPARAM)e));
		}
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
#ifdef UPDATE_CON_STATUS_ON_FAV_HUBS_IN_REALTIME
		void on(FavoriteStatusChanged, const FavoriteHubEntry* e) noexcept override
		{
			const int pos = ctrlHubs.find((LPARAM)e);
			const ConnectionStatus& connectionStatus = e->getConnectionStatus();
			const time_t curTime = GET_TIME();
			
			ctrlHubs.SetItemText(pos, COLUMN_CONNECTION_STATUS, getLastAttempts(connectionStatus, curTime).c_str());
			ctrlHubs.SetItemText(pos, COLUMN_LAST_SUCCESFULLY_CONNECTED, getLastSucces(connectionStatus, curTime).c_str());
		}
#endif // UPDATE_CON_STATUS_ON_FAV_HUBS_IN_REALTIME
		
#endif // IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/) override;
		
		void on(ClientConnected, const Client* c) noexcept override
		{
			PostMessage(WM_SPEAKER, (WPARAM)HUB_CONNECTED, (LPARAM)new string(c->getHubUrl()));
		}
		void on(ClientDisconnected, const Client* c) noexcept override
		{
			PostMessage(WM_SPEAKER, (WPARAM)HUB_DISCONNECTED, (LPARAM)new string(c->getHubUrl()));
		}
		
};

#endif // !defined(FAVORITE_HUBS_FRM_H)

/**
 * @file
 * $Id: FavoritesFrm.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
