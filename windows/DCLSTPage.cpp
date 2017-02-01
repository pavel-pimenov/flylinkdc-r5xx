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

#include "DCLSTPage.h"
#include "CommandDlg.h"

PropPage::TextItem DCLSTPage::texts[] =
{
	{ IDC_DCLS_USE, ResourceManager::DCLS_USE },
	{ IDC_DCLS_GENERATORBORDER, ResourceManager::DCLS_GENERATORBORDER },
	{ IDC_DCLS_CREATE_IN_FOLDER, ResourceManager::DCLS_CREATE_IN_FOLDER },
	{ IDC_DCLS_ANOTHER_FOLDER, ResourceManager::DCLS_ANOTHER_FOLDER },
	//{ IDC_PREVIEW_BROWSE, ResourceManager::BROWSE}, // [~] JhaoDa, not necessary any more
	{ IDC_DCLST_CLICK_STATIC, ResourceManager::DCLS_CLICK_ACTION},
	{ IDC_DCLST_INCLUDESELF, ResourceManager::DCLST_INCLUDESELF},
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item DCLSTPage::items[] =
{
	{ IDC_DCLS_FOLDER, SettingsManager::DCLST_FOLDER_DIR, PropPage::T_STR },
	{ IDC_DCLS_USE, SettingsManager::DCLST_REGISTER, PropPage::T_BOOL },
	{ IDC_DCLST_INCLUDESELF, SettingsManager::DCLST_INCLUDESELF, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

LRESULT DCLSTPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items);
	
	if (BOOLSETTING(DCLST_CREATE_IN_SAME_FOLDER))
	{
		CheckDlgButton(IDC_DCLS_CREATE_IN_FOLDER, BST_CHECKED);
	}
	else
	{
		CheckDlgButton(IDC_DCLS_ANOTHER_FOLDER, BST_CHECKED);
	}
	
	
	magnetClick.Attach(GetDlgItem(IDC_DCLST_CLICK));
	magnetClick.AddString(CTSTRING(ASK));
	magnetClick.AddString(CTSTRING(SEARCH));
	magnetClick.AddString(CTSTRING(DOWNLOAD));
	magnetClick.AddString(CTSTRING(OPEN));
	
//	if (SETTING(DCLST_ASK) == 1)
//	{
//		magnetClick.SetCurSel(0);
//	}
//	else
	{
		const int action = SETTING(DCLST_ACTION);
		magnetClick.SetCurSel(action);
	}
	magnetClick.Detach();
	
	EnableDCLST(BOOLSETTING(DCLST_REGISTER));
	
	return TRUE;
}

void DCLSTPage::write()
{
	PropPage::write(*this, items);
	SET_SETTING(DCLST_CREATE_IN_SAME_FOLDER,    IsDlgButtonChecked(IDC_DCLS_CREATE_IN_FOLDER) == BST_CHECKED);
	magnetClick.Attach(GetDlgItem(IDC_DCLST_CLICK));
	g_settings->set(SettingsManager::DCLST_ACTION, magnetClick.GetCurSel());
	magnetClick.Detach();
	
}

LRESULT DCLSTPage::OnClickedUseDCLST(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EnableDCLST(IsDlgButtonChecked(IDC_DCLS_USE) == BST_CHECKED);
	return NULL;
}

LRESULT DCLSTPage::OnClickedDCLSTFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CheckDCLSTPath(IsDlgButtonChecked(IDC_DCLS_USE) == BST_CHECKED && IsDlgButtonChecked(IDC_DCLS_ANOTHER_FOLDER) == BST_CHECKED);
	return NULL;
}

void DCLSTPage::EnableDCLST(BOOL isEnabled)
{
	::EnableWindow(GetDlgItem(IDC_DCLS_GENERATORBORDER), isEnabled);
	// ::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_URL), isEnabled);
	::EnableWindow(GetDlgItem(IDC_DCLS_CREATE_IN_FOLDER), isEnabled);
	::EnableWindow(GetDlgItem(IDC_DCLS_ANOTHER_FOLDER), isEnabled);
	::EnableWindow(GetDlgItem(IDC_DCLST_INCLUDESELF), isEnabled);
	::EnableWindow(GetDlgItem(IDC_DCLST_CLICK), isEnabled);
	CheckDCLSTPath(isEnabled && IsDlgButtonChecked(IDC_DCLS_ANOTHER_FOLDER) == BST_CHECKED);
}

void DCLSTPage::CheckDCLSTPath(BOOL isEnabled)
{
	::EnableWindow(GetDlgItem(IDC_DCLS_FOLDER), isEnabled);
	::EnableWindow(GetDlgItem(IDC_PREVIEW_BROWSE), isEnabled);
}

LRESULT DCLSTPage::OnBrowseClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// Select Folder
	tstring target;
	if (WinUtil::browseDirectory(target, m_hWnd))
	{
		CWindow ctrlName = GetDlgItem(IDC_DCLS_FOLDER);
		ctrlName.SetWindowText(target.c_str());
		if (ctrlName.GetWindowTextLength() == 0)
			ctrlName.SetWindowText(Util::getLastDir(target).c_str());
	}
	
	return NULL;
}
