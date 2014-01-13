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

#include "PublicHubsFrm.h"

#ifdef IRAINMAN_ENABLE_HUB_LIST

#include "HubFrame.h"
#include "WinUtil.h"
#include "PublicHubsListDlg.h"

int PublicHubsFrame::columnIndexes[] =
{
	COLUMN_NAME,
	COLUMN_DESCRIPTION,
	COLUMN_USERS,
	COLUMN_SERVER,
	COLUMN_COUNTRY,
	COLUMN_SHARED,
	COLUMN_MINSHARE,
	COLUMN_MINSLOTS,
	COLUMN_MAXHUBS,
	COLUMN_MAXUSERS,
	COLUMN_RELIABILITY,
	COLUMN_RATING
};

int PublicHubsFrame::columnSizes[] = { 200, 290, 50, 100, 100, 100, 100, 100, 100, 100, 100, 100 };

static ResourceManager::Strings columnNames[] =
{
	ResourceManager::HUB_NAME,
	ResourceManager::DESCRIPTION,
	ResourceManager::USERS,
	ResourceManager::HUB_ADDRESS,
	ResourceManager::COUNTRY,
	ResourceManager::SHARED,
	ResourceManager::MIN_SHARE,
	ResourceManager::MIN_SLOTS,
	ResourceManager::MAX_HUBS,
	ResourceManager::MAX_USERS,
	ResourceManager::RELIABILITY,
	ResourceManager::RATING,
};

LRESULT PublicHubsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	int w[3] = { 0, 0, 0 };
	ctrlStatus.SetParts(3, w);
	
	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | /*LVS_SINGLESEL |*/ LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_HUBLIST);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlHubs);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(PUBLICHUBSFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(PUBLICHUBSFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_USERS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlHubs.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlHubs.SetColumnOrderArray(COLUMN_LAST, columnIndexes);
	
	SET_LIST_COLOR(ctrlHubs);
	
	ctrlHubs.SetImageList(g_flagImage.getIconList(), LVSIL_SMALL);
	ctrlHubs.SetFocus();
	
	ctrlConfigure.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     BS_PUSHBUTTON , 0, IDC_PUB_LIST_CONFIG);
	ctrlConfigure.SetWindowText(CTSTRING(CONFIGURE));
	ctrlConfigure.SetFont(Fonts::systemFont);
	
	ctrlRefresh.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                   BS_PUSHBUTTON , 0, IDC_REFRESH_HUB_LIST);
	ctrlRefresh.SetWindowText(CTSTRING(REFRESH));
	ctrlRefresh.SetFont(Fonts::systemFont);
	
	ctrlLists.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 BS_GROUPBOX, WS_EX_TRANSPARENT);
	ctrlLists.SetFont(Fonts::systemFont);
	ctrlLists.SetWindowText(CTSTRING(CONFIGURED_HUB_LISTS));
	
	ctrlPubLists.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE, IDC_PUB_LIST_DROPDOWN);
	ctrlPubLists.SetFont(Fonts::systemFont, FALSE);
	// populate with values from the settings
	updateDropDown();
	
	ctrlFilter.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
	filterContainer.SubclassWindow(ctrlFilter.m_hWnd);
	ctrlFilter.SetFont(Fonts::systemFont);
	
	ctrlFilterSel.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     WS_HSCROLL | WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
	ctrlFilterSel.SetFont(Fonts::systemFont, FALSE);
	
	//populate the filter list with the column names
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		ctrlFilterSel.AddString(CTSTRING_I(columnNames[j]));
	}
	ctrlFilterSel.AddString(CTSTRING(ANY));
	ctrlFilterSel.SetCurSel(COLUMN_LAST);
	
	ctrlFilterDesc.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                      BS_GROUPBOX, WS_EX_TRANSPARENT);
	ctrlFilterDesc.SetWindowText(CTSTRING(FILTER));
	ctrlFilterDesc.SetFont(Fonts::systemFont);
	
	FavoriteManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	
	FavoriteManager::getInstance()->getPublicHubs(hubs);
	if (FavoriteManager::getInstance()->isDownloading())
		ctrlStatus.SetText(0, CTSTRING(DOWNLOADING_HUB_LIST));
	else if (hubs.empty())
		FavoriteManager::getInstance()->refresh();
		
	updateList();
	
