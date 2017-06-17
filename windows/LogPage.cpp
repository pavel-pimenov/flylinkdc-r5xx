/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#include "../client/LogManager.h"
#include "../client/File.h"
#include "Resource.h"
#include "LogPage.h"
#include "WinUtil.h"


PropPage::TextItem LogPage::texts[] =
{
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
	{ IDC_FLY_LOG_SQLITE,                    ResourceManager::SETTINGS_USE_SQLITE_FOR_LOGS },
	{ IDC_FLY_LOG_TEXT,                      ResourceManager::SETTINGS_USE_TEXT_FOR_LOGS },
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
	{ IDC_SETTINGS_LOGGING,                  ResourceManager::SETTINGS_LOGGING },
	{ IDC_SETTINGS_LOG_DIR,                  ResourceManager::DIRECTORY },
	{ IDC_SETTINGS_FORMAT,                   ResourceManager::SETTINGS_FORMAT },
	{ IDC_WRITE_LOGS,                        ResourceManager::SETTINGS_LOGS },
	{ IDC_SETTINGS_MAX_FINISHED_UPLOADS_L,   ResourceManager::MAX_FINISHED_UPLOADS },
	{ IDC_SETTINGS_MAX_FINISHED_DOWNLOADS_L, ResourceManager::MAX_FINISHED_DOWNLOADS },
	{ IDC_SETTINGS_DB_LOG_FINISHED_UPLOADS_L, ResourceManager::DB_LOG_FINISHED_UPLOADS },
	{ IDC_SETTINGS_DB_LOG_FINISHED_DOWNLOADS_L, ResourceManager::DB_LOG_FINISHED_DOWNLOADS },
	{ IDC_SETTINGS_FILE_NAME, ResourceManager::SETTINGS_FILE_NAME },
	{ 0,                                     ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item LogPage::items[] =
{
	{ IDC_LOG_DIRECTORY,                   SettingsManager::LOG_DIRECTORY,          PropPage::T_STR },
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
	{ IDC_FLY_LOG_SQLITE,                  SettingsManager::FLY_SQLITE_LOG,         PropPage::T_BOOL },
	{ IDC_FLY_LOG_TEXT,                    SettingsManager::FLY_TEXT_LOG,           PropPage::T_BOOL },
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
	{ IDC_SETTINGS_MAX_FINISHED_UPLOADS,   SettingsManager::MAX_FINISHED_UPLOADS,   PropPage::T_INT },
	{ IDC_SETTINGS_MAX_FINISHED_DOWNLOADS, SettingsManager::MAX_FINISHED_DOWNLOADS, PropPage::T_INT },
	{ IDC_SETTINGS_DB_LOG_FINISHED_UPLOADS, SettingsManager::DB_LOG_FINISHED_UPLOADS, PropPage::T_INT },
	{ IDC_SETTINGS_DB_LOG_FINISHED_DOWNLOADS, SettingsManager::DB_LOG_FINISHED_DOWNLOADS, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

PropPage::ListItem LogPage::listItems[] =
{
	{ SettingsManager::LOG_MAIN_CHAT,           ResourceManager::SETTINGS_LOG_MAIN_CHAT },
	{ SettingsManager::LOG_PRIVATE_CHAT,        ResourceManager::SETTINGS_LOG_PRIVATE_CHAT },
	{ SettingsManager::LOG_DOWNLOADS,           ResourceManager::SETTINGS_LOG_DOWNLOADS },
	{ SettingsManager::LOG_UPLOADS,             ResourceManager::SETTINGS_LOG_UPLOADS },
	{ SettingsManager::LOG_SYSTEM,              ResourceManager::SETTINGS_LOG_SYSTEM_MESSAGES },
	{ SettingsManager::LOG_STATUS_MESSAGES,     ResourceManager::SETTINGS_LOG_STATUS_MESSAGES },
	{ SettingsManager::LOG_WEBSERVER,           ResourceManager::SETTINGS_LOG_WEBSERVER },
#ifdef RIP_USE_LOG_PROTOCOL
	{ SettingsManager::LOG_PROTOCOL,            ResourceManager::SETTINGS_LOG_PROTOCOL },
#endif
	{ SettingsManager::LOG_CUSTOM_LOCATION,     ResourceManager::SETTINGS_LOG_CUSTOM_LOCATION }, // [+]IRainman
	{ SettingsManager::LOG_SQLITE_TRACE,        ResourceManager::SETTINGS_LOG_TRACE_SQLITE },
	{ SettingsManager::LOG_VIRUS_TRACE, ResourceManager::SETTINGS_LOG_VIRUS_TRACE },
	{ SettingsManager::LOG_DDOS_TRACE,          ResourceManager::SETTINGS_LOG_DDOS_TRACE },
	{ SettingsManager::LOG_CMDDEBUG_TRACE,          ResourceManager::SETTINGS_LOG_CMDDEBUG_TRACE },
#ifdef FLYLINKDC_USE_TORRENT
	{ SettingsManager::LOG_TORRENT_TRACE,           ResourceManager::SETTINGS_LOG_TORRENT_TRACE },
#endif
	{ SettingsManager::LOG_PSR_TRACE,           ResourceManager::SETTINGS_LOG_PSR_TRACE },
	{ SettingsManager::LOG_FLOOD_TRACE,           ResourceManager::SETTINGS_LOG_FLOOD_TRACE },
	{ SettingsManager::LOG_FILELIST_TRANSFERS,  ResourceManager::SETTINGS_LOG_FILELIST_TRANSFERS },
	{ SettingsManager::LOG_IF_SUPPRESS_PMS,     ResourceManager::SETTINGS_LOG_IF_SUPPRESS_PMS }, // [+]IRainman
	{ 0,                                        ResourceManager::SETTINGS_AUTO_AWAY }
};


LRESULT LogPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_LOG_OPTIONS));
	
	for (int i = 0; i < LogManager::LAST; ++i)
	{
		TStringPair pair;
		pair.first = Text::toT(LogManager::getSetting(i, LogManager::FILE));
		pair.second = Text::toT(LogManager::getSetting(i, LogManager::FORMAT));
		options.push_back(pair);
	}
	
	::EnableWindow(GetDlgItem(IDC_LOG_FORMAT), false);
	::EnableWindow(GetDlgItem(IDC_LOG_FILE), false);
#ifdef FLYLINKDC_USE_ROTATION_FINISHED_MANAGER
	// For fix - crash https://drdump.com/DumpGroup.aspx?DumpGroupID=301739
	::EnableWindow(GetDlgItem(IDC_SETTINGS_MAX_FINISHED_UPLOADS), false);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_MAX_FINISHED_DOWNLOADS), false);
