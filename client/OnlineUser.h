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

#ifndef DCPLUSPLUS_DCPP_ONLINEUSER_H_
#define DCPLUSPLUS_DCPP_ONLINEUSER_H_

#include <boost/unordered/unordered_map.hpp>
#include "StringPool.h"
#include "User.h"
#include "UserInfoBase.h"
#include "UserInfoColumns.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"

class ClientBase;
class NmdcHub;
class AdcHub;

extern bool setAdcUserFlags(const UserPtr& user, const string& feat);

/** One of possibly many identities of a user, mainly for UI purposes */
class Identity
#ifdef _DEBUG // [+] IRainman fix.
	: virtual NonDerivable<Identity>
#ifdef IRAINMAN_IDENTITY_IS_NON_COPYABLE
	, boost::noncopyable
#endif
#endif
{
	public:
// [+] IRAINMAN_USE_NG_FAST_USER_INFO
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
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
			CHANGES_UPLOAD = 1 << COLUMN_UPLOAD,
			CHANGES_DOWNLOAD = 1 << COLUMN_DOWNLOAD,
			CHANGES_MESSAGES = 1 << COLUMN_MESSAGES,
#endif
#ifdef PPA_INCLUDE_DNS
			CHANGES_DNS = 1 << COLUMN_DNS, // !SMT!-IP
#endif
			//[-]PPA        CHANGES_PK = 1 << COLUMN_PK,
			CHANGE_CID = 1 << COLUMN_CID,
			CHANGE_TAG = 1 << COLUMN_TAG
		};
// [~] IRAINMAN_USE_NG_FAST_USER_INFO
		enum ClientType
		{
			CT_BOT = 0x01,
			CT_REGGED = 0x02,
			CT_OP = 0x04, //-V112
			CT_SU = 0x08,
			CT_OWNER = 0x10,
			CT_HUB = 0x20, //-V112
#ifdef IRAINMAN_USE_HIDDEN_USERS
			CT_HIDDEN = 0x40
#endif
		};
		
		Identity()
		{
			memzero(&m_bits_info, sizeof(m_bits_info));
		}
		Identity(const UserPtr& ptr, uint32_t aSID) : user(ptr)
		{
			memzero(&m_bits_info, sizeof(m_bits_info));
			setSID(aSID);
		}
		
		//enum Status // [!] IRainman: Only for parse the tag on NMDC hubs! moved to NmdcStatus class.
		
		enum FakeCard // [!] IRainman: The internal feature to the protocols it has nothing to do!
		{
			NOT_CHECKED = 0x01,
			CHECKED     = 0x02,
			BAD_CLIENT  = 0x04, //-V112
			BAD_LIST    = 0x08
		};
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
			memcpy(&m_bits_info, &rhs.m_bits_info, sizeof(m_bits_info));
			return *this;
		}
#endif // IRAINMAN_IDENTITY_IS_NON_COPYABLE
		~Identity();
		
// [!] IRAINMAN_USE_NG_FAST_USER_INFO
#define GSMC(n, x, c)\
	string get##n() const { return getStringParam(x); }\
	void set##n(const string& v) { /*dcassert(get##n() == v /* please don't set same value!);*/ setStringParam(x, v); change(c); } // [!] IRainman opt.

#define GSM(n, x)\
	string get##n() const { return getStringParam(x); }\
	void set##n(const string& v) { /*dcassert(get##n() == v /* please don't set same value!);*/ setStringParam(x, v); } // [!] IRainman fix.

		GSMC(Description, "DE", CHANGES_DESCRIPTION) // ok
		GSMC(Email, "EM", CHANGES_EMAIL) // ok
		
		void setNick(const string& p_nick) // "NI"
		{
			m_nick = p_nick;
			getUser()->setLastNick(p_nick);
			change(CHANGES_NICK);
		}
		void setNickFast(const string& p_nick) // Используется при первой инициализации
		{
			m_nick = p_nick;
		}
		GETM(string, m_nick, Nick); // "NI"
		const tstring getNickT() const
		{
			return Text::toT(m_nick);
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
			webrtc::WriteLockScoped l(*g_rw_cs);
			dcassert(bytes >= 0);
			getUser()->setBytesShared(bytes);
			change(CHANGES_SHARED | CHANGES_EXACT_SHARED);
		}
		const int64_t getBytesShared() const // "SS"
		{
			webrtc::ReadLockScoped l(*g_rw_cs);
			return getUser()->getBytesShared();
		}
		
		void setIp(const string& p_ip) // "I4"
		{
			boost::system::error_code ec;
			m_ip = boost::asio::ip::address_v4::from_string(p_ip, ec);
			dcassert(!ec);
			getUser()->setIP(m_ip);
			change(CHANGES_IP | CHANGES_GEO_LOCATION);
		}
		bool isFantomIP() const
		{
			return m_ip.is_unspecified();
		}
		boost::asio::ip::address_v4 getIp() const
		{
			if (!m_ip.is_unspecified())
				return m_ip;
			else
				return getUser()->getIP();
		}
		string getIpAsString() const
		{
			if (!m_ip.is_unspecified())
				return m_ip.to_string();
			else
				return getUser()->getIPAsString();
		}
	private:
		boost::asio::ip::address_v4 m_ip; // "I4" // [!] IRainman fix: needs here, details https://code.google.com/p/flylinkdc/issues/detail?id=1330
	public:
	
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
			e_FakeCard,   // 6 бит
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
		GSUINT(8, FakeCard);
		GSUINTBIT(8, FakeCard);
		GSUINTBIT(8, NotEmptyString);
		GSUINT(8, NotEmptyString);
		
