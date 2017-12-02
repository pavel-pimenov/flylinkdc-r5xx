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

#if !defined(USERS_FRAME_H)
#define USERS_FRAME_H

#pragma once


#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "ExListViewCtrl.h"
#include "WinUtil.h"
#include "../client/UserInfoBase.h"

#include "../client/FavoriteManager.h"
#include "../client/File.h"
#include "../client/OnlineUser.h"

class UsersFrame : public MDITabChildWindowImpl < UsersFrame, RGB(0, 0, 0), IDR_FAVORITE_USERS >,
	public StaticFrame<UsersFrame, ResourceManager::FAVORITE_USERS, IDC_FAVUSERS>,
	public CSplitterImpl<UsersFrame>,
	private FavoriteManagerListener,
	public UserInfoBaseHandler<UsersFrame, UserInfoGuiTraits::INLINE_CONTACT_LIST>,
	private SettingsManagerListener
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
	
		UsersFrame() : startup(true), m_ignoreListCnange(false) { }
		~UsersFrame()
		{
			images.Destroy();
		}
		
		DECLARE_FRAME_WND_CLASS_EX(_T("UsersFrame"), IDR_FAVORITE_USERS, 0, COLOR_3DFACE);
		
		typedef MDITabChildWindowImpl < UsersFrame, RGB(0, 0, 0), IDR_FAVORITE_USERS > baseClass;
		typedef CSplitterImpl<UsersFrame> splitBase;
		typedef UserInfoBaseHandler<UsersFrame, UserInfoGuiTraits::INLINE_CONTACT_LIST> uibBase;
		
		BEGIN_MSG_MAP(UsersFrame)
		NOTIFY_HANDLER(IDC_USERS, LVN_GETDISPINFO, ctrlUsers.onGetDispInfo)
		NOTIFY_HANDLER(IDC_USERS, LVN_COLUMNCLICK, ctrlUsers.onColumnClick)
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		NOTIFY_HANDLER(IDC_USERS, NM_CUSTOMDRAW, ctrlUsers.onCustomDraw) // [+] IRainman
#endif
		NOTIFY_HANDLER(IDC_USERS, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_USERS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_USERS, NM_DBLCLK, onDoubleClick)
		NOTIFY_HANDLER(IDC_IGNORELIST, NM_CUSTOMDRAW, ctrlBadUsers.onCustomDraw)
		NOTIFY_HANDLER(IDC_IGNORELIST, LVN_ITEMCHANGED, onBadItemChanged)
		COMMAND_ID_HANDLER(IDC_IGNORE_ADD, onIgnoreAdd)
		COMMAND_ID_HANDLER(IDC_IGNORE_REMOVE, onIgnoreRemove)
		COMMAND_ID_HANDLER(IDC_IGNORE_CLEAR, onIgnoreClear)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_CONNECT, onConnect)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		CHAIN_MSG_MAP(splitBase)
		CHAIN_MSG_MAP(uibBase)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onConnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		// !SMT!-S
		LRESULT onIgnorePrivate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSetUserLimit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
		
		LRESULT onSetFocus(UINT uMsg, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlUsers.SetFocus();
			return 0;
		}
		void fillBad();
		void updateBad();
		void saveBad();
		LRESULT onBadItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onIgnoreAdd(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoreRemove(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onIgnoreClear(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
	private:
		class UserInfo;
	public:
		typedef TypedListViewCtrl<UserInfo, IDC_USERS> UserInfoList;
		UserInfoList& getUserList()
		{
			return ctrlUsers;
		}
	private:
		enum
		{
			COLUMN_FIRST,
			COLUMN_NICK = COLUMN_FIRST,
			COLUMN_HUB,
			COLUMN_SEEN,
			COLUMN_DESCRIPTION,
			// [-] COLUMN_SUPERUSER, [!] IRainman deprecated, use speed limit.
			COLUMN_SPEED_LIMIT, // !SMT!-S
			COLUMN_IGNORE, // !SMT!-S
			COLUMN_USER_SLOTS, //[+]ppa
			COLUMN_CID,
			COLUMN_LAST
		};
		enum
		{
			USER_UPDATED
		};
		
		class UserInfo : public UserInfoBase // class UserInfo уже есть в client - не хорошо дублировать имя
		{
			public:
				UserInfo(const FavoriteUser& u) : user(u.getUser())
				{
					update(u);
				}
				
				const tstring& getText(int col) const
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					return columns[col];
				}
				
				static int compareItems(const UserInfo* a, const UserInfo* b, int col)
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					return Util::DefaultSort(a->columns[col].c_str(), b->columns[col].c_str());
				}
				
				int getImageIndex() const
				{
					return 2;
				}
				
				void remove()
				{
					FavoriteManager::getInstance()->removeFavoriteUser(user);
				}
				
				void update(const FavoriteUser& u);
				
				const UserPtr& getUser() const
				{
					return user;
				}
				// TODO private:
				tstring columns[COLUMN_LAST];
			private:
				UserPtr user;
		};
		
		UserInfoList ctrlUsers;
		CImageList images;
		
		bool startup;
		static int columnSizes[COLUMN_LAST];
		static int columnIndexes[COLUMN_LAST];
		
		// FavoriteManagerListener
		void on(UserAdded, const FavoriteUser& aUser) noexcept override;
		void on(UserRemoved, const FavoriteUser& aUser) noexcept override;
		void on(StatusChanged, const UserPtr& aUser) noexcept override;
		
		void on(SettingsManagerListener::Repaint) override;
		
		void addUser(const FavoriteUser& aUser);
		void updateUser(const UserPtr& aUser);
		void updateUser(const int i, UserInfo* p_ui, const FavoriteUser& favUser); // [+] IRainman fix.
		void removeUser(const FavoriteUser& aUser);
		
	public:
		ExListViewCtrl ctrlBadUsers;
		StringSet m_BadUsers;
		CButton ctrlBadAdd;
		CEdit ctrlBadFilter;
		CButton ctrlBadRemove;
#ifdef FLYLINKDC_USE_ALL_CLEAR_FOR_IGNORE_USER
		CButton ctrlBadClear;
#endif
		bool m_ignoreListCnange;
};

#endif // !defined(USERS_FRAME_H)

/**
 * @file
 * $Id: UsersFrame.h,v 1.28 2006/10/22 18:57:56 bigmuscle Exp $
 */
