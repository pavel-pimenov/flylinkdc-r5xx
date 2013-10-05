/*
 * Copyright (C) 2003-2013 RevConnect, http://www.revconnect.com
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

#include "stdinc.h"
#include "SearchQueue.h"
#include "QueueManager.h"
#include "SearchManager.h"

bool SearchQueue::add(const Search& s)
{
	dcassert(s.owners.size() == 1);
	dcassert(interval >= 10000); // min interval is 10 seconds.
	
	Lock l(cs);
	
	for (auto i = searchQueue.begin(); i != searchQueue.end(); ++i)
	{
		// check dupe
		if (*i == s)
		{
			void* aOwner = *s.owners.begin();
			i->owners.insert(aOwner);
			
			// if previous search was autosearch and current one isn't, it should be readded before autosearches
			if (s.token != "auto" && i->token == "auto")
			{
				// FlylinkDC Team TODO не стирать,  делать swap или что то ещё, теряем овнеров :( пока не осознал чем это грозит (L.)
				searchQueue.erase(i);
				break;
			}
			
			return false;
		}
	}
	
	if (s.token == "auto")
	{
		// Insert last (automatic search)
		searchQueue.push_back(s);
	}
	else
	{
		bool added = false;
		if (searchQueue.empty())
		{
			searchQueue.push_front(s);
			added = true;
		}
		else
		{
			// Insert before the automatic searches (manual search)
			for (auto i = searchQueue.cbegin(); i != searchQueue.cend(); ++i)
			{
				if (i->token == "auto")
				{
					searchQueue.insert(i, s);
					added = true;
					break;
				}
			}
		}
		if (!added)
		{
			searchQueue.push_back(s);
		}
	}
	return true;
}

bool SearchQueue::pop(Search& s, uint64_t now)
{
	dcassert(interval);
	
	//uint64_t now = GET_TICK(); // [-] IRainman opt
	if (now <= lastSearchTime + interval)
		return false;
		
	{
		Lock l(cs);
		if (!searchQueue.empty())
		{
			s = searchQueue.front();
			searchQueue.pop_front();
			lastSearchTime = now;
			return true;
		}
	}
	
	return false;
}

uint64_t SearchQueue::getSearchTime(void* aOwner, uint64_t now)
{
	if (aOwner == 0)
		return 0xFFFFFFFF; // for auto searches.
		
	Lock l(cs);
	
	uint64_t x = max(lastSearchTime, uint64_t(/*GET_TICK()*/now - interval)); // [!] IRainman opt
	
	for (auto i = searchQueue.cbegin(); i != searchQueue.cend(); ++i)
	{
		x += interval;
		
		if (i->owners.find(aOwner) != i->owners.end()) // [!] IRainman opt.
			return x;
		else if (i->owners.empty())
			break;
	}
	
	return 0;
}

bool SearchQueue::cancelSearch(void* aOwner)
{
	Lock l(cs);
	for (auto i = searchQueue.begin(); i != searchQueue.end(); ++i)
	{
		// [!] IRainman opt.
		auto &l_owners = i->owners; // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'i->owners' expression repeatedly. searchqueue.cpp 135
		const auto j = l_owners.find(aOwner);
		if (j != l_owners.end())
		{
			l_owners.erase(j);
			// [~] IRainman opt.
			if (l_owners.empty())
				searchQueue.erase(i);
			return true;
		}
	}
	return false;
}