//	ctrlHubs.setSort(COLUMN_USERS, ExListViewCtrl::SORT_INT, false);
	const int l_sort = SETTING(HUBS_PUBLIC_COLUMNS_SORT);
	int l_sort_type = ExListViewCtrl::SORT_STRING_NOCASE;
	if (l_sort == 2 || l_sort > 4)
		l_sort_type = ExListViewCtrl::SORT_INT;
	if (l_sort == 5)
		l_sort_type = ExListViewCtrl::SORT_BYTES;
	ctrlHubs.setSort(SETTING(HUBS_PUBLIC_COLUMNS_SORT), l_sort_type, BOOLSETTING(HUBS_PUBLIC_COLUMNS_SORT_ASC));
	
	hubsMenu.CreatePopupMenu();
	hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT));
	hubsMenu.AppendMenu(MF_STRING, IDC_ADD, CTSTRING(ADD_TO_FAVORITES_HUBS));
	hubsMenu.AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS));
	hubsMenu.AppendMenu(MF_STRING, IDC_COPY_HUB, CTSTRING(COPY_HUB));
	hubsMenu.SetMenuDefaultItem(IDC_CONNECT);
	
	bHandled = FALSE;
	return TRUE;
}

LRESULT PublicHubsFrame::onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
	if (l->iSubItem == ctrlHubs.getSortColumn())
	{
		if (!ctrlHubs.isAscending())
			ctrlHubs.setSort(-1, ctrlHubs.getSortType());
		else
			ctrlHubs.setSortDirection(false);
	}
	else
	{
		// BAH, sorting on bytes will break of course...oh well...later...
		if (l->iSubItem == COLUMN_USERS || l->iSubItem == COLUMN_MINSLOTS || l->iSubItem == COLUMN_MAXHUBS || l->iSubItem == COLUMN_MAXUSERS)
		{
			ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_INT);
		}
		else if (l->iSubItem == COLUMN_RELIABILITY)
		{
			ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_FLOAT);
		}
		else if (l->iSubItem == COLUMN_SHARED || l->iSubItem == COLUMN_MINSHARE)
		{
			ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_BYTES);
		}
		else
		{
			ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_STRING_NOCASE);
		}
	}
	return 0;
}

LRESULT PublicHubsFrame::onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	if (!checkNick())
		return 0;
		
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem != -1)
		openHub(item->iItem);
		
	return 0;
}

void PublicHubsFrame::openHub(int ind) // [+] IRainman fix.
{
	RecentHubEntry r;
	r.setName(ctrlHubs.ExGetItemText(ind, COLUMN_NAME));
	r.setDescription(ctrlHubs.ExGetItemText(ind, COLUMN_DESCRIPTION));
	r.setUsers(ctrlHubs.ExGetItemText(ind, COLUMN_USERS));
	r.setShared(ctrlHubs.ExGetItemText(ind, COLUMN_SHARED));
	const string l_server = Util::formatDchubUrl(ctrlHubs.ExGetItemText(ind, COLUMN_SERVER));
	r.setServer(l_server);
	FavoriteManager::getInstance()->addRecent(r);
	
	HubFrame::openWindow(Text::toT(l_server));
}

LRESULT PublicHubsFrame::onEnter(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	if (!checkNick())
		return 0;
		
	int item = ctrlHubs.GetNextItem(-1, LVNI_FOCUSED);
	if (item != -1)
		openHub(item);
		
	return 0;
}

LRESULT PublicHubsFrame::onClickedRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlHubs.DeleteAllItems();
	users = 0;
	visibleHubs = 0;
	ctrlStatus.SetText(0, CTSTRING(DOWNLOADING_HUB_LIST));
	FavoriteManager::getInstance()->refresh(true);
	updateDropDown();
	return 0;
}

LRESULT PublicHubsFrame::onClickedConfigure(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	PublicHubListDlg dlg;
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		updateDropDown();
	}
	return 0;
}

LRESULT PublicHubsFrame::onClickedConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!checkNick())
		return 0;
		
	if (ctrlHubs.GetSelectedCount() >= 10) // maximum hubs per one connection
	{
		if (MessageBox(CTSTRING(PUBLIC_HUBS_WARNING), _T(" "), MB_ICONWARNING | MB_YESNO) == IDNO)
			return 0;
	}
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
		openHub(i);
		
	return 0;
}

