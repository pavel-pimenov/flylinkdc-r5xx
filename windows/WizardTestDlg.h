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


#if !defined(_WIZARD_TEST_DLG_H_)
#define _WIZARD_TEST_DLG_H_

#pragma once

#include "WinUtil.h"


class WizardTestDlg : public CDialogImpl<WizardTestDlg>
{
		CEdit ctrlTestDlgEdit;
	public:
		enum { IDD = IDD_WIZARD_NET_TESTSETUP };
		BEGIN_MSG_MAP(WizardTestDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_WIZARD_NET_TST_BTN_DEFAULT, OnBtnDefault)
		COMMAND_ID_HANDLER(IDOK, OnBtnClose)
		COMMAND_ID_HANDLER(IDCANCEL, OnBtnClose)
		END_MSG_MAP()
		
		WizardTestDlg()  { }
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnBtnClose(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnBtnDefault(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
};

#endif // _WIZARD_TEST_DLG_H_

/**
* @file
* $Id: WizardTestDlg.h 8234 2011-09-30 22:27:19Z sa.stolper $
*/