#endif
	setEnabled();
	oldSelection = -1;
	
	// Do specialized reading here
	return TRUE;
}


LRESULT LogPage::onCheckTypeLog(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& bHandled)
{
	bHandled = FALSE;
	setEnabled();
	return 0;
}
void LogPage::setEnabled()
{
	logOptions.Attach(GetDlgItem(IDC_LOG_OPTIONS));
	
	getValues();
	
	int sel = logOptions.GetSelectedIndex();
	
	if (sel >= 0 && sel < LogManager::LAST)
	{
		BOOL checkState = (logOptions.GetCheckState(sel) == BST_CHECKED);
		
		::EnableWindow(GetDlgItem(IDC_LOG_FORMAT), checkState);
		::EnableWindow(GetDlgItem(IDC_LOG_FILE), checkState
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
		               && (IsDlgButtonChecked(IDC_FLY_LOG_TEXT) == BST_CHECKED)
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
		              );
		              
		SetDlgItemText(IDC_LOG_FILE, options[sel].first.c_str());
		SetDlgItemText(IDC_LOG_FORMAT, options[sel].second.c_str());
		
		//save the old selection so we know where to save the values
		oldSelection = sel;
	}
	else
	{
		::EnableWindow(GetDlgItem(IDC_LOG_FORMAT), FALSE);
		::EnableWindow(GetDlgItem(IDC_LOG_FILE), FALSE);
		
		SetDlgItemText(IDC_LOG_FILE, _T(""));
		SetDlgItemText(IDC_LOG_FORMAT, _T(""));
	}
	
	logOptions.Detach();
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
	::EnableWindow(GetDlgItem(IDC_LOG_DIRECTORY), IsDlgButtonChecked(IDC_FLY_LOG_SQLITE) == BST_UNCHECKED);
	::EnableWindow(GetDlgItem(IDC_BROWSE_LOG), IsDlgButtonChecked(IDC_FLY_LOG_SQLITE) == BST_UNCHECKED);
#else
	::EnableWindow(GetDlgItem(IDC_LOG_DIRECTORY), TRUE);
	::EnableWindow(GetDlgItem(IDC_BROWSE_LOG), TRUE);
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
}

LRESULT LogPage::onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	setEnabled();
	return 0;
}

void LogPage::getValues()
{
	if (oldSelection >= 0)
	{
		tstring buf;
		
		GET_TEXT(IDC_LOG_FILE, buf);
		if (!buf.empty())
			std::swap(options[oldSelection].first, buf);
		GET_TEXT(IDC_LOG_FORMAT, buf);
		if (!buf.empty())
			std::swap(options[oldSelection].second, buf);
	}
}

void LogPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_LOG_OPTIONS));
	
	File::ensureDirectory(SETTING(LOG_DIRECTORY));
	
	//make sure we save the last edit too, the user
	//might not have changed the selection
	getValues();
	
	for (int i = 0; i < LogManager::LAST; ++i)
	{
		string tmp = Text::fromT(options[i].first);
		if (stricmp(Util::getFileExt(tmp), ".log") != 0)
			tmp += ".log";
			
		LogManager::saveSetting(i, LogManager::FILE, tmp);
		LogManager::saveSetting(i, LogManager::FORMAT, Text::fromT(options[i].second));
	}
}

LRESULT LogPage::onClickedBrowseDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring dir = Text::toT(SETTING(LOG_DIRECTORY));
	if (WinUtil::browseDirectory(dir, m_hWnd))
	{
		AppendPathSeparator(dir);
		SetDlgItemText(IDC_LOG_DIRECTORY, dir.c_str());
	}
	return 0;
}

/**
 * @file
 * $Id: LogPage.cpp,v 1.6 2006/09/23 19:24:39 bigmuscle Exp $
 */
