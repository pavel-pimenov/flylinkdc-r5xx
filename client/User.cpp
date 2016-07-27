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

#include "stdinc.h"
#include "AdcHub.h"
#include "Client.h"
#include "ClientManager.h"
#include "UserCommand.h"
#include "CFlylinkDBManager.h"
#include "Wildcards.h"
#include "UserConnection.h"
#include "LogManager.h"
#include "../FlyFeatures/flyServer.h"

std::unique_ptr<webrtc::RWLockWrapper> Identity::g_rw_cs = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());

#ifdef _DEBUG
#define DISALLOW(a, b) { uint16_t tag1 = TAG(name[0], name[1]); uint16_t tag2 = TAG(a, b); dcassert(tag1 != tag2); }
#else
#define DISALLOW(a, b)
#endif

#ifdef _DEBUG
boost::atomic_int User::g_user_counts(0);
boost::atomic_int OnlineUser::g_online_user_counts(0);
#endif

Identity::StringDictionaryReductionPointers Identity::g_infoDic;
Identity::StringDictionaryIndex Identity::g_infoDicIndex;

#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO

User::User(const CID& aCID) : m_cid(aCID),
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	m_support_slots(FLY_SUPPORT_SLOTS_FIRST),
#endif
	m_slots(0),
	m_bytesShared(0),
	m_limit(0)
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	, m_hub_id(0)
	, m_ratio_ptr(nullptr)
#endif
{
	m_message_count.set(0);
	m_message_count.reset_dirty();
	setFlag(User::IS_SQL_NOT_FOUND);
	BOOST_STATIC_ASSERT(LAST_BIT < 32);
#ifdef _DEBUG
#ifdef FLYLINKDC_USE_RATIO_CS
	m_ratio_cs.use_log();
#endif
	++g_user_counts;
# ifdef ENABLE_DEBUG_LOG_IN_USER_CLASS
	dcdebug(" [!!!!!!]   [!!!!!!]  User::User(const CID& aCID) this = %p, g_user_counts = %d\n", this, g_user_counts);
# endif
#endif
}

User::~User()
{
	// TODO пока нельзя - вешается flushRatio();
#ifdef _DEBUG
	--g_user_counts;
# ifdef ENABLE_DEBUG_LOG_IN_USER_CLASS
	dcdebug(" [!!!!!!]   [!!!!!!]  User::~User() this = %p, g_user_counts = %d\n", this, g_user_counts);
# endif
#endif
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	safe_delete(m_ratio_ptr);
#endif
}

void User::setLastNick(const string& p_nick)
{
	//dcassert(!p_nick.empty());
	if (m_ratio_ptr == nullptr)
	{
		m_nick = p_nick;
	}
	else
	{
		if (m_nick != p_nick)
		{
			const bool l_is_change_nick = !m_nick.empty() && !p_nick.empty();
			if (l_is_change_nick)
			{
				if (m_ratio_ptr)
				{
					{
#ifdef FLYLINKDC_USE_RATIO_CS
						CFlyFastLock(m_ratio_cs);
#endif
						safe_delete(m_ratio_ptr);
					}
					m_nick = p_nick;
					initRatio();
				}
			}
			else
			{
				m_nick = p_nick;
			}
			if (m_ratio_ptr)
			{
				m_ratio_ptr->set_dirty(true);
			}
		}
		else
		{
			// TODO dcassert(p_nick != m_nick); // Ловим холостое обновление
		}
	}
}
void User::setIP(const string& p_ip, bool p_is_set_only_ip)
{
	boost::system::error_code ec;
	const auto l_ip = boost::asio::ip::address_v4::from_string(p_ip, ec);
	dcassert(!ec);
	if (!ec)
	{
		setIP(l_ip, p_is_set_only_ip);
	}
	else
	{
#ifdef FLYLINKDC_BETA
		const string l_message = "User::setIP Error IP = " + p_ip;
		LogManager::message(l_message);
		CFlyServerJSON::pushError(27, l_message);
#endif
	}
}

