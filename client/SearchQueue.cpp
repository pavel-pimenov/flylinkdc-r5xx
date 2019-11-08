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
#include "NmdcHub.h"

bool SearchQueue::add(const Search& s)
{
	dcassert(s.m_owners.size() == 1);
	dcassert(m_interval >= 2000);
	
	CFlyFastLock(m_cs);
	
	for (auto i = m_searchQueue.begin(); i != m_searchQueue.end(); ++i)
	{
		// check dupe
		if (*i == s)
		{
			void* aOwner = *s.m_owners.begin();
			i->m_owners.insert(aOwner);
			
			// if previous search was autosearch and current one isn't, it should be readded before autosearches
			if (!s.isAutoToken() && i->isAutoToken())
			{
				// FlylinkDC Team TODO не стирать,  делать swap или что то ещЄ, тер€ем овнеров :( пока не осознал чем это грозит (L.)
				m_searchQueue.erase(i);
				break;
			}
			
			return false;
		}
	}
	
	if (s.isAutoToken())
	{
		// Insert last (automatic search)
		m_searchQueue.push_back(s);
	}
	else
	{
		bool added = false;
		if (m_searchQueue.empty())
		{
			m_searchQueue.push_front(s);
			added = true;
		}
		else
		{
			// Insert before the automatic searches (manual search)
			for (auto i = m_searchQueue.cbegin(); i != m_searchQueue.cend(); ++i)
			{
				if (i->isAutoToken())
				{
					m_searchQueue.insert(i, s);
					added = true;
					break;
				}
			}
		}
		if (!added)
		{
			m_searchQueue.push_back(s);
		}
	}
	return true;
}

bool SearchQueue::empty()
{
	// CFlyFastLock(m_cs);
	// “ут лок не критичны и зоветс€ не часто
	return m_searchQueue.empty();
}
void SearchQueue::clear()
{
	CFlyFastLock(m_cs);
	m_searchQueue.clear();
}

bool SearchQueue::pop(Search& s, uint64_t p_now, bool p_is_passive)
{
	dcassert(m_interval);
#ifdef _DEBUG
	const auto l_new_now = GET_TICK();
	if (l_new_now != p_now)
	{
		// LogManager::message("l_new_now != now,  l_new_now = " + Util::toString(l_new_now) + " now = " + Util::toString(p_now));
	}
#endif
	if (p_now <= m_lastSearchTime + (p_is_passive ? m_interval_passive : m_interval))
	{
		return false;
	}
	
	{
		CFlyFastLock(m_cs);
		if (!m_searchQueue.empty())
		{
			s = m_searchQueue.front();
			m_searchQueue.pop_front();
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
		
	CFlyFastLock(m_cs);
	
#ifdef _DEBUG
	const auto l_new_now = GET_TICK();
	if (l_new_now != p_now)
	{
		// LogManager::message("[2] l_new_now != now,  l_new_now = " + Util::toString(l_new_now) + " now = " + Util::toString(p_now));
	}
#endif
	
	uint64_t x = std::max(m_lastSearchTime, uint64_t(p_now - m_interval)); // [!] IRainman opt
	
	for (auto i = m_searchQueue.cbegin(); i != m_searchQueue.cend(); ++i)
	{
		x += m_interval;
		
		if (i->m_owners.find(aOwner) != i->m_owners.end()) // [!] IRainman opt.
			return x;
		else if (i->m_owners.empty()) // ѕроверку на пустоту подн€ть выше?
			break;
	}
	
	return 0;
}

bool SearchQueue::cancelSearch(void* aOwner)
{
	CFlyFastLock(m_cs);
	for (auto i = m_searchQueue.begin(); i != m_searchQueue.end(); ++i)
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
				m_searchQueue.erase(i);
			}
			return true;
		}
	}
	return false;
}

bool SearchParam::is_parse_nmdc_search(const string& p_raw_search)
{
	m_raw_search = p_raw_search;
	dcassert(m_raw_search.size() > 4);
	if (m_raw_search.size() < 4)
	{
		m_error_level = 1;
		return false;
	}
	m_is_passive = m_raw_search.compare(0, 4, "Hub:", 4) == 0;
	const string param = Text::toUtf8(m_raw_search);
	m_raw_search = param;
	string::size_type i = 0;
	string::size_type j = param.find(' ', i);
	m_query_pos = j;
	if (j == string::npos || i == j)
	{
		m_error_level = 1;
		return false;
	}
	m_seeker = param.substr(i, j - i);
#ifdef FLYLINKDC_USE_COLLECT_STAT
	string l_tth;
	const auto l_tth_pos = param.find("?9?TTH:", i);
	if (l_tth_pos != string::npos)
		l_tth = param.c_str() + l_tth_pos + 7;
	if (!l_tth.empty())
		CFlylinkDBManager::getInstance()->push_event_statistic(p_is_passive ? "search-p" : "search-a", "TTH", param, getIpAsString(), "", getHubUrlAndIP(), l_tth);
	else
		CFlylinkDBManager::getInstance()->push_event_statistic(p_is_passive ? "search-p" : "search-a", "Others", param, getIpAsString(), "", getHubUrlAndIP());
#endif
	i = j + 1;
	if (param.size() < (i + 4))
	{
		m_error_level = 1;
		return false;
	}
	if (param[i] == 'F')
	{
		m_size_mode = Search::SIZE_DONTCARE;
	}
	else if (param[i + 2] == 'F')
	{
		m_size_mode = Search::SIZE_ATLEAST;
	}
	else
	{
		m_size_mode = Search::SIZE_ATMOST;
	}
	i += 4;
	j = param.find('?', i);
	if (j == string::npos || i == j)
	{
		m_error_level = 4;
		return false;
	}
	if ((j - i) == 1 && param[i] == '0')
	{
		m_size = 0;
	}
	else
	{
		m_size = _atoi64(param.c_str() + i);
	}
	i = j + 1;
	j = param.find('?', i);
	if (j == string::npos || i == j)
	{
		m_error_level = 5;
		return false;
	}
	const int l_type_search = atoi(param.c_str() + i);
	m_file_type = Search::TypeModes(l_type_search - 1);
	i = j + 1;
	
	if (m_file_type == Search::TYPE_TTH && (param.size() - i) == 39 + 4) // 39+4 = strlen("TTH:VGUKIR6NLP6LQB7P5NDCZGUSR3MFHRMRO3VJLWY")
	{
		m_filter = param.substr(i);
	}
	else
	{
		m_filter = NmdcHub::unescape(param.substr(i));
	}
	if (m_filter.empty())
	{
		m_error_level = 6;
		return false;
	}
	return true;
}
