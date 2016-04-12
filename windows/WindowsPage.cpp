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

#include "Resource.h"
#include "WindowsPage.h"
#ifdef RIP_USE_PORTAL_BROWSER
#include "PortalBrowser.h"
#endif

PropPage::Item WindowsPage::items[] =
{
	{ 0, 0, PropPage::T_END }
};

// Блоки настроек.
PropPage::TextItem WindowsPage::textItem[] =
{
	{ IDC_SETTINGS_AUTO_OPEN, ResourceManager::SETTINGS_AUTO_OPEN },
	{ IDC_SETTINGS_WINDOWS_OPTIONS, ResourceManager::SETTINGS_WINDOWS_OPTIONS },
	{ IDC_SETTINGS_CONFIRM_OPTIONS, ResourceManager::SETTINGS_CONFIRM_DIALOG_OPTIONS }, // [+] InfinitySky.
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

// Открывать при запуске.
WindowsPage::ListItem WindowsPage::listItems[] =
{
	{ SettingsManager::OPEN_RECENT_HUBS, ResourceManager::LAST_RECENT_HUBS },
	{ SettingsManager::OPEN_FAVORITE_HUBS, ResourceManager::FAVORITE_HUBS },
	{ SettingsManager::OPEN_FAVORITE_USERS, ResourceManager::FAVORITE_USERS },
	{ SettingsManager::OPEN_QUEUE, ResourceManager::DOWNLOAD_QUEUE },
	{ SettingsManager::OPEN_FINISHED_DOWNLOADS, ResourceManager::FINISHED_DOWNLOADS },
	{ SettingsManager::OPEN_WAITING_USERS, ResourceManager::WAITING_USERS },
	{ SettingsManager::OPEN_FINISHED_UPLOADS, ResourceManager::FINISHED_UPLOADS },
	{ SettingsManager::OPEN_NETWORK_STATISTICS, ResourceManager::NETWORK_STATISTICS },
	{ SettingsManager::OPEN_NOTEPAD, ResourceManager::NOTEPAD },
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
	{ SettingsManager::OPEN_CDMDEBUG, ResourceManager::MENU_CDMDEBUG_MESSAGES },
#endif
#ifdef IRAINMAN_INCLUDE_RSS
	{ SettingsManager::OPEN_RSS, ResourceManager::RSS_NEWS }, // [+] SSA
#endif
	{ SettingsManager::OPEN_SEARCH_SPY, ResourceManager::SEARCH_SPY },
#ifdef RIP_USE_PORTAL_BROWSER
	// NB: MUST be last checkbox in list!!!
	{ SettingsManager::OPEN_PORTAL_BROWSER, ResourceManager::PORTAL_BROWSER }, // [+] BRAIN_RIPPER
#endif
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

// Настройки окон.
WindowsPage::ListItem WindowsPage::optionItems[] =
{
	{ SettingsManager::POPUP_PMS, ResourceManager::SETTINGS_POPUP_PMS },
	{ SettingsManager::POPUP_HUB_PMS, ResourceManager::SETTINGS_POPUP_HUB_PMS },
	{ SettingsManager::POPUP_BOT_PMS, ResourceManager::SETTINGS_POPUP_BOT_PMS },
	{ SettingsManager::POPUNDER_FILELIST, ResourceManager::SETTINGS_POPUNDER_FILELIST },
	{ SettingsManager::POPUNDER_PM, ResourceManager::SETTINGS_POPUNDER_PM },
	{ SettingsManager::JOIN_OPEN_NEW_WINDOW, ResourceManager::SETTINGS_OPEN_NEW_WINDOW },
	{ SettingsManager::TOGGLE_ACTIVE_WINDOW, ResourceManager::SETTINGS_TOGGLE_ACTIVE_WINDOW },
	{ SettingsManager::PROMPT_PASSWORD, ResourceManager::SETTINGS_PROMPT_PASSWORD },
	{ SettingsManager::REMEMBER_SETTINGS_PAGE, ResourceManager::REMEMBER_SETTINGS_PAGE }, // [<-] InfinitySky. Запоминать положение окна настроек.
	{ SettingsManager::REMEMBER_SETTINGS_WINDOW_POS, ResourceManager::REMEMBER_SETTINGS_WINDOW_POS },
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
	{ SettingsManager::SETTINGS_WINDOW_TRANSP, ResourceManager::SETTINGS_WINDOW_TRANSP },
#endif
#ifdef SCALOLAZ_PROPPAGE_COLOR
	{ SettingsManager::SETTINGS_WINDOW_COLORIZE, ResourceManager::SETTINGS_WINDOW_COLORIZE },
#endif
#ifdef SCALOLAZ_PROPPAGE_HELPLINK
	{ SettingsManager::SETTINGS_WINDOW_WIKIHELP, ResourceManager::SETTINGS_WINDOW_WIKIHELP },
#endif
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

// [+] InfinitySky. Подтверждения.
WindowsPage::ListItem WindowsPage::confirmItems[] =
{
	{ SettingsManager::CONFIRM_EXIT, ResourceManager::SETTINGS_CONFIRM_EXIT },
	{ SettingsManager::CONFIRM_HUB_REMOVAL, ResourceManager::SETTINGS_CONFIRM_HUB_REMOVAL },
	{ SettingsManager::CONFIRM_HUBGROUP_REMOVAL, ResourceManager::SETTINGS_CONFIRM_HUBGROUP_REMOVAL }, // [+] NightOrion
	{ SettingsManager::CONFIRM_DELETE, ResourceManager::SETTINGS_CONFIRM_ITEM_REMOVAL },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

// При инициализации диалога.
LRESULT WindowsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
#ifdef RIP_USE_PORTAL_BROWSER
	_ASSERTE(_countof(listItems) >= 2);
	_ASSERTE(listItems[_countof(listItems) - 2].setting == SettingsManager::OPEN_PORTAL_BROWSER); // TODO Crash!!!!
	
	// Hide PortalBrowser checkbox if there is no visible Portals
	if (_countof(listItems) >= 2 && !IfHaveVisiblePortals())
	{
		listItems[_countof(listItems) - 2].setting = 0;
		listItems[_countof(listItems) - 2].desc = ResourceManager::SETTINGS_AUTO_AWAY;
	}
#endif
	
	PropPage::translate((HWND)(*this), textItem);
	PropPage::read((HWND)*this, items, listItems, GetDlgItem(IDC_WINDOWS_STARTUP));
	PropPage::read((HWND)*this, items, optionItems, GetDlgItem(IDC_WINDOWS_OPTIONS));
	PropPage::read((HWND)*this, items, confirmItems, GetDlgItem(IDC_CONFIRM_OPTIONS)); // [+] InfinitySky.
	
	ctrlStartup.Attach(GetDlgItem(IDC_WINDOWS_STARTUP)); // [+] IRainman
	ctrlOptions.Attach(GetDlgItem(IDC_WINDOWS_OPTIONS)); // [+] IRainman
	ctrlConfirms.Attach(GetDlgItem(IDC_CONFIRM_OPTIONS)); // [+] IRainman
	
	return TRUE;
}

void WindowsPage::write()
{
	PropPage::write((HWND)*this, items, listItems, GetDlgItem(IDC_WINDOWS_STARTUP));
	PropPage::write((HWND)*this, items, optionItems, GetDlgItem(IDC_WINDOWS_OPTIONS));
	PropPage::write((HWND)*this, items, confirmItems, GetDlgItem(IDC_CONFIRM_OPTIONS)); // [+] InfinitySky.
}