LRESULT PublicHubsFrame::onFilterFocus(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	bHandled = true;
	ctrlFilter.SetFocus();
	return 0;
}

LRESULT PublicHubsFrame::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!checkNick())
		return 0;
		
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		FavoriteHubEntry e;
		e.setName(ctrlHubs.ExGetItemText(i, COLUMN_NAME));
		e.setDescription(ctrlHubs.ExGetItemText(i, COLUMN_DESCRIPTION));
		e.setServer(Util::formatDchubUrl(ctrlHubs.ExGetItemText(i, COLUMN_SERVER)));
		//  if (!client->getPassword().empty()) // ToDo: Use SETTINGS Nick and Password
		//  {
		//      e.setNick(client->getMyNick());
		//      e.setPassword(client->getPassword());
		//  }
		FavoriteManager::getInstance()->addFavorite(e);
	}
	return 0;
}

LRESULT PublicHubsFrame::onRemoveFav(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const auto fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(getPubServer((int)i));
		if (fhe)
		{
			FavoriteManager::getInstance()->removeFavorite(fhe);
		}
	}
	return 0;
}

LRESULT PublicHubsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		FavoriteManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		WinUtil::setButtonPressed(ID_FILE_CONNECT, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlHubs, SettingsManager::PUBLICHUBSFRAME_ORDER,
		                         SettingsManager::PUBLICHUBSFRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		SET_SETTING(HUBS_PUBLIC_COLUMNS_SORT, ctrlHubs.getSortColumn());
		SET_SETTING(HUBS_PUBLIC_COLUMNS_SORT_ASC, ctrlHubs.isAscending());
		bHandled = FALSE;
		return 0;
	}
}

LRESULT PublicHubsFrame::onListSelChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	FavoriteManager::getInstance()->setHubList(ctrlPubLists.GetCurSel());
	FavoriteManager::getInstance()->getPublicHubs(hubs);
	updateList();
	bHandled = FALSE;
	return 0;
}

void PublicHubsFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[3];
		ctrlStatus.GetClientRect(sr);
		int tmp = (sr.Width() > 600) ? 180 : 150;
		
		w[2] = sr.right;
		w[1] = w[2] - tmp;          // Users Field start
		w[0] = w[1] - (tmp / 2);    // Hubs Field start
		
		ctrlStatus.SetParts(3, w);
	}
	
	int const comboH = 140;
	
	// listview
	CRect rc = rect;
	//rc.top += 2; //[~] Sergey Shuhskanov
	rc.bottom -= 56;
	ctrlHubs.MoveWindow(rc);
	
	// filter box
	rc = rect;
	rc.left += 4;
	rc.top = rc.bottom - 52;
	rc.bottom = rc.top + 46;
	rc.right -= 100;
	rc.right -= ((rc.right - rc.left) / 2) + 1;
	ctrlFilterDesc.MoveWindow(rc);
	
	// filter edit
	rc.top += 16;
	rc.bottom -= 8;
	rc.left += 8;
	rc.right -= ((rc.right - rc.left - 4) / 3);
	ctrlFilter.MoveWindow(rc);
	
	//filter sel
	rc.bottom += comboH;
	rc.right += ((rc.right - rc.left - 12) / 2) ;
	rc.left += ((rc.right - rc.left + 8) / 3) * 2;
	ctrlFilterSel.MoveWindow(rc);
	
	// lists box
	rc = rect;
	rc.top = rc.bottom - 52;
	rc.bottom = rc.top + 46;
	rc.right -= 100;
	rc.left += ((rc.right - rc.left) / 2) + 1;
	ctrlLists.MoveWindow(rc);
	
	// lists dropdown
	rc.top += 16;
	rc.bottom -= 8 - comboH;
	rc.right -= 8 + 100;
	rc.left += 8;
	ctrlPubLists.MoveWindow(rc);
	
	// configure button
	rc.left = rc.right + 4;
	rc.bottom -= comboH;
	rc.right += 100;
	ctrlConfigure.MoveWindow(rc);
	
	// refresh button
	rc = rect;
	rc.bottom -= 2 + 8 + 4;
	rc.top = rc.bottom - 22;
	rc.left = rc.right - 96;
	rc.right -= 2;
	ctrlRefresh.MoveWindow(rc);
}

