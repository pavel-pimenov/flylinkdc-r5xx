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

#include "UsersFrame.h"
#include "LineDlg.h"
#include "HubFrame.h"
#include "TextFrame.h"
#include "ResourceLoader.h" // [+] InfinitySky. PNG Support from Apex 1.3.8.
#include "ExMessageBox.h"

int UsersFrame::columnIndexes[] = { COLUMN_NICK, COLUMN_HUB, COLUMN_SEEN, COLUMN_DESCRIPTION, COLUMN_SPEED_LIMIT, COLUMN_IGNORE, COLUMN_USER_SLOTS, COLUMN_CID }; // !SMT!-S
int UsersFrame::columnSizes[] = { 200, 300, 150, 200, 100, 100, 100, 300 }; // !SMT!-S
static ResourceManager::Strings columnNames[] = { ResourceManager::AUTO_GRANT_NICK, ResourceManager::LAST_HUB, ResourceManager::LAST_SEEN, ResourceManager::DESCRIPTION,
                                                  ResourceManager::UPLOAD_SPEED_LIMIT,
                                                  ResourceManager::IGNORE_PRIVATE,
                                                  ResourceManager::SLOTS,
                                                  ResourceManager::CID
                                                }; // !SMT!-S

LRESULT UsersFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);  //[-] SCALOlaz
	// ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_USERS);
	SET_EXTENDENT_LIST_VIEW_STYLE_WITH_CHECK(ctrlUsers);
	ResourceLoader::LoadImageList(IDR_FAV_USERS_STATES, images, 16, 16);
	ctrlUsers.SetImageList(images, LVSIL_SMALL);
	
	SET_LIST_COLOR(ctrlUsers);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(USERSFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(USERSFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		ctrlUsers.InsertColumn(j, TSTRING_I(columnNames[j]), LVCFMT_LEFT, columnSizes[j], j);
	}
	
	ctrlUsers.setColumnOrderArray(COLUMN_LAST, columnIndexes);
	ctrlUsers.setVisible(SETTING(USERSFRAME_VISIBLE)); // !SMT!-UI
	//ctrlUsers.setSortColumn(COLUMN_NICK);
	ctrlUsers.setSortColumn(SETTING(USERS_COLUMNS_SORT));
	ctrlUsers.setAscending(BOOLSETTING(USERS_COLUMNS_SORT_ASC));
	
	// [-] brain-ripper
	// Make menu dynamic (in context menu handler), since its content depends of which
	// user selected (for add/remove favorites item)
	/*
	usersMenu.CreatePopupMenu();
	usersMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES));
	usersMenu.AppendMenu(MF_STRING, IDC_SUPER_USER, CTSTRING(SUPER_USER));
	usersMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)WinUtil::speedMenu, CTSTRING(UPLOAD_SPEED_LIMIT)); // !SMT!-S
	usersMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)WinUtil::privateMenu, CTSTRING(IGNORE_PRIVATE)); // !SMT!-PSW
	usersMenu.AppendMenu(MF_STRING, IDC_OPEN_USER_LOG, CTSTRING(OPEN_USER_LOG));
	usersMenu.AppendMenu(MF_SEPARATOR);
	appendUserItems(usersMenu, Util::emptyString); // TODO: hubhint
	usersMenu.AppendMenu(MF_SEPARATOR);
	usersMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
	*/
	
	FavoriteManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	
	CLockRedraw<> l_lock_draw(ctrlUsers);
	{
		FavoriteManager::LockInstanceUsers lockedInstance;
		const auto& l_fav_users = lockedInstance.getFavoriteUsersL();
		for (auto i = l_fav_users.cbegin(); i != l_fav_users.cend(); ++i)
		{
			addUser(i->second);
		}
	}
	startup = false;
	bHandled = FALSE;
	return TRUE;
}

