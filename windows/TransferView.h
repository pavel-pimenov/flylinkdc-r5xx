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
#define SCALOLAZ_USE_TRANSFER_CONTROL
#pragma once

#include "../client/DownloadManagerListener.h"
#include "../client/UploadManagerListener.h"
#include "../client/ConnectionManagerListener.h"
#include "../client/QueueManagerListener.h"
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
	virtual private CFlyTimerAdapter,
	virtual private CFlyTaskAdapter
{
	public:
		DECLARE_WND_CLASS(_T("TransferView"))
		
		TransferView();
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
		MESSAGE_HANDLER(WM_TIMER, onTimerTask)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_NOTIFYFORMAT, onNotifyFormat)
#ifdef IRAINMAN_ENABLE_WHOIS
		COMMAND_ID_HANDLER(IDC_WHOIS_IP, onWhoisIP)
		COMMAND_ID_HANDLER(IDC_WHOIS_IP2, onWhoisIP)
#endif
		COMMAND_ID_HANDLER(IDC_FORCE, onForce)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchAlternates)
		COMMAND_ID_HANDLER(IDC_ADD_P2P_GUARD, onAddP2PGuard)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVEALL, onRemoveAll)
		COMMAND_ID_HANDLER(IDC_DISCONNECT_ALL, onDisconnectAll)
		COMMAND_ID_HANDLER(IDC_COLLAPSE_ALL, onCollapseAll)
		COMMAND_ID_HANDLER(IDC_EXPAND_ALL, onExpandAll)
		COMMAND_ID_HANDLER(IDC_MENU_SLOWDISCONNECT, onSlowDisconnect)
		COMMAND_ID_HANDLER(IDC_FORCE_PASSIVE_MODE, onForcePassiveMode)
		COMMAND_ID_HANDLER(IDC_AUTO_PASSIVE_MODE, onForceAutoPassiveMode)
		COMMAND_ID_HANDLER(IDC_AVDB_BLOCK_CONNECTIONS, onAVDBBlockConnections)
		MESSAGE_HANDLER_HWND(WM_INITMENUPOPUP, OMenu::onInitMenuPopup)
		MESSAGE_HANDLER_HWND(WM_MEASUREITEM, OMenu::onMeasureItem)
		MESSAGE_HANDLER_HWND(WM_DRAWITEM, OMenu::onDrawItem)
		COMMAND_ID_HANDLER(IDC_PRIORITY_PAUSED, onPauseSelectedItem)
#ifdef SCALOLAZ_USE_TRANSFER_CONTROL
		COMMAND_ID_HANDLER(IDC_UPLOAD_QUEUE, onOpenWindows)
		COMMAND_ID_HANDLER(IDC_QUEUE, onOpenWindows)