void User::setIP(const boost::asio::ip::address_v4& p_last_ip, bool p_is_set_only_ip)
{
	if (m_ratio_ptr && p_is_set_only_ip == false)
	{
		dcassert(!p_last_ip.is_unspecified());
		if (m_last_ip_sql.get() != p_last_ip) // TODO подумать где лучше делать преобразование
		{
#ifdef _DEBUG
			if (!m_last_ip_sql.get().is_unspecified() && p_last_ip.is_unspecified())
			{
				dcassert(0);
			}
#endif
			const bool l_is_change_ip = !m_last_ip_sql.get().is_unspecified() && !p_last_ip.is_unspecified();
			if (l_is_change_ip)
			{
				if (m_ratio_ptr)
				{
#ifdef FLYLINKDC_USE_RATIO_CS
					CFlyFastLock(m_ratio_cs);
#endif
					safe_delete(m_ratio_ptr);
					initRatioL(p_last_ip);
				}
				if (m_last_ip_sql.set(p_last_ip))
				{
					setFlag(CHANGE_IP);
				}
			}
		}
		else
		{
			// dcassert(p_last_ip != m_ratio_ptr->m_last_ip); // Ловим холостое обновление
		}
	}
	else
	{
#ifdef DEBUG
		//if (getLastNick() == "BSOD2600")
		//{
		//  initMesageCount();
		//}
#endif // DEBUG
		if (p_is_set_only_ip == false)
		{
			initMesageCount();
		}
		if (m_last_ip_sql.set(p_last_ip))
		{
			setFlag(CHANGE_IP);
		}
	}
}
boost::asio::ip::address_v4 User::getIP()
{
	initMesageCount();
	if (!m_last_ip_sql.get().is_unspecified())
	{
		return m_last_ip_sql.get();
	}
	return boost::asio::ip::address_v4();
}
string User::getIPAsString()
{
	const auto l_ip = getIP();
	if (!l_ip.is_unspecified())
	{
		return l_ip.to_string();
	}
	return Util::emptyString;
}
uint64_t User::getBytesUpload()
{
	initRatio();
#ifdef FLYLINKDC_USE_RATIO_CS
	CFlyFastLock(m_ratio_cs);
#endif
	if (m_ratio_ptr)
	{
		return m_ratio_ptr->get_upload();
	}
	else
	{
		return 0;
	}
}
uint64_t User::getMessageCount()
{
	initRatio();
	return m_message_count.get();
}
uint64_t User::getBytesDownload()
{
	initRatio();
#ifdef FLYLINKDC_USE_RATIO_CS
	CFlyFastLock(m_ratio_cs);
#endif
	if (m_ratio_ptr)
	{
		return m_ratio_ptr->get_download();
	}
	else
	{
		return 0;
	}
}
void User::fixLastIP()
{
	initMesageCount();
	if (!m_last_ip_sql.get().is_unspecified())
	{
		setIP(m_last_ip_sql.get(), true);
	}
}
void User::incMessagesCount()
{
	m_message_count.set(m_message_count.get() + 1);
}
void User::AddRatioUpload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size)
{
#ifdef FLYLINKDC_USE_RATIO_CS
	CFlyFastLock(m_ratio_cs);
#endif
	initRatioL(p_ip);
	if (m_ratio_ptr)
	{
		m_ratio_ptr->addUpload(p_ip, p_size);
	}
}
void User::AddRatioDownload(const boost::asio::ip::address_v4& p_ip, uint64_t p_size)
{
#ifdef FLYLINKDC_USE_RATIO_CS
	CFlyFastLock(m_ratio_cs);
#endif
	initRatioL(p_ip);
	if (m_ratio_ptr)
	{
		m_ratio_ptr->addDownload(p_ip, p_size);
	}
}
bool User::flushRatio()
{
	bool l_result = false;
	{
#ifdef FLYLINKDC_USE_RATIO_CS
		CFlyFastLock(m_ratio_cs);
#endif
		if (m_ratio_ptr)
		{
			l_result = m_ratio_ptr->flushRatioL();
			return l_result;
		}
	}
	if (BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER))
	{
		if ((m_last_ip_sql.is_dirty() && !m_last_ip_sql.get().is_unspecified()) ||
		        m_message_count.is_dirty() && m_message_count.get())
		{
			// LogManager::message("User::flushRatio m_nick = " + m_nick);
			if (getHubID() && !m_nick.empty() && CFlylinkDBManager::isValidInstance() && !m_last_ip_sql.get().is_unspecified())
			{
				bool l_is_sql_not_found = isSet(User::IS_SQL_NOT_FOUND);
				CFlylinkDBManager::getInstance()->update_last_ip_and_message_count(getHubID(), m_nick, m_last_ip_sql.get(), m_message_count.get(), l_is_sql_not_found,
				                                                                   m_last_ip_sql.is_dirty(),
				                                                                   m_message_count.is_dirty()
				                                                                  );
				setFlag(User::IS_SQL_NOT_FOUND, false);
				m_last_ip_sql.reset_dirty();
				m_message_count.reset_dirty();
				l_result = true;
			}
		}
	}
	return l_result;
}
void User::initRatioL(const boost::asio::ip::address_v4& p_ip)
{
	if (BOOLSETTING(ENABLE_RATIO_USER_LIST))
	{
		if (m_ratio_ptr == nullptr && !m_nick.empty() && m_hub_id)
		{
			m_ratio_ptr = new CFlyUserRatioInfo(this);
			if (m_ratio_ptr->tryLoadRatio(p_ip) == false)
			{
				// TODO - стереть m_ratio_ptr?
			}
		}
	}
}
void User::initMesageCount()
{
	if (BOOLSETTING(ENABLE_LAST_IP_AND_MESSAGE_COUNTER))
	{
		if (!m_nick.empty() && m_hub_id && !isSet(IS_FIRST_INIT_RATIO)
		        // Глючит когда шлется UserIP && isSet(IS_MYINFO)
		   )
		{
			setFlag(IS_FIRST_INIT_RATIO);
			m_last_ip_sql.reset_dirty();
			m_message_count.reset_dirty();
			// Узнаем, есть ли в базе last_ip или счетчик мессаг
			uint32_t l_message_count = 0;
			boost::asio::ip::address_v4 l_last_ip_from_sql;
			const bool l_is_sql_not_found = !CFlylinkDBManager::getInstance()->load_last_ip_and_user_stat(m_hub_id, m_nick, l_message_count, l_last_ip_from_sql);
			setFlag(IS_SQL_NOT_FOUND, l_is_sql_not_found);
			if (!l_is_sql_not_found)
			{
				m_message_count.set(l_message_count);
				m_message_count.reset_dirty();
				if (m_last_ip_sql.set(l_last_ip_from_sql))
				{
					setFlag(CHANGE_IP);
				}
				m_last_ip_sql.reset_dirty();
			}
		}
	}
}
void User::initRatio(bool p_force /* = false */)
{
	initMesageCount();
	if (!m_last_ip_sql.get().is_unspecified() || p_force)
	{
		initRatioL(m_last_ip_sql.get());
	}
}

