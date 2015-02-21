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

#if !defined(HUB_FRAME_H)
#define HUB_FRAME_H

#pragma once

#include "../client/DCPlusPlus.h" // [!] IRainman fix whois entery on menu 
#include "../client/User.h"
#include "../client/ClientManager.h"
#include "../client/DirectoryListing.h"
#include "../client/TaskQueue.h"
#include "../client/SimpleXML.h"
#include "../client/ConnectionManager.h"

#include "BaseChatFrame.h"
#include "UCHandler.h"

#define EDIT_MESSAGE_MAP 10     // This could be any number, really...
#define FILTER_MESSAGE_MAP 8
#define HUBSTATUS_MESSAGE_MAP 5 // Status frame
struct CompareItems;

class HubFrame : public MDITabChildWindowImpl < HubFrame, RGB(255, 0, 0), IDR_HUB, IDR_HUB_OFF > , private ClientListener,
	public  CSplitterImpl<HubFrame>,
	private CFlyTimerAdapter,
//  TODO    private ClientManagerListener,
public UCHandler<HubFrame>,
public UserInfoBaseHandler < HubFrame, UserInfoGuiTraits::NO_CONNECT_FAV_HUB | UserInfoGuiTraits::NICK_TO_CHAT | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::INLINE_CONTACT_LIST, OnlineUserPtr > ,
private SettingsManagerListener,
public BaseChatFrame // [+] IRainman copy-past fix.
#ifdef RIP_USE_CONNECTION_AUTODETECT
	, private ConnectionManagerListener // [+] FlylinkDC
#endif
#ifdef _DEBUG
	, virtual NonDerivable<HubFrame> // [+] IRainman fix.
