/*
* Copyright (C) 2003 Twink,  spm7@waikato.ac.nz
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

#include "../client/SettingsManager.h"
#include "AVIPreviewPage.h"
#include "PreviewDlg.h"

PropPage::TextItem AVIPreview::texts[] =
{
	{ IDC_ADD_MENU, ResourceManager::ADD },
	{ IDC_CHANGE_MENU, ResourceManager::SETTINGS_CHANGE },
	{ IDC_REMOVE_MENU, ResourceManager::REMOVE },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};


LRESULT AVIPreview::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	
	CRect rc;
	
	ctrlCommands.Attach(GetDlgItem(IDC_MENU_ITEMS));
	ctrlCommands.GetClientRect(rc);
	
	ctrlCommands.InsertColumn(0, CTSTRING(SETTINGS_NAME), LVCFMT_LEFT, rc.Width() / 5, 0);
	ctrlCommands.InsertColumn(1, CTSTRING(SETTINGS_COMMAND), LVCFMT_LEFT, rc.Width() * 2 / 5, 1);
	ctrlCommands.InsertColumn(2, CTSTRING(SETTINGS_ARGUMENT), LVCFMT_LEFT, rc.Width() / 5, 2);
	ctrlCommands.InsertColumn(3, CTSTRING(SETTINGS_EXTENSIONS), LVCFMT_LEFT, rc.Width() / 5, 3);
	
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlCommands);
	SET_LIST_COLOR_IN_SETTING(ctrlCommands);
	
	// Do specialized reading here
	const PreviewApplication::List lst = FavoriteManager::getPreviewApps();
	auto cnt = ctrlCommands.GetItemCount();
	for (auto i = lst.cbegin(); i != lst.cend(); ++i)
	{
		PreviewApplication::Ptr pa = *i;
		addEntry(pa, cnt++);
	}
	checkMenu();
	return 0;
}

void AVIPreview::addEntry(PreviewApplication::Ptr pa, int pos)
{
	TStringList lst;
	lst.push_back(Text::toT(pa->getName()));
	lst.push_back(Text::toT(pa->getApplication()));
	lst.push_back(Text::toT(pa->getArguments()));
	lst.push_back(Text::toT(pa->getExtension()));
	ctrlCommands.insert(pos, lst, 0, 0);
}

void AVIPreview::checkMenu()
{
	bool l_yopta = false;
	if (ctrlCommands.GetItemCount() > 0 && ctrlCommands.GetSelectedCount() == 1)
		l_yopta = true;
		
	::EnableWindow(GetDlgItem(IDC_CHANGE_MENU), l_yopta);
	::EnableWindow(GetDlgItem(IDC_REMOVE_MENU), l_yopta);
}

LRESULT AVIPreview::onAddMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PreviewDlg dlg;
	if (dlg.DoModal() == IDOK)
	{
		addEntry(FavoriteManager::addPreviewApp(Text::fromT(dlg.m_name),
		                                        Text::fromT(dlg.m_application),
		                                        Text::fromT(dlg.m_argument),
		                                        Text::fromT(dlg.m_extensions)), ctrlCommands.GetItemCount());
	}
	checkMenu();
	return 0;
}

LRESULT AVIPreview::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	//NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	checkMenu();
	//::EnableWindow(GetDlgItem(IDC_CHANGE_MENU), (lv->uNewState & LVIS_FOCUSED));
	//::EnableWindow(GetDlgItem(IDC_REMOVE_MENU), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

LRESULT AVIPreview::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD_MENU, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE_MENU, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT AVIPreview::onChangeMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int sel = ctrlCommands.GetSelectedIndex();
		const auto pa = FavoriteManager::getInstance()->getPreviewApp(sel);
		if (pa)
		{
			PreviewDlg dlg;
			dlg.m_name = Text::toT(pa->getName());
			dlg.m_application = Text::toT(pa->getApplication());
			dlg.m_argument = Text::toT(pa->getArguments());
			dlg.m_extensions = Text::toT(pa->getExtension());
			
			if (dlg.DoModal() == IDOK)
			{
				pa->setName(Text::fromT(dlg.m_name));
				pa->setApplication(Text::fromT(dlg.m_application));
				pa->setArguments(Text::fromT(dlg.m_argument));
				pa->setExtension(Text::fromT(dlg.m_extensions));
				
				ctrlCommands.SetItemText(sel, 0, dlg.m_name.c_str());
				ctrlCommands.SetItemText(sel, 1, dlg.m_application.c_str());
				ctrlCommands.SetItemText(sel, 2, dlg.m_argument.c_str());
				ctrlCommands.SetItemText(sel, 3, dlg.m_extensions.c_str());
			}
		}
	}
	
	return 0;
}

LRESULT AVIPreview::onRemoveMenu(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int sel = ctrlCommands.GetSelectedIndex();
		FavoriteManager::removePreviewApp(sel);
		ctrlCommands.DeleteItem(sel);
	}
	checkMenu();
	return 0;
}