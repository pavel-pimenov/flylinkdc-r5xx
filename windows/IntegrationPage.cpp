/*
 * Copyright (C) 2011-2016 FlylinkDC++ Team http://flylinkdc.com/
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
#include "WinUtil.h"
#include "../FlylinkDCExt.h"
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
#ifdef SSA_SHELL_INTEGRATION
	{ SettingsManager::POPUP_NEW_FOLDERSHARE, ResourceManager::POPUP_NEW_FOLDERSHARE }, // [+] SSA
#endif
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};


LRESULT IntegrationPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_INTEGRATION_BOOLEANS));
	
	m_ctrlList.Attach(GetDlgItem(IDC_INTEGRATION_BOOLEANS));
#ifdef SSA_SHELL_INTEGRATION
	CheckShellIntegration();
	GetDlgItem(IDC_INTEGRATION_SHELL_BTN).EnableWindow(_canShellIntegration);
	if (_isShellIntegration)
		SetDlgItemText(IDC_INTEGRATION_SHELL_BTN, CTSTRING(INTEGRATION_SHELL_BTN_REMOVE));
#else
	::EnableWindow(GetDlgItem(IDC_INTEGRATION_SHELL_BTN), FALSE);
	GetDlgItem(IDC_INTEGRATION_SHELL_BTN).ShowWindow(FALSE);
	GetDlgItem(IDC_INTEGRATION_SHELL_TEXT).ShowWindow(FALSE);
#endif
	CheckStartupIntegration();
	if (_isStartupIntegration)
		SetDlgItemText(IDC_INTEGRATION_IFACE_BTN_AUTOSTART, CTSTRING(INTEGRATION_IFACE_BTN_AUTOSTART_REMOVE));
		
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	if (CompatibilityManager::isOsVistaPlus())
#endif
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
#ifdef SSA_SHELL_INTEGRATION
LRESULT IntegrationPage::OnClickedShellIntegrate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // TODO: please fix copy-past.
{
	// Make Smth
	bool oldState = _isShellIntegration;
	bool bResult = WinUtil::makeShellIntegration(_isShellIntegration);
	if (!bResult)
	{
		CheckShellIntegration();
		if (oldState == _isShellIntegration
#ifdef FLYLINKDC_SUPPORT_WIN_XP
		        && CompatibilityManager::isOsVistaPlus()
#endif
		   )
		{
			WinUtil::runElevated(NULL, Util::getModuleFileName().c_str(), _isShellIntegration ? _T("/uninstallShellExt") : _T("/installShellExt"));
		}
	}
	CheckShellIntegration();
	if (oldState == _isShellIntegration)
	{
		LogManager::message(STRING(INTEGRATION_SHELL_CANT_INTEGRATE));
	}
	
	
	GetDlgItem(IDC_INTEGRATION_SHELL_BTN).EnableWindow(_canShellIntegration);
	
	if (_isShellIntegration)
		SetDlgItemText(IDC_INTEGRATION_SHELL_BTN, CTSTRING(INTEGRATION_SHELL_BTN_REMOVE));
	else
		SetDlgItemText(IDC_INTEGRATION_SHELL_BTN, CTSTRING(INTEGRATION_SHELL_BTN_ADD));
		
	return FALSE;
}
#endif // SSA_SHELL_INTEGRATION
LRESULT IntegrationPage::OnClickedMakeStartup(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // TODO: please fix copy-past.
{
	// Make Smth
	bool oldState = _isStartupIntegration;
	bool bResult =  WinUtil::AutoRunShortCut(!_isStartupIntegration);
	if (bResult)
	{
		CheckStartupIntegration();
		if (oldState == _isStartupIntegration
#ifdef FLYLINKDC_SUPPORT_WIN_XP
		        && CompatibilityManager::isOsVistaPlus()
#endif
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
#ifdef SSA_SHELL_INTEGRATION
void IntegrationPage::CheckShellIntegration()
{
// Check The function creates the HKCR\CLSID\{<CLSID>} key in the registry.
	_canShellIntegration = false;
	_isShellIntegration = false;
	// Search dll
	const auto filePath = WinUtil::getShellExtDllPath();
	if (!File::isExist(filePath))
	{
		return;
	}
	_canShellIntegration = true;
	// Check CLSID
	wchar_t szCLSID[40] = {0};
	::StringFromGUID2(CLSID_FlylinkShellExt, szCLSID, ARRAYSIZE(szCLSID));
	
	wchar_t szSubkey[MAX_PATH];
	
	// Create the HKCR\CLSID\{<CLSID>} key.
	HRESULT hr = StringCchPrintf(szSubkey, ARRAYSIZE(szSubkey), L"CLSID\\%s", szCLSID);
	if (SUCCEEDED(hr))
	{
	
		HKEY hKey = NULL;
		
		// Try to open the specified registry key.
		hr = HRESULT_FROM_WIN32(RegOpenKeyEx(HKEY_CLASSES_ROOT, szSubkey, 0,
		                                     KEY_READ, &hKey));
		                                     
		if (SUCCEEDED(hr))
		{
			_isShellIntegration = true;
			RegCloseKey(hKey);
		}
		
	}
	
}
#endif // SSA_SHELL_INTEGRATION
void IntegrationPage::CheckStartupIntegration()
{
	_isStartupIntegration = WinUtil::IsAutoRunShortCutExists();
}