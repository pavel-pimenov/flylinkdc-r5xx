/*
 * Copyright (C) 2009-2011 Big Muscle, http://strongdc.sf.net
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

#ifndef _DHT_SEARCHMANAGER_H
#define _DHT_SEARCHMANAGER_H

#ifdef STRONG_USE_DHT

#include "KBucket.h"
#include "../client/TimerManager.h"

namespace dht
{

struct Search {

	enum SearchType { TYPE_FILE = 1, TYPE_NODE = 3, TYPE_STOREFILE = 4 };   // standard types should match ADC protocol

	Search(SearchType p_type,const string& p_term, uint32_t p_token) : 
        m_term(p_term),
        m_token(p_token),
        m_type(p_type), 
        partial(false), 
        stopping(false),
        m_filesize(0),
        m_lifeTime(0)
	{
	}
	
	~Search();
	
	
	Node::Map possibleNodes;    // nodes where send search request soon to
	Node::Map triedNodes;       // nodes where search request has already been sent to
	Node::Map respondedNodes;   // nodes who responded to this search request
	
	uint32_t m_token;         // search identificator
	string m_term;            // search term (TTH/CID)
	uint64_t m_lifeTime;      // time when this search has been started //[!] Member variable 'Search::lifeTime' is not initialized in the constructor.
	int64_t  m_filesize;      // file size
	SearchType m_type;        // search type
	bool partial;             // is this partial file search?
	bool stopping;            // search is being stopped
	
	/** Processes this search request */
	void process();
};

class SearchManager :
	public Singleton<SearchManager>
{
	public:
		SearchManager();
		~SearchManager();
		
		/** Performs node lookup in the network */
		void findNode(const CID& cid);
		
		/** Performs value lookup in the network */
		void findFile(const string& p_tth, uint32_t p_token);
		
		/** Performs node lookup to store key/value pair in the network */
		void findStore(const string& tth, int64_t size, bool partial);
		
		/** Process incoming search request */
		void processSearchRequest(const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& cmd);
		
		/** Process incoming search result */
		void processSearchResult(const AdcCommand& cmd);
		
		/** Processes all running searches and removes long-time ones */
		void processSearches();
		
		/** Processes incoming search results */
		bool processSearchResults(const UserPtr& user, size_t slots);
		
	private:
	
		/** Running search operations */
		typedef std::unordered_map<uint32_t, Search*> SearchMap;
		SearchMap m_searches;
		
		/** Locks access to "searches" */
		FastCriticalSection cs; // [!] IRainman: use spin lock here.
		
		typedef std::unordered_multimap<CID, std::pair<uint64_t, SearchResultPtr>> ResultsMap;
		ResultsMap searchResults;
		
		/** Performs general search operation in the network */
		void search(Search& s);
		
		/** Sends publishing request */
		void publishFile(const Node::Map& nodes, const string& tth, int64_t size, bool partial);
		
		/** Checks whether we are alreading searching for a term */
		bool isAlreadySearchingFor(const string& term);
		
		uint64_t m_lastTimeSearchFile;
		
};

}

#endif // STRONG_USE_DHT

#endif  // _DHT_SEARCHMANAGER_H