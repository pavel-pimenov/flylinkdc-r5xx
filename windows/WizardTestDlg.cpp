/*
 * Copyright (C) 2011-2013 FlylinkDC++ Team http://flylinkdc.com/
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
#include "WizardTestDlg.h"

LRESULT WizardTestDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(WIZARD_TEST_DLG_TITLE));
	SetDlgItemText(IDC_WIZARD_NET_TST_BTN_DEFAULT, CTSTRING(RESTORE_DEFAUL));
	
	
	const string& aUrl = SETTING(URL_TEST_IP);
	SetDlgItemText(IDC_WIZARD_NET_TSTURL, Text::toT(aUrl).c_str());
	
	return 0;
}

LRESULT WizardTestDlg::OnBtnDefault(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const string aUrl = URL_TEST_IP_DEFAULT;
	SetDlgItemText(IDC_WIZARD_NET_TSTURL, Text::toT(aUrl).c_str());
	
	return 0;
}
LRESULT WizardTestDlg::OnBtnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		tstring aurl;
		GET_TEXT(IDC_WIZARD_NET_TSTURL, aurl);
		string url = Text::fromT(aurl);
		SET_SETTING(URL_TEST_IP, url);
	}
	EndDialog(wID);
	return 0;
}
