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

#if !defined(PRIVATE_FRAME_H)
#define PRIVATE_FRAME_H

#include "../client/ClientManagerListener.h"
#include "../client/ResourceManager.h"

#include "HubFrame.h"
#include "../client/QueueManager.h"

#define PM_MESSAGE_MAP 9

class PrivateFrame : public MDITabChildWindowImpl < PrivateFrame, RGB(0, 255, 255), IDR_PRIVATE, IDR_PRIVATE_OFF > ,
	private ClientManagerListener, public UCHandler<PrivateFrame>,
	public UserInfoBaseHandler < PrivateFrame, UserInfoGuiTraits::NO_SEND_PM | UserInfoGuiTraits::USER_LOG > , // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=621
	private SettingsManagerListener
	, private BaseChatFrame // [+] IRainman copy-past fix.
{
	public:
		static bool gotMessage(const Identity& from, const Identity& to, const Identity& replyTo, const tstring& aMessage, const string& sHubHint, const bool bMyMess, const bool bThirdPerson, const bool notOpenNewWindow = false); // !SMT!-S
		static void openWindow(const OnlineUserPtr& ou, const HintedUser& replyTo, string myNick = Util::emptyString, const tstring& aMessage = Util::emptyStringT);
		static bool isOpen(const UserPtr& u)
		{
			return g_pm_frames.find(u) != g_pm_frames.end();
		}
		static bool closeUser(const UserPtr& u); // !SMT!-S
		static void closeAll();
		static void closeAllOffline();
		
		enum
		{
			USER_UPDATED
		};
		
		DECLARE_FRAME_WND_CLASS_EX(_T("PrivateFrame"), IDR_PRIVATE, 0, COLOR_3DFACE);
		
		typedef MDITabChildWindowImpl < PrivateFrame, RGB(0, 255, 255), IDR_PRIVATE, IDR_PRIVATE_OFF > baseClass;
		typedef UCHandler<PrivateFrame> ucBase;
		typedef UserInfoBaseHandler < PrivateFrame, UserInfoGuiTraits::NO_SEND_PM | UserInfoGuiTraits::USER_LOG > uiBase; // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=621
		
		BEGIN_MSG_MAP(PrivateFrame)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		CHAIN_MSG_MAP(BaseChatFrame)
		COMMAND_ID_HANDLER(IDC_SEND_MESSAGE, onSendMessage)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_OFFLINE_PM, onCloseAllOffline) // [+] InfinitySky.
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_PM, onCloseAll) // [+] InfinitySky.
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_OPEN_USER_LOG, onOpenUserLog)
		CHAIN_COMMANDS(ucBase) // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=621
		CHAIN_COMMANDS(uiBase) // fix http://code.google.com/p/flylinkdc/issues/detail?id=1406
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(PM_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		MESSAGE_HANDLER(WM_KEYDOWN, onChar)
		MESSAGE_HANDLER(WM_KEYUP, onChar)
		MESSAGE_HANDLER(WM_LBUTTONDBLCLK, onLButton) // !Decker!
		END_MSG_MAP()
		
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			openFrameLog();
			return 0;
		}
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled); // !Decker!
		
		void onBeforeActiveTab(HWND aWnd); // http://code.google.com/p/flylinkdc/issues/detail?id=1242
		void onAfterActiveTab(HWND aWnd);
		void onInvalidateAfterActiveTab(HWND aWnd);
		
		void addLine(const Identity& from, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, const CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void runUserCommand(UserCommand& uc);
		void readFrameLog();
		void openFrameLog() const
		{
			WinUtil::openLog(SETTING(LOG_FILE_PRIVATE_CHAT), getFrameLogParams(), TSTRING(NO_LOG_FOR_USER));
		}
		
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		// [+] InfinitySky.
		LRESULT onCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			closeAll();
			return 0;
		}
		
		// [+] InfinitySky.
		LRESULT onCloseAllOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			closeAllOffline();
			return 0;
		}
		
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
		{
			updateTitle();
			return 0;
		}
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (m_ctrlMessage)
				m_ctrlMessage->SetFocus();
			if (ctrlClient.IsWindow())
				ctrlClient.GoToEnd(false);
			return 0;
		}
		
		void addStatus(const tstring& aLine, const bool bInChat = true, const bool bHistory = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem)
		{
			if (!m_created)
			{
				CreateEx(WinUtil::g_mdiClient);
			}
			BaseChatFrame::addStatus(aLine, bInChat, bHistory, cf);
		}
		
		void sendMessage(const tstring& msg, bool thirdperson = false) override;
		
		const UserPtr& getUser() const
		{
			return m_replyTo.user;
		}
		const string& getHub() const
		{
			return m_replyTo.hint;
		}
	private:
		PrivateFrame(const HintedUser& replyTo_, const string& myNick);
		~PrivateFrame();
		virtual void doDestroyFrame();
		
		bool m_created; // TODO: fix me please.
		typedef std::unordered_map<UserPtr, PrivateFrame*, User::Hash> FrameMap;
		static FrameMap g_pm_frames;
		static std::unordered_map<string, unsigned> g_count_pm;
		
#define MAX_PM_FRAMES 100
		
		const HintedUser m_replyTo; // [+] IRainman fix: this is const ptr.
		tstring m_replyToRealName; // [+] IRainman fix.
		
		CContainedWindow m_ctrlChatContainer;
		
		bool m_isoffline;
		
		void updateTitle();
		
		// ClientManagerListener
		void on(ClientManagerListener::UserUpdated, const OnlineUserPtr& aUser) noexcept override   // !SMT!-fix
		{
			on(ClientManagerListener::UserDisconnected(), aUser->getUser()); // !SMT!-fix
		}
		void on(ClientManagerListener::UserConnected, const UserPtr& aUser) noexcept override
		{
			on(ClientManagerListener::UserDisconnected(), aUser);
		}
		void on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept override
		{
			if (aUser == m_replyTo.user)
			{
				PostMessage(WM_SPEAKER, USER_UPDATED);
			}
		}
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/) override;
		// [+] IRainman: copy-past fix.
		void processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText);
		void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText);
		
		void addMesageLogParams(StringMap& params, const Identity& from, const tstring& aLine, bool bThirdPerson, const tstring& extra);
		StringMap getFrameLogParams() const;
		// [~] IRainman: copy-past fix.
	public:
		void createMessagePanel();
		void destroyMessagePanel(bool p_is_destroy);
};

#endif // !defined(PRIVATE_FRAME_H)

/**
 * @file
 * $Id: PrivateFrame.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