#endif
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
		LRESULT onForcePassiveMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onForceAutoPassiveMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAVDBBlockConnections(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef SCALOLAZ_USE_TRANSFER_CONTROL
		LRESULT onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
#ifdef IRAINMAN_ENABLE_WHOIS
		LRESULT onWhoisIP(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		void runUserCommand(UserCommand& uc);
		void prepareClose();
		void doTimerTask() override;
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
		
		LRESULT onAddP2PGuard(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			ctrlTransfers.forEachSelected(&ItemInfo::disconnectAndP2PGuard);
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
			if (checkState == BST_CHECKED || ::MessageBox(NULL, CTSTRING(REALLY_REMOVE), T_APPNAME_WITH_VERSION, CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES) // [~] InfinitySky.
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
#ifdef FLYLINKDC_USE_TREEE_LIST_VIEW_WITHOUT_POINTER
		typedef TypedTreeListViewCtrlSafe<ItemInfo, IDC_TRANSFERS, tstring> ItemInfoList;
#else
		typedef TypedTreeListViewCtrl<ItemInfo, IDC_TRANSFERS, tstring, noCaseStringHash, noCaseStringEq> ItemInfoList;
#endif
		ItemInfoList& getUserList()
		{
			return ctrlTransfers;
		}
#ifdef IRAINMAN_ENABLE_WHOIS
		static tstring g_sSelectedIP;
#endif
	private:
		enum
		{
			TRANSFER_ADD_ITEM,
			TRANSFER_REMOVE_ITEM,
			TRANSFER_UPDATE_ITEM,
			TRANSFER_UPDATE_PARENT,
			TRANSFER_UPDATE_PARENT_WITH_PARSE, // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=1082
			TRANSFER_REMOVE_DOWNLOAD_ITEM
		};
		
		enum
		{
			COLUMN_FIRST,
			COLUMN_USER = COLUMN_FIRST,
			COLUMN_ANTIVIRUS,
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
			COLUMN_P2P_GUARD,
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
				bool m_is_file_list;
			protected:
				CFlyTargetInfo() : m_is_file_list(false)
				{
				}
			public:
				bool isFileList() const
				{
					return m_is_file_list;
				}
				void parseTarget(const string& p_target)
				{
					m_is_file_list = p_target.find(Util::getListPath()) != string::npos ||
					                 p_target.find(Util::getConfigPath()) != string::npos;
				}
		};
		struct UpdateInfo;
		class ItemInfo : public UserInfoBase, public CFlyTargetInfo
		{
			public:
				enum Status
				{
					STATUS_RUNNING,
					STATUS_WAITING
				};
				
				ItemInfo(const HintedUser& u, const bool isDownload) : m_hintedUser(u), download(isDownload), transferFailed(false),
					m_status(STATUS_WAITING), m_pos(0), m_size(0), m_actual(0), m_speed(0), m_timeLeft(0),
					collapsed(true), parent(nullptr), m_hits(-1), running(0), m_type(Transfer::TYPE_FILE), m_is_force_passive(false)
				{
					update_nicks();
				}
				
				const bool download; // [!] is const member.
				bool transferFailed;
				bool collapsed;
				
				int16_t running;
				int16_t m_hits;
				
				ItemInfo* parent;
				HintedUser m_hintedUser; // [!] IRainman fix.
				tstring m_antivirus_text;
				tstring m_p2p_guard_text;
				Status m_status;
				bool m_is_force_passive;
				Transfer::Type m_type;
				
				int64_t m_pos;
				int64_t m_size;
				int64_t m_actual;
				int64_t m_speed;
				int64_t m_timeLeft;
				tstring m_transfer_ip;
				tstring m_statusString;
				tstring m_cipher;
				tstring m_target;
				tstring m_nicks;
				tstring m_hubs;
				mutable Util::CustomNetworkIndex m_location; // [+] IRainman opt.
				
#ifdef PPA_INCLUDE_COLUMN_RATIO
				tstring m_ratio_as_text; // [+] brain-ripper
#endif
				
				void update(const UpdateInfo& ui);
				void update_nicks();
				const UserPtr& getUser() const
				{
					return m_hintedUser.user;
				}
				
				void disconnect();
				void disconnectAndP2PGuard();
				void removeAll();
				
				double getProgressPosition() const
				{
					return (m_pos > 0) ? (double)m_actual / (double)m_pos : 1.0;
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
					ii->m_hits = 0;
					ii->m_target = m_target;
					ii->m_statusString = TSTRING(CONNECTING);
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
				MASK_CIPHER         = 0x400,
				MASK_USER           = 0x800,
				MASK_FORCE_PASSIVE  = 0x1000
			};
			
			bool operator==(const ItemInfo& ii) const
			{
				return download == ii.download && m_hintedUser.user == ii.m_hintedUser.user; // [!] IRainman fix.
			}
#if 0
			UpdateInfo(const HintedUser& aUser, const bool isDownload, const bool isTransferFailed = false) :
				updateMask(0), m_hintedUser(aUser), download(isDownload), transferFailed(isTransferFailed), type(Transfer::TYPE_LAST), running(0)
			{
			}
#endif
			UpdateInfo(const HintedUser& aHintedUser, const bool isDownload, const bool isTransferFailed = false) :
				updateMask(0), download(isDownload), m_hintedUser(aHintedUser), // fix empty string
				transferFailed(isTransferFailed), type(Transfer::TYPE_LAST), running(0), m_is_force_passive(false),
				status(ItemInfo::STATUS_WAITING), pos(0), size(0), actual(0), speed(0), timeLeft(0)
			{
			}
			UpdateInfo() :
				updateMask(0), download(true), transferFailed(false), type(Transfer::TYPE_LAST), running(0), m_is_force_passive(false),
				status(ItemInfo::STATUS_WAITING), pos(0), size(0), actual(0), speed(0), timeLeft(0)
			{
			}
			~UpdateInfo()
			{
			}
			
			uint32_t updateMask;
			
			// [!] IRainman fix.
			HintedUser m_hintedUser; // [!]
			void setHintedUser(const HintedUser& aHitedUser)
			{
				m_hintedUser = aHitedUser;
				updateMask |= MASK_USER;
			}
			const bool download; // [!] is const member.
			const bool transferFailed; // [!] is const member.
			// [~]
			void setForcePassive(bool p_is_force_passive)
			{
				if (m_is_force_passive != p_is_force_passive)
				{
					m_is_force_passive = p_is_force_passive;
					updateMask |= MASK_FORCE_PASSIVE;
				}
			}
			bool m_is_force_passive;
			
			void setRunning(int16_t aRunning)
			{
				if (running != aRunning)
				{
					running = aRunning;
					updateMask |= MASK_SEGMENT;
				}
			}
			int16_t running;
			void setStatus(ItemInfo::Status aStatus)
			{
				if (status != aStatus)
				{
					status = aStatus;
					updateMask |= MASK_STATUS;
				}
			}
			ItemInfo::Status status;
			void setPos(int64_t aPos)
			{
				if (pos != aPos)
				{
					pos = aPos;
					updateMask |= MASK_POS;
				}
			}
			int64_t pos;
			void setSize(int64_t aSize)
			{
				if (size != aSize)
				{
					size = aSize;
					updateMask |= MASK_SIZE;
				}
			}
			int64_t size;
			void setActual(int64_t aActual)
			{
				if (actual != aActual)
				{
					actual = aActual;
					updateMask |= MASK_ACTUAL;
				}
			}
			int64_t actual;
			void setSpeed(int64_t aSpeed)
			{
				if (speed != aSpeed)
				{
					speed = aSpeed;
					updateMask |= MASK_SPEED;
				}
			}
			int64_t speed;
			void setTimeLeft(int64_t aTimeLeft)
			{
				if (timeLeft != aTimeLeft)
				{
					timeLeft = aTimeLeft;
					updateMask |= MASK_TIMELEFT;
				}
			}
			int64_t timeLeft;
			void setStatusString(const tstring& aStatusString)
			{
				if (statusString != aStatusString)
				{
					statusString = aStatusString;
					updateMask |= MASK_STATUS_STRING;
				}
			}
			tstring statusString;
			void setTarget(const string& aTarget)
			{
				const auto l_new_value = Text::toT(aTarget);
				if (l_new_value != m_target)
				{
					m_target = l_new_value;
					parseTarget(aTarget);
					updateMask |= MASK_FILE;
				}
			}
			tstring m_target;
			void setCipher(const string& aCipher)
			{
				if (aCipher.empty() && m_cipher.empty())
					return;
				const auto l_new_value = Text::toT(aCipher);
				if (l_new_value != m_cipher)
				{
					m_cipher = l_new_value;
					updateMask |= MASK_CIPHER;
				}
			}
			tstring m_cipher;
			void setType(const Transfer::Type& aType)
			{
				type = aType;
			}
			Transfer::Type type;
			
			// !SMT!-IP
			void setIP(const string& aIP)
			{
				const auto l_new_value = Text::toT(aIP);
				if (l_new_value != m_ip)
				{
					dcassert(!(!m_ip.empty() && aIP.empty())); // Ловим случай http://code.google.com/p/flylinkdc/issues/detail?id=1298
					m_ip = l_new_value;
#ifdef PPA_INCLUDE_DNS
					dns = Text::toT(Socket::nslookup(aIP));
#endif
					updateMask |= MASK_IP;
				}
			}
			tstring m_ip; // TODO - зачем тут tstring?
		};
		void onSpeakerAddItem(const UpdateInfo& ui);
		void parseQueueItemUpdateInfoL(UpdateInfo* p_ui, const QueueItemPtr& p_queueItem);
		// [~] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1082
		
		UpdateInfo* createUpdateInfoForAddedEvent(const ConnectionQueueItem* aCqi); // [+] IRainman fix.
		
		ItemInfoList ctrlTransfers;
		CButton m_PassiveModeButton;
		CButton m_AutoPassiveModeButton;
		CButton m_AVDB_BlockButton;
	public:
		void UpdateLayout();
		void setButtonState();
	private:
	
		CFlyToolTipCtrl m_force_passive_tooltip;
		CFlyToolTipCtrl m_active_passive_tooltip;
		CFlyToolTipCtrl m_avdb_block_tooltip;
		
		static int columnIndexes[];
		static int columnSizes[];
		
		CImageList m_arrows;
		CImageList m_speedImages;
		CImageList m_speedImagesBW;
		
		static HIconWrapper g_user_icon;
		//static HIconWrapper g_fiwrewall_icon;
		
		//OMenu transferMenu;
		OMenu segmentedMenu;
		OMenu usercmdsMenu;
		OMenu copyMenu; // !SMT!-UI
		
		StringMap ucLineParams;
		bool m_is_need_resort;
		
		void on(ConnectionManagerListener::Added, const ConnectionQueueItem* aCqi) noexcept override;
		void on(ConnectionManagerListener::Failed, const ConnectionQueueItem* aCqi, const string& aReason) noexcept override;
		void on(ConnectionManagerListener::Removed, const ConnectionQueueItem* aCqi) noexcept override;
		void on(ConnectionManagerListener::UserUpdated, const ConnectionQueueItem* aCqi) noexcept override;
		void on(ConnectionManagerListener::ConnectionStatusChanged, const ConnectionQueueItem* aCqi) noexcept override;
		
		void on(DownloadManagerListener::Requesting, const DownloadPtr& aDownload) noexcept override;
		void on(DownloadManagerListener::Complete, const DownloadPtr& aDownload) noexcept override
		{
			onTransferComplete(aDownload.get(), true, Util::getFileName(aDownload->getPath())); // [!] IRainman fix.
		}
		void on(DownloadManagerListener::Failed, const DownloadPtr& aDownload, const string& aReason) noexcept override;
		void on(DownloadManagerListener::Starting, const DownloadPtr& aDownload) noexcept override;
		void on(DownloadManagerListener::Tick, const DownloadArray& aDownload) noexcept override;
		void on(DownloadManagerListener::Status, const UserConnection*, const string&) noexcept override;
		
		void on(UploadManagerListener::Starting, const UploadPtr& aUpload) noexcept override;
		void on(UploadManagerListener::Tick, const UploadArray& aUpload) noexcept override;
		void on(UploadManagerListener::Complete, const UploadPtr& aUpload) noexcept override
		{
			onTransferComplete(aUpload.get(), false, aUpload->getPath()); // [!] IRainman fix.
		}
		void on(QueueManagerListener::StatusUpdated, const QueueItemPtr&) noexcept override;
		void on(QueueManagerListener::StatusUpdatedList, const QueueItemList& p_list) noexcept override; // [+] IRainman opt.
		void on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept override; // [+] IRainman opt.
		void on(QueueManagerListener::Removed, const QueueItemPtr&) noexcept override;
		void on(QueueManagerListener::Finished, const QueueItemPtr&, const string&, const DownloadPtr& aDownload) noexcept override;
		
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/) override;
		
		void onTransferComplete(const Transfer* aTransfer, const bool download, const string& aFileName); // [!] IRainman fix.
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