LRESULT UsersFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlUsers && ctrlUsers.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlUsers, pt);
		}
		
		clearUserMenu(); // [+] IRainman fix.
		
		// [+] brain-ripper
		// Make menu dynamic, since its content depends of which
		// user selected (for add/remove favorites item)
		OMenu usersMenu;
		usersMenu.CreatePopupMenu();
		usersMenu.AppendMenu(MF_STRING, IDC_EDIT, CTSTRING(PROPERTIES));
		usersMenu.AppendMenu(MF_STRING, IDC_OPEN_USER_LOG, CTSTRING(OPEN_USER_LOG));
		usersMenu.AppendMenu(MF_STRING, IDC_REMOVE_FROM_FAVORITES, CTSTRING(REMOVE_FROM_FAVORITES)); //[+] NightOrion
		
		tstring x;
		if (ctrlUsers.GetSelectedCount() == 1)
		{
			const auto user = ctrlUsers.getItemData(ctrlUsers.GetSelectedIndex())->getUser();
			if (user->isOnline())
			{
				usersMenu.AppendMenu(MF_SEPARATOR);
				x = user->getLastNickT();
				reinitUserMenu(user, Util::emptyString); // TODO: add hub hint.
				if (!x.empty())
					usersMenu.InsertSeparatorFirst(x);
					
				appendAndActivateUserItems(usersMenu);
			}
		}
		else
		{
			usersMenu.AppendMenu(MF_SEPARATOR);
			appendAndActivateUserItems(usersMenu);
		}
		usersMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		if (!x.empty())
			usersMenu.RemoveFirstItem();
			
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

void UsersFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	CRect rc = rect;
	ctrlUsers.MoveWindow(rc);
}

LRESULT UsersFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlUsers.getSelectedCount())
	{
		int i = -1;
		UINT checkState = BOOLSETTING(CONFIRM_USER_REMOVAL) ? BST_UNCHECKED : BST_CHECKED;
		if (checkState == BST_CHECKED || ::MessageBox(m_hWnd, CTSTRING(REALLY_REMOVE), _T(APPNAME) _T(" ") T_VERSIONSTRING, CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES)
		{
			while ((i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				ctrlUsers.getItemData(i)->remove();
			}
		}
		// Let's update the setting unchecked box means we bug user again...
		SET_SETTING(CONFIRM_USER_REMOVAL, checkState != BST_CHECKED);
	}
	return 0;
}

LRESULT UsersFrame::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlUsers.GetSelectedCount() == 1)
	{
		int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
		UserInfo* ui = ctrlUsers.getItemData(i);
		dcassert(i != -1);
		LineDlg dlg;
		dlg.description = TSTRING(DESCRIPTION);
		dlg.title = ui->getText(COLUMN_NICK);
		dlg.line = ui->getText(COLUMN_DESCRIPTION);
		if (dlg.DoModal(m_hWnd))
		{
			FavoriteManager::getInstance()->setUserDescription(ui->getUser(), Text::fromT(dlg.line));
			ui->columns[COLUMN_DESCRIPTION] = dlg.line;
			ctrlUsers.updateItem(i);
		}
	}
	return 0;
}

LRESULT UsersFrame::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* l = (NMITEMACTIVATE*)pnmh;
	if (!startup && l->iItem != -1 && ((l->uNewState & LVIS_STATEIMAGEMASK) != (l->uOldState & LVIS_STATEIMAGEMASK)))
	{
		FavoriteManager::getInstance()->setAutoGrantSlot(ctrlUsers.getItemData(l->iItem)->getUser(), ctrlUsers.GetCheckState(l->iItem) != FALSE);
	}
	return 0;
}

LRESULT UsersFrame::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem != -1)
	{
		// !SMT!-UI
		static const int cmd[] = { IDC_GETLIST, IDC_PRIVATE_MESSAGE, IDC_MATCH_QUEUE, IDC_EDIT, IDC_OPEN_USER_LOG };
		PostMessage(WM_COMMAND, cmd[SETTING(FAVUSERLIST_DBLCLICK)], 0);
	}
	else
	{
		bHandled = FALSE;
	}
	
	return 0;
}

