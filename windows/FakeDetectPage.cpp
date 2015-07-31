/*
 * Copyright (C) 2001-2003 Jacek Sieka, j_s@telia.com
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
#ifdef IRAINMAN_ENABLE_AUTO_BAN
#include "FakeDetectPage.h"
#include "WinUtil.h"

PropPage::TextItem FakeDetect::texts[] =
{
	{ IDC_ENABLE_AUTO_BAN, ResourceManager::SETTINGS_AUTO_BAN },
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	{ IDC_PROTECT_OP, ResourceManager::SETTINGS_PROTECT_OP },
#endif
	{ IDC_FAKE_SET, ResourceManager::SETTINGS_FAKE_SET },
	{ IDC_RAW_COMMANDS, ResourceManager::RAW_SET },
	{ IDC_DISCONNECTS_T, ResourceManager::DISCONNECT_RAW },
	{ IDC_TIMEOUT_T, ResourceManager::TIMEOUT_RAW },
	{ IDC_FAKESHARE_T, ResourceManager::FAKESHARE_RAW },
	{ IDC_LISTLEN_MISMATCH_T, ResourceManager::LISTLEN_MISMATCH },
	{ IDC_FILELIST_TOO_SMALL_T, ResourceManager::FILELIST_TOO_SMALL },
	{ IDC_FILELIST_UNAVAILABLE_T, ResourceManager::FILELIST_UNAVAILABLE },
	{ IDC_TEXT_FAKEPERCENT, ResourceManager::TEXT_FAKEPERCENT },
	{ IDC_TIMEOUTS, ResourceManager::ACCEPTED_TIMEOUTS },
	{ IDC_DISCONNECTS, ResourceManager::ACCEPTED_DISCONNECTS },
	{ IDC_PROT_USERS, ResourceManager::PROT_USERS },
	{ IDC_PROT_DESC, ResourceManager::PROT_DESC },
	{ IDC_PROT_FAVS, ResourceManager::PROT_FAVS },
	// !SMT!-S
	{ IDC_BAN_SLOTS, ResourceManager::BAN_SLOTS },
	{ IDC_BAN_SLOTS_H, ResourceManager::BAN_SLOTS_H },
	{ IDC_BAN_SHARE, ResourceManager::BAN_SHARE },
	{ IDC_BAN_LIMIT, ResourceManager::BAN_LIMIT },
	{ IDC_BAN_MSG, ResourceManager::BAN_MESSAGE },
	{ IDC_BANMSG_PERIOD, ResourceManager::BANMSG_PERIOD },
	{ IDC_BAN_STEALTH, ResourceManager::BAN_STEALTH },
	{ IDC_BAN_FORCE_PM, ResourceManager::BAN_FORCE_PM },
	
	{ IDC_AUTOBAN_HINT, ResourceManager::AUTOBAN_HINT }, //[+] Drakon
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item FakeDetect::items[] =
{
	{ IDC_ENABLE_AUTO_BAN, SettingsManager::ENABLE_AUTO_BAN, PropPage::T_BOOL }, // [+]IRainman
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	{ IDC_PROTECT_OP, SettingsManager::AUTOBAN_PPROTECT_OP, PropPage::T_BOOL }, // [+]IRainman
#endif
	{ IDC_PERCENT_FAKE_SHARE_TOLERATED, SettingsManager::PERCENT_FAKE_SHARE_TOLERATED, PropPage::T_INT },
	{ IDC_TIMEOUTS_NO, SettingsManager::ACCEPTED_TIMEOUTS, PropPage::T_INT },
	{ IDC_DISCONNECTS_NO, SettingsManager::ACCEPTED_DISCONNECTS, PropPage::T_INT },
	{ IDC_PROT_PATTERNS, SettingsManager::PROT_USERS, PropPage::T_STR },
	{ IDC_PROT_FAVS, SettingsManager::PROT_FAVS, PropPage::T_BOOL },
	// !SMT!-S
	{ IDC_BAN_SLOTS_NO, SettingsManager::BAN_SLOTS, PropPage::T_INT },
	{ IDC_BAN_SLOTS_NO_H, SettingsManager::BAN_SLOTS_H, PropPage::T_INT },
	{ IDC_BAN_SHARE_NO, SettingsManager::BAN_SHARE, PropPage::T_INT },
	{ IDC_BAN_LIMIT_NO, SettingsManager::BAN_LIMIT, PropPage::T_INT },
	{ IDC_BAN_MSG_STR, SettingsManager::BAN_MESSAGE, PropPage::T_STR },
	{ IDC_BANMSG_PERIOD_NO, SettingsManager::BANMSG_PERIOD, PropPage::T_INT },
	{ IDC_BAN_STEALTH, SettingsManager::BAN_STEALTH, PropPage::T_BOOL },
	{ IDC_BAN_FORCE_PM, SettingsManager::BAN_FORCE_PM, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

FakeDetect::ListItem FakeDetect::listItems[] =
{
	//
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT FakeDetect::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::read((HWND)*this, items, listItems, GetDlgItem(IDC_FAKE_BOOLEANS));
	ctrlList.Attach(GetDlgItem(IDC_FAKE_BOOLEANS)); // [+] IRainman
	CComboBox cRaw;
	
#define ADDSTRINGS \
	cRaw.AddString(CWSTRING(NO_ACTION)); \
	cRaw.AddString(Text::toT(SETTING(RAW1_TEXT)).c_str()); \
	cRaw.AddString(Text::toT(SETTING(RAW2_TEXT)).c_str()); \
	cRaw.AddString(Text::toT(SETTING(RAW3_TEXT)).c_str()); \
	cRaw.AddString(Text::toT(SETTING(RAW4_TEXT)).c_str()); \
	cRaw.AddString(Text::toT(SETTING(RAW5_TEXT)).c_str());
	
	cRaw.Attach(GetDlgItem(IDC_DISCONNECT_RAW));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::DISCONNECT_RAW));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_TIMEOUT_RAW));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::TIMEOUT_RAW));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_FAKE_RAW));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::FAKESHARE_RAW));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_LISTLEN));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::LISTLEN_MISMATCH));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_FILELIST_TOO_SMALL));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::FILELIST_TOO_SMALL));
	cRaw.Detach();
	
	cRaw.Attach(GetDlgItem(IDC_FILELIST_UNAVAILABLE));
	ADDSTRINGS
	cRaw.SetCurSel(g_settings->get(SettingsManager::FILELIST_UNAVAILABLE));
	cRaw.Detach();
	
#undef ADDSTRINGS
	
	PropPage::translate((HWND)(*this), texts);
	
	::ShowWindow(GetDlgItem(IDC_FAKE_BOOLEANS), FALSE); //Hide ctrlList
	
	// Do specialized reading here
	
	return TRUE;
}

void FakeDetect::write()
{
	PropPage::write((HWND)*this, items, listItems, GetDlgItem(IDC_FAKE_BOOLEANS));
	
	// Do specialized writing here
	// settings->set(XX, YY);
	CComboBox cRaw(GetDlgItem(IDC_DISCONNECT_RAW));
	SET_SETTING(DISCONNECT_RAW, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_TIMEOUT_RAW);
	SET_SETTING(TIMEOUT_RAW, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_FAKE_RAW);
	SET_SETTING(FAKESHARE_RAW, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_LISTLEN);
	SET_SETTING(LISTLEN_MISMATCH, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_FILELIST_TOO_SMALL);
	SET_SETTING(FILELIST_TOO_SMALL, cRaw.GetCurSel());
	
	cRaw = GetDlgItem(IDC_FILELIST_UNAVAILABLE);
	SET_SETTING(FILELIST_UNAVAILABLE, cRaw.GetCurSel());
}

#endif // IRAINMAN_ENABLE_AUTO_BAN

/**
 * @file
 * $Id: FakeDetect.cpp,v 1.12 2005/03/06 17:09:21 bigmuscle Exp $
 */