bool PublicHubsFrame::checkNick()
{
	if (SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), T_APPNAME_WITH_VERSION, MB_ICONSTOP | MB_OK);
		return false;
	}
	return true;
}

void PublicHubsFrame::updateList()
{
	CLockRedraw<> l_lock_draw(ctrlHubs);
	ctrlHubs.DeleteAllItems();
	users = 0;
	visibleHubs = 0;
	
	double size = -1;
	FilterModes mode = NONE;
	
	int sel = ctrlFilterSel.GetCurSel();
	
	bool doSizeCompare = parseFilter(mode, size);
	
	auto cnt = ctrlHubs.GetItemCount();
	for (auto i = hubs.cbegin(); i != hubs.cend(); ++i)
	{
		if (matchFilter(*i, sel, doSizeCompare, mode, size))
		{
			TStringList l;
			l.resize(COLUMN_LAST);
			l[COLUMN_NAME] = Text::toT(i->getName());
			l[COLUMN_DESCRIPTION] = Text::toT(i->getDescription());
			l[COLUMN_USERS] = Util::toStringW(i->getUsers());
			l[COLUMN_SERVER] = Text::toT(i->getServer());
			l[COLUMN_COUNTRY] = Text::toT(i->getCountry()); // !SMT!-IP
			l[COLUMN_SHARED] = Util::formatBytesW(i->getShared());
			l[COLUMN_MINSHARE] = Util::formatBytesW(i->getMinShare());
			l[COLUMN_MINSLOTS] = Util::toStringW(i->getMinSlots());
			l[COLUMN_MAXHUBS] = Util::toStringW(i->getMaxHubs());
			l[COLUMN_MAXUSERS] = Util::toStringW(i->getMaxUsers());
			l[COLUMN_RELIABILITY] = Util::toStringW(i->getReliability());
			l[COLUMN_RATING] = Text::toT(i->getRating());
			ctrlHubs.insert(cnt++, l, WinUtil::getFlagIndexByName(i->getCountry().c_str())); // !SMT!-IP
			visibleHubs++;
			users += i->getUsers();
		}
	}
	
	ctrlHubs.resort();
	
	updateStatus();
}

void PublicHubsFrame::updateStatus()
{
	ctrlStatus.SetText(1, (TSTRING(HUBS) + _T(": ") + Util::toStringW(visibleHubs)).c_str());
	ctrlStatus.SetText(2, (TSTRING(USERS) + _T(": ") + Util::toStringW(users)).c_str());
}

LRESULT PublicHubsFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam == FINISHED)
	{
		FavoriteManager::getInstance()->getPublicHubs(hubs);
		updateList();
		tstring* x = (tstring*)lParam;
		ctrlStatus.SetText(0, (TSTRING(HUB_LIST_DOWNLOADED) + _T(" (") + (*x) + _T(")")).c_str());
		delete x;
	}
	else if (wParam == SET_TEXT)
	{
		tstring* x = (tstring*)lParam;
		ctrlStatus.SetText(0, (*x).c_str());
		delete x;
	}
	return 0;
}

LRESULT PublicHubsFrame::onFilterChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (wParam == VK_RETURN)
	{
		tstring tmp;
		WinUtil::GetWindowText(tmp, ctrlFilter);
		
		filter = Text::fromT(tmp);
		
		updateList();
	}
	else
	{
		bHandled = FALSE;
	}
	return 0;
}

