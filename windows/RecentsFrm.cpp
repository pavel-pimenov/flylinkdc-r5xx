/*
 *
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

#include "RecentsFrm.h"
#include "HubFrame.h"
#include "LineDlg.h"

int RecentHubsFrame::columnIndexes[] = { COLUMN_NAME, COLUMN_DESCRIPTION, COLUMN_USERS, COLUMN_SHARED, COLUMN_SERVER, COLUMN_DATETIME };
int RecentHubsFrame::columnSizes[] = { 200, 290, 50, 50, 100, 130 };
static ResourceManager::Strings columnNames[] = { ResourceManager::HUB_NAME, ResourceManager::DESCRIPTION,
                                                  ResourceManager::USERS, ResourceManager::SHARED, ResourceManager::HUB_ADDRESS
                                                  , ResourceManager::LAST_SEEN
                                                };

LRESULT RecentHubsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, IDC_RECENTS);
	                
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlHubs);
	SET_LIST_COLOR(ctrlHubs);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(RECENTFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(RECENTFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = LVCFMT_LEFT;
		ctrlHubs.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlHubs.SetColumnOrderArray(COLUMN_LAST, columnIndexes);
	ctrlConnect.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                   BS_PUSHBUTTON , 0, IDC_CONNECT);
	ctrlConnect.SetWindowText(CTSTRING(CONNECT));
	ctrlConnect.SetFont(Fonts::systemFont); // [~] Sergey Shuhskanov
	
	ctrlRemove.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  BS_PUSHBUTTON , 0, IDC_REMOVE);
	ctrlRemove.SetWindowText(CTSTRING(REMOVE));
	ctrlRemove.SetFont(Fonts::systemFont); // [~] Sergey Shuhskanov
	
	ctrlRemoveAll.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     BS_PUSHBUTTON , 0, IDC_REMOVE_ALL);
	ctrlRemoveAll.SetWindowText(CTSTRING(REMOVE_ALL));
	ctrlRemoveAll.SetFont(Fonts::systemFont); // [~] Sergey Shuhskanov
	
	FavoriteManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	updateList(FavoriteManager::getInstance()->getRecentHubs());
	
	const int l_sort = SETTING(HUBS_RECENTS_COLUMNS_SORT);
	int l_sort_type = ExListViewCtrl::SORT_STRING_NOCASE;
	if (l_sort == 2 || l_sort == 3)
		l_sort_type = ExListViewCtrl::SORT_INT;
	ctrlHubs.setSort(SETTING(HUBS_RECENTS_COLUMNS_SORT), l_sort_type , BOOLSETTING(HUBS_RECENTS_COLUMNS_SORT_ASC));
	
	hubsMenu.CreatePopupMenu();
	hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT));
	hubsMenu.AppendMenu(MF_STRING, IDC_ADD, CTSTRING(ADD_TO_FAVORITES_HUBS));
	hubsMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS));
	hubsMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES));
	hubsMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
	hubsMenu.AppendMenu(MF_STRING, IDC_REMOVE_ALL, CTSTRING(REMOVE_ALL));
	hubsMenu.SetMenuDefaultItem(IDC_CONNECT);
	
	bHandled = FALSE;
	return TRUE;
}

LRESULT RecentHubsFrame::onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem != -1)
	{
		RecentHubEntry* entry = (RecentHubEntry*)ctrlHubs.GetItemData(item->iItem);
		HubFrame::openWindow(Text::toT(entry->getServer()));
	}
	return 0;
}

LRESULT RecentHubsFrame::onEnter(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	int item = ctrlHubs.GetNextItem(-1, LVNI_FOCUSED);
	
	if (item != -1)
	{
		RecentHubEntry* entry = (RecentHubEntry*)ctrlHubs.GetItemData(item);
		HubFrame::openWindow(Text::toT(entry->getServer()));
	}
	
	return 0;
}

LRESULT RecentHubsFrame::onClickedConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		RecentHubEntry* entry = (RecentHubEntry*)ctrlHubs.GetItemData(i);
		HubFrame::openWindow(Text::toT(entry->getServer()));
	}
	return 0;
}

LRESULT RecentHubsFrame::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlHubs.GetSelectedCount() == 1)
	{
		int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
		FavoriteHubEntry e;
		e.setName(ctrlHubs.ExGetItemText(i, COLUMN_NAME));
		e.setDescription(ctrlHubs.ExGetItemText(i, COLUMN_DESCRIPTION));
		e.setServer(ctrlHubs.ExGetItemText(i, COLUMN_SERVER));
		FavoriteManager::getInstance()->addFavorite(e);
	}
	return 0;
}

LRESULT RecentHubsFrame::onRemoveFav(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const auto fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(getRecentServer((int)i));
		if (fhe)
		{
			FavoriteManager::getInstance()->removeFavorite(fhe);
		}
	}
	return 0;
}

LRESULT RecentHubsFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED)) != -1)
	{
		FavoriteManager::getInstance()->removeRecent((RecentHubEntry*)ctrlHubs.GetItemData(i));
	}
	return 0;
}

LRESULT RecentHubsFrame::onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlHubs.DeleteAllItems();
	FavoriteManager::getInstance()->removeallRecent();
	return 0;
}

LRESULT RecentHubsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		FavoriteManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		m_closed = true;
		WinUtil::setButtonPressed(IDC_RECENTS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlHubs, SettingsManager::RECENTFRAME_ORDER,
		                         SettingsManager::RECENTFRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		                         
		SET_SETTING(HUBS_RECENTS_COLUMNS_SORT, ctrlHubs.getSortColumn());
		SET_SETTING(HUBS_RECENTS_COLUMNS_SORT_ASC, ctrlHubs.isAscending());
		bHandled = FALSE;
		return 0;
	}
}

void RecentHubsFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	CRect rc = rect;
	rc.bottom -= 26;
	ctrlHubs.MoveWindow(rc);
	
	const long bwidth = 90;
	const long bspace = 10;
	
	rc = rect;
	rc.bottom -= 2;
	rc.top = rc.bottom - 22;
	
	rc.left = 2;
	rc.right = rc.left + bwidth;
	ctrlConnect.MoveWindow(rc);
	
	rc.OffsetRect(bspace + bwidth + 2, 0);
	ctrlRemove.MoveWindow(rc);
	
	rc.OffsetRect(bwidth + 2, 0);
	ctrlRemoveAll.MoveWindow(rc);
}

LRESULT RecentHubsFrame::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	if ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		RecentHubEntry* r = (RecentHubEntry*)ctrlHubs.GetItemData(i);
		dcassert(r != NULL);
		LineDlg dlg;
		dlg.description = TSTRING(DESCRIPTION);
		dlg.title = Text::toT(r->getName());
		dlg.line = Text::toT(r->getDescription());
		if (dlg.DoModal(m_hWnd) == IDOK)
		{
			r->setDescription(Text::fromT(dlg.line));
			ctrlHubs.SetItemText(i, COLUMN_DESCRIPTION, Text::toT(r->getDescription()).c_str());
			FavoriteManager::getInstance()->recentsave();
		}
	}
	return 0;
}

void RecentHubsFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
{
	bool refresh = false;
	if (ctrlHubs.GetBkColor() != Colors::bgColor)
	{
		ctrlHubs.SetBkColor(Colors::bgColor);
		ctrlHubs.SetTextBkColor(Colors::bgColor);
		refresh = true;
	}
	if (ctrlHubs.GetTextColor() != Colors::textColor)
	{
		ctrlHubs.SetTextColor(Colors::textColor);
		refresh = true;
	}
	if (refresh == true)
	{
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}


LRESULT RecentHubsFrame::onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_CONNECT), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_REMOVE), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

LRESULT RecentHubsFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			cd->clrText = Colors::textColor;
			const auto fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(getRecentServer((int)cd->nmcd.dwItemSpec));
			if (fhe)
			{
				if (fhe->getConnect())
				{
					cd->clrTextBk = SETTING(HUB_IN_FAV_CONNECT_BK_COLOR);
				}
				else
				{
					cd->clrTextBk = SETTING(HUB_IN_FAV_BK_COLOR);
				}
			}
			Colors::alternationBkColor(cd); // [+] IRainman
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
		}
		default:
			return CDRF_DODEFAULT;
	}
	
}