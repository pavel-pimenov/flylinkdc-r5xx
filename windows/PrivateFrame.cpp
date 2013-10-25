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

#include "stdafx.h"

#include "Resource.h"
#include "PrivateFrame.h"
#include "SearchFrm.h"
#include "WinUtil.h"
#include "MainFrm.h"
#include "TextFrame.h"
#include "ChatBot.h"
#include "UserInfoSimple.h"
#include "../client/ClientManager.h"
#include "../client/LogManager.h"
#include "../client/UploadManager.h"
#include "../client/ShareManager.h"

PrivateFrame::FrameMap PrivateFrame::g_frames;

PrivateFrame::PrivateFrame(const HintedUser& replyTo_, const string& myNick) : replyTo(replyTo_.user),
	m_replyToRealName(replyTo->getLastNickT()),
	m_created(false), m_isoffline(false),
	ctrlClientContainer(WC_EDIT, this, PM_MESSAGE_MAP) // !Decker!
{
	m_ctrlStatusCache.resize(1); // В статусе одна строчка
	ctrlClient.setHubParam(replyTo_.hint, myNick); // [+] IRainman fix.
}

PrivateFrame::~PrivateFrame()
{
}

void PrivateFrame::doDestroyFrame()
{
	destroyMessagePanel(true);
}

// [+] IRainman: copy-past fix.
StringMap PrivateFrame::getFrameLogParams() const
{
	StringMap params;
	params["hubNI"] = Util::toString(ClientManager::getInstance()->getHubNames(replyTo->getCID(), getHubHint()));
	params["hubURL"] = Util::toString(ClientManager::getInstance()->getHubs(replyTo->getCID(), getHubHint()));
	params["userCID"] = replyTo->getCID().toBase32();
	params["userNI"] = Text::fromT(m_replyToRealName);
	params["myCID"] = ClientManager::getMyCID().toBase32();
	return params;
}

void PrivateFrame::addMesageLogParams(StringMap& params, const Identity& from, const tstring& aLine, bool bThirdPerson, const tstring& extra)
{
	params["message"] = ChatMessage::formatNick(from.getNick(), bThirdPerson) + Text::fromT(aLine);
	if (!extra.empty())
		params["extra"] = Text::fromT(extra);
}
// [~] IRainman: copy-past fix.

LRESULT PrivateFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	BaseChatFrame::OnCreate(m_hWnd, rcDefault);
	ctrlClientContainer.SubclassWindow(ctrlClient.m_hWnd);
	PostMessage(WM_SPEAKER, USER_UPDATED);
	m_created = true;
	ClientManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	bHandled = FALSE;
	return 1;
}

void PrivateFrame::gotMessage(const Identity& from, const Identity& to, const Identity& replyTo,
                              const tstring& aMessage, const string& sHubHint, const bool bMyMess,
                              const bool bThirdPerson, const bool notOpenNewWindow /*= false*/)   // !SMT!-S
{
	const auto& id = bMyMess ? to : replyTo;
	const auto& myId = bMyMess ? replyTo : to; // [+] IRainman fix.
	
	const auto i = g_frames.find(id.getUser());
	if (i == g_frames.end())
	{
		if (notOpenNewWindow || g_frames.size() > MAX_PM_FRAMES)
			return; // !SMT!-S
			
		PrivateFrame* p = new PrivateFrame(HintedUser(id.getUser(), sHubHint), myId.getNick());
		g_frames.insert(make_pair(id.getUser(), p));
		p->addLine(from, bMyMess, bThirdPerson, aMessage);
		// [!] TODO! и видимо в ядро!
		if (!bMyMess && Util::getAway())
		{
			if (/*!(BOOLSETTING(NO_AWAYMSG_TO_BOTS) && */ !(replyTo.isBot() || replyTo.isHub())) // [!] IRainman fix.
			{
				// Again, is there better way for this?
				const FavoriteHubEntry *fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(Util::toString(ClientManager::getInstance()->getHubs(id.getUser()->getCID(), sHubHint)));
				StringMap params;
				from.getParams(params, "user", false);
				
				if (fhe)
				{
					if (!fhe->getAwayMsg().empty())
						p->sendMessage(Text::toT(fhe->getAwayMsg()));
					else
						p->sendMessage(Text::toT(Util::getAwayMessage(params)));
				}
				else
				{
					p->sendMessage(Text::toT(Util::getAwayMessage(params)));
				}
			}
		}
		// [~] TODO! и видимо в ядро!
		SHOW_POPUP_EXT(POPUP_NEW_PM, Text::toT(id.getNick() + " - " + sHubHint), PM_PREVIEW, aMessage, 250, TSTRING(PRIVATE_MESSAGE));
		PLAY_SOUND_BEEP(PRIVATE_MESSAGE_BEEP_OPEN);
		ChatBot::getInstance()->onMessage(myId, id, aMessage, true); // !SMT!-CB
	}
	else
	{
		if (!bMyMess)
		{
			SHOW_POPUP_EXT(POPUP_PM, Text::toT(id.getNick() + " - " + sHubHint), PM_PREVIEW, aMessage, 250, TSTRING(PRIVATE_MESSAGE));
			PLAY_SOUND_BEEP(PRIVATE_MESSAGE_BEEP);
			ChatBot::getInstance()->onMessage(myId, id, aMessage, false); // !SMT!-CB
		}
		i->second->addLine(from, bMyMess, bThirdPerson, aMessage);
	}
}

