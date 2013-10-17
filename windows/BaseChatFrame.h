/*
 * Copyright (C) 2012-2013 FlylinkDC++ Team http://flylinkdc.com/
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
#ifdef _DEBUG
	, boost::noncopyable
#endif
{
		typedef InternetSearchBaseHandler<BaseChatFrame> isBase;
		
		BEGIN_MSG_MAP(BaseChatFrame)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_FORWARDMSG, OnForwardMsg)
		COMMAND_ID_HANDLER(IDC_WINAMP_SPAM, onWinampSpam)
		NOTIFY_CODE_HANDLER(TTN_GETDISPINFO, onGetToolTip)
		CHAIN_COMMANDS(isBase)
		CHAIN_MSG_MAP_MEMBER(ctrlClient)
		// CHAIN_MSG_MAP_MEMBER_PTR(m_msgPanel)
		{
			if (!g_isStartupProcess) // try fix https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=38156
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
		}
		COMMAND_ID_HANDLER(IDC_MESSAGEPANEL, onMultilineChatInputButton)
		COMMAND_ID_HANDLER(ID_TEXT_TRANSCODE, OnTextTranscode)
		COMMAND_RANGE_HANDLER(IDC_BOLD, IDC_STRIKE, onTextStyleSelect)
		END_MSG_MAP()
	public:
		void createMessagePanel();
		void destroyMessagePanel(bool p_is_shutdown);
		const string& getHubHint() const // [!] IRainman fix.
		{
			return ctrlClient.getHubHint();
		}
	protected:
		void createMessageCtrl(ATL::CMessageMap *p_map, DWORD p_MsgMapID);
		void destroyMessageCtrl(bool p_is_shutdown);
	protected:
	
		BaseChatFrame() :
			m_curCommandPosition(0),
			m_bUseTempMultiChat(false),
			m_bProcessNextChar(false),
			m_timeStamps(BOOLSETTING(TIME_STAMPS)),
			m_currentNeedlePos(-1),
			m_msgPanel(nullptr),
			m_MessagePanelHWnd(0),
			m_ctrlLastLinesToolTip(nullptr),
			m_ctrlStatus(nullptr),
			m_ctrlMessage(nullptr),
			m_ctrlMessageContainer(nullptr)
		{
		}
		virtual ~BaseChatFrame() {}
		virtual void doDestroyFrame() = 0;
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE; // Обязательно чтобы продолжить обработку дальше. http://www.rsdn.ru/forum/atl/633568.1
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
		LRESULT onTextStyleSelect(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (m_ctrlMessage)
				WinUtil::SetBBCodeForCEdit(*m_ctrlMessage, wID);
			return 0;
		}
		LRESULT OnTextTranscode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (m_ctrlMessage)
				WinUtil::TextTranscode(*m_ctrlMessage);
			return 0;
		}
		LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			const HWND hWnd = (HWND)lParam;
			const HDC hDC = (HDC)wParam;
			if (hWnd == ctrlClient.m_hWnd || (m_ctrlMessage && hWnd == m_ctrlMessage->m_hWnd)) // TODO: please verify this!
			{
				::SetBkColor(hDC, Colors::bgColor);
				::SetTextColor(hDC, Colors::textColor);
				return (LRESULT)Colors::bgBrush;
			}
			bHandled = FALSE;
			return FALSE;
		}
		LRESULT onSearchFileInInternet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (!ChatCtrl::sSelectedText.empty())
			{
				searchFileInInternet(wID, ChatCtrl::sSelectedText);
			}
			return 0;
		}
		LRESULT onMultilineChatInputButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetToolTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		tstring findTextPopup();
		void findText(const tstring & needle) noexcept;
		
		virtual void processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText) = 0;
		virtual void processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText) = 0;
		
		virtual void sendMessage(const tstring& msg, bool thirdperson = false) = 0;
		void addLine(const tstring& aLine, CHARFORMAT2& cf = Colors::g_ChatTextGeneral);
		virtual void addLine(const Identity& ou, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, const CHARFORMAT2& cf, tstring& extra);
		virtual void addStatus(const tstring& aLine, const bool bInChat = true, const bool bHistory = true, const CHARFORMAT2& cf = Colors::g_ChatTextSystem);
		virtual void UpdateLayout(BOOL bResizeBars = TRUE) = 0;
		
		static tstring getIpCountry(const string& ip, bool ts, bool p_ipInChat, bool p_countryInChat);
		
		void appendChatCtrlItems(OMenu& p_menu, const Client* client = nullptr);
		
		static TCHAR getChatRefferingToNick()
		{
#ifdef SCALOLAZ_CHAT_REFFERING_TO_NICK
			return BOOLSETTING(CHAT_REFFERING_TO_NICK) ? _T(',') : _T(':');
#else
			return _T(',');
#endif
		}
		
		void appendNickToChat(const tstring& nick);
		
		void appendLogToChat(const string& path , const size_t linesCount);
		virtual void readFrameLog() = 0;
		ChatCtrl ctrlClient;
		
		CEdit*        m_ctrlMessage;
		tstring       m_LastMessage;
		MessagePanel* m_msgPanel;
		CContainedWindow* m_ctrlMessageContainer;
		RECT          m_MessagePanelRECT;
		HWND          m_MessagePanelHWnd;
		CFlyToolTipCtrl* m_ctrlLastLinesToolTip; // TODO - создаются выше в наследника - не красиво. (не доступен метод CreateSimpleStatusBar)
		CStatusBarCtrl*  m_ctrlStatus; // TODO - создаются выше в наследника - не красиво (не доступен метод CreateSimpleStatusBar)
		void createStatusCtrl(HWND p_hWndStatusBar);
		void destroyStatusCtrl();
		std::vector<tstring> m_ctrlStatusCache; // Пока не создан GUI - текст сохраняем тут
		void setStatusText(int p_index, const tstring& p_text);
		void restoreStatusFromCache();
		void destroyStatusbar(bool p_is_shutdown);
		
		enum { MAX_CLIENT_LINES = 10 }; // TODO copy-paste
		TStringList lastLinesList;
		tstring lastLines;
		StringMap ucLineParams;
		
		TStringList m_prevCommands;
		tstring m_currentCommand;
		size_t m_curCommandPosition;
		bool m_bUseTempMultiChat;
	private:
		bool m_bProcessNextChar;
		bool m_timeStamps;
		tstring m_currentNeedle;      // search in chat window
		long m_currentNeedlePos;      // search in chat window
		
		bool adjustChatInputSize(BOOL& bHandled);
		void insertLineHistoryToChatInput(const WPARAM wParam, BOOL& bHandled);
	public:
		static bool g_isStartupProcess;
};