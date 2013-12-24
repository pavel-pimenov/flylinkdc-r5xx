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


#if !defined(WAITING_QUEUE_FRAME_H)
#define WAITING_QUEUE_FRAME_H

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "WinUtil.h"
#include "../client/UserInfoBase.h"
#include "../client/UploadManager.h"

#define SHOWTREE_MESSAGE_MAP 12

class WaitingUsersFrame : public MDITabChildWindowImpl < WaitingUsersFrame, RGB(0, 0, 0), IDR_UPLOAD_QUEUE > , public StaticFrame<WaitingUsersFrame, ResourceManager::WAITING_USERS, IDC_UPLOAD_QUEUE>,
	private UploadManagerListener, public CSplitterImpl<WaitingUsersFrame>,
	public UserInfoBaseHandler<WaitingUsersFrame>, // [+] IRainman fix: add user menu.
	private SettingsManagerListener,
	private CFlyTimerAdapter
#ifdef _DEBUG
	, virtual NonDerivable<WaitingUsersFrame>, boost::noncopyable // [+] IRainman fix.
#endif
{
		typedef UserInfoBaseHandler<WaitingUsersFrame> uiBase;
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("WaitingUsersFrame"), IDR_UPLOAD_QUEUE, 0, COLOR_3DFACE);
		
		WaitingUsersFrame() : CFlyTimerAdapter(m_hWnd), m_showTree(true),
			m_needsUpdateStatus(false), m_needsResort(false), m_spoken(false), // [+] IRainman opt.
			showTreeContainer(_T("BUTTON"), this, SHOWTREE_MESSAGE_MAP)
		{
			memzero(statusSizes, sizeof(statusSizes));
		}
		
		~WaitingUsersFrame()
		{
		}
		
		enum
		{
			ADD_ITEM,
			REMOVE,
			REMOVE_ITEM,
			UPDATE_ITEMS
		};
		
		typedef MDITabChildWindowImpl < WaitingUsersFrame, RGB(0, 0, 0), IDR_UPLOAD_QUEUE > baseClass;
		typedef CSplitterImpl<WaitingUsersFrame> splitBase;
		
		// Inline message map
		BEGIN_MSG_MAP(WaitingUsersFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		COMMAND_HANDLER(IDC_REMOVE, BN_CLICKED, onRemove)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_GETDISPINFO, m_ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_COLUMNCLICK, m_ctrlList.onColumnClick)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, NM_CUSTOMDRAW, onCustomDraw)
//      NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_ITEMCHANGED, onItemChangedQueue)
		NOTIFY_HANDLER(IDC_UPLOAD_QUEUE, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		
		CHAIN_COMMANDS(uiBase) // fix http://code.google.com/p/flylinkdc/issues/detail?id=1406
		CHAIN_MSG_MAP(splitBase)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(SHOWTREE_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onShowTree)
		END_MSG_MAP()
		
		// Message handlers
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (!tasks.empty())
			{
				speak();
			}
			return 0;
		}
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onShowTree(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			m_showTree = (wParam == BST_CHECKED);
			UpdateLayout(FALSE);
			LoadAll();
			return 0;
		}
		
		// Update control layouts
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
	private:
		static int columnSizes[]; // [!] IRainman fix: don't set size here!
		static int columnIndexes[]; // [!] IRainman fix: don't set size here!
		
		struct UserItem
		{
			UserPtr user;
			UserItem(const UserPtr& u) : user(u) { }
		};
		
		UserPtr getCurrentdUser()
		{
			HTREEITEM selectedItem = GetParentItem();
			return selectedItem ? reinterpret_cast<UserItem *>(ctrlQueued.GetItemData(selectedItem))->user : nullptr;
		}
		
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
			if (kd->wVKey == VK_DELETE)
			{
				if (m_ctrlList.getSelectedCount())
				{
					removeSelected();
				}
			}
			return 0;
		}
		
		LRESULT onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMTVKEYDOWN* kd = (NMTVKEYDOWN*) pnmh;
			if (kd->wVKey == VK_DELETE)
			{
				removeSelectedUser();
			}
			return 0;
		}
		
		void removeSelected();
		
		void removeSelectedUser()
		{
			const UserPtr& User = getCurrentdUser();
			if (User)
			{
				UploadManager::LockInstanceQueue lockedInstance; // [+] IRainman opt.
				lockedInstance->clearUserFilesL(User);
			}
			m_needsUpdateStatus = true; // [!] IRainman opt.
		}
		
		// Communication with manager
		void LoadAll();
		void UpdateSearch(size_t index, BOOL doDelete = TRUE);
		
		HTREEITEM GetParentItem();
		
		// Contained controls
		CButton ctrlShowTree;
		CContainedWindow showTreeContainer;
		
		bool m_showTree;
		
		UserList UQFUsers;
		
		typedef TypedListViewCtrl<UploadQueueItem, IDC_UPLOAD_QUEUE> CtrlList;
		CtrlList m_ctrlList;
	public:
		CtrlList& getUserList() // [+] IRainman fix: add user menu.
		{
			return m_ctrlList;
		}
	private:
		CTreeViewCtrl ctrlQueued;
		
		CStatusBarCtrl ctrlStatus;
		int statusSizes[4]; // TODO: fix my size.
		
		void AddFile(UploadQueueItemPtr aUQI);
		void RemoveFile(UploadQueueItemPtr aUQI);
		void RemoveUser(const UserPtr& aUser);
		
		void addAllFiles(Upload * /*aUser*/);
		void updateStatus();
		
		// [+] IRainman opt
		bool m_needsUpdateStatus;
		bool m_needsResort;
		TaskQueue tasks;
		bool m_spoken;
		
		void speak()
		{
			if (!m_spoken)
			{
				m_spoken = true;
				PostMessage(WM_SPEAKER);
			}
		}
		
		struct UploadQueueTask : public Task
#ifdef _DEBUG
				, virtual NonDerivable<UploadQueueTask>
#endif
		{
			UploadQueueTask(UploadQueueItem* item) : m_item(item)
			{
				m_item->inc();
			}
			~UploadQueueTask()
			{
				m_item->dec();
			}
			UploadQueueItem* getItem() const
			{
				return m_item;
			}
			UploadQueueItem* m_item;
		};
		
		struct UserTask : public Task
#ifdef _DEBUG
				, virtual NonDerivable<UserTask>
#endif
		{
			UserTask(const UserPtr& user) : m_user(user)
			{
			}
			const UserPtr& getUser() const
			{
				return m_user;
			}
			UserPtr m_user;
		};
		// [~] IRainman opt
		
		// UploadManagerListener
		void on(UploadManagerListener::QueueAdd, UploadQueueItem* aUQI) noexcept
		{
			tasks.add(ADD_ITEM, new UploadQueueTask(aUQI));
		}
		void on(UploadManagerListener::QueueRemove, const UserPtr& aUser) noexcept
		{
			tasks.add(REMOVE, new UserTask(aUser));
		}
		void on(UploadManagerListener::QueueItemRemove, UploadQueueItem* aUQI) noexcept
		{
			tasks.add(REMOVE_ITEM, new UploadQueueTask(aUQI));
		}
		void on(UploadManagerListener::QueueUpdate) noexcept;
		// SettingsManagerListener
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept;
		
};

#endif
