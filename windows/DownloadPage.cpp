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

#include "stdafx.h"


#include "DownloadPage.h"
#include "WinUtil.h"
#include "PublicHubsListDlg.h"

PropPage::TextItem DownloadPage::texts[] =
{
	{ IDC_SETTINGS_DIRECTORIES, ResourceManager::SETTINGS_DIRECTORIES },
	{ IDC_SETTINGS_DOWNLOAD_DIRECTORY, ResourceManager::SETTINGS_DOWNLOAD_DIRECTORY },
	//{ IDC_BROWSEDIR, ResourceManager::BROWSE_ACCEL }, // [~] JhaoDa, not necessary any more
	{ IDC_SETTINGS_UNFINISHED_DOWNLOAD_DIRECTORY, ResourceManager::SETTINGS_UNFINISHED_DOWNLOAD_DIRECTORY },
	//{ IDC_BROWSETEMPDIR, ResourceManager::BROWSE }, // [~] JhaoDa, not necessary any more
	{ IDC_SETTINGS_DOWNLOAD_LIMITS, ResourceManager::SETTINGS_DOWNLOAD_LIMITS },
	{ IDC_SETTINGS_DOWNLOADS_MAX, ResourceManager::SETTINGS_DOWNLOADS_MAX },
	{ IDC_SETTINGS_FILES_MAX, ResourceManager::SETTINGS_FILES_MAX },
	{ IDC_EXTRA_DOWNLOADS_MAX, ResourceManager::SETTINGS_CZDC_EXTRA_DOWNLOADS },
	{ IDC_SETTINGS_DOWNLOADS_SPEED_PAUSE, ResourceManager::SETTINGS_DOWNLOADS_SPEED_PAUSE },
	{ IDC_SETTINGS_SPEEDS_NOT_ACCURATE, ResourceManager::SETTINGS_SPEEDS_NOT_ACCURATE },
	{ IDC_SETTINGS_PUBLIC_HUB_LIST, ResourceManager::SETTINGS_PUBLIC_HUB_LIST },
	{ IDC_SETTINGS_PUBLIC_HUB_LIST_URL, ResourceManager::SETTINGS_PUBLIC_HUB_LIST_URL },
	{ IDC_SETTINGS_LIST_CONFIG, ResourceManager::SETTINGS_CONFIGURE_HUB_LISTS },
	{ IDC_BITTORRENT_PROGRAMM, ResourceManager::SETTINGS_BITTORRENT_PROGRAMM },
	{ IDC_BT_HELP, ResourceManager::SETTINGS_BT_HELP },
	//{ IDC_BROWSE_BT_PROGRAMM, ResourceManager::BROWSE }, //[+] IRainman: BitTorrent
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item DownloadPage::items[] =
{
	{ IDC_TEMP_DOWNLOAD_DIRECTORY, SettingsManager::TEMP_DOWNLOAD_DIRECTORY, PropPage::T_STR },
	{ IDC_DOWNLOADDIR,  SettingsManager::DOWNLOAD_DIRECTORY, PropPage::T_STR },
	{ IDC_DOWNLOADS, SettingsManager::DOWNLOAD_SLOTS, PropPage::T_INT },
	{ IDC_FILES, SettingsManager::FILE_SLOTS, PropPage::T_INT },
	{ IDC_MAXSPEED, SettingsManager::MAX_DOWNLOAD_SPEED, PropPage::T_INT },
	{ IDC_EXTRA_DOWN_SLOT, SettingsManager::EXTRA_DOWNLOAD_SLOTS, PropPage::T_INT },
	{ IDC_BT_MAGNET_HANDLER, SettingsManager::BT_MAGNET_OPEN_CMD, PropPage::T_STR }, //[+] IRainman: BitTorrent
	{ 0, 0, PropPage::T_END }
};

LRESULT DownloadPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	
	CUpDownCtrl spin;
	spin.Attach(GetDlgItem(IDC_FILESPIN));
	spin.SetRange32(0, 100);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_SLOTSSPIN));
	spin.SetRange32(0, 100);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_SPEEDSPIN));
	spin.SetRange32(0, 10000);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_EXTRASLOTSSPIN));
	spin.SetRange32(0, 100);
	spin.Detach();
	// Do specialized reading here
	return TRUE;
}

void DownloadPage::write()
{
	PropPage::write((HWND)*this, items);
}

LRESULT DownloadPage::onClickedBrowseDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
	if (WinUtil::browseDirectory(dir, m_hWnd))
	{
		AppendPathSeparator(dir);
		SetDlgItemText(IDC_DOWNLOADDIR, dir.c_str());
	}
	return 0;
}

LRESULT DownloadPage::onClickedBrowseTempDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir = Text::toT(SETTING(TEMP_DOWNLOAD_DIRECTORY));
	if (WinUtil::browseDirectory(dir, m_hWnd))
	{
		AppendPathSeparator(dir);
		SetDlgItemText(IDC_TEMP_DOWNLOAD_DIRECTORY, dir.c_str());
	}
	return 0;
}

//[+] SCALOlaz: BitTorrent Browse.
static const TCHAR types[] = _T("BitTorrent application\0*.exe\0\0"); // TODO translate
static const TCHAR defExt[] = _T(".exe");

LRESULT DownloadPage::onClickedBrowseBT(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring xbt;
	GET_TEXT(IDC_BT_MAGNET_HANDLER, xbt);
	if (WinUtil::browseFile(xbt, m_hWnd, false, xbt, types, defExt) == IDOK)
	{
		SetDlgItemText(IDC_BT_MAGNET_HANDLER, xbt.c_str());
	}
	return 0;
}

LRESULT DownloadPage::onClickedListConfigure(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PublicHubListDlg dlg;
	dlg.DoModal(m_hWnd);
	return 0;
}

/**
 * @file
 * $Id: DownloadPage.cpp,v 1.14 2006/05/08 08:36:19 bigmuscle Exp $
 */
