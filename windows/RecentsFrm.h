/*
 *
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

#if !defined(__RECENTS_FRAME_H__)
#define __RECENTS_FRAME_H__

#include "FlatTabCtrl.h"
#include "ExListViewCtrl.h"
#include "../client/FavoriteManager.h"

class RecentHubsFrame : public MDITabChildWindowImpl < RecentHubsFrame, RGB(0, 0, 0), IDR_RECENT_HUBS > , public StaticFrame<RecentHubsFrame, ResourceManager::RECENT_HUBS, IDC_RECENTS>,
	private FavoriteManagerListener, private SettingsManagerListener
#ifdef _DEBUG
	, virtual NonDerivable<RecentHubsFrame>, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		typedef MDITabChildWindowImpl < RecentHubsFrame, RGB(0, 0, 0), IDR_RECENT_HUBS > baseClass;
		
		RecentHubsFrame()  { }
		~RecentHubsFrame() { }
		
		DECLARE_FRAME_WND_CLASS_EX(_T("RecentHubsFrame"), IDR_RECENT_HUBS, 0, COLOR_3DFACE);
		
		
		BEGIN_MSG_MAP(RecentHubsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		COMMAND_ID_HANDLER(IDC_CONNECT, onClickedConnect)
		COMMAND_ID_HANDLER(IDC_ADD, onAdd)
		COMMAND_ID_HANDLER(IDC_REM_AS_FAVORITE, onRemoveFav)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVE_ALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		NOTIFY_HANDLER(IDC_RECENTS, LVN_COLUMNCLICK, onColumnClickHublist)
		NOTIFY_HANDLER(IDC_RECENTS, NM_DBLCLK, onDoubleClickHublist)
		NOTIFY_HANDLER(IDC_RECENTS, NM_RETURN, onEnter)
		NOTIFY_HANDLER(IDC_RECENTS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_RECENTS, LVN_ITEMCHANGED, onItemchangedDirectories)
		//NOTIFY_HANDLER(IDC_RECENTS, NM_CUSTOMDRAW, ctrlHubs.onCustomDraw) // [+] IRainman
		NOTIFY_HANDLER(IDC_RECENTS, NM_CUSTOMDRAW, onCustomDraw)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDoubleClickHublist(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onEnter(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemoveFav(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onClickedConnect(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemoveAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onItemchangedDirectories(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		
		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlHubs.SetFocus();
			return 0;
		}
		
		LRESULT onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
	private:
		enum
		{
			COLUMN_FIRST,
			COLUMN_NAME = COLUMN_FIRST,
			COLUMN_DESCRIPTION,
			COLUMN_USERS,
			COLUMN_SHARED,
			COLUMN_SERVER,
			COLUMN_DATETIME,
			COLUMN_LAST
		};
		
		CButton ctrlConnect;
		CButton ctrlRemove;
		CButton ctrlRemoveAll;
		CMenu hubsMenu;
		
		ExListViewCtrl ctrlHubs;
		
		
		static int columnSizes[COLUMN_LAST];
		static int columnIndexes[COLUMN_LAST];
		
		void updateList(const RecentHubEntry::List& fl);
		
		void addEntry(const RecentHubEntry* entry, int pos);
		
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		
		const string getRecentServer(int pos)const
		{
			return ctrlHubs.ExGetItemText(pos, COLUMN_SERVER);
		}
		
		void on(RecentAdded, const RecentHubEntry* entry) noexcept
		{
			addEntry(entry, ctrlHubs.GetItemCount());
		}
		void on(RecentRemoved, const RecentHubEntry* entry) noexcept
		{
			ctrlHubs.DeleteItem(ctrlHubs.find((LPARAM)entry));
		}
		void on(RecentUpdated, const RecentHubEntry* entry) noexcept;
		
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/);
};

#endif