//////////////////// uint16 ///////////////////
	private:
	
		enum eTypeUint16Attr
		{
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
			e_Changes,
#endif
			e_UdpPort,
			e_DicVE, // "VE"
			e_DicAP, // "AP"
			e_TypeUInt16AttrLast
		};
		GSUINTBITS(16);
#if 0
	public:
		bool is_ip_change_and_clear()
		{
			if (get_uint16(e_Changes) & CHANGES_IP)
			{
				get_uint16(e_Changes) &= ~CHANGES_IP;
				return true;
			}
			return false;
		}
	private:
#endif
		void change(const uint16_t p_change)
		{
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
			BOOST_STATIC_ASSERT(COLUMN_LAST - 1 <= 16); // [!] If you require more than 16 columns in the hub, please increase the size of m_changed_status and correct types in the code harness around her.
			//FastUniqueLock l(g_cs);
			get_uint16(e_Changes) |= p_change;
#endif
		}
	public:
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
		uint16_t getChanges()
		{
			uint16_t ret = 0;
			std::swap(ret, get_uint16(e_Changes));
			return ret;
		}
#endif
		
		GSUINT(16, UdpPort); // "U4"
		GSUINT(16, DicVE); // "VE"
		GSUINT(16, DicAP); // "AP"
		
//////////////////// uint32 ///////////////////
	private:
	
		enum eTypeUint32Attr
		{
			e_SID,
			e_HubNormalRegOper, // 30 bit.
			e_InfoBitMap,
			e_DownloadSpeed,
			e_SharedFiles,
			e_TypeUInt32AttrLast
		};
		GSUINTBITS(32);
		
	public:
	
		GSUINT(32, SID); // "SI"
		string getSIDString() const
		{
			const uint32_t sid = getSID();
			return string((const char*)&sid, 4); //-V112
		}
		
		GSUINTC(32, DownloadSpeed, CHANGES_CONNECTION); // "DS", "CO" (unofficial)
		
		GSUINT(32, SharedFiles); // "SF"
		
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
			setHubNormal(Util::toInt(p_val));
		}
		void setHubRegister(const char* p_val) // "HR"
		{
			setHubRegister(Util::toInt(p_val));
		}
		void setHubOperator(const char* p_val) // "HO"
		{
			setHubOperator(Util::toInt(p_val));
		}
		void setHubNormal(int p_val) // "HN"
		{
			get_uint32(e_HubNormalRegOper) |= uint32_t((p_val << 20) & 0x3FF00000); // 00111111111100000000000000000000
		}
		void setHubRegister(int p_val) // "HR"
		{
			get_uint32(e_HubNormalRegOper) |= uint32_t((p_val << 10) & 0xFFC00);    // 00000000000011111111110000000000
		}
		void setHubOperator(int p_val) // "HO"
		{
			get_uint32(e_HubNormalRegOper) |= uint32_t(p_val & 0x3FF);              // 00000000000000000000001111111111
		}
		
//////////////////// int64 ///////////////////
	private:
	
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
#define FLYLINKDC_USE_IDENTITY_64_BIT
		enum eTypeInt64Attr
		{
			e_RealBytesShared = 0,
			e_TypeInt64AttrLast
		};
		GSINTBITS(64);
	public:
		GSINT(64, RealBytesShared) // "RS"
