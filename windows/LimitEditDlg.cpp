/*
* Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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
#include "LimitEditDlg.h"
#include "WinUtil.h"
#include "../client/FavoriteUser.h"

LRESULT LimitEditDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(SPEED_LIMIT));
	
	//tstring limitEdit = Text::toT(FavoriteUser::GetLimitText(m_limit));   // [-] SCALOlaz: clear Kb/Mb text
	tstring limitEdit = Util::toStringW(m_limit);
	SetDlgItemText(IDC_SPEEDLIMITDLG_EDIT, limitEdit.c_str());
	
	// IDC_SPEEDLIMITDLG_SLIDER
	trackBar.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_SLIDER));
	trackBar.SetRange(MAXIMAL_LIMIT_KBPS / -10, 0, true);
	
	trackBar.SetTicFreq(MAXIMAL_LIMIT_KBPS / 100);
	trackBar.SetPos(m_limit / -10);
	
	trackBar.SetFocus();
	trackBar.Detach();
	
	return FALSE;
}

LRESULT LimitEditDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		// Update search
		tstring buf;
		GET_TEXT(IDC_SPEEDLIMITDLG_EDIT, buf);
		m_limit = Util::toInt(buf.data());
		//  if (m_limit < 0) m_limit = 10;
		//  if (m_limit > m_max) m_limit = m_max;
	}
	EndDialog(wID);
	return FALSE;
}

LRESULT LimitEditDlg::OnChangeSliderScroll(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	trackBar.Attach(GetDlgItem(IDC_SPEEDLIMITDLG_SLIDER));
	
	int pos = trackBar.GetPos() * -10;
	
	
	//tstring limitEdit = Text::toT(FavoriteUser::GetLimitText(pos));   //[-] SCALOlaz: clear Kb/Mb text
	tstring limitEdit = Util::toStringW(pos);
	SetDlgItemText(IDC_SPEEDLIMITDLG_EDIT, limitEdit.c_str());
	
	trackBar.Detach();
	return FALSE;
}