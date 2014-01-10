/*
* Copyright (C) 2003-2005 Pär Björklund, per.bjorklund@gmail.com
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

#include "WinUtil.h"
#include "MainFrm.h"


//[!]PopupManager* Singleton< PopupManager >::instance = nullptr;

void PopupManager::Show(const tstring &aMsg, const tstring &aTitle, int Icon, bool preview /*= false*/)
{
	dcassert(BaseChatFrame::g_isStartupProcess == false);
	if (BaseChatFrame::g_isStartupProcess == true)
		return;
		
	if (!activated)
		return;
		
	if (!Util::getAway() && BOOLSETTING(POPUP_AWAY) && !preview)
		return;
		
	if (!MainFrame::isAppMinimized() && BOOLSETTING(POPUP_MINIMIZED) && !preview)
	{
		return;
	}
	
	tstring msg = aMsg;
	if (int(aMsg.length()) > SETTING(MAX_MSG_LENGTH))
	{
		msg = aMsg.substr(0, (SETTING(MAX_MSG_LENGTH) - 3));
		msg += _T("...");
	}
	
	if (SETTING(POPUP_TYPE) == BALLOON)
	{
		NOTIFYICONDATA m_nid = {0};
		m_nid.cbSize = sizeof(NOTIFYICONDATA);
		m_nid.hWnd = MainFrame::getMainFrame()->m_hWnd;
		m_nid.uID = 0;
		m_nid.uFlags = NIF_INFO;
		m_nid.uTimeout = (SETTING(POPUP_TIME) * 1000);
		m_nid.dwInfoFlags = Icon;
		_tcsncpy(m_nid.szInfo, msg.c_str(), 255);
		_tcsncpy(m_nid.szInfoTitle, aTitle.c_str(), 63);
		Shell_NotifyIcon(NIM_MODIFY, &m_nid);
		return;
	}
	
//	if (PopupImage != SETTING(POPUPFILE) || popuptype != SETTING(POPUP_TYPE))
//	{
	if (SETTING(POPUP_TYPE) == CUSTOM && PopupImage != SETTING(POPUPFILE))
	{
		PopupImage = SETTING(POPUPFILE);
		popuptype = SETTING(POPUP_TYPE);
		m_hBitmap = (HBITMAP)::LoadImage(NULL, (Text::toT(PopupImage).c_str()), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	}
//		if (m_hBitmap && SETTING(POPUP_TYPE) == CUSTOM)
//		{
//			BITMAP bm = {0};
//			GetObject(m_hBitmap, sizeof(bm), &bm);
//			height = (uint16_t)bm.bmHeight;
//			width = (uint16_t)bm.bmWidth;
//		}
//		else if (SETTING(POPUP_TYPE) != CUSTOM)
//		{
//			height = 90;
//			width = 200;
//		}
//	}

	height = SETTING(POPUP_H);
	width = SETTING(POPUP_W);
	
	CRect rcDesktop;
	
	//get desktop rect so we know where to place the popup
	::SystemParametersInfo(SPI_GETWORKAREA, 0, &rcDesktop, 0);
	
	int screenHeight = rcDesktop.bottom;
	int screenWidth = rcDesktop.right;
	
	//if we have popups all the way up to the top of the screen do not create a new one
	if ((offset + height) > screenHeight)
		return;
		
	//get the handle of the window that has focus
	HWND gotFocus = ::SetFocus(WinUtil::mainWnd);
	
	//compute the window position
	CRect rc(screenWidth - width , screenHeight - height - offset, screenWidth, screenHeight - offset);
	
	//Create a new popup
	PopupWnd *p = new PopupWnd(msg, aTitle, rc, id++, m_hBitmap);
	p->height = height; // save the height, for removal
	
	if (SETTING(POPUP_TYPE) != /*CUSTOM*/ BALLOON)
	{
#ifdef FLYLINKDC_SUPPORT_WIN_2000
		if (LOBYTE(LOWORD(GetVersion())) >= 5)
#endif
		{
			p->SetWindowLongPtr(GWL_EXSTYLE, p->GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
			typedef bool (CALLBACK * LPFUNC)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
			LPFUNC _d_SetLayeredWindowAttributes = (LPFUNC)GetProcAddress(LoadLibrary(_T("user32")), "SetLayeredWindowAttributes");
			if (_d_SetLayeredWindowAttributes)
			{
				_d_SetLayeredWindowAttributes(p->m_hWnd, 0, SETTING(POPUP_TRANSP), LWA_ALPHA);
			}
		}
	}
	
	//move the window to the top of the z-order and display it
	p->SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	p->ShowWindow(SW_SHOWNA);
	
	//restore focus to window
	::SetFocus(gotFocus);
	
	//increase offset so we know where to place the next popup
	offset = offset + height;
	
	m_popups.push_back(p);
}

void PopupManager::on(TimerManagerListener::Second /*type*/, uint64_t tick)
{
	// TODO - ïîäïèñàòüñÿ ïîçæå. dcassert(!BaseChatFrame::g_isStartupProcess);
	::PostMessage(WinUtil::mainWnd, WM_SPEAKER, MainFrame::REMOVE_POPUP, (LPARAM)tick); // [!] IRainman opt.
	
}


void PopupManager::AutoRemove(uint64_t tick) // [!] IRainman opt.
{
	const uint64_t popupTime = SETTING(POPUP_TIME) * 1000; // [+] IRainman opt.
	//check all m_popups and see if we need to remove anyone
	for (auto i = m_popups.cbegin(); i != m_popups.cend(); ++i)
	{
	
		if ((*i)->visible + popupTime < tick)
		{
			//okay remove the first popup
			Remove((*i)->id);
			
			//if list is empty there is nothing more to do
			if (m_popups.empty())
				return;
				
			//start over from the beginning
			i = m_popups.cbegin();
		}
	}
}

void PopupManager::Remove(uint32_t pos)
{
	//find the correct window
	auto i = m_popups.cbegin();
	
	for (; i != m_popups.cend(); ++i)
	{
		if ((*i)->id == pos)
			break;
	}
	dcassert(i != m_popups.cend());
	if (i == m_popups.cend())
		return;
	//remove the window from the list
	PopupWnd *p = *i;
	i = m_popups.erase(i);
	
	dcassert(p);
	if (p == nullptr)
	{
		return;
	}
	
	//close the window and delete it, ensure that correct height is used from here
	height = p->height;
	p->SendMessage(WM_CLOSE, 0, 0);
	safe_delete(p);
	
	//set offset one window position lower
	dcassert(offset > 0);
	offset = offset - height;
	
	//nothing to do
	if (m_popups.empty())
		return;
		
	CRect rc;
	
	//move down all windows
	for (; i != m_popups.cend(); ++i)
	{
		(*i)->GetWindowRect(rc);
		rc.top += height;
		rc.bottom += height;
		(*i)->MoveWindow(rc);
	}
}
