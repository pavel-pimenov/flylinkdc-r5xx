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

#include "stdafx.h"

#include "Resource.h"
#include "PrivateFrame.h"
#include "SearchFrm.h"
#include "MainFrm.h"
#include "../client/LogManager.h"
#include "../client/UploadManager.h"

PrivateFrame::FrameMap PrivateFrame::g_pm_frames;
std::unordered_map<string, unsigned> PrivateFrame::g_count_pm;

PrivateFrame::PrivateFrame(const HintedUser& replyTo_, const string& myNick) : m_replyTo(replyTo_),
	m_replyToRealName(m_replyTo.user->getLastNickT()),
	m_created(false), m_isoffline(false),
	m_ctrlChatContainer(WC_EDIT, this, PM_MESSAGE_MAP) // !Decker!
{
	m_ctrlStatusCache.resize(1);
	ctrlClient.setHubParam(replyTo_.hint, myNick);
}

PrivateFrame::~PrivateFrame()
{
}

void PrivateFrame::doDestroyFrame()
{
	destroyUserMenu();
	destroyMessagePanel(true);
}

StringMap PrivateFrame::getFrameLogParams() const
{
	StringMap params;
	params["hubNI"] = Util::toString(ClientManager::getHubNames(m_replyTo.user->getCID(), getHubHint()));
	params["hubURL"] = Util::toString(ClientManager::getHubs(m_replyTo.user->getCID(), getHubHint()));
	params["userCID"] = m_replyTo.user->getCID().toBase32();
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

LRESULT PrivateFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	BaseChatFrame::OnCreate(m_hWnd, rcDefault);
	PostMessage(WM_SPEAKER, PM_USER_UPDATED);
	m_created = true;
	ClientManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	bHandled = FALSE;
	return 1;
}