LRESULT UsersFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		case VK_RETURN:
			PostMessage(WM_COMMAND, IDC_EDIT, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT UsersFrame::onConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const int l_cnt = ctrlUsers.GetItemCount();
	for (int i = 0; i < l_cnt; ++i)
	{
		dcassert(l_cnt == ctrlUsers.GetItemCount());
		UserInfo *ui = ctrlUsers.getItemData(i);
		const string& l_url = FavoriteManager::getInstance()->getUserUrl(ui->getUser());
		if (!l_url.empty())
		{
			HubFrame::openWindow(l_url);
		}
	}
	return 0;
}

void UsersFrame::addUser(const FavoriteUser& user)
{
	auto ui = new UserInfo(user); // [+] IRainman fix.
	int i = ctrlUsers.insertItem(ui, 0);
	bool b = user.isSet(FavoriteUser::FLAG_GRANT_SLOT);
	ctrlUsers.SetCheckState(i, b);
	updateUser(i, ui, user); // [!] IRainman fix.
}

void UsersFrame::updateUser(const UserPtr& user)
{
	const int l_cnt = ctrlUsers.GetItemCount();
	for (int i = 0; i < l_cnt; ++i)
	{
		dcassert(l_cnt == ctrlUsers.GetItemCount());
		UserInfo *ui = ctrlUsers.getItemData(i);
		if (ui->getUser() == user)
		{
			FavoriteUser currentFavUser;
			if (FavoriteManager::getInstance()->getFavoriteUser(user, currentFavUser))
			{
				updateUser(i, ui, currentFavUser);
			}
		}
	}
}

void UsersFrame::updateUser(const int i, UserInfo* p_ui, const FavoriteUser& favUser) // [+] IRainman fix.
{
	p_ui->columns[COLUMN_SEEN] = favUser.getUser()->isOnline() ? TSTRING(ONLINE) : Text::toT(Util::formatDigitalClock(favUser.getLastSeen()));
	
	// !SMT!-UI
	int imageIndex = favUser.getUser()->isOnline() ? (favUser.getUser()->isAway() ? 1 : 0) : 2;
	
	if (favUser.getUploadLimit() == FavoriteUser::UL_BAN || favUser.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
	{
		imageIndex += 3;
	}
	
	p_ui->update(favUser);
	
	ctrlUsers.SetItem(i, 0, LVIF_IMAGE, NULL, imageIndex, 0, 0, NULL);
	
	ctrlUsers.updateItem(i);
	setCountMessages(ctrlUsers.GetItemCount());
}

void UsersFrame::removeUser(const FavoriteUser& aUser)
{
	const int l_cnt = ctrlUsers.GetItemCount();
	for (int i = 0; i < l_cnt; ++i)
	{
		dcassert(l_cnt == ctrlUsers.GetItemCount());
		UserInfo *ui = ctrlUsers.getItemData(i);
		if (ui->getUser() == aUser.getUser())
		{
			ctrlUsers.DeleteItem(i);
			delete ui;
			setCountMessages(ctrlUsers.GetItemCount());
			return;
		}
	}
}

LRESULT UsersFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		FavoriteManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		//WinUtil::UnlinkStaticMenus(usersMenu); // !SMT!-S
		WinUtil::setButtonPressed(IDC_FAVUSERS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		ctrlUsers.saveHeaderOrder(SettingsManager::USERSFRAME_ORDER, SettingsManager::USERSFRAME_WIDTHS, SettingsManager::USERSFRAME_VISIBLE); // !SMT!-UI
		SET_SETTING(USERS_COLUMNS_SORT, ctrlUsers.getSortColumn());
		SET_SETTING(USERS_COLUMNS_SORT_ASC, ctrlUsers.isAscending());
		ctrlUsers.DeleteAndCleanAllItems();
		
		bHandled = FALSE;
		return 0;
	}
}

