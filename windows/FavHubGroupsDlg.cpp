/*
 * Copyright (C) 2007-2013 adrian_007, adrian-007 on o2 point pl
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

#include "FavHubGroupsDlg.h"
#include "WinUtil.h"
#include "ExMessageBox.h" // [+] NightOrion. From Apex.

LRESULT FavHubGroupsDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlGroups.Attach(GetDlgItem(IDC_GROUPS));
	
	uint32_t width;
	{
		CRect rc;
		ctrlGroups.GetClientRect(rc);
		width = rc.Width() - 20; // for scroll
	}
	
	// Translate dialog
	SetWindowText(CTSTRING(MANAGE_GROUPS));
	SetDlgItemText(IDC_ADD, CTSTRING(ADD));
	SetDlgItemText(IDC_REMOVE, CTSTRING(REMOVE));
	SetDlgItemText(IDC_SAVE, CTSTRING(SAVE)); // [~] NightOrion.
	SetDlgItemText(IDCANCEL, CTSTRING(CLOSE));
	SetDlgItemText(IDC_NAME_STATIC, CTSTRING(NAME));
	
	SetDlgItemText(IDC_PRIVATE, CTSTRING(GROUPS_PRIVATE_CHECKBOX));
	SetDlgItemText(IDC_GROUP_PROPERTIES, CTSTRING(GROUPS_PROPERTIES_FIELD));
	
	ctrlGroups.InsertColumn(0, CTSTRING(NAME), LVCFMT_LEFT, WinUtil::percent(width, 70), 0);
	ctrlGroups.InsertColumn(1, CTSTRING(GROUPS_PRIVATE), LVCFMT_LEFT, WinUtil::percent(width, 15), 0);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlGroups);
	SET_LIST_COLOR(ctrlGroups);
	
	{
		FavoriteManager::LockInstanceHubs lockedInstanceHubs;
		const FavHubGroups& groups = lockedInstanceHubs.getFavHubGroups();
		for (auto i = groups.cbegin(); i != groups.cend(); ++i)
		{
			addItem(Text::toT(i->first), i->second.priv);
		}
	}
	updateSelectedGroup(true);
	return 0;
}

LRESULT FavHubGroupsDlg::onClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	save();
	EndDialog(FALSE);
	return 0;
}

LRESULT FavHubGroupsDlg::onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
{
	updateSelectedGroup();
	return 0;
}

void FavHubGroupsDlg::save()
{
	FavHubGroups groups;
	string name;
	FavHubGroupProperties group;
	
	for (int i = 0; i < ctrlGroups.GetItemCount(); ++i)
	{
		//@todo: fixme
		/*group.first = Text::fromT(getText(0, i));
		group.second.priv = getText(1, i) == _T("Yes");
		group.second.connect = getText(2, i) == _T("Yes");
		groups.insert(group);*/
		name = Text::fromT(getText(0, i));
		group.priv = getText(1, i) == TSTRING(YES);
		groups.insert(make_pair(name, group));
	}
	FavoriteManager::getInstance()->setFavHubGroups(groups);
	FavoriteManager::getInstance()->save();
}

int FavHubGroupsDlg::findGroup(LPCTSTR name)
{
	for (int i = 0; i < ctrlGroups.GetItemCount(); ++i)
	{
		if (wcscmp(ctrlGroups.ExGetItemTextT(i, 0).c_str(), name) == 0)
		{
			return i;
		}
	}
	return -1;
}

void FavHubGroupsDlg::addItem(const tstring& name, bool priv, bool select /*= false*/)
{
	int item = ctrlGroups.InsertItem(ctrlGroups.GetItemCount(), name.c_str());
	ctrlGroups.SetItemText(item, 1, priv ? CTSTRING(YES) : CTSTRING(NO));
	if (select)
		ctrlGroups.SelectItem(item);
}

bool FavHubGroupsDlg::getItem(tstring& name, bool& priv, bool checkSel)
{
	WinUtil::GetWindowText(name, GetDlgItem(IDC_NAME));
	if (name.empty())
	{
		MessageBox(CTSTRING(ENTER_GROUP_NAME), CTSTRING(MANAGE_GROUPS), MB_ICONERROR);
		return false;
	}
	else
	{
		int pos = findGroup(name.c_str());
		if (pos != -1 && (checkSel == false || pos != ctrlGroups.GetSelectedIndex()))
		{
			MessageBox(CTSTRING(ITEM_EXIST), CTSTRING(MANAGE_GROUPS), MB_ICONERROR);
			return false;
		}
	}
	
	CButton wnd;
	wnd.Attach(GetDlgItem(IDC_PRIVATE));
	priv = wnd.GetCheck() == 1;
	wnd.Detach();
	
	return true;
}

