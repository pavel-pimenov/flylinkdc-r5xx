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

#include "../client/CryptoManager.h"
#include "Resource.h"
#include "CertificatesPage.h"
#include "CommandDlg.h"

PropPage::TextItem CertificatesPage::texts[] =
{
	{ IDC_STATIC1, ResourceManager::PRIVATE_KEY_FILE },
	{ IDC_STATIC2, ResourceManager::OWN_CERTIFICATE_FILE },
	{ IDC_STATIC3, ResourceManager::TRUSTED_CERTIFICATES_PATH },
	{ IDC_GENERATE_CERTS, ResourceManager::GENERATE_CERTIFICATES },
	{ IDC_SECURITY_GROUP, ResourceManager::SETTINGS_CERTIFICATES },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item CertificatesPage::items[] =
{
	{ IDC_TLS_CERTIFICATE_FILE, SettingsManager::TLS_CERTIFICATE_FILE, PropPage::T_STR },
	{ IDC_TLS_PRIVATE_KEY_FILE, SettingsManager::TLS_PRIVATE_KEY_FILE, PropPage::T_STR },
	{ IDC_TLS_TRUSTED_CERTIFICATES_PATH, SettingsManager::TLS_TRUSTED_CERTIFICATES_PATH, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

PropPage::ListItem CertificatesPage::listItems[] =
{
	{ SettingsManager::USE_TLS, ResourceManager::SETTINGS_USE_TLS },
	{ SettingsManager::ALLOW_UNTRUSTED_HUBS, ResourceManager::SETTINGS_ALLOW_UNTRUSTED_HUBS },
	{ SettingsManager::ALLOW_UNTRUSTED_CLIENTS, ResourceManager::SETTINGS_ALLOW_UNTRUSTED_CLIENTS },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::ListItem CertificatesPage::securityItems[] =
{
	{ SettingsManager::SECURITY_ASK_ON_SHARE_FROM_SHELL, ResourceManager::SECURITY_ASK_ON_SHARE_FROM_SHELL },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT CertificatesPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_TLS_OPTIONS));
	PropPage::read(*this, NULL, securityItems, GetDlgItem(IDC_SECURITY_LIST));
	ctrlList.Attach(GetDlgItem(IDC_TLS_OPTIONS));
	
	// Do specialized reading here
	return TRUE;
}

void CertificatesPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_TLS_OPTIONS));
	PropPage::write(*this, NULL, securityItems, GetDlgItem(IDC_SECURITY_LIST));
}

LRESULT CertificatesPage::onBrowsePrivateKey(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring target = Text::toT(SETTING(TLS_PRIVATE_KEY_FILE));
	CEdit edt(GetDlgItem(IDC_TLS_PRIVATE_KEY_FILE));
	
	if (WinUtil::browseFile(target, m_hWnd, false, target))
	{
		edt.SetWindowText(&target[0]);
	}
	return 0;
}

LRESULT CertificatesPage::onBrowseCertificate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring target = Text::toT(SETTING(TLS_CERTIFICATE_FILE));
	CEdit edt(GetDlgItem(IDC_TLS_CERTIFICATE_FILE));
	
	if (WinUtil::browseFile(target, m_hWnd, false, target))
	{
		edt.SetWindowText(&target[0]);
	}
	return 0;
}

LRESULT CertificatesPage::onBrowseTrustedPath(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring target = Text::toT(SETTING(TLS_TRUSTED_CERTIFICATES_PATH));
	CEdit edt(GetDlgItem(IDC_TLS_TRUSTED_CERTIFICATES_PATH));
	
	if (WinUtil::browseDirectory(target, m_hWnd))
	{
		edt.SetWindowText(&target[0]);
	}
	return 0;
}

LRESULT CertificatesPage::onGenerateCerts(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	try
	{
		CryptoManager::getInstance()->generateCertificate();
	}
	catch (const CryptoException& e)
	{
		MessageBox(Text::toT(e.getError()).c_str(), CTSTRING(ERROR_GENERATING_CERTIFICATE));
	}
	return 0;
}

/**
 * @file
 * $Id: CertificatesPage.cpp 403 2008-07-10 21:27:57Z BigMuscle $
 */
