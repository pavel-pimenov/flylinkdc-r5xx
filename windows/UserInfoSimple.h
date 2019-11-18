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

#ifndef USERINFOSIMPLE_H
#define USERINFOSIMPLE_H

#pragma once


#include "../client/UserInfo.h"
#include "../client/UploadManager.h"

class UserInfoSimple: public UserInfoBase
{
	public:
		explicit UserInfoSimple(const HintedUser& p_hinted_user) : m_hintedUser(p_hinted_user)
		{
		}
		explicit UserInfoSimple(const UserPtr& p_user, const string& p_hub_hint) : m_hintedUser(HintedUser(p_user, p_hub_hint))
		{
		}
		explicit UserInfoSimple(const OnlineUserPtr& p_user, const string& p_hub_hint) : m_hintedUser(HintedUser(p_user->getUser(), p_hub_hint))
		{
			m_hintedUser.user->setLastNick(p_user->getIdentity().getNick());
		}
		
		void addSummaryMenu();
		static tstring getBroadcastPrivateMessage();
		static uint64_t inputSlotTime();
		
		const UserPtr& getUser() const
		{
			return m_hintedUser.user;
		}
		
		const HintedUser m_hintedUser;
};

#endif