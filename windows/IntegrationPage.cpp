/*
 * Copyright (C) 2011-2021 FlylinkDC++ Team http://flylinkdc.com
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

#include <strsafe.h>

#include "../client/LogManager.h"
#include "IntegrationPage.h"
#include "CommandDlg.h"

PropPage::TextItem IntegrationPage::texts[] =
{
	{ IDC_INTEGRATION_SHELL_GROUP, ResourceManager::INTEGRATION_SHELL_GROUP},
	{ IDC_INTEGRATION_SHELL_BTN, ResourceManager::INTEGRATION_SHELL_BTN_ADD},
	{ IDC_INTEGRATION_SHELL_TEXT, ResourceManager::INTEGRATION_SHELL_TEXT},
	{ IDC_INTEGRATION_INTERFACE_GROUP, ResourceManager::INTEGRATION_INTERFACE_GROUP},
	{ IDC_INTEGRATION_IFACE_BTN_AUTOSTART, ResourceManager::INTEGRATION_IFACE_BTN_AUTOSTART_ADD},
	{ IDC_INTEGRATION_IFACE_STARTUP_TEXT, ResourceManager::INTEGRATION_IFACE_STARTUP_TEXT},
// End of Addition.
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item IntegrationPage::items[] =
{
	{ 0, 0, PropPage::T_END }
};

PropPage::ListItem IntegrationPage::listItems[] =
{
	{ SettingsManager::URL_HANDLER, ResourceManager::SETTINGS_URL_HANDLER },
	{ SettingsManager::MAGNET_REGISTER, ResourceManager::SETCZDC_MAGNET_URI_HANDLER },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};


LRESULT IntegrationPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_INTEGRATION_BOOLEANS));
	
	m_ctrlList.Attach(GetDlgItem(IDC_INTEGRATION_BOOLEANS));
	::EnableWindow(GetDlgItem(IDC_INTEGRATION_SHELL_BTN), FALSE);
	GetDlgItem(IDC_INTEGRATION_SHELL_BTN).ShowWindow(FALSE);
	GetDlgItem(IDC_INTEGRATION_SHELL_TEXT).ShowWindow(FALSE);
	CheckStartupIntegration();
	if (_isStartupIntegration)
		SetDlgItemText(IDC_INTEGRATION_IFACE_BTN_AUTOSTART, CTSTRING(INTEGRATION_IFACE_BTN_AUTOSTART_REMOVE));
		
	if (CompatibilityManager::isOsVistaPlus())
	{
		GetDlgItem(IDC_INTEGRATION_SHELL_BTN).SendMessage(BCM_FIRST + 0x000C, 0, 0xFFFFFFFF);
		GetDlgItem(IDC_INTEGRATION_IFACE_BTN_AUTOSTART).SendMessage(BCM_FIRST + 0x000C, 0, 0xFFFFFFFF);
	}
	
	return TRUE;
}

void IntegrationPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_INTEGRATION_BOOLEANS));
	PropPage::write(*this, items);
}
LRESULT IntegrationPage::OnClickedMakeStartup(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // TODO: please fix copy-past.
{
	// Make Smth
	bool oldState = _isStartupIntegration;
	bool bResult =  WinUtil::AutoRunShortCut(!_isStartupIntegration);
	if (bResult)
	{
		CheckStartupIntegration();
		if (oldState == _isStartupIntegration
		        && CompatibilityManager::isOsVistaPlus()
		   )
		{
			//  runas uac
			WinUtil::runElevated(NULL, Util::getModuleFileName().c_str(), _isShellIntegration ? _T("/uninstallStartup") : _T("/installStartup"));
		}
	}
	CheckStartupIntegration();
	if (oldState == _isStartupIntegration)
	{
		// Message Error
	}
	if (_isStartupIntegration)
		SetDlgItemText(IDC_INTEGRATION_IFACE_BTN_AUTOSTART, CTSTRING(INTEGRATION_IFACE_BTN_AUTOSTART_REMOVE));
	else
		SetDlgItemText(IDC_INTEGRATION_IFACE_BTN_AUTOSTART, CTSTRING(INTEGRATION_IFACE_BTN_AUTOSTART_ADD));
		
	return FALSE;
}
void IntegrationPage::CheckStartupIntegration()
{
	_isStartupIntegration = WinUtil::IsAutoRunShortCutExists();
}