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

#include "DefaultClickPage.h"

PropPage::TextItem DefaultClickPage::texts[] =
{
	{ IDC_DOUBLE_CLICK_ACTION, ResourceManager::DOUBLE_CLICK_ACTION },
	{ IDC_USERLISTDBLCLICKACTION, ResourceManager::USERLISTDBLCLICKACTION },
	{ IDC_FAVUSERLISTDBLCLICKACTION, ResourceManager::FAVUSERLISTDBLCLICKACTION },
	{ IDC_TRANSFERLISTDBLCLICKACTION, ResourceManager::TRANSFERLISTDBLCLICKACTION },
	{ IDC_CHATDBLCLICKACTION, ResourceManager::CHATDBLCLICKACTION },
	{ IDC_MAGNETURLCLICKACTION, ResourceManager::MAGNETURLCLICKACTION },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
	
};

//PropPage::Item DefaultClickPage::items[] =
//{
//
//};

LRESULT DefaultClickPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
//	PropPage::read(*this, items);

	// Do specialized reading here
	userlistaction.Attach(GetDlgItem(IDC_USERLIST_DBLCLICK));
	transferlistaction.Attach(GetDlgItem(IDC_TRANSFERLIST_DBLCLICK));
	chataction.Attach(GetDlgItem(IDC_CHAT_DBLCLICK));
	
	
	favuserlistaction.Attach(GetDlgItem(IDC_FAVUSERLIST_DBLCLICK));
	favuserlistaction.AddString(CTSTRING(GET_FILE_LIST));
	favuserlistaction.AddString(CTSTRING(SEND_PRIVATE_MESSAGE));
	favuserlistaction.AddString(CTSTRING(MATCH_QUEUE));
	favuserlistaction.AddString(CTSTRING(PROPERTIES));
	favuserlistaction.AddString(CTSTRING(OPEN_USER_LOG));
	favuserlistaction.SetCurSel(SETTING(FAVUSERLIST_DBLCLICK));
	favuserlistaction.Detach();
	
	userlistaction.AddString(CTSTRING(GET_FILE_LIST));
	userlistaction.AddString(CTSTRING(ADD_NICK_TO_CHAT));
	userlistaction.AddString(CTSTRING(SEND_PRIVATE_MESSAGE));
	userlistaction.AddString(CTSTRING(MATCH_QUEUE));
	userlistaction.AddString(CTSTRING(GRANT_EXTRA_SLOT));
	userlistaction.AddString(CTSTRING(ADD_TO_FAVORITES));
	userlistaction.AddString(CTSTRING(BROWSE_FILE_LIST));
	transferlistaction.AddString(CTSTRING(SEND_PRIVATE_MESSAGE));
	transferlistaction.AddString(CTSTRING(GET_FILE_LIST));
	transferlistaction.AddString(CTSTRING(MATCH_QUEUE));
	transferlistaction.AddString(CTSTRING(GRANT_EXTRA_SLOT));
	transferlistaction.AddString(CTSTRING(ADD_TO_FAVORITES));
	transferlistaction.AddString(CTSTRING(FORCE_ATTEMPT));
	transferlistaction.AddString(CTSTRING(BROWSE_FILE_LIST));
	chataction.AddString(CTSTRING(SELECT_USER_LIST));
	chataction.AddString(CTSTRING(ADD_NICK_TO_CHAT));
	chataction.AddString(CTSTRING(SEND_PRIVATE_MESSAGE));
	chataction.AddString(CTSTRING(GET_FILE_LIST));
	chataction.AddString(CTSTRING(MATCH_QUEUE));
	chataction.AddString(CTSTRING(GRANT_EXTRA_SLOT));
	chataction.AddString(CTSTRING(ADD_TO_FAVORITES));
	
	userlistaction.SetCurSel(SETTING(USERLIST_DBLCLICK));
	transferlistaction.SetCurSel(SETTING(TRANSFERLIST_DBLCLICK));
	chataction.SetCurSel(SETTING(CHAT_DBLCLICK));
	
	userlistaction.Detach();
	transferlistaction.Detach();
	chataction.Detach();
	
	magneturllistaction.Attach(GetDlgItem(IDC_MAGNETURLLIST_CLICK));
	magneturllistaction.AddString(CTSTRING(ASK));
	magneturllistaction.AddString(CTSTRING(SEARCH));
	magneturllistaction.AddString(CTSTRING(DOWNLOAD));
	
	if (SETTING(MAGNET_ASK) == 1)
	{
		magneturllistaction.SetCurSel(0);
	}
	else
	{
		if (SETTING(MAGNET_ACTION) == 0)
		{
			magneturllistaction.SetCurSel(1);
		}
		else
		{
			magneturllistaction.SetCurSel(2);
		}
	}
	
	magneturllistaction.Detach();
	
	return TRUE;
}

void DefaultClickPage::write()
{
//	PropPage::write(*this, items);

	userlistaction.Attach(GetDlgItem(IDC_USERLIST_DBLCLICK));
	transferlistaction.Attach(GetDlgItem(IDC_TRANSFERLIST_DBLCLICK));
	chataction.Attach(GetDlgItem(IDC_CHAT_DBLCLICK));
	g_settings->set(SettingsManager::USERLIST_DBLCLICK, userlistaction.GetCurSel());
	g_settings->set(SettingsManager::TRANSFERLIST_DBLCLICK, transferlistaction.GetCurSel());
	g_settings->set(SettingsManager::CHAT_DBLCLICK, chataction.GetCurSel());
	userlistaction.Detach();
	transferlistaction.Detach();
	chataction.Detach();
	
	
	favuserlistaction.Attach(GetDlgItem(IDC_FAVUSERLIST_DBLCLICK));
	g_settings->set(SettingsManager::FAVUSERLIST_DBLCLICK, favuserlistaction.GetCurSel());
	favuserlistaction.Detach();
	
	magneturllistaction.Attach(GetDlgItem(IDC_MAGNETURLLIST_CLICK));
	if (magneturllistaction.GetCurSel() == 0)
	{
		g_settings->set(SettingsManager::MAGNET_ASK, true);
	}
	else
	{
		g_settings->set(SettingsManager::MAGNET_ASK, false);
		if (magneturllistaction.GetCurSel() == 1)
		{
			g_settings->set(SettingsManager::MAGNET_ACTION, false);
		}
		else
		{
			g_settings->set(SettingsManager::MAGNET_ACTION, true);
		}
	}
	
}

/**
 * @file
 * $Id: DownloadPage.cpp,v 1.14 2010/07/12 12:30:26 NightOrion
 */
