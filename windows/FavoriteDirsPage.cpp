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

#include "../client/Util.h"
#include "Resource.h"
#include "FavoriteDirsPage.h"
#include "LineDlg.h"
#include "FavDirDlg.h"

// Кнопки и их переводы.
PropPage::TextItem FavoriteDirsPage::texts[] =
{
	{ IDC_SETTINGS_FAVORITE_DIRECTORIES, ResourceManager::SETTINGS_FAVORITE_DIRS },
	{ IDC_REMOVE, ResourceManager::REMOVE },
	{ IDC_ADD, ResourceManager::SETTINGS_ADD_FOLDER },
	{ IDC_RENAME, ResourceManager::RENAME },
	{ IDC_CHANGE, ResourceManager::SETTINGS_CHANGE },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT FavoriteDirsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	ctrlDirectories.Attach(GetDlgItem(IDC_FAVORITE_DIRECTORIES));
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlDirectories);
	SET_LIST_COLOR_IN_SETTING(ctrlDirectories);
	
	// Prepare shared dir list
	CRect rc;
	ctrlDirectories.GetClientRect(rc);
	ctrlDirectories.InsertColumn(0, CTSTRING(FAVORITE_DIR_NAME), LVCFMT_LEFT, rc.Width() / 4, 0);
	ctrlDirectories.InsertColumn(1, CTSTRING(DIRECTORY), LVCFMT_LEFT, rc.Width() * 2 / 4, 1);
	ctrlDirectories.InsertColumn(2, CTSTRING(SETTINGS_EXTENSIONS), LVCFMT_LEFT, rc.Width() / 4, 2);
	FavoriteManager::LockInstanceDirs lockedInstance;
	const auto& directories = lockedInstance.getFavoriteDirsL();
	auto cnt = ctrlDirectories.GetItemCount();
	for (auto j = directories.cbegin(); j != directories.cend(); ++j)
	{
		int i = ctrlDirectories.insert(cnt++, Text::toT(j->name));
		ctrlDirectories.SetItemText(i, 1, Text::toT(j->dir).c_str());
		ctrlDirectories.SetItemText(i, 2, Text::toT(Util::toSettingString(j->ext)).c_str());
	}
	
	return TRUE;
}


void FavoriteDirsPage::write() { }

LRESULT FavoriteDirsPage::onDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	HDROP drop = (HDROP)wParam;
	AutoArray <TCHAR> buf(FULL_MAX_PATH);
	UINT nrFiles;
	
	nrFiles = DragQueryFile(drop, (UINT) - 1, NULL, 0);
	
	for (UINT i = 0; i < nrFiles; ++i)
	{
		// TODO добавить поддержку длинных путей
		if (DragQueryFile(drop, i, buf, FULL_MAX_PATH))
		{
			if (PathIsDirectory(buf))
				addDirectory(buf.data());
		}
	}
	
	DragFinish(drop);
	
	return 0;
}

// Активация неактивных кнопок.
LRESULT FavoriteDirsPage::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_REMOVE), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_RENAME), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_CHANGE), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

// Управление клавишами.
LRESULT FavoriteDirsPage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

// Управление мышью.
LRESULT FavoriteDirsPage::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	
	if (item->iItem >= 0)
	{
		PostMessage(WM_COMMAND, IDC_CHANGE, 0);
	}
	else if (item->iItem == -1)
	{
		PostMessage(WM_COMMAND, IDC_ADD, 0);
	}
	
	return 0;
}

// При добавлении.
LRESULT FavoriteDirsPage::onClickedAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	addDirectory();
	return 0;
}

// При удалении.
LRESULT FavoriteDirsPage::onClickedRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlDirectories.GetSelectedCount())
	{
		LocalArray< TCHAR, MAX_PATH > buf;
		LVITEM item = {0};
		item.mask = LVIF_TEXT;
		item.cchTextMax = static_cast<int>(buf.size());
		item.pszText = buf.data();
		
		int i = -1;
		while ((i = ctrlDirectories.GetNextItem(-1, LVNI_SELECTED)) != -1)
		{
			item.iItem = i;
			item.iSubItem = 1;
			ctrlDirectories.GetItem(&item);
			if (FavoriteManager::removeFavoriteDir(Text::fromT(buf.data())))
				ctrlDirectories.DeleteItem(i);
		}
	}
	return 0;
}