tstring FavHubGroupsDlg::getText(const int column, const int item /*= -1*/)
{
	const int selection = item == -1 ? ctrlGroups.GetSelectedIndex() : item;
	if (selection >= 0)
	{
		return ctrlGroups.ExGetItemTextT(selection, column);
	}
	return Util::emptyStringT;
}

void FavHubGroupsDlg::updateSelectedGroup(bool forceClean /*= false*/)
{
	tstring name;
	bool priv = false;
	bool enableButtons = false;
	
	if (ctrlGroups.GetSelectedIndex() != -1)
	{
		if (forceClean == false)
		{
			name = getText(0);
			priv = getText(1) == TSTRING(YES);
		}
		enableButtons = true;
	}
	
	{
		CWindow wnd;
		wnd.Attach(GetDlgItem(IDC_REMOVE));
		wnd.EnableWindow(enableButtons);
		wnd.Detach();
		
		wnd.Attach(GetDlgItem(IDC_SAVE)); // [~] NightOrion.
		wnd.EnableWindow(enableButtons);
		wnd.Detach();
	}
	{
		CEdit wnd;
		wnd.Attach(GetDlgItem(IDC_NAME));
		wnd.SetWindowText(name.c_str());
		wnd.Detach();
	}
	{
		CButton wnd;
		wnd.Attach(GetDlgItem(IDC_PRIVATE));
		wnd.SetCheck(priv ? 1 : 0);
		wnd.Detach();
	}
}

LRESULT FavHubGroupsDlg::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring name;
	bool priv;
	if (getItem(name, priv, false))
	{
		addItem(name, priv, true);
		updateSelectedGroup(true);
	}
	return 0;
}

LRESULT FavHubGroupsDlg::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int pos = ctrlGroups.GetSelectedIndex();
	UINT checkState = BOOLSETTING(CONFIRM_HUBGROUP_REMOVAL) ? BST_UNCHECKED : BST_CHECKED; // [+] NightOrion.
	if (pos >= 0 && (checkState == BST_CHECKED || ::MessageBox(m_hWnd, CTSTRING(REALLY_REMOVE), _T(APPNAME) _T(" ") T_VERSIONSTRING, CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES)) // [~] NightOrion.
	{
		tstring name = getText(0, pos);
		FavoriteHubEntryList l = FavoriteManager::getInstance()->getFavoriteHubs(Text::fromT(name));
		if (!l.empty())
		{
			tstring msg;
			msg += TSTRING(GROUPS_GROUP);
			msg += _T(" '") + name + _T("' ");
			msg += TSTRING(GROUPS_CONTAINS) + _T(' ');
			msg += Util::toStringW(l.size());
			msg += TSTRING(GROUPS_REMOVENOTIFY);
			int remove = MessageBox(msg.c_str(), CTSTRING(GROUPS_REMOVEGROUP) /*_T("Remove Group")*/, MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON1);
			
			switch (remove)
			{
				case IDCANCEL:
					return 0;
				case IDYES:
					for (auto i = l.cbegin(); i != l.cend(); ++i) FavoriteManager::getInstance()->removeFavorite(*i);
					break;
				case IDNO:
					for (auto i = l.cbegin(); i != l.cend(); ++i)(*i)->setGroup(Util::emptyString);
					break;
			}
		}
		ctrlGroups.DeleteItem(pos);
		updateSelectedGroup(true);
	}
	// Let's update the setting unchecked box means we bug user again...
	SET_SETTING(CONFIRM_HUBGROUP_REMOVAL, checkState != BST_CHECKED); // [+] NightOrion.
	return 0;
}

LRESULT FavHubGroupsDlg::onUpdate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int item = ctrlGroups.GetSelectedIndex();
	if (item >= 0)
	{
		tstring name;
		tstring oldName = getText(0);
		bool priv;
		if (getItem(name, priv, true))
		{
			if (oldName != name)
			{
				FavoriteHubEntryList l = FavoriteManager::getInstance()->getFavoriteHubs(Text::fromT(oldName));
				const string l_newName = Text::fromT(name);
				for (auto i = l.cbegin(); i != l.cend(); ++i)
				{
					(*i)->setGroup(l_newName);
				}
			}
			ctrlGroups.DeleteItem(item);
			addItem(name, priv, true);
			updateSelectedGroup();
		}
	}
	return 0;
}
