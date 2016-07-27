/*
 * Copyright (C) 2012-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#include "stdafx.h"
#include "BaseChatFrame.h"
#include "../client/QueueManager.h"

LRESULT BaseChatFrame::OnCreate(HWND p_hWnd, RECT &rcDefault)
{
	m_MessagePanelRECT = rcDefault;
	m_MessagePanelHWnd = p_hWnd;
	return 1;
}
void BaseChatFrame::destroyStatusbar(bool p_is_shutdown)
{
	safe_destroy_window(m_ctrlStatus);
	safe_destroy_window(m_ctrlLastLinesToolTip);
	safe_delete(m_ctrlStatus);
	safe_delete(m_ctrlLastLinesToolTip);
}

void BaseChatFrame::createStatusCtrl(HWND p_hWndStatusBar)
{
	m_ctrlStatus = new CStatusBarCtrl;
	m_ctrlStatus->Attach(p_hWndStatusBar);
	m_ctrlLastLinesToolTip = new CFlyToolTipCtrl;
	m_ctrlLastLinesToolTip->Create(m_ctrlStatus->m_hWnd, m_MessagePanelRECT , _T("Fly_BaseChatFrame_ToolTips"), WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	m_ctrlLastLinesToolTip->SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	m_ctrlLastLinesToolTip->AddTool(m_ctrlStatus->m_hWnd);
	m_ctrlLastLinesToolTip->SetDelayTime(TTDT_AUTOPOP, 15000);
}

void BaseChatFrame::destroyStatusCtrl()
{
}
void BaseChatFrame::createChatCtrl()
{
	if (!ctrlClient.IsWindow())
	{
		const auto l_res = ctrlClient.Create(m_MessagePanelHWnd, m_MessagePanelRECT, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                                     WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_READONLY, WS_EX_STATICEDGE, IDC_CLIENT);
		if (!l_res)
		{
			dcdebug("Error create BaseChatFrame::createChatCtrl %s", Util::translateError().c_str());
			dcassert(0);
		}
		else
		{
			ctrlClient.LimitText(0);
			ctrlClient.SetFont(Fonts::g_font);
			ctrlClient.SetAutoURLDetect(false);
			ctrlClient.SetEventMask(ctrlClient.GetEventMask() | ENM_LINK);
			ctrlClient.SetBackgroundColor(Colors::g_bgColor);
			if (m_is_suppress_chat_and_pm)
			{
				//   ctrlClient.ShowWindow(SW_HIDE);
			}
			else
			{
				readFrameLog();
			}
		}
	}
}
void BaseChatFrame::createMessageCtrl(ATL::CMessageMap *p_map, DWORD p_MsgMapID, bool p_is_suppress_chat_and_pm)
{
	m_is_suppress_chat_and_pm = p_is_suppress_chat_and_pm;
	dcassert(m_ctrlMessage == nullptr);
	createChatCtrl();
	m_ctrlMessage = new CEdit;
	m_ctrlMessage->Create(m_MessagePanelHWnd,
	                      m_MessagePanelRECT,
	                      NULL,
	                      WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
	                      WS_EX_CLIENTEDGE,
	                      IDC_CHAT_MESSAGE_EDIT);
	if (!m_LastMessage.empty())
	{
		m_ctrlMessage->SetWindowText(m_LastMessage.c_str());
		m_ctrlMessage->SetSel(m_LastSelPos);
	}
	m_ctrlMessage->SetFont(Fonts::g_font);
	m_ctrlMessage->SetLimitText(9999);
	m_ctrlMessageContainer = new CContainedWindow(WC_EDIT, p_map, p_MsgMapID);
	m_ctrlMessageContainer->SubclassWindow(m_ctrlMessage->m_hWnd);
	
	if (p_is_suppress_chat_and_pm)
	{
		m_ctrlMessage->SetReadOnly();
		m_ctrlMessage->EnableWindow(FALSE);
	}
}
void BaseChatFrame::destroyMessageCtrl(bool p_is_shutdown)
{
	dcassert(!m_msgPanel); // Панелька должна быть разрушена до этого
	//safe_unsubclass_window(m_ctrlMessageContainer);
	if (m_ctrlMessage)
	{
		if (!p_is_shutdown && m_ctrlMessage->IsWindow()) // fix https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=36887
		{
			WinUtil::GetWindowText(m_LastMessage, *m_ctrlMessage);
			m_LastSelPos = m_ctrlMessage->GetSel();
		}
		safe_destroy_window(m_ctrlMessage);
	}
	safe_delete(m_ctrlMessage);
	safe_delete(m_ctrlMessageContainer);
}
void BaseChatFrame::createMessagePanel()
{
	dcassert(!ClientManager::isShutdown());
	
	if (!m_msgPanel && ClientManager::isStartup() == false)
	{
		m_msgPanel = new MessagePanel(m_ctrlMessage);
		m_msgPanel->InitPanel(m_MessagePanelHWnd, m_MessagePanelRECT);
		ctrlClient.restore_chat_cache();
	}
}
void BaseChatFrame::destroyMessagePanel(bool p_is_shutdown)
{
	if (m_msgPanel)
	{
		m_msgPanel->DestroyPanel(p_is_shutdown); // Тоже падаем при разрушении https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=36613
		safe_delete(m_msgPanel);
	}
}

void BaseChatFrame::setStatusText(unsigned char p_index, const tstring& p_text)
{
	dcassert(!ClientManager::isShutdown());
	dcassert(p_index < m_ctrlStatusCache.size());
	if (p_index < m_ctrlStatusCache.size())
	{
		m_ctrlStatusCache[p_index].second = m_ctrlStatusCache[p_index].first != p_text;
		if (m_ctrlStatusCache[p_index].second)
		{
			m_ctrlStatusCache[p_index].first = p_text;
			if (m_ctrlStatus)
			{
				m_ctrlStatus->SetText(p_index, p_text.c_str());
				m_ctrlStatusCache[p_index].second = false;
			}
		}
	}
}

void BaseChatFrame::restoreStatusFromCache()
{
	CLockRedraw<true> l_lock_draw(m_ctrlStatus->m_hWnd);
	for (size_t i = 0 ; i < m_ctrlStatusCache.size(); ++i)
	{
		m_ctrlStatus->SetText(i, m_ctrlStatusCache[i].first.c_str());
		m_ctrlStatusCache[i].second = false;
	}
}

void BaseChatFrame::checkMultiLine()
{
	if (m_ctrlMessage && m_ctrlMessage->GetWindowTextLength() > 0)
	{
		tstring fullMessageText;
		WinUtil::GetWindowText(fullMessageText, *m_ctrlMessage);
		const unsigned l_count_lines = std::count(fullMessageText.cbegin(), fullMessageText.cend(), L'\r');
		if (l_count_lines != m_MultiChatCountLines)
		{
			m_MultiChatCountLines = std::min(l_count_lines, unsigned(10));
			UpdateLayout();
		}
	}
}
bool BaseChatFrame::adjustChatInputSize(BOOL& bHandled)
{
	bool needsAdjust = WinUtil::isCtrlOrAlt();
	if (BOOLSETTING(MULTILINE_CHAT_INPUT) && BOOLSETTING(MULTILINE_CHAT_INPUT_BY_CTRL_ENTER))  // [+] SSA - Added Enter for MulticharInput
	{
		needsAdjust = !needsAdjust;
	}
	if (needsAdjust)
	{
		bHandled = FALSE;
		if (!BOOLSETTING(MULTILINE_CHAT_INPUT) && !m_bUseTempMultiChat)
		{
			m_bUseTempMultiChat = true;
			UpdateLayout();
		}
	}
	checkMultiLine();
	return needsAdjust;
}
LRESULT BaseChatFrame::onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND hWnd = (HWND)lParam;
	const HDC hDC = (HDC)wParam;
	if (hWnd == ctrlClient.m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	else if (m_is_suppress_chat_and_pm == false && m_ctrlMessage && hWnd == m_ctrlMessage->m_hWnd)
	{
		return Colors::setColor(hDC);
	}
	bHandled = FALSE;
	return FALSE;
}

void BaseChatFrame::insertLineHistoryToChatInput(const WPARAM wParam, BOOL& bHandled)
{
	const bool allowToInsertLineHistory = (WinUtil::isAlt() || (WinUtil::isCtrl() && BOOLSETTING(USE_CTRL_FOR_LINE_HISTORY)));
	if (allowToInsertLineHistory)
	{
		switch (wParam)
		{
			case VK_UP:
				//scroll up in chat command history
				//currently beyond the last command?
				if (m_ctrlMessage)
				{
					if (m_curCommandPosition > 0)
					{
						//check whether current command needs to be saved
						if (m_curCommandPosition == m_prevCommands.size())
						{
							WinUtil::GetWindowText(m_currentCommand, *m_ctrlMessage);
						}
						//replace current chat buffer with current command
						m_ctrlMessage->SetWindowText(m_prevCommands[--m_curCommandPosition].c_str());
					}
					// move cursor to end of line
					m_ctrlMessage->SetSel(m_ctrlMessage->GetWindowTextLength(), m_ctrlMessage->GetWindowTextLength());
				}
				break;
			case VK_DOWN:
				//scroll down in chat command history
				//currently beyond the last command?
				if (m_ctrlMessage)
				{
					if (m_curCommandPosition + 1 < m_prevCommands.size())
					{
						//replace current chat buffer with current command
						m_ctrlMessage->SetWindowText(m_prevCommands[++m_curCommandPosition].c_str());
					}
					else if (m_curCommandPosition + 1 == m_prevCommands.size())
					{
						//revert to last saved, unfinished command
						m_ctrlMessage->SetWindowText(m_currentCommand.c_str());
						++m_curCommandPosition;
					}
					// move cursor to end of line
					m_ctrlMessage->SetSel(m_ctrlMessage->GetWindowTextLength(), m_ctrlMessage->GetWindowTextLength());
				}
				break;
			case VK_HOME:
				if (m_ctrlMessage)
				{
					if (!m_prevCommands.empty())
					{
						m_curCommandPosition = 0;
						WinUtil::GetWindowText(m_currentCommand, *m_ctrlMessage);
						m_ctrlMessage->SetWindowText(m_prevCommands[m_curCommandPosition].c_str());
					}
				}
				break;
			case VK_END:
				m_curCommandPosition = m_prevCommands.size();
				if (m_ctrlMessage)
				{
					m_ctrlMessage->SetWindowText(m_currentCommand.c_str());
				}
				break;
		}
	}
	else
	{
		bHandled = FALSE;
	}
}
LRESULT BaseChatFrame::onChange(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	checkMultiLine();
	return 0;
}

bool BaseChatFrame::processingServices(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	const bool isService = uMsg != WM_KEYDOWN;
	if (isService)
	{
		switch (wParam)
		{
			case VK_RETURN:
				adjustChatInputSize(bHandled);
				break;
			case VK_TAB:
				bHandled = TRUE;
				break;
			case 0x0A:
				if (m_bProcessNextChar)
				{
					bHandled = TRUE;
					break;
				}
			default:
				bHandled = FALSE;
				break;
		}
		m_bProcessNextChar = false;
		if (uMsg == WM_CHAR && m_ctrlMessage && GetFocus() == m_ctrlMessage->m_hWnd && wParam != VK_RETURN && wParam != VK_TAB && wParam != VK_BACK)
		{
			PLAY_SOUND(SOUND_TYPING_NOTIFY);
		}
	}
	return isService;
}

void BaseChatFrame::processingHotKeys(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (wParam == VK_ESCAPE)
	{
		// Clear find text and give the focus back to the message box
		if (m_ctrlMessage)
			m_ctrlMessage->SetFocus();
		if (ctrlClient.IsWindow())
		{
			ctrlClient.SetSel(-1, -1);
			ctrlClient.SendMessage(EM_SCROLL, SB_BOTTOM, 0);
			ctrlClient.InvalidateRect(NULL);
		}
		m_currentNeedle.clear();
	}
	// Processing find.
	else if (((wParam == VK_F3 && WinUtil::isShift()) || (wParam == 'F' && WinUtil::isCtrl())) && !WinUtil::isAlt())
	{
		findText(findTextPopup());
	}
	else if (wParam == VK_F3)
	{
		findText(m_currentNeedle.empty() ? findTextPopup() : m_currentNeedle);
	}
	// ~Processing find.
	else
	{
		// don't handle these keys unless the user is entering a message
		if (m_ctrlMessage && GetFocus() == m_ctrlMessage->m_hWnd)
		{
			switch (wParam)
			{
				case 'A': // [+] birkoff.anarchist
					if (WinUtil::isCtrl() && !WinUtil::isAlt() && !WinUtil::isShift())
					{
						m_ctrlMessage->SetSelAll();
					}
					break;
				case VK_RETURN:
				{
					if (!adjustChatInputSize(bHandled))
					{
						onEnter();
						if (BOOLSETTING(MULTILINE_CHAT_INPUT) && BOOLSETTING(MULTILINE_CHAT_INPUT_BY_CTRL_ENTER))  // [+] SSA - Added Enter for MulticharInput
						{
							m_bProcessNextChar = true;
						}
					}
				}
				break;
				case VK_UP:
				case VK_DOWN:
				case VK_HOME:
				case VK_END:
					insertLineHistoryToChatInput(wParam, bHandled);
					break;
				case VK_PRIOR: // page up
					if (ctrlClient.IsWindow())
						ctrlClient.SendMessage(WM_VSCROLL, SB_PAGEUP);
					break;
				case VK_NEXT: // page down
					if (ctrlClient.IsWindow())
						ctrlClient.SendMessage(WM_VSCROLL, SB_PAGEDOWN);
					break;
				default:
					bHandled = FALSE;
			}
			checkMultiLine();
		}
		else
		{
			bHandled = FALSE;
		}
	}
}

LRESULT BaseChatFrame::OnForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	LPMSG pMsg = (LPMSG)lParam;
	if (pMsg->message >= WM_MOUSEFIRST && pMsg->message <= WM_MOUSELAST && m_ctrlLastLinesToolTip)
	{
		m_ctrlLastLinesToolTip->RelayEvent(pMsg);
	}
	
	return 0;
}

void BaseChatFrame::onEnter()
{
	bool resetInputMessageText = true;
	dcassert(m_ctrlMessage);
	if (m_ctrlMessage && m_ctrlMessage->GetWindowTextLength() > 0)
	{
		tstring fullMessageText;
		WinUtil::GetWindowText(fullMessageText, *m_ctrlMessage);
		
		// save command in history, reset current buffer pointer to the newest command
		m_curCommandPosition = m_prevCommands.size();       //this places it one position beyond a legal subscript
		if (!m_curCommandPosition || (m_curCommandPosition > 0 && m_prevCommands[m_curCommandPosition - 1] != fullMessageText))
		{
			++m_curCommandPosition;
			m_prevCommands.push_back(fullMessageText);
		}
		m_currentCommand.clear();
		
		// Process special commands
		if (fullMessageText[0] == '/')
		{
			tstring cmd = fullMessageText;
			tstring param;
			tstring message;
			tstring local_message;
			tstring status;
			if (WinUtil::checkCommand(cmd, param, message, status, local_message))
			{
				if (!message.empty())
				{
					sendMessage(message);
				}
				if (!status.empty())
				{
					addStatus(status);
				}
				if (!local_message.empty())
				{
					addLine(local_message, Colors::g_ChatTextSystem);
				}
			}
			else if ((stricmp(cmd.c_str(), _T("clear")) == 0) || (stricmp(cmd.c_str(), _T("cls")) == 0) || (stricmp(cmd.c_str(), _T("c")) == 0))
			{
				if (ctrlClient.IsWindow())
				{
					ctrlClient.Clear();
				}
			}
			
			else if (stricmp(cmd.c_str(), _T("ts")) == 0)
			{
				m_bTimeStamps = !m_bTimeStamps;
				if (m_bTimeStamps)
				{
					addStatus(TSTRING(TIMESTAMPS_ENABLED));
				}
				else
				{
					addStatus(TSTRING(TIMESTAMPS_DISABLED));
				}
			}
			
			else if (stricmp(cmd.c_str(), _T("find")) == 0)
			{
				if (param.empty())
					param = findTextPopup();
					
				findText(param);
			}
			else if (stricmp(cmd.c_str(), _T("savequeue")) == 0)
			{
				QueueManager::getInstance()->saveQueue();
				addStatus(TSTRING(QUEUE_SAVED));
#ifdef IRAINMAN_ENABLE_WHOIS
			}
			else if (stricmp(cmd.c_str(), _T("whois")) == 0)
			{
				WinUtil::openLink(_T("http://www.ripe.net/perl/whois?form_type=simple&full_query_string=&searchtext=") + Text::toT(Util::encodeURI(Text::fromT(param))));
#endif
			}
			else if (stricmp(cmd.c_str(), _T("ignorelist")) == 0)
			{
				tstring l_ignorelist = TSTRING(IGNORED_USERS) + _T(':');
				l_ignorelist += UserManager::getIgnoreListAsString();
				addLine(l_ignorelist, Colors::g_ChatTextSystem);
			}
			else if (stricmp(cmd.c_str(), _T("me")) == 0)
			{
				sendMessage(param, true);
			}
			else
			{
				processFrameCommand(fullMessageText, cmd, param, resetInputMessageText);
			}
		}
		else
		{
			processFrameMessage(fullMessageText, resetInputMessageText);
		}
		if (resetInputMessageText)
		{
			clearMessageWindow();
			UpdateLayout();
			return;
		}
	}
	else
	{
		MessageBeep(MB_ICONEXCLAMATION);
	}
	
	if (m_bUseTempMultiChat)
	{
		m_bUseTempMultiChat = false;
		UpdateLayout();
	}
}

LRESULT BaseChatFrame::onWinampSpam(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring cmd, param, message, status, local_message;
	if (SETTING(MEDIA_PLAYER) == SettingsManager::WinAmp)
	{
		cmd = _T("/winamp");
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::WinMediaPlayer)
	{
		cmd = _T("/wmp");
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::iTunes)
	{
		cmd = _T("/itunes");
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::WinMediaPlayerClassic)
	{
		cmd = _T("/mpc");
	}
	else if (SETTING(MEDIA_PLAYER) == SettingsManager::JetAudio)
	{
		cmd = _T("/ja");
	}
	else
	{
		addStatus(TSTRING(NO_MEDIA_SPAM));
		return 0;
	}
	if (WinUtil::checkCommand(cmd, param, message, status, local_message))
	{
		if (!message.empty())
		{
			sendMessage(message);
		}
		if (!status.empty())
		{
			addStatus(status);
		}
	}
	return 0;
}

tstring BaseChatFrame::findTextPopup()
{
	LineDlg finddlg;
	finddlg.title = TSTRING(SEARCH);
	finddlg.description = TSTRING(SPECIFY_SEARCH_STRING);
	if (finddlg.DoModal() == IDOK)
	{
		return finddlg.line;
	}
	return Util::emptyStringT;
}

void BaseChatFrame::findText(const tstring& needle) noexcept
{
	dcassert(ctrlClient.IsWindow());
	if (ctrlClient.IsWindow())
	{
		int max = ctrlClient.GetWindowTextLength();
		// a new search? reset cursor to bottom
		if (needle != m_currentNeedle || m_currentNeedlePos == -1)
		{
			m_currentNeedle = needle;
			m_currentNeedlePos = max;
		}
		// set current selection
		FINDTEXT ft = {0};
		ft.chrg.cpMin = m_currentNeedlePos;
		ft.lpstrText = needle.c_str();
		// empty search? stop
		if (!needle.empty())
		{
			// find upwards
			m_currentNeedlePos = (int)ctrlClient.SendMessage(EM_FINDTEXT, 0, (LPARAM) & ft);
			// not found? try again on full range
			if (m_currentNeedlePos == -1 && ft.chrg.cpMin != max)  // no need to search full range twice
			{
				m_currentNeedlePos = max;
				ft.chrg.cpMin = m_currentNeedlePos;
				m_currentNeedlePos = (int)ctrlClient.SendMessage(EM_FINDTEXT, 0, (LPARAM) & ft);
			}
			// found? set selection
			if (m_currentNeedlePos != -1)
			{
				ft.chrg.cpMin = m_currentNeedlePos;
				ft.chrg.cpMax = m_currentNeedlePos + static_cast<LONG>(needle.length());
				ctrlClient.SetFocus();
				ctrlClient.SendMessage(EM_EXSETSEL, 0, (LPARAM)&ft);
			}
			else
			{
				addStatus(TSTRING(STRING_NOT_FOUND) + _T(' ') + needle);
				m_currentNeedle.clear();
			}
		}
	}
}

void BaseChatFrame::addStatus(const tstring& aLine, const bool bInChat /*= true*/, const bool bHistory /*= true*/, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextSystem*/)
{
	dcassert(!ClientManager::isShutdown());
	if (ClientManager::isShutdown())
		return;
	tstring line = _T('[') + Text::toT(Util::getShortTimeString()) + _T("] ") + aLine;
	if (line.size() > 512)
	{
		line.resize(512);
	}
	setStatusText(0, line);
	
	if (bHistory)
	{
		lastLinesList.push_back(line);
		while (lastLinesList.size() > static_cast<size_t>(MAX_CLIENT_LINES))
			lastLinesList.erase(lastLinesList.begin());
	}
	
	if (bInChat && BOOLSETTING(STATUS_IN_CHAT))
	{
		addLine(_T("*** ") + aLine, Colors::g_ChatTextServer);
	}
}
tstring BaseChatFrame::getIpCountry(const string& ip, bool ts, bool p_ipInChat, bool p_countryInChat, bool p_ISPInChat)
{
	tstring l_result;
	if (!ip.empty())
	{
		l_result = ts ? _T(" | ") : _T(" ");
		if (p_ipInChat)
		{
			l_result += Text::toT(ip);
		}
		if (p_countryInChat || p_ISPInChat)
		{
			const Util::CustomNetworkIndex& l_location = Util::getIpCountry(ip);
			if (p_countryInChat && !l_location.getCountry().empty())
			{
				l_result += (p_ipInChat ? _T(" | ") : _T("")) + l_location.getCountry();
			}
			else
				p_countryInChat = false;
				
			if (p_ISPInChat && !l_location.getDescription().empty())
			{
				l_result += ((p_countryInChat || p_ipInChat) ? _T(" | ") : _T("")) + l_location.getDescription();
			}
			else
				p_ISPInChat = false;
		}
		if (!p_countryInChat && !p_ISPInChat)
			l_result += _T(" ");  // Fix Right Click Menu on IP without space after IP
	}
	return l_result;
}

