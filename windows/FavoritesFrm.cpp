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

#include "FavoritesFrm.h"
#include "HubFrame.h"
#include "FavHubProperties.h"
#include "FavHubGroupsDlg.h"
#include "ExMessageBox.h"

HIconWrapper g_hOfflineIco(IDR_OFFLINE_ICO);
HIconWrapper g_hOnlineIco(IDR_ONLINE_ICO);

int FavoriteHubsFrame::columnIndexes[] = { COLUMN_NAME, COLUMN_DESCRIPTION, COLUMN_NICK, COLUMN_PASSWORD, COLUMN_SERVER, COLUMN_USERDESCRIPTION, COLUMN_EMAIL,
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
                                           COLUMN_HIDESHARE,
#endif
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
                                           COLUMN_CONNECTION_STATUS,
                                           COLUMN_LAST_SUCCESFULLY_CONNECTED,
#endif
                                         };
int FavoriteHubsFrame::columnSizes[] = { 200, 290, 125, 100, 100, 125, 125,
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
                                         100,
#endif
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
                                         100,
                                         100,
#endif
                                       };
static ResourceManager::Strings columnNames[] = { ResourceManager::AUTO_CONNECT, ResourceManager::DESCRIPTION,
                                                  ResourceManager::NICK, ResourceManager::PASSWORD, ResourceManager::SERVER, ResourceManager::USER_DESCRIPTION, ResourceManager::EMAIL,
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
                                                  ResourceManager::USER_HIDESHARE,
#endif
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
                                                  ResourceManager::STATUS,
                                                  ResourceManager::LAST_SUCCESFULLY_CONNECTED,
#endif
                                                };

LRESULT FavoriteHubsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlHubs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, IDC_HUBLIST);
	SET_EXTENDENT_LIST_VIEW_STYLE_WITH_CHECK(ctrlHubs);
	SET_LIST_COLOR(ctrlHubs);
	ctrlHubs.EnableGroupView(TRUE);
	
	LVGROUPMETRICS metrics = {0};
	metrics.cbSize = sizeof(metrics);
	metrics.mask = LVGMF_TEXTCOLOR;
	metrics.crHeader = SETTING(TEXT_GENERAL_FORE_COLOR);
	ctrlHubs.SetGroupMetrics(&metrics);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(FAVORITESFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(FAVORITESFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = LVCFMT_LEFT;
		ctrlHubs.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	ctrlHubs.SetColumnOrderArray(COLUMN_LAST, columnIndexes);
	//ctrlHubs.setVisible(SETTING(FAVORITESFRAME_VISIBLE)); // !SMT!-UI
	ctrlHubs.setSort(SETTING(HUBS_FAVORITES_COLUMNS_SORT), ExListViewCtrl::SORT_STRING_NOCASE, BOOLSETTING(HUBS_FAVORITES_COLUMNS_SORT_ASC));
	
	ctrlConnect.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                   BS_PUSHBUTTON, 0, IDC_CONNECT);
	ctrlConnect.SetWindowText(CTSTRING(CONNECT));
	ctrlConnect.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	ctrlNew.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	               BS_PUSHBUTTON, 0, IDC_NEWFAV);
	ctrlNew.SetWindowText(CTSTRING(NEW));
	ctrlNew.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlProps.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 BS_PUSHBUTTON, 0, IDC_EDIT);
	ctrlProps.SetWindowText(CTSTRING(PROPERTIES));
	ctrlProps.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlRemove.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  BS_PUSHBUTTON, 0, IDC_REMOVE);
	ctrlRemove.SetWindowText(CTSTRING(REMOVE));
	ctrlRemove.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlUp.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	              BS_PUSHBUTTON, 0, IDC_MOVE_UP);
	ctrlUp.SetWindowText(CTSTRING(MOVE_UP));
	ctrlUp.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlDown.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                BS_PUSHBUTTON, 0, IDC_MOVE_DOWN);
	ctrlDown.SetWindowText(CTSTRING(MOVE_DOWN));
	ctrlDown.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlManageGroups.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                        BS_PUSHBUTTON, 0, IDC_MANAGE_GROUPS);
	ctrlManageGroups.SetWindowText(CTSTRING(MANAGE_GROUPS));
	ctrlManageGroups.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	m_onlineStatusImg.Create(16, 16, ILC_COLOR32 | ILC_MASK,  0, 2);
	m_onlineStatusImg.AddIcon(g_hOnlineIco);
	m_onlineStatusImg.AddIcon(g_hOfflineIco);
	ctrlHubs.SetImageList(m_onlineStatusImg, LVSIL_SMALL);
	ClientManager::getOnlineClients(m_onlineHubs);
	
	FavoriteManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	ClientManager::getInstance()->addListener(this);
	
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
	create_timer(1000 * 60);
	
