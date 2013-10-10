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

#include "TreePropertySheet.h"
#include "ResourceLoader.h" // [+] InfinitySky. PNG Support from Apex 1.3.8.
#include "WinUtil.h"
#include "../client/ResourceManager.h"

static const TCHAR SEPARATOR = _T('\\');

int TreePropertySheet::PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
	if (uMsg == PSCB_INITIALIZED)
	{
		::PostMessage(hwndDlg, WM_USER_INITDIALOG, 0, 0);
	}
	
	return CPropertySheet::PropSheetCallback(hwndDlg, uMsg, lParam);
}

LRESULT TreePropertySheet::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
{
	/* [-] IRainman fix.
	if (ResourceManager::getInstance()->isRTL())
	    SetWindowLongPtr(GWL_EXSTYLE, GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYOUTRTL);
	*/
	
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
	m_SliderPos = 255;
	setTransp(/*SETTING(PROPPAGE_TRANSP)*/ m_SliderPos);
#endif
	ResourceLoader::LoadImageList(IDR_SETTINGS_ICONS, tree_icons, 16, 16);
	hideTab();
	if (BOOLSETTING(REMEMBER_SETTINGS_WINDOW_POS))
	{
		CRect rcWindow;
		GetWindowRect(rcWindow);
		ScreenToClient(rcWindow);
		rcWindow.left = SETTING(SETTINGS_WINDOW_POS_X);
		rcWindow.top = SETTING(SETTINGS_WINDOW_POS_Y);
		rcWindow.right = /*rcWindow.left +*/ SETTING(SETTINGS_WINDOW_SIZE_X);
		rcWindow.bottom = /*rcWindow.top +*/ SETTING(SETTINGS_WINDOW_SIZE_YY);
		if (rcWindow.right > 0 && rcWindow.bottom > 0)
		{
			MoveWindow(rcWindow, TRUE);
		}
		else
			CenterWindow(GetParent());
	}
	if (!BOOLSETTING(REMEMBER_SETTINGS_WINDOW_POS))
		CenterWindow(GetParent());
	addTree();
	fillTree();
#ifdef SCALOLAZ_PROPPAGE_HELPLINK
	if (BOOLSETTING(SETTINGS_WINDOW_WIKIHELP))
		addHelp();
#endif
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
	if (BOOLSETTING(SETTINGS_WINDOW_TRANSP))
		addTransparency();
#endif
	return 0;
}
#ifdef SCALOLAZ_PROPPAGE_HELPLINK
void TreePropertySheet::addHelp()
{
	LPCTSTR help_str = CTSTRING(PROPERTIES_WIKIHELP);
	m_Help.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE /*| MB_ICONINFORMATION */, 0, IDC_PROPPAGE_WIKIHELP);
	m_Help.SetFont(GetFont());
	CRect rectok, rectok2, wind2;
	CWindow idok = GetDlgItem(IDOK);
	idok.GetWindowRect(rectok);
	idok.GetWindowRect(rectok2);
	GetWindowRect(wind2);
	ScreenToClient(rectok);
	ScreenToClient(rectok2);
	ScreenToClient(wind2);
	int width = WinUtil::getTextWidth((tstring) help_str, m_hWnd);     // calculate a center X
	rectok2.left = ((wind2.right - wind2.left) / 2) - (width / 2) + 20;
	rectok2.right = rectok2.left + width;
	if (rectok2.right > rectok.left) rectok2.right = rectok.left - 5;
	rectok2.top += 5;
	m_Help.MoveWindow(rectok2);
	m_Help.SetLabel(help_str);
	genHelpLink(HwndToIndex(GetActivePage()));
	idok.MoveWindow(rectok);
}


void TreePropertySheet::genHelpLink(int p_page)
{
	const TCHAR* l_HelpLinkTable[] =
	{
		L"general",
		L"isp",
		L"connection_settings",
		L"proxy_settings",
		L"downloads",
		L"favirites_directiries",
		L"preview",
		L"queue",
		L"priority",
		L"share",
		L"slots",
		L"messages",
		L"ui_settings",
		L"color_fonts",
		L"progress_bar",
		L"user_list_color",
		L"baloon_popups",
		L"sounds",
		L"toolbar",
		L"windows",
		L"tabs",
		L"advanced",
		L"experts",
		L"default_click",
		L"logs",
		L"user_commands",
		L"speed_limit",
		L"autoban",
		L"clients",
		L"rss_properties",
		L"security",
		L"misc",
		L"ipfilter",
		L"",
		L"webserver",
		L"autoupdate",
		L"dcls",
		L"intergration",
		L"internal_preview",
		L"messages_advanced",
		L"sharemisc",
		L"searchpage"
	};
	if (p_page < 0 || p_page >= _countof(l_HelpLinkTable) /*39*/)
		p_page = 0;
		
	const tstring l_url = WinUtil::GetWikiLink() + tstring(l_HelpLinkTable[p_page]);
	
	m_Help.SetHyperLink(l_url.c_str());
	m_Help.SetHyperLinkExtendedStyle(/*HLINK_LEFTIMAGE |*/ HLINK_UNDERLINEHOVER);
}
#endif

#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
void TreePropertySheet::addTransparency()
{
	CRect rectok, wind;
	CWindow idok = GetDlgItem(IDOK);
	idok.GetWindowRect(rectok);
	GetWindowRect(wind);
	ScreenToClient(rectok);
	ScreenToClient(wind);
	rectok.left = wind.left + 7;
	rectok.right = rectok.left + 80;
	m_Slider.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_BOTH | TBS_NOTICKS, 0, IDC_PROPPAGE_TRANSPARENCY);
	m_Slider.MoveWindow(rectok);
	m_Slider.SetRangeMin(50, true);
	m_Slider.SetRangeMax(255, true);
	m_Slider.SetPos(/*SETTING(PROPPAGE_TRANSP)*/ m_SliderPos);
	m_tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	m_tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(m_tooltip.IsWindow());
	m_tooltip.AddTool(m_Slider, ResourceManager::TRANSPARENCY_PROPPAGE);
	if (!BOOLSETTING(POPUPS_DISABLED)) m_tooltip.Activate(TRUE);
}
void TreePropertySheet::setTransp(int p_Layered)
{
#ifdef FLYLINKDC_SUPPORT_WIN_2000
	if (LOBYTE(LOWORD(GetVersion())) >= 5)
#endif
	{
		SetWindowLongPtr(GWL_EXSTYLE, GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYERED /*| WS_EX_TRANSPARENT*/);
		typedef bool (CALLBACK * LPFUNC)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
		LPFUNC _d_SetLayeredWindowAttributes = (LPFUNC)GetProcAddress(LoadLibrary(_T("user32")), "SetLayeredWindowAttributes");
		_d_SetLayeredWindowAttributes(m_hWnd, 0, p_Layered, LWA_ALPHA);
	}
}
LRESULT TreePropertySheet::onTranspChanged(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	setTransp(m_Slider.GetPos());
	return 0;
}
#endif
void TreePropertySheet::hideTab()
{
	CRect rcClient, rcTab, rcPage, rcWindow;
	CWindow tab = GetTabControl();
	CWindow page = IndexToHwnd(0);
	GetClientRect(&rcClient);
	tab.GetWindowRect(&rcTab);
	tab.ShowWindow(SW_HIDE);
	page.GetClientRect(&rcPage);
	page.MapWindowPoints(m_hWnd, &rcPage);
	GetWindowRect(&rcWindow);
	::MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rcTab, 2);
	
	ScrollWindow(SPACE_LEFT + TREE_WIDTH + SPACE_MID - rcPage.left, SPACE_TOP - rcPage.top);
	rcWindow.right += SPACE_LEFT + TREE_WIDTH + SPACE_MID - rcPage.left - (rcClient.Width() - rcTab.right) + SPACE_RIGHT;
	rcWindow.bottom += SPACE_TOP - rcPage.top - SPACE_BOTTOM;
	
	MoveWindow(&rcWindow, TRUE);
	
	tabContainer.SubclassWindow(tab.m_hWnd);
}

void TreePropertySheet::addTree()
{
	// Insert the space to the left
	CRect rcWindow, rcPage;
	
	const HWND page = IndexToHwnd(0);
	::GetWindowRect(page, &rcPage);
	::MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rcPage, 2);
	
	CRect rc(SPACE_LEFT, rcPage.top, TREE_WIDTH, rcPage.bottom);
	ctrlTree.Create(m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_EX_LAYERED |
	                WS_TABSTOP | // [+] http://code.google.com/p/flylinkdc/issues/detail?id=821
	                TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, WS_EX_CLIENTEDGE, IDC_PAGE);
	                
	if (BOOLSETTING(USE_EXPLORER_THEME)
#ifdef FLYLINKDC_SUPPORT_WIN_2000
	        && CompatibilityManager::IsXPPlus()
#endif
	   )
		SetWindowTheme(ctrlTree.m_hWnd, L"explorer", NULL);
		
	ctrlTree.SetImageList(tree_icons, TVSIL_NORMAL);
}

void TreePropertySheet::fillTree()
{
	CTabCtrl tab = GetTabControl();
	const int pages = tab.GetItemCount();
	LocalArray<TCHAR, MAX_NAME_LENGTH> buf;
	TCITEM item = {0};
	item.mask = TCIF_TEXT;
	item.pszText = buf.data();
	item.cchTextMax = MAX_NAME_LENGTH - 1;
	
	HTREEITEM first = NULL;
	for (int i = 0; i < pages; ++i)
	{
		tab.GetItem(i, &item);
		if (i == 0)
			first = createTree(buf.data(), TVI_ROOT, i);
		else
			createTree(buf.data(), TVI_ROOT, i);
	}
	if (SETTING(REMEMBER_SETTINGS_PAGE))
		ctrlTree.SelectItem(findItem(SETTING(PAGE), ctrlTree.GetRootItem()));
	else
		ctrlTree.SelectItem(first);
}

