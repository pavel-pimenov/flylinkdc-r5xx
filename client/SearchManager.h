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

#ifndef DCPLUSPLUS_DCPP_SEARCH_MANAGER_H
#define DCPLUSPLUS_DCPP_SEARCH_MANAGER_H

#include "CFlyThread.h"
#include "StringSearch.h" // [+] IRainman-S
#include "SearchManagerListener.h"
#include "AdcCommand.h"
#include "ClientManager.h"

class SocketException;

class SearchManager : public Speaker<SearchManagerListener>, public Singleton<SearchManager>, public BASE_THREAD
{
	public:
		static const char* getTypeStr(Search::TypeModes type);
		
		void search_auto(const string& p_tth)
		{
			SearchParamOwner l_search_param;
			l_search_param.m_token = 0; /*"auto"*/
			l_search_param.m_size_mode = Search::SIZE_DONTCARE;
			l_search_param.m_file_type = Search::TYPE_TTH;
			l_search_param.m_size = 0;
			l_search_param.m_filter = p_tth;
			// Для TTH не нужно этого. l_search_param.normalize_whitespace();
			l_search_param.m_owner = nullptr;
			l_search_param.m_is_force_passive = false;
			
			dcassert(p_tth.size() == 39);
			
			//search(l_search_param);
			ClientManager::getInstance()->search(l_search_param);
		}
		
		ClientManagerListener::SearchReply respond(const AdcCommand& cmd, const CID& cid, bool isUdpActive, const string& hubIpPort, StringSearch::List& reguest); // [!] IRainman-S add  StringSearch::List& reguest and return type
		
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
		void disconnect();
		void onSearchResult(const string& aLine)
		{
			onData((const uint8_t*)aLine.data(), aLine.length(), boost::asio::ip::address_v4());
			// TODO - лишнее преобразование  aLine в массив батиков где в onData - создается дубликат string x()
			// а третий параметр всегда пустой - это IP-шник
		}
		
		void onRES(const AdcCommand& cmd, const UserPtr& from, const boost::asio::ip::address_v4& remoteIp);
		void onPSR(const AdcCommand& cmd, UserPtr from, const boost::asio::ip::address_v4& remoteIp);
		void toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo) const;
		
	private:
		class UdpQueue: public BASE_THREAD
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
					m_s.signal();
				}
				void addResult(const string& buf, const boost::asio::ip::address_v4& p_ip4)
				{
					{
						FastLock l(m_cs);
						m_resultList.push_back(make_pair(buf, p_ip4));
					}
					m_s.signal();
				} // Venturi Firewall 2012-04-23_22-28-18_A6JRQEPFW5263A7S7ZOBOAJGFCMET3YJCUYOVCQ_34B61CDE_crash-stack-r501-build-9812.dmp
				
			private:
				FastCriticalSection m_cs; // [!] IRainman opt: use spin lock here.
				Semaphore m_s;
				deque<pair<string, boost::asio::ip::address_v4>> m_resultList;
				volatile bool m_is_stop; // [!] IRainman fix: this variable is volatile.
		} m_queue_thread;
		
		// [-] CriticalSection cs; [-] FlylinkDC++
		unique_ptr<Socket> socket;
		static uint16_t g_search_port;
		volatile bool m_stop; // [!] IRainman fix: this variable is volatile.
		friend class Singleton<SearchManager>;
		
		SearchManager();
		
		int run();
		
		~SearchManager();
		void onData(const uint8_t* buf, size_t aLen, const boost::asio::ip::address_v4& address);
		
		string getPartsString(const PartsInfo& partsInfo) const;
};

#endif // !defined(SEARCH_MANAGER_H)

/**
 * @file
 * $Id: SearchManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
