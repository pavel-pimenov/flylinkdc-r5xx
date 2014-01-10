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

#if !defined(TRANSFER_VIEW_H)
#define TRANSFER_VIEW_H

#pragma once

#include "../client/DownloadManagerListener.h"
#include "../client/UploadManagerListener.h"
#include "../client/ConnectionManagerListener.h"
#include "../client/QueueManagerListener.h"
#include "../client/TaskQueue.h"
#include "../client/forward.h"
#include "../client/Util.h"
#include "../client/Download.h"
#include "../client/Upload.h"

#include "OMenu.h"
#include "UCHandler.h"
#include "TypedListViewCtrl.h"
#include "SearchFrm.h"
#include "ExMessageBox.h" // [+] InfinitySky. From Apex.

class TransferView : public CWindowImpl<TransferView>, private DownloadManagerListener,
	private UploadManagerListener,
	private ConnectionManagerListener,
	private QueueManagerListener,
	public UserInfoBaseHandler<TransferView, UserInfoGuiTraits::NO_COPY>,
	public PreviewBaseHandler<TransferView>, // [+] IRainman fix.
	public UCHandler<TransferView>,
	private SettingsManagerListener,
	private CFlyTimerAdapter
{
	public:
		DECLARE_WND_CLASS(_T("TransferView"))
		
		TransferView() : CFlyTimerAdapter(m_hWnd)
		{
		}
		~TransferView();
		
		typedef UserInfoBaseHandler<TransferView, UserInfoGuiTraits::NO_COPY> uibBase;
		typedef PreviewBaseHandler<TransferView> prevBase; // [+] IRainman fix.
		typedef UCHandler<TransferView> ucBase;
		
		BEGIN_MSG_MAP(TransferView)
		NOTIFY_HANDLER(IDC_TRANSFERS, LVN_GETDISPINFO, ctrlTransfers.onGetDispInfo)
		NOTIFY_HANDLER(IDC_TRANSFERS, LVN_COLUMNCLICK, ctrlTransfers.onColumnClick)
		NOTIFY_HANDLER(IDC_TRANSFERS, LVN_GETINFOTIP, ctrlTransfers.onInfoTip)
		NOTIFY_HANDLER(IDC_TRANSFERS, LVN_KEYDOWN, onKeyDownTransfers)
		NOTIFY_HANDLER(IDC_TRANSFERS, NM_CUSTOMDRAW, onCustomDraw)
		NOTIFY_HANDLER(IDC_TRANSFERS, NM_DBLCLK, onDoubleClickTransfers)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_TIMER, onTimer);
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_NOTIFYFORMAT, onNotifyFormat)
		COMMAND_ID_HANDLER(IDC_FORCE, onForce)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchAlternates)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVEALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchAlternates)
		COMMAND_ID_HANDLER(IDC_DISCONNECT_ALL, onDisconnectAll)
		COMMAND_ID_HANDLER(IDC_COLLAPSE_ALL, onCollapseAll)
		COMMAND_ID_HANDLER(IDC_EXPAND_ALL, onExpandAll)
		COMMAND_ID_HANDLER(IDC_MENU_SLOWDISCONNECT, onSlowDisconnect)
		MESSAGE_HANDLER_HWND(WM_INITMENUPOPUP, OMenu::onInitMenuPopup)
		MESSAGE_HANDLER_HWND(WM_MEASUREITEM, OMenu::onMeasureItem)
		MESSAGE_HANDLER_HWND(WM_DRAWITEM, OMenu::onDrawItem)
		COMMAND_ID_HANDLER(IDC_PRIORITY_PAUSED, onPauseSelectedItem)
		
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy) // !JhaoDa
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy) // !SMT!-UI
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy) // !SMT!-UI
		COMMAND_RANGE_HANDLER(IDC_COPY, IDC_COPY + COLUMN_LAST - 1, onCopy) // !SMT!-UI
		
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uibBase)
		CHAIN_COMMANDS(prevBase) // [+] IRainman fix.
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onForce(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onDoubleClickTransfers(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onDisconnectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSlowDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (!m_tasks.empty())
			{
				speak();
			}
			return 0;
		}
		
		void runUserCommand(UserCommand& uc);
		void prepareClose();
		
		LRESULT onCollapseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			CollapseAll();
			return 0;
		}
		
		LRESULT onExpandAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			ExpandAll();
			return 0;
		}
		
		LRESULT onKeyDownTransfers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
			if (kd->wVKey == VK_DELETE)
			{
				ctrlTransfers.forEachSelected(&ItemInfo::disconnect);
			}
			return 0;
		}
		
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			ctrlTransfers.forEachSelected(&ItemInfo::disconnect);
			return 0;
		}
		
		LRESULT onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			UINT checkState = BOOLSETTING(CONFIRM_DELETE) ? BST_UNCHECKED : BST_CHECKED; // [+] InfinitySky.
			if (checkState == BST_CHECKED || ::MessageBox(0, CTSTRING(REALLY_REMOVE), T_APPNAME_WITH_VERSION, CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES) // [~] InfinitySky.
				ctrlTransfers.forEachSelected(&ItemInfo::removeAll); // [6] https://www.box.net/shared/4eed8e2e275210b6b654
			// Let's update the setting unchecked box means we bug user again...
			SET_SETTING(CONFIRM_DELETE, checkState != BST_CHECKED); // [+] InfinitySky.
			return 0;
		}
		
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlTransfers.DeleteAndClearAllItems(); // [!] IRainman
			return 0;
		}
		
		LRESULT onNotifyFormat(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
#ifdef _UNICODE
			return NFR_UNICODE;
#else
			return NFR_ANSI;
#endif
		}
		
		// [+] Flylink
		LRESULT onPauseSelectedItem(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PauseSelectedTransfer();
			return 0;
		}
		// [+] Flylink
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // !SMT!-UI
		// !SMT!-S
		LRESULT onSetUserLimit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
	private:
		class ItemInfo;
	public:
		typedef TypedTreeListViewCtrl<ItemInfo, IDC_TRANSFERS, tstring, noCaseStringHash, noCaseStringEq> ItemInfoList;
		ItemInfoList& getUserList()
		{
			return ctrlTransfers;
		}
	private:
		enum
		{
			ADD_ITEM,
			REMOVE_ITEM,
			UPDATE_ITEM,
			UPDATE_PARENT,
			UPDATE_PARENT_WITH_PARSE, // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=1082
			//UPDATE_PARENT_WITH_PARSE_2
		};
		
		enum
		{
			COLUMN_FIRST,
			COLUMN_USER = COLUMN_FIRST,
			COLUMN_HUB,
			COLUMN_STATUS,
			COLUMN_TIMELEFT,
			COLUMN_SPEED,
			COLUMN_FILE,
			COLUMN_SIZE,
			COLUMN_PATH,
			COLUMN_CIPHER,
			COLUMN_LOCATION,
			COLUMN_IP,
#ifdef PPA_INCLUDE_COLUMN_RATIO
			COLUMN_RATIO,
#endif
			COLUMN_SHARE, //[+]PPA
			COLUMN_SLOTS, //[+]PPA
			COLUMN_LAST
		};
		
		enum
		{
			IMAGE_DOWNLOAD = 0,
			IMAGE_UPLOAD,
			IMAGE_SEGMENT
		};
		struct CFlyTargetInfo
		{
				friend class TransferView;
				bool m_isFilelist;
			protected:
				CFlyTargetInfo() : m_isFilelist(false)
				{
				}
			public:
				bool isFileList() const
				{
					return m_isFilelist;
				}
				void parseTarget(const string& p_target)
				{
					m_isFilelist = p_target.find(Util::getListPath()) != string::npos ||
					               p_target.find(Util::getConfigPath()) != string::npos;
				}
		};
		struct UpdateInfo;
		class ItemInfo : public UserInfoBase, public CFlyTargetInfo
