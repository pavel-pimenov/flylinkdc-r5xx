/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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
#include "../client/QueueManagerListener.h"
#include "../client/DownloadManagerListener.h"

#define SHOWTREE_MESSAGE_MAP 12

class QueueFrame : public MDITabChildWindowImpl < QueueFrame, RGB(0, 0, 0), IDR_QUEUE >,
	public StaticFrame<QueueFrame, ResourceManager::DOWNLOAD_QUEUE, IDC_QUEUE>,
	private QueueManagerListener,
	private DownloadManagerListener,
	public CSplitterImpl<QueueFrame>,
	public PreviewBaseHandler<QueueFrame, false>,
	private SettingsManagerListener,
	virtual private CFlyTimerAdapter,
	virtual private CFlyTaskAdapter
{
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("QueueFrame"), IDR_QUEUE, 0, COLOR_3DFACE);
		
		QueueFrame();
		~QueueFrame();
		
		typedef MDITabChildWindowImpl < QueueFrame, RGB(0, 0, 0), IDR_QUEUE > baseClass;
		typedef CSplitterImpl<QueueFrame> splitBase;
		typedef PreviewBaseHandler<QueueFrame, false> prevBase;
		
		BEGIN_MSG_MAP(QueueFrame)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_GETDISPINFO, ctrlQueue.onGetDispInfo)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_COLUMNCLICK, ctrlQueue.onColumnClick)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_GETINFOTIP, ctrlQueue.onInfoTip)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_QUEUE, LVN_ITEMCHANGED, onItemChangedQueue)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		NOTIFY_HANDLER(IDC_QUEUE, NM_DBLCLK, onSearchDblClick)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimerTask)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchAlternates)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopyMagnet)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVE_ALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_RECHECK, onRecheck);
		COMMAND_ID_HANDLER(IDC_REMOVE_OFFLINE, onRemoveOffline)
		COMMAND_ID_HANDLER(IDC_MOVE, onMove)
		COMMAND_ID_HANDLER(IDC_RENAME, onRename)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
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
		CHAIN_COMMANDS(prevBase)
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
		
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onSearchDblClick(int idCtrl, LPNMHDR /*pnmh*/, BOOL& bHandled)
		{
			return onSearchAlternates(BN_CLICKED, (WORD)idCtrl, m_hWnd, bHandled);
		}
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void removeDir(HTREEITEM ht);
		void setPriority(HTREEITEM ht, const QueueItem::Priority& p);
		void setAutoPriority(HTREEITEM ht, const bool& ap);
		void changePriority(bool inc);
		
		LRESULT onItemChangedQueue(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			const NMLISTVIEW* lv = (NMLISTVIEW*)pnmh;
			if ((lv->uNewState & LVIS_SELECTED) != (lv->uOldState & LVIS_SELECTED))
			{
				m_update_status++;
			}
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
			COLUMN_TYPE,
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
			COLUMN_SPEED,
			COLUMN_LAST
		};
		enum Tasks
		{
			ADD_ITEM,
			REMOVE_ITEM,
			REMOVE_ITEM_ARRAY,
			UPDATE_ITEM,
			UPDATE_STATUSBAR,
			UPDATE_STATUS
		};
		StringList m_tmp_target_to_delete;
		
		std::vector<std::pair<std::string, UserPtr> > m_remove_source_array;
		void removeSources();
		void doTimerTask();
		
		class QueueItemInfo
		{
			public:
				explicit QueueItemInfo(const QueueItemPtr& p_qi) : m_qi(p_qi)
				{
				}
				explicit QueueItemInfo(const libtorrent::sha1_hash& p_sha1, const std::string& p_save_path) : m_sha1(p_sha1), m_save_path(p_save_path)
				{
				}
				const tstring getText(int col) const;
				static int compareItems(const QueueItemInfo* a, const QueueItemInfo* b, int col);
				void removeTarget(bool p_is_batch_remove);
				
				void removeBatch()
				{
					removeTarget(true);
				}
				int getImageIndex() const
				{
					return g_fileImage.getIconIndex(getTarget());
				}
				static uint8_t getStateImageIndex()
				{
					return 0;
				}
				bool isTorrent() const
				{
					if (m_qi)
						return false;
					else
						return true;
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
					if (!isTorrent())
						return (m_qi->getFlags() & aFlag) == aFlag;
					else
						return false; // TODO
				}
				bool isAnySet(Flags::MaskType aFlag) const
				{
					if (!isTorrent())
						return (m_qi->getFlags() & aFlag) != 0;
					else
						return false; // TODO
				}
				string getTarget() const
				{
					if (m_qi)
						return m_qi->getTarget();
					else
						return m_save_path; // TODO
				}
				int64_t getSize() const
				{
					if (m_qi)
						return m_qi->getSize();
					else
						return 0; // TODO
				}
				int64_t getDownloadedBytes() const
				{
					if (!isTorrent())
						return m_qi->getDownloadedBytes();
					else
						return  0; // TODO
				}
				time_t getAdded() const
				{
					if (!isTorrent())
						return m_qi->getAdded();
					else
						return GET_TIME(); // TODO
				}
				const TTHValue getTTH() const
				{
					if (!isTorrent())
						return m_qi->getTTH();
					else
						return TTHValue(); // TODO
				}
				QueueItem::Priority getPriority() const
				{
					if (!isTorrent())
						return m_qi->getPriority();
					else
						return QueueItem::Priority(); // TODO
				}
				bool isWaiting() const
				{
					if (!isTorrent())
						return m_qi->isWaiting();
					else
						return false; // TODO
				}
				bool isFinished() const
				{
					if (!isTorrent())
						return m_qi->isFinished();
					else
						return false; // TODO
				}
				bool getAutoPriority() const
				{
					if (!isTorrent())
						return m_qi->getAutoPriority();
					else
						return false; // TODO
				}
				
			private:
				const QueueItemPtr m_qi;
				const libtorrent::sha1_hash m_sha1;
				string m_save_path;
		};
		
		struct QueueItemInfoTask :  public Task
		{
			explicit QueueItemInfoTask(QueueItemInfo* p_ii) : m_ii(p_ii) { }
			QueueItemInfo* m_ii;
		};
		
		struct UpdateTask : public Task
		{
				explicit UpdateTask(const string& p_target, const libtorrent::sha1_hash& p_sha1) : m_target(p_target), m_sha1(p_sha1) { }
				explicit UpdateTask(const string& p_target) : m_target(p_target) { }
				const string& getTarget() const
				{
					return m_target;
				}
				libtorrent::sha1_hash m_sha1;
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
		unsigned m_last_count;
		int64_t  m_last_total;
		
		int menuItems;
		int readdItems;
		
		HTREEITEM m_fileLists;
		
		typedef pair<string, QueueItemInfo*> DirectoryMapPair;
		typedef std::unordered_multimap<string, QueueItemInfo*, noCaseStringHash, noCaseStringEq> QueueDirectoryMap;
		typedef QueueDirectoryMap::iterator QueueDirectoryIter;
		typedef QueueDirectoryMap::const_iterator QueueDirectoryIterC;
		typedef pair<QueueDirectoryIterC, QueueDirectoryIterC> QueueDirectoryPairC;
		QueueDirectoryMap m_directories;
		string curDir;
		
		TypedListViewCtrl<QueueItemInfo, IDC_QUEUE> ctrlQueue;
		CTreeViewCtrl ctrlDirs;
		
		CStatusBarCtrl ctrlStatus;
		int statusSizes[6]; // TODO: fix my size.
		
		int64_t m_queueSize;
		int m_queueItems;
		int m_update_status;
		
		static int columnIndexes[COLUMN_LAST];
		static int columnSizes[COLUMN_LAST];
		
		void addQueueList();
		void addQueueItem(QueueItemInfo* qi, bool noSort);
		
		HTREEITEM addDirectory(const string& dir, bool isFileList = false, HTREEITEM startAt = NULL);
		void removeDirectory(const string& dir, bool isFileList = false);
		void removeDirectories(HTREEITEM ht);
		
		void updateQueue();
		void updateQueueStatus();
		
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
		
		string getSelectedDir() const;
		string getDir(HTREEITEM ht) const;
		
		void removeItem(const string& p_target);
		void on(QueueManagerListener::Added, const QueueItemPtr& aQI) noexcept override;
		void on(QueueManagerListener::AddedArray, const std::vector<QueueItemPtr>& p_qi_array) noexcept override;
		void on(QueueManagerListener::Moved, const QueueItemPtr& aQI, const string& oldTarget) noexcept override;
		void on(QueueManagerListener::Removed, const QueueItemPtr& aQI) noexcept override;
		void on(QueueManagerListener::RemovedArray, const std::vector<string>& p_qi_array) noexcept override;
		void on(QueueManagerListener::TargetsUpdated, const StringList& p_targets) noexcept override;
		void on(QueueManagerListener::StatusUpdated, const QueueItemPtr& aQI) noexcept override;
		void on(QueueManagerListener::StatusUpdatedList, const QueueItemList& p_list) noexcept override;
		void on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept override;
		void on(SettingsManagerListener::Repaint) override;
		
		void onRechecked(const string& target, const string& message);
		
		void on(QueueManagerListener::RecheckStarted, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckNoFile, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckFileTooSmall, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckDownloadsRunning, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckNoTree, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckAlreadyFinished, const string& target) noexcept override;
		void on(QueueManagerListener::RecheckDone, const string& target) noexcept override;
		
		void on(DownloadManagerListener::RemoveTorrent, const libtorrent::sha1_hash& p_sha1) noexcept override;
		void on(DownloadManagerListener::CompleteTorrentFile, const std::string& p_file_name) noexcept override;
		void on(DownloadManagerListener::TorrentEvent, const DownloadArray&) noexcept override;
		void on(DownloadManagerListener::AddedTorrent, const libtorrent::sha1_hash& p_sha1, const std::string& p_save_path) noexcept override;
		
		
};

#endif // !defined(QUEUE_FRAME_H)

/**
 * @file
 * $Id: QueueFrame.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
