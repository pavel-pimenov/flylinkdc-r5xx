/*
 * Copyright (C) 2012-2013 FlylinkDC++ Team http://flylinkdc.com/
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

#include "UpdatePage.h"
#include "CommandDlg.h"

#include "../FlyFeatures/AutoUpdate.h"


PropPage::TextItem UpdatePage::texts[] =
{
	{ IDC_AUTOUPDATE_USE, ResourceManager::AUTOUPDATE_ENABLE},
	{ IDC_AUTOUPDATE_RUNONSTARTUP, ResourceManager::AUTOUPDATE_RUNONSTARTUP},
	{ IDC_AUTOUPDATE_STARTATTIME, ResourceManager::AUTOUPDATE_STARTATTIME},
	{ IDC_AUTOUPDATE_URL_LABEL, ResourceManager::AUTOUPDATE_URL_LABEL},
	{ IDC_AUTOUPDATE_SCHEDULE_LABEL, ResourceManager::AUTOUPDATE_SCHEDULE_LABEL},
	{ IDC_AUTOUPDATE_COMPONENT_LABEL, ResourceManager::AUTOUPDATE_COMPONENT_LABEL},
	{ IDC_AUTOUPDATE_SERVER_BETA, ResourceManager::AUTOUPDATE_TO_BETA_SERV },
	{ IDC_AUTOUPDATE_USE_CUSTOM_SERVER, ResourceManager::AUTOUPDATE_USE_CUSTOM_URL },
// End of Addition.
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item UpdatePage::items[] =
{
	{ IDC_AUTOUPDATE_USE, SettingsManager::AUTOUPDATE_ENABLE, PropPage::T_BOOL},
	{ IDC_AUTOUPDATE_RUNONSTARTUP, SettingsManager::AUTOUPDATE_RUNONSTARTUP, PropPage::T_BOOL},
	{ IDC_AUTOUPDATE_STARTATTIME, SettingsManager::AUTOUPDATE_STARTATTIME, PropPage::T_BOOL},
	{ IDC_AUTOUPDATE_SERVER_BETA, SettingsManager::AUTOUPDATE_TO_BETA, PropPage::T_BOOL },
	{ IDC_AUTOUPDATE_URL, SettingsManager::AUTOUPDATE_SERVER_URL, PropPage::T_STR},
	{ IDC_AUTOUPDATE_USE_CUSTOM_SERVER, SettingsManager::AUTOUPDATE_USE_CUSTOM_URL, PropPage::T_BOOL},
// End of Addition.
	{ 0, 0, PropPage::T_END }
};

UpdatePage::ListItem UpdatePage::listItems[] =
{
	{ SettingsManager::AUTOUPDATE_SHOWUPDATEREADY, ResourceManager::AUTOUPDATE_SHOWUPDATEREADY}, // [+] SSA
	{ SettingsManager::AUTOUPDATE_FORCE_RESTART, ResourceManager::AUTOUPDATE_FORCE_RESTART}, // [+] SSA
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

UpdatePage::ListItem UpdatePage::listComponents[] =
{
	{ SettingsManager::AUTOUPDATE_EXE, ResourceManager::AUTOUPDATE_EXE}, // [+] SSA
	{ SettingsManager::AUTOUPDATE_UTILITIES, ResourceManager::AUTOUPDATE_UTILITIES}, // [+] SSA
	{ SettingsManager::AUTOUPDATE_LANG, ResourceManager::AUTOUPDATE_LANG}, // [+] SSA
	{ SettingsManager::AUTOUPDATE_PORTALBROWSER, ResourceManager::AUTOUPDATE_PORTALBROWSER}, // [+] SSA
#ifdef IRAINMAN_INCLUDE_SMILE
	{ SettingsManager::AUTOUPDATE_EMOPACKS, ResourceManager::AUTOUPDATE_EMOPACKS}, // [+] SSA
#endif
	{ SettingsManager::AUTOUPDATE_WEBSERVER, ResourceManager::WEBSERVER}, // [+] SSA
	// { SettingsManager::AUTOUPDATE_UPDATE_CHATBOT, ResourceManager::AUTOUPDATE_UPDATE_CHATBOT}, // [+] SSA - we don't update chat bot
	{ SettingsManager::AUTOUPDATE_SOUNDS, ResourceManager::AUTOUPDATE_SOUNDS}, // [+] SSA
	{ SettingsManager::AUTOUPDATE_ICONTHEMES, ResourceManager::AUTOUPDATE_ICONTHEMES}, // [+] SSA
	{ SettingsManager::AUTOUPDATE_COLORTHEMES, ResourceManager::AUTOUPDATE_COLORTHEMES}, // [+] SSA
	{ SettingsManager::AUTOUPDATE_DOCUMENTATION, ResourceManager::AUTOUPDATE_DOCUMENTATION}, // [+] SSA
#ifdef IRAINMAN_AUTOUPDATE_ALL_USERS_DATA
	{ SettingsManager::AUTOUPDATE_GEOIP, ResourceManager::AUTOUPDATE_GEOIP}, // [+] IRainman
	{ SettingsManager::AUTOUPDATE_CUSTOMLOCATION, ResourceManager::AUTOUPDATE_CUSTOMLOCATION}, // [+] IRainman
#endif
#ifdef SSA_SHELL_INTEGRATION
	{ SettingsManager::AUTOUPDATE_SHELL_EXT, ResourceManager::AUTOUPDATE_SHELL_EXT}, // [+] IRainman
#endif
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT UpdatePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
#ifdef AUTOUPDATE_NOT_DISABLE
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_USE), FALSE);
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_RUNONSTARTUP), FALSE);
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_STARTATTIME), FALSE);
#endif
#ifdef HOURLY_CHECK_UPDATE
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_AT), FALSE);
#endif
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items, listItems, GetDlgItem(IDC_AUTOUPDATE_LIST));
	PropPage::read((HWND)*this, NULL, listComponents, GetDlgItem(IDC_AUTOUPDATE_COMPONENTS));
	
	ctrlComponents.Attach(GetDlgItem(IDC_AUTOUPDATE_LIST)); // [+] IRainman
	ctrlAutoupdates.Attach(GetDlgItem(IDC_AUTOUPDATE_COMPONENTS)); // [+] IRainman
	
	
	ctrlTime.Attach(GetDlgItem(IDC_AUTOUPDATE_AT));
	WinUtil::GetTimeValues(ctrlTime);
#ifdef HOURLY_CHECK_UPDATE
	ctrlTime.AddString(CTSTRING(AUTOUPDATE_HOURLY));
#endif // HOURLY_CHECK_UPDATE
	ctrlTime.SetCurSel(SETTING(AUTOUPDATE_TIME));
	ctrlTime.Detach();
	
#ifdef AUTOUPDATE_NOT_DISABLE
	EnableAutoUpdate(TRUE);
#else
	EnableAutoUpdate(BOOLSETTING(AUTOUPDATE_ENABLE));
#endif
	// [+] SSA
	CheckUseCustomURL();
	// Do specialized reading here
	return TRUE;
}

void UpdatePage::write()
{
	PropPage::write((HWND)*this, items, listItems, GetDlgItem(IDC_AUTOUPDATE_LIST));
	PropPage::write((HWND)*this, NULL, listComponents, GetDlgItem(IDC_AUTOUPDATE_COMPONENTS));
	ctrlTime.Attach(GetDlgItem(IDC_AUTOUPDATE_AT));
	SET_SETTING(AUTOUPDATE_TIME, ctrlTime.GetCurSel());
	ctrlTime.Detach();
}

#ifndef AUTOUPDATE_NOT_DISABLE
LRESULT UpdatePage::onClickedUseAutoUpdate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EnableAutoUpdate(IsDlgButtonChecked(IDC_AUTOUPDATE_USE) == BST_CHECKED);
	
	return 0;
}
#endif
void UpdatePage::EnableAutoUpdate(BOOL isEnabled)
{
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_URL_LABEL), isEnabled);
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_SERVER_BETA), isEnabled);
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_SCHEDULE_LABEL), isEnabled);
#ifndef AUTOUPDATE_NOT_DISABLE
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_RUNONSTARTUP), isEnabled);
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_STARTATTIME), isEnabled);
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_AT), isEnabled);
#endif
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_LIST), isEnabled);
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_COMPONENT_LABEL), isEnabled);
	::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_COMPONENTS), isEnabled);
}

LRESULT UpdatePage::onClickedUseCustomURL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CheckUseCustomURL();
	return 0;
}

void UpdatePage::CheckUseCustomURL()
{
#ifndef AUTOUPDATE_NOT_DISABLE
	if (IsDlgButtonChecked(IDC_AUTOUPDATE_USE) == BST_CHECKED)
	{
#endif
		::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_URL), IsDlgButtonChecked(IDC_AUTOUPDATE_USE_CUSTOM_SERVER) == BST_CHECKED);
		::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_SERVER_BETA), IsDlgButtonChecked(IDC_AUTOUPDATE_USE_CUSTOM_SERVER) != BST_CHECKED);
#ifndef AUTOUPDATE_NOT_DISABLE
	}
	else
	{
		::EnableWindow(GetDlgItem(IDC_AUTOUPDATE_URL), FALSE);
	}
#endif
}