#endif
{
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("HubFrame"), IDR_HUB, 0, COLOR_3DFACE);
		
		typedef CSplitterImpl<HubFrame> splitBase;
		typedef MDITabChildWindowImpl < HubFrame, RGB(255, 0, 0), IDR_HUB, IDR_HUB_OFF > baseClass;
		typedef UCHandler<HubFrame> ucBase;
		typedef UserInfoBaseHandler < HubFrame, UserInfoGuiTraits::NO_CONNECT_FAV_HUB | UserInfoGuiTraits::NICK_TO_CHAT | UserInfoGuiTraits::USER_LOG | UserInfoGuiTraits::INLINE_CONTACT_LIST, OnlineUserPtr > uibBase;
		
		BEGIN_MSG_MAP(HubFrame)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		//MESSAGE_HANDLER(WM_SPEAKER_FIRST_USER_JOIN, OnSpeakerFirstUserJoin)
		MESSAGE_RANGE_HANDLER(WM_SPEAKER_BEGIN, WM_SPEAKER_END, OnSpeakerRange)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETDISPINFO, m_ctrlUsers->onGetDispInfo)
		NOTIFY_HANDLER(IDC_USERS, LVN_COLUMNCLICK, m_ctrlUsers->onColumnClick)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETINFOTIP, m_ctrlUsers->onInfoTip)
		NOTIFY_HANDLER(IDC_USERS, LVN_KEYDOWN, onKeyDownUsers)
		NOTIFY_HANDLER(IDC_USERS, NM_DBLCLK, onDoubleClickUsers)
		NOTIFY_HANDLER(IDC_USERS, NM_RETURN, onEnterUsers)
		//NOTIFY_HANDLER(IDC_USERS, LVN_ITEMCHANGED, onItemChanged) [-] IRainman opt
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu) // 2012-04-29_13-46-19_6YBC2BUJRYPCJLE2L63SZAFLWGMNBOSFJB64BTI_5FE1A0BF_crash-stack-r501-x64-build-9869.dmp
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onHubFrmCtlColor)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(WM_MOUSEMOVE, onStyleChange)
		MESSAGE_HANDLER(WM_CAPTURECHANGED, onStyleChanged)
		MESSAGE_HANDLER(WM_WINDOWPOSCHANGING, onSizeMove)
		CHAIN_MSG_MAP(BaseChatFrame)
		COMMAND_ID_HANDLER(ID_FILE_RECONNECT, onFileReconnect)
		COMMAND_ID_HANDLER(ID_DISCONNECT, onDisconnect)
		COMMAND_ID_HANDLER(IDC_REFRESH, onRefresh)
		COMMAND_ID_HANDLER(IDC_FOLLOW, onFollow)
		COMMAND_ID_HANDLER(IDC_SEND_MESSAGE, onSendMessage)
		COMMAND_ID_HANDLER(IDC_ADD_AS_FAVORITE, onAddAsFavorite)
		COMMAND_ID_HANDLER(IDC_REM_AS_FAVORITE, onRemAsFavorite)
		COMMAND_ID_HANDLER(IDC_AUTO_START_FAVORITE, onAutoStartFavorite)
		COMMAND_ID_HANDLER(IDC_EDIT_HUB_PROP, onEditHubProp)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindows)            // [~] SCALOlaz
		COMMAND_ID_HANDLER(IDC_RECONNECT_DISCONNECTED, onCloseWindows)  // [+] SCALOlaz
		COMMAND_ID_HANDLER(IDC_CLOSE_DISCONNECTED, onCloseWindows)      // [+] SCALOlaz
		COMMAND_ID_HANDLER(IDC_SELECT_USER, onSelectUser)
		COMMAND_ID_HANDLER(IDC_AUTOSCROLL_CHAT, onAutoScrollChat)
		COMMAND_ID_HANDLER(IDC_BAN_IP, onBanIP)
		COMMAND_ID_HANDLER(IDC_UNBAN_IP, onUnBanIP)
		COMMAND_ID_HANDLER(IDC_OPEN_HUB_LOG, onOpenHubLog)
		COMMAND_ID_HANDLER(IDC_OPEN_USER_LOG, onOpenUserLog)
		NOTIFY_HANDLER(IDC_USERS, NM_CUSTOMDRAW, onCustomDraw)
		COMMAND_ID_HANDLER(IDC_COPY_HUBNAME, onCopyHubInfo)
		COMMAND_ID_HANDLER(IDC_COPY_HUBADDRESS, onCopyHubInfo)
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uibBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(splitBase)
		ALT_MSG_MAP(EDIT_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		MESSAGE_HANDLER(WM_KEYDOWN, onChar)
		MESSAGE_HANDLER(WM_KEYUP, onChar)
		MESSAGE_HANDLER(BM_SETCHECK, onShowUsers)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, onLButton)
		//MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		ALT_MSG_MAP(FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_CHAR, onFilterChar)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onSelChange)
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		ALT_MSG_MAP(HUBSTATUS_MESSAGE_MAP)
		//  COMMAND_ID_HANDLER(IDC_HUBS_SWITCHPANELS, onSwitchPanels)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onSwitchPanels)
#endif
		END_MSG_MAP()
		
		LRESULT onHubFrmCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT OnSpeakerRange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
