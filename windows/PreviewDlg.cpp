/*
 * Copyright (C) 2012-2017 FlylinkDC++ Team http://flylinkdc.com
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
#include "wtl_flylinkdc.h"
#include "PreviewDlg.h"
#include "WinUtil.h"
#include "../FlyFeatures/flyServer.h"

LRESULT PreviewDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(SETTINGS_PREVIEW_DLG));
	SetDlgItemText(IDC_PREV_NAME2, CTSTRING(SETTINGS_NAME2));
	SetDlgItemText(IDC_PREV_APPLICATION, CTSTRING(SETTINGS_PREVIEW_DLG_APPLICATION));
	SetDlgItemText(IDC_PREV_ARG, CTSTRING(SETTINGS_PREVIEW_DLG_ARGUMENTS));
	tstring l_def_text = CTSTRING(SETTINGS_PREVIEW_DLG_EXT);
	string l_ext = CFlyServerConfig::getAllMediainfoExt();
	Text::replace_all(l_ext, ",", ";");
	l_def_text += _T("\r\nDefault: ") + Text::toT(l_ext);
	SetDlgItemText(IDC_PREV_EXT, l_def_text.c_str());
	
	SetDlgItemText(IDCANCEL, CTSTRING(CANCEL));
	SetDlgItemText(IDOK, CTSTRING(OK));
	
	ATTACH(IDC_PREVIEW_NAME, ctrlName);
	ATTACH(IDC_PREVIEW_APPLICATION, ctrlApplication);
	ATTACH(IDC_PREVIEW_ARGUMENTS, ctrlArguments);
	ATTACH(IDC_PREVIEW_EXTENSION, ctrlExtensions);
	
	ctrlName.SetWindowText(m_name.c_str());
	ctrlApplication.SetWindowText(m_application.c_str());
	ctrlArguments.SetWindowText(m_argument.c_str());
	ctrlExtensions.SetWindowText(m_extensions.c_str());
	
	CenterWindow(GetParent());
	return 0;
}

LRESULT PreviewDlg::OnBrowse(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/)
{
	tstring x;
	
	GET_TEXT(IDC_PREVIEW_APPLICATION, x);
	
	if (WinUtil::browseFile(x, m_hWnd, false,  Util::emptyStringT, _T("Application\0*.exe\0\0")) == IDOK)  // TODO translate
	{
		SetDlgItemText(IDC_PREVIEW_APPLICATION, x.c_str());
	}
	
	return 0;
}
LRESULT PreviewDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		if (ctrlName.GetWindowTextLength() == 0 ||
		        ctrlApplication.GetWindowTextLength() == 0)
		{
			MessageBox(CWSTRING(NAME_COMMAND_EMPTY));
			return 0;
		}
		GET_TEXT(IDC_PREVIEW_NAME, m_name);
		GET_TEXT(IDC_PREVIEW_APPLICATION, m_application);
		GET_TEXT(IDC_PREVIEW_ARGUMENTS, m_argument);
		GET_TEXT(IDC_PREVIEW_EXTENSION, m_extensions);
		Text::replace_all(m_extensions, _T(" "), _T(""));
		Text::replace_all(m_extensions, _T(","), _T(";"));
	}
	EndDialog(wID);
	return 0;
}
