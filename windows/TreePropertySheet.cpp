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

#include "Resource.h"

#include "TreePropertySheet.h"
#include "ResourceLoader.h"
#include "WinUtil.h"
#include "../client/ResourceManager.h"

static const TCHAR SEPARATOR = _T('\\');

#ifdef SCALOLAZ_PROPPAGE_CAMSHOOT
//HIconWrapper TreePropertySheet::g_CamPNG(IDR_ICON_CAMSHOOT_PNG);
#endif
int TreePropertySheet::PropSheetProc(HWND hwndDlg, UINT uMsg, LPARAM lParam)
{
	if (uMsg == PSCB_INITIALIZED)
	{
		::PostMessage(hwndDlg, WM_USER_INITDIALOG, 0, 0);
	}
	
	return CPropertySheet::PropSheetCallback(hwndDlg, uMsg, lParam);
}
LRESULT TreePropertySheet::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	safe_destroy_timer();
	return 0;
}
LRESULT TreePropertySheet::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	onTimerSec();
	return 0;
}
LRESULT TreePropertySheet::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
{
	/* [-] IRainman fix.
	if (ResourceManager::getInstance()->isRTL())
	    SetWindowLongPtr(GWL_EXSTYLE, GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYOUTRTL);
	*/
	
#ifdef SCALOLAZ_PROPPAGE_TRANSPARENCY
	if (BOOLSETTING(SETTINGS_WINDOW_TRANSP))
	{
		m_SliderPos = 255;
		setTransp(/*SETTING(PROPPAGE_TRANSP)*/ m_SliderPos);
	}
#endif
	ResourceLoader::LoadImageList(IDR_SETTINGS_ICONS, tree_icons, 16, 16);
	hideTab();
	CenterWindow(GetParent());
	addTree();
	fillTree();
	m_offset = 0;
#ifdef SCALOLAZ_PROPPAGE_HELPLINK
	if (BOOLSETTING(SETTINGS_WINDOW_WIKIHELP))
		addHelp();
#endif
#ifdef SCALOLAZ_PROPPAGE_CAMSHOOT
	addCam();
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
	const int width = WinUtil::getTextWidth(CTSTRING(PROPERTIES_WIKIHELP), m_hWnd);     // calculate a center X
	rectok2.left = ((wind2.right - wind2.left) / 2) - (width / 2) + 20;
	rectok2.right = rectok2.left + width;
	if (rectok2.right > rectok.left)
	{
		rectok2.right = rectok.left - 5;
	}
	rectok2.top += 5;
	m_Help.MoveWindow(rectok2);
	m_Help.SetLabel(CTSTRING(PROPERTIES_WIKIHELP));
	genHelpLink(HwndToIndex(GetActivePage()));
	idok.MoveWindow(rectok);
}

static const TCHAR* g_HelpLinkTable[] =
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
void TreePropertySheet::genHelpLink(int p_page)
{
	const tstring l_url = WinUtil::GetWikiLink() + genPropPageName(p_page);
	m_Help.SetHyperLink(l_url.c_str());
	m_Help.SetHyperLinkExtendedStyle(/*HLINK_LEFTIMAGE |*/ HLINK_UNDERLINEHOVER);
}

tstring TreePropertySheet::genPropPageName(int p_page)
{
	if (p_page < 0 || p_page >= _countof(g_HelpLinkTable) /*39*/)
		p_page = 0;
	const tstring m_name = g_HelpLinkTable[p_page];
	return m_name;
}
#endif

#ifdef SCALOLAZ_PROPPAGE_CAMSHOOT
void TreePropertySheet::addCam()
{
	CRect rectok, wind;
	CWindow idok = GetDlgItem(IDOK);
	idok.GetWindowRect(rectok);
	GetWindowRect(wind);
	ScreenToClient(rectok);
	ScreenToClient(wind);
	rectok.left = wind.left + 12;
	rectok.bottom = wind.bottom - 7;
	rectok.top = rectok.bottom - 26;
	rectok.right = rectok.left + 34;
	m_offset = 4 + (rectok.right - rectok.left);    //Score offset for next control
	
	m_Cam = new CButton;
	m_Cam->Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER | BS_PUSHBUTTON, 0, IDC_PROPPAGE_CAMSHOOT);
	m_Cam->MoveWindow(rectok);
	
	m_Camtooltip = new CFlyToolTipCtrl;
	m_Camtooltip->Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	m_Camtooltip->SetDelayTime(TTDT_AUTOPOP, 20000);
	dcassert(m_Camtooltip->IsWindow());
	m_Camtooltip->SetMaxTipWidth(355);   //[+] SCALOlaz: activate tooltips
	m_Camtooltip->AddTool(*m_Cam, _T("Create ScreenShoot for this page. ScreenShoot will be sending to our MediaServer and link for him will be copying into chat") /*ResourceManager::CAMSHOOT_PROPPAGE*/);
	
	if (!BOOLSETTING(POPUPS_DISABLED))
	{
		m_Camtooltip->Activate(TRUE);
	}
	//g_CamPNG.LoadFromResource(IDR_ICON_CAMSHOOT_PNG, _T("PNG"));
	//m_Cam.SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)g_CamPNG));
	static HIconWrapper g_CamIco(IDR_ICON_CAMSHOOT_ICO, 32, 32);
	m_Cam->SetIcon(g_CamIco);
}


