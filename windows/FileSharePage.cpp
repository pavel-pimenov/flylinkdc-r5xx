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

#ifdef SSA_INCLUDE_FILE_SHARE_PAGE


#include "../client/SettingsManager.h"
#include "Resource.h"

#include "FileSharePage.h"

PropPage::TextItem FileSharePage::texts[] =
{
	{ IDC_FILESHARE_BORDER, ResourceManager::FILESHARE_BORDER },
	{ IDC_FILESHARE_WARNING, ResourceManager::FILESHARE_WARNING },
	{ IDC_FILESHARE_ADD, ResourceManager::ADD2 },
	{ IDC_FILESHARE_REMOVE, ResourceManager::REMOVE2 },
	{ IDC_FILESHARE_REINDEX, ResourceManager::FILESHARE_REINDEX },
	{ IDC_FILSHARE_OPTIONS_BORDER, ResourceManager::SETTINGS_OPTIONS },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item FileSharePage::items[] =
{
	// { IDC_SLOTS, SettingsManager::SLOTS, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

PropPage::ListItem FileSharePage::listItems[] =
{
	{ SettingsManager::FILESHARE_INC_FILELIST, ResourceManager::FILESHARE_INC_FILELIST },
	{ SettingsManager::FILESHARE_REINDEX_ON_START, ResourceManager::FILESHARE_REINDEX_ON_START },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT FileSharePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_FILESHARE_BOOLEANS));
	ctrlList.Attach(GetDlgItem(IDC_FILESHARE_BOOLEANS));
	
	return TRUE;
}
void FileSharePage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
}

#endif // SSA_INCLUDE_FILE_SHARE_PAGE

/**
 * @file
 * $Id: FileSharePage.cpp 8089 2011-09-03 21:57:11Z a.rainman $
 */