bool PrivateFrame::gotMessage(const Identity& from, const Identity& to, const Identity& replyTo,
                              const tstring& aMessage, unsigned p_max_smiles, const string& p_HubHint, const bool bMyMess,
                              const bool bThirdPerson, const bool notOpenNewWindow /*= false*/)
{
	const auto& id = bMyMess ? to : replyTo;
	const auto& myId = bMyMess ? replyTo : to;
	
	const string l_key = id.getUser()->getLastNick() + " + " + p_HubHint;
	string l_message = Text::fromT(aMessage);
	const bool l_is_spam = CFlyServerConfig::isSpam(l_message);
	const auto i = g_pm_frames.find(id.getUser());
	if (i == g_pm_frames.end())
	{
		Text::removeString_rn(l_message);
		if (l_is_spam)
		{
			CFlyServerJSON::pushError(47, "Ignore spam: [ " + l_message + " ] [user+hub = " + l_key + "]");
			return true; // ���� ��� ��
		}
		if (notOpenNewWindow || g_pm_frames.size() > MAX_PM_FRAMES)
		{
			LogManager::message("Lock > 100 open private message windows! Hub: " + l_key + " Message: " + l_message);
			return false;
		}
		auto& l_count_pm = g_count_pm[l_key];
		if (l_count_pm > 10)
		{
			LogManager::message("Lock > 10 private message for Hub: " + l_key + " Message: " + l_message);
			return false;
		}
		++l_count_pm;
		// TODO - Add antispam!
		PrivateFrame* p = new PrivateFrame(HintedUser(id.getUser(), p_HubHint), myId.getNick());
		g_pm_frames.insert(make_pair(id.getUser(), p));
		p->addLine(from, bMyMess, bThirdPerson, aMessage, p_max_smiles);
		// [!] TODO! � ������ � ����!
		if (!bMyMess && Util::getAway())
		{
			if (/*!(BOOLSETTING(NO_AWAYMSG_TO_BOTS) && */ !replyTo.isBotOrHub())
			{
				// Again, is there better way for this?
				const FavoriteHubEntry *fhe = FavoriteManager::getFavoriteHubEntry(Util::toString(ClientManager::getHubs(id.getUser()->getCID(), p_HubHint)));
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
		
		SHOW_POPUP_EXT(POPUP_NEW_PM, Text::toT(id.getNick() + " - " + p_HubHint), PM_PREVIEW, aMessage, 250, TSTRING(PRIVATE_MESSAGE));
		PLAY_SOUND_BEEP(PRIVATE_MESSAGE_BEEP_OPEN);
	}
	else
	{
		if (l_is_spam)
		{
			CFlyServerJSON::pushError(48, "Detect private spam: [ " + l_message + " ] [user+hub = " + l_key + "]");
		}
		else
		{
			if (!bMyMess)
			{
				SHOW_POPUP_EXT(POPUP_PM, Text::toT(id.getNick() + " - " + p_HubHint), PM_PREVIEW, aMessage, 250, TSTRING(PRIVATE_MESSAGE));
				PLAY_SOUND_BEEP(PRIVATE_MESSAGE_BEEP);
			}
		}
		// Add block spam???
		i->second->addLine(from, bMyMess, bThirdPerson, aMessage, p_max_smiles);
	}
	return true;
}

void PrivateFrame::openWindow(const OnlineUserPtr& ou, const HintedUser& replyTo, string myNick, const tstring& msg)
{

	if (myNick.empty())
	{
		if (ou)
		{
			myNick = ou->getClient().getMyNick();
		}
		else if (!replyTo.hint.empty())
		{
			const auto l_client = ClientManager::findClient(replyTo.hint);
			myNick = l_client ? l_client->getMyNick() : SETTING(NICK);
		}
		else
		{
			myNick = SETTING(NICK);
		}
	}
	
	
	PrivateFrame* p = nullptr;
	const auto i = g_pm_frames.find(replyTo);
	if (i == g_pm_frames.end())
	{
		if (g_pm_frames.size() > MAX_PM_FRAMES)
		{
			LogManager::message("Lock > 100 open private message windows! Hub+nick: " + replyTo.hint + " " + myNick + " Message: " + Text::fromT(msg));
			return;
		}
		
		
		if (ou)
		{
			replyTo.user->setLastNick(ou->getIdentity().getNick());
		}
		
		p = new PrivateFrame(replyTo, myNick);
		g_pm_frames.insert(std::make_pair(replyTo, p));
		g_count_pm[replyTo.user->getLastNick() + "~" + replyTo.hint]++;
		p->CreateEx(WinUtil::g_mdiClient);
	}
	else
	{
		p = i->second;
		if (::IsIconic(p->m_hWnd))
			::ShowWindow(p->m_hWnd, SW_RESTORE);
			
		p->MDIActivate(p->m_hWnd);
	}
	if (!msg.empty())
	{
		p->sendMessage(msg);
	}
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
		// ��� � ���
		appendNickToChat(x.substr(start, end - start));
	}
	return 0;
}

void PrivateFrame::processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText)
{
	if (m_replyTo.user->isOnline())
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
	else if (stricmp(cmd.c_str(), _T("favorite")) == 0 || stricmp(cmd.c_str(), _T("fav")) == 0)
	{
		FavoriteManager::getInstance()->addFavoriteUser(getUser());
		addStatus(TSTRING(FAVORITE_USER_ADDED));
	}
	else if (stricmp(cmd.c_str(), _T("getlist")) == 0 || stricmp(cmd.c_str(), _T("gl")) == 0)
	{
		BOOL bTmp;
		clearUserMenu();
		reinitUserMenu(m_replyTo, getHubHint());
		onGetList(0, 0, 0, bTmp);
	}
	else if (stricmp(cmd.c_str(), _T("log")) == 0)
	{
		openFrameLog();
	}
}