#endif
	
	fillList();
	
	hubsMenu.CreatePopupMenu();
	hubsMenu.AppendMenu(MF_STRING, IDC_OPEN_HUB_LOG, CTSTRING(OPEN_HUB_LOG));
	hubsMenu.AppendMenu(MF_SEPARATOR);
	hubsMenu.AppendMenu(MF_STRING, IDC_CONNECT, CTSTRING(CONNECT));
	hubsMenu.AppendMenu(MF_STRING, IDC_NEWFAV, CTSTRING(NEW));
	hubsMenu.AppendMenu(MF_STRING, IDC_MOVE_UP, CTSTRING(MOVE_UP));
	hubsMenu.AppendMenu(MF_STRING, IDC_MOVE_DOWN, CTSTRING(MOVE_DOWN));
	hubsMenu.AppendMenu(MF_SEPARATOR);
	hubsMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
	hubsMenu.AppendMenu(MF_SEPARATOR);
	hubsMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES));
	hubsMenu.SetMenuDefaultItem(IDC_CONNECT);
	
	m_nosave = false;
	
	bHandled = FALSE;
	return TRUE;
}

FavoriteHubsFrame::StateKeeper::StateKeeper(ExListViewCtrl& hubs_, bool ensureVisible_) :
	hubs(hubs_),
	ensureVisible(ensureVisible_)
{
	hubs.SetRedraw(FALSE);
	
	// in grouped mode, the indexes of each item are completely random, so use entry pointers instead
	int i = -1;
	while ((i = hubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		selected.push_back((FavoriteHubEntry*)hubs.GetItemData(i));
	}
	
	SCROLLINFO si = { 0 };
	si.cbSize = sizeof(si);
	si.fMask = SIF_POS;
	hubs.GetScrollInfo(SB_VERT, &si);
	
	scroll = si.nPos;
}

FavoriteHubsFrame::StateKeeper::~StateKeeper()
{
	// restore visual updating now, otherwise it doesn't always scroll
	hubs.SetRedraw(TRUE);
	
	for (auto i = selected.cbegin(), iend = selected.cend(); i != iend; ++i)
	{
		const auto l_cnt = hubs.GetItemCount();
		for (int j = 0; j < l_cnt; ++j)
		{
			if ((FavoriteHubEntry*)hubs.GetItemData(j) == *i)
			{
				hubs.SetItemState(j, LVIS_SELECTED, LVIS_SELECTED);
				if (ensureVisible)
					hubs.EnsureVisible(j, FALSE);
				break;
			}
		}
	}
	
	ListView_Scroll(hubs.m_hWnd, 0, scroll);
}

const FavoriteHubEntryList& FavoriteHubsFrame::StateKeeper::getSelection() const
{
	return selected;
}

void FavoriteHubsFrame::openSelected()
{
	if (!checkNick())
		return;
		
	int i = -1;
	while ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		FavoriteHubEntry* entry = (FavoriteHubEntry*)ctrlHubs.GetItemData(i);
		RecentHubEntry r;
		r.setName(entry->getName());
		r.setDescription(entry->getDescription());
		r.setUsers("*");
		r.setShared("*");
		r.setServer(entry->getServer());
		FavoriteManager::getInstance()->addRecent(r);
		HubFrame::openHubWindow(true,
		                        entry->getServer(),
		                        entry->getName(),
		                        entry->getRawOne(),
		                        entry->getRawTwo(),
		                        entry->getRawThree(),
		                        entry->getRawFour(),
		                        entry->getRawFive(),
		                        entry->getWindowPosX(),
		                        entry->getWindowPosY(),
		                        entry->getWindowSizeX(),
		                        entry->getWindowSizeY(),
		                        entry->getWindowType(),
		                        entry->getChatUserSplit(),
		                        entry->getUserListState(),
		                        entry->getSuppressChatAndPM());
	}
	return;
}