// При переименовании. Можно переименовать в onClickedChange. Но там почему-то не сохраняется.
LRESULT FavoriteDirsPage::onClickedRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LocalArray<TCHAR, MAX_PATH> buf;
	LVITEM item = {0};
	item.mask = LVIF_TEXT;
	item.cchTextMax = static_cast<int>(buf.size());
	item.pszText = buf.data();
	
	int i = -1;
	while ((i = ctrlDirectories.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		item.iItem = i;
		item.iSubItem = 0;
		ctrlDirectories.GetItem(&item);
		
		LineDlg virt;
		virt.title = TSTRING(FAVORITE_DIR_NAME);
		virt.description = TSTRING(FAVORITE_DIR_NAME_LONG);
		virt.line = tstring(buf.data());
		if (virt.DoModal(m_hWnd) == IDOK)
		{
			if (FavoriteManager::renameFavoriteDir(Text::fromT(buf.data()), Text::fromT(virt.line)))
			{
				ctrlDirectories.SetItemText(i, 0, virt.line.c_str());
			}
			else
			{
				MessageBox(CTSTRING(DIRECTORY_ADD_ERROR));
			}
		}
	}
	return 0;
}

// При изменении.
LRESULT FavoriteDirsPage::onClickedChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LocalArray<TCHAR, MAX_PATH> buf;
	LVITEM item = { 0 };
	item.mask = LVIF_TEXT;
	item.cchTextMax = buf.size();
	item.pszText = buf.data();
	
	int i = -1;
	while ((i = ctrlDirectories.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		item.iItem = i;
		FavDirDlg dlg;
		item.iSubItem = 0;
		ctrlDirectories.GetItem(&item);
		dlg.name = tstring(buf.data());
		tstring l_lastname = tstring(buf.data());
		item.iSubItem = 1;
		ctrlDirectories.GetItem(&item);
		dlg.dir = tstring(buf.data());
		item.iSubItem = 2;
		ctrlDirectories.GetItem(&item);
		dlg.extensions = tstring(buf.data());
		if (dlg.DoModal(m_hWnd) == IDOK)
		{
			// [!] SSA Fixed problem with deadlock for folders w/o path separator
			AppendPathSeparator(dlg.dir);
			if (l_lastname != dlg.name)
			{
				if (FavoriteManager::renameFavoriteDir(Text::fromT(l_lastname), Text::fromT(dlg.name)))
				{
					ctrlDirectories.SetItemText(i, 0, dlg.name.c_str());
				}
			}
			if (FavoriteManager::updateFavoriteDir(Text::fromT(dlg.name), Text::fromT(dlg.dir), Text::fromT(dlg.extensions)))
			{
				//  ctrlDirectories.SetItemText(i, 0, dlg.name.c_str());
				ctrlDirectories.SetItemText(i, 1, dlg.dir.c_str());
				ctrlDirectories.SetItemText(i, 2, dlg.extensions.c_str());
			}
			else
			{
				MessageBox(CTSTRING(DIRECTORY_ADD_ERROR));
			}
		}
	}
	return 0;
}

// При добавлении директории.
void FavoriteDirsPage::addDirectory(const tstring& aPath /*= Util::emptyStringT*/)
{
	tstring path = aPath;
	AppendPathSeparator(path); //[+]PPA
	
	FavDirDlg dlg;
	dlg.name = Util::getLastDir(path);
	dlg.dir = path;
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		// [!] SSA Fixed problem with deadlock for folders w/o path separator
		tstring tdir = dlg.dir;
		AppendPathSeparator(tdir);
		
		if (FavoriteManager::addFavoriteDir(Text::fromT(tdir), Text::fromT(dlg.name), Text::fromT(dlg.extensions)))
		{
			int j = ctrlDirectories.insert(ctrlDirectories.GetItemCount(), dlg.name);
			ctrlDirectories.SetItemText(j, 1, tdir.c_str());
			ctrlDirectories.SetItemText(j, 2, dlg.extensions.c_str());
		}
		else
		{
			MessageBox(CTSTRING(DIRECTORY_ADD_ERROR));
		}
	}
}

/**
 * @file
 * $Id: FavoriteDirsPage.cpp 477 2010-01-29 08:59:43Z bigmuscle $
 */
