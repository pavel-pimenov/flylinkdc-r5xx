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

#ifndef DCPLUSPLUS_DCPP_HINTEDUSER_H_
#define DCPLUSPLUS_DCPP_HINTEDUSER_H_

#include "User.h"

/** User pointer associated to a hub url */
struct HintedUser
{
	UserPtr user;
	string hint;
	HintedUser(): user(nullptr) {}
	explicit HintedUser(const UserPtr& p_user, const string& p_hint) : user(p_user), hint(p_hint) { }
	
	bool operator==(const UserPtr& rhs) const
	{
		return user == rhs;
	}
	bool operator==(const HintedUser& rhs) const
	{
		return user == rhs.user;
		// ignore the hint, we don't want lists with multiple instances of the same user...
	}
	bool isEQU(const HintedUser& rhs) const
	{
		return hint == rhs.hint && user == rhs.user;
		// ignore the hint, we don't want lists with multiple instances of the same user...
	}
	
	operator UserPtr() const
	{
		return user;
	}
};

#endif /* HINTEDUSER_H_ */
