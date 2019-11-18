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

#include "ToolbarPage.h"
#include "MainFrm.h"

PropPage::TextItem ToolbarPage::texts[] =
{
	{ IDC_MOUSE_OVER, ResourceManager::SETTINGS_MOUSE_OVER },
	{ IDC_NORMAL, ResourceManager::SETTINGS_NORMAL },
	{ IDC_TOOLBAR_IMAGE_BOX, ResourceManager::SETTINGS_TOOLBAR_IMAGE },
	{ IDC_TOOLBAR_ADD, ResourceManager::SETTINGS_TOOLBAR_ADD },
	{ IDC_TOOLBAR_REMOVE, ResourceManager::SETTINGS_TOOLBAR_REMOVE },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item ToolbarPage::items[] =
{
	{ IDC_TOOLBAR_IMAGE, SettingsManager::TOOLBARIMAGE, PropPage::T_STR },
	{ IDC_TOOLBAR_HOT_IMAGE, SettingsManager::TOOLBARHOTIMAGE, PropPage::T_STR },
	{ IDC_ICON_SIZE, SettingsManager::TB_IMAGE_SIZE, PropPage::T_INT },
	{ IDC_ICON_SIZE_HOVER, SettingsManager::TB_IMAGE_SIZE_HOT, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};


string ToolbarPage::filter(string s)
{
//  s = Util::replace(s, '&',"");
	s = s.substr(0, s.find('\t'));
	return s;
}

LRESULT ToolbarPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items);
	
	m_ctrlCommands.Attach(GetDlgItem(IDC_TOOLBAR_POSSIBLE));
	CRect rc;
	m_ctrlCommands.GetClientRect(rc);
	m_ctrlCommands.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
	
	m_ctrlCommands.SetImageList(MainFrame::getMainFrame()->largeImages, LVSIL_SMALL);
	
	
	LVITEM lvi = {0};
	lvi.mask = LVIF_TEXT | LVIF_IMAGE;
	lvi.iSubItem = 0;
	
	for (int i = -1; i < 0 || g_ToolbarButtons[i].id != 0; i++)
	{
		makeItem(&lvi, i);
		lvi.iItem = i + 1;
		m_ctrlCommands.InsertItem(&lvi);
		m_ctrlCommands.SetItemData(lvi.iItem, i);
	}
	m_ctrlCommands.SetColumnWidth(0, LVSCW_AUTOSIZE);
	
	m_ctrlToolbar.Attach(GetDlgItem(IDC_TOOLBAR_ACTUAL));
	m_ctrlToolbar.GetClientRect(rc);
	m_ctrlToolbar.InsertColumn(0, _T("Dummy"), LVCFMT_LEFT, rc.Width(), 0);
	m_ctrlToolbar.SetImageList(MainFrame::getMainFrame()->largeImagesHot, LVSIL_SMALL);
	
	const StringTokenizer<string> t(SETTING(TOOLBAR), ',');
	const StringList& l = t.getTokens();
	
	int n = 0;
	for (auto k = l.cbegin(); k != l.cend(); ++k)
	{
		int i = Util::toInt(*k);
		const int l_cnt = g_cout_of_ToolbarButtons;
		if (i < l_cnt)
		{
			makeItem(&lvi, i);
			lvi.iItem = n++;
			m_ctrlToolbar.InsertItem(&lvi);
			m_ctrlToolbar.SetItemData(lvi.iItem, i);
			
			// disable items that are already in toolbar,
			// to avoid duplicates
			if (i != -1)
				m_ctrlCommands.SetItemState(i + 1, LVIS_CUT, LVIS_CUT);
		}
	}
	
	m_ctrlToolbar.SetColumnWidth(0, LVSCW_AUTOSIZE);
	
	return TRUE;
}

