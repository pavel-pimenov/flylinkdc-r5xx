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

#include "Resource.h"

#include "TabsPage.h"

// Настройки.
PropPage::Item TabsPage::items[] =
{
	{ IDC_MAX_TAB_ROWS, SettingsManager::MAX_TAB_ROWS, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

// Перевод элементов настроек.
PropPage::TextItem TabsPage::textItem[] =
{
	{ IDC_SETTINGS_TABS_OPTIONS, ResourceManager::SETTINGS_TABS_OPTIONS },
	{ IDC_SETTINGS_BOLD_CONTENTS, ResourceManager::SETTINGS_BOLD_OPTIONS },
	{ IDC_TABSTEXT, ResourceManager::TABS_POSITION },
	{ IDC_SETTINGS_MAX_TAB_ROWS, ResourceManager::SETTINGS_MAX_TAB_ROWS },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

// Опции вкладок.
PropPage::ListItem TabsPage::optionItems[] =
{
	{ SettingsManager::NON_HUBS_FRONT, ResourceManager::NON_HUBS_FRONT },
	{ SettingsManager::TABS_CLOSEBUTTONS, ResourceManager::TABS_CLOSEBUTTONS },
	{ SettingsManager::STRIP_TOPIC, ResourceManager::SETTINGS_STRIP_TOPIC },    //AdvancedPage
	{ SettingsManager::SHOW_FULL_HUB_INFO_ON_TAB, ResourceManager::SETTINGS_SHOW_FULL_HUB_INFO_ON_TAB }, // [+] NightOrion.
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

// Выделение вкладок при изменениях.
PropPage::ListItem TabsPage::boldItems[] =
{
	{ SettingsManager::BOLD_FINISHED_DOWNLOADS, ResourceManager::FINISHED_DOWNLOADS },
	{ SettingsManager::BOLD_FINISHED_UPLOADS, ResourceManager::FINISHED_UPLOADS },
	{ SettingsManager::BOLD_QUEUE, ResourceManager::DOWNLOAD_QUEUE },
	{ SettingsManager::BOLD_HUB, ResourceManager::HUB },
	{ SettingsManager::BOLD_PM, ResourceManager::PRIVATE_MESSAGE },
	{ SettingsManager::BOLD_SEARCH, ResourceManager::SEARCH },
	{ SettingsManager::BOLD_WAITING_USERS, ResourceManager::WAITING_USERS },
#ifdef IRAINMAN_INCLUDE_RSS
	{ SettingsManager::BOLD_NEWRSS, ResourceManager::NEW_RSS_NEWS},
#endif
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

// При инициализации диалога.
LRESULT TabsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), textItem);
	PropPage::read(*this, items, optionItems, GetDlgItem(IDC_TABS_OPTIONS));
	PropPage::read(*this, items, boldItems, GetDlgItem(IDC_BOLD_BOOLEANS));
	
	ctrlOption.Attach(GetDlgItem(IDC_TABS_OPTIONS)); // [+] IRainman
	ctrlBold.Attach(GetDlgItem(IDC_BOLD_BOOLEANS)); // [+] IRainman
	
	// Расположение вкладок.
	// Do specialized reading here
	CUpDownCtrl updown;
	updown.Attach(GetDlgItem(IDC_TAB_SPIN));
	updown.SetRange32(1, 20);
	updown.Detach();
	
	CComboBox tabsPosition;
	tabsPosition.Attach(GetDlgItem(IDC_TABSCOMBO));
	tabsPosition.AddString(CTSTRING(TABS_TOP));
	tabsPosition.AddString(CTSTRING(TABS_BOTTOM));
	tabsPosition.AddString(CTSTRING(TABS_LEFT));
	tabsPosition.AddString(CTSTRING(TABS_RIGHT));
	tabsPosition.SetCurSel(SETTING(TABS_POS));
	tabsPosition.Detach();
	
	return TRUE;
}

void TabsPage::write()
{
	PropPage::write(*this, items, optionItems, GetDlgItem(IDC_TABS_OPTIONS));
	PropPage::write(*this, items, boldItems, GetDlgItem(IDC_BOLD_BOOLEANS));
	
	CComboBox tabsPosition;
	tabsPosition.Attach(GetDlgItem(IDC_TABSCOMBO));
	g_settings->set(SettingsManager::TABS_POS, tabsPosition.GetCurSel());
	tabsPosition.Detach();
}