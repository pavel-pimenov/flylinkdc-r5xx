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

#ifndef DCPLUSPLUS_DCPP_SEARCHRESULT_H
#define DCPLUSPLUS_DCPP_SEARCHRESULT_H

#include "forward.h"
#include "AdcCommand.h"
#include "Pointer.h"

class CFlySearchItem
{
	public:
		TTHValue m_tth;
		std::string m_search;
		std::string * m_toSRCommand;
		CFlySearchItem(const TTHValue& p_tth, const std::string& p_search):
			m_tth(p_tth),
			m_search(p_search),
			m_toSRCommand(nullptr)
		{
			dcassert(m_search.size() > 4);
		}
		
};
typedef std::vector<CFlySearchItem> CFlySearchArray;

class SearchManager;
class SearchResultBaseTTH
{
	public:
		enum Types
		{
			TYPE_FILE,
			TYPE_DIRECTORY
		};
		
		SearchResultBaseTTH(Types aType, int64_t aSize, const string& aName, const TTHValue& aTTH, uint8_t aSlots = 0, uint8_t aFreeSlots = 0);
		virtual ~SearchResultBaseTTH() {}
		void initSlot();
		string toSR(const Client& c) const;
		void toRES(AdcCommand& cmd, char type) const;
		const string& getFile() const
		{
			return file;
		}
		int64_t getSize() const
		{
			return size;
		}
		size_t getSlots() const
		{
			return slots;
		}
		size_t getFreeSlots() const
		{
			return freeSlots;
		}
		const TTHValue& getTTH() const
		{
			return tth;
		}
		Types getType() const
		{
			return m_type;
		}
		// for DHT use only
		void setSlots(size_t p_slots)
		{
			slots = p_slots;
		}
	protected:
		TTHValue tth;
		string file;
		int64_t size;
		size_t slots;
		size_t freeSlots;
		Types m_type;
};

class SearchResult : public SearchResultBaseTTH, public intrusive_ptr_base<SearchResult>
#ifdef _DEBUG
	, boost::noncopyable
#endif
{
	public:
	
		SearchResult(Types aType, int64_t aSize, const string& name, const TTHValue& aTTH);
		
		SearchResult(const UserPtr& aUser, Types aType, uint8_t aSlots, uint8_t aFreeSlots,
		             int64_t aSize, const string& aFile, const string& aHubName,
		             const string& aHubURL, const string& ip, const TTHValue& aTTH, uint32_t aToken);
		             
		string getFileName() const;
		
		const UserPtr& getUser() const
		{
			return user;
		}
		string getSlotString() const
		{
			return Util::toString(getFreeSlots()) + '/' + Util::toString(getSlots());
		}
		const string& getHubURL() const
		{
			return hubURL;
		}
		const string& getHubName() const
		{
			return m_hubName;
		}
		void calcHubName();
		
		const string& getIP() const
		{
			return IP;
		}
		const uint32_t getToken() const
		{
			return m_token;
		}
		
		bool m_is_tth_share;
		bool m_is_tth_download;
		bool m_is_tth_remembrance;
		
		void checkTTH();
		
	private:
		friend class SearchManager;
		
		string m_hubName;
		string hubURL;
		string IP;
		uint32_t m_token;
		UserPtr user;
		bool m_is_tth_check;
};

#endif
