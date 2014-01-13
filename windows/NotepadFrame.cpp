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

#include "NotepadFrame.h"
#include "../client/File.h"

LRESULT NotepadFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlPad.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	               WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL, WS_EX_CLIENTEDGE);
	               
	ctrlPad.LimitText(0);
	ctrlPad.SetFont(Fonts::font);
	string tmp;
	try
	{
		tmp = File(Util::getNotepadFile(), File::READ, File::OPEN).read();
	}
	catch (const FileException&)
	{
		// ...
	}
	
	if (tmp.empty())
	{
		tmp = SETTING(NOTEPAD_TEXT);
		if (!tmp.empty())
		{
			m_dirty = true;
			SET_SETTING(NOTEPAD_TEXT, Util::emptyString);
		}
	}
	
	ctrlPad.SetWindowText(Text::toT(tmp).c_str());
	ctrlPad.EmptyUndoBuffer();
	ctrlClientContainer.SubclassWindow(ctrlPad.m_hWnd);
	
	bHandled = FALSE;
	return 1;
}

LRESULT NotepadFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		SettingsManager::getInstance()->removeListener(this);
		if (m_dirty || ctrlPad.GetModify())
		{
			tstring tmp;
			WinUtil::GetWindowText(tmp, ctrlPad);
			if (tmp.size())
			{
				try
				{
					File(Util::getNotepadFile(), File::WRITE, File::CREATE | File::TRUNCATE).write(Text::fromT(tmp));
				}
				catch (const FileException&)
				{
					// Oops...
				}
			}
		}
		
		WinUtil::setButtonPressed(IDC_NOTEPAD, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		bHandled = FALSE;
		return 0;
	}
}

void NotepadFrame::UpdateLayout(BOOL /*bResizeBars*/ /* = TRUE */)
{
	CRect rc;
	
	GetClientRect(rc);
	
	rc.bottom -= 1;
	//rc.top += 1; [~] Sergey Shushkanov
	//rc.left += 1; [~] Sergey Shushkanov
	//rc.right -= 1; [~] Sergey Shushkanov
	ctrlPad.MoveWindow(rc);
	
}

LRESULT NotepadFrame::onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	HWND focus = GetFocus();
	bHandled = false;
	if (focus == ctrlPad.m_hWnd)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		tstring x;
		tstring::size_type start = (tstring::size_type)WinUtil::textUnderCursor(pt, ctrlPad, x);
		tstring::size_type end = x.find(_T(' '), start);
		
		if (end == string::npos)
			end = x.length();
			
		bHandled = WinUtil::parseDBLClick(x, start, end);
	}
	return 0;
}

void NotepadFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

/**
 * @file
 * $Id: NotepadFrame.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
