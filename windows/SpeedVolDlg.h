/*
 * Copyright (C) 2011-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#ifndef _SPEED_VOL_DLG_H_
#define _SPEED_VOL_DLG_H_

#pragma once

#include <commctrl.h>

class SpeedVolDlg: public CDialogImpl<SpeedVolDlg>
{
	public:
		enum { IDD = IDD_SPEEDLIMIT_DLG };
		
		BEGIN_MSG_MAP(SpeedVolDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		MESSAGE_HANDLER(WM_VSCROLL, OnChangeSliderScroll)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		END_MSG_MAP()
		
		SpeedVolDlg(int limit = 0, int min = 0, int max = 6144) : m_limit(limit), m_min(min), m_max(max) {}
		~SpeedVolDlg() {}
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		int GetLimit()
		{
			return m_limit;
		}
		
		// При потере фокуса окна выходим как будто ткнули IDOK. Кто умеет - допиливаем, не стесняемся
		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			HWND focus = ::GetFocus();
			if (focus != trackBar.m_hWnd)
			{
				//  ctrlList.SetFocus();
				//  OnCloseCmd(NULL, IDOK, NULL);
			}
			return 0;
		}
		
	protected:
		//int VerifyLimit(int l_lim);
		int m_limit;
		int m_min;
		int m_max;
		CTrackBarCtrl trackBar;
};

#endif // _SPEED_VOL_DLG_H_
