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

#include "PropPage.h"

#include "../client/SettingsManager.h"
#include "WinUtil.h"

void PropPage::read(HWND page, Item const* items, ListItem* listItems /* = NULL */, HWND list /* = 0 */)
{
#ifdef _DEBUG
	m_check_read_write++;
#endif
	dcassert(page != NULL);
	
	if (items != NULL) // [+] SSA
	{
		bool const useDef = true;
		for (Item const* i = items; i->type != T_END; i++)
		{
			switch (i->type)
			{
				case T_STR:
					if (GetDlgItem(page, i->itemID) == NULL)
					{
						// Control not exist ? Why ??
						throw;
					}
					::SetDlgItemText(page, i->itemID,
					                 Text::toT(settings->get((SettingsManager::StrSetting)i->setting, useDef)).c_str());
					break;
				case T_INT:
				
					if (GetDlgItem(page, i->itemID) == NULL)
					{
						// Control not exist ? Why ??
						throw;
					}
					::SetDlgItemInt(page, i->itemID,
					                settings->get((SettingsManager::IntSetting)i->setting, useDef), FALSE);
					break;
				case T_BOOL:
					if (GetDlgItem(page, i->itemID) == NULL)
					{
						// Control not exist ? Why ??
						throw;
					}
					if (settings->getBool((SettingsManager::IntSetting)i->setting, useDef))
						::CheckDlgButton(page, i->itemID, BST_CHECKED);
					else
						::CheckDlgButton(page, i->itemID, BST_UNCHECKED);
					break;
				case T_END:
					dcassert(false);
					break;
			}
		}
	}
	if (listItems != NULL)
	{
		CListViewCtrl ctrl;
		
		ctrl.Attach(list);
		CRect rc;
		ctrl.GetClientRect(rc);
		SET_EXTENDENT_LIST_VIEW_STYLE_WITH_CHECK(ctrl);
		SET_LIST_COLOR_IN_SETTING(ctrl);
		ctrl.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
		
		LVITEM lvi = {0};
		lvi.mask = LVIF_TEXT;
		lvi.iSubItem = 0;
		
		for (int i = 0; listItems[i].setting != 0; i++)
		{
			lvi.iItem = i;
			lvi.pszText = const_cast<TCHAR*>(CTSTRING_I(listItems[i].desc));
			ctrl.InsertItem(&lvi);
			ctrl.SetCheckState(i, SettingsManager::getInstance()->getBool(SettingsManager::IntSetting(listItems[i].setting), true));
		}
		ctrl.SetColumnWidth(0, LVSCW_AUTOSIZE);
		ctrl.Detach();
	}
}

void PropPage::write(HWND page, Item const* items, ListItem* listItems /* = NULL */, HWND list /* = NULL */)
{
#ifdef _DEBUG
	m_check_read_write--;
#endif
	dcassert(page != NULL);
	
	bool l_showUserWarning = false;// [+] IRainman
	
	if (items != NULL) // [+] SSA
	{
		for (Item const* i = items; i->type != T_END; ++i)
		{
			tstring buf;
			switch (i->type)
			{
				case T_STR:
				{
					WinUtil::GetDlgItemText(page, i->itemID, buf);
					l_showUserWarning |= settings->set((SettingsManager::StrSetting)i->setting, Text::fromT(buf));// [!] IRainman
					// Crash https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=78416
					break;
				}
				case T_INT:
				{
					WinUtil::GetDlgItemText(page, i->itemID, buf);
					l_showUserWarning |= settings->set((SettingsManager::IntSetting)i->setting, Util::toInt(buf));// [!] IRainman
					break;
				}
				case T_BOOL:
				{
					if (GetDlgItem(page, i->itemID) == NULL)
					{
						// Control not exist ? Why ??
						dcassert(0);
						throw;
					}
					if (::IsDlgButtonChecked(page, i->itemID) == BST_CHECKED)
						l_showUserWarning |= settings->set((SettingsManager::IntSetting)i->setting, true);// [!] IRainman
					else
						l_showUserWarning |= settings->set((SettingsManager::IntSetting)i->setting, false);// [!] IRainman
					break;
				}
				case T_END:
					dcassert(0);
					break;
			}
		}
	}
	
	if (listItems != NULL)
	{
		CListViewCtrl ctrl;
		ctrl.Attach(list);
		
		for (int i = 0; listItems[i].setting != 0; i++)
		{
			l_showUserWarning |= SET_SETTING(IntSetting(listItems[i].setting), ctrl.GetCheckState(i));// [!] IRainman
		}
		
		ctrl.Detach();
	}
#ifdef _DEBUG
	if (l_showUserWarning)
	{
		MessageBox(page, _T("Values of the changed settings are automatically adjusted"), CTSTRING(WARNING), MB_OK);
	}
#endif
}

void PropPage::cancel(HWND page)
{
	dcassert(page != NULL);
	cancel_check();
}

void PropPage::translate(HWND page, TextItem* textItems)
{
	if (textItems != NULL)
	{
		for (int i = 0; textItems[i].itemID != 0; i++)
		{
			::SetDlgItemText(page, textItems[i].itemID,
			                 CTSTRING_I(textItems[i].translatedString));
		}
	}
}

PropPage::TextItem EmptyPage::texts[] = // [+] IRainman HE
{
	{ IDC_FUNCTIONAL_IS_DISABLED, ResourceManager::THIS_FUNCTIONAL_IS_DISABLED },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

/**
 * @file
 * $Id: PropPage.cpp,v 1.15 2006/08/21 18:21:37 bigmuscle Exp $
 */