//		LRESULT OnSpeakerFirstUserJoin(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);

		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onCopyUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyHubInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onDoubleClickUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onShowUsers(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onFollow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onEnterUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onFilterChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onFileReconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClientRButtonDown(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSelectUser(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAutoScrollChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onUnBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBanned(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onStyleChange(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onStyleChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSizeMove(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		void OnSwitchedPanels();
		LRESULT onSwitchPanels(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			OnSwitchedPanels();
			return 0;
		}
#endif
		void onBeforeActiveTab(HWND aWnd); // http://code.google.com/p/flylinkdc/issues/detail?id=1242
		void onAfterActiveTab(HWND aWnd);
		void onInvalidateAfterActiveTab(HWND aWnd);
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void addLine(const tstring& aLine, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void addLine(const Identity& ou, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void addStatus(const tstring& aLine, const bool bInChat = true, const bool bHistory = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem);
		void onTab();
		void handleTab(bool reverse);
		void runUserCommand(::UserCommand& uc);
		
		static HubFrame* openWindow(const string& p_server,
		                            const string& p_name     = Util::emptyString,
		                            const string& p_rawOne   = Util::emptyString,
		                            const string& p_rawTwo   = Util::emptyString,
		                            const string& p_rawThree = Util::emptyString,
		                            const string& p_rawFour  = Util::emptyString,
		                            const string& p_rawFive  = Util::emptyString,
		                            int  p_windowposx = 0,
		                            int  p_windowposy = 0,
		                            int  p_windowsizex = 0,
		                            int  p_windowsizey = 0,
		                            int  p_windowtype = 0,
		                            int  p_ChatUserSplit = 0,
		                            bool p_UserListState = true
		                                                   // bool p_ChatUserSplitState = true,
		                                                   // const string& p_ColumsOrder = Util::emptyString,
		                                                   // const string& p_ColumsWidth = Util::emptyString,
		                                                   // const string& p_ColumsVisible = Util::emptyString
		                           );
		static void resortUsers();
		static void closeDisconnected();
		static void reconnectDisconnected();
		static void closeAll(size_t thershold = 0);
		
		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (m_ctrlMessage)
				m_ctrlMessage->SetFocus();
			return 0;
		}
		
		LRESULT onAddAsFavorite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			addAsFavorite();
			return 0;
		}
		
		LRESULT onRemAsFavorite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			removeFavoriteHub();
			return 0;
		}
		LRESULT onAutoStartFavorite(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			autoConnectStart();
			return 0;
		}
		LRESULT onEditHubProp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCloseWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (m_client->isConnected())
			{
				clearUserList(false);
				m_client->refreshUserList(false);
			}
			return 0;
		}
		// [-] IRainman opt
		//LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
		/*{
		    updateStatusBar();
		    return 0;
		}
		*/
		LRESULT onKeyDownUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			const NMLVKEYDOWN* l = (NMLVKEYDOWN*)pnmh;
			if (l->wVKey == VK_TAB)
			{
				onTab();
			}
			return 0;
		}
		
		typedef TypedListViewCtrl<UserInfo, IDC_USERS> CtrlUsers;
		CtrlUsers& getUserList()
		{
			return *m_ctrlUsers;
		}
		
		
	private:
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
		
		HubFrame(const string& aServer,
		         const string& aName,
		         const string& aRawOne,
		         const string& aRawTwo,
		         const string& aRawThree,
		         const string& aRawFour,
		         const string& aRawFive,
		         int  p_ChatUserSplit,
		         bool p_UserListState
		         //bool p_ChatUserSplitState
		        );
		~HubFrame();
		
		virtual void doDestroyFrame();
		typedef boost::unordered_map<string, HubFrame*> FrameMap;
		static FrameMap g_frames;
		
		tstring m_shortHubName;
		uint8_t m_hub_name_update_count;
		bool m_is_hub_name_updated;
		bool m_is_first_goto_end;
		void onTimerHubUpdated();
		uint8_t m_second_count;
		void setShortHubName(const tstring& p_name);
		string m_redirect;
		tstring m_complete;
		bool m_waitingForPW;
		uint8_t m_password_do_modal;
		HTHEME m_Theme;
		
		Client* m_client;
		string m_server;
		
		CContainedWindow ctrlClientContainer;
		
		CtrlUsers* m_ctrlUsers;
		void createCtrlUsers();
		
		tstring m_lastUserName; // SSA_SAVE_LAST_NICK_MACROS
		
		bool m_showUsers;
		bool m_showUsersStore;
		void setShowUsers(bool m_value)
		{
			m_showUsers = m_value;
			m_showUsersStore = m_value;
		}
		void firstLoadAllUsers();
		void usermap2ListrView();
		
#if 0
		class CFlyUserMap : public Thread
		{
			public:
				CFlyUserMap() : m_hub_frame(nullptr), m_is_first_run(false)
				{
				}
				~CFlyUserMap()
				{
				}
				int run()
				{
					m_hub_frame->usermap2ListrView();
					return 0;
				}
				void process_init_user_list(HubFrame* p_hub_frame)
				{
					if (is_active())
					{
						return;
					}
					else
					{
						m_is_first_run = true;
						m_hub_frame = p_hub_frame;
						start(64);
					}
				}
			private:
				HubFrame* m_hub_frame;
				bool m_is_first_run;
		} m_userMapInitThread;
		
		//std::unique_ptr<webrtc::RWLockWrapper> m_userMapCS;
		CriticalSection m_userMapCS;
#endif
		
		UserInfo::OnlineUserMap m_userMap;
		TaskQueue m_tasks;
		bool m_needsUpdateStats;
		bool m_needsResort;
		
		static int g_columnIndexes[COLUMN_LAST];
		static int g_columnSizes[COLUMN_LAST];
		
		bool updateUser(const OnlineUserPtr& p_ou, const int p_index_column);
		void removeUser(const OnlineUserPtr& p_ou);
		
		void InsertUserList(UserInfo* ui);
		void updateUserList(); // [!] IRainman opt.
		bool parseFilter(FilterModes& mode, int64_t& size);
		bool matchFilter(UserInfo& ui, int sel, bool doSizeCompare = false, FilterModes mode = NONE, int64_t size = 0);
		UserInfo* findUser(const tstring& p_nick);   // !SMT!-S
		UserInfo* findUser(const OnlineUserPtr& p_user)
		{
			dcassert(!m_is_fynally_clear_user_list);
			//if(m_is_fynally_clear_user_list)
			//{
			//  LogManager::message("findUser after m_is_fynally_clear_user_list = " + p_user->getUser()->getLastNick());
			//}
			return m_userMap.findUser(p_user);
		}
		//const tstring& getNick(const UserPtr& aUser);
		
		FavoriteHubEntry* addAsFavorite(const FavoriteManager::AutoStartType p_autoconnect = FavoriteManager::NOT_CHANGE);// [!] IRainman fav options
		void removeFavoriteHub();
		
		void createFavHubMenu(const FavoriteHubEntry* p_fhe);
		
		void autoConnectStart();
		
		void clearUserList(bool p_is_fynally_clear_user_list);
		void clearTaskList();
		
		void appendHubAndUsersItems(OMenu& p_menu, const bool isChat);
		
		// FavoriteManagerListener
		void on(FavoriteManagerListener::UserAdded, const FavoriteUser& /*aUser*/) noexcept;
		void on(FavoriteManagerListener::UserRemoved, const FavoriteUser& /*aUser*/) noexcept;
		void resortForFavsFirst(bool justDoIt = false);
		
		// SettingsManagerListener
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/);
		
		// ClientListener
		void on(ClientListener::Connecting, const Client*) noexcept;
		void on(ClientListener::Connected, const Client*) noexcept;
		void on(ClientListener::UserUpdated, const OnlineUserPtr&) noexcept; // !SMT!-fix
		void on(ClientListener::UsersUpdated, const Client*, const OnlineUserList&) noexcept;
		void on(ClientListener::UserRemoved, const Client*, const OnlineUserPtr&) noexcept;
		void on(ClientListener::Redirect, const Client*, const string&) noexcept;
		void on(ClientListener::Failed, const Client*, const string&) noexcept;
		void on(ClientListener::GetPassword, const Client*) noexcept;
		void on(ClientListener::HubUpdated, const Client*) noexcept;
		void on(ClientListener::Message, const Client*, std::unique_ptr<ChatMessage>&) noexcept;
		//void on(PrivateMessage, const Client*, const string &strFromUserName, const UserPtr&, const UserPtr&, const UserPtr&, const string&, bool = true) noexcept; // !SMT!-S [-] IRainman fix.
		void on(NickTaken, const Client*) noexcept;
		void on(ClientListener::CheatMessage, const string&) noexcept;
		void on(ClientListener::UserReport, const Client*, const string&) noexcept; // [+] IRainman
		void on(ClientListener::HubTopic, const Client*, const string&) noexcept;
		void on(ClientListener::StatusMessage, const Client*, const string& line, int statusFlags);
#ifdef RIP_USE_CONNECTION_AUTODETECT
		void on(ConnectionManagerListener::DirectModeDetected, const string&) noexcept;
#endif
		void on(ClientListener::DDoSSearchDetect, const string&) noexcept;
		
		struct StatusTask : public Task
		{
			explicit StatusTask(const string& p_msg, bool p_isInChat) : m_str(p_msg), m_isInChat(p_isInChat) { }
			const string m_str;
			const bool m_isInChat;
		};
		void speak(Tasks s)
		{
			m_tasks.add(static_cast<uint8_t>(s), nullptr);
		}
#ifndef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
		void speak(Tasks s, const OnlineUserPtr& u)
		{
			m_tasks.add(static_cast<uint8_t>(s), new OnlineUserTask(u));
		}
#endif
		// [~] !SMT!-S
		
		void speak(Tasks s, const string& msg, bool inChat = true)
		{
			m_tasks.add(static_cast<uint8_t>(s), new StatusTask(msg, inChat));
		}
#ifndef FLYLINKDC_PRIVATE_MESSAGE_USE_WIN_MESSAGES_Q
		void speak(Tasks s, ChatMessage* p_message_ptr)
		{
			m_tasks.add(static_cast<uint8_t>(s), new MessageTask(p_message_ptr));
			force_speak();
		}
#endif
		
	public:
		static void addDupeUsersToSummaryMenu(const int64_t &share, const string& ip); // !SMT!-UI
		
		// [+] IRainman: copy-past fix.
		void sendMessage(const tstring& msg, bool thirdperson = false)
		{
			m_client->hubMessage(Text::fromT(msg), thirdperson);
		}
		void processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText);
		void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText);
		
		void addMesageLogParams(StringMap& params, tstring aLine, bool bThirdPerson, tstring extra);
		void addFrameLogParams(StringMap& params);
		
		StringMap getFrameLogParams() const;
		void readFrameLog();
		void openFrameLog() const
		{
			WinUtil::openLog(SETTING(LOG_FILE_MAIN_CHAT), getFrameLogParams(), TSTRING(NO_LOG_FOR_HUB));
		}
		// [~] IRainman: copy-past fix.
	private:
		// GDI создаются динамически
		// http://code.google.com/p/flylinkdc/issues/detail?id=1242
		CEdit* m_ctrlFilter;
		CComboBox* m_ctrlFilterSel;
		int m_FilterSelPos;
		int getFilterSelPos() const
		{
			return m_ctrlFilterSel ? m_ctrlFilterSel->GetCurSel() : m_FilterSelPos;
		}
		tstring m_filter;
		string m_window_text;
		uint8_t m_is_window_text_update;
		void setWindowTitle(const string& p_text);
		void updateWindowText();
		CContainedWindow* m_ctrlFilterContainer;
		CContainedWindow* m_ctrlChatContainer;
		CContainedWindow* m_ctrlFilterSelContainer;
		bool m_is_fynally_clear_user_list;
		bool m_showJoins;
		bool m_favShowJoins;
		bool m_isUpdateColumnsInfoProcessed;
		size_t m_ActivateCounter;
		
		void updateSplitterPosition(const FavoriteHubEntry *p_fhe);
		void updateColumnsInfo(const FavoriteHubEntry *p_fhe);
		void storeColumsInfo();
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		bool m_isClientUsersSwitch;
		CButton* m_ctrlSwitchPanels;
		CContainedWindow* m_switchPanelsContainer;
		static HIconWrapper g_hSwitchPanelsIco;
#endif
		CFlyToolTipCtrl*  m_tooltip_hubframe;
		CButton* m_ctrlShowUsers;
		void setShowUsersCheck()
		{
			m_ctrlShowUsers->SetCheck((m_ActivateCounter == 1 ? m_showUsersStore : m_showUsers) ? BST_CHECKED : BST_UNCHECKED);
		}
		CContainedWindow* m_showUsersContainer;
		
		OMenu* m_tabMenu;
		OMenu* m_userMenu;
		bool   m_tabMenuShown;
		
#ifdef SCALOLAZ_HUB_MODE
		CStatic* m_ctrlShowMode;
		static HIconWrapper g_hModeActiveIco;
		static HIconWrapper g_hModePassiveIco;
		static HIconWrapper g_hModeNoneIco;
		void HubModeChange();
#endif
		void TuneSplitterPanes();
		void addPasswordCommand();
		OMenu* createTabMenu();
		OMenu* createUserMenu();
		void destroyOMenu();
	public:
		void createMessagePanel();
		void destroyMessagePanel(bool p_is_destroy);
		
};

#endif // !defined(HUB_FRAME_H)

/**
 * @file
 * $Id: HubFrame.h,v 1.78 2006/10/22 18:57:56 bigmuscle Exp $
 */