void UsersFrame::UserInfo::update(const FavoriteUser& u)
{
	columns[COLUMN_NICK] = Text::toT(u.getNick());
	columns[COLUMN_HUB] = user->isOnline() ? WinUtil::getHubNames(u.getUser(), u.getUrl()).first : Text::toT(u.getUrl());
	columns[COLUMN_SEEN] = user->isOnline() ? TSTRING(ONLINE) : Text::toT(Util::formatDigitalClock(u.getLastSeen()));
	columns[COLUMN_DESCRIPTION] = Text::toT(u.getDescription());
	
	// !SMT!-S
	if (u.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
		columns[COLUMN_IGNORE] = TSTRING(IGNORE_S);
	else if (u.isSet(FavoriteUser::FLAG_FREE_PM_ACCESS))
		columns[COLUMN_IGNORE] = TSTRING(FREE_PM_ACCESS);
	else
		columns[COLUMN_IGNORE].clear();
		
	columns[COLUMN_SPEED_LIMIT] = Text::toT(FavoriteUser::getSpeedLimitText(u.getUploadLimit()));
	//[+]PPA
	columns[COLUMN_USER_SLOTS] = Util::toStringW(u.getUser()->getSlots());
	columns[COLUMN_CID] = Text::toT(u.getUser()->getCID().toBase32());
}

LRESULT UsersFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (wParam == USER_UPDATED)
	{
		UserPtr* user = (UserPtr*)lParam;
		updateUser(*user);
		delete user;
	}
	return 0;
}

LRESULT UsersFrame::onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlUsers.GetSelectedCount() == 1)
	{
		int i = ctrlUsers.GetNextItem(-1, LVNI_SELECTED);
		UserInfo* ui = ctrlUsers.getItemData(i);
		dcassert(i != -1);
		
		const auto& l_user = ui->getUser(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'ui->getUser()' expression repeatedly. usersframe.cpp 445
		StringMap params;
		params["hubNI"] = Util::toString(ClientManager::getHubNames(l_user->getCID(), Util::emptyString));
		params["hubURL"] = Util::toString(ClientManager::getHubs(l_user->getCID(), Util::emptyString));
		params["userCID"] = l_user->getCID().toBase32();
		params["userNI"] = l_user->getLastNick();
		params["myCID"] = ClientManager::getMyCID().toBase32();
		
		WinUtil::openLog(SETTING(LOG_FILE_PRIVATE_CHAT), params, TSTRING(NO_LOG_FOR_USER));
	}
	return 0;
}

void UsersFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		if (ctrlUsers.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

// !SMT!-S
LRESULT UsersFrame::onIgnorePrivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		UserInfo *ui = ctrlUsers.getItemData(i);
		
		FavoriteManager::getInstance()->setNormalPM(ui->getUser());
		ui->columns[COLUMN_IGNORE].clear();
		
		switch (wID)
		{
			case IDC_PM_IGNORED:
				ui->columns[COLUMN_IGNORE] = TSTRING(IGNORE_S);
				FavoriteManager::getInstance()->setIgnorePM(ui->getUser(), true);
				break;
			case IDC_PM_FREE:
				ui->columns[COLUMN_IGNORE] = TSTRING(FREE_PM_ACCESS);
				FavoriteManager::getInstance()->setFreePM(ui->getUser(), true);
				break;
		};
		
		updateUser(ui->getUser());
		ctrlUsers.updateItem(i);
	}
	return 0;
}

// !SMT!-S
LRESULT UsersFrame::onSetUserLimit(WORD /* wNotifyCode */, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	MENUINFO menuInfo = {0};
	menuInfo.cbSize = sizeof(MENUINFO);
	menuInfo.fMask = MIM_MENUDATA;
	speedMenu.GetMenuInfo(&menuInfo);
	const int iLimit = menuInfo.dwMenuData;
	const int lim = getSpeedLimitByCtrlId(wID, iLimit);
	int i = -1;
	while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		UserInfo *ui = ctrlUsers.getItemData(i);
		FavoriteManager::getInstance()->setUploadLimit(ui->getUser(), lim);
		ui->columns[COLUMN_SPEED_LIMIT] = Text::toT(FavoriteUser::getSpeedLimitText(lim));
		updateUser(ui->getUser());
		ctrlUsers.updateItem(i);
	}
	return 0;
}

/**
 * @file
 * $Id: UsersFrame.cpp,v 1.37 2006/08/13 19:03:50 bigmuscle Exp $
 */