void BaseChatFrame::addLine(const tstring& aLine, CHARFORMAT2& cf /*= Colors::g_ChatTextGeneral */)
{
#ifdef _DEBUG
	if (aLine.find(_T("&#124")) != tstring::npos)
	{
		dcassert(0);
	}
#endif
	if (m_bTimeStamps)
	{
		const ChatCtrl::CFlyChatCache l_message(ClientManager::getFlylinkDCIdentity(), false, true, _T('[') + Text::toT(Util::getShortTimeString()) + _T("] "), aLine, cf, false);
		ctrlClient.AppendText(l_message, true);
	}
	else
	{
		const ChatCtrl::CFlyChatCache l_message(ClientManager::getFlylinkDCIdentity(), false, true, Util::emptyStringT, aLine, cf, false);
		ctrlClient.AppendText(l_message, true);
	}
}

void BaseChatFrame::addLine(const Identity& from, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, const CHARFORMAT2& cf, tstring& extra)
{
	if (ctrlClient.IsWindow())
	{
		ctrlClient.AdjustTextSize();
	}
	const bool ipInChat = BOOLSETTING(IP_IN_CHAT);
	const bool countryInChat = BOOLSETTING(COUNTRY_IN_CHAT);
	const bool ISPInChat = BOOLSETTING(ISP_IN_CHAT);
	if (ipInChat || countryInChat || ISPInChat)
	{
		if (from.isFantomIP() == false)
		{
			const string l_ip = from.getIpAsString();
			extra = getIpCountry(l_ip, m_bTimeStamps, ipInChat, countryInChat, ISPInChat);
		}
	}
	if (m_bTimeStamps)
	{
		const ChatCtrl::CFlyChatCache l_message(from, bMyMess, bThirdPerson, _T('[') + Text::toT(Util::getShortTimeString()) + extra + _T("] "), aLine, cf, true);
		ctrlClient.AppendText(l_message, true);
	}
	else
	{
		const ChatCtrl::CFlyChatCache l_message(from, bMyMess, bThirdPerson, !extra.empty() ? _T('[') + extra + _T("] ") : Util::emptyStringT, aLine, cf, true);
		ctrlClient.AppendText(l_message, true);
	}
}

