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


#ifndef DCPLUSPLUS_DCPP_CLIENT_MANAGER_LISTENER_H
#define DCPLUSPLUS_DCPP_CLIENT_MANAGER_LISTENER_H

#include "forward.h"

class Client;
class ClientManagerListener
{
	public:
		virtual ~ClientManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> UserConnected;
		typedef X<1> UserUpdated;
		typedef X<2> UserDisconnected;
		typedef X<3> IncomingSearch;
		typedef X<4> ClientConnected;
		typedef X<5> ClientUpdated;
		typedef X<6> ClientDisconnected;
		
		typedef enum { SEARCH_MISS = 0, SEARCH_PARTIAL_HIT, SEARCH_HIT } SearchReply; // !SMT!-S
		
		/** User online in at least one hub */
		virtual void on(UserConnected, const UserPtr&) noexcept { }
		virtual void on(UserUpdated, const OnlineUserPtr&) noexcept { }
		/** User offline in all hubs */
		virtual void on(UserDisconnected, const UserPtr&) noexcept { }
		virtual void on(IncomingSearch, const string&, const string&, SearchReply) noexcept { } // !SMT!-S
		virtual void on(ClientConnected, const Client*) noexcept { }
		virtual void on(ClientUpdated, const Client*) noexcept { }
		virtual void on(ClientDisconnected, const Client*) noexcept { }
};

#endif // !defined(CLIENT_MANAGER_LISTENER_H)

/**
 * @file
 * $Id: ClientManagerListener.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