void PrivateFrame::sendMessage(const tstring& msg, bool thirdperson /*= false*/)
{
	ClientManager::privateMessage(HintedUser(m_replyTo, getHubHint()), Text::fromT(msg), thirdperson);
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
		g_count_pm[m_replyTo.user->getLastNick() + "~" + m_replyTo.hint]--;
		g_pm_frames.erase(m_replyTo);
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
void PrivateFrame::addLine(const Identity& from, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, unsigned p_max_smiles, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
{
	if (!m_created)
	{
		if (BOOLSETTING(POPUNDER_PM))
			WinUtil::hiddenCreateEx(this);
		else
			CreateEx(WinUtil::g_mdiClient);
	}
	
	tstring extra;
	BaseChatFrame::addLine(from, bMyMess, bThirdPerson, aLine, p_max_smiles, cf, extra);
	
	if (BOOLSETTING(LOG_PRIVATE_CHAT))
	{
		StringMap params = getFrameLogParams();
		addMesageLogParams(params, from, aLine, bThirdPerson, extra);
		LOG(PM, params);
	}
	
	addStatus(TSTRING(LAST_CHANGE), false, false);
	
	if (BOOLSETTING(BOLD_PM))
	{
		setDirty(1);
	}
}

LRESULT PrivateFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	
	OMenu tabMenu;
	tabMenu.CreatePopupMenu();
	clearUserMenu();
	
	tabMenu.InsertSeparatorFirst(m_replyToRealName);
	reinitUserMenu(m_replyTo, getHubHint());
	appendAndActivateUserItems(tabMenu);
	appendUcMenu(tabMenu, UserCommand::CONTEXT_USER, ClientManager::getHubs(m_replyTo.user->getCID(), getHubHint()));
	if (!(tabMenu.GetMenuState(tabMenu.GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
	{
		tabMenu.AppendMenu(MF_SEPARATOR);
	}
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_OFFLINE_PM, CTSTRING(MENU_CLOSE_ALL_OFFLINE_PM));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_PM, CTSTRING(MENU_CLOSE_ALL_PM));
	tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
	
	tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
	
	cleanUcMenu(tabMenu);
	WinUtil::unlinkStaticMenus(tabMenu);
	return TRUE;
}

void PrivateFrame::runUserCommand(UserCommand& uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
	StringMap ucParams = ucLineParams;
	ClientManager::userCommand(HintedUser(m_replyTo, getHubHint()), uc, ucParams, true);
	// TODO ��� ucParams �� ������������ �����
}

void PrivateFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	if (ClientManager::isStartup())
		return;
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
		return;
	if (m_ctrlMessage)
	{
		RECT rect;
		GetClientRect(&rect);
		// position bars and offset their dimensions
		UpdateBarsPosition(rect, bResizeBars);
		if (m_ctrlStatus && m_ctrlLastLinesToolTip)
		{
			if (m_ctrlStatus->IsWindow() && m_ctrlLastLinesToolTip->IsWindow())
			{
				CRect sr;
				int w[1]; // ���������� ������ :)
				m_ctrlStatus->GetClientRect(sr);
				
				w[0] = sr.right - 16;
				
				m_ctrlStatus->SetParts(1, w);
				
				m_ctrlLastLinesToolTip->SetMaxTipWidth(max(w[0], 400));
			}
		}
		int h = 0, chat_columns = 0;
		const bool bUseMultiChat = isMultiChat(h, chat_columns);
		CRect rc = rect;
		//rc.bottom -= h + 15;
		rc.bottom -= h + (Fonts::g_fontHeightPixl + 1) * int(bUseMultiChat) + 18;
		
		if (ctrlClient.IsWindow())
		{
			ctrlClient.MoveWindow(rc);
		}
		
		const int iButtonPanelLength = MessagePanel::GetPanelWidth();
		
		rc = rect;
		rc.bottom -= 4;
		rc.top = rc.bottom - h - Fonts::g_fontHeightPixl * int(bUseMultiChat) - 12;
		rc.left += 2;
		rc.right -= iButtonPanelLength + 2;
		if (m_ctrlMessage)
		{
			m_ctrlMessage->MoveWindow(rc);
		}
		
		if (bUseMultiChat)
		{
			rc.top += h + 6;
		}
		rc.left = rc.right;
		rc.bottom -= 1;
		
		if (m_msgPanel)
		{
			m_msgPanel->UpdatePanel(rc);
		}
	}
}
void PrivateFrame::updateTitle()
{
	dcassert(!isClosedOrShutdown());
	if (isClosedOrShutdown())
		return;
	if (!m_replyTo.user)
		return;
	pair<tstring, bool> hubs = WinUtil::getHubNames(m_replyTo, getHubHint());
	
	bool banIcon = false;
	Flags::MaskType l_flags;
	int l_ul;
	if (FavoriteManager::getFavUserParam(m_replyTo, l_flags, l_ul))
	{
		banIcon = FavoriteManager::hasUploadBan(l_ul) || FavoriteManager::hasIgnorePM(l_flags); // TODO - ���������� �� ��������� ������
	}
	
	if (hubs.second)
	{
		if (banIcon)
			setCustomIcon(WinUtil::g_banIconOnline);
		else
			unsetIconState();
			
		setTabColor(RGB(0, 255, 255));
		
		if (m_replyToRealName.empty())
		{
			m_replyToRealName = m_replyTo.user->getLastNickT();
		}
		if (m_isoffline)
		{
			addStatus(TSTRING(USER_WENT_ONLINE) + _T(" [") + m_replyToRealName + _T(" - ") + hubs.first + _T("]"));
		}
		m_isoffline = false;
	}
	else
	{
		m_isoffline = true;
		if (banIcon)
		{
			setCustomIcon(WinUtil::g_banIconOffline);
		}
		else
		{
			setIconState();
		}
		
		setTabColor(RGB(255, 0, 0));
		
		addStatus(TSTRING(USER_WENT_OFFLINE) + _T(" [") + m_replyToRealName + _T(" - ") + Text::toT(getHubHint()) + _T("]"));
	}
	SetWindowText((m_replyToRealName + _T(" - ") + (hubs.second ? hubs.first : Text::toT(getHubHint()))).c_str());
}

LRESULT PrivateFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{

	bHandled = FALSE;
	POINT cpt;
	GetCursorPos(&cpt);
	
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	
	
	if (m_msgPanel && m_msgPanel->OnContextMenu(pt, wParam))
		return TRUE;
		
	if (reinterpret_cast<HWND>(wParam) == ctrlClient && ctrlClient.IsWindow())
	{
		if (pt.x == -1 && pt.y == -1)
		{
			CRect erc;
			ctrlClient.GetRect(&erc);
			pt.x = erc.Width() / 2;
			pt.y = erc.Height() / 2;
			//  ctrlClient.ClientToScreen(&pt);
		}
		//  POINT ptCl = pt;
		ctrlClient.ScreenToClient(&pt);
		ctrlClient.OnRButtonDown(pt);
		
		//  ctrlClient.OnRButtonDown(p, m_replyTo);
		const int i = ctrlClient.CharFromPos(pt);
		const int line = ctrlClient.LineFromChar(i);
		const int c = LOWORD(i) - ctrlClient.LineIndex(line);
		const int len = ctrlClient.LineLength(i) + 1;
		if (len < 2)
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
			if (!m_replyTo.user->isOnline())
			{
				return S_OK;
			}
			OMenu* l_user_menu = createUserMenu();
			l_user_menu->ClearMenu();
			clearUserMenu();
			
			reinitUserMenu(m_replyTo, getHubHint());
			
			appendUcMenu(*l_user_menu, UserCommand::CONTEXT_USER, ClientManager::getHubs(m_replyTo.user->getCID(), getHubHint()));
			if (!(l_user_menu->GetMenuState(l_user_menu->GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
			{
				l_user_menu->AppendMenu(MF_SEPARATOR);
			}
			l_user_menu->InsertSeparatorFirst(m_replyToRealName);
			appendAndActivateUserItems(*l_user_menu);
			
			l_user_menu->AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
			l_user_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, cpt.x, cpt.y, m_hWnd);
			
			WinUtil::unlinkStaticMenus(*l_user_menu); // TODO - fix copy-paste
			cleanUcMenu(*l_user_menu);
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
	dcdrun(const auto l_size_g_frames = g_pm_frames.size());
	for (auto i = g_pm_frames.cbegin(); i != g_pm_frames.cend(); ++i)
	{
		i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == g_pm_frames.size());
}

void PrivateFrame::closeAllOffline()
{
	dcdrun(const auto l_size_g_frames = g_pm_frames.size());
	for (auto i = g_pm_frames.cbegin(); i != g_pm_frames.cend(); ++i)
	{
		if (!i->first->isOnline())
			i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == g_pm_frames.size());
}

void PrivateFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlClient.IsWindow())
		{
			ctrlClient.SetBackgroundColor(Colors::g_bgColor);
		}
		UpdateLayout();
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}


bool PrivateFrame::closeUser(const UserPtr& u)
{
	const auto i = g_pm_frames.find(u);
	if (i == g_pm_frames.end())
	{
		return false;
	}
	i->second->PostMessage(WM_CLOSE, 0, 0);
	return true;
}

void PrivateFrame::onBeforeActiveTab(HWND aWnd)
{
	dcdrun(const auto l_size_g_frames = g_pm_frames.size());
	for (auto i = g_pm_frames.cbegin(); i != g_pm_frames.cend(); ++i)
	{
		i->second->destroyMessagePanel(false);
	}
	dcassert(l_size_g_frames == g_pm_frames.size());
}
void PrivateFrame::onAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isBeforeShutdown())
	{
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
	dcassert(!ClientManager::isBeforeShutdown());
	if (!isClosedOrShutdown())
	{
		if (m_ctrlStatus == nullptr && ClientManager::isStartup() == false)
		{
			BaseChatFrame::createMessageCtrl(this, PM_MESSAGE_MAP, false); // TODO - ��������� hub
			if (!m_ctrlChatContainer.IsWindow())
				m_ctrlChatContainer.SubclassWindow(ctrlClient.m_hWnd);
			CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
			BaseChatFrame::createStatusCtrl(m_hWndStatusBar);
			restoreStatusFromCache(); // ��������������� ������ ����� ����� UpdateLayout
			m_ctrlMessage->SetFocus();
		}
		BaseChatFrame::createMessagePanel();
		setCountMessages(0);
	}
}
void PrivateFrame::destroyMessagePanel(bool p_is_destroy)
{
	const bool l_is_shutdown = p_is_destroy || ClientManager::isBeforeShutdown();
	BaseChatFrame::destroyStatusbar(l_is_shutdown);
	BaseChatFrame::destroyMessagePanel(l_is_shutdown);
	BaseChatFrame::destroyMessageCtrl(l_is_shutdown);
}

/**
 * @file
 * $Id: PrivateFrame.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