#ifdef _DEBUG
			, virtual NonDerivable<ItemInfo> // [+] IRainman fix.
#endif
		{
			public:
				enum Status
				{
					STATUS_RUNNING,
					STATUS_WAITING
				};
				
				ItemInfo(const HintedUser& u, const bool isDownload) : m_hintedUser(u), download(isDownload), transferFailed(false),
					m_status(STATUS_WAITING), pos(0), size(0), actual(0), m_speed(0), timeLeft(0),
					collapsed(true), parent(nullptr), hits(-1), running(0), m_type(Transfer::TYPE_FILE)
				{
					if (m_hintedUser.user)
					{
						m_niks = WinUtil::getNicks(m_hintedUser);
						m_hubs = WinUtil::getHubNames(m_hintedUser).first;
					}
				}
				
				const bool download; // [!] is const member.
				bool transferFailed;
				bool collapsed;
				
				int16_t running;
				int16_t hits;
				
				ItemInfo* parent;
				HintedUser m_hintedUser; // [!] IRainman fix.
				Status m_status;
				Transfer::Type m_type;
				
				int64_t pos;
				int64_t size;
				int64_t actual;
				int64_t m_speed;
				int64_t timeLeft;
				tstring ip;
				tstring statusString;
				tstring cipher;
				tstring m_target;
				tstring m_niks;
				tstring m_hubs;
				mutable Util::CustomNetworkIndex m_location; // [+] IRainman opt.
				
#ifdef PPA_INCLUDE_COLUMN_RATIO
				tstring m_ratio_as_text; // [+] brain-ripper
#endif
				
				void update(const UpdateInfo& ui);
				
				const UserPtr& getUser() const
				{
					return m_hintedUser.user;
				}
				
				void disconnect();
				void removeAll();
				
				double getProgressPosition() const
				{
					return (pos > 0) ? (double)actual / (double)pos : 1.0;
				}
				const tstring getText(uint8_t col) const;
				static int compareItems(const ItemInfo* a, const ItemInfo* b, uint8_t col);
				
				uint8_t getImageIndex() const
				{
					return static_cast<uint8_t>(download ? (!parent ? IMAGE_DOWNLOAD : IMAGE_SEGMENT) : IMAGE_UPLOAD); // [!] IRainman fix.
				}
				ItemInfo* createParent()
				{
					dcassert(download);
					ItemInfo* ii = new ItemInfo(HintedUser(nullptr, Util::emptyString), true);
					ii->running = 0;
					ii->hits = 0;
					ii->m_target = m_target;
					ii->statusString = TSTRING(CONNECTING);
					return ii;
				}
				const tstring& getGroupCond() const
				{
					return m_target;
				}
		};
		
		struct UpdateInfo : public Task, public CFlyTargetInfo
		{
			enum
			{
				MASK_POS            = 0x01,
				MASK_SIZE           = 0x02,
				MASK_ACTUAL         = 0x04, //-V112
				MASK_SPEED          = 0x08,
				MASK_FILE           = 0x10,
				MASK_STATUS         = 0x20, //-V112
				MASK_TIMELEFT       = 0x40,
				MASK_IP             = 0x80,
				MASK_STATUS_STRING  = 0x100,
				MASK_SEGMENT        = 0x200,
				MASK_CIPHER         = 0x400
			};
			
			bool operator==(const ItemInfo& ii) const
			{
				return download == ii.download && hintedUser.user == ii.m_hintedUser.user; // [!] IRainman fix.
			}
			
			UpdateInfo(const HintedUser& aUser, const bool isDownload, const bool isTransferFailed = false) :
				updateMask(0), hintedUser(aUser), download(isDownload), transferFailed(isTransferFailed), type(Transfer::TYPE_LAST), running(0)
			{
			}
			
			UpdateInfo(const UserPtr& aUser, const bool isDownload, const bool isTransferFailed = false) :
				updateMask(0), download(isDownload), hintedUser(HintedUser(aUser, Util::emptyString)), // fix empty string
				transferFailed(isTransferFailed), type(Transfer::TYPE_LAST), running(0)
			{
			}
			
			UpdateInfo() :
				updateMask(0), download(true), hintedUser(HintedUser(nullptr, Util::emptyString)), transferFailed(false), type(Transfer::TYPE_LAST), running(0)
			{
			}
			
			~UpdateInfo()
			{
			}
			
			uint32_t updateMask;
			
			// [!] IRainman fix.
			HintedUser hintedUser; // [!]
			
			const bool download; // [!] is const member.
			const bool transferFailed; // [!] is const member.
			// [~]
			void setRunning(int16_t aRunning)
			{
				running = aRunning;
				updateMask |= MASK_SEGMENT;
			}
			int16_t running;
			void setStatus(ItemInfo::Status aStatus)
			{
				status = aStatus;
				updateMask |= MASK_STATUS;
			}
			ItemInfo::Status status;
			void setPos(int64_t aPos)
			{
				pos = aPos;
				updateMask |= MASK_POS;
			}
			int64_t pos;
			void setSize(int64_t aSize)
			{
				size = aSize;
				updateMask |= MASK_SIZE;
			}
			int64_t size;
			void setActual(int64_t aActual)
			{
				actual = aActual;
				updateMask |= MASK_ACTUAL;
			}
			int64_t actual;
			void setSpeed(int64_t aSpeed)
			{
				speed = aSpeed;
				updateMask |= MASK_SPEED;
			}
			int64_t speed;
			void setTimeLeft(int64_t aTimeLeft)
			{
				timeLeft = aTimeLeft;
				updateMask |= MASK_TIMELEFT;
			}
			int64_t timeLeft;
			void setStatusString(const tstring& aStatusString)
			{
				statusString = aStatusString;
				updateMask |= MASK_STATUS_STRING;
			}
			tstring statusString;
			void setTarget(const string& aTarget)
			{
				Text::toT(aTarget, m_target); // [+] IRainman opt: This call is equivalent to target = Text::toT(aTarget); but it is a bit faster.
				parseTarget(aTarget);
				updateMask |= MASK_FILE;
			}
			tstring m_target;
			void setCipher(const string& aCipher)
			{
				Text::toT(aCipher, cipher); // [+] IRainman opt: This call is equivalent to cipher = Text::toT(aCipher); but it is a bit faster.
				updateMask |= MASK_CIPHER;
			}
			tstring cipher;
			void setType(const Transfer::Type& aType)
			{
				type = aType;
			}
			Transfer::Type type;
			
			// !SMT!-IP
			void setIP(const string& aIP)
			{
				dcassert(!(!ip.empty() && aIP.empty())); // Ловим случай http://code.google.com/p/flylinkdc/issues/detail?id=1298
				Text::toT(aIP, ip); // [+] IRainman opt: This call is equivalent to ip = Text::toT(aIP); but it is a bit faster.
#ifdef PPA_INCLUDE_DNS
				dns = Text::toT(Socket::nslookup(aIP));
#endif
				updateMask |= MASK_IP;
			}
			tstring ip;
		};
		
		// [+] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1082
		struct QueueItemUpdateInfo : public UpdateInfo