void PrivateFrame::openWindow(const OnlineUserPtr& ou, const HintedUser& replyTo, string myNick, const tstring& msg)
{
	// [+] IRainman fix.
	if (myNick.empty())
	{
		if (ou)
		{
			myNick = ou->getClient().getMyNick();
		}
		else if (!replyTo.hint.empty())
		{
			auto client = ClientManager::getInstance()->findClient(replyTo.hint);
			myNick = client ? client->getMyNick() : SETTING(NICK);
		}
		else
		{
			myNick = SETTING(NICK);
		}
	}
	// [~] IRainman fix.
	
	PrivateFrame* p = nullptr;
	const auto i = g_frames.find(replyTo);
	if (i == g_frames.end())
	{
		if (g_frames.size() > MAX_PM_FRAMES)
			return;
			
		// [+] IRainman fix.
		if (ou)
		{
			replyTo.user->setLastNick(ou->getIdentity().getNick());
		}
		// [~] IRainman fix.
		p = new PrivateFrame(replyTo, myNick);
		g_frames.insert(make_pair(replyTo, p));
		p->CreateEx(WinUtil::mdiClient);
	}
	else
	{
		p = i->second;
		if (::IsIconic(p->m_hWnd))
			::ShowWindow(p->m_hWnd, SW_RESTORE);
			
		p->MDIActivate(p->m_hWnd);
	}
	if (!msg.empty())
		p->sendMessage(msg);
}

LRESULT PrivateFrame::onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!processingServices(uMsg, wParam, lParam, bHandled))
	{
		processingHotKeys(uMsg, wParam, lParam, bHandled);
	}
	return 0;
}
// !Decker!
LRESULT PrivateFrame::onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	HWND focus = GetFocus();
	bHandled = false;
	if (focus == ctrlClient.m_hWnd)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		tstring x;
		
		int i = ctrlClient.CharFromPos(pt);
		int line = ctrlClient.LineFromChar(i);
		int c = LOWORD(i) - ctrlClient.LineIndex(line);
		int len = ctrlClient.LineLength(i) + 1;
		if (len < 3)
		{
			return 0;
		}
		
		x.resize(len);
		ctrlClient.GetLine(line, &x[0], len);
		
		string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);
		
		if (start == string::npos)
			start = 0;
		else
			start++;
			
		string::size_type end = x.find_first_of(_T(" >\t"), start + 1);
		
		if (end == string::npos) // get EOL as well
			end = x.length();
		else if (end == start + 1)
			return 0;
			
		bHandled = true;
		// Nickname click, let's see if we can find one like it in the name list...
		// ник в чат
		appendNickToChat(x.substr(start, end - start));
	}
	return 0;
}

void PrivateFrame::processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText)
{
	if (replyTo->isOnline())
	{
		sendMessage(fullMessageText);
	}
	else
	{
		setStatusText(0, CTSTRING(USER_WENT_OFFLINE));
		resetInputMessageText = false;
	}
}

void PrivateFrame::processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText)
{
	if (stricmp(cmd.c_str(), _T("grant")) == 0)
	{
		UploadManager::getInstance()->reserveSlot(HintedUser(getUser(), getHubHint()), 600);
		addStatus(TSTRING(SLOT_GRANTED));
	}
	else if (stricmp(cmd.c_str(), _T("close")) == 0) // TODO
	{
		PostMessage(WM_CLOSE);
	}
	else if ((stricmp(cmd.c_str(), _T("favorite")) == 0) || (stricmp(cmd.c_str(), _T("fav")) == 0))
	{
		FavoriteManager::getInstance()->addFavoriteUser(getUser());
		addStatus(TSTRING(FAVORITE_USER_ADDED));
	}
	else if ((stricmp(cmd.c_str(), _T("getlist")) == 0) || (stricmp(cmd.c_str(), _T("gl")) == 0))
	{
		BOOL bTmp;
		clearUserMenu();
		reinitUserMenu(replyTo, getHubHint());
		onGetList(0, 0, 0, bTmp);
	}
	else if (stricmp(cmd.c_str(), _T("log")) == 0)
	{
		openFrameLog();
	}
}