LRESULT PublicHubsFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlHubs /*&& ctrlHubs.GetSelectedCount() == 1*/)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		CRect rc;
		ctrlHubs.GetHeader().GetWindowRect(&rc);
		if (PtInRect(&rc, pt))
		{
			return 0;
		}
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlHubs, pt);
		}
		int status = ctrlHubs.GetSelectedCount() > 0 ? MFS_ENABLED : MFS_DISABLED;
		hubsMenu.EnableMenuItem(IDC_CONNECT, status);
		hubsMenu.EnableMenuItem(IDC_ADD, status);
		hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, status);
		if (ctrlHubs.GetSelectedCount() > 1)
		{
			hubsMenu.EnableMenuItem(IDC_COPY_HUB, MFS_DISABLED);
		}
		else
		{
			int i = -1;
			while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const auto fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(getPubServer((int)i));
				if (fhe)
				{
					hubsMenu.EnableMenuItem(IDC_ADD, MFS_DISABLED);
					hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, MFS_ENABLED);
				}
				else
				{
					hubsMenu.EnableMenuItem(IDC_ADD, MFS_ENABLED);
					hubsMenu.EnableMenuItem(IDC_REM_AS_FAVORITE, MFS_DISABLED);
				}
			}
			hubsMenu.EnableMenuItem(IDC_COPY_HUB, status);
		}
		hubsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT PublicHubsFrame::onCopyHub(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlHubs.GetSelectedCount() == 1)
	{
		LocalArray<TCHAR, 256> buf;
		int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
		ctrlHubs.GetItemText(i, COLUMN_SERVER, buf.data(), 256);
		WinUtil::setClipboard(buf.data());
	}
	return 0;
}

void PublicHubsFrame::updateDropDown()
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		ctrlPubLists.ResetContent();
		const StringList& lists = FavoriteManager::getInstance()->getHubLists();
		for (auto idx = lists.cbegin(); idx != lists.cend(); ++idx)
			ctrlPubLists.AddString(Text::toT(*idx).c_str());
		ctrlPubLists.SetCurSel(FavoriteManager::getInstance()->getSelectedHubList());
	}
}

bool PublicHubsFrame::parseFilter(FilterModes& mode, double& size)
{
	string::size_type start = string::npos;
	string::size_type end = string::npos;
	int64_t multiplier = 1;
	
	if (filter.empty()) return false;
	if (filter.compare(0, 2, ">=", 2) == 0)
	{
		mode = GREATER_EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, "<=", 2) == 0)
	{
		mode = LESS_EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, "==", 2) == 0)
	{
		mode = EQUAL;
		start = 2;
	}
	else if (filter.compare(0, 2, "!=", 2) == 0)
	{
		mode = NOT_EQUAL;
		start = 2;
	}
	else if (filter[0] == _T('<'))
	{
		mode = LESS;
		start = 1;
	}
	else if (filter[0] == _T('>'))
	{
		mode = GREATER;
		start = 1;
	}
	else if (filter[0] == _T('='))
	{
		mode = EQUAL;
		start = 1;
	}
	
	if (start == string::npos)
		return false;
	if (filter.length() <= start)
		return false;
		
	if ((end = Util::findSubString(filter, "TiB")) != tstring::npos)
	{
		multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
	}
	else if ((end = Util::findSubString(filter, "GiB")) != tstring::npos)
	{
		multiplier = 1024 * 1024 * 1024;
	}
	else if ((end = Util::findSubString(filter, "MiB")) != tstring::npos)
	{
		multiplier = 1024 * 1024;
	}
	else if ((end = Util::findSubString(filter, "KiB")) != tstring::npos)
	{
		multiplier = 1024;
	}
	else if ((end = Util::findSubString(filter, "TB")) != tstring::npos)
	{
		multiplier = 1000LL * 1000LL * 1000LL * 1000LL;
	}
	else if ((end = Util::findSubString(filter, "GB")) != tstring::npos)
	{
		multiplier = 1000 * 1000 * 1000;
	}
	else if ((end = Util::findSubString(filter, "MB")) != tstring::npos)
	{
		multiplier = 1000 * 1000;
	}
	else if ((end = Util::findSubString(filter, "kB")) != tstring::npos)
	{
		multiplier = 1000;
	}
	else if ((end = Util::findSubString(filter, "B")) != tstring::npos)
	{
		multiplier = 1;
	}
	
	
	if (end == string::npos)
	{
		end = filter.length();
	}
	
	string tmpSize = filter.substr(start, end - start);
	size = Util::toDouble(tmpSize) * multiplier;
	
	return true;
}