LRESULT BaseChatFrame::onGetToolTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTTDISPINFO* nm = (NMTTDISPINFO*)pnmh;
	lastLines.clear();
	for (auto i = lastLinesList.cbegin(); i != lastLinesList.cend(); ++i)
	{
		lastLines += *i;
		lastLines += _T("\r\n");
	}
	if (lastLines.size() > 2)
	{
		lastLines.erase(lastLines.size() - 2);
	}
	nm->lpszText = const_cast<TCHAR*>(lastLines.c_str());
	
	return 0;
}

LRESULT BaseChatFrame::onMultilineChatInputButton(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SET_SETTING(MULTILINE_CHAT_INPUT, !BOOLSETTING(MULTILINE_CHAT_INPUT));
	m_bUseTempMultiChat = false;
	UpdateLayout();
	checkMultiLine();
	return 0;
}

void BaseChatFrame::appendChatCtrlItems(OMenu& p_menu, const Client* client)
{
	if (!ChatCtrl::g_sSelectedIP.empty())
	{
		p_menu.InsertSeparatorFirst(ChatCtrl::g_sSelectedIP);
#ifdef IRAINMAN_ENABLE_WHOIS
		WinUtil::AppendMenuOnWhoisIP(p_menu, ChatCtrl::g_sSelectedIP, false);
		p_menu.AppendMenu(MF_SEPARATOR);
#endif
		if (client) // add menus, necessary only for windows hub here.
		{
			if (client->isOp())
			{
				//p_menu.AppendMenu(MF_SEPARATOR);
				p_menu.AppendMenu(MF_STRING, IDC_BAN_IP, (_T("!banip ") + ChatCtrl::g_sSelectedIP).c_str());
				p_menu.SetMenuDefaultItem(IDC_BAN_IP);
				p_menu.AppendMenu(MF_STRING, IDC_UNBAN_IP, (_T("!unban ") + ChatCtrl::g_sSelectedIP).c_str());
				
				p_menu.AppendMenu(MF_SEPARATOR);
			}
		}
	}
#ifdef OLD_MENU_HEADER //[~]JhaoDa
	else
	{
		p_menu.InsertSeparatorFirst(TSTRING(TEXT_STR));
	}
#endif
	
	p_menu.AppendMenu(MF_STRING, ID_EDIT_COPY, CTSTRING(COPY));
	p_menu.AppendMenu(MF_STRING, IDC_COPY_ACTUAL_LINE,  CTSTRING(COPY_LINE));
	if (!ChatCtrl::g_sSelectedURL.empty())
	{
		p_menu.AppendMenu(MF_STRING, IDC_COPY_URL, Util::isMagnetLink(ChatCtrl::g_sSelectedURL) ? CTSTRING(COPY_MAGNET_LINK) : CTSTRING(COPY_URL));
		
#ifdef IRAINMAN_ENABLE_WHOIS
		if (!Util::isMagnetLink(ChatCtrl::g_sSelectedURL))
		{
			p_menu.AppendMenu(MF_SEPARATOR);
			p_menu.AppendMenu(MF_STRING, IDC_WHOIS_URL, (TSTRING(WHO_IS) + _T(" URL: Bgp.He")/* + ChatCtrl::g_sSelectedURL*/).c_str());
			
		}
#endif
	}
	
	
	if (!ChatCtrl::g_sSelectedText.empty())   // [+] SCALOlaz: add Search for Selected Text in Chat
	{
		p_menu.AppendMenu(MF_SEPARATOR);
		appendInternetSearchItems(p_menu); // [!] IRainman fix.
	}
	p_menu.AppendMenu(MF_SEPARATOR);
	
	
	p_menu.AppendMenu(MF_STRING, ID_EDIT_SELECT_ALL, CTSTRING(SELECT_ALL));
	p_menu.AppendMenu(MF_STRING, ID_EDIT_CLEAR_ALL, CTSTRING(CLEAR));
	p_menu.AppendMenu(MF_SEPARATOR);
	
	
	p_menu.AppendMenu(MF_STRING, IDC_AUTOSCROLL_CHAT, CTSTRING(ASCROLL_CHAT));
	if (ctrlClient.GetAutoScroll())
	{
		p_menu.CheckMenuItem(IDC_AUTOSCROLL_CHAT, MF_BYCOMMAND | MF_CHECKED);
	}
}

