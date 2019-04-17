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

#ifndef DCPLUSPLUS_DCPP_SEARCH_MANAGER_H
#define DCPLUSPLUS_DCPP_SEARCH_MANAGER_H

#include "CFlyThread.h"
#include "StringSearch.h" // [+] IRainman
#include "SearchManagerListener.h"
#include "AdcCommand.h"
#include "ClientManager.h"

class SearchManager : public Speaker<SearchManagerListener>, public Singleton<SearchManager>, public Thread
{
	public:
		static const char* getTypeStr(Search::TypeModes type);
		
		void search_auto(const string& p_tth);
		
		ClientManagerListener::SearchReply respond(const AdcCommand& cmd, const CID& cid, bool isUdpActive, const string& hubIpPort, StringSearch::List& reguest); // [!] IRainman add  StringSearch::List& reguest and return type
		
		static bool isSearchPortValid()
		{
			return g_search_port != 0;
		}
		static uint16_t getSearchPortUint()
		{
			dcassert(g_search_port);
			return g_search_port;
		}
		static string getSearchPort()
		{
			return Util::toString(getSearchPortUint());
		}
		
		void listen();
		static void runTestUDPPort();
		
		void disconnect();
		void onSearchResult(const string& aLine)
		{
			onData(aLine);
		}
		
		void onRES(const AdcCommand& cmd, const UserPtr& from, const boost::asio::ip::address_v4& remoteIp);
		void onPSR(const AdcCommand& cmd, UserPtr from, const boost::asio::ip::address_v4& remoteIp);
		static void toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo);
		
	private:
		class UdpQueue: public Thread
		{
			public:
				UdpQueue() : m_is_stop(false) {}
				~UdpQueue()
				{
					shutdown();
				}
				
				int run();
				void shutdown()
				{
					m_is_stop = true;
                    m_resultList.clear();
					m_search_semaphore.signal();
				}
				void addResult(const string& buf, const boost::asio::ip::address_v4& p_ip4)
				{
                    if(m_is_stop == false)
					{
						CFlyFastLock(m_cs);
						m_resultList.push_back(make_pair(buf, p_ip4));
					}
					m_search_semaphore.signal();
				}
				
			private:
				FastCriticalSection m_cs;
				Semaphore m_search_semaphore;
				deque<pair<string, boost::asio::ip::address_v4>> m_resultList;
				volatile bool m_is_stop;
		} m_queue_thread;
		
		unique_ptr<Socket> socket;
		static uint16_t g_search_port;
		volatile bool m_stop;
		friend class Singleton<SearchManager>;
		
		SearchManager();
		
		int run();
		
		~SearchManager();
		void onData(const uint8_t* buf, size_t aLen, const boost::asio::ip::address_v4& address);
		void onData(const std::string& p_line);
		
		static string getPartsString(const PartsInfo& partsInfo);
};

#endif // !defined(SEARCH_MANAGER_H)

/**
 * @file
 * $Id: SearchManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
