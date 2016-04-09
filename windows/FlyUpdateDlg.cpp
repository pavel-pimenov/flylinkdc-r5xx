/*
 * Copyright (C) 2001-2016 FlylinkDC++ Team http://flylinkdc.com
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
#include "FlyUpdateDlg.h"
#include "WinUtil.h"

FlyUpdateDlg::FlyUpdateDlg(const string& data, const string& rtfData, const AutoUpdateFiles& fileList)
	: m_data(data)
	, m_rtfData(rtfData)
	, m_fileList(fileList)
	, m_UpdateIcon(NULL)
{
	m_richEditLibrary = ::LoadLibrary(L"Msftedit.dll");
	if (!m_richEditLibrary)
	{
		_RPT1(_CRT_WARN, "LoadLibrary for Msftedit.dll failed with: %d\n", ::GetLastError());
		return;
	}
	
}

FlyUpdateDlg::~FlyUpdateDlg()
{
	ctrlCommands.Detach();
	if (m_UpdateIcon)
		::DeleteObject(m_UpdateIcon);
	if (m_richEditLibrary)
		::FreeLibrary(m_richEditLibrary);
}

LRESULT FlyUpdateDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetWindowText(CTSTRING(AUTOUPDATE_TITLE));
	
	GetDlgItem(IDC_STATICDESCRIPTION).SetWindowText(Text::toT(m_data).c_str());
	
	CRect rc;
	ctrlCommands.Attach(GetDlgItem(IDC_UPDATE_FILES));
	ctrlCommands.GetClientRect(rc);
	ctrlCommands.InsertColumn(0, CTSTRING(FILE), LVCFMT_LEFT, (rc.Width() / 8) * 5, 0);
	ctrlCommands.InsertColumn(1, CTSTRING(SIZE), LVCFMT_LEFT, rc.Width() / 6, 1);
	ctrlCommands.InsertColumn(2, CTSTRING(COMPRESSED), LVCFMT_LEFT, rc.Width() / 6, 2);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlCommands);
	SET_LIST_COLOR(ctrlCommands);
	auto cnt = ctrlCommands.GetItemCount();
	for (size_t i = 0; i < m_fileList.size(); i++)
	{
		ctrlCommands.insert(cnt++, Text::toT(m_fileList[i].m_sName).c_str());
		ctrlCommands.SetItemText(i, 1, Text::toT(Util::formatBytes(m_fileList[i].m_size)).c_str());
		ctrlCommands.SetItemText(i, 2, Text::toT(Util::formatBytes(m_fileList[i].m_packedSize)).c_str());
	}
	
	GetDlgItem(IDOK).SetWindowText(CTSTRING(AUTOUPDATE_UPDATE_NOW_BUTTON));
	GetDlgItem(IDC_IGNOREUPDATE).SetWindowText(CTSTRING(AUTOUPDATE_UPDATE_IGNORE_BUTTON));
	GetDlgItem(IDCANCEL).SetWindowText(CTSTRING(AUTOUPDATE_UPDATE_CANCEL_BUTTON));
	// TODO падаем на XP
	// http://code.google.com/p/flylinkdc/issues/detail?id=1253
	WinUtil::FillRichEditFromString((HWND)GetDlgItem(IDC_RTFDESCRIPTION), m_rtfData);   // TODO please refactoring this to use unicode.
	
	m_UpdateIcon = (HICON)::LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_AUTOUPDATE), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
	SetIcon(m_UpdateIcon, FALSE);
	SetIcon(m_UpdateIcon, TRUE);
	// for Custom Themes Bitmap
	img_f.LoadFromResource(IDR_FLYLINK_PNG, _T("PNG")); // Leak?
	GetDlgItem(IDC_STATIC).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)img_f));
	
	LRESULT lResult = GetDlgItem(IDC_RTFDESCRIPTION).SendMessage(EM_GETEVENTMASK, 0, 0);
	GetDlgItem(IDC_RTFDESCRIPTION).SendMessage(EM_SETEVENTMASK , 0, lResult | ENM_LINK);
	GetDlgItem(IDC_RTFDESCRIPTION).SendMessage(EM_AUTOURLDETECT, TRUE, 0);
	
	CenterWindow(GetParent());
	return TRUE;
}
LRESULT FlyUpdateDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT FlyUpdateDlg::OnLink(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	const ENLINK* pL = (const ENLINK*)pnmh;
	if (WM_LBUTTONDOWN == pL->msg)
	{
		LocalArray<TCHAR, MAX_PATH> buf;
		CRichEditCtrl(pL->nmhdr.hwndFrom).GetTextRange(pL->chrg.cpMin, pL->chrg.cpMax, buf.data());
		WinUtil::openFile(buf.data());
	}
	else
		bHandled = FALSE;
	return 0;
}

/**
* @file
* $Id: FlyUpdateDlg.cpp 7011 2011-05-16 20:17:37Z sa.stolper $
*/