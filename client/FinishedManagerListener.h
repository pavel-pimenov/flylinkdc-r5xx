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

#ifndef DCPLUSPLUS_DCPP_FINISHED_MANAGER_LISTENER_H
#define DCPLUSPLUS_DCPP_FINISHED_MANAGER_LISTENER_H

#include "forward.h"

class FinishedManagerListener
{
	public:
		virtual ~FinishedManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> AddedUl;
		typedef X<1> AddedDl;
		typedef X<2> RemovedUl; // [+] IRainman
		typedef X<3> RemovedDl; // [+] IRainman
		
		virtual void on(AddedDl, const FinishedItem*) noexcept { }
		virtual void on(AddedUl, const FinishedItem*) noexcept { }
		virtual void on(RemovedUl, const FinishedItem*) noexcept { } // [+] IRainman
		virtual void on(RemovedDl, const FinishedItem*) noexcept { } // [+] IRainman
		
};

#endif // !defined(DCPLUSPLUS_DCPP_FINISHED_MANAGER_LISTENER_H)
