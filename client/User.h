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


#pragma once

#ifndef DCPLUSPLUS_DCPP_USER_H
#define DCPLUSPLUS_DCPP_USER_H

#include <boost/asio/ip/address_v4.hpp>
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"

#include "Pointer.h"
#include "Util.h"
#include "CID.h"
#include "Flags.h"
#include "forward.h"
#include "CFlyProfiler.h"
#include "CFlyUserRatioInfo.h"

class ClientBase;
class Client;

#define TAG(x,y) (x + (y << 8)) // TODO static_assert


/** A user connected to one or more hubs. */
class User : public Flags
{
	public:
		// [!]IRainman This enum is necessary for a comfortable job status flags
		enum Bits
		{
			ONLINE_BIT,
			// [!] IRainman fix.
			// [-] PASSIVE_BIT, deprecated.
			NMDC_FILES_PASSIVE_BIT, // [+]
			NMDC_SEARCH_PASSIVE_BIT, // [+]
			TCP4_BIT = NMDC_FILES_PASSIVE_BIT, // [+]
			UDP4_BIT = NMDC_SEARCH_PASSIVE_BIT, // [+]
			TCP6_BIT, // [+]
			UDP6_BIT, // [+]
			// [~] IRainman fix.
			NMDC_BIT,
#ifdef IRAINMAN_ENABLE_TTH_GET_FLAG
			TTH_GET_BIT, //[+]FlylinkDC
#endif
			TLS_BIT,
			AWAY_BIT, //[+]FlylinkDC
			SERVER_BIT,
			FIREBALL_BIT,
			
			NO_ADC_1_0_PROTOCOL_BIT,
			NO_ADCS_0_10_PROTOCOL_BIT,
			DHT_BIT,
			NAT_BIT,
#ifdef PPA_INCLUDE_IPFILTER
			PG_IPGUARD_BLOCK_BIT,
			PG_IPTRUST_BLOCK_BIT,
			PG_P2PGUARD_BLOCK_BIT,
#endif
			PG_AVDB_BLOCK_BIT,
			CHANGE_IP_BIT,
			MYINFO_BIT,
			OPERATOR_BIT,
			FIRST_INIT_RATIO_BIT,
			LAST_IP_DIRTY_BIT,
			SQL_NOT_FOUND_BIT,
			LAST_BIT // for static_assert (32 bit)
		};
		
		/** Each flag is set if it's true in at least one hub */
		enum UserFlags
		{
			ONLINE = 1 << ONLINE_BIT,
			// [!] IRainman fix.
			// [-] PASSIVE = 1 << PASSIVE_BIT, deprecated
			NMDC_FILES_PASSIVE = 1 << NMDC_FILES_PASSIVE_BIT, // [!]
			NMDC_SEARCH_PASSIVE = 1 << NMDC_SEARCH_PASSIVE_BIT, // [+]
			TCP4 = 1 << TCP4_BIT, // [+]
			UDP4 = 1 << UDP4_BIT, // [+]
			TCP6 = 1 << TCP6_BIT, // [+]
			UDP6 = 1 << UDP6_BIT, // [+]
			// [~] IRainman fix.
			NMDC = 1 << NMDC_BIT,
#ifdef IRAINMAN_ENABLE_TTH_GET_FLAG
			TTH_GET = 1 << TTH_GET_BIT,     //< User supports getting files by tth -> don't have path in queue...
#endif
			TLS = 1 << TLS_BIT,             //< Client supports TLS
			ADCS = TLS,                     //< Client supports TLS
			AWAY = 1 << AWAY_BIT,
			SERVER = 1 << SERVER_BIT,
			FIREBALL = 1 << FIREBALL_BIT,
			
			NO_ADC_1_0_PROTOCOL = 1 << NO_ADC_1_0_PROTOCOL_BIT,
			NO_ADCS_0_10_PROTOCOL = 1 << NO_ADCS_0_10_PROTOCOL_BIT,
			
			DHT0 = 1 << DHT_BIT,
			NAT0 = 1 << NAT_BIT,
			
#ifdef PPA_INCLUDE_IPFILTER
			PG_IPGUARD_BLOCK = 1 << PG_IPGUARD_BLOCK_BIT,
			PG_IPTRUST_BLOCK = 1 << PG_IPTRUST_BLOCK_BIT,
			PG_P2PGUARD_BLOCK = 1 << PG_P2PGUARD_BLOCK_BIT,
#endif
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			PG_AVDB_BLOCK = 1 << PG_AVDB_BLOCK_BIT,
#endif
			CHANGE_IP = 1 << CHANGE_IP_BIT,
			IS_MYINFO = 1 << MYINFO_BIT,
			IS_OPERATOR = 1 << OPERATOR_BIT,
			IS_FIRST_INIT_RATIO = 1 << FIRST_INIT_RATIO_BIT,
			IS_SQL_NOT_FOUND   = 1 << SQL_NOT_FOUND_BIT
		};
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		enum DefinedAutoBanFlags
		{
			BAN_NONE          = 0x00,
			BAN_BY_MIN_SLOT   = 0x01,
			BAN_BY_MAX_SLOT   = 0x02,
			BAN_BY_SHARE      = 0x04, //-V112
			BAN_BY_LIMIT      = 0x08
		};
		