tstring User::getDownload()
{
	const auto l_value = getBytesDownload();
	if (l_value)
		return Util::formatBytesW(l_value);
	else
		return Util::emptyStringT;
}

tstring User::getUpload()
{
	const auto l_value = getBytesUpload();
	if (l_value)
		return Util::formatBytesW(l_value);
	else
		return Util::emptyStringT;
}

tstring User::getUDratio()
{
#ifdef FLYLINKDC_USE_RATIO_CS
	CFlyFastLock(m_ratio_cs);
#endif
	if (m_ratio_ptr && (m_ratio_ptr->get_download() || m_ratio_ptr->get_upload()))
		return Util::toStringW(m_ratio_ptr->get_download() ? ((double)m_ratio_ptr->get_upload() / (double)m_ratio_ptr->get_download()) : 0) +
		       L" (" + Util::formatBytesW(m_ratio_ptr->get_upload()) + _T('/') + Util::formatBytesW(m_ratio_ptr->get_download()) + L")";
	else
		return Util::emptyStringT;
}
#endif // FLYLINKDC_USE_LASTIP_AND_USER_RATIO

bool Identity::isTcpActive() const
{
	// [!] IRainman fix.
	if (user->isNMDC())
	{
		return !user->isSet(User::NMDC_FILES_PASSIVE);
	}
	else
	{
		return !getIp().is_unspecified() && user->isSet(User::TCP4);
	}
	// [~] IRainman fix.
}

bool Identity::isUdpActive() const
{
	// [!] IRainman fix.
	if (getIp().is_unspecified() || !getUdpPort())
	{
		return false;
	}
	else
	{
		return user->isSet(User::UDP4);
	}
	// [~] IRainman fix.
}
void Identity::setExtJSON(const string& p_ExtJSON)
{
#ifdef _DEBUG
	if (!m_lastExtJSON.empty())
	{
		if (m_lastExtJSON == p_ExtJSON)
		{
			LogManager::message("Duplicate ExtJSON = " + p_ExtJSON);
			//dcassert(0);
		}
	}
	m_lastExtJSON = p_ExtJSON;
#endif
	m_is_ext_json = true;
}

void Identity::getParams(StringMap& sm, const string& prefix, bool compatibility, bool dht) const
{
	PROFILE_THREAD_START();
	if (!dht)
	{
#define APPEND(cmd, val) sm[prefix + cmd] = val;
#define SKIP_EMPTY(cmd, val) { if (!val.empty()) { APPEND(cmd, val); } }
	
		APPEND("NI", getNick());
		SKIP_EMPTY("SID", getSIDString());
		const auto& l_cid = user->getCID().toBase32();
		APPEND("CID", l_cid);
		APPEND("SSshort", Util::formatBytes(getBytesShared()));
		SKIP_EMPTY("SU", getSupports());
// Справочные значения заберем через функцию get т.к. в мапе их нет
		SKIP_EMPTY("VE", getStringParam("VE"));
		SKIP_EMPTY("AP", getStringParam("AP"));
		if (compatibility)
		{
			if (prefix == "my")
			{
				sm["mynick"] = getNick();
				sm["mycid"] = l_cid;
			}
			else
			{
				sm["nick"] = getNick();
				sm["cid"] = l_cid;
				sm["ip"] = getIpAsString();
				sm["tag"] = getTag();
				sm["description"] = getDescription();
				sm["email"] = getEmail();
				sm["share"] = Util::toString(getBytesShared());
				const auto l_share = Util::formatBytes(getBytesShared());
				sm["shareshort"] = l_share;
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
				sm["realshareformat"] = Util::formatBytes(getRealBytesShared());
#else
				sm["realshareformat"] = l_share;
#endif
			}
		}
#undef APPEND
#undef SKIP_EMPTY
	}
	{
		CFlyFastLock(m_si_fcs);
		for (auto i = m_stringInfo.cbegin(); i != m_stringInfo.cend(); ++i)
		{
			sm[prefix + string((char*)(&i->first), 2)] = i->second;
		}
	}
}
string Identity::getTag() const
{
	if (isAppNameExists())
	{
		char l_tagItem[128];
		l_tagItem[0] = 0;
		string l_version;
		if (getDicAP())
		{
			l_version = getStringParam("AP") + " V:" + getStringParam("VE");
		}
		else
		{
			l_version = getStringParam("VE");
		}
		_snprintf(l_tagItem, sizeof(l_tagItem), "<%s,M:%c,H:%u/%u/%u,S:%u>", // http://mydc.ru/topic915.html?p=6721#entry6721 TODO - нужно ли вертать L:
		          //getApplication().c_str(),
		          l_version.c_str(),
		          isTcpActive() ? 'A' : 'P',
		          getHubNormal(),
		          getHubRegister(),
		          getHubOperator(),
		          getSlots());
		return l_tagItem;
	}
	else
	{
		return Util::emptyString;
	}
}
string Identity::getApplication() const
{
	const auto& application = getStringParam("AP");
	const auto& version = getStringParam("VE");
	if (version.empty())
	{
		return application;
	}
	if (application.empty())
	{
		// AP is an extension, so we can't guarantee that the other party supports it, so default to VE.
		return version;
	}
	return application + ' ' + version;
}