#endif
//////////////////////////////////
	public:
		string getCID() const
		{
			return getUser()->getCID().toBase32();
		}
		string getTag() const;
		string getApplication() const;
		tstring getHubs() const;
		
		// [+] IRainman
		static string formatShareBytes(uint64_t p_bytes);
		static string formatIpString(const string& value);
		static string formatSpeedLimit(const uint32_t limit);
		// [~] IRainman
		
		bool isTcpActive(const Client* client) const;
		bool isTcpActive() const;
		//bool isUdpActive(const Client* client) const;
		bool isUdpActive() const;
		
		// [!] IRainman fix.
		string getStringParam(const char* name) const;
		void setStringParam(const char* p_name, const string& p_val);
		bool isAppNameExists() const
		{
			if (getDicAP() > 0)
				return true;
			else
			if (getDicVE() > 0)
				return true;
			else
				return false;
		}
		// [~] IRainman fix.
		
		string setCheat(const ClientBase& c, const string& aCheatDescription, bool aBadClient);
		void getReport(string& p_report) const;
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
		string updateClientType(const OnlineUser& ou);
		bool matchProfile(const string& aString, const string& aProfile) const;
		
		static string getVersion(const string& aExp, string aTag);
		static string splitVersion(const string& aExp, string aTag, size_t part);
#else
		void updateClientType(const OnlineUser& ou)
		{
			setStringParam("CS", Util::emptyString);
			setFakeCardBit(BAD_CLIENT , false);
		}
#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER
		
		void getParams(StringMap& map, const string& prefix, bool compatibility, bool dht = false) const;
		UserPtr& getUser()
		{
			return user;
		}
		GETSET(UserPtr, user, User);
		
		typedef boost::unordered_map<short, string> InfMap;
		
		InfMap m_stringInfo;
		
		typedef vector<const string*> StringDictionaryReductionPointers;
		typedef boost::unordered_map<string, uint16_t> StringDictionaryIndex;
		
		static StringDictionaryReductionPointers g_infoDic;
		static StringDictionaryIndex g_infoDicIndex;
		
		uint32_t mergeDicIdL(const string& p_val)
		{
			if (p_val.empty())
				return 0;
			auto l_find = g_infoDicIndex.insert(make_pair(p_val, g_infoDic.size() + 1));
			if (l_find.second == true) // Новое значение в справочнике?
			{
				g_infoDic.push_back(&l_find.first->first);   // Сохраняем указатель на строчку
			}
			return l_find.first->second;
		}
		
		const string& getDicValL(uint16_t p_index) const
		{
			dcassert(p_index > 0);
			if (p_index > 0)
			{
				return *g_infoDic[p_index - 1];
			}
			else
			{
				return Util::emptyString;
			}
		}
		
#pragma pack(push,1)
		struct
		{
#ifdef FLYLINKDC_USE_IDENTITY_64_BIT
			int64_t  info_int64 [e_TypeInt64AttrLast];
#endif
			uint32_t info_uint32[e_TypeUInt32AttrLast];
			uint16_t info_uint16[e_TypeUInt16AttrLast];
			uint8_t  info_uint8 [e_TypeUInt8AttrLast];
		} m_bits_info;
#pragma pack(pop)
		
		static std::unique_ptr<webrtc::RWLockWrapper> g_rw_cs;
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
		string getDetectionField(const string& aName) const;
		void getDetectionParams(StringMap& p);
		string getPkVersion() const;
#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER
};
class OnlineUser :
	public intrusive_ptr_base<OnlineUser>, public UserInfoBase
{
		friend class NmdcHub;
	public:
		enum
		{
			COLUMN_FIRST,
			COLUMN_NICK = COLUMN_FIRST,
			COLUMN_SHARED,
			COLUMN_EXACT_SHARED,
			COLUMN_DESCRIPTION,
			COLUMN_CONNECTION,
			COLUMN_IP,
			//[+]PPA
			COLUMN_LAST_IP,
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
			COLUMN_UPLOAD,
			COLUMN_DOWNLOAD,
			COLUMN_MESSAGES,
#endif
			//[~]PPA
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
				size_t cidHash = 0;
				boost::hash_combine(cidHash, x);
				//return boost::hash<OnlineUserPtr>(x);
				// TODO - check x->getUser()
				//memcpy(&cidHash, &x->getUser()->getCID(), sizeof(size_t)); //-V512
				return cidHash;
			}
		};
		
		OnlineUser(const UserPtr& p_user, ClientBase& p_client, uint32_t p_sid)
			: m_identity(p_user, p_sid), m_client(p_client), m_is_first_find(true)
		{
#ifdef _DEBUG
			++g_online_user_counts;
#endif
		}
		
		virtual ~OnlineUser() noexcept
		{
#ifdef _DEBUG
			--g_online_user_counts;
#endif
		}
#ifdef _DEBUG
		static boost::atomic_int g_online_user_counts; //
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
		bool m_is_first_find;
};

// http://stackoverflow.com/questions/17016175/c-unordered-map-using-a-custom-class-type-as-the-key
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

#endif /* ONLINEUSER_H_ */
