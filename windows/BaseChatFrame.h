/*
 * Copyright (C) 2012-2017 FlylinkDC++ Team http://flylinkdc.com
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

#pragma once

#include "FlatTabCtrl.h"
#include "ChatCtrl.h"
#include "LineDlg.h"
#include "MessagePanel.h"

class BaseChatFrame : public InternetSearchBaseHandler<BaseChatFrame>
{
		typedef InternetSearchBaseHandler<BaseChatFrame> isBase;
		
		BEGIN_MSG_MAP(BaseChatFrame)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_FORWARDMSG, OnForwardMsg)
		COMMAND_ID_HANDLER(IDC_WINAMP_SPAM, onWinampSpam)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, onGetToolTip)
		CHAIN_COMMANDS(isBase)
		CHAIN_MSG_MAP_MEMBER(ctrlClient)
		//CHAIN_MSG_MAP_MEMBER_PTR(m_msgPanel)
		if (ClientManager::isStartup() == false) // try fix https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=38156
		{
			if (m_msgPanel && m_msgPanel->ProcessWindowMessage(hWnd, uMsg, wParam, lParam, lResult))
			{
				return TRUE;
			}
		}
		else
		{
			// dcassert(0);
		}
		COMMAND_ID_HANDLER(IDC_MESSAGEPANEL, onMultilineChatInputButton)
		COMMAND_ID_HANDLER(ID_TEXT_TRANSCODE, OnTextTranscode)
		COMMAND_RANGE_HANDLER(IDC_BOLD, IDC_STRIKE, onTextStyleSelect)
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		COMMAND_ID_HANDLER(IDC_COLOR, onTextStyleSelect)
#endif
		COMMAND_ID_HANDLER(IDC_OSAGO, onOSAGOSelect)
		
		COMMAND_HANDLER(IDC_CHAT_MESSAGE_EDIT, EN_CHANGE, onChange)
		END_MSG_MAP()
	public:
		void createMessagePanel();
		void destroyMessagePanel(bool p_is_shutdown);
		string getHubHint() const
		{
			return ctrlClient.getHubHint();
		}
	private:
		void createChatCtrl();
	protected:
		OMenu* m_userMenu;
		OMenu* createUserMenu();
		void destroyUserMenu();
		void createMessageCtrl(ATL::CMessageMap *p_map, DWORD p_MsgMapID, bool p_is_suppress_chat_and_pm);
		void destroyMessageCtrl(bool p_is_shutdown);
		
		BaseChatFrame() :
			m_curCommandPosition(0),
			m_bUseTempMultiChat(false),
			m_MultiChatCountLines(0),
			m_bProcessNextChar(false),
			m_bTimeStamps(BOOLSETTING(TIME_STAMPS)),
			m_currentNeedlePos(-1),
			m_msgPanel(nullptr),
			m_MessagePanelHWnd(0),
			m_ctrlLastLinesToolTip(nullptr),
			m_ctrlStatus(nullptr),
			m_ctrlMessage(nullptr),
			m_ctrlMessageContainer(nullptr),
			m_LastSelPos(0),
			m_userMenu(nullptr),
			m_is_suppress_chat_and_pm(false)
		{
			RECT r = { 0 };
			m_MessagePanelRECT = r;
		}
		virtual ~BaseChatFrame() {}
		virtual void doDestroyFrame() = 0;
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE; // ����������� ����� ���������� ��������� ������. http://www.rsdn.ru/forum/atl/633568.1
			doDestroyFrame();
			return 0;
		}
		
		LRESULT OnCreate(HWND p_hWnd, RECT &rcDefault);
		bool processingServices(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		void processingHotKeys(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onSendMessage(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			onEnter();
			return 0;
		}
		void onEnter();
		LRESULT onWinampSpam(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOSAGOSelect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTextStyleSelect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnTextTranscode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onSearchFileInInternet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMultilineChatInputButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetToolTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		tstring findTextPopup();
		void findText(const tstring & needle) noexcept;
		
		virtual void processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText) = 0;
		virtual void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText) = 0;
		
		virtual void sendMessage(const tstring& msg, bool thirdperson = false) = 0;
		void addLine(const tstring& aLine, unsigned p_max_smiles, CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		virtual void addLine(const Identity& ou, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, unsigned p_max_smiles, const CHARFORMAT2& cf, tstring& extra);
		virtual void addStatus(const tstring& aLine, const bool bInChat = true, const bool bHistory = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem);
		virtual void UpdateLayout(BOOL bResizeBars = TRUE) = 0;
		
		static tstring getIpCountry(const string& ip, bool ts, bool p_ipInChat, bool p_countryInChat, bool p_ISPInChat);
		static TCHAR getChatRefferingToNick();
		
		void appendChatCtrlItems(OMenu& p_menu, const Client* client = nullptr);
		
		void appendNickToChat(const tstring& nick);
		void appendLogToChat(const string& path, const size_t linesCount);
		virtual void readFrameLog() = 0;
		ChatCtrl ctrlClient;
		
		CEdit*        m_ctrlMessage;
		tstring       m_LastMessage;
		DWORD         m_LastSelPos;
		MessagePanel* m_msgPanel;
		CContainedWindow* m_ctrlMessageContainer;
		RECT          m_MessagePanelRECT;
		HWND          m_MessagePanelHWnd;
		CFlyToolTipCtrl* m_ctrlLastLinesToolTip; // TODO - ��������� ���� � ���������� - �� �������. (�� �������� ����� CreateSimpleStatusBar)
		CStatusBarCtrl*  m_ctrlStatus; // TODO - ��������� ���� � ���������� - �� ������� (�� �������� ����� CreateSimpleStatusBar)
		bool m_is_suppress_chat_and_pm;
		
		void createStatusCtrl(HWND p_hWndStatusBar);
		void destroyStatusCtrl();
		std::vector<std::pair<tstring, bool> > m_ctrlStatusCache; // ���� �� ������ GUI - ����� ��������� ���
		void setStatusText(unsigned char p_index, const tstring& p_text);
		void restoreStatusFromCache();
		void destroyStatusbar(bool p_is_shutdown);
		
		enum { MAX_CLIENT_LINES = 10 }; // TODO copy-paste
		std::list<wstring> m_lastLinesList;
		tstring m_lastLines;
		StringMap ucLineParams;
		
		TStringList m_prevCommands;
		tstring m_currentCommand;
		size_t m_curCommandPosition;
		bool m_bUseTempMultiChat;
		bool isMultiChat(int& p_h, int& p_chat_columns) const;
		void clearMessageWindow();
	protected:
		unsigned m_MultiChatCountLines;
	private:
		bool m_bProcessNextChar;
		bool m_bTimeStamps;
		tstring m_currentNeedle;      // search in chat window
		long m_currentNeedlePos;      // search in chat window
		
		bool adjustChatInputSize(BOOL& bHandled);
		void checkMultiLine();
		void insertLineHistoryToChatInput(const WPARAM wParam, BOOL& bHandled);
};