void PrivateFrame::sendMessage(const tstring& msg, bool thirdperson /*= false*/)
{
	ClientManager::getInstance()->privateMessage(HintedUser(replyTo, getHubHint()), Text::fromT(msg), thirdperson);
}

LRESULT PrivateFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		ClientManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		g_frames.erase(replyTo);
		
		bHandled = FALSE;
		return 0;
	}
}
void PrivateFrame::readFrameLog()
{
	const auto linesCount = SETTING(SHOW_LAST_LINES_LOG);
	if (linesCount)
	{
		const string path = Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatParams(SETTING(LOG_FILE_PRIVATE_CHAT), getFrameLogParams(), false));
		appendLogToChat(path, linesCount);
	}
}
void PrivateFrame::addLine(const Identity& from, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
{
	if (!m_created)
	{
		if (BOOLSETTING(POPUNDER_PM))
			WinUtil::hiddenCreateEx(this);
		else
			CreateEx(WinUtil::mdiClient);
	}
	
	tstring extra;
	BaseChatFrame::addLine(from, bMyMess, bThirdPerson, aLine, cf, extra);
	
	if (BOOLSETTING(LOG_PRIVATE_CHAT))
	{
		StringMap params = getFrameLogParams();
		addMesageLogParams(params, from, aLine, bThirdPerson, extra);
		LOG(PM, params);
	}
	
	addStatus(TSTRING(LAST_CHANGE), false, false);
	
	if (BOOLSETTING(BOLD_PM))
	{
		setDirty();
	}
}

LRESULT PrivateFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	
	OMenu tabMenu;
	tabMenu.CreatePopupMenu();
	clearUserMenu();
	
	//#ifdef OLD_MENU_HEADER //[~]JhaoDa
	tabMenu.InsertSeparatorFirst(m_replyToRealName);
	//#endif
	reinitUserMenu(replyTo, getHubHint()); // [!] IRainman fix.
	appendAndActivateUserItems(tabMenu); // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=621
	appendUcMenu(tabMenu, UserCommand::CONTEXT_USER, ClientManager::getInstance()->getHubs(replyTo->getCID(), getHubHint()));
	if (!(tabMenu.GetMenuState(tabMenu.GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
	{
		tabMenu.AppendMenu(MF_SEPARATOR);
	}
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_PM, CTSTRING(MENU_CLOSE_ALL_OFFLINE_PM)); // [+] InfinitySky. Закрыть все оффлайн лички.
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_PM, CTSTRING(MENU_CLOSE_ALL_PM)); // [+] InfinitySky. Закрыть все лички.
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
	
	tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
	
	cleanUcMenu(tabMenu);
	WinUtil::unlinkStaticMenus(tabMenu); // TODO - fix copy-paste
	return TRUE;
}

void PrivateFrame::runUserCommand(UserCommand& uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
	StringMap ucParams = ucLineParams;
	ClientManager::getInstance()->userCommand(HintedUser(replyTo, getHubHint()), uc, ucParams, true);
	// TODO тут ucParams не используется позже
}

void PrivateFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (g_isStartupProcess)
		return;
	dcassert(!ClientManager::isShutdown());
	if (ClientManager::isShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	if (m_ctrlStatus && m_ctrlLastLinesToolTip)
	{
		if (m_ctrlStatus->IsWindow() && m_ctrlLastLinesToolTip->IsWindow())
		{
			CRect sr;
			int w[1]; // Прикольный массив :)
			m_ctrlStatus->GetClientRect(sr);
			
			w[0] = sr.right - 16;
			
			m_ctrlStatus->SetParts(1, w);
			
			m_ctrlLastLinesToolTip->SetMaxTipWidth(max(w[0], 400));
		}
	}
	
	/*[+] Это условие для тех, кто любит большие шрифты,
	будет включаться многострочный ввод принудительно,
	чтоб не портить расположение элементов Sergey Shushkanov */
	const bool bUseMultiChat = BOOLSETTING(MULTILINE_CHAT_INPUT) || m_bUseTempMultiChat
#ifdef MULTILINE_CHAT_IF_BIG_FONT_SET
	                           || Fonts::fontHeight > FONT_SIZE_FOR_AUTO_MULTILINE_CHAT //[+] TEST VERSION Sergey Shushkanov
#endif
	                           ;
	                           
	const int h = bUseMultiChat ? 20 : 14;//[+] TEST VERSION Sergey Shushkanov
	const int chat_columns = bUseMultiChat ? 2 : 1; // !Decker! // [~] Sergey Shushkanov
	
	CRect rc = rect;
	rc.bottom -= h * chat_columns + 15; // !Decker! //[~] Sergey Shushkanov
	
	ctrlClient.MoveWindow(rc);
	
	const int iButtonPanelLength = MessagePanel::GetPanelWidth();
	
	rc = rect;
	rc.bottom -= 4; //[~] Sergey Shushkanov
	rc.top = rc.bottom - h * chat_columns - 7; // !Decker!
	rc.left += 2; //[~] Sergey Shushkanov
	rc.right -= iButtonPanelLength + 2; //[~] Sergey Shushkanov
	CRect ctrlMessageRect = rc;
	if (m_ctrlMessage)
		m_ctrlMessage->MoveWindow(rc);
		
	if (bUseMultiChat) //[+] TEST VERSION Sergey Shushkanov
	{
		rc.top += h + 6; //[+] TEST VERSION Sergey Shushkanov
	}//[+] TEST VERSION Sergey Shushkanov
	//rc.top += h * (chat_columns - 1); // !Decker! // [-] Sergey Shushkanov
	rc.left = rc.right; // [~] Sergey Shushkanov
	rc.bottom -= 1; // [+] Sergey Shushkanov
	
	if (m_msgPanel)
		m_msgPanel->UpdatePanel(rc);
		
	/*rc = rect;
	rc.bottom -= 1;
	rc.top = rc.bottom - h * chat_columns - 7; // !Decker!
	rc.left += 1;
	rc.right -= iButtonPanelLength;
	CRect ctrlMessageRect = rc;
	ctrlMessage.MoveWindow(rc);
	
	rc.top += h * (chat_columns - 1) + 4; // !Decker!
	rc.left = rc.right + 1;
	
	m_msgPanel.UpdatePanel(rc); [-] DONT DELETE!!! Sergey Shushkanov */
}
/* [-] IRainman fix.
string PrivateFrame::getHubHint() const
{
    return ctrlClient.getClient() ? ctrlClient.getClient()->getHubUrl() : Util::emptyString;
}
*/
void PrivateFrame::updateTitle()
{
	//[+]FlylinkDC++ Team
	if (m_closed)
		return;
	if (!replyTo)
		return;
	//[~]FlylinkDC++ Team
	pair<tstring, bool> hubs = WinUtil::getHubNames(replyTo, getHubHint());
	
	bool banIcon = false;
	Flags::MaskType l_flags;
	int l_ul;
	if (FavoriteManager::getInstance()->getFavUserParam(replyTo, l_flags, l_ul))
	{
		banIcon = FavoriteManager::hasUploadBan(l_ul) || FavoriteManager::hasIgnorePM(l_flags); // !SMT!-UI
	}
	
	if (hubs.second)
	{
		if (banIcon) // !SMT!-UI
			setCustomIcon(WinUtil::g_banIconOnline);
		else
			unsetIconState();
			
		setTabColor(RGB(0, 255, 255));
		
		// [!] IRainman fix: when the user first came to the network
		// with the opening of the window private message - update the name,
		// if when you open the window it was already known the real name - use it.
		if (m_replyToRealName.empty())
		{
			m_replyToRealName = replyTo->getLastNickT();
		}
		if (m_isoffline)
		{
			addStatus(TSTRING(USER_WENT_ONLINE) + _T(" [") + m_replyToRealName + _T(" - ") + hubs.first + _T("]"));
		}
		
		m_isoffline = false;
	}
	else
	{
		// [+] IRainman fix
		m_isoffline = true;
//[-]PPA        ctrlClient.setClient(NULL);
		// [~] IRainman fix
		if (banIcon) // !SMT!-UI
		{
			setCustomIcon(WinUtil::g_banIconOffline);
		}
		else
		{
			setIconState();
		}
		
		setTabColor(RGB(255, 0, 0));
		
		addStatus(TSTRING(USER_WENT_OFFLINE) + _T(" [") + m_replyToRealName + _T(" - ") + Text::toT(getHubHint()) + _T("]")); // [!] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=491
		// [-] IRainman fix
		//m_isoffline = true;
		//ctrlClient.setClient(NULL);
		// [~] IRainman fix
	}
	SetWindowText((m_replyToRealName + _T(" - ") + (hubs.second ? hubs.first : Text::toT(getHubHint()))).c_str()); // [!] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=491
}

LRESULT PrivateFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	
	POINT p;
	p.x = GET_X_LPARAM(lParam);
	p.y = GET_Y_LPARAM(lParam);
	::ScreenToClient(ctrlClient.m_hWnd, &p);
	
	POINT cpt;
	GetCursorPos(&cpt);
	
	CRect rc;            // client area of window
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	OMenu Mnu;
	
	if (m_msgPanel && m_msgPanel->OnContextMenu(pt, wParam))
		return TRUE;
		
	if (reinterpret_cast<HWND>(wParam) == ctrlClient)
	{
		ctrlClient.OnRButtonDown(pt, replyTo);
		const int i = ctrlClient.CharFromPos(p);
		const int line = ctrlClient.LineFromChar(i);
		const int c = LOWORD(i) - ctrlClient.LineIndex(line);
		const int len = ctrlClient.LineLength(i) + 1;
		if (len < 3)
			return 0;
			
		tstring x;
		x.resize(len);
		ctrlClient.GetLine(line, &x[0], len);
		
		string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);
		if (start == string::npos)
		{
			start = 0;
		}
		if (x.substr(start, (m_replyToRealName.length() + 2)) == (_T('<') + m_replyToRealName + _T('>')))
		{
			if (!replyTo->isOnline())
			{
				return S_OK;
			}
			OMenu menu;
			menu.CreatePopupMenu();
			clearUserMenu();
			
			reinitUserMenu(replyTo, getHubHint()); // [!] IRainman fix.
			
			appendUcMenu(menu, UserCommand::CONTEXT_USER, ClientManager::getInstance()->getHubs(replyTo->getCID(), getHubHint()));
			if (!(menu.GetMenuState(menu.GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
			{
				menu.AppendMenu(MF_SEPARATOR);
			}
			//#ifdef OLD_MENU_HEADER //[~]JhaoDa
			menu.InsertSeparatorFirst(m_replyToRealName);
			//#endif
			appendAndActivateUserItems(menu);
			
			menu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
			menu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, cpt.x, cpt.y, m_hWnd);
			
			cleanUcMenu(menu);
			WinUtil::unlinkStaticMenus(menu); // TODO - fix copy-paste
			bHandled = TRUE;
		}
		else
		{
			OMenu textMenu;
			textMenu.CreatePopupMenu();
			appendChatCtrlItems(textMenu);
			textMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, cpt.x, cpt.y, m_hWnd);
			bHandled = TRUE;
		}
	}
	return S_OK;
}

