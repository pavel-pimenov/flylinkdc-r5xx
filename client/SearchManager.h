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
		static const char* getTypeStr(int type);
		
		void search_auto(const string& aName)
		{
			search(aName, 0, Search::TYPE_TTH, Search::SIZE_DONTCARE, 0 /*"auto"*/, nullptr, false);
		}
		void search(const string& aName, int64_t aSize, Search::TypeModes aTypeMode, Search::SizeModes aSizeMode, uint32_t aToken, void* aOwner, bool p_is_force_passive);
		
		uint64_t search(const StringList& who,
		                const string& aName,
		                int64_t aSize,
		                Search::TypeModes aTypeMode,
		                Search::SizeModes aSizeMode,
		                uint32_t aToken,
		                const StringList& aExtList,
		                void* aOwner,
		                bool p_is_force_passive);
		                
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
			onData((const uint8_t*)aLine.data(), aLine.length(), Util::emptyString);
			// TODO - лишнее преобразование  aLine в массив батиков где в onData - создается дубликат string x()
			// а третий параметр всегда пустой - это IP-шник
		}
		
		void onRES(const AdcCommand& cmd, const UserPtr& from, const string& remoteIp = Util::emptyString);
		void onPSR(const AdcCommand& cmd, UserPtr from, const string& remoteIp = Util::emptyString);
		void toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo) const;
		
	private:
		class UdpQueue: public BASE_THREAD
		{
			public:
				UdpQueue() : stop(false) {}
				~UdpQueue()
				{
					shutdown();
				}
				
				int run();
				void shutdown()
				{
					stop = true;
					m_s.signal();
				}
				void addResult(const string& buf, const string& ip)
				{
					{
						FastLock l(cs);
						resultList.push_back(make_pair(buf, ip));
					}
					m_s.signal();
				} // Venturi Firewall 2012-04-23_22-28-18_A6JRQEPFW5263A7S7ZOBOAJGFCMET3YJCUYOVCQ_34B61CDE_crash-stack-r501-build-9812.dmp
				
			private:
				FastCriticalSection cs; // [!] IRainman opt: use spin lock here.
				Semaphore m_s;
				
				deque<pair<string, string>> resultList;
				
				volatile bool stop; // [!] IRainman fix: this variable is volatile.
		} m_queue_thread;
		
		// [-] CriticalSection cs; [-] FlylinkDC++
		unique_ptr<Socket> socket;
		static uint16_t g_search_port;
		volatile bool m_stop; // [!] IRainman fix: this variable is volatile.
		friend class Singleton<SearchManager>;
		
		SearchManager();
		
		static std::string normalizeWhitespace(const std::string& aString);
		int run();
		
		~SearchManager();
		void onData(const uint8_t* buf, size_t aLen, const string& address);
		
		string getPartsString(const PartsInfo& partsInfo) const;
};

#endif // !defined(SEARCH_MANAGER_H)

/**
 * @file
 * $Id: SearchManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