void FavoriteHubsFrame::addEntry(const FavoriteHubEntry* entry, int pos, int groupIndex)
{
	TStringList l;
	l.push_back(Text::toT(entry->getName()));
	l.push_back(Text::toT(entry->getDescription()));
	l.push_back(Text::toT(entry->getNick(false)));
	l.push_back(tstring(entry->getPassword().size(), '*'));
	l.push_back(Text::toT(entry->getServer()));
	l.push_back(Text::toT(entry->getUserDescription()));
	l.push_back(Text::toT(entry->getEmail()));
	/* [-] IRainman fix.
	    l.push_back(Text::toT(entry->getRawOne()));
	    l.push_back(Text::toT(entry->getRawTwo()));
	    l.push_back(Text::toT(entry->getRawThree()));
	    l.push_back(Text::toT(entry->getRawFour()));
	    l.push_back(Text::toT(entry->getRawFive()));
	*/
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	l.push_back(entry->getHideShare() ? TSTRING(YES) : Util::emptyStringT/*TSTRING(NO)*/);
#endif
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
	const ConnectionStatus& l_connectionStatus = entry->getConnectionStatus();
	const time_t l_curTime = GET_TIME();
	
	l.push_back(getLastAttempts(l_connectionStatus, l_curTime));
	l.push_back(getLastSucces(l_connectionStatus, l_curTime));
#endif
	
	const bool b = entry->getConnect();
	const int i = ctrlHubs.insert(pos, l, 0, (LPARAM)entry);
	ctrlHubs.SetCheckState(i, b);
	
	LVITEM lvItem = { 0 };
	lvItem.mask = LVIF_GROUPID | LVIF_IMAGE;
	lvItem.iItem = i;
	// lvItem.iImage = isOnline(entry->getServer()) ? 0 : 1;
	if (isOnline(entry->getServer()))
		lvItem.iImage = 0;
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS   // The protection, just in case ( SCALOlaz 17/05/2015 )
	else if (getLastSucces(l_connectionStatus, l_curTime) == TSTRING(JUST_NOW))
		lvItem.iImage = 1;
#endif
	else
		lvItem.iImage = -1;
	lvItem.iGroupId = groupIndex;
	ctrlHubs.SetItem(&lvItem);
}

LRESULT FavoriteHubsFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam == HUB_CONNECTED)
	{
		std::unique_ptr<string> hub(reinterpret_cast<string*>(lParam));
		m_onlineHubs.insert(*hub);
		for (int i = 0; i < ctrlHubs.GetItemCount(); ++i)
		{
			const FavoriteHubEntry* e = (FavoriteHubEntry*)ctrlHubs.GetItemData(i);
			if (e && e->getServer() == *hub)
			{
				ctrlHubs.SetItem(i, 0, LVIF_IMAGE, NULL, 0, 0, 0, NULL);
				ctrlHubs.Update(i);
				return 0;
			}
		}
	}
	
	else if (wParam == HUB_DISCONNECTED)
	{
		std::unique_ptr<string> hub(reinterpret_cast<string*>(lParam));
		m_onlineHubs.erase(*hub);
		
		for (int i = 0; i < ctrlHubs.GetItemCount(); ++i)
		{
			FavoriteHubEntry* e = (FavoriteHubEntry*)ctrlHubs.GetItemData(i);
			if (e && e->getServer() == *hub)
			{
				ctrlHubs.SetItem(i, 0, LVIF_IMAGE, NULL, 1, 0, 0, NULL);
				ctrlHubs.Update(i);
				return 0;
			}
		}
	}
	
	return 0;
}

