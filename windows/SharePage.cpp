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

#include "SharePage.h"
#include "HashProgressDlg.h"
#include "LineDlg.h"

#include "../client/Util.h"
#include "../client/ShareManager.h"

PropPage::TextItem SharePage::texts[] =
{
	{ IDC_SETTINGS_SHARED_DIRECTORIES, ResourceManager::SETTINGS_SHARED_DIRECTORIES },
	{ IDC_SETTINGS_SHARE_SIZE, ResourceManager::SETTINGS_SHARE_SIZE },
	{ IDC_SHAREHIDDEN, ResourceManager::SETTINGS_SHARE_HIDDEN },
// [+]IRainman
	{ IDC_SHARESYSTEM, ResourceManager::SETTINGS_SHARE_SYSTEM },
	{ IDC_SHAREVIRTUAL, ResourceManager::SETTINGS_SHARE_VIRTUAL },
// ~[+]IRainman
	{ IDC_REMOVE, ResourceManager::REMOVE },
	{ IDC_ADD, ResourceManager::SETTINGS_ADD_FOLDER },
	{ IDC_RENAME, ResourceManager::RENAME },
	{ IDC_SETTINGS_UPLOADS_MIN_SPEED, ResourceManager::SETTINGS_UPLOADS_MIN_SPEED },
	{ IDC_SETTINGS_KBPS, ResourceManager::KBPS },
	{ IDC_SETTINGS_ONLY_HASHED, ResourceManager::SETTINGS_ONLY_HASHED },
	{ IDC_SETTINGS_SKIPLIST, ResourceManager::SETTINGS_SKIPLIST_SHARE },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item SharePage::items[] =
{
	{ IDC_SHAREHIDDEN, SettingsManager::SHARE_HIDDEN, PropPage::T_BOOL },
// [+]IRainman
	{ IDC_SHARESYSTEM, SettingsManager::SHARE_SYSTEM, PropPage::T_BOOL },
	{ IDC_SHAREVIRTUAL, SettingsManager::SHARE_VIRTUAL, PropPage::T_BOOL },
// ~[+]IRainman
	{ IDC_SKIPLIST_SHARE, SettingsManager::SKIPLIST_SHARE, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

LRESULT SharePage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	
	GetDlgItem(IDC_TREE).ShowWindow((BOOLSETTING(USE_OLD_SHARING_UI)) ? SW_HIDE : SW_SHOW);
	GetDlgItem(IDC_DIRECTORIES).ShowWindow((BOOLSETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_ADD).ShowWindow((BOOLSETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_REMOVE).ShowWindow((BOOLSETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	GetDlgItem(IDC_RENAME).ShowWindow((BOOLSETTING(USE_OLD_SHARING_UI)) ? SW_SHOW : SW_HIDE);
	
	ctrlDirectories.Attach(GetDlgItem(IDC_DIRECTORIES));
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlDirectories);
#ifdef USE_SET_LIST_COLOR_IN_SETTINGS
	SET_LIST_COLOR_IN_SETTING(ctrlDirectories);
#endif
	
	ctrlTotal.Attach(GetDlgItem(IDC_TOTAL));
	
	PropPage::read((HWND)*this, items);
	
	if (BOOLSETTING(USE_OLD_SHARING_UI))
	{
		// Prepare shared dir list
		ctrlDirectories.InsertColumn(0, CTSTRING(VIRTUAL_NAME), LVCFMT_LEFT, 80, 0);
		ctrlDirectories.InsertColumn(1, CTSTRING(DIRECTORY), LVCFMT_LEFT, 197, 1);
		ctrlDirectories.InsertColumn(2, CTSTRING(SIZE), LVCFMT_RIGHT, 90, 2);
	}
	directoryListInit();
	
	ft.SubclassWindow(GetDlgItem(IDC_TREE));
	ft.SetStaticCtrl(&ctrlTotal);
	if (!BOOLSETTING(USE_OLD_SHARING_UI))
		ft.PopulateTree();
		
	return TRUE;
}

LRESULT SharePage::onDropFiles(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
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

void SharePage::write()
{
	PropPage::write((HWND)*this, items);
	
	if (!BOOLSETTING(USE_OLD_SHARING_UI) && ft.IsDirty())
	{
		ShareManager::getInstance()->setDirty();
		ShareManager::getInstance()->refresh_share(true);
	}
	else
	{
		ShareManager::getInstance()->refresh_share();
	}
}

LRESULT SharePage::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_REMOVE), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_RENAME), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

LRESULT SharePage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
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

LRESULT SharePage::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	
	if (item->iItem >= 0)
	{
		PostMessage(WM_COMMAND, IDC_RENAME, 0);
	}
	else if (item->iItem == -1)
	{
		PostMessage(WM_COMMAND, IDC_ADD, 0);
	}
	
	return 0;
}

LRESULT SharePage::onClickedAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring target;
	if (WinUtil::browseDirectory(target, m_hWnd))
	{
		addDirectory(target);
		if (!HashProgressDlg::g_is_execute) // fix http://code.google.com/p/flylinkdc/issues/detail?id=1126
		{
			HashProgressDlg(true).DoModal();
		}
	}
	
	return 0;
}

LRESULT SharePage::onClickedRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AutoArray <TCHAR> buf(FULL_MAX_PATH);
	LVITEM item = {0};
	item.mask = LVIF_TEXT;
	item.cchTextMax = FULL_MAX_PATH;
	item.pszText = buf.data();
	
	int i = -1;
	while ((i = ctrlDirectories.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		item.iItem = i;
		item.iSubItem = 1;
		ctrlDirectories.GetItem(&item);
		ShareManager::getInstance()->removeDirectory(Text::fromT(buf.data()));
		ctrlTotal.SetWindowText(ShareManager::getShareSizeformatBytesW().c_str());
		ctrlDirectories.DeleteItem(i);
	}
	
	return 0;
}

LRESULT SharePage::onClickedRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	AutoArray <TCHAR> buf(FULL_MAX_PATH);
	LVITEM item = {0};
	item.mask = LVIF_TEXT;
	item.cchTextMax = FULL_MAX_PATH;
	item.pszText = buf.data();
	
	bool setDirty = false;
	
	int i = -1;
	while ((i = ctrlDirectories.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		item.iItem = i;
		item.iSubItem = 0;
		ctrlDirectories.GetItem(&item);
		tstring vName = buf.data();
		item.iSubItem = 1;
		ctrlDirectories.GetItem(&item);
		tstring rPath = buf.data();
		try
		{
			LineDlg virt;
			virt.title = TSTRING(VIRTUAL_NAME);
			virt.description = TSTRING(VIRTUAL_NAME_LONG);
			virt.line = vName;
			if (virt.DoModal(m_hWnd) == IDOK)
			{
				if (stricmp(buf.data(), virt.line) != 0)
				{
					ShareManager::getInstance()->renameDirectory(Text::fromT(rPath), Text::fromT(virt.line));
					ctrlDirectories.SetItemText(i, 0, virt.line.c_str());
					
					setDirty = true;
				}
				else
				{
					MessageBox(CTSTRING(SKIP_RENAME), T_APPNAME_WITH_VERSION, MB_ICONINFORMATION | MB_OK);
				}
			}
		}
		catch (const ShareException& e)
		{
			MessageBox(Text::toT(e.getError()).c_str(), T_APPNAME_WITH_VERSION, MB_ICONSTOP | MB_OK);
		}
	}
	
	if (setDirty)
		ShareManager::getInstance()->setDirty();
		
	return 0;
}

LRESULT SharePage::onClickedShareHidden(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	return onClickedShare(0);
}

LRESULT SharePage::onClickedShareSystem(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	return onClickedShare(1);
}

LRESULT SharePage::onClickedShareVirtual(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	return onClickedShare(2);
}

void SharePage::directoryListInit()
{
	if (BOOLSETTING(USE_OLD_SHARING_UI))
	{
		// Clear the GUI list, for insertion of updated shares
		ctrlDirectories.DeleteAllItems();
		CFlyDirItemArray directories;
		ShareManager::getDirectories(directories);
		
		auto cnt = ctrlDirectories.GetItemCount();
		for (auto j = directories.cbegin(); j != directories.cend(); ++j)
		{
			int i = ctrlDirectories.insert(cnt++, Text::toT(j->m_synonym));
			ctrlDirectories.SetItemText(i, 1, Text::toT(j->m_path).c_str());
			ctrlDirectories.SetItemText(i, 2, Util::formatBytesW(ShareManager::getShareSize(j->m_path)).c_str());
		}
	}
	// Display the new total share size
	ctrlTotal.SetWindowText(ShareManager::getShareSizeformatBytesW().c_str());
}
LRESULT SharePage::onClickedShare(int item)
{
	// Save the checkbox state so that ShareManager knows to include/exclude hidden files
	Item i = items[item]; // The checkbox. Explicit index used - bad!
	if (::IsDlgButtonChecked((HWND)* this, i.itemID) == BST_CHECKED)
	{
		g_settings->set((SettingsManager::IntSetting)i.setting, true);
	}
	else
	{
		g_settings->set(SettingsManager::IntSetting(i.setting), false);
	}
	
	// Refresh the share. This is a blocking refresh. Might cause problems?
	// Hopefully people won't click the checkbox enough for it to be an issue. :-)
	ShareManager::getInstance()->setDirty();
	ShareManager::getInstance()->refresh_share(true, false);
	directoryListInit();
	return 0;
}

void SharePage::addDirectory(const tstring& aPath)
{
	tstring path = aPath;
	
	AppendPathSeparator(path);
	
	//if (path.length()) //[+]PPA
	//  if (path[ path.length() - 1 ] != _T('\\'))
	//      path += _T('\\');
	
	try
	{
		LineDlg virt;
		virt.title = TSTRING(VIRTUAL_NAME);
		virt.description = TSTRING(VIRTUAL_NAME_LONG);
		virt.line = Text::toT(ShareManager::validateVirtual(
		                          Util::getLastDir(Text::fromT(path))));
		if (virt.DoModal(m_hWnd) == IDOK)
		{
			CWaitCursor l_cursor_wait; //-V808
			ShareManager::getInstance()->addDirectory(Text::fromT(path), Text::fromT(virt.line), true);
			int i = ctrlDirectories.insert(ctrlDirectories.GetItemCount(), virt.line);
			ctrlDirectories.SetItemText(i, 1, path.c_str());
			ctrlDirectories.SetItemText(i, 2, Util::formatBytesW(ShareManager::getShareSize(Text::fromT(path))).c_str());
			ctrlTotal.SetWindowText(ShareManager::getShareSizeformatBytesW().c_str());
		}
	}
	catch (const ShareException& e)
	{
		MessageBox(Text::toT(e.getError()).c_str(), T_APPNAME_WITH_VERSION, MB_ICONSTOP | MB_OK);
	}
}

/**
 * @file
 * $Id: UploadPage.cpp,v 1.36 2006/10/15 10:53:39 bigmuscle Exp $
 * $Id: SharePage.h, 2013/07/19 FlylinkDC++ Team $
 */
