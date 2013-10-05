/*
 * Copyright (C) 2003-2006 RevConnect, http://www.revconnect.com
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

#include "Thread.h"
#include "typedefs.h"

struct Search
{
	enum SizeModes
	{
		SIZE_DONTCARE = 0x00,
		SIZE_ATLEAST = 0x01,
		SIZE_ATMOST = 0x02,
		SIZE_EXACT = 0x03
	};
	
	SizeModes   sizeMode;
	int64_t   size;
	int32_t   fileType;
	string    query;
	string    token;
	StringList  exts;
	std::unordered_set<void*> owners;
	
	bool operator==(const Search& rhs) const
	{
		return sizeMode == rhs.sizeMode &&
		       size == rhs.size &&
		       fileType == rhs.fileType &&
		       query == rhs.query;
	}
};

class SearchQueue
{
	public:
	
		SearchQueue(uint32_t aInterval = 0)
			: lastSearchTime(0), interval(aInterval)
		{
		}
		
		bool add(const Search& s);
		bool pop(Search& s, uint64_t now); // [!] IRainman opt
		
		void clear()
		{
			Lock l(cs);
			searchQueue.clear();
		}
		
		bool cancelSearch(void* aOwner);
		
		/** return 0 means not in queue */
		uint64_t getSearchTime(void* aOwner, uint64_t now); // [!] IRainman opt
		
		/**
		    by milli-seconds
		    0 means no interval, no auto search and manual search is sent immediately
		*/
		uint32_t interval;
		
	private:
		deque<Search> searchQueue;
		uint64_t lastSearchTime;
		CriticalSection cs;
};