LRESULT FavoriteHubsFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlHubs)
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
		hubsMenu.EnableMenuItem(IDC_OPEN_HUB_LOG, status);
		hubsMenu.EnableMenuItem(IDC_CONNECT, status);
		hubsMenu.EnableMenuItem(IDC_EDIT, status);
		hubsMenu.EnableMenuItem(IDC_MOVE_UP, status);
		hubsMenu.EnableMenuItem(IDC_MOVE_DOWN, status);
		hubsMenu.EnableMenuItem(IDC_REMOVE, status);
		
		tstring x;
		if (ctrlHubs.GetSelectedCount() == 1)
		{
			FavoriteHubEntry* f = (FavoriteHubEntry*)ctrlHubs.GetItemData(ctrlHubs.GetSelectedIndex());
			x = Text::toT(f->getName());
		}
		
		if (!x.empty())
			hubsMenu.InsertSeparatorFirst(x);
			
		hubsMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		if (!x.empty())
			hubsMenu.RemoveFirstItem();
			
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT FavoriteHubsFrame::onDoubleClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem == -1)
	{
		PostMessage(WM_COMMAND, IDC_NEWFAV, 0);
	}
	else
	{
		PostMessage(WM_COMMAND, IDC_CONNECT, 0);
	}
	
	return 0;
}

