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

#include "TextFrame.h"
#include "../client/File.h"

#define MAX_TEXT_LEN 32768

void TextFrame::openWindow(const tstring& aFileName)
{
	TextFrame* frame = new TextFrame(aFileName);
	frame->CreateEx(WinUtil::g_mdiClient);
}

DWORD CALLBACK TextFrame::EditStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb)
{
	DWORD dwRet = 0xffffffff;
	File *pFile = reinterpret_cast<File *>(dwCookie);
	
	if (pFile)
	{
		//FileException e;
		
		try
		{
			size_t len = cb;
			*pcb = (LONG)pFile->read(pbBuff, len);
			dwRet = 0;
		}
		catch (const FileException& /*e*/)
		{
			//UNREFERENCED_PARAMETER(e);
		}
	}
	
	return dwRet;
}

LRESULT TextFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlPad.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	               WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_NOHIDESEL | ES_READONLY, WS_EX_CLIENTEDGE);
	               
	ctrlPad.LimitText(0);
	ctrlPad.SetFont(Fonts::g_font);
	
	try
	{
		File hFile(file, File::READ, File::OPEN);
		
		string str = hFile.read(4);
		
		static BYTE bUTF8signature[] = {0xef, 0xbb, 0xbf};
		static BYTE bUnicodeSignature[] = {0xff, 0xfe};
		int nFormat = SF_TEXT;
		if (memcmp(str.c_str(), bUTF8signature, sizeof(bUTF8signature)) == 0)
		{
			nFormat = SF_TEXT | SF_USECODEPAGE | (CP_UTF8 << 16);
			hFile.setPos(_countof(bUTF8signature));
		}
		else if (memcmp(str.c_str(), bUnicodeSignature, sizeof(bUnicodeSignature)) == 0)
		{
			nFormat = SF_TEXT | SF_UNICODE;
			hFile.setPos(_countof(bUnicodeSignature));
		}
		else
		{
			nFormat = SF_TEXT;
			hFile.setPos(0);
		}
		
		EDITSTREAM es =
		{
			(DWORD_PTR)&hFile,
			0,
			EditStreamCallback
		};
		ctrlPad.StreamIn(nFormat, es);
		
		ctrlPad.EmptyUndoBuffer();
		SetWindowText(Text::toT(Util::getFileName(Text::fromT(file))).c_str());
	}
	catch (const FileException& e)
	{
		SetWindowText(Text::toT(Util::getFileName(Text::fromT(file)) + ": " + e.getError()).c_str());
	}
	
	bHandled = FALSE;
	return 1;
}

LRESULT TextFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	SettingsManager::getInstance()->removeListener(this);
	bHandled = FALSE;
	return 0;
}

void TextFrame::UpdateLayout(BOOL /*bResizeBars*/ /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	CRect rc;
	
	GetClientRect(rc);
	
	rc.bottom -= 1;
	rc.top += 1;
	rc.left += 1;
	rc.right -= 1;
	ctrlPad.MoveWindow(rc);
}

void TextFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

/**
 * @file
 * $Id: TextFrame.cpp,v 1.17 2006/09/04 12:18:49 bigmuscle Exp $
 */
