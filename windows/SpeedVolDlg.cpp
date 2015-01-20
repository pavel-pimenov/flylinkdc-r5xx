/*
* Copyright (C) 2011-2015 FlylinkDC++ Team http://flylinkdc.com/
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
#include "SpeedVolDlg.h"
#include "WinUtil.h"
#include "../client/Util.h"

LRESULT SpeedVolDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	//SetWindowText(CTSTRING(SPEED_LIMIT_DLG_CAPTION));
	//VerifyLimit(m_limit);
	if (m_limit < m_min) m_limit = m_min;
	if (m_limit > m_max) m_limit = m_max;
	
	// Буду строить велосипед.
	CPoint pt;
	GetCursorPos(&pt);
	CRect rcWindow;
	GetWindowRect(rcWindow);
	ScreenToClient(rcWindow);
	SetWindowPos(NULL, pt.x, pt.y - (rcWindow.bottom - rcWindow.top), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	// Велосипед построен, модалька выведена там где нужно
	
	tstring limitEdit = Util::toStringW(m_limit);
	SetDlgItemText(IDC_SPEEDLIMITDLG_EDIT, limitEdit.c_str());
	trackBar.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_SLIDER));
	trackBar.SetRange(m_max / -24, m_min / -24, true);
	trackBar.SetTicFreq(24);
	trackBar.SetPos(m_limit / -24);
	trackBar.SetFocus();
	trackBar.Detach();
	
	return FALSE;
}

LRESULT SpeedVolDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		// Update search
		tstring buf;
		GET_TEXT(IDC_SPEEDLIMITDLG_EDIT, buf);
		m_limit = Util::toInt(buf);
		//  m_limit = VerifyLimit(m_limit);
		if (m_limit < m_min) m_limit = m_min;
		if (m_limit > m_max) m_limit = m_max;
	}
	EndDialog(wID);
	return FALSE;
}

LRESULT SpeedVolDlg::OnChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	trackBar.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_SLIDER));
	
	const int pos = trackBar.GetPos() * -24;
	
	tstring limitEdit = Util::toStringW(pos);
	SetDlgItemText(IDC_SPEEDLIMITDLG_EDIT, limitEdit.c_str());
	
	trackBar.Detach();
	return FALSE;
}
/*
int VerifyLimit(int l_lim)
{
    if (l_lim < m_min) l_lim = m_min;
    if (l_lim > m_max) l_lim = m_max;
    return l_lim;
}
*/