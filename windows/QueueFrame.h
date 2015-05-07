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

#if !defined(QUEUE_FRAME_H)
#define QUEUE_FRAME_H

#pragma once

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "../client/QueueManager.h"

#define SHOWTREE_MESSAGE_MAP 12

class QueueFrame : public MDITabChildWindowImpl < QueueFrame, RGB(0, 0, 0), IDR_QUEUE > ,
	public StaticFrame<QueueFrame, ResourceManager::DOWNLOAD_QUEUE, IDC_QUEUE>,
	private QueueManagerListener,
	public CSplitterImpl<QueueFrame>,
	public PreviewBaseHandler<QueueFrame, false>, // [+] IRainman fix.
	private SettingsManagerListener,
	virtual private CFlyTimerAdapter,
	virtual private CFlyTaskAdapter
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("QueueFrame"), IDR_QUEUE, 0, COLOR_3DFACE);
		
		QueueFrame();
		~QueueFrame();
		
		typedef MDITabChildWindowImpl < QueueFrame, RGB(0, 0, 0), IDR_QUEUE > baseClass;
		typedef CSplitterImpl<QueueFrame> splitBase;
		typedef PreviewBaseHandler<QueueFrame, false> prevBase; // [+] IRainman fix.
		
		BEGIN_MSG_MAP(QueueFrame)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_GETDISPINFO, ctrlQueue.onGetDispInfo)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_COLUMNCLICK, ctrlQueue.onColumnClick)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_GETINFOTIP, ctrlQueue.onInfoTip)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_ITEMCHANGED, onItemChangedQueue)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		NOTIFY_HANDLER(IDC_QUEUE, NM_DBLCLK, onSearchDblClick) // !SMT!-UI
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimerTask);
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchAlternates)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy) // !SMT!-UI
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy) // !SMT!-UI
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopyMagnet)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVE_ALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_RECHECK, onRecheck);
		COMMAND_ID_HANDLER(IDC_REMOVE_OFFLINE, onRemoveOffline)
		COMMAND_ID_HANDLER(IDC_MOVE, onMove)
		COMMAND_ID_HANDLER(IDC_RENAME, onRename)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		COMMAND_RANGE_HANDLER(IDC_COPY, IDC_COPY + COLUMN_LAST - 1, onCopy)
		COMMAND_RANGE_HANDLER(IDC_PRIORITY_PAUSED, IDC_PRIORITY_HIGHEST, onPriority)
		COMMAND_RANGE_HANDLER(IDC_SEGMENTONE, IDC_SEGMENTTWO_HUNDRED, onSegments)
		COMMAND_RANGE_HANDLER(IDC_BROWSELIST, IDC_BROWSELIST + menuItems, onBrowseList)
		COMMAND_RANGE_HANDLER(IDC_REMOVE_SOURCE, IDC_REMOVE_SOURCE + menuItems, onRemoveSource)
		COMMAND_RANGE_HANDLER(IDC_REMOVE_SOURCES, IDC_REMOVE_SOURCES + 1 + menuItems, onRemoveSources)
		COMMAND_RANGE_HANDLER(IDC_PM, IDC_PM + menuItems, onPM)
		COMMAND_RANGE_HANDLER(IDC_READD, IDC_READD + 1 + readdItems, onReadd)
		COMMAND_ID_HANDLER(IDC_AUTOPRIORITY, onAutoPriority)
		NOTIFY_HANDLER(IDC_QUEUE, NM_CUSTOMDRAW, onCustomDraw)
		CHAIN_MSG_MAP(splitBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_COMMANDS(prevBase) // [+] IRainman fix.
		ALT_MSG_MAP(SHOWTREE_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onShowTree)
		END_MSG_MAP()
		
		LRESULT onPriority(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSegments(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemoveSource(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemoveSources(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onReadd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRecheck(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT OnChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onAutoPriority(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onRemoveOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onSearchDblClick(int idCtrl, LPNMHDR /*pnmh*/, BOOL& bHandled)
		{
			return onSearchAlternates(BN_CLICKED, (WORD)idCtrl, m_hWnd, bHandled); // !SMT!-UI
		}
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void removeDir(HTREEITEM ht);
		void setPriority(HTREEITEM ht, const QueueItem::Priority& p);
		void setAutoPriority(HTREEITEM ht, const bool& ap);
		void changePriority(bool inc);
		
		LRESULT onItemChangedQueue(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLISTVIEW* lv = (NMLISTVIEW*)pnmh;
			if ((lv->uNewState & LVIS_SELECTED) != (lv->uOldState & LVIS_SELECTED))
				updateStatus();
			return 0;
		}
		
		LRESULT onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
		{
			if (ctrlQueue.IsWindow())
			{
				ctrlQueue.SetFocus();
			}
			return 0;
		}
		
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			usingDirMenu ? removeSelectedDir() : removeSelected();
			return 0;
		}
		LRESULT onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			removeAllDir();
			return 0;
		}
		
		LRESULT onMove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			usingDirMenu ? moveSelectedDir() : moveSelected();
			return 0;
		}
		
		LRESULT onRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			usingDirMenu ? renameSelectedDir() : renameSelected();
			return 0;
		}
		
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		
		LRESULT onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		
		void onTab();
		
		LRESULT onShowTree(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			showTree = (wParam == BST_CHECKED);
			UpdateLayout(FALSE);
			return 0;
		}
		
	private:
	
		enum
		{
			COLUMN_FIRST,
			COLUMN_TARGET = COLUMN_FIRST,
			COLUMN_STATUS,
			COLUMN_SEGMENTS,
			COLUMN_SIZE,
			COLUMN_PROGRESS,
			COLUMN_DOWNLOADED,
			COLUMN_PRIORITY,
			COLUMN_USERS,
			COLUMN_PATH,
			COLUMN_LOCAL_PATH,
			COLUMN_EXACT_SIZE,
			COLUMN_ERRORS,
			COLUMN_ADDED,
			COLUMN_TTH,
			COLUMN_LAST
		};
		enum Tasks
		{
			ADD_ITEM,
			REMOVE_ITEM,
			UPDATE_ITEM,
			UPDATE_STATUSBAR,//[+]IRainman optimize QueueFrame
			UPDATE_STATUS
		};
		StringList m_tmp_target_to_delete; // [+] NightOrion bugfix deleting folder from queue
		
		std::vector<std::pair<std::string, UserPtr> > m_remove_source_array;
		void removeSources();
		
		class QueueItemInfo;
		friend class QueueItemInfo;
		
		class QueueItemInfo
#ifdef _DEBUG
			: boost::noncopyable // [+] IRainman fix.
#endif
			
		{
			public:
				explicit QueueItemInfo(const QueueItemPtr& aQI) : m_qi(aQI)
				{
#ifdef _DEBUG
					m_size   = m_qi->getSize();
					m_Target = m_qi->getTarget();
#endif
				}
				
				~QueueItemInfo()
				{
				}
				
				void remove()
				{
					QueueManager::getInstance()->remove(getTarget());
				}
				
				// TypedListViewCtrl functions
				const tstring getText(int col) const;
				
				static int compareItems(const QueueItemInfo* a, const QueueItemInfo* b, int col)
				{
					switch (col)
					{
						case COLUMN_SIZE:
						case COLUMN_EXACT_SIZE:
							return compare(a->getSize(), b->getSize());
						case COLUMN_PRIORITY:
							return compare((int)a->getPriority(), (int)b->getPriority());
						case COLUMN_DOWNLOADED:
							return compare(a->getDownloadedBytes(), b->getDownloadedBytes());
						case COLUMN_ADDED:
							return compare(a->getAdded(), b->getAdded());
						default:
							return lstrcmpi(a->getText(col).c_str(), b->getText(col).c_str());
					}
				}
				int getImageIndex() const
				{
					return g_fileImage.getIconIndex(getTarget());
				}
				static uint8_t getStateImageIndex()
				{
					return 0;
				}
				
				const QueueItemPtr& getQueueItem() const
				{
					return m_qi;
				}
				string getPath() const
				{
					return Util::getFilePath(getTarget());
				}
				
				bool isSet(Flags::MaskType aFlag) const
				{
					return (m_qi->getFlags() & aFlag) == aFlag;
				}
				
				bool isAnySet(Flags::MaskType aFlag) const
				{
					return (m_qi->getFlags() & aFlag) != 0;
				}
				
				const string& getTarget() const
				{
					dcassert(m_Target == m_qi->getTarget());
					return m_qi->getTarget();
				}
				
				int64_t getSize() const
				{
					dcassert(m_size == m_qi->getSize())
					return m_qi->getSize();
				}
				int64_t getDownloadedBytes() const
				{
					return m_qi->getDownloadedBytes();
				}
				time_t getAdded() const
				{
					return m_qi->getAdded();
				}
				const TTHValue& getTTH() const
				{
					return m_qi->getTTH();
				}
				
				QueueItem::Priority getPriority() const
				{
					return m_qi->getPriority();
				}
				bool isWaitingL() const
				{
					return m_qi->isWaitingL();
				}
				bool isFinishedL() const
				{
					return m_qi->isFinishedL();
				}
				
				bool getAutoPriority() const
				{
					return m_qi->getAutoPriority();
				}
				
			private:
				const QueueItemPtr m_qi;
#ifdef _DEBUG
				string m_Target;
				int64_t m_size;
#endif
		};
		
		struct QueueItemInfoTask :  public Task
		{
			explicit QueueItemInfoTask(QueueItemInfo* p_ii) : m_ii(p_ii) { }
			QueueItemInfo* m_ii;
		};
		
		struct UpdateTask : public Task
		{
				explicit UpdateTask(const string& p_target) : m_target(p_target) { }
				const string& getTarget() const
				{
					return m_target;
				}
			private:
				const string m_target;
		};
		
		OMenu browseMenu;
		OMenu removeMenu;
		OMenu removeAllMenu;
		OMenu pmMenu;
		OMenu readdMenu;
		
		CButton ctrlShowTree;
		CContainedWindow showTreeContainer;
		bool showTree;
		
		bool usingDirMenu;
		
		bool m_dirty;
		
		int menuItems;
		int readdItems;
		
		HTREEITEM m_fileLists;
		
		typedef pair<string, QueueItemInfo*> DirectoryMapPair;
		typedef std::unordered_multimap<string, QueueItemInfo*, noCaseStringHash, noCaseStringEq> DirectoryMap;
		typedef DirectoryMap::iterator DirectoryIter;
		typedef DirectoryMap::const_iterator DirectoryIterC;
		typedef pair<DirectoryIterC, DirectoryIterC> DirectoryPairC;
		DirectoryMap m_directories;
		string curDir;
		
		TypedListViewCtrl<QueueItemInfo, IDC_QUEUE> ctrlQueue;
		CTreeViewCtrl ctrlDirs;
		
		CStatusBarCtrl ctrlStatus;
		int statusSizes[6]; // TODO: fix my size.
		
		int64_t queueSize;
		int queueItems;
		
		static int columnIndexes[COLUMN_LAST];
		static int columnSizes[COLUMN_LAST];
		
		void addQueueList();
		void addQueueItem(QueueItemInfo* qi, bool noSort);
		HTREEITEM addDirectory(const string& dir, bool isFileList = false, HTREEITEM startAt = NULL);
		void removeDirectory(const string& dir, bool isFileList = false);
		void removeDirectories(HTREEITEM ht);
		
		void updateQueue();
		void updateStatus();
		
		
		bool isCurDir(const string& aDir) const
		{
			return stricmp(curDir, aDir) == 0;
		}
		
		void moveSelected();
		void moveSelectedDir();
		void moveDir(HTREEITEM ht, const string& target);
		const QueueItemInfo* getSelectedQueueItem() const
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(ctrlQueue.GetNextItem(-1, LVNI_SELECTED));
			return ii;
		}
		
		void moveNode(HTREEITEM item, HTREEITEM parent);
		
		// temporary vector for moving directories
		typedef pair<QueueItemInfo*, string> TempMovePair;
		vector<TempMovePair> m_move_temp_array;
		void moveTempArray();
		
		void clearTree(HTREEITEM item);
		
		QueueItemInfo* getItemInfo(const string& target, const string& p_path) const;
		
		void removeSelected();
		void removeSelectedDir();
		void removeAllDir();
		
		void renameSelected();
		void renameSelectedDir();
		
		const string& getSelectedDir() const
		{
			HTREEITEM ht = ctrlDirs.GetSelectedItem();
			return ht == NULL ? Util::emptyString : getDir(ctrlDirs.GetSelectedItem());
		}
		
		const string& getDir(HTREEITEM ht) const
		{
			dcassert(ht != NULL);
			return *reinterpret_cast<string*>(ctrlDirs.GetItemData(ht));
		}
		
		void on(QueueManagerListener::Added, const QueueItemPtr& aQI) noexcept;
		void on(QueueManagerListener::AddedArray, const std::vector<QueueItemPtr>& p_qi_array) noexcept;
		void on(QueueManagerListener::Moved, const QueueItemPtr& aQI, const string& oldTarget) noexcept;
		void on(QueueManagerListener::Removed, const QueueItemPtr& aQI) noexcept;
		void on(QueueManagerListener::SourcesUpdated, const QueueItemPtr& aQI) noexcept;
		void on(QueueManagerListener::StatusUpdated, const QueueItemPtr& aQI) noexcept;
		void on(QueueManagerListener::StatusUpdatedList, const QueueItemList& p_list) noexcept; // [+] IRainman opt.
		void on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept; // [+] IRainman opt.
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/);
		
		void onRechecked(const string& target, const string& message);
		
		void on(QueueManagerListener::RecheckStarted, const string& target) noexcept;
		void on(QueueManagerListener::RecheckNoFile, const string& target) noexcept;
		void on(QueueManagerListener::RecheckFileTooSmall, const string& target) noexcept;
		void on(QueueManagerListener::RecheckDownloadsRunning, const string& target) noexcept;
		void on(QueueManagerListener::RecheckNoTree, const string& target) noexcept;
		void on(QueueManagerListener::RecheckAlreadyFinished, const string& target) noexcept;
		void on(QueueManagerListener::RecheckDone, const string& target) noexcept;
		
};

#endif // !defined(QUEUE_FRAME_H)

/**
 * @file
 * $Id: QueueFrame.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