LRESULT FavoriteHubsFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_NEWFAV, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		case VK_RETURN:
			PostMessage(WM_COMMAND, IDC_CONNECT, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlHubs.GetSelectedCount())
	{
		int i = -1;
		UINT checkState = BOOLSETTING(CONFIRM_HUB_REMOVAL) ? BST_UNCHECKED : BST_CHECKED; // [+] InfinitySky.
		if (checkState == BST_CHECKED || ::MessageBox(m_hWnd, CTSTRING(REALLY_REMOVE), getFlylinkDCAppCaptionWithVersionT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES) // [~] InfinitySky.
		{
			while ((i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				FavoriteManager::getInstance()->removeFavorite((FavoriteHubEntry*)ctrlHubs.GetItemData(i));
			}
		}
		// Let's update the setting unchecked box means we bug user again...
		SET_SETTING(CONFIRM_HUB_REMOVAL, checkState != BST_CHECKED); // [+] InfinitySky.
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	if ((i = ctrlHubs.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		FavoriteHubEntry* e = (FavoriteHubEntry*)ctrlHubs.GetItemData(i);
		dcassert(e != NULL);
		if (!e)
			return 0;
		FavHubProperties dlg(e);
		if (dlg.DoModal(m_hWnd) == IDOK)
		{
			StateKeeper keeper(ctrlHubs);
			fillList();
		}
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onNew(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	FavoriteHubEntry e;
	FavHubProperties dlg(&e);
	
	while (true)
	{
		if (dlg.DoModal(*this) == IDOK)
		{
			if (FavoriteManager::getFavoriteHubEntry(e.getServer()))
			{
				MessageBox(
				    CTSTRING(FAVORITE_HUB_ALREADY_EXISTS), _T(" "), MB_ICONWARNING | MB_OK);
			}
			else
			{
				FavoriteManager::getInstance()->addFavorite(e);
				break;
			}
		}
		else
		{
			break;
		}
	}
	return 0;
}

bool FavoriteHubsFrame::checkNick()
{
	if (SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_ICONSTOP | MB_OK);
		return false;
	}
	return true;
}

LRESULT FavoriteHubsFrame::onMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	handleMove(true);
	return 0;
}

LRESULT FavoriteHubsFrame::onMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	handleMove(false);
	return 0;
}

void FavoriteHubsFrame::handleMove(bool up)
{
	FavoriteHubEntryList fh_copy;
	{
		// [!] IRainman fix.
		FavoriteManager::LockInstanceHubs lockedInstanceHubs;
		FavoriteHubEntryList& fh = lockedInstanceHubs.getFavoriteHubs();
		if (fh.size() <= 1)
			return;
			
		fh_copy = fh;
	}
	
	StateKeeper keeper(ctrlHubs);
	const FavoriteHubEntryList& selected = keeper.getSelection();
	
	if (!up)
		reverse(fh_copy.begin(), fh_copy.end());
	FavoriteHubEntryList moved;
	for (auto i = fh_copy.begin(); i != fh_copy.end(); ++i)
	{
		if (find(selected.begin(), selected.end(), *i) == selected.end())
			continue;
		if (find(moved.begin(), moved.end(), *i) != moved.end())
			continue;
		const string& group = (*i)->getGroup();
		for (auto j = i; ;)
		{
			if (j == fh_copy.begin())
			{
				// couldn't move within the same group; change group.
				TStringList groups(getSortedGroups());
				if (!up)
					reverse(groups.begin(), groups.end());
				const auto& ig = find(groups.begin(), groups.end(), Text::toT(group));
				if (ig != groups.begin())
				{
					auto f = *i;
					f->setGroup(Text::fromT(*(ig - 1)));
					i = fh_copy.erase(i);
					fh_copy.push_back(f);
					moved.push_back(f);
				}
				break;
			}
			if ((*--j)->getGroup() == group)
			{
				std::swap(*i, *j);
				break;
			}
		}
	}
	if (!up)
		reverse(fh_copy.begin(), fh_copy.end());
		
	{
		// [!] IRainman fix.
		FavoriteManager::LockInstanceHubs lockedInstanceHubs(true);
		lockedInstanceHubs.getFavoriteHubs() = fh_copy;
	}
	FavoriteManager::save_favorites();
	
	fillList();
}

TStringList FavoriteHubsFrame::getSortedGroups() const
{
	std::set<tstring, noCaseStringLess> sorted_groups;
	{
		FavoriteManager::LockInstanceHubs lockedInstanceHubs;
		const FavHubGroups& favHubGroups = lockedInstanceHubs.getFavHubGroups();
		for (auto i = favHubGroups.cbegin(), iend = favHubGroups.cend(); i != iend; ++i)
			sorted_groups.insert(Text::toT(i->first));
	}
	
	TStringList groups(sorted_groups.begin(), sorted_groups.end());
	groups.insert(groups.begin(), Util::emptyStringT); // default group (otherwise, hubs without group don't show up)
	return groups;
}

void FavoriteHubsFrame::fillList()
{
	bool old_nosave = m_nosave;
	m_nosave = true;
	
	ctrlHubs.DeleteAllItems();
	
	// sort groups
	TStringList groups(getSortedGroups());
	
	for (size_t i = 0; i < groups.size(); ++i)
	{
		// insert groups
		LVGROUP lg = {0};
		lg.cbSize = sizeof(lg);
		lg.iGroupId = static_cast<int>(i);
		lg.state = LVGS_NORMAL |
#ifdef FLYLINKDC_SUPPORT_WIN_XP
		           (CompatibilityManager::isOsVistaPlus() ? LVGS_COLLAPSIBLE : 0)
#else
		           LVGS_COLLAPSIBLE
#endif
		           ;
		lg.mask = LVGF_GROUPID | LVGF_HEADER | LVGF_STATE | LVGF_ALIGN;
		lg.uAlign = LVGA_HEADER_LEFT;
		
		// Header-title must be unicode (Convert if necessary)
		lg.pszHeader = (LPWSTR)groups[i].c_str();
		lg.cchHeader = static_cast<int>(groups[i].size());
		ctrlHubs.InsertGroup(i, &lg);
	}
	
	{
		// [!] IRainman fix.
		FavoriteManager::LockInstanceHubs lockedInstanceHubs;
		const auto& fl = lockedInstanceHubs.getFavoriteHubs();
		auto cnt = ctrlHubs.GetItemCount();
		for (auto i = fl.cbegin(); i != fl.cend(); ++i)
		{
			const string& group = (*i)->getGroup();
			
			int index = 0;
			if (!group.empty())
			{
				TStringList::const_iterator groupI = find(groups.begin() + 1, groups.end(), Text::toT(group));
				if (groupI != groups.end())
					index = groupI - groups.begin();
			}
			
			addEntry(*i, cnt++, index);
		}
	}
	
	m_nosave = old_nosave;
}

LRESULT FavoriteHubsFrame::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NMITEMACTIVATE* l = (NMITEMACTIVATE*)pnmh;
	if (l->iItem != -1)
	{
		const auto l_enabled = ctrlHubs.GetItemState(l->iItem, LVIS_SELECTED);
		::EnableWindow(GetDlgItem(IDC_CONNECT), l_enabled);
		::EnableWindow(GetDlgItem(IDC_REMOVE), l_enabled);
		::EnableWindow(GetDlgItem(IDC_EDIT), l_enabled);
		::EnableWindow(GetDlgItem(IDC_MOVE_UP), l_enabled);
		::EnableWindow(GetDlgItem(IDC_MOVE_DOWN), l_enabled);
		if (!m_nosave && ((l->uNewState & LVIS_STATEIMAGEMASK) != (l->uOldState & LVIS_STATEIMAGEMASK)))
		{
			FavoriteHubEntry* f = (FavoriteHubEntry*)ctrlHubs.GetItemData(l->iItem);
			const bool l_connect = ctrlHubs.GetCheckState(l->iItem) != FALSE;
			f->setConnect(l_connect);
			FavoriteManager::save_favorites();
		}
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		safe_destroy_timer();
#endif
		ClientManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		FavoriteManager::getInstance()->removeListener(this);
		WinUtil::setButtonPressed(IDC_FAVORITES, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlHubs, SettingsManager::FAVORITESFRAME_ORDER,
		                         SettingsManager::FAVORITESFRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		                         
		SET_SETTING(HUBS_FAVORITES_COLUMNS_SORT, ctrlHubs.getSortColumn());
		SET_SETTING(HUBS_FAVORITES_COLUMNS_SORT_ASC, ctrlHubs.isAscending());
		bHandled = FALSE;
		return 0;
	}
}

void FavoriteHubsFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	CRect rc = rect;
	rc.bottom -= 28;
	ctrlHubs.MoveWindow(rc);
	
	const long bwidth = 90;
	const long bspace = 10;
	
	rc = rect;
	rc.bottom -= 2;
	rc.top = rc.bottom - 22;
	
	rc.left = 2;
	rc.right = rc.left + bwidth;
	ctrlNew.MoveWindow(rc);
	
	rc.OffsetRect(bwidth + 2, 0);
	ctrlProps.MoveWindow(rc);
	
	rc.OffsetRect(bwidth + 2, 0);
	ctrlRemove.MoveWindow(rc);
	
	rc.OffsetRect(bspace + bwidth + 2, 0);
	ctrlUp.MoveWindow(rc);
	
	rc.OffsetRect(bwidth + 2, 0);
	ctrlDown.MoveWindow(rc);
	
	rc.OffsetRect(bspace + bwidth + 2, 0);
	ctrlConnect.MoveWindow(rc);
	
	rc.OffsetRect(bspace + bwidth + 2, 0);
	rc.right += 16;
	ctrlManageGroups.MoveWindow(rc);
}

LRESULT FavoriteHubsFrame::onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlHubs.GetSelectedCount() == 1)
	{
		int i = ctrlHubs.GetNextItem(-1, LVNI_SELECTED);
		FavoriteHubEntry* entry = (FavoriteHubEntry*)ctrlHubs.GetItemData(i);
		StringMap params;
		params["hubNI"] = entry->getName();
		params["hubURL"] = entry->getServer();
		params["myNI"] = entry->getNick();
		
		WinUtil::openLog(SETTING(LOG_FILE_MAIN_CHAT), params, TSTRING(NO_LOG_FOR_HUB));
	}
	return 0;
}

