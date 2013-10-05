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

class SearchManager;

class SearchResult : public intrusive_ptr_base<SearchResult>
#ifdef _DEBUG
	, boost::noncopyable
#endif
{
	public:
		enum Types
		{
			TYPE_FILE,
			TYPE_DIRECTORY
		};
		
		SearchResult(Types aType, int64_t aSize, const string& name, const TTHValue& aTTH);
		
		SearchResult(const UserPtr& aUser, Types aType, uint8_t aSlots, uint8_t aFreeSlots,
		             int64_t aSize, const string& aFile, const string& aHubName,
		             const string& aHubURL, const string& ip, const TTHValue& aTTH, const string& aToken);
		             
		string getFileName() const;
		string toSR(const Client& client) const;
		AdcCommand toRES(char type) const;
		
		const UserPtr& getUser() const
		{
			return user;
		}
		string getSlotString() const
		{
			return Util::toString(getFreeSlots()) + '/' + Util::toString(getSlots());
		}
		
		const string& getFile() const
		{
			return file;
		}
		const string& getHubURL() const
		{
			return hubURL;
		}
		const string& getHubName() const
		{
			return hubName;
		}
		int64_t getSize() const
		{
			return size; // 2012-06-09_18-15-11_AED5TO72V7NDNHNH4GNUHSODFQV7M6IMKGVUK6I_413105D2_crash-stack-r501-build-10294.dmp
		}
		Types getType() const
		{
			return type;
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
		// for DHT use only
		void setSlots(size_t p_slots)
		{
			slots = p_slots;
		}
		const string& getIP() const
		{
			return IP;
		}
		const string& getToken() const
		{
			return token;
		}
		
		bool m_is_tth_share;
		bool m_is_tth_download;
		bool m_is_tth_remembrance;
		
		void checkTTH();
		
	private:
		friend class SearchManager;
		
		TTHValue tth;
		
		string file;
		string hubName;
		string hubURL;
		string IP;
		string token;
		
		int64_t size;
		
		size_t slots;
		size_t freeSlots;
		
		UserPtr user;
		Types type;
		bool m_is_tth_check;
};

#endif