#ifdef _DEBUG
				, virtual NonDerivable<QueueItemUpdateInfo>
#endif
		{
			QueueItemUpdateInfo(const QueueItemPtr& queueItem) : m_queueItem(queueItem)
			{
				m_queueItem->inc();
			}
			~QueueItemUpdateInfo()
			{
				m_queueItem->dec();
			}
			QueueItemPtr m_queueItem;
		};
		
		void parseQueueItemUpdateInfo(QueueItemUpdateInfo* ui);
		// [~] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1082
		
		UpdateInfo* createUpdateInfoForAddedEvent(const ConnectionQueueItem* aCqi); // [+] IRainman fix.
		/*
		void speak(uint8_t type, UpdateInfo* ui) deprecated
		{
		    m_tasks.add(type, ui);
		    // [-] speak(); // [-] IRainman opt.
		}
		*/
		
		ItemInfoList ctrlTransfers;
		static int columnIndexes[];
		static int columnSizes[];
		
		CImageList m_arrows;
		CImageList m_speedImages;
		CImageList m_speedImagesBW;
		
		static HIconWrapper g_user_icon;
		
		//OMenu transferMenu;
		OMenu segmentedMenu;
		OMenu usercmdsMenu;
		OMenu copyMenu; // !SMT!-UI
		
		TaskQueue m_tasks;
		
		StringMap ucLineParams;
		
		void on(ConnectionManagerListener::Added, const ConnectionQueueItem* aCqi) noexcept;
		void on(ConnectionManagerListener::Failed, const ConnectionQueueItem* aCqi, const string& aReason) noexcept;
		void on(ConnectionManagerListener::Removed, const ConnectionQueueItem* aCqi) noexcept;
		void on(ConnectionManagerListener::StatusChanged, const ConnectionQueueItem* aCqi) noexcept;
		
		void on(DownloadManagerListener::Requesting, const Download* aDownload) noexcept;
		void on(DownloadManagerListener::Complete, const Download* aDownload, bool isTree) noexcept
		{
			onTransferComplete(aDownload, true, Util::getFileName(aDownload->getPath()), isTree); // [!] IRainman fix.
		}
		void on(DownloadManagerListener::Failed, const Download* aDownload, const string& aReason) noexcept;
		void on(DownloadManagerListener::Starting, const Download* aDownload) noexcept;
		void on(DownloadManagerListener::Tick, const DownloadMap& aDownload, uint64_t CurrentTick) noexcept;//[!]IRainman refactoring transfer mechanism + uint64_t CurrentTick
		void on(DownloadManagerListener::Status, const UserConnection*, const string&) noexcept;
		
		void on(UploadManagerListener::Starting, const Upload* aUpload) noexcept;
		void on(UploadManagerListener::Tick, const UploadList& aUpload, uint64_t CurrentTick) noexcept;//[!]IRainman refactoring transfer mechanism + uint64_t CurrentTick
		void on(UploadManagerListener::Complete, const Upload* aUpload) noexcept
		{
			onTransferComplete(aUpload, false, aUpload->getPath(), false); // [!] IRainman fix.
		}
		void on(QueueManagerListener::StatusUpdated, const QueueItemPtr&) noexcept;
		void on(QueueManagerListener::StatusUpdatedList, const QueueItemList& p_list) noexcept // [+] IRainman opt.
		{
			for (auto i = p_list.cbegin(); i != p_list.cend(); ++i)
			{
				on(QueueManagerListener::StatusUpdated(), *i);
			}
		}
		void on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept; // [+] IRainman opt.
		void on(QueueManagerListener::Removed, const QueueItemPtr&) noexcept;
		void on(QueueManagerListener::Finished, const QueueItemPtr&, const string&, const Download*) noexcept;
		
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept;
		
		void onTransferComplete(const Transfer* aTransfer, const bool download, const string& aFileName, const bool isTree); // [!] IRainman fix.
		void starting(UpdateInfo* ui, const Transfer* t);
		
		void CollapseAll();
		void ExpandAll();
		
		ItemInfo* findItem(const UpdateInfo& ui, int& pos) const;
		void updateItem(int ii, uint32_t updateMask);
		
		// [+]Drakon
		void PauseSelectedTransfer(void);
		// [+] NightOrion
		bool getTTH(const ItemInfo* p_ii, TTHValue& p_tth);
};

#endif // !defined(TRANSFER_VIEW_H)

/**
 * @file
 * $Id: TransferView.h 568 2011-07-24 18:28:43Z bigmuscle $
 */