HTREEITEM TreePropertySheet::createTree(const tstring& str, HTREEITEM parent, int page)
{
	TVINSERTSTRUCT tvi = {0};
	tvi.hInsertAfter = TVI_LAST;
	tvi.hParent = parent;
	
	const HTREEITEM first = (parent == TVI_ROOT) ? ctrlTree.GetRootItem() : ctrlTree.GetChildItem(parent);
	
	string::size_type i = str.find(SEPARATOR);
	if (i == string::npos)
	{
		// Last dir, the actual page
		HTREEITEM item = findItem(str, first);
		if (item == NULL)
		{
			// Doesn't exist, add
			tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
			tvi.item.pszText = const_cast<LPTSTR>(str.c_str());
			tvi.item.lParam = page;
			item = ctrlTree.InsertItem(&tvi);
			ctrlTree.SetItemImage(item, page, page);
			ctrlTree.Expand(parent);
			return item;
		}
		else
		{
			// Update page
			if (ctrlTree.GetItemData(item) == -1)
				ctrlTree.SetItemData(item, page);
			return item;
		}
	}
	else
	{
		tstring name = str.substr(0, i);
		HTREEITEM item = findItem(name, first);
		if (item == NULL)
		{
			// Doesn't exist, add...
			tvi.item.mask = TVIF_PARAM | TVIF_TEXT;
			tvi.item.lParam = -1;
			tvi.item.pszText = const_cast<LPTSTR>(name.c_str());
			item = ctrlTree.InsertItem(&tvi);
			ctrlTree.SetItemImage(item, page, page);
		}
		ctrlTree.Expand(parent);
		// Recurse...
		return createTree(str.substr(i + 1), item, page);
	}
}

HTREEITEM TreePropertySheet::findItem(const tstring& str, HTREEITEM start)
{
	LocalArray<TCHAR, MAX_NAME_LENGTH> buf;
	while (start != NULL)
	{
		ctrlTree.GetItemText(start, buf.data(), MAX_NAME_LENGTH - 1);
		if(lstrcmp(str.c_str(),buf.data()) == 0) // TODO PVS
		{
			return start;
		}
		start = ctrlTree.GetNextSiblingItem(start);
	}
	return start;
}

HTREEITEM TreePropertySheet::findItem(int page, HTREEITEM start)
{
	while (start != NULL)
	{
		if (((int)ctrlTree.GetItemData(start)) == page)
			return start;
		const HTREEITEM ret = findItem(page, ctrlTree.GetChildItem(start));
		if (ret != NULL)
			return ret;
		start = ctrlTree.GetNextSiblingItem(start);
	}
	return NULL;
}

LRESULT TreePropertySheet::onSelChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /* bHandled */)
{
	NMTREEVIEW* nmtv = (NMTREEVIEW*)pnmh;
	int page = nmtv->itemNew.lParam;
	if (page == -1)
	{
		HTREEITEM next = ctrlTree.GetChildItem(nmtv->itemNew.hItem);
		if (next == NULL)
		{
			next = ctrlTree.GetNextSiblingItem(nmtv->itemNew.hItem);
			if (next == NULL)
			{
				next = ctrlTree.GetParentItem(nmtv->itemNew.hItem);
				if (next != NULL)
					next = ctrlTree.GetNextSiblingItem(next);
			}
		}
		if (next != NULL)
		{
			ctrlTree.SelectItem(next);
			if (SETTING(REMEMBER_SETTINGS_PAGE))
				SET_SETTING(PAGE, (int)next);
		}
	}
	else
	{
		if (HwndToIndex(GetActivePage()) != page)
		{
			SetActivePage(page);
			if (SETTING(REMEMBER_SETTINGS_PAGE))
				SET_SETTING(PAGE, page);
		}
	}
#ifdef SCALOLAZ_PROPPAGE_HELPLINK
	if (BOOLSETTING(SETTINGS_WINDOW_WIKIHELP))
		genHelpLink(HwndToIndex(GetActivePage()));
#endif
	return 0;
}

LRESULT TreePropertySheet::onSetCurSel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlTree.SelectItem(findItem((int)wParam, ctrlTree.GetRootItem()));
	bHandled = FALSE;
	return 0;
}

/**
* @file
* $Id: TreePropertySheet.cpp,v 1.11 2006/08/13 19:03:50 bigmuscle Exp $
*/
