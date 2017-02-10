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


#ifndef DCPLUSPLUS_DCPP_FAVORITEMANAGERLISTENER_H_
#define DCPLUSPLUS_DCPP_FAVORITEMANAGERLISTENER_H_

#include "forward.h"
#include "noexcept.h"

class RecentHubEntry;

class FavoriteManagerListener
{
	public:
		virtual ~FavoriteManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<3> FavoriteAdded;
		typedef X<4> FavoriteRemoved;
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
#ifdef UPDATE_CON_STATUS_ON_FAV_HUBS_IN_REALTIME
		typedef X<5> FavoriteStatusChanged;
#endif
#endif
		typedef X<6> UserAdded;
		typedef X<7> UserRemoved;
		typedef X<8> StatusChanged;
		
		typedef X<11> RecentAdded;
		typedef X<12> RecentRemoved;
		typedef X<13> RecentUpdated;
		
		virtual void on(FavoriteAdded, const FavoriteHubEntry*) noexcept { }
		virtual void on(FavoriteRemoved, const FavoriteHubEntry*) noexcept { }
		
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
#ifdef UPDATE_CON_STATUS_ON_FAV_HUBS_IN_REALTIME
		virtual void on(FavoriteStatusChanged, const FavoriteHubEntry*) noexcept { }
#endif
#endif
		virtual void on(UserAdded, const FavoriteUser&) noexcept { }
		virtual void on(UserRemoved, const FavoriteUser&) noexcept { }
		virtual void on(StatusChanged, const UserPtr&) noexcept { }
		virtual void on(RecentAdded, const RecentHubEntry*) noexcept { }
		virtual void on(RecentRemoved, const RecentHubEntry*) noexcept { }
		virtual void on(RecentUpdated, const RecentHubEntry*) noexcept { }
};

#endif
