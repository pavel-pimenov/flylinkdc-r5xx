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


#ifndef DCPLUSPLUS_DCPP_FAVHUBGROUP_H
#define DCPLUSPLUS_DCPP_FAVHUBGROUP_H

struct FavHubGroupProperties
{
	/**
	* Designates a private group; hubs in a private group don't share their users with any other
	* hub when trying to match an online user, and are not shared with any peer.
	*/
	bool priv;
};

typedef std::unordered_map<string, FavHubGroupProperties> FavHubGroups;
typedef FavHubGroups::value_type FavHubGroup;

#endif
