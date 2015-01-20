/*
 * Copyright (C) 2012-2015 FlylinkDC++ Team http://flylinkdc.com/
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


#ifdef SSA_VIDEO_PREVIEW_FEATURE

#include "../client/SettingsManager.h"
#include "WinUtil.h"
#include "Resource.h"

#include "IntPreviewPage.h"

PropPage::TextItem IntPreviewPage::texts[] =
{
	{ IDC_INT_PREVIEW_HTTP_SETUP_GROUP, ResourceManager::INT_PREVIEW_HTTP_SETUP_GROUP },
	{ IDC_INT_PREVIEW_SERVER_PORT_STATIC, ResourceManager::INT_PREVIEW_SERVER_PORT_STATIC },
	{ IDC_INT_PREVIEW_SERVER_SPEED_STATIC, ResourceManager::INT_PREVIEW_SERVER_SPEED_STATIC },
	{ IDC_KBPS, ResourceManager::KBPS },
	{ IDC_INT_PREVIEW_HTTP_CL_SETUP_GROUP, ResourceManager::INT_PREVIEW_HTTP_CL_SETUP_GROUP },
	{ IDC_INT_PREVIEW_HTTP_CL_SETUP_PATH, ResourceManager::INT_PREVIEW_HTTP_CL_SETUP_PATH },
	//{ IDC_BTN_SELECT, ResourceManager::BROWSE }, // [~] JhaoDa, not necessary any more
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item IntPreviewPage::items[] =
{
	{ IDC_INT_PREVIEW_HTTP_CL_SETUP_PATH_EDIT, SettingsManager::INT_PREVIEW_CLIENT_PATH, PropPage::T_STR },
	{ IDC_INT_PREVIEW_SERVER_PORT, SettingsManager::INT_PREVIEW_SERVER_PORT, PropPage::T_INT },
	{ IDC_INT_PREVIEW_SERVER_SPEED, SettingsManager::INT_PREVIEW_SERVER_SPEED, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};


PropPage::ListItem IntPreviewPage::listItems[] =
{
	{ SettingsManager::INT_PREVIEW_USE_VIDEO_SCROLL, ResourceManager::INT_PREVIEW_USE_VIDEO_SCROLL },
	{ SettingsManager::INT_PREVIEW_START_CLIENT, ResourceManager::INT_PREVIEW_START_CLIENT },
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};


LRESULT IntPreviewPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{

	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items, listItems, GetDlgItem(IDC_INT_PREVIEW_LIST));
	ctrlPrevlist.Attach(GetDlgItem(IDC_INT_PREVIEW_LIST)); // [+] IRainman
	
	return TRUE;
}


void IntPreviewPage::write()
{
	PropPage::write((HWND)*this, items);
	
}

LRESULT IntPreviewPage::OnBrowseClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// Select Folder
	tstring target;
	if (WinUtil::browseFile(target, m_hWnd))
	{
		CWindow ctrlName = GetDlgItem(IDC_INT_PREVIEW_HTTP_CL_SETUP_PATH_EDIT);
		ctrlName.SetWindowText(target.c_str());
		if (ctrlName.GetWindowTextLength() == 0)
			ctrlName.SetWindowText(Util::getLastDir(target).c_str());
	}
	
	return NULL;
}

#endif // SSA_VIDEO_PREVIEW_FEATURE