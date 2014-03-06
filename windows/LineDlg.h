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

#if !defined(LINE_DLG_H)
#define LINE_DLG_H

#pragma once

#include "../client/Util.h"
#include "WinUtil.h"

class LineDlg : public CDialogImpl<LineDlg>
{
		CEdit ctrlLine;
		CStatic ctrlDescription;
	public:
		tstring description;
		tstring title;
		// Out data:
		tstring line;
		bool checked;
		
		bool save_option;
		// Type:
		bool password;
		bool disabled;
		
		enum { IDD = IDD_LINE };
		
		BEGIN_MSG_MAP(LineDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		END_MSG_MAP()
		
		LineDlg() : password(false), disabled(false), save_option(false), checked(true) { }
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlLine.SetFocus();
			return FALSE;
		}
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlLine.Attach(GetDlgItem(IDC_LINE));
			ctrlLine.SetFocus();
			ctrlLine.SetWindowText(line.c_str());
			ctrlLine.SetSelAll(TRUE);
			SetDlgItemText(IDCANCEL, CTSTRING(CANCEL));
			
			if (password)
			{
				GetDlgItem(IDC_SAVE_PASSWORD).ShowWindow(true);
				SetDlgItemText(IDC_SAVE_PASSWORD, CTSTRING(SAVE_PASSWORD));
				ctrlLine.SetWindowLongPtr(GWL_STYLE, ctrlLine.GetWindowLongPtr(GWL_STYLE) | ES_PASSWORD);
				ctrlLine.SetPasswordChar('*');
			}
			else if (save_option)
			{
				GetDlgItem(IDC_SAVE_PASSWORD).ShowWindow(true);
				SetDlgItemText(IDC_SAVE_PASSWORD, CTSTRING(SAVE));
			}
			
			ctrlDescription.Attach(GetDlgItem(IDC_DESCRIPTION));
			ctrlDescription.SetWindowText(description.c_str());
			
			SetWindowText(title.c_str());
			
			if (disabled)
			{
				::EnableWindow(GetDlgItem(IDCANCEL), false);
				::ShowWindow(GetDlgItem(IDCANCEL), FALSE);
				::MoveWindow(GetDlgItem(IDOK), 275, 29, 75, 24, false);
				::SetForegroundWindow((HWND)*this);
			}
			
			CenterWindow(GetParent());
			return FALSE;
		}
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (wID == IDOK)
			{
				WinUtil::GetWindowText(line, ctrlLine);
				checked = IsDlgButtonChecked(IDC_SAVE_PASSWORD) == BST_CHECKED;
			}
			EndDialog(wID);
			return 0;
		}
};

class KickDlg : public CDialogImpl<KickDlg>
{
		CComboBox ctrlLine;
		CStatic ctrlDescription;
	public:
		tstring line;
		static tstring m_sLastMsg;
		tstring description;
		tstring title;
		
		enum { IDD = IDD_KICK };
		
		BEGIN_MSG_MAP(KickDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		END_MSG_MAP()
		
		KickDlg() {}
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlLine.SetFocus();
			return FALSE;
		}
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
	private:
		tstring m_Msgs[20];
};

#endif // !defined(LINE_DLG_H)

/**
 * @file
 * $Id: LineDlg.h 392 2008-06-21 21:10:31Z BigMuscle $
 */
