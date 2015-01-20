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
	dcassert(s.m_owners.size() == 1);
	dcassert(m_interval >= 10000); // min interval is 10 seconds.
	
	FastLock l(m_cs);
	
	for (auto i = searchQueue.begin(); i != searchQueue.end(); ++i)
	{
		// check dupe
		if (*i == s)
		{
			void* aOwner = *s.m_owners.begin();
			i->m_owners.insert(aOwner);
			
			// if previous search was autosearch and current one isn't, it should be readded before autosearches
			if (!s.isAutoToken() && i->isAutoToken())
			{
				// FlylinkDC Team TODO не стирать,  делать swap или что то ещё, теряем овнеров :( пока не осознал чем это грозит (L.)
				searchQueue.erase(i);
				break;
			}
			
			return false;
		}
	}
	
	if (s.isAutoToken())
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
				if (i->isAutoToken())
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

bool SearchQueue::pop(Search& s, uint64_t p_now)
{
	dcassert(m_interval);
#ifdef _DEBUG
	const auto l_new_now = GET_TICK();
	if (l_new_now != p_now)
	{
		// LogManager::message("l_new_now != now,  l_new_now = " + Util::toString(l_new_now) + " now = " + Util::toString(p_now));
	}
#endif
	if (p_now <= m_lastSearchTime + m_interval)
		return false;
		
	{
		FastLock l(m_cs);
		if (!searchQueue.empty())
		{
			s = searchQueue.front();
			searchQueue.pop_front();
			m_lastSearchTime = p_now;
			return true;
		}
	}
	
	return false;
}

uint64_t SearchQueue::getSearchTime(void* aOwner, uint64_t p_now)
{
	if (aOwner == 0)
		return 0xFFFFFFFF; // for auto searches.
		
	FastLock l(m_cs);
	
#ifdef _DEBUG
	const auto l_new_now = GET_TICK();
	if (l_new_now != p_now)
	{
		// LogManager::message("[2] l_new_now != now,  l_new_now = " + Util::toString(l_new_now) + " now = " + Util::toString(p_now));
	}
#endif
	
	uint64_t x = max(m_lastSearchTime, uint64_t(p_now - m_interval)); // [!] IRainman opt
	
	for (auto i = searchQueue.cbegin(); i != searchQueue.cend(); ++i)
	{
		x += m_interval;
		
		if (i->m_owners.find(aOwner) != i->m_owners.end()) // [!] IRainman opt.
			return x;
		else if (i->m_owners.empty()) // Проверку на пустоту поднять выше?
			break;
	}
	
	return 0;
}

bool SearchQueue::cancelSearch(void* aOwner)
{
	FastLock l(m_cs);
	for (auto i = searchQueue.begin(); i != searchQueue.end(); ++i)
	{
		// [!] IRainman opt.
		auto &l_owners = i->m_owners; // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'i->owners' expression repeatedly. searchqueue.cpp 135
		const auto j = l_owners.find(aOwner);
		if (j != l_owners.end())
		{
			l_owners.erase(j);
			// [~] IRainman opt.
			if (l_owners.empty())
			{
				searchQueue.erase(i);
			}
			return true;
		}
	}
	return false;
}
