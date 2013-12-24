/**
* Страница в настройках "Разное" / Page "Misc".
*/

/*
 * Copyright (C) 2006-2013 Crise, crise<at>mail.berlios.de
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
#include "MiscPage.h"
#include "WinUtil.h"

PropPage::TextItem MiscPage::texts[] =
{
	{ IDC_ADV_MISC, ResourceManager::SETTINGS_ADVANCED3 },
	{ IDC_PSR_DELAY_STR, ResourceManager::PSR_DELAY },
	{ IDC_THOLD_STR, ResourceManager::THRESHOLD },
	{ IDC_IGNORE_ADD, ResourceManager::ADD },
	{ IDC_IGNORE_REMOVE, ResourceManager::REMOVE },
	{ IDC_IGNORE_CLEAR, ResourceManager::IGNORE_CLEAR },
	{ IDC_MISC_IGNORE, ResourceManager::IGNORED_USERS },
	{ IDC_RAW_TEXTS, ResourceManager::RAW_TEXTS },
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item MiscPage::items[] =
{

	{ IDC_PSR_DELAY, SettingsManager::PSR_DELAY, PropPage::T_INT },
	{ IDC_THOLD, SettingsManager::USER_THERSHOLD, PropPage::T_INT },
	
	{ IDC_RAW1_TEXT, SettingsManager::RAW1_TEXT, PropPage::T_STR },
	{ IDC_RAW2_TEXT, SettingsManager::RAW2_TEXT, PropPage::T_STR },
	{ IDC_RAW3_TEXT, SettingsManager::RAW3_TEXT, PropPage::T_STR },
	{ IDC_RAW4_TEXT, SettingsManager::RAW4_TEXT, PropPage::T_STR },
	{ IDC_RAW5_TEXT, SettingsManager::RAW5_TEXT, PropPage::T_STR },
	
	{ 0, 0, PropPage::T_END }
};

LRESULT MiscPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	
	CRect rc;
	// [!] IRainman fix.
	// [-] HubFrame::loadIgnoreList();
	m_ignoreList = UserManager::getIgnoreList();
	// [~] IRainman fix.
	ignoreListCtrl.Attach(GetDlgItem(IDC_IGNORELIST));
	ignoreListCtrl.GetClientRect(rc);
	ignoreListCtrl.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, (rc.Width() - 17), 0);
	SET_EXTENDENT_LIST_VIEW_STYLE(ignoreListCtrl);
	SET_LIST_COLOR_IN_SETTING(ignoreListCtrl);
	
	auto cnt = ignoreListCtrl.GetItemCount();
	for (auto i = m_ignoreList.cbegin(); i != m_ignoreList.cend(); ++i)
	{
		ignoreListCtrl.insert(cnt++, Text::toT(*i));
	}
	
	return TRUE;
}

LRESULT MiscPage::onIgnoreAdd(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	m_ignoreListCnange = true;
	tstring buf;
	GET_TEXT(IDC_IGNORELIST_EDIT, buf);
	if (!buf.empty())
	{
		const auto& p = m_ignoreList.insert(Text::fromT(buf));
		if (p.second)
		{
			ignoreListCtrl.insert(ignoreListCtrl.GetItemCount(), buf);
		}
		else
		{
			MessageBox(CTSTRING(ALREADY_IGNORED), _T(APPNAME) _T(" ") T_VERSIONSTRING, MB_OK);
		}
	}
	SetDlgItemText(IDC_IGNORELIST_EDIT, _T(""));
	return 0;
}

LRESULT MiscPage::onIgnoreRemove(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	m_ignoreListCnange = true;
	int i = -1;
	
	while ((i = ignoreListCtrl.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		m_ignoreList.erase(ignoreListCtrl.ExGetItemText(i, 0));
		ignoreListCtrl.DeleteItem(i);
	}
	return 0;
}

LRESULT MiscPage::onIgnoreClear(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	m_ignoreListCnange = true;
	ignoreListCtrl.DeleteAllItems();
	m_ignoreList.clear();
	return 0;
}

void MiscPage::write()
{
	PropPage::write((HWND)*this, items);
	
	if (m_ignoreListCnange)
	{
		// [!] IRainman fix.
		UserManager::setIgnoreList(m_ignoreList);
		// [-] HubFrame::saveIgnoreList();
		// [~] IRainman fix.
		m_ignoreListCnange = false;
	}
	/*if(SETTING(PSR_DELAY) < 5)
	    settings->set(SettingsManager::PSR_DELAY, 5);*/
}