void PrivateFrame::closeAll()
{
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == g_frames.size());
}

void PrivateFrame::closeAllOffline()
{
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		if (!i->first->isOnline())
			i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == g_frames.size());
}

void PrivateFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
{
	ctrlClient.SetBackgroundColor(Colors::bgColor);
	UpdateLayout();
	RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

// !SMT!-S
bool PrivateFrame::closeUser(const UserPtr& u)
{
	const auto i = g_frames.find(u);
	if (i == g_frames.end())
	{
		return false;
	}
	i->second->PostMessage(WM_CLOSE, 0, 0);
	return true;
}

void PrivateFrame::onBeforeActiveTab(HWND aWnd)
{
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		i->second->destroyMessagePanel(false);
	}
	dcassert(l_size_g_frames == g_frames.size());
}
void PrivateFrame::onAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isShutdown())
	{
		dcassert(!ClientManager::isShutdown());
		createMessagePanel();
		if (m_ctrlStatus)
		{
			UpdateLayout();
		}
	}
}
void PrivateFrame::onInvalidateAfterActiveTab(HWND aWnd)
{
}
void PrivateFrame::createMessagePanel()
{
	dcassert(!ClientManager::isShutdown());
	if (m_ctrlStatus == nullptr && g_isStartupProcess == false)
	{
		BaseChatFrame::createMessageCtrl(this, PM_MESSAGE_MAP);
		CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
		BaseChatFrame::createStatusCtrl(m_hWndStatusBar);
		restoreStatusFromCache(); // Восстанавливать статус нужно после UpdateLayout
		m_ctrlMessage->SetFocus();
	}
	BaseChatFrame::createMessagePanel();
}
void PrivateFrame::destroyMessagePanel(bool p_is_destroy)
{
	const bool l_is_shutdown = p_is_destroy || ClientManager::isShutdown();
	BaseChatFrame::destroyStatusbar(l_is_shutdown);
	BaseChatFrame::destroyMessagePanel(l_is_shutdown);
	BaseChatFrame::destroyMessageCtrl(l_is_shutdown);
}

/**
 * @file
 * $Id: PrivateFrame.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