		DefinedAutoBanFlags hasAutoBan(Client *p_Client, const bool p_is_favorite);//[+]FlylinkDC
	private:
		enum SupportSlotsFlag
		{
			FLY_SUPPORT_SLOTS_FIRST = 0x00,
			FLY_SUPPORT_SLOTS = 0x01,
			FLY_NSUPPORT_SLOTS = 0x02
		};
		SupportSlotsFlag m_support_slots;
	public:
#endif // IRAINMAN_ENABLE_AUTO_BAN
		// TODO
		struct Hash
		{
			size_t operator()(const UserPtr& x) const
			{
				size_t cidHash = 0;
				boost::hash_combine(cidHash, x);
				return cidHash;
			}
		};
//		bool operator==(const UserPtr & x) const
//		{
//			return m_cid == x->m_cid;
//		}
//#define ENABLE_DEBUG_LOG_IN_USER_CLASS

		explicit User(const CID& aCID);
		virtual ~User();
		
#ifdef _DEBUG
		static boost::atomic_int g_user_counts; // [!] IRainman fix: Issue 1037 Иногда теряем объект User? https://code.google.com/p/flylinkdc/issues/detail?id=1037
#endif
		
		const CID& getCID() const
		{
			return m_cid;
		}
		void generateNewMyCID()
		{
			return m_cid.regenerate();
		}
		//[+]FlylinkDC
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		const string& getLastNick() const
		{
#ifdef _DEBUG
//			static boost::atomic_int g_call_counts;
//			if (++g_call_counts % 1000 == 0)
//				dcdebug("User::getLastNick() called %d\n", int(++g_call_counts));
#endif
			return m_nick;
		}
		void initLastNick(const string& p_nick)
		{
			if (getLastNick().empty())
			{
				setLastNick(p_nick);
			}
		}
		void setLastNick(const string& p_nick);
		void setIP(const string& p_ip, bool p_is_set_only_ip);
		void setIP(const boost::asio::ip::address_v4& p_ip, bool p_is_set_only_ip);
		uint32_t getHubID() const
		{
			return m_hub_id;
		}
		void setHubID(uint32_t p_hub_id)
		{
			dcassert(p_hub_id);
			m_hub_id = p_hub_id;
		}
		
#else
	public:
		void setIP(const string& ip)
		{
			dcassert(!ip.empty());
			m_ip = ip;
		}
		GETSET(string, m_lastNick, LastNick);
		GETM(string, m_ip, IP);
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
		tstring getLastNickT() const
		{
#ifdef _DEBUG
			//static boost::atomic_int g_call_counts;
			//if (++g_call_counts % 1000 == 0)
			//  dcdebug("User::getLastNickT() called %d\n", g_call_counts); //-V510
#endif
			return Text::toT(getLastNick());
		}
		
		GETSET(int64_t, m_bytesShared, BytesShared); // http://code.google.com/p/flylinkdc/issues/detail?id=1109 нужно для автобана и некоторых других мест
		GETSET(uint32_t, m_limit, Limit);
		GETSET(uint8_t, m_slots, Slots);
		string m_nick;
		CFlyDirtyValue<boost::asio::ip::address_v4> m_last_ip_sql;
		CFlyDirtyValue<uint32_t> m_message_count;
		
		bool isOnline() const
		{
			return isSet(ONLINE);
		}
		bool isNMDC() const
		{
			return isSet(NMDC);
		}
		// [+] IRainman fix.
		bool isServer() const
		{
			return isSet(SERVER);
		}
		bool isFireball() const
		{
			return isSet(FIREBALL);
		}
		bool isAway() const
		{
			return isSet(AWAY);
		}
		// [~] IRainman fix.
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		void fixLastIP();
		void AddRatioUpload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size);
		void AddRatioDownload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size);
		void incMessagesCount();
		bool flushRatio();
		tstring getUDratio();
		tstring getUpload();
		tstring getDownload();
		
		uint64_t getBytesUploadRAW() const
		{
#ifdef FLYLINKDC_USE_RATIO_CS
			CFlyFastLock(m_ratio_cs);
#endif
			if (m_ratio_ptr)
				return m_ratio_ptr->get_upload();
			else
				return 0;
		}
		uint64_t getBytesDownloadRAW() const
		{
#ifdef FLYLINKDC_USE_RATIO_CS
			CFlyFastLock(m_ratio_cs);
#endif
			if (m_ratio_ptr)
				return m_ratio_ptr->get_download();
			else
				return 0;
		}
		boost::asio::ip::address_v4 getIP();
		boost::asio::ip::address_v4 getLastIPfromRAM() const
		{
			return m_last_ip_sql.get();
		}
		string getIPAsString();
		uint64_t getBytesUpload();
		uint64_t getBytesDownload();
		uint64_t getMessageCount();
		void initRatio(bool p_force = false);
		void initMesageCount();
		void initRatioL(const boost::asio::ip::address_v4& p_ip);
#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
	private:
		CID m_cid;
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		CFlyUserRatioInfo* m_ratio_ptr;
		uint32_t  m_hub_id;
		friend struct CFlyUserRatioInfo;
		////static std::unique_ptr<webrtc::RWLockWrapper> g_ratio_cs;
#ifdef FLYLINKDC_USE_RATIO_CS
		mutable FastCriticalSection m_ratio_cs;
#endif
#endif
};

// TODO - для буста это пока не цепляется
// http://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key
namespace std
{
template <>
struct hash<UserPtr>
{
	size_t operator()(const UserPtr & x) const
	{
		return ((size_t)(&(*x))) / sizeof(User);
	}
};
}

#endif // !defined(USER_H)

/**
 * @file
 * $Id: User.h 551 2010-12-18 12:14:16Z bigmuscle $
 */