void TreePropertySheet::doCamShoot()
{
	CRect rWnd;
	GetClientRect(&rWnd);
	int Height, Width;
	Width = rWnd.Width();   // ширина области
	Height = rWnd.Height(); // высота области
	
	HDC scrDC, memDC;
	HBITMAP membit;
	scrDC = ::GetDC(m_hWnd);//
	if ((memDC = CreateCompatibleDC(scrDC)) == 0) return;
	if ((membit = CreateCompatibleBitmap(scrDC, Width, Height)) == 0) return;
	//Снимок
	if (SelectObject(memDC, membit) == 0) return;
	if (BitBlt(memDC, 0, 0, Width, Height, scrDC, 0, 0, SRCCOPY) == 0) return;
	
	//Сборка имени файла
	//SYSTEMTIME systime;
	//GetLocalTime(&systime);
	//char cCurTime[256];
	//sprintf_s(cCurTime, "%d-%d-%d %d-%d-%02d", systime.wDay, systime.wMonth, systime.wYear, systime.wHour, systime.wMinute, systime.wSecond);
	
	tstring m_Prop_namepage = _T("SettingsPage");
#ifdef SCALOLAZ_PROPPAGE_HELPLINK
	m_Prop_namepage = genPropPageName(HwndToIndex(GetActivePage()));
#endif
	wstring szPath = (L"D:/\/\Nick_Time_" + wstring(m_Prop_namepage) + L".png");   // Путь и имя файла для сохранения
	// Пока можно направить в папку, расшаренную по умолчанию.
	
	//Сохранение в PNG
	Gdiplus::Bitmap bitmap(membit, NULL);
	static const GUID png = { 0x557cf406, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e } };
	bitmap.Save(szPath.c_str(), &png);
	//Очистка
	DeleteObject(membit);
	DeleteDC(memDC);
	DeleteDC(scrDC);
	//PLAY_SOUND(SOUND_CAMSHOOT);
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
	rectok.left = wind.left + m_offset + 7;
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
	if (!BOOLSETTING(POPUPS_DISABLED))
	{
		m_tooltip.Activate(TRUE);
	}
}
void TreePropertySheet::setTransp(int p_Layered)
{
	typedef bool (CALLBACK * LPFUNC)(HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);
	LPFUNC _d_SetLayeredWindowAttributes = (LPFUNC)GetProcAddress(LoadLibrary(_T("user32")), "SetLayeredWindowAttributes");
	if (_d_SetLayeredWindowAttributes)
	{
		SetWindowLongPtr(GWL_EXSTYLE, GetWindowLongPtr(GWL_EXSTYLE) | WS_EX_LAYERED /*| WS_EX_TRANSPARENT*/);
		_d_SetLayeredWindowAttributes(m_hWnd, 0, p_Layered, LWA_ALPHA);
	}
}
LRESULT TreePropertySheet::onTranspChanged(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	setTransp(m_Slider.GetPos());
	return 0;
}
#endif // SCALOLAZ_PROPPAGE_TRANSPARENCY
void TreePropertySheet::hideTab()
{
	CRect rcClient, rcTab, rcPage, rcWindow;
	CWindow tab = GetTabControl();
	CWindow l_page = IndexToHwnd(0);
	GetClientRect(&rcClient);
	tab.GetWindowRect(&rcTab);
	tab.ShowWindow(SW_HIDE);
	if (l_page.IsWindow())
	{
		l_page.GetClientRect(&rcPage);
		l_page.MapWindowPoints(m_hWnd, &rcPage);
	}
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
	CRect rcPage;
	
	const HWND page = IndexToHwnd(0);
	::GetWindowRect(page, &rcPage);
	::MapWindowPoints(NULL, m_hWnd, (LPPOINT)&rcPage, 2);
	
	CRect rc(SPACE_LEFT, rcPage.top, TREE_WIDTH, rcPage.bottom);
	ctrlTree.Create(m_hWnd, rc, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_EX_LAYERED |
	                WS_TABSTOP |
	                TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, WS_EX_CLIENTEDGE, IDC_PAGE);
	                
	WinUtil::SetWindowThemeExplorer(ctrlTree.m_hWnd);
	
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
	create_timer(1000);
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
		if (lstrcmp(str.c_str(), buf.data()) == 0) // TODO PVS
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
		if (next == nullptr)
		{
			next = ctrlTree.GetNextSiblingItem(nmtv->itemNew.hItem);
			if (next == nullptr)
			{
				next = ctrlTree.GetParentItem(nmtv->itemNew.hItem);
				if (next != nullptr)
				{
					next = ctrlTree.GetNextSiblingItem(next);
				}
			}
		}
		if (next != nullptr)
		{
			ctrlTree.SelectItem(next);
			if (SETTING(REMEMBER_SETTINGS_PAGE))
			{
				SET_SETTING(PAGE, (int)next);
			}
		}
	}
	else
	{
		if (HwndToIndex(GetActivePage()) != page)
		{
			SetActivePage(page);
			if (SETTING(REMEMBER_SETTINGS_PAGE))
			{
				SET_SETTING(PAGE, page);
			}
		}
	}
#ifdef SCALOLAZ_PROPPAGE_HELPLINK
	if (BOOLSETTING(SETTINGS_WINDOW_WIKIHELP))
	{
		genHelpLink(HwndToIndex(GetActivePage()));
	}
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
