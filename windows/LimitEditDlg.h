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

#ifndef _LIMIT_EDIT_DLG_H_
#define _LIMIT_EDIT_DLG_H_

#pragma once

#include <commctrl.h>

class LimitEditDlg: public CDialogImpl<LimitEditDlg>
{
	public:
		enum { IDD = IDD_SPEEDLIMIT_DLG };
		
		BEGIN_MSG_MAP(LimitEditDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		MESSAGE_HANDLER(WM_VSCROLL, OnChangeSliderScroll);
		END_MSG_MAP()
		
		
		LimitEditDlg(int limit = 0) : m_limit(limit) {}
		~LimitEditDlg() {}
		
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		int GetLimit() const
		{
			return m_limit;
		}
		
		
	protected:
		int m_limit;
		CTrackBarCtrl trackBar;
};

#endif // _LIMIT_EDIT_DLG_H_
