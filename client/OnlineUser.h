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

#ifndef DCPLUSPLUS_DCPP_ONLINEUSER_H_
#define DCPLUSPLUS_DCPP_ONLINEUSER_H_

#pragma once

#include "UserInfoBase.h"
#include "UserInfoColumns.h"

class ClientBase;
class NmdcHub;
class AdcHub;

extern bool setAdcUserFlags(const UserPtr& user, const string& feat);

/** One of possibly many identities of a user, mainly for UI purposes */
class Identity
#ifdef _DEBUG
#ifdef IRAINMAN_IDENTITY_IS_NON_COPYABLE
	: boost::noncopyable
#endif
#endif
{
	public:
		enum
		{
			CHANGES_NICK = 1 << COLUMN_NICK, // done
			CHANGES_SHARED = 1 << COLUMN_SHARED, // done
			CHANGES_EXACT_SHARED = 1 << COLUMN_EXACT_SHARED, // done
			CHANGES_DESCRIPTION = 1 << COLUMN_DESCRIPTION, // done
			CHANGES_APPLICATION = 1 << COLUMN_APPLICATION,
			CHANGES_CONNECTION =
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
			    1 << COLUMN_CONNECTION, // done
#else
			    0, // done
#endif
			CHANGES_EMAIL = 1 << COLUMN_EMAIL, // done
			CHANGES_VERSION =
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
			    1 << COLUMN_VERSION,
#else
			    0,
#endif
			CHANGES_MODE =
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
			    1 << COLUMN_MODE,
#else
			    0,
#endif
			CHANGES_HUBS = 1 << COLUMN_HUBS, // done
			CHANGES_SLOTS = 1 << COLUMN_SLOTS, // done
			CHANGES_UPLOAD_SPEED =
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
			    1 << COLUMN_UPLOAD_SPEED,
#else
			    0,
#endif
			CHANGES_IP = 1 << COLUMN_IP, // done
			CHANGES_GEO_LOCATION = 1 << COLUMN_GEO_LOCATION, // done
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
			CHANGES_UPLOAD = 1 << COLUMN_UPLOAD,
			CHANGES_DOWNLOAD = 1 << COLUMN_DOWNLOAD,
			CHANGES_MESSAGES = 1 << COLUMN_MESSAGES,
#endif
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			CHANGES_ANTIVIRUS = 1 << COLUMN_ANTIVIRUS,
#endif
			CHANGES_P2P_GUARD = 1 << COLUMN_P2P_GUARD,
#ifdef FLYLINKDC_USE_DNS
			CHANGES_DNS = 1 << COLUMN_DNS,
#endif
			CHANGES_CID = 1 << COLUMN_CID,
			CHANGES_TAG = 1 << COLUMN_TAG
		};
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		enum VirusType
		{
			VT_NICK  = 0x01,
			VT_SHARE = 0x02,
			VT_IP    = 0x04
			, VT_CALC_AVDB  = 0x80
		};
#endif
		enum ClientType
		{
			CT_BOT = 0x01,
			CT_REGGED = 0x02,
			CT_OP = 0x04, //-V112
			CT_SU = 0x08,
			CT_OWNER = 0x10,
			CT_HUB = 0x20, //-V112
#ifdef IRAINMAN_USE_HIDDEN_USERS
			CT_HIDDEN = 0x40,
#endif
			CT_USE_IP6 = 0x80
		};
		
		Identity()
		{
			memzero(&m_bits_info, sizeof(m_bits_info));
			m_is_p2p_guard_calc = false;
			m_is_real_user_ip_from_hub = false;
			m_bytes_shared = 0;
			m_is_ext_json = false;
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			m_virus_type = 0;
#endif
		}
		Identity(const UserPtr& ptr, uint32_t aSID) : user(ptr)
		{
			memzero(&m_bits_info, sizeof(m_bits_info));
			m_is_p2p_guard_calc = false;
			m_is_real_user_ip_from_hub = false;
			m_bytes_shared = 0;
			m_is_ext_json = false;
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			m_virus_type = 0;
#endif
			setSID(aSID);
		}
		
		
#ifdef FLYLINKDC_USE_DETECT_CHEATING
		
		enum FakeCard
		{
			NOT_CHECKED = 0x01,
			CHECKED     = 0x02,
			BAD_CLIENT  = 0x04 //-V112
			, BAD_LIST    = 0x08
		};
#endif
		enum NotEmptyString
		{
			EM = 0x01,
			DE = 0x02
		};
		
#ifndef IRAINMAN_IDENTITY_IS_NON_COPYABLE
		Identity(const Identity& rhs)
		{
			*this = rhs; // Use operator= since we have to lock before reading...
		}
		Identity& operator=(const Identity& rhs)
		{
			FastUniqueLock l(g_cs);
			user = rhs.user;
			m_stringInfo = rhs.m_stringInfo;
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			m_virus_type = rhs.m_virus_type;
#endif
			m_is_p2p_guard_calc = rhs.m_is_p2p_guard_calc;
			m_is_real_user_ip_from_hub = rhs.m_is_real_user_ip_from_hub;
			m_bytes_shared = rsh.m_bytes_shared;
			m_is_ext_json = rhs.m_is_ext_json;
			
			memcpy(&m_bits_info, &rhs.m_bits_info, sizeof(m_bits_info));
			return *this;
		}
#endif // IRAINMAN_IDENTITY_IS_NON_COPYABLE
		~Identity();
		
#define GSMC(n, x, c)\
	string get##n() const { return getStringParam(x); }\
	void set##n(const string& v) { /*dcassert(get##n() == v /* please don't set same value!);*/ setStringParam(x, v); change(c); }
		
#define GSM(n, x)\
	string get##n() const { return getStringParam(x); }\
	void set##n(const string& v) { /*dcassert(get##n() == v /* please don't set same value!);*/ setStringParam(x, v); }
		
		GSMC(Description, "DE", CHANGES_DESCRIPTION) // ok
		GSMC(Email, "EM", CHANGES_EMAIL) // ok
		GSMC(IP6, "I6", CHANGES_IP) // ok
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		GSMC(VirusPath, "VP", CHANGES_DESCRIPTION)
#endif
		GSMC(P2PGuard, "P2", CHANGES_DESCRIPTION)
		void setNick(const string& p_nick) // "NI"
		{
			// dcassert(!p_nick.empty());
			m_user_nick = p_nick;
			m_user_nickT = Text::toT(p_nick);
			getUser()->setLastNick(p_nick);
			change(CHANGES_NICK);
		}
#if 0
		void setNickFast(const string& p_nick) // Используется при первой инициализации
		{
			dcassert(!p_nick.empty());
			m_user_nick = p_nick;
			m_user_nickT = Text::toT(p_nick);
		}
#endif
		const string& getNick() const
		{
			static int g_count = 0;
			//dcdebug("[1]const string getNick %s count = %d\r\n", m_user_nick.c_str(), ++g_count);
			return m_user_nick;
		}
		const tstring& getNickT() const
		{
			static int g_count = 0;
			//dcdebug("[2]const tstring getNickT %s count = %d\r\n", m_user_nick.c_str(), ++g_count);
			return m_user_nickT;
		}
	public:
		string getSupports() const; // "SU"
		void setLimit(uint32_t lim) // "US"
		{
			getUser()->setLimit(lim);
			change(CHANGES_UPLOAD_SPEED);
		}
		const uint32_t getLimit() const// "US"
		{
			return getUser()->getLimit();
		}
		void setSlots(uint8_t slots) // "SL"
		{
			getUser()->setSlots(slots);
			change(CHANGES_SLOTS);
		}
		const uint8_t getSlots() const// "SL"
		{
			return getUser()->getSlots();
		}
		void setBytesShared(const int64_t bytes) // "SS"
		{
			// CFlyWriteLock(*g_rw_cs);
			dcassert(bytes >= 0);
			m_bytes_shared = bytes;
			getUser()->setBytesShared(bytes);
			change(CHANGES_SHARED | CHANGES_EXACT_SHARED);
		}
		const int64_t getBytesShared() const // "SS"
		{
			// CFlyReadLock(*g_rw_cs);
			return m_bytes_shared;
			//return getUser()->getBytesShared();
		}
		
		void setIp(const string& p_ip);
		bool isFantomIP() const;
		boost::asio::ip::address_v4 getIpRAW() const
		{
			return m_ip;
		}
		boost::asio::ip::address_v4 getIp() const
		{
			if (!m_ip.is_unspecified())
				return m_ip;
			else
				return getUser()->getIP();
		}
		bool isIPValid() const
		{
			return !m_ip.is_unspecified();
		}
		string getIpAsString() const;
	private:
		string m_user_nick;
		tstring m_user_nickT;
		boost::asio::ip::address_v4 m_ip; // "I4"
		int64_t m_bytes_shared;
	public:
		bool m_is_real_user_ip_from_hub;
		bool m_is_p2p_guard_calc;
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		unsigned char m_virus_type;
		void setVirusType(unsigned char p_virus_type_mask)
		{
			m_virus_type |= p_virus_type_mask;
		}
		void resetAntivirusInfo()
		{
			m_virus_type = 0;
		}
		unsigned char getVirusType() const
		{
			return m_virus_type & ~Identity::VT_CALC_AVDB;
		}
		unsigned char calcVirusType(bool p_force = false);
		bool isVirusOnlySingleType(VirusType p_type) const
		{
			return (m_virus_type & ~Identity::VT_CALC_AVDB) == p_type;
			return false;
		}
		string getVirusDesc() const;
#endif
		void calcP2PGuard();
		
// Нужна ли тут блокировка?
// L: с одной стороны надо блокировать такие операции,
// а с другой эти данные всегда изменяются в одном потоке,
// в потоке хаба, т.е. в потоке сокета.
// А вот читать их могут одновременно многие,
// однако, поскольку, чтение происходит побитно,
// то можно считать данную операцию атомарной, и не блокировать,
// но при этом куски, которые больше чем int для платформы надо блокировать обязательно.

#define GSUINTC(bits, x, c)\
	uint##bits##_t get##x() const  { return get_uint##bits##(e_##x); }\
	void set##x(uint##bits##_t val) { set_uint##bits##(e_##x, val); change(c); }

#define GSUINT(bits, x)\
	uint##bits##_t get##x() const  { return get_uint##bits##(e_##x); }\
	void set##x(uint##bits##_t val) { set_uint##bits##(e_##x, val); }
#define GC_INC_UINT(bits, x)\
	uint##bits##_t  inc##x() { return ++m_bits_info.info_uint##bits[e_##x]; }

#define GSUINTBIT(bits,x)\
	bool get##x##Bit(const uint##bits##_t p_bit_mask) const\
	{\
		return (get_uint##bits(e_##x) & p_bit_mask) != 0;\
	}\
	void set##x##Bit(const uint##bits##_t p_bit_mask, bool p_is_set)\
	{\
		auto& bits_info = get_uint##bits(e_##x);\
		if (p_is_set)\
		{\
			bits_info |= p_bit_mask;\
		}\
		else\
		{\
			bits_info &= ~p_bit_mask;\
		}\
	}

#define GSUINTBITS(bits)\
	const uint##bits##_t& get_uint##bits##(eTypeUint##bits##Attr p_attr_index) const\
	{\
		return m_bits_info.info_uint##bits[p_attr_index];\
	}\
	uint##bits##_t& get_uint##bits##(eTypeUint##bits##Attr p_attr_index)\
	{\
		return m_bits_info.info_uint##bits[p_attr_index];\
	}\
	void set_uint##bits(eTypeUint##bits##Attr p_attr_index, const uint##bits##_t& p_val)\
	{\
		m_bits_info.info_uint##bits[p_attr_index] = p_val;\
	}

#define GSINTC(bits, x)\
	int##bits##_t get##x() const  { return get_int##bits##(e_##x); }\
	void set##x(int##bits##_t val) { set_int##bits##(e_##x, val); change(c); }

#define GSINT(bits, x)\
	int##bits##_t get##x() const  { return get_int##bits##(e_##x); }\
	void set##x(int##bits##_t val) { set_int##bits##(e_##x, val); }

#define GSINTBITS(bits)\
	const int##bits##_t& get_int##bits(eTypeInt##bits##Attr p_attr_index) const\
	{\
		return m_bits_info.info_int##bits[p_attr_index];\
	}\
	int##bits##_t& get_int##bits(eTypeInt##bits##Attr p_attr_index)\
	{\
		return m_bits_info.info_int##bits[p_attr_index];\
	}\
	void set_int##bits(eTypeInt##bits##Attr p_attr_index, const int##bits##_t& p_val)\
	{\
		m_bits_info.info_int##bits[p_attr_index] = p_val;\
	}

//////////////////// uint8 ///////////////////
	private:
	
		enum eTypeUint8Attr
		{
			e_ClientType, // 7 бит
#ifdef FLYLINKDC_USE_DETECT_CHEATING
			e_FakeCard,   // 6 бит
#endif
			e_ConnectionTimeouts,
			e_FileListDisconnects,
			e_FreeSlots,
			e_KnownSupports, // 1 бит для ADC, 0 для NMDC
			e_KnownUcSupports, // 7 бит вперемешку.
			e_NotEmptyString, // 2 бита
			e_TypeUInt8AttrLast
		};
		GSUINTBITS(8);
		GSUINTBIT(8, ClientType);
		
	public:
	
		GSUINT(8, ConnectionTimeouts); // "TO"
		GC_INC_UINT(8, ConnectionTimeouts); // "TO"
		GSUINT(8, FileListDisconnects); // "FD"
		GC_INC_UINT(8, FileListDisconnects); // "FD"
		GSUINT(8, FreeSlots); // "FS"
		GSUINT(8, ClientType); // "CT"
		GSUINT(8, KnownSupports); // "SU"
		GSUINT(8, KnownUcSupports); // "SU"
		
		void setHub() // "CT"
		{
			return setClientTypeBit(CT_HUB, true);
		}
		bool isHub() const // "CT"
		{
			return getClientTypeBit(CT_HUB);
		}
		void setBot() // "CT"
		{
			return setClientTypeBit(CT_BOT, true);
		}
		bool isBot() const // "CT"
		{
			return getClientTypeBit(CT_BOT);
		}
		bool isBotOrHub() const // "CT"
		{
			dcassert(getClientTypeBit(CT_BOT | CT_HUB) == isBot() || isHub());
			return getClientTypeBit(CT_BOT | CT_HUB);
		}
		
		void setUseIP6() // "CT"
		{
			return setClientTypeBit(CT_USE_IP6, true);
		}
		bool isUseIP6() const // "CT"
		{
			return getClientTypeBit(CT_USE_IP6);
		}
		
		void setOp(bool op) // "CT"
		{
			return setClientTypeBit(CT_OP, op);
		}
		bool isOp() const  // "CT"
		{
			// TODO: please fix me.
			return getClientTypeBit(CT_OP /*| CT_SU*/ | CT_OWNER);
		}
		
		void setRegistered(bool reg) // "CT"
		{
			return setClientTypeBit(CT_REGGED, reg);
		}
		bool isRegistered() const // "CT"
		{
			return getClientTypeBit(CT_REGGED);
		}
#ifdef IRAINMAN_USE_HIDDEN_USERS
		void setHidden() // "CT"
		{
			return setClientTypeBit(CT_HIDDEN, true);
		}
		bool isHidden() const // "CT"
		{
			return getClientTypeBit(CT_HIDDEN);
		}
#endif
#ifdef FLYLINKDC_USE_DETECT_CHEATING
		GSUINT(8, FakeCard);
		GSUINTBIT(8, FakeCard);
#endif
		
		GSUINTBIT(8, NotEmptyString);
		GSUINT(8, NotEmptyString);
		
//////////////////// uint16 ///////////////////
	private:
		enum eTypeUint16Attr
		{
			e_UdpPort,
			e_DicVE, // "VE"
			e_DicAP, // "AP"
			e_TypeUInt16AttrLast
		};
		GSUINTBITS(16);
	public:
		GSUINT(16, UdpPort); // "U4"
		GSUINT(16, DicVE); // "VE"
		GSUINT(16, DicAP); // "AP"
		
//////////////////// uint32 ///////////////////
	private:
	
		enum eTypeUint32Attr
		{
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
			e_Changes,
#endif
			e_SID,
			e_HubNormalRegOper, // 30 bit.
			e_InfoBitMap,
			e_DownloadSpeed,
			e_SharedFiles,
			e_ExtJSONRAMWorkingSet,
			e_ExtJSONRAMPeakWorkingSet,
			e_ExtJSONRAMFree,
			e_ExtJSONlevelDBHistSize,
			e_ExtJSONSQLiteDBSize,
			e_ExtJSONSQLiteDBSizeFree,
			e_ExtJSONCountFiles,
			e_ExtJSONQueueFiles,
			e_ExtJSONQueueSrc,
			e_ExtJSONTimesStartCore,
			e_ExtJSONTimesStartGUI,
			//e_ExtJSONGDI,
			e_TypeUInt32AttrLast
		};
		GSUINTBITS(32);
	private:
		void change(const uint32_t p_change)
		{
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
			BOOST_STATIC_ASSERT(COLUMN_LAST - 1 <= 32); // [!] If you require more than 16 columns in the hub, please increase the size of m_changed_status and correct types in the code harness around her.
			//FastUniqueLock l(g_cs);
			get_uint32(e_Changes) |= p_change;
#endif
		}
	public:
		bool is_ip_change_and_clear()
		{
			if (get_uint32(e_Changes) & CHANGES_IP)
			{
				get_uint32(e_Changes) &= ~CHANGES_IP;
				return true;
			}
			return false;
		}
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
		uint32_t getChanges()
		{
			uint32_t ret = 0;
			std::swap(ret, get_uint32(e_Changes));
			return ret;
		}
#endif
		
	public:
	
		GSUINT(32, SID); // "SI"
		string getSIDString() const
		{
			const uint32_t sid = getSID();
			return string((const char*)&sid, 4); //-V112
		}
		
		GSUINTC(32, DownloadSpeed, CHANGES_CONNECTION); // "DS", "CO" (unofficial)
		GSUINT(32, SharedFiles); // "SF"
		
		GSUINT(32, ExtJSONRAMWorkingSet);
		GSUINT(32, ExtJSONRAMPeakWorkingSet);
		GSUINT(32, ExtJSONRAMFree);
		GSUINT(32, ExtJSONCountFiles);
		GSUINT(32, ExtJSONSQLiteDBSize);
		GSUINT(32, ExtJSONlevelDBHistSize);
		GSUINT(32, ExtJSONSQLiteDBSizeFree);
		GSUINT(32, ExtJSONQueueFiles);
		GSUINT(32, ExtJSONQueueSrc);
		GSUINT(32, ExtJSONTimesStartCore);
		GSUINT(32, ExtJSONTimesStartGUI);
		
		//GSUINT(32, ExtJSONGDI);
		
		GSUINTC(32, HubNormalRegOper, CHANGES_HUBS); // "HN"/"HR"/"HO" - Экономим RAM - 32 бита по 10 бит каждому значению
		uint16_t getHubNormal() const // "HN"
		{
			return (getHubNormalRegOper() >> 20) & 0x3FF;
		}
		uint16_t getHubRegister() const // "HR"
		{
			return (getHubNormalRegOper() >> 10) & 0x3FF;
		}
		uint16_t getHubOperator() const // "HO"
		{
			return getHubNormalRegOper() & 0x3FF;
		}
		void setHubNormal(const char* p_val) // "HN"
		{
			setHubNormal(Util::toUInt32(p_val));
		}
		void setHubRegister(const char* p_val) // "HR"
		{
			setHubRegister(Util::toUInt32(p_val));
		}
		void setHubOperator(const char* p_val) // "HO"
		{
			setHubOperator(Util::toUInt32(p_val));
		}
		void setHubNormal(unsigned p_val) // "HN"
		{
			get_uint32(e_HubNormalRegOper) &= ~0x3FF00000;
			get_uint32(e_HubNormalRegOper) |= uint32_t((p_val << 20) & 0x3FF00000); // 00111111111100000000000000000000
		}
		void setHubRegister(unsigned p_val) // "HR"
		{
			get_uint32(e_HubNormalRegOper) &= ~0xFFC00;
			get_uint32(e_HubNormalRegOper) |= uint32_t((p_val << 10) & 0xFFC00);    // 00000000000011111111110000000000
		}
		void setHubOperator(unsigned p_val) // "HO"
		{
			get_uint32(e_HubNormalRegOper) &= ~0x3FF;
			get_uint32(e_HubNormalRegOper) |= uint32_t(p_val & 0x3FF);              // 00000000000000000000001111111111
		}
		
//////////////////// int64 ///////////////////
	private:
	
		enum eTypeInt64Attr
		{
			e_ExtJSONLastSharedDate = 0,
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
			e_RealBytesShared = 1,
#endif
			e_TypeInt64AttrLast
		};
		GSINTBITS(64);
	public:
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
		GSINT(64, RealBytesShared) // "RS"
#endif
		GSINT(64, ExtJSONLastSharedDate)
//////////////////////////////////
	public:
		string getCID() const
		{
			return getUser()->getCID().toBase32();
		}
		string getTag() const;
		string getApplication() const;
		tstring getHubs() const;
		
#ifdef FLYLINKDC_USE_EXT_JSON
	private:
		bool m_is_ext_json;
#ifdef FLYLINKDC_USE_CHECK_EXT_JSON
		string m_lastExtJSON;
#endif
	public:
#ifdef FLYLINKDC_USE_LOCATION_DIALOG
		string getFlyHubCountry() const
		{
			return getStringParamExtJSON("F1");
		}
		string getFlyHubCity() const
		{
			return getStringParamExtJSON("F2");
		}
		string getFlyHubISP() const
		{
			return getStringParamExtJSON("F3");
		}
#endif
		int getGenderType() const
		{
			return Util::toInt(getStringParamExtJSON("F4"));
		}
		tstring getGenderTypeAsString() const
		{
			return getGenderTypeAsString(getGenderType());
		}
		tstring getGenderTypeAsString(int p_index) const
		{
			switch (p_index)
			{
				case 1:
					return WSTRING(FLY_GENDER_NONE);
				case 2:
					return WSTRING(FLY_GENDER_MALE);
				case 3:
					return WSTRING(FLY_GENDER_FEMALE);
				case 4:
					return WSTRING(FLY_GENDER_ASEXUAL);
			}
			return Util::emptyStringT;
		}
		string getExtJSONSupportInfo() const
		{
			return getStringParamExtJSON("F5");
		}
		void setExtJSONSupportInfo(const string& p_value)
		{
			setStringParam("F5", p_value);
		}
		string getExtJSONHubRamAsText() const
		{
			string l_res;
			if (m_is_ext_json)
			{
				if (getExtJSONRAMWorkingSet())
				{
					l_res = Util::formatBytes(int64_t(getExtJSONRAMWorkingSet() * 1024 * 1024));
				}
				if (getExtJSONRAMPeakWorkingSet() != getExtJSONRAMWorkingSet())
				{
					l_res += " [Max: " + Util::formatBytes(int64_t(getExtJSONRAMPeakWorkingSet() * 1024 * 1024)) + "]";
				}
				if (getExtJSONRAMFree())
				{
					l_res += " [Free: " + Util::formatBytes(int64_t(getExtJSONRAMFree() * 1024 * 1024)) + "]";
				}
			}
			return l_res;
		}
		
		string getExtJSONCountFilesAsText() const
		{
			if (m_is_ext_json && getExtJSONCountFiles())
				return Util::toString(getExtJSONCountFiles());
			else
				return Util::emptyString;
		}
		string getExtJSONLastSharedDateAsText() const
		{
			if (m_is_ext_json && getExtJSONLastSharedDate())
				return Util::formatDigitalClock(getExtJSONLastSharedDate());
			else
				return Util::emptyString;
		}
		
		string getExtJSONSQLiteDBSizeAsText() const
		{
			string l_res;
			if (m_is_ext_json)
			{
				if (getExtJSONSQLiteDBSize())
				{
					l_res = Util::formatBytes(int64_t(getExtJSONSQLiteDBSize()) * 1024 * 1024);
				}
				if (getExtJSONSQLiteDBSizeFree())
				{
					l_res += " [Free: " + Util::formatBytes(int64_t(getExtJSONSQLiteDBSizeFree()) * 1024 * 1024) + "]";
				}
				if (getExtJSONlevelDBHistSize())
				{
					l_res += " [LevelDB: " + Util::formatBytes(int64_t(getExtJSONlevelDBHistSize()) * 1024 * 1024) + "]";
				}
			}
			return l_res;
		}
		string getExtJSONQueueFilesText() const
		{
			string l_res;
			if (m_is_ext_json)
			{
				if (getExtJSONQueueFiles())
				{
					l_res = "[Files: " + Util::toString(getExtJSONQueueFiles()) + "]";
				}
				if (getExtJSONQueueSrc())
				{
					l_res += " [Sources: " + Util::toString(getExtJSONQueueSrc()) + "]";
				}
			}
			return l_res;
		}
		string getExtJSONTimesStartCoreText() const
		{
			string l_res;
			if (m_is_ext_json)
			{
				if (getExtJSONTimesStartCore())
				{
					l_res = "[Start core: " + Util::toString(getExtJSONTimesStartCore()) + "]";
				}
				if (getExtJSONTimesStartGUI())
				{
					l_res += " [Start GUI: " + Util::toString(getExtJSONTimesStartGUI()) + "]";
				}
			}
			return l_res;
		}
		
		
#endif // EXT_JSON
		
		static string formatShareBytes(uint64_t p_bytes);
		static string formatIpString(const string& value);
		static string formatSpeedLimit(const uint32_t limit);
		
		bool isTcpActive() const;
		bool isUdpActive() const;
		
		
		string getStringParam(const char* name) const;
		string getStringParamExtJSON(const char* name) const
		{
			if (m_is_ext_json)
				return getStringParam(name);
			else
				return Util::emptyString;
		}
		void setStringParam(const char* p_name, const string& p_val);
		bool isAppNameExists() const
		{
			if (getDicAP() > 0)
				return true;
			else if (getDicVE() > 0)
				return true;
			else
				return false;
		}
		
		
#ifdef FLYLINKDC_USE_DETECT_CHEATING
		string setCheat(const ClientBase& c, const string& aCheatDescription, bool aBadClient);
#endif
		void getReport(string& p_report) const;
		void updateClientType(const OnlineUser& ou)
		{
			setStringParam("CS", Util::emptyString);
#ifdef FLYLINKDC_USE_DETECT_CHEATING
			setFakeCardBit(BAD_CLIENT, false);
#endif
		}
		
		void getParams(StringMap& map, const string& prefix, bool compatibility) const;
		UserPtr& getUser()
		{
			return user;
		}
		GETSET(UserPtr, user, User);
		bool isExtJSON() const
		{
			return m_is_ext_json;
		}
		bool setExtJSON(const string& p_ExtJSON);
		
		typedef std::unordered_map<short, string> InfMap;
		
		mutable FastCriticalSection m_si_fcs;
		InfMap m_stringInfo;
		
		typedef vector<string> StringDictionaryReductionPointers;
		typedef std::unordered_map<string, uint16_t> StringDictionaryIndex;
		
		static StringDictionaryReductionPointers g_infoDic;
		static StringDictionaryIndex g_infoDicIndex;
		
#ifndef _DEBUG
		static
#endif
		uint32_t mergeDicId(const string& p_val);
		static string getDicVal(uint16_t p_index);
		
#pragma pack(push,1)
		struct
		{
			int64_t  info_int64 [e_TypeInt64AttrLast];
			uint32_t info_uint32[e_TypeUInt32AttrLast];
			uint16_t info_uint16[e_TypeUInt16AttrLast];
			uint8_t  info_uint8 [e_TypeUInt8AttrLast];
		} m_bits_info;
#pragma pack(pop)
		
		static std::unique_ptr<webrtc::RWLockWrapper> g_rw_cs;
};
class OnlineUser :  public UserInfoBase
{
		friend class NmdcHub;
	public:
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
		string m_raw_myinfo;
#endif
		enum
		{
			COLUMN_FIRST,
			COLUMN_NICK = COLUMN_FIRST,
			COLUMN_SHARED,
			COLUMN_EXACT_SHARED,
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			COLUMN_ANTIVIRUS,
#endif
			COLUMN_P2P_GUARD,
			COLUMN_DESCRIPTION,
			COLUMN_CONNECTION,
			COLUMN_IP,
			
			COLUMN_LAST_IP,
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
			COLUMN_UPLOAD,
			COLUMN_DOWNLOAD,
			COLUMN_MESSAGES,
#endif
			COLUMN_EMAIL,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
			COLUMN_VERSION,
			COLUMN_MODE,
#endif
			COLUMN_HUBS,
			COLUMN_SLOTS,
			COLUMN_CID,
			COLUMN_TAG,
			COLUMN_LAST
		};
		
		struct Hash
		{
			size_t operator()(const OnlineUserPtr& x) const
			{
				// return ((size_t)(&(*x))) / sizeof(OnlineUser);
				size_t cidHash = 0;
				boost::hash_combine(cidHash, x);
				return cidHash;
			}
		};
		
		OnlineUser(const UserPtr& p_user, ClientBase& p_client, uint32_t p_sid)
			: m_identity(p_user, p_sid), m_client(p_client), m_is_first_find(true)
		{
#ifdef _DEBUG
			g_online_user_counts++;
#endif
		}
		
		virtual ~OnlineUser() noexcept
		{
#ifdef _DEBUG
			g_online_user_counts--;
#endif
		}
#ifdef _DEBUG
		static std::atomic<int> g_online_user_counts;
#endif
		
		operator UserPtr&()
		{
			return getUser();
		}
		operator const UserPtr&() const
		{
			return getUser();
		}
		UserPtr& getUser() // TODO
		{
			return m_identity.getUser();
		}
		const UserPtr& getUser() const // TODO
		{
			return m_identity.getUser();
		}
		Identity& getIdentity() // TODO
		{
			return m_identity;
		}
		const Identity& getIdentity() const // TODO
		{
			return m_identity;
		}
		Client& getClient()
		{
			return (Client&)m_client;
		}
		const Client& getClient() const
		{
			return (const Client&)m_client;
		}
		ClientBase& getClientBase()
		{
			return m_client;
		}
		const ClientBase& getClientBase() const
		{
			return m_client;
		}
		
		bool update(int sortCol, const tstring& oldText = Util::emptyStringT);
		uint8_t getImageIndex() const
		{
			return UserInfoBase::getImage(*this);
		}
		bool isFirstFind()
		{
			const auto l_old = m_is_first_find;
			m_is_first_find = false;
			return l_old;
		}
		static int compareItems(const OnlineUser* a, const OnlineUser* b, uint8_t col);
		bool isHub() const
		{
			return m_identity.isHub();
		}
#ifdef IRAINMAN_USE_HIDDEN_USERS
		bool isHidden() const
		{
			return m_identity.isHidden();
		}
#endif
		tstring getText(uint8_t col) const;
	private:
		Identity m_identity;
		ClientBase& m_client;
#ifdef FLYLINKDC_USE_CHECK_CHANGE_TAG
		string m_tag;
#endif
		bool m_is_first_find;
#ifdef FLYLINKDC_USE_CHECK_CHANGE_TAG
	public:
		bool isTagUpdate(const string& p_tag, bool& p_is_version_change);
		string m_tag_old;
#endif
		
};

// http://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key
/*
namespace std
{
template <>
struct hash<OnlineUserPtr>
{
    size_t operator()(const OnlineUserPtr & x) const
    {
        return ((size_t)(&(*x))) / sizeof(OnlineUser);
    }
};
}
*/


#endif /* ONLINEUSER_H_ */
