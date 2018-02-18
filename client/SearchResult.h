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


#ifndef DCPLUSPLUS_DCPP_SEARCHRESULT_H
#define DCPLUSPLUS_DCPP_SEARCHRESULT_H

#include "forward.h"
#include "Pointer.h"
#include "SearchQueue.h"
#include "HintedUser.h"
#include <boost/asio/ip/address_v4.hpp>

class AdcCommand;
class SearchManager;
class SearchResultBaseTTH
{
	public:
		enum Types
		{
			TYPE_FILE,
			TYPE_DIRECTORY,
			TYPE_TORRENT_MAGNET
		};
		SearchResultBaseTTH():
			m_size(0),
			m_slots(0),
			m_freeSlots(0),
			m_type(TYPE_FILE)
		{
		}
		
		SearchResultBaseTTH(Types aType, int64_t aSize, const string& aName, const TTHValue& aTTH, uint8_t aSlots = 0, uint8_t aFreeSlots = 0);
		virtual ~SearchResultBaseTTH() {}
		void initSlot();
		string toSR(const Client& c) const;
		void toRES(AdcCommand& cmd, char type) const;
		const string& getFile() const
		{
			return m_file;
		}
		void setFile(const string& p_file)
		{
			m_file = p_file;
		}
		const string getSHA1() const
		{
			const auto l_pos = m_torrent_magnet.find("xt=urn:btih:");
			if (l_pos != string::npos)
			{
				return m_torrent_magnet.substr(l_pos + 12, 40);
			}
			return "";
		}
		const string& getTorrentMagnet() const
		{
			return m_torrent_magnet;
		}
		void setTorrentMagnet(const string& p_torrent_magnet)
		{
			m_torrent_magnet = p_torrent_magnet;
			m_type = TYPE_TORRENT_MAGNET;
		}
		int64_t getSize() const
		{
			return m_size;
		}
		uint8_t getSlots() const
		{
			return m_slots;
		}
		uint8_t getFreeSlots() const
		{
			return m_freeSlots;
		}
		const TTHValue& getTTH() const
		{
			return m_tth;
		}
		Types getType() const
		{
			return m_type;
		}
		// for DHT use only
		void setSlots(uint8_t p_slots)
		{
			m_slots = p_slots;
		}
		string getSlotString() const
		{
			return Util::toString(getFreeSlots()) + '/' + Util::toString(getSlots());
		}
		string getPeersString() const
		{
			return Util::toString(m_peer) + '/' + Util::toString(m_seed);
		}
		// TODO унести свойства торрента в отдельный класс
		uint16_t m_peer = 0;
		uint16_t m_seed = 0;
		uint16_t m_group_index = 0;
		string   m_group_name;
		uint16_t m_comment = 0;
		string m_url;
		string m_tracker;
		uint16_t m_tracker_index = 0;
		string m_date;
		int64_t m_size;
	protected:
		TTHValue m_tth;
		string m_file;
		string m_torrent_magnet;
		uint8_t m_slots;
		uint8_t m_freeSlots;
		Types m_type;
};

class SearchResultCore : public SearchResultBaseTTH
#ifdef _DEBUG
//    , boost::noncopyable
#endif

{
	public:
		SearchResultCore()
		{
		}
		SearchResultCore(Types aType, int64_t aSize, const string& aName, const TTHValue& aTTH, uint8_t aSlots = 0, uint8_t aFreeSlots = 0) :
			SearchResultBaseTTH(aType, aSize, aName, aTTH, aSlots, aFreeSlots)
		{
		
		}
};

class SearchResult : public SearchResultCore
#ifdef _DEBUG
//, boost::noncopyable
#endif
{
	public:
		SearchResult() :
			m_is_tth_share(false),
			m_is_tth_download(false),
			m_is_virus(false),
			m_is_tth_remembrance(false),
			m_token(uint32_t (-1)),
			m_is_tth_check(false),
			m_is_p2p_guard_calc(false),
			m_virus_level(0),
			m_is_tth_queue(false)
		{
		}
		SearchResult(Types aType, int64_t aSize, const string& aFile, const TTHValue& aTTH, uint32_t aToken);
		
		SearchResult(const UserPtr& aUser, Types aType, uint8_t aSlots, uint8_t aFreeSlots,
		             int64_t aSize, const string& aFile, const string& aHubName,
		             const string& aHubURL, const boost::asio::ip::address_v4& aIP4, const TTHValue& aTTH, uint32_t aToken);
		             
		string getFileName() const;
		string getFilePath() const;
		
		const UserPtr& getUser() const
		{
			return m_user;
		}
		HintedUser getHintedUser() const
		{
			return HintedUser(getUser(), getHubUrl());
		}
		const string& getHubUrl() const
		{
			return m_hubURL;
		}
		const string& getHubName() const
		{
			return m_hubName;
		}
		void calcHubName();
		
		string getIPAsString() const
		{
			if (!m_search_ip4.is_unspecified())
				return m_search_ip4.to_string();
			else
				return Util::emptyString;
		}
		const boost::asio::ip::address_v4& getIP() const
		{
			return m_search_ip4;
		}
		const uint32_t getToken() const
		{
			return m_token;
		}
		
		bool m_is_tth_share;
		bool m_is_tth_download;
		bool m_is_virus;
		bool m_is_tth_remembrance;
		bool m_is_tth_queue;
		unsigned m_torrent_page = 0;
		
		mutable uint8_t m_virus_level;
		const string& getP2PGuard() const
		{
			return m_p2p_guard_text;
		}
		void checkTTH();
		void calcP2PGuard();
	private:
		friend class SearchManager;
		
		string m_hubName;
		string m_hubURL;
		string m_p2p_guard_text;
		boost::asio::ip::address_v4 m_search_ip4;
		uint32_t m_token;
		UserPtr m_user;
		bool m_is_tth_check;
		bool m_is_p2p_guard_calc;
};

#endif
