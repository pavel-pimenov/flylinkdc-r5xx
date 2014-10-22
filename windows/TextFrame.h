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

#if !defined(TEXT_FRAME_H)
#define TEXT_FRAME_H

#include "FlatTabCtrl.h"
#include "WinUtil.h"

class TextFrame : public MDITabChildWindowImpl<TextFrame>, private SettingsManagerListener
#ifdef _DEBUG
	, virtual NonDerivable<TextFrame>, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		static void openWindow(const tstring& aFileName);
		
		DECLARE_FRAME_WND_CLASS_EX(_T("TextFrame"), IDR_NOTEPAD, 0, COLOR_3DFACE);
		
		TextFrame(const tstring& fileName) : file(fileName)
		{
			SettingsManager::getInstance()->addListener(this);
		}
		~TextFrame() { }
		
		typedef MDITabChildWindowImpl<TextFrame> baseClass;
		BEGIN_MSG_MAP(TextFrame)
		MESSAGE_HANDLER(WM_SETFOCUS, OnFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
		LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			HWND hWnd = (HWND)lParam;
			HDC hDC = (HDC)wParam;
			if (hWnd == ctrlPad.m_hWnd)
			{
				::SetBkColor(hDC, Colors::bgColor);
				::SetTextColor(hDC, Colors::textColor);
				return (LRESULT)Colors::bgBrush;
			}
			bHandled = FALSE;
			return FALSE;
		}
		
		LRESULT OnFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlPad.SetFocus();
			return 0;
		}
		
	private:
	
		tstring file;
		CRichEditCtrl ctrlPad;
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/);
		
		static DWORD CALLBACK EditStreamCallback(DWORD_PTR dwCookie, LPBYTE pbBuff, LONG cb, LONG *pcb);
};

#endif // !defined(TEXT_FRAME_H)

/**
 * @file
 * $Id: TextFrame.h,v 1.13 2006/05/08 08:36:20 bigmuscle Exp $
 */