void ToolbarPage::write()
{
	PropPage::write(*this, items);
	string toolbar;
	for (int i = 0; i < m_ctrlToolbar.GetItemCount(); i++)
	{
		if (i != 0)toolbar += ",";
		const int j = m_ctrlToolbar.GetItemData(i);
		toolbar += Util::toString(j);
	}
	if (toolbar != g_settings->get(SettingsManager::TOOLBAR))
	{
		g_settings->set(SettingsManager::TOOLBAR, toolbar);
		dcassert(WinUtil::g_mainWnd);
		if (WinUtil::g_mainWnd)
			::SendMessage(WinUtil::g_mainWnd, IDC_REBUILD_TOOLBAR, 0, 0);
	}
}

void ToolbarPage::BrowseForPic(int DLGITEM)
{
	tstring x;
	
	GET_TEXT(DLGITEM, x);
	
	if (WinUtil::browseFile(x, m_hWnd, false) == IDOK)
	{
		SetDlgItemText(DLGITEM, x.c_str());
	}
}

LRESULT ToolbarPage::onImageBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BrowseForPic(IDC_TOOLBAR_IMAGE);
	return 0;
}

LRESULT ToolbarPage::onHotBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BrowseForPic(IDC_TOOLBAR_HOT_IMAGE);
	return 0;
}

string name;
void ToolbarPage::makeItem(LPLVITEM lvi, int item)
{
	const int l_cnt = g_cout_of_ToolbarButtons;
	if (item > -1 && item < l_cnt) // [!] Идентификаторы отображаемых иконок: первый (-1 - разделитель) и последний (кол-во иконок начиная с 0) для панели инструментов.
	{
		lvi->iImage = g_ToolbarButtons[item].image;
		name = Text::toT(filter(ResourceManager::getString(g_ToolbarButtons[item].tooltip)));
	}
	else
	{
		name = TSTRING(SEPARATOR);
		lvi->iImage = -1;
	}
	lvi->pszText = const_cast<TCHAR*>(name.c_str());
}

LRESULT ToolbarPage::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_ctrlCommands.GetSelectedCount() == 1)
	{
		int iSelectedInd = m_ctrlCommands.GetSelectedIndex();
		bool bAlreadyExist = false;
		
		if (iSelectedInd != 0)
		{
			LVFINDINFO lvifi = { 0 };
			lvifi.flags  = LVFI_PARAM;
			lvifi.lParam = iSelectedInd - 1;
			const int iFoundInd = m_ctrlToolbar.FindItem(&lvifi, -1);
			
			if (iFoundInd != -1)
			{
				// item already in toolbar,
				// don't add new item, but hilite old one
				m_ctrlToolbar.SetItemState(iFoundInd, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
				bAlreadyExist = true;
			}
		}
		
		if (!bAlreadyExist)
		{
			// add new item to toolbar
			
			LVITEM lvi = {0};
			lvi.mask = LVIF_TEXT | LVIF_IMAGE;
			lvi.iSubItem = 0;
			const int i = m_ctrlCommands.GetItemData(iSelectedInd);
			makeItem(&lvi, i);
			lvi.iItem = m_ctrlToolbar.GetSelectedIndex() + 1;//ctrlToolbar.GetSelectedIndex()>0?ctrlToolbar.GetSelectedIndex():ctrlToolbar.GetItemCount();
			m_ctrlToolbar.InsertItem(&lvi);
			m_ctrlToolbar.SetItemData(lvi.iItem, i);
			m_ctrlCommands.SetItemState(i + 1, LVIS_CUT, LVIS_CUT);
		}
	}
	return 0;
}

LRESULT ToolbarPage::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_ctrlToolbar.GetSelectedCount() == 1)
	{
		const int sel = m_ctrlToolbar.GetSelectedIndex();
		const int ind = m_ctrlToolbar.GetItemData(sel);
		m_ctrlToolbar.DeleteItem(sel);
		m_ctrlToolbar.SelectItem(std::max(sel - 1, 0));
		m_ctrlCommands.SetItemState(ind + 1, 0, LVIS_CUT);
	}
	return 0;
}