void BaseChatFrame::appendNickToChat(const tstring& nick)
{
	dcassert(m_ctrlMessage)
	if (m_ctrlMessage)
	{
		CAtlString sUser(WinUtil::toAtlString(nick));
		CAtlString sText;
		int iSelBegin, iSelEnd;
		m_ctrlMessage->GetSel(iSelBegin, iSelEnd);
		m_ctrlMessage->GetWindowText(sText);
		
		if (iSelBegin == 0 && iSelEnd == 0)
		{
			sUser += getChatRefferingToNick();
			sUser += _T(' ');
			if (sText.IsEmpty())
			{
				m_ctrlMessage->SetWindowText(sUser);
				m_ctrlMessage->SetFocus();
				m_ctrlMessage->SetSel(m_ctrlMessage->GetWindowTextLength(), m_ctrlMessage->GetWindowTextLength());
			}
			else
			{
				m_ctrlMessage->ReplaceSel(sUser);
				m_ctrlMessage->SetFocus();
			}
		}
		else
		{
			sUser += _T(',');
			sUser += _T(' ');
			m_ctrlMessage->ReplaceSel(sUser);
			m_ctrlMessage->SetFocus();
		}
	}
}

void BaseChatFrame::appendLogToChat(const string& path , const size_t linesCount)
{
	static const int64_t LOG_SIZE_TO_READ = 64 * 1024;
	string buf;
	try
	{
		File f(path, File::READ, File::OPEN);
		const int64_t size = f.getSize();
		if (size > LOG_SIZE_TO_READ)
		{
			f.setPos(size - LOG_SIZE_TO_READ);
		}
		buf = f.read(LOG_SIZE_TO_READ);
	}
	catch (const FileException&)
	{
		// LogManager::message("BaseChatFrame::appendLogToChat, Error load = " + path + " Error = " + e.getError());
	}
	const bool l_is_utf = buf.compare(0, 3, "\xef\xbb\xbf", 3) == 0;
	const StringTokenizer<string> l_lines(l_is_utf ? buf.substr(3) : buf, "\r\n");
	size_t i = l_lines.getTokens().size() > (linesCount + 1) ? l_lines.getTokens().size() - linesCount : 0;
	ChatCtrl::CFlyChatCache l_message(ClientManager::getFlylinkDCIdentity(), false, true, Util::emptyStringT, Util::emptyStringT, Colors::g_ChatTextLog, true, false);
	{
		CLockRedraw<true> l_lock_redraw(ctrlClient);
		for (; i < l_lines.getTokens().size(); ++i)
		{
			l_message.m_Msg = Text::toT(l_lines.getTokens()[i] + '\n');
			ctrlClient.AppendText(l_message, false);
		}
	}
}
bool BaseChatFrame::isMultiChat(int& p_h, int & p_chat_columns) const
{
	const bool bUseMultiChat = BOOLSETTING(MULTILINE_CHAT_INPUT) || m_bUseTempMultiChat || /* Fonts::g_fontHeightPixl > 16 || */  m_MultiChatCountLines > 1;
	if (bUseMultiChat && m_MultiChatCountLines)
	{
		p_h = Fonts::g_fontHeightPixl * m_MultiChatCountLines;
	}
	else
	{
		p_h = Fonts::g_fontHeightPixl;
	}
	p_chat_columns = bUseMultiChat ? 2 : 1;
	return bUseMultiChat;
}

OMenu* BaseChatFrame::createUserMenu()
{
	if (!m_userMenu)
	{
		m_userMenu = new OMenu;
		m_userMenu->CreatePopupMenu();
	}
	return m_userMenu;
}

void BaseChatFrame::destroyUserMenu()
{
	safe_delete(m_userMenu);
}
