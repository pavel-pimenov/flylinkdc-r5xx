/**
* Страница в настройках "Внешний вид" / Page "Appearance".
*/

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
#include "AppearancePage.h"
#include "../client/File.h"

#include "WinUtil.h"

PropPage::TextItem AppearancePage::texts[] =
{
	{ IDC_SETTINGS_APPEARANCE_OPTIONS, ResourceManager::SETTINGS_OPTIONS },
	{ IDC_SETTINGS_TIME_STAMPS_FORMAT, ResourceManager::SETTINGS_TIME_STAMPS_FORMAT },
	{ IDC_SETTINGS_REQUIRES_RESTART, ResourceManager::SETTINGS_REQUIRES_RESTART },
	{ IDC_THEME, ResourceManager::THEME },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item AppearancePage::items[] =
{
	{ IDC_TIME_STAMPS_FORMAT, SettingsManager::TIME_STAMPS_FORMAT, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

PropPage::ListItem AppearancePage::listItems[] =
{
	{ SettingsManager::MINIMIZE_ON_STARTUP, ResourceManager::SETTINGS_MINIMIZE_ON_STARTUP },
	{ SettingsManager::MINIMIZE_ON_CLOSE, ResourceManager::MINIMIZE_ON_CLOSE }, // [+] InfinitySky.
	{ SettingsManager::MINIMIZE_TRAY, ResourceManager::SETTINGS_MINIMIZE_TRAY },
	{ SettingsManager::SHOW_CURRENT_SPEED_IN_TITLE, ResourceManager::SHOW_CURRENT_SPEED_IN_TITLE }, // [+] InfinitySky.
	{ SettingsManager::SORT_FAVUSERS_FIRST, ResourceManager::SETTINGS_SORT_FAVUSERS_FIRST },
	{ SettingsManager::USE_SYSTEM_ICONS, ResourceManager::SETTINGS_USE_SYSTEM_ICONS },
	{ SettingsManager::USE_OLD_SHARING_UI, ResourceManager::SETTINGS_USE_OLD_SHARING_UI },
//	{ SettingsManager::SHOW_PROGRESS_BARS, ResourceManager::SETTINGS_SHOW_PROGRESS_BARS },
//	{ SettingsManager::UP_TRANSFER_COLORS, ResourceManager::UP_TRANSFER_COLORS }, // [+] Drakon.
	{ SettingsManager::VIEW_GRIDCONTROLS, ResourceManager::VIEW_GRIDCONTROLS }, // [+] ZagZag.
	{ SettingsManager::FILTER_MESSAGES, ResourceManager::SETTINGS_FILTER_MESSAGES },
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
	{ SettingsManager::USE_CUSTOM_LIST_BACKGROUND, ResourceManager::USE_CUSTOM_LIST_BACKGROUND },
#endif
	{ SettingsManager::USE_EXPLORER_THEME, ResourceManager::USE_EXPLORER_THEME },
	{ SettingsManager::USE_12_HOUR_FORMAT, ResourceManager::USE_12_HOUR_FORMAT }, // [+] InfinitySky.
	{ SettingsManager::SHOW_CUSTOM_MINI_ICON_ON_TASKBAR, ResourceManager::SHOW_CUSTOM_MINI_ICON_ON_TASKBAR }, // [+] InfinitySky.
//	{ SettingsManager::SHOW_FULL_HUB_INFO_ON_TAB, ResourceManager::SETTINGS_SHOW_FULL_HUB_INFO_ON_TAB }, // [+] NightOrion.

#ifdef SCALOLAZ_HUB_MODE
	{ SettingsManager::ENABLE_HUBMODE_PIC, ResourceManager::ENABLE_HUBMODE_PIC },
#endif
	{ SettingsManager::ENABLE_COUNTRYFLAG, ResourceManager::ENABLE_COUNTRYFLAG },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

AppearancePage::~AppearancePage()
{
	ctrlList.Detach(); // [+] IRainman
}

void AppearancePage::write()
{
	PropPage::write((HWND)*this, items, listItems, GetDlgItem(IDC_APPEARANCE_BOOLEANS));
	
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO));
	const string l_filetheme = WinUtil::getDataFromMap(ctrlTheme.GetCurSel(), m_ThemeList);
	if (SETTING(THEME_MANAGER_THEME_DLL_NAME) != l_filetheme)
	{
		settings->set(SettingsManager::THEME_MANAGER_THEME_DLL_NAME, l_filetheme);
		if (m_ThemeList.size() != 1)
			MessageBox(CTSTRING(THEME_CHANGE_THEME_INFO), CTSTRING(THEME_CHANGE_THEME), MB_OK | MB_ICONEXCLAMATION);
	}
	ctrlTheme.Detach();
	
}

LRESULT AppearancePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	
	PropPage::read((HWND)*this, items, listItems, GetDlgItem(IDC_APPEARANCE_BOOLEANS));
	
	ctrlList.Attach(GetDlgItem(IDC_APPEARANCE_BOOLEANS)); // [+] IRainman
	
	ctrlTheme.Attach(GetDlgItem(IDC_THEME_COMBO));
	GetThemeList();
	for (auto i = m_ThemeList.cbegin(); i != m_ThemeList.cend(); ++i)
		ctrlTheme.AddString(i->first.c_str());
		
	ctrlTheme.SetCurSel(WinUtil::getIndexFromMap(m_ThemeList, SETTING(THEME_MANAGER_THEME_DLL_NAME)));
	ctrlTheme.Detach();
	
	// Do specialized reading here
	return TRUE;
}

LRESULT AppearancePage::onClickedHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	MessageBox(CTSTRING(TIMESTAMP_HELP), CTSTRING(TIMESTAMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
	return S_OK;
}

void AppearancePage::GetThemeList()
{
	if (m_ThemeList.empty())
	{
		typedef  void (WINAPIV ResourceName)(wchar_t*, size_t);
		m_ThemeList.insert(ThemePair(TSTRING(THEME_DEFAULT_NAME), Util::emptyString));
		// Find in Theme folder .DLL's check'em
		string fileFindPath = Util::getThemesPath() + "*.dll";
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			string name  = i->getFileName();
			string fullPath = Util::getThemesPath() + name;
			if (name.empty() || i->isDirectory() || i->getSize() == 0
			        || CompatibilityManager::getDllPlatform(fullPath) !=
#if defined(_WIN64)
			        IMAGE_FILE_MACHINE_AMD64
#elif defined(_WIN32)
			        IMAGE_FILE_MACHINE_I386
#endif
			   )
			{
				continue;
			}
			// CheckName;
			HINSTANCE hModule = nullptr;
			try
			{
				hModule =::LoadLibrary(Text::toT(fullPath).c_str());
				if (hModule != NULL)
				{
					ResourceName* resourceName = (ResourceName*)::GetProcAddress((HMODULE)hModule, "ResourceName");
					if (resourceName != NULL)
					{
						unique_ptr<wchar_t[]> buff(new wchar_t[256]);
						buff.get()[0] = 0;
						resourceName(buff.get(), 256);
						if (buff.get()[0] != 0) // [!]PVS V805 Decreased performance. It is inefficient to identify an empty string by using 'wcslen(str) > 0' construct. A more efficient way is to check: str[0] != '\0'.
						{
							wstring wName = buff.get();
							wName +=  L" (" + Text::toT(name) + L')';
							m_ThemeList.insert(ThemePair(wName, name));
						}
					}
					::FreeLibrary(hModule);
				}
			}
			catch (...)
			{
				if (hModule)
					::FreeLibrary(hModule);
				hModule = NULL;
			}
		}
		
	}
}