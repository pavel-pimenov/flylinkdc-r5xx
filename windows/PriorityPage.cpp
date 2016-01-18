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

#include "PriorityPage.h"
#include "CommandDlg.h"

// Перевод текстов на странице.
PropPage::TextItem PriorityPage::texts[] =
{
	{ IDC_SETTINGS_AUTOPRIO, ResourceManager::SETTINGS_PRIO_AUTOPRIO },
	{ IDC_SETTINGS_PRIO_HIGHEST, ResourceManager::SETTINGS_PRIO_HIGHEST },
	{ IDC_SETTINGS_KB3, ResourceManager::KB },
	{ IDC_SETTINGS_PRIO_HIGH, ResourceManager::SETTINGS_PRIO_HIGH },
	{ IDC_SETTINGS_KB4, ResourceManager::KB },
	{ IDC_SETTINGS_PRIO_NORMAL, ResourceManager::SETTINGS_PRIO_NORMAL },
	{ IDC_SETTINGS_KB5, ResourceManager::KB },
	{ IDC_SETTINGS_PRIO_LOW, ResourceManager::SETTINGS_PRIO_LOW },
	{ IDC_SETTINGS_KB6, ResourceManager::KB },
	{ IDC_HIGHEST_STR, ResourceManager::PRIO_FILE_HIGHEST },
	{ IDC_LOWEST_STR, ResourceManager::PRIO_FILE_LOWEST },
	{ IDC_PRIO_FILE, ResourceManager::PRIO_FILE },
	{ IDC_PRIO_LOWEST, ResourceManager::SETTINGS_PRIO_LOWEST }, // [~] InfinitySky.
	{ IDC_USE_AUTOPRIORITY, ResourceManager::SETTINGS_AUTO_PRIORITY_DEFAULT }, // [~] InfinitySky.
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

// Элементы настроек.
PropPage::Item PriorityPage::items[] =
{
	{ IDC_PRIO_HIGHEST_SIZE, SettingsManager::PRIO_HIGHEST_SIZE, PropPage::T_INT },
	{ IDC_PRIO_HIGH_SIZE, SettingsManager::PRIO_HIGH_SIZE, PropPage::T_INT },
	{ IDC_PRIO_NORMAL_SIZE, SettingsManager::PRIO_NORMAL_SIZE, PropPage::T_INT },
	{ IDC_PRIO_LOW_SIZE, SettingsManager::PRIO_LOW_SIZE, PropPage::T_INT },
	{ IDC_HIGHEST, SettingsManager::HIGH_PRIO_FILES, PropPage::T_STR },
	{ IDC_LOWEST, SettingsManager::LOW_PRIO_FILES, PropPage::T_STR },
	{ IDC_PRIO_LOWEST, SettingsManager::PRIO_LOWEST, PropPage::T_BOOL }, // [~] InfinitySky.
	{ IDC_USE_AUTOPRIORITY, SettingsManager::AUTO_PRIORITY_DEFAULT, PropPage::T_BOOL }, // [~] InfinitySky.
	{ 0, 0, PropPage::T_END }
};

// При инициализации.
LRESULT PriorityPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items, 0, 0);
	
	fixControls();
	
	// Do specialized reading here
	return TRUE;
}

void PriorityPage::write()
{
	PropPage::write((HWND)*this, items, 0, 0);
}

// [+] InfinitySky. При отключении автоприоритета активируются указанные элементы.
void PriorityPage::fixControls()
{
	const BOOL state = IsDlgButtonChecked(IDC_USE_AUTOPRIORITY) == 0;
	::EnableWindow(GetDlgItem(IDC_PRIO_HIGHEST_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_PRIO_HIGH_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_PRIO_NORMAL_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_PRIO_LOW_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_PRIO_LOWEST), state);
	
	::EnableWindow(GetDlgItem(IDC_PRIO_FILE), FALSE);
	::EnableWindow(GetDlgItem(IDC_HIGHEST), FALSE);
	::EnableWindow(GetDlgItem(IDC_LOWEST_STR), FALSE);
	::EnableWindow(GetDlgItem(IDC_HIGHEST_STR), FALSE);
}

// [+] InfinitySky. При смене состояния кнопки включения автоприоритета.
LRESULT PriorityPage::onChangeCont(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_USE_AUTOPRIORITY:
			fixControls();
			break;
	}
	return true;
}