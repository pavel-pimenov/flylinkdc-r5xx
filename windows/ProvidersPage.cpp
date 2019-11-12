/*
 * Copyright (C) 2012-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifdef FLYLINKDC_USE_PROVIDER_RESOURCES

#include "ProvidersPage.h"

PropPage::TextItem ProvidersPage::texts[] =
{
	{ IDC_PROVIDER_USE_RESOURCES, ResourceManager::PROVIDER_USE_RESOURCES },
	{ IDC_PROVIDER_URL_GROUP, ResourceManager::PROVIDER_URL_GROUP },
	{ IDC_PROVIDER_USELIST_PAGE, ResourceManager::PROVIDER_USELIST_PAGE },
// End of Addition.
	{ IDC_GROUP_CUSTOMMENU, ResourceManager::GROUP_CUSTOMMENU},
	{ IDC_USE_CUSTOM_MENU, ResourceManager::USE_CUSTOM_MENU },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item ProvidersPage::items[] =
{
	{ IDC_PROVIDER_USE_RESOURCES, SettingsManager::PROVIDER_USE_RESOURCES, PropPage::T_BOOL },
	{ IDC_PROVIDER_URL, SettingsManager::ISP_RESOURCE_ROOT_URL, PropPage::T_STR }, // PROVIDER_URL
// End of Addition.
	{ IDC_USE_CUSTOM_MENU, SettingsManager::USE_CUSTOM_MENU, PropPage::T_BOOL },
	{ IDC_CUSTOM_MENU_URL, SettingsManager::CUSTOM_MENU_PATH, PropPage::T_STR},
	{ 0, 0, PropPage::T_END }
};

ProvidersPage::ListItem ProvidersPage::listItems[] =
{
	{ SettingsManager::PROVIDER_USE_MENU, ResourceManager::PROVIDER_USE_MENU},
	{ SettingsManager::PROVIDER_USE_HUBLIST, ResourceManager::PROVIDER_USE_HUBLIST},
	{ SettingsManager::PROVIDER_USE_PROVIDER_LOCATIONS, ResourceManager::PROVIDER_USE_PROVIDER_LOCATIONS},
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT ProvidersPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_PROVIDER_USE_LIST));
	m_url.init(GetDlgItem(IDC_ISP_MORE_INFO_LINK), _T(""));
	
	fixControls();
	
	return TRUE;
}

void ProvidersPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_PROVIDER_USE_LIST));
}

LRESULT ProvidersPage::onClickedUse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	
	return 0;
}

void ProvidersPage::fixControls()
{
	BOOL use_resources = IsDlgButtonChecked(IDC_PROVIDER_USE_RESOURCES) == BST_CHECKED;
	
	GetDlgItem(IDC_PROVIDER_URL_GROUP).EnableWindow(use_resources);
	GetDlgItem(IDC_PROVIDER_USELIST_PAGE).EnableWindow(use_resources);
	GetDlgItem(IDC_PROVIDER_USE_LIST).EnableWindow(use_resources);
	GetDlgItem(IDC_PROVIDER_URL).EnableWindow(use_resources);
	
	GetDlgItem(IDC_GROUP_CUSTOMMENU).EnableWindow(!use_resources);
	GetDlgItem(IDC_CUSTOM_MENU_URL).EnableWindow(!use_resources);
	GetDlgItem(IDC_USE_CUSTOM_MENU).EnableWindow(!use_resources);
}

#endif // FLYLINKDC_USE_PROVIDER_RESOURCES