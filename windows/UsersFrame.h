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

#if !defined(USERS_FRAME_H)
#define USERS_FRAME_H

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "WinUtil.h"
#include "../client/UserInfoBase.h"

#include "../client/FavoriteManager.h"
#include "../client/File.h"
#include "../client/OnlineUser.h"

class UsersFrame : public MDITabChildWindowImpl < UsersFrame, RGB(0, 0, 0), IDR_FAVORITE_USERS > , public StaticFrame<UsersFrame, ResourceManager::FAVORITE_USERS, IDC_FAVUSERS>,
	private FavoriteManagerListener, public UserInfoBaseHandler<UsersFrame, UserInfoGuiTraits::INLINE_CONTACT_LIST>, private SettingsManagerListener
#ifdef _DEBUG
	, virtual NonDerivable<UsersFrame>, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
	
		UsersFrame() : startup(true) { }
		~UsersFrame()
		{
			images.Destroy();
		}
		
		DECLARE_FRAME_WND_CLASS_EX(_T("UsersFrame"), IDR_FAVORITE_USERS, 0, COLOR_3DFACE);
		
		typedef MDITabChildWindowImpl < UsersFrame, RGB(0, 0, 0), IDR_FAVORITE_USERS > baseClass;
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
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_CONNECT, onConnect)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		
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
		
		LRESULT onSetFocus(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlUsers.SetFocus();
			return 0;
		}
		
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
		void on(UserAdded, const FavoriteUser& aUser) noexcept
		{
#ifdef IRAINMAN_USE_NON_RECURSIVE_BEHAVIOR
			PostMessage(WM_CLOSE);
#else
			addUser(aUser);
#endif
		}
		void on(UserRemoved, const FavoriteUser& aUser) noexcept
		{
			removeUser(aUser);
		}
		void on(StatusChanged, const UserPtr& aUser) noexcept
		{
			PostMessage(WM_SPEAKER, (WPARAM)USER_UPDATED, (LPARAM)new UserPtr(aUser)); //-V572
		}
		
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/);
		
		void addUser(const FavoriteUser& aUser);
		void updateUser(const UserPtr& aUser);
		void updateUser(const int i, UserInfo* p_ui, const FavoriteUser& favUser); // [+] IRainman fix.
		void removeUser(const FavoriteUser& aUser);
};

#endif // !defined(USERS_FRAME_H)

/**
 * @file
 * $Id: UsersFrame.h,v 1.28 2006/10/22 18:57:56 bigmuscle Exp $
 */