#ifdef _DEBUG

// #define FLYLINKDC_USE_TEST
#ifdef FLYLINKDC_USE_TEST
FastCriticalSection csTest;
#endif

#endif

#define ENABLE_CHECK_GET_SET_IN_IDENTITY
#ifdef ENABLE_CHECK_GET_SET_IN_IDENTITY
# define CHECK_GET_SET_COMMAND()\
	DISALLOW('T','A');\
	DISALLOW('C','L');\
	DISALLOW('N','I');\
	DISALLOW('A','W');\
	DISALLOW('B','O');\
	DISALLOW('S','U');\
	DISALLOW('U','4');\
	DISALLOW('U','6');\
	DISALLOW('I','4');\
	DISALLOW('S','S');\
	DISALLOW('C','T');\
	DISALLOW('S','I');\
	DISALLOW('U','S');\
	DISALLOW('S','L');\
	DISALLOW('H','N');\
	DISALLOW('H','R');\
	DISALLOW('H','O');\
	DISALLOW('F','C');\
	DISALLOW('S','T');\
	DISALLOW('R','S');\
	DISALLOW('S','S');\
	DISALLOW('F','D');\
	DISALLOW('F','S');\
	DISALLOW('S','F');\
	DISALLOW('R','S');\
	DISALLOW('C','M');\
	DISALLOW('D','S');\
	DISALLOW('I','D');\
	DISALLOW('L','O');\
	DISALLOW('P','K');\
	DISALLOW('O','P');\
	DISALLOW('U','4');\
	DISALLOW('U','6');\
	DISALLOW('C','O');\
	DISALLOW('W','O');


#else
# define CHECK_GET_SET_COMMAND()
#endif // ENABLE_CHECK_GET_SET_IN_IDENTITY