bool PublicHubsFrame::matchFilter(const HubEntry& entry, const int& sel, bool doSizeCompare, const FilterModes& mode, const double& size)
{
	if (filter.empty())
		return true;
		
	double entrySize = 0;
	string entryString;
	
	switch (sel)
	{
		case COLUMN_NAME:
			entryString = entry.getName();
			doSizeCompare = false;
			break;
		case COLUMN_DESCRIPTION:
			entryString = entry.getDescription();
			doSizeCompare = false;
			break;
		case COLUMN_USERS:
			entrySize = entry.getUsers();
			break;
		case COLUMN_SERVER:
			entryString = entry.getServer();
			doSizeCompare = false;
			break;
		case COLUMN_COUNTRY:
			entryString = entry.getCountry();
			doSizeCompare = false;
			break;
		case COLUMN_SHARED:
			entrySize = (double)entry.getShared();
			break;
		case COLUMN_MINSHARE:
			entrySize = (double)entry.getMinShare();
			break;
		case COLUMN_MINSLOTS:
			entrySize = entry.getMinSlots();
			break;
		case COLUMN_MAXHUBS:
			entrySize = entry.getMaxHubs();
			break;
		case COLUMN_MAXUSERS:
			entrySize = entry.getMaxUsers();
			break;
		case COLUMN_RELIABILITY:
			entrySize = entry.getReliability();
			break;
		case COLUMN_RATING:
			entryString = entry.getRating();
			doSizeCompare = false;
			break;
		default:
			break;
	}
	
	bool insert = false;
	if (doSizeCompare)
	{
		switch (mode)
		{
			case EQUAL:
				insert = (size == entrySize);
				break;
			case GREATER_EQUAL:
				insert = (size <=  entrySize);
				break;
			case LESS_EQUAL:
				insert = (size >=  entrySize);
				break;
			case GREATER:
				insert = (size < entrySize);
				break;
			case LESS:
				insert = (size > entrySize);
				break;
			case NOT_EQUAL:
				insert = (size != entrySize);
				break;
		}
	}
	else
	{
		if (sel >= COLUMN_LAST)
		{
			if (Util::findSubString(entry.getName(), filter) != string::npos ||
			        Util::findSubString(entry.getDescription(), filter) != string::npos ||
			        Util::findSubString(entry.getServer(), filter) != string::npos ||
			        Util::findSubString(entry.getCountry(), filter) != string::npos ||
			        Util::findSubString(entry.getRating(), filter) != string::npos)
			{
				insert = true;
			}
		}
		if (Util::findSubString(entryString, filter) != string::npos)
			insert = true;
	}
	
	return insert;
}

void PublicHubsFrame::on(DownloadStarting, const string& l) noexcept
{
	speak(SET_TEXT, TSTRING(DOWNLOADING_HUB_LIST) + _T(" (") + Text::toT(l) + _T(")"));
}

void PublicHubsFrame::on(DownloadFailed, const string& l) noexcept
{
	speak(SET_TEXT, TSTRING(DOWNLOAD_FAILED) + _T(' ') + Text::toT(l));
}

void PublicHubsFrame::on(DownloadFinished, const string& l
#ifdef RIP_USE_CORAL
                         , bool /* TODO fromCoral*/
#endif
                        ) noexcept
{
	speak(FINISHED, TSTRING(HUB_LIST_DOWNLOADED) + _T(" (") + Text::toT(l) + _T(")"));
}

void PublicHubsFrame::on(LoadedFromCache, const string& l, const string& /* TODO d*/) noexcept
{
	speak(FINISHED, TSTRING(HUB_LIST_LOADED_FROM_CACHE) + _T(" (") + Text::toT(l) + _T(")"));
}

void PublicHubsFrame::on(Corrupted, const string& l) noexcept
{
	if (l.empty())
	{
		speak(FINISHED, TSTRING(HUBLIST_CACHE_CORRUPTED));
	}
	else
	{
		speak(FINISHED, TSTRING(HUBLIST_DOWNLOAD_CORRUPTED) + _T(" (") + Text::toT(l) + _T(")"));
	}
}

void PublicHubsFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		if (ctrlHubs.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
		updateDropDown();
	}
}

LRESULT PublicHubsFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			cd->clrText = Colors::textColor;
			const auto fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(getPubServer((int)cd->nmcd.dwItemSpec));
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
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
			Colors::alternationBkColor(cd); // [+] IRainman
#endif
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
		}
		default:
			return CDRF_DODEFAULT;
	}
	
}

#endif // End of IRAINMAN_ENABLE_HUB_LIST

/**
 * @file
 * $Id: PublicHubsFrm.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
