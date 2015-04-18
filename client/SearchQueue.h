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

#include "CFlyThread.h"
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
	enum TypeModes
	{
		TYPE_ANY = 0,
		TYPE_AUDIO,
		TYPE_COMPRESSED,
		TYPE_DOCUMENT,
		TYPE_EXECUTABLE,
		TYPE_PICTURE,
		TYPE_VIDEO,
		TYPE_DIRECTORY,
		TYPE_TTH,
		TYPE_CD_IMAGE, //[+] от flylinkdc++
		TYPE_LAST
	};
	Search() : m_is_force_passive(false), m_sizeMode(SIZE_DONTCARE), m_size(0), m_fileTypes_bitmap(0), m_token(0)
	{
	}
	bool      m_is_force_passive;
	SizeModes m_sizeMode;
	int64_t   m_size;
	uint16_t  m_fileTypes_bitmap;
	string    m_query;
	uint32_t  m_token;
	StringList  m_ext_list;
	std::unordered_set<void*> m_owners; // boost - падает.
	bool isAutoToken() const
	{
		return m_token == 0; /*"auto"*/
	}
	bool operator==(const Search& rhs) const
	{
		BOOST_STATIC_ASSERT(TYPE_LAST < 16); // Иначе не влезет в m_fileTypes_bitmap
		return m_sizeMode == rhs.m_sizeMode &&
		       m_size == rhs.m_size &&
		       m_fileTypes_bitmap == rhs.m_fileTypes_bitmap &&
		       m_query == rhs.m_query;
	}
};

class SearchParamBase
{
	public:
		Search::SizeModes m_size_mode;
		int64_t m_size;
		uint8_t m_max_results;
		bool m_is_passive;
		Search::TypeModes m_file_type;
		string m_filter;
		Client* m_client;
		SearchParamBase() : m_size(0), m_size_mode(Search::SIZE_DONTCARE), m_file_type(Search::TYPE_ANY), m_max_results(0), m_is_passive(false), m_client(nullptr)
		{
		}
		void normalize_whitespace()
		{
			string::size_type found = 0;
			while ((found = m_filter.find_first_of("\t\n\r", found)) != string::npos)
			{
				m_filter[found] = ' ';
				found++;
			}
		}
		void init(Client* p_client, bool p_is_passive)
		{
			m_client = p_client;
			m_is_passive  = p_is_passive;
			m_max_results = p_is_passive ? 5 : 10;
		}
};
class SearchParam : public SearchParamBase
{
	public:
		string m_raw_search;
		string m_seeker;
		string::size_type m_query_pos;
		char m_error_level;
		SearchParam(): m_query_pos(string::npos), m_error_level(0)
		{
		}
		bool is_parse_nmdc_search(const string& p_raw_search);
		string getRAWQuery() const
		{
			dcassert(m_query_pos != string::npos);
			if (m_query_pos != string::npos)
				return m_raw_search.substr(m_query_pos);
			else
				return "";
		}
};

class SearchParamTokenClass
{
	public:
		uint32_t    m_token;
		bool        m_is_force_passive;
		void*       m_owner;
		StringList  m_ext_list;
		SearchParamTokenClass() : m_token(0), m_is_force_passive(false), m_owner(nullptr)
		{
		}
};

class SearchParamToken : public SearchParamBase, public SearchParamTokenClass // TODO - убрать множественное наследование.
{
};
class SearchParamOwner : public SearchParamBase, public SearchParamTokenClass
{
	public:
		SearchParamOwner()
		{
		}
};
class SearchParamTokenMultiClient : public SearchParamToken
{
	public:
		StringList  m_clients;
		void check_clients(unsigned p_count_item)
		{
			if (!m_clients.empty() && m_clients.size() == p_count_item && m_clients[0].empty() || m_clients.size() == 1 && m_clients[0].empty())
			{
				m_clients.clear();
			}
		}
};

class SearchQueue
{
	public:
	
		SearchQueue()
			: m_lastSearchTime(0), m_interval(0)
		{
		}
		
		bool add(const Search& s);
		bool pop(Search& s, uint64_t now); // [!] IRainman opt
		bool empty()
		{
			FastLock l(m_cs);
			return m_searchQueue.empty();
		}
		void clear()
		{
			FastLock l(m_cs);
			m_searchQueue.clear();
		}
		
		bool cancelSearch(void* aOwner);
		
		/** return 0 means not in queue */
		uint64_t getSearchTime(void* aOwner, uint64_t now); // [!] IRainman opt
		
		/**
		    by milli-seconds
		    0 means no interval, no auto search and manual search is sent immediately
		*/
		uint32_t m_interval;
		
	private:
		deque<Search> m_searchQueue;
		uint64_t m_lastSearchTime;
		FastCriticalSection m_cs;
};