string Identity::getStringParam(const char* name) const // [!] IRainman fix.
{
	CHECK_GET_SET_COMMAND();
	
#ifdef FLYLINKDC_USE_TEST
	{
		static std::map<short, int> g_cnt;
		CFlyFastLock(ll(csTest);
		             auto& j = g_cnt[*(short*)name];
		             j++;
		             //if (j % 100 == 0)
		{
			LogManager::message("Identity::getStringParam = " + string(name) + " count = " + Util::toString(j));
			//dcdebug(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!! get[%s] = %d \n", name, j);
		}
	}
#endif
	
	switch (*(short*)name)
	{
		case TAG('A', 'P'):
		{
			const auto l_dic_value = getDicAP();
			if (l_dic_value > 0)
			{
				const string l_value = getDicVal(l_dic_value);
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
				CFlylinkDBManager::getInstance()->identity_get(name, l_value); // TODO вывести из лока g_rw_cs
#endif
				return l_value;
			}
			else
			{
				return Util::emptyString;
			}
		}
		case TAG('V', 'E'):
		{
			const auto l_dic_value = getDicVE();
			if (l_dic_value > 0)
			{
				const string l_value = getDicVal(l_dic_value);
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
				CFlylinkDBManager::getInstance()->identity_get(name, l_value); // TODO вывести из лока g_rw_cs
#endif
				return l_value;
			}
			else
			{
				return Util::emptyString;
			}
		}
		case TAG('E', 'M'):
		{
			if (!getNotEmptyStringBit(EM))
			{
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
				CFlylinkDBManager::getInstance()->identity_get(name, "");
#endif
				return Util::emptyString;
			}
			break;
		}
		case TAG('D', 'E'):
		{
			if (!getNotEmptyStringBit(DE))
			{
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
				CFlylinkDBManager::getInstance()->identity_get(name, "");
#endif
				return Util::emptyString;
			}
			break;
		}
	};
	
	{
		CFlyFastLock(m_si_fcs);
		const auto i = m_stringInfo.find(*(short*)name);
#ifdef FLYLINKDC_USE_PROFILER_CS
		l_lock.m_add_log_info = "[get] name = ";
		l_lock.m_add_log_info += string(name) + string(i == m_stringInfo.end() ? " [ not_found! ]" : "[ " + i->second + " ]" + " Nick = " + getNick());
#endif
		if (i != m_stringInfo.end())
		{
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
			CFlylinkDBManager::getInstance()->identity_get(name, iNickRule->second);
#endif
			return i->second;
		}
	}
	return Util::emptyString;
}
uint32_t Identity::mergeDicId(const string& p_val)
{
	if (p_val.empty())
		return 0;
	{
		CFlyReadLock(*g_rw_cs);
#ifdef FLYLINKDC_USE_PROFILER_CS
		l_lock.m_add_log_info = "p_val = ";
		l_lock.m_add_log_info += p_val + " Nick = " + getNick();
#endif
		auto l_find_ro = g_infoDicIndex.find(p_val);
		if (l_find_ro != g_infoDicIndex.end())
		{
			return l_find_ro->second;
		}
	}
	CFlyWriteLock(*g_rw_cs);
	auto l_find = g_infoDicIndex.insert(make_pair(p_val, uint16_t(g_infoDic.size() + 1)));
	if (l_find.second == true) // Новое значение в справочнике?
	{
		g_infoDic.push_back(p_val);   // Сохраняем индекс на строчку в справочнике
	}
	return l_find.first->second;
}

string Identity::getDicVal(uint16_t p_index)
{
	dcassert(p_index > 0 && p_index <= g_infoDic.size());
	if (p_index > 0)
	{
		CFlyReadLock(*g_rw_cs);
		return g_infoDic[p_index - 1];
	}
	else
	{
		return Util::emptyString;
	}
}

void Identity::setStringParam(const char* name, const string& val) // [!] IRainman fix.
{
	CHECK_GET_SET_COMMAND();
	
#ifdef FLYLINKDC_USE_TEST
	{
		static std::map<short, int> g_cnt;
		CFlyFastLock(csTest);
//	auto& i = g_cnt[*(short*)name];
//	i++;
//	if (i % 100 == 0)
//	{
//		dcdebug(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! set[%s] '%s' count = %d sizeof(*this) = %d\n", name, val.c_str(), i, sizeof(*this));
//	}
		static std::map<string, int> g_cnt_val;
		string l_key = string(name).substr(0, 2);
		auto& j = g_cnt_val[l_key + "~" + val];
		j++;
		if (l_key != "AP" && l_key != "EM" &&  l_key != "DE" &&  l_key != "VE")
		{
			//dcdebug(" !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! set[%s] '%s' count = %d sizeof(*this) = %d\n", name, val.c_str(), j, sizeof(*this));
			LogManager::message("Identity::setStringParam = " + string(name) + " val = " + val);
		}
	}
#endif
#ifdef FLYLINKDC_USE_GATHER_IDENTITY_STAT
	CFlylinkDBManager::getInstance()->identity_set(name, val);
#endif
	bool l_is_processing_stringInfo_map = true;
	if (val.empty())
	{
		switch (*(short*)name) // TODO: move to instantly method
		{
			case TAG('E', 'M'):
			{
				l_is_processing_stringInfo_map = getNotEmptyStringBit(EM); // TODO два раза пишем пустоту
				setNotEmptyStringBit(EM, false);
				break;
			}
			case TAG('D', 'E'):
			{
				l_is_processing_stringInfo_map = getNotEmptyStringBit(DE);  // TODO два раза пишем пустоту
				setNotEmptyStringBit(DE, false);
				break;
			}
		}
	}
	bool l_is_skip_string_map = false;
	if (l_is_processing_stringInfo_map)
	{
		{
			switch (*(short*)name)
			{
				case TAG('A', 'P'):
				{
					setDicAP(mergeDicId(val));
					l_is_skip_string_map = true;
					break;
				}
				case TAG('V', 'E'):
				{
					setDicVE(mergeDicId(val));
					l_is_skip_string_map = true;
					break;
				}
				case TAG('E', 'M'):
				{
					setNotEmptyStringBit(EM, !val.empty());
					break;
				}
				case TAG('D', 'E'):
				{
					setNotEmptyStringBit(DE, !val.empty());
					break;
				}
			}
		}
		if (l_is_skip_string_map == false)
		{
			CFlyFastLock(m_si_fcs);
#ifdef FLYLINKDC_USE_PROFILER_CS
			l_lock.m_add_log_info = "[set] name = ";
			l_lock.m_add_log_info += string(name) + string(val.empty() ? " val.empty()" : val);
#endif
			if (val.empty())
			{
				m_stringInfo.erase(*(short*)name);
			}
			else
			{
				m_stringInfo[*(short*)name] = val;
			}
		}
	}
	else
	{
		// Пропускаем модификацию m_stringInfo под локом
		// Но такого быть не должно ?
	}
}

Identity::~Identity()
{
}

void FavoriteUser::update(const OnlineUser& info) // !SMT!-fix
{
	// [!] FlylinkDC Team: please let me know if the assertions fail. IRainman.
	dcassert(!info.getIdentity().getNick().empty() || info.getClient().getHubUrl().empty());
	
	setNick(info.getIdentity().getNick());
	setUrl(info.getClient().getHubUrl());
}

void Identity::calcP2PGuard()
{
	if (!m_is_p2p_guard_calc)
	{
		if (getIp().to_ulong() && Util::isPrivateIp(getIp().to_ulong()) == false)
		{
			const string l_p2p_guard = CFlylinkDBManager::getInstance()->is_p2p_guard(getIp().to_ulong());
			setP2PGuard(l_p2p_guard);
			m_is_p2p_guard_calc = true;
		}
	}
}
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
unsigned char Identity::calcVirusType(bool p_force /* = false */)
{
	if (p_force)
		m_virus_type = 0;
	if (!(m_virus_type & Identity::VT_CALC_AVDB))
	{
		unsigned char l_virus_type = 0;
		string l_virus_path;
		if (const auto l_bs = getBytesShared())
		{
			l_virus_type = CFlylinkDBManager::getInstance()->calc_antivirus_flag(getNick(), getIpRAW(), l_bs, l_virus_path);
		}
		setVirusPath(l_virus_path);
		setVirusType(l_virus_type | Identity::VT_CALC_AVDB);
		// TODO - выкинуть из чата
	}
	return getVirusType();
}
string Identity::getVirusDesc() const
{
	string l_result;
	if (m_virus_type & ~VT_CALC_AVDB)
	{
		if (m_virus_type & VT_NICK)
		{
			l_result   += "+Nick";
		}
		if (m_virus_type & VT_SHARE)
		{
			l_result += "+Share";
		}
		if (m_virus_type & VT_IP)
		{
			l_result += "+IP";
		}
	}
	if (!l_result.empty())
	{
		l_result += getVirusPath();
		return l_result.substr(1);
	}
	else
	{
		return l_result;
	}
}
string Identity::getVirusDesc() const
{
	return Util::emptyString;
}
#endif

string Identity::setCheat(const ClientBase& c, const string& aCheatDescription, bool aBadClient)
{
	if (!c.isOp() || isOp())
	{
		return Util::emptyString;
	}
	
	PLAY_SOUND(SOUND_FAKERFILE);
	
	StringMap ucParams;
	getParams(ucParams, "user", true);
	string cheat = Util::formatParams(aCheatDescription, ucParams, false);
	
	setStringParam("CS", cheat);
	setFakeCardBit(BAD_CLIENT, aBadClient);
	
	string report = "*** " + STRING(USER) + ' ' + getNick() + " - " + cheat;
	return report;
}

tstring Identity::getHubs() const
{
	const auto l_hub_normal = getHubNormal();
	const auto l_hub_reg = getHubRegister();
	const auto l_hub_op = getHubOperator();
	if (l_hub_normal || l_hub_reg || l_hub_op)
	{
		tstring hubs;
		hubs.resize(64);
		hubs.resize(_snwprintf(&hubs[0], hubs.size(), _T("%u (%u/%u/%u)"),
		                       l_hub_normal + l_hub_reg + l_hub_op,
		                       l_hub_normal,
		                       l_hub_reg,
		                       l_hub_op));
		return hubs;
	}
	else
	{
		return Util::emptyStringT;
	}
}

string Identity::formatShareBytes(uint64_t p_bytes) // [+] IRainman
{
	return p_bytes ? Util::formatBytes(p_bytes) + " (" + Text::fromT(Util::formatExactSize(p_bytes)) + ")" : Util::emptyString;
}

string Identity::formatIpString(const string& value) // [+] IRainman IP.
{
	if (!value.empty())
	{
		auto ret = value + " (";
		const auto l_dns = Socket::getRemoteHost(value);
		if (!l_dns.empty())
		{
			ret +=  l_dns + " - ";
		}
		const auto l_location = Util::getIpCountry(value);
		ret += Text::fromT(l_location.getDescription()) + ")";
		return ret;
	}
	else
	{
		return Util::emptyString;
	}
};

string Identity::formatSpeedLimit(const uint32_t limit) // [+] IRainman
{
	return limit ? Util::formatBytes(limit) + '/' + STRING(S) : Util::emptyString;
}

void Identity::getReport(string& p_report) const
{
	p_report = " *** FlylinkDC user info ***\r\n";
	const string sid = getSIDString();
	{
		// [+] IRainman fix.
		auto appendBoolValue = [&](const string & name, const bool value, const string & iftrue, const string & iffalse)
		{
			p_report += "\t" + name + ": " + (value ? iftrue : iffalse) + "\r\n";
		};
		
		auto appendStringIfSetBool = [&](const string & str, const bool value)
		{
			if (value)
			{
				p_report += str + ' ';
			}
		};
		
		auto appendIfValueNotEmpty = [&](const string & name, const string & value)
		{
			if (!value.empty())
			{
				p_report += "\t" + name + ": " + value + "\r\n";
			}
		};
		
		auto appendIfValueSetInt = [&](const string & name, const unsigned int value)
		{
			if (value)
			{
				appendIfValueNotEmpty(name, Util::toString(value));
			}
		};
		
		auto appendIfValueSetSpeedLimit = [&](const string & name, const unsigned int value)
		{
			if (value)
			{
				appendIfValueNotEmpty(name, formatSpeedLimit(value));
			}
		};
		
		// TODO: translate
		const auto isNmdc = getUser()->isNMDC();
		
		appendIfValueNotEmpty(STRING(NICK), getNick());
		if (!isNmdc)
		{
			appendIfValueNotEmpty("Nicks", Util::toString(ClientManager::getNicks(user->getCID(), Util::emptyString)));
		}
		
		{
			CFlyFastLock(m_si_fcs);
			for (auto i = m_stringInfo.cbegin(); i != m_stringInfo.cend(); ++i)
			{
				auto name = string((char*)(&i->first), 2);
				const auto& value = i->second;
				// TODO: translate known tags and format values to something more readable
				switch (i->first)
				{
					case TAG('C', 'S'): // ok
						name = "Cheat description";
						break;
					case TAG('D', 'E'): // ok
						name = STRING(DESCRIPTION);
						break;
					case TAG('E', 'M'): // ok
						name = "E-mail";
						break;
					case TAG('K', 'P'): // ok
						name = "KeyPrint";
						break;
					case TAG('V', 'E'):
					case TAG('A', 'P'):
					case TAG('F', '1'):
					case TAG('F', '2'):
					case TAG('F', '3'):
					case TAG('F', '4'):
					case TAG('F', '5'):
						continue;
						break;
					default:
						name += " (unknown)";
				}
				appendIfValueNotEmpty(name, value);
			}
		}
		
		appendIfValueNotEmpty(STRING(HUBS), Text::fromT(getHubs()));
		if (!isNmdc)
		{
			appendIfValueNotEmpty("Hub names", Util::toString(ClientManager::getHubNames(user->getCID(), Util::emptyString)));
			appendIfValueNotEmpty("Hub addresses", Util::toString(ClientManager::getHubs(user->getCID(), Util::emptyString)));
		}
		
		p_report += "\t" "Client type" ": ";
		appendStringIfSetBool("Hub", isHub());
		appendStringIfSetBool("Bot", isBot());
		appendStringIfSetBool(STRING(OPERATOR), isOp());
		p_report += '(' + Util::toString(getClientType()) + ')';
		p_report += "\r\n";
		
		p_report += "\t";
		appendStringIfSetBool(STRING(AWAY), getUser()->isAway());
		appendStringIfSetBool(STRING(SERVER), getUser()->isServer());
		appendStringIfSetBool(STRING(FIREBALL), getUser()->isFireball());
		p_report += "\r\n";
		
		appendIfValueNotEmpty("Client ID", getUser()->getCID().toBase32());
		appendIfValueSetInt("Session ID", getSID());
		
		appendIfValueSetInt(STRING(SLOTS), getSlots());
		appendIfValueSetSpeedLimit(STRING(AVERAGE_UPLOAD), getLimit());
		appendIfValueSetSpeedLimit(isNmdc ? STRING(CONNECTION) : "Download speed", getDownloadSpeed());
		
		appendIfValueSetInt(STRING(SHARED_FILES), getSharedFiles());
		appendIfValueNotEmpty(STRING(SHARED) + " - reported", formatShareBytes(getBytesShared()));
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
		appendIfValueNotEmpty(STRING(SHARED) + " - real", formatShareBytes(getRealBytesShared()));
#endif
		
		appendIfValueSetInt("Fake check card", getFakeCard());
		appendIfValueSetInt("Connection Timeouts", getConnectionTimeouts());
		appendIfValueSetInt("Filelist disconnects", getFileListDisconnects());
		
		if (isNmdc)
		{
			appendIfValueNotEmpty("NMDC status", NmdcSupports::getStatus(*this));
			appendBoolValue("Files mode", getUser()->isSet(User::NMDC_FILES_PASSIVE), "Passive", "Active");
			appendBoolValue("Search mode", getUser()->isSet(User::NMDC_SEARCH_PASSIVE), "Passive", "Active");
		}
		appendIfValueNotEmpty("Known supports", getSupports());
		
		appendIfValueNotEmpty("IPv4 Address", formatIpString(getIpAsString()));
		// appendIfValueNotEmpty("IPv6 Address", formatIpString(getIp())); TODO
		// [~] IRainman fix.
		
		// Справочные значения заберем через функцию get т.к. в мапе их нет
		appendIfValueNotEmpty("DC client", getStringParam("AP"));
		appendIfValueNotEmpty("Client version", getStringParam("VE"));
		
		appendIfValueNotEmpty("P2P Guard", getP2PGuard());
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		appendIfValueNotEmpty("Antivirus database", getVirusDesc());
#endif
		appendIfValueNotEmpty("Support info", getExtJSONSupportInfo());
		appendIfValueNotEmpty("Gender", Text::fromT(getGenderTypeAsString()));
		
		appendIfValueNotEmpty("Country", getFlyHubCountry());
		appendIfValueNotEmpty("City", getFlyHubCity());
		appendIfValueNotEmpty("ISP", getFlyHubISP());
		
		appendIfValueNotEmpty("Count files", getExtJSONCountFilesAsText());
		appendIfValueNotEmpty("Last share", getExtJSONLastSharedDateAsText());
		appendIfValueNotEmpty("SQLite DB size", getExtJSONSQLiteDBSizeAsText());
		appendIfValueNotEmpty("Queue info:", getExtJSONQueueFilesText());
		appendIfValueNotEmpty("Start/stop core:", getExtJSONTimesStartCoreText());
		
	}
}

string Identity::getSupports() const // [+] IRainman fix.
{
	string tmp = UcSupports::getSupports(*this);
	/*
	if (getUser()->isNMDC())
	{
	    tmp += NmdcSupports::getSupports(*this);
	}
	else
	*/
	{
		tmp += AdcSupports::getSupports(*this);
	}
	return tmp;
}

string Identity::getIpAsString() const
{
	if (!m_ip.is_unspecified())
		return m_ip.to_string();
	else
	{
		if (isUseIP6())
		{
			return getIP6();
		}
		else
		{
			return getUser()->getIPAsString();
		}
	}
}
void Identity::setIp(const string& p_ip) // "I4"
{
	boost::system::error_code ec;
	m_ip = boost::asio::ip::address_v4::from_string(p_ip, ec);
	dcassert(!ec);
	if (!ec)
	{
		getUser()->setIP(m_ip, true);
	}
	else
	{
#ifdef FLYLINKDC_BETA
		const string l_message = "Identity::setIP Error IP = " + p_ip;
		LogManager::message(l_message);
		CFlyServerJSON::pushError(27, l_message);
#endif
	}
	change(CHANGES_IP | CHANGES_GEO_LOCATION);
}
bool Identity::isFantomIP() const
{
	if (m_ip.is_unspecified())
	{
		if (isUseIP6())
			return false;
		else
			return true;
	}
	return false;
}

#ifdef IRAINMAN_ENABLE_AUTO_BAN
User::DefinedAutoBanFlags User::hasAutoBan(Client *p_Client, const bool p_is_favorite)
{
	// Check exclusion first
	bool bForceAllow = BOOLSETTING(PROT_FAVS) && p_is_favorite;
	if (!bForceAllow && !UserManager::protectedUserListEmpty())
	{
		const string l_Nick = getLastNick();
		bForceAllow = !l_Nick.empty() && !UserManager::isInProtectedUserList(l_Nick); // TODO - часто toLower
	}
	int iBan = BAN_NONE;
	if (!bForceAllow)
	{
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		if (getHubID() != 0) // Value HubID is zero for himself, do not check your user.
#endif
		{
			const int l_limit = getLimit();
			const int l_slots = getSlots();
			
			const int iSettingBanSlotMax = SETTING(BAN_SLOTS_H);
			const int iSettingBanSlotMin = SETTING(BAN_SLOTS);
			const int iSettingLimit = SETTING(BAN_LIMIT);
			const int iSettingShare = SETTING(BAN_SHARE);
			
			// [+] IRainman
			if (m_support_slots == FLY_SUPPORT_SLOTS_FIRST)//[+]PPA optimize autoban
			{
				bool HubDoenstSupportSlot = isNMDC();
				if (HubDoenstSupportSlot)
				{
					if (p_Client)
					{
						HubDoenstSupportSlot = p_Client->hubIsNotSupportSlot();
					}
				}
				m_support_slots = HubDoenstSupportSlot ? FLY_NSUPPORT_SLOTS : FLY_SUPPORT_SLOTS;
			}
			// [~] IRainman
			
			if ((m_support_slots == FLY_SUPPORT_SLOTS || l_slots) && iSettingBanSlotMin && l_slots < iSettingBanSlotMin)
				iBan |= BAN_BY_MIN_SLOT;
				
			if (iSettingBanSlotMax && l_slots > iSettingBanSlotMax)
				iBan |= BAN_BY_MAX_SLOT;
				
			if (iSettingShare && static_cast<int>(getBytesShared() / uint64_t(1024 * 1024 * 1024)) < iSettingShare)
				iBan |= BAN_BY_SHARE;
				
			// Skip users with limitation turned off
			if (iSettingLimit && l_limit && l_limit < iSettingLimit)
				iBan |= BAN_BY_LIMIT;
		}
	}
	return static_cast<DefinedAutoBanFlags>(iBan);
}
#endif // IRAINMAN_ENABLE_AUTO_BAN

bool OnlineUser::isDHT() const
{
	return m_client.isDHT();
}
#ifdef FLYLINKDC_USE_CHECK_CHANGE_TAG
bool OnlineUser::isTagUpdate(const string& p_tag, bool& p_is_version_change)
{
	p_is_version_change = false;
	if (p_tag != m_tag)
	{
		if (!m_tag.empty())
		{
			auto l_find_sep = p_tag.find(',');
			if (l_find_sep != string::npos)
			{
				if (m_tag.size() > l_find_sep && m_tag[l_find_sep] == ',')
				{
					// Сравним приложение и версию - если не изменились - упростим парсинг тэга позже
					if (m_tag.compare(0, l_find_sep, p_tag, 0, l_find_sep) == 0)
						p_is_version_change = false;
					else
						p_is_version_change = true;
				}
			}
		}
		else
		{
			p_is_version_change = true; // Первый раз
		}
		m_tag = p_tag;
		return true;
	}
	else
	{
#ifdef _DEBUG
		LogManager::message("OnlineUser::isTagUpdate - duplicate tag = " + p_tag + " Hub =" + getClient().getHubUrl() + " user = " + getUser()->getLastNick());
#endif
		return false;
	}
}
#endif // FLYLINKDC_USE_CHECK_CHANGE_TAG


//[~]FlylinkDC
/**
 * @file
 * $Id: User.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