LRESULT FavoriteHubsFrame::onManageGroups(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	FavHubGroupsDlg dlg;
	dlg.DoModal();
	
	StateKeeper keeper(ctrlHubs);
	fillList();
	
	return 0;
}

void FavoriteHubsFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlHubs.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

LRESULT FavoriteHubsFrame::onColumnClickHublist(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	// On sort, disable move functionality.
	//::EnableWindow(GetDlgItem(IDC_MOVE_UP), FALSE);
	//::EnableWindow(GetDlgItem(IDC_MOVE_DOWN), FALSE);
	NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
	if (l->iSubItem == ctrlHubs.getSortColumn())
	{
		if (!ctrlHubs.isAscending())
		{
			ctrlHubs.setSort(-1, ctrlHubs.getSortType());
		}
		else
		{
			ctrlHubs.setSortDirection(false);
		}
	}
	else
	{
		ctrlHubs.setSort(l->iSubItem, ExListViewCtrl::SORT_STRING_NOCASE);
	}
	return 0;
}
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
tstring FavoriteHubsFrame::getLastAttempts(const ConnectionStatus& connectionStatus, const time_t curTime)
{
	if (connectionStatus.getLastAttempts())
	{
		const time_t delta = curTime - connectionStatus.getLastAttempts();
		if (delta > 60)
			return connectionStatus.getStatusText() + _T(" (") + Text::toT(Util::formatTime(delta, false)) + _T(' ') + TSTRING(AGO) + _T(')');
	}
	return connectionStatus.getStatusText();
}
tstring FavoriteHubsFrame::getLastSucces(const ConnectionStatus& connectionStatus, const time_t curTime)
{
	if (connectionStatus.getLastSucces())
	{
		const time_t delta = curTime - connectionStatus.getLastSucces();
		if (delta > 60)
		{
			if (connectionStatus.getLastAttempts() == connectionStatus.getLastSucces())
				return TSTRING(AT_THE_SAME_TIME);
			else
				return Text::toT(Util::formatTime(curTime - connectionStatus.getLastSucces(), false)) + _T(' ') + TSTRING(AGO);
		}
		else
			return TSTRING(JUST_NOW);
	}
	else
		return Util::emptyStringT;
}
LRESULT FavoriteHubsFrame::onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	const time_t curTime = GET_TIME();
	CLockRedraw<> l_lock_draw(ctrlHubs);
	const int l_cnt = ctrlHubs.GetItemCount();
	for (int pos = 0; pos < l_cnt; ++pos)
	{
		const FavoriteHubEntry* entry = (const FavoriteHubEntry*)ctrlHubs.GetItemData(pos);
		dcassert(entry);
		if (entry)
		{
			const ConnectionStatus& connectionStatus = entry->getConnectionStatus();
			ctrlHubs.SetItemText(pos, COLUMN_CONNECTION_STATUS, getLastAttempts(connectionStatus, curTime).c_str());
			ctrlHubs.SetItemText(pos, COLUMN_LAST_SUCCESFULLY_CONNECTED, getLastSucces(connectionStatus, curTime).c_str());
		}
	}
	if (ctrlHubs.getSortColumn() == COLUMN_CONNECTION_STATUS || ctrlHubs.getSortColumn() == COLUMN_LAST_SUCCESFULLY_CONNECTED)
		ctrlHubs.resort();
	return 0;
}
#endif // IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS

/**
 * @file
 * $Id: FavoritesFrm.cpp,v 1.33 2006/08/30 12:32:00 bigmuscle Exp $
 */
