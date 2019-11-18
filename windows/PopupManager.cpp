/*
* Copyright (C) 2003-2005 P�r Bj�rklund, per.bjorklund@gmail.com
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

#include "PopupManager.h"
#include "MainFrm.h"

void PopupManager::Show(const tstring &aMsg, const tstring &aTitle, int Icon, bool preview /*= false*/)
{
	dcassert(ClientManager::isStartup() == false);
	dcassert(ClientManager::isBeforeShutdown() == false);
	if (ClientManager::isBeforeShutdown())
		return;
	if (ClientManager::isStartup())
		return;
		
	if (!m_is_activated)
		return;
		
		
	if (!Util::getAway() && BOOLSETTING(POPUP_AWAY) && !preview)
		return;
		
	if (!MainFrame::isAppMinimized() && BOOLSETTING(POPUP_MINIMIZED) && !preview)
		return;
		
	tstring msg = aMsg;
	if (int(aMsg.length()) > SETTING(MAX_MSG_LENGTH))
	{
		msg = aMsg.substr(0, (SETTING(MAX_MSG_LENGTH) - 3));
		msg += _T("...");
	}
#ifdef _DEBUG
	msg += Text::toT(" m_popups.size() = " + Util::toString(m_popups.size()));
#endif
	
	if (SETTING(POPUP_TYPE) == BALLOON && MainFrame::getMainFrame())
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
	
	if (m_popups.size() > 10)
	{
		//LogManager::message("PopupManager - m_popups.size() > 10! Ignore");
		return;
	}
	
	if (SETTING(POPUP_TYPE) == CUSTOM && PopupImage != SETTING(POPUPFILE))
	{
		PopupImage = SETTING(POPUPFILE);
		m_popuptype = SETTING(POPUP_TYPE);
		m_hBitmap = (HBITMAP)::LoadImage(NULL, Text::toT(PopupImage).c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
	}
	
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
	dcassert(WinUtil::g_mainWnd);
	HWND gotFocus = ::SetFocus(WinUtil::g_mainWnd);
	
	//compute the window position
	CRect rc(screenWidth - width, screenHeight - height - offset, screenWidth, screenHeight - offset);
	
	//Create a new popup
	PopupWnd *p = new PopupWnd(msg, aTitle, rc, m_id++, m_hBitmap);
	p->height = height; // save the height, for removal
	
	if (SETTING(POPUP_TYPE) != /*CUSTOM*/ BALLOON)
	{
		typedef bool (CALLBACK * LPFUNC)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
		LPFUNC _d_SetLayeredWindowAttributes = (LPFUNC)GetProcAddress(LoadLibrary(_T("user32")), "SetLayeredWindowAttributes");
		if (_d_SetLayeredWindowAttributes)
		{
			p->SetWindowLongPtr(GWL_EXSTYLE, p->GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
			_d_SetLayeredWindowAttributes(p->m_hWnd, 0, SETTING(POPUP_TRANSP), LWA_ALPHA);
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

void PopupManager::on(TimerManagerListener::Second /*type*/, uint64_t tick) noexcept
{
	dcassert(WinUtil::g_mainWnd);
	if (ClientManager::isBeforeShutdown())
		return;
	if (WinUtil::g_mainWnd)
	{
		::PostMessage(WinUtil::g_mainWnd, WM_SPEAKER, MainFrame::REMOVE_POPUP, (LPARAM)tick);
	}
}

void PopupManager::AutoRemove(uint64_t tick)
{
	const uint64_t popupTime = SETTING(POPUP_TIME) * 1000;
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
	if (!ClientManager::isBeforeShutdown())
	{
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
}
