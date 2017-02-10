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

#pragma once


#ifndef DCPLUSPLUS_DCPP_FAVORITE_USER_H
#define DCPLUSPLUS_DCPP_FAVORITE_USER_H

#include "User.h"
#include "ResourceManager.h" //[+]IRainman

#define MAXIMAL_LIMIT_KBPS  100 * 1024

class FavoriteUser : public Flags
{
	public:
// !SMT!-S
		enum UPLOAD_LIMIT
		{
			UL_SU  = -2,
			UL_BAN = -1,
			UL_NONE = 0
		};
		
// !SMT!-S
		explicit FavoriteUser(const UserPtr& user_, const string& nick_, const string& hubUrl_, UPLOAD_LIMIT limit_ = UL_NONE) : user(user_), nick(nick_), url(hubUrl_), uploadLimit(limit_), lastSeen(0) { }
		
		explicit FavoriteUser() : uploadLimit(UL_NONE), lastSeen(0) { } // [+] IRainman opt.
		
		enum Flags
		{
			FLAG_NONE = 0, // [+] IRainman fix.
			FLAG_GRANT_SLOT = 1 << 0,
			// [-] FLAG_SUPER_USER = 1 << x, // [+] FlylinkDC++ compatibility mode. [!] IRainman fix: deprecated, please use UPLOAD_LIMIT::UL_SU.
			FLAG_IGNORE_PRIVATE      = 1 << 1,  // !SMT!-S
			FLAG_FREE_PM_ACCESS     = 1 << 2  // !SMT!-S
		};
		
		// !SMT!-S
		static string getSpeedLimitText(int lim)
		{
			switch (lim)
			{
				case UL_SU:
					return STRING(SPEED_SUPER_USER);
				case UL_BAN:
					return ("BAN");
			}
			if (lim > 0)
				return Util::formatBytes(int64_t(lim * 1024)) + '/' + STRING(S);
				
			return Util::emptyString;
		}
		UserPtr& getUser()
		{
			return user;
		}
		void update(const OnlineUser& p_info); // !SMT!-fix
		GETSET(UserPtr, user, User);
		GETSET(string, nick, Nick);
		GETSET(string, url, Url);
		GETSET(time_t, lastSeen, LastSeen);
		GETSET(string, description, Description);
		GETSET(UPLOAD_LIMIT, uploadLimit, UploadLimit); // !SMT!-S
};

#endif // !defined(FAVORITE_USER_H)

/**
 * @file
 * $Id: FavoriteUser.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
