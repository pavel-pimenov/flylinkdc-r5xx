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
#include <boost/algorithm/string.hpp>

#include "NmdcHub.h"
#include "ShareManager.h"
#include "CryptoManager.h"
#include "UserCommand.h"
#include "DebugManager.h"
#include "QueueManager.h"
#include "ThrottleManager.h"
#include "StringTokenizer.h"
#include "MappingManager.h"
#include "CompatibilityManager.h"

#include "../FlyFeatures/flyServer.h"
#include "ZenLib/Format/Http/Http_Utils.h"
#include "../jsoncpp/include/json/reader.h"

CFlyUnknownCommand NmdcHub::g_unknown_command;
CFlyUnknownCommandArray NmdcHub::g_unknown_command_array;
FastCriticalSection NmdcHub::g_unknown_cs;
uint8_t NmdcHub::g_version_fly_info = 33;

NmdcHub::NmdcHub(const string& aHubURL, bool secure, bool p_is_auto_connect) :
	Client(aHubURL, '|', false, p_is_auto_connect),
	m_supportFlags(0),
	m_modeChar(0),
	m_version_fly_info(0),
	m_lastBytesShared(0),
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	m_hubSupportsSlots(false),
#endif // IRAINMAN_ENABLE_AUTO_BAN
	m_lastUpdate(0)
{
	AutodetectInit();
	// [+] IRainman fix.
	m_myOnlineUser->getUser()->setFlag(User::NMDC);
	m_hubOnlineUser->getUser()->setFlag(User::NMDC);
	// [~] IRainman fix.
}

NmdcHub::~NmdcHub()
{
#ifdef FLYLINKDC_USE_EXT_JSON
	//dcassert(m_ext_json_deferred.empty());
#endif
	clearUsers();
}


#define checkstate() if(state != STATE_NORMAL) return

void NmdcHub::disconnect(bool p_graceless)
{
	Client::disconnect(p_graceless);
	clearUsers();
	m_delay_search.clear();
}

void NmdcHub::connect(const OnlineUser& p_user, const string& p_token, bool p_is_force_passive)
{
	m_delay_search.clear();
	checkstate();
	dcdebug("NmdcHub::connect %s\n", p_user.getIdentity().getNick().c_str());
	if (p_is_force_passive == false && isActive())
	{
		connectToMe(p_user);
	}
	else
	{
		revConnectToMe(p_user);
	}
}
void NmdcHub::resetAntivirusInfo()
{
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	CFlyReadLock(*m_cs);
	for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
	{
		i->second->getIdentity().resetAntivirusInfo();
	}
#else
	dcassert(0);
#endif
}
void NmdcHub::refreshUserList(bool refreshOnly)
{
	if (refreshOnly)
	{
		OnlineUserList v;
		v.reserve(m_users.size());
		{
			// [!] IRainman fix potential deadlock.
			CFlyReadLock(*m_cs);
			for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
			{
				v.push_back(i->second);
			}
		}
		fire_user_updated(v);
	}
	else
	{
		clearUsers();
		getNickList();
	}
}

#if 0

OnlineUserPtr NmdcHub::getUser(const string& aNick, bool p_hub, bool p_first_load)
{
	CFlyFastLock(cs);
	OnlineUser * l_ou_ptr;
	bool l_is_CID_User = nullptr;
	if (p_hub)
	{
		l_ou_ptr = getHubOnlineUser().get();
	}
	else if (p_first_load == false && aNick == getMyNick())
	{
		l_ou_ptr = getMyOnlineUser().get();
	}
	else
	{
		l_is_CID_User = true;
		UserPtr p = ClientManager::getUser(aNick, getHubUrl()
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		                                   , getHubID()
#endif
		                                   , p_first_load
		                                  );
		l_ou_ptr = new OnlineUser(p, *this, 0);
	}
	auto l_find = m_users.insert(make_pair(aNick, l_ou_ptr));
	if (l_find.second == false) // Не прошла вставка т.к. такой ник уже есть в мапе?
	{
		return l_find.first->second;
	}
	else
	{
		// Новый элемент
		l_find.first->second->inc();
		if (p_hub)
		{
			l_find.first->second->getIdentity().setNick(aNick);
		}
		else
		{
			l_find.first->second->getIdentity().setNickFast(aNick);
		}
	}
	//dcassert(!l_find.first->second->getUser()->getCID().isZero());
	if (l_is_CID_User)
	{
		ClientManager::getInstance()->putOnline(l_find.first->second);
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		UserManager::checkUser(ou);
#endif
	}
	return l_find.first->second;
}


#endif
OnlineUserPtr NmdcHub::getUser(const string& aNick, bool p_hub, bool p_first_load)
{

	OnlineUserPtr ou;
	//if (!p_first_load) пока возникают дубли - почему не понятно
	// http://www.flickr.com/photos/96019675@N02/11474777653/
	{
		ou = findUser(aNick); // !SMT!-S
		if (ou)
		{
			return ou; // !SMT!-S
		}
	}
	
	// [+] IRainman fix.
	auto cm = ClientManager::getInstance();
	if (p_hub)
	{
		{
			CFlyWriteLock(*m_cs);
			ou = m_users.insert(make_pair(aNick, getHubOnlineUser().get())).first->second;
			ou->inc();
		}
		ou->getIdentity().setNick(aNick);
	}
	else if (aNick == getMyNick())
	{
		CFlyWriteLock(*m_cs);
		ou = m_users.insert(make_pair(aNick, getMyOnlineUser().get())).first->second;
		ou->inc();
	}
	// [~] IRainman fix.
	else
	{
		UserPtr p = cm->getUser(aNick, getHubUrl()
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		                        , getHubID()
#endif
		                        , p_first_load
		                       );
		OnlineUser* newUser = new OnlineUser(p, *this, 0);
		{
			CFlyWriteLock(*m_cs);
			ou = m_users.insert(make_pair(aNick, newUser)).first->second; //2012-06-09_18-19-42_JBOQDRXR35PEW7OJOPLNPXJSQDETX4IUV3SHOHA_DEF32407_crash-stack-r501-x64-build-10294.dmp
			ou->inc();
		}
		ou->getIdentity().setNick(aNick);
	}
	
	if (!ou->getUser()->getCID().isZero()) // [+] IRainman fix.
	{
		cm->putOnline(ou, is_all_my_info_loaded());
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		UserManager::checkUser(ou);
#endif
	}
	return ou;
}
void NmdcHub::supports(const StringList& feat)
{
	const string x = Util::toSupportsCommand(feat);
	send(x);
}

OnlineUserPtr NmdcHub::findUser(const string& aNick) const
{
	CFlyReadLock(*m_cs);
	const auto& i = m_users.find(aNick); 
#ifdef FLYLINKDC_USE_PROFILER_CS
	l_lock.m_add_log_info = " User = " + aNick;
#endif
	return i == m_users.end() ? nullptr : i->second; // 2012-04-29_13-38-26_EJMPFXUHZAKEQON7Y6X7EIKZVS3S3GMF43CWO3Y_C95F3090_crash-stack-r501-build-9869.dmp
}

void NmdcHub::putUser(const string& aNick)
{
	OnlineUserPtr ou;
	{
		CFlyWriteLock(*m_cs);
#ifdef FLYLINKDC_USE_EXT_JSON
		//m_ext_json_deferred.erase(aNick);
#endif
		const auto& i = m_users.find(aNick);
		if (i == m_users.end())
			return;
		ou = i->second;
		m_users.erase(i);
		decBytesSharedL(ou->getIdentity());
		{
			CFlyFastLock(m_cs_virus);
			m_virus_nick.erase(aNick);
		}
	}
	
	if (!ou->getUser()->getCID().isZero()) // [+] IRainman fix.
	{
		ClientManager::getInstance()->putOffline(ou); // [2] https://www.box.net/shared/7b796492a460fe528961
	}
	
	fly_fire2(ClientListener::UserRemoved(), this, ou); // [+] IRainman fix.
	ou->dec();// [!] IRainman fix memoryleak
}

void NmdcHub::clearUsers()
{
	if (ClientManager::isShutdown())
	{
		CFlyWriteLock(*m_cs);
		for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
		{
			i->second->dec();
		}
		m_users.clear();
		clearAvailableBytesL();
	}
	else
	{
		NickMap u2;
		{
			CFlyWriteLock(*m_cs);
			u2.swap(m_users);
#ifdef FLYLINKDC_USE_EXT_JSON
			//m_ext_json_deferred.clear();
#endif
			clearAvailableBytesL();
		}
		for (auto i = u2.cbegin(); i != u2.cend(); ++i)
		{
			if (!i->second->getUser()->getCID().isZero()) // [+] IRainman fix.
			{
				ClientManager::getInstance()->putOffline(i->second); // http://code.google.com/p/flylinkdc/issues/detail?id=1064
			}
			// Варианты
			// - скармливать юзеров массивом
			// - Держать юзеров в нескольких контейнерах для каждого хаба отдельно
			// - проработать команду на убивание всей мапы сразу без поиска
			i->second->getIdentity().setBytesShared(0);
			i->second->dec();// [!] IRainman fix memoryleak
		}
	}
}
//==========================================================================================
void NmdcHub::updateFromTag(Identity& id, const string & tag) // [!] IRainman opt.
{
	const StringTokenizer<string> tok(tag, ',', 4); // TODO - убрать разбор токенов. сделать простое сканирование в цикле в поиске запятых
	string::size_type j;
	id.setLimit(0);
	for (auto i = tok.getTokens().cbegin(); i != tok.getTokens().cend(); ++i)
	{
		if (i->length() < 2)
		{
			continue;
		}
		// [!] IRainman opt: first use the compare, and only then to find.
		else if (i->compare(0, 2, "H:", 2) == 0)
		{
			int u[3] = {0};
			const auto l_items = sscanf_s(i->c_str() + 2, "%u/%u/%u", &u[0], &u[1], &u[2]);
			if (l_items != 3)
				continue;
			id.setHubNormal(u[0]);
			id.setHubRegister(u[1]);
			id.setHubOperator(u[2]);
		}
		else if (i->compare(0, 2, "S:", 2) == 0)
		{
			const uint16_t slots = Util::toInt(i->c_str() + 2);
			id.setSlots(slots);
#ifdef IRAINMAN_ENABLE_AUTO_BAN
			if (slots > 0)
			{
				m_hubSupportsSlots = true;
			}
#endif // IRAINMAN_ENABLE_AUTO_BAN
		}
		else if (i->compare(0, 2, "M:", 2) == 0)
		{
			if (i->size() == 3)
			{
				// [!] IRainman fix.
				if ((*i)[2] == 'A')
				{
					id.getUser()->unsetFlag(User::NMDC_FILES_PASSIVE);
					id.getUser()->unsetFlag(User::NMDC_SEARCH_PASSIVE);
				}
				else
				{
					id.getUser()->setFlag(User::NMDC_FILES_PASSIVE);
					id.getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
				}
				// [~] IRainman fix.
			}
		}
		else if ((j = i->find("V:")) != string::npos ||
		         (j = i->find("v:")) != string::npos // [+] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=1234
		        )
		{
			//dcassert(j > 1);
			if (j > 1)
				id.setStringParam("AP", i->substr(0, j - 1));
			id.setStringParam("VE", i->substr(j + 2));
		}
		else if ((j = i->find("L:")) != string::npos)
		{
			const uint32_t l_limit = Util::toInt(i->c_str() + j + 2);
			id.setLimit(l_limit * 1024);
		}
		else if ((j = i->find(' ')) != string::npos)
		{
			//dcassert(j > 1);
			if (j > 1)
				id.setStringParam("AP", i->substr(0, j - 1));
			id.setStringParam("VE", i->substr(j + 1));
		}
		else if ((j = i->find("++")) != string::npos)
		{
			id.setStringParam("AP", *i);
		}
		// [+] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=1234
		else if (i->compare(0, 2, "O:", 2) == 0)
		{
			// [?] TODO http://nmdc.sourceforge.net/NMDC.html#_tag
		}
		else if (i->compare(0, 2, "C:", 2) == 0)
		{
			// http://dchublist.ru/forum/viewtopic.php?p=24035#p24035
		}
		// [~] IRainman fix.
#ifdef FLYLINKDC_COLLECT_UNKNOWN_TAG
		else
		{
			CFlyFastLock(NmdcSupports::g_debugCsUnknownNmdcTagParam);
			NmdcSupports::g_debugUnknownNmdcTagParam[tag]++;
			// dcassert(0);
			// TODO - сброс ошибочных тэгов в качестве статы?
		}
#endif // FLYLINKDC_COLLECT_UNKNOWN_TAG
		// [~] IRainman fix.
	}
	/// @todo Think about this
	// [-] id.setStringParam("TA", '<' + tag + '>'); [-] IRainman opt.
}
//=================================================================================================================
void NmdcHub::NmdcSearch(const SearchParam& p_search_param)
{
	ClientManagerListener::SearchReply l_re = ClientManagerListener::SEARCH_MISS; // !SMT!-S
	SearchResultList l;
	dcassert(p_search_param.m_max_results > 0);
	dcassert(p_search_param.m_client);
	if (ClientManager::isShutdown())
		return;
#ifdef PPA_USE_HIGH_LOAD_FOR_SEARCH_ENGINE_IN_DEBUG
	ShareManager::getInstance()->search(l, p_search_param);
#else
	ShareManager::getInstance()->search(l, p_search_param);
#endif
	if (!l.empty())
	{
		l_re = ClientManagerListener::SEARCH_HIT;
		if (p_search_param.m_is_passive)
		{
			const string l_name = p_search_param.m_seeker.substr(4); //-V112
			// Good, we have a passive seeker, those are easier...
			string str;
			for (auto i = l.cbegin(); i != l.cend(); ++i)
			{
				const SearchResult& sr = *i;
				str += sr.toSR(*this);
				str[str.length() - 1] = 5;
//#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
//				str += name;
//#else
				str += Text::fromUtf8(l_name, getEncoding());
//#endif
				str += '|';
			}
			
			if (!str.empty())
			{
				send(str);
			}
		}
		else
		{
			try
			{
				Socket udp;
				for (auto i = l.cbegin(); i != l.cend(); ++i)
				{
					const string l_sr = i->toSR(*this);
					if (ConnectionManager::checkDuplicateSearchFile(l_sr))
					{
#ifdef FLYLINKDC_USE_COLLECT_STAT
						CFlylinkDBManager::getInstance()->push_event_statistic("search-a-skip-dup-file-search [NmdcHub::NmdcSearch]", "File", param, getIpAsString(), "", getHubUrlAndIP(), l_tth);
#endif
						COMMAND_DEBUG("[~][0]$SR [SkipUDP-File] " + l_sr, DebugTask::HUB_IN, getIpPort());
					}
					else
					{
						sendUDPSR(udp, p_search_param.m_seeker, l_sr, this);
					}
				}
			}
			catch (Exception& e)
			{
#ifdef _DEBUG
				LogManager::message("ClientManager::on(NmdcSearch, Search caught error= " + e.getError());
#endif
				dcdebug("Search caught error = %s\n", + e.getError().c_str());
			}
		}
	}
	else
	{
		if (!p_search_param.m_is_passive)
		{
			if (NmdcPartialSearch(p_search_param))
			{
				l_re = ClientManagerListener::SEARCH_PARTIAL_HIT;
			}
		}
	}
	ClientManager::getInstance()->fireIncomingSearch(p_search_param.m_seeker, p_search_param.m_filter, l_re);
}
//=================================================================================================
void NmdcHub::sendUDPSR(Socket& p_udp, const string& p_seeker, const string& p_sr, const Client* p_client) // const CFlySearchItem& p_result, const Client* p_client
{
	try
	{
		string l_ip;
		uint16_t l_port = 412;
		Util::parseIpPort(p_seeker, l_ip, l_port);
		//dcassert(l_ip == Socket::resolve(l_ip));
		p_udp.writeTo(l_ip, l_port, p_sr);
		COMMAND_DEBUG("[Active-Search]" + p_sr, DebugTask::CLIENT_OUT, l_ip + ':' + Util::toString(l_port));
#ifdef FLYLINKDC_USE_COLLECT_STAT
		const string l_sr = *p_result.m_toSRCommand;
		string l_tth;
		const auto l_tth_pos = l_sr.find("TTH:");
		if (l_tth_pos != string::npos)
			l_tth = l_sr.substr(l_tth_pos + 4, 39);
		CFlylinkDBManager::getInstance()->push_event_statistic("$SR", "UDP-write-dc", l_sr, ip, Util::toString(port), p_client->getHubUrl(), l_tth);
#endif
	}
	catch (Exception& e)
	{
#ifdef _DEBUG
		LogManager::message("ClientManager::on(NmdcSearch, Search caught error= " + e.getError());
#endif
		dcdebug("Search caught error = %s\n", + e.getError().c_str());
	}
}
//==========================================================================================
string NmdcHub::calcExternalIP() const
{
	string l_result;
	if (getFavIp().empty())
		l_result = getLocalIp();
	else
		l_result = getFavIp();
	l_result += ':' + SearchManager::getSearchPort();
#ifdef _DEBUG
	// LogManager::message("NmdcHub::calcExternalIP() = " + l_result);
#endif
	return l_result;
}
//==========================================================================================
void NmdcHub::searchParse(const string& param, bool p_is_passive)
{
	if (state != STATE_NORMAL
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	        || getHideShare()
#endif
	   )
	{
		return;
	}
	SearchParam l_search_param;
	l_search_param.m_raw_search = param;
	string::size_type i = 0;
	string::size_type j = param.find(' ', i);
	l_search_param.m_query_pos = j;
	if (j == string::npos || i == j)
	{
#ifdef FLYLINKDC_BETA
		LogManager::message("Error [part 1] $Search command = " + param + " Hub: " + getHubUrl());
#endif
		return;
	}
	l_search_param.m_seeker = param.substr(i, j - i);
	
#ifdef FLYLINKDC_USE_COLLECT_STAT
	string l_tth;
	const auto l_tth_pos = param.find("?9?TTH:", i);
	if (l_tth_pos != string::npos)
		l_tth = param.c_str() + l_tth_pos + 7;
	if (!l_tth.empty())
		CFlylinkDBManager::getInstance()->push_event_statistic(p_is_passive ? "search-p" : "search-a", "TTH", param, getIpAsString(), "", getHubUrlAndIP(), l_tth);
	else
		CFlylinkDBManager::getInstance()->push_event_statistic(p_is_passive ? "search-p" : "search-a", "Others", param, getIpAsString(), "", getHubUrlAndIP());
#endif
		
	if (!p_is_passive)
	{
		const auto m = l_search_param.m_seeker.rfind(':');
		if (m != string::npos)
		{
			const auto k = param.find("?9?TTH:", m); // Если идет запрос по TTH - пропускаем без проверки
			if (k == string::npos)
			{
				if (ConnectionManager::getInstance()->checkIpFlood(l_search_param.m_seeker.substr(0, m), Util::toInt(l_search_param.m_seeker.substr(m + 1)), getIp(), param, getHubUrlAndIP()))
				{
					return; // http://dchublist.ru/forum/viewtopic.php?f=6&t=1028&start=150
				}
			}
		}
	}
	// Filter own searches
	if (p_is_passive)
	{
		// [!] PPA fix
		// seeker в начале может не содержать "Hub:" - падаем
		// https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=64297
		// https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=63507
		const auto& myNick = getMyNick();
		dcassert(l_search_param.m_seeker.size() > 4);
		if (l_search_param.m_seeker.size() <= 4)
			return;
		if (l_search_param.m_seeker.compare(4, myNick.size(), myNick) == 0) // [!] IRainman fix: strongly check
		{
			return;
		}
	}
	else if (isActive())
	{
		if (!SearchManager::isSearchPortValid())
		{
			return;
		}
		if (l_search_param.m_seeker == calcExternalIP())
		{
			return;
		}
	}
	i = j + 1;
	if (param.size() < (i + 4))
	{
#ifdef FLYLINKDC_BETA
		LogManager::message("Error [part 2] $Search command = " + param + " Hub: " + getHubUrl());
#endif
		return;
	}
	if (param[i] == 'F')
	{
		l_search_param.m_size_mode = Search::SIZE_DONTCARE;
	}
	else if (param[i + 2] == 'F')
	{
		l_search_param.m_size_mode = Search::SIZE_ATLEAST;
	}
	else
	{
		l_search_param.m_size_mode = Search::SIZE_ATMOST;
	}
	i += 4;
	j = param.find('?', i);
	if (j == string::npos || i == j)
	{
#ifdef FLYLINKDC_BETA
		LogManager::message("Error [part 4] $Search command = " + param + " Hub: " + getHubUrl());
#endif
		return;
	}
	if ((j - i) == 1 && param[i] == '0')
	{
		l_search_param.m_size = 0;
	}
	else
	{
		l_search_param.m_size = _atoi64(param.c_str() + i);
	}
	i = j + 1;
	j = param.find('?', i);
	if (j == string::npos || i == j)
	{
#ifdef FLYLINKDC_BETA
		// LogManager::message("Error [part 5] $Search command = " + param + " Hub: " + getHubUrl());
#endif
		return;
	}
	const int l_type_search = atoi(param.c_str() + i);
	l_search_param.m_file_type = Search::TypeModes(l_type_search - 1);
	i = j + 1;
	
	if (l_search_param.m_file_type == Search::TYPE_TTH && (param.size() - i) == 39 + 4) // 39+4 = strlen("TTH:VGUKIR6NLP6LQB7P5NDCZGUSR3MFHRMRO3VJLWY")
	{
		l_search_param.m_filter = param.substr(i);
	}
	else
	{
		l_search_param.m_filter = unescape(param.substr(i));
	}
	//dcassert(!l_search_param.m_filter.empty());
	if (!l_search_param.m_filter.empty())
	{
		if (p_is_passive)
		{
			OnlineUserPtr u = findUser(l_search_param.m_seeker.substr(4));
			
			if (!u)
				return;
				
			// [!] IRainman fix
			if (!u->getUser()->isSet(User::NMDC_SEARCH_PASSIVE))
			{
				u->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
				updated(u);
			}
			// [~] IRainman fix.
			
			// ignore if we or remote client don't support NAT traversal in passive mode although many NMDC hubs won't send us passive if we're in passive too, so just in case...
			if (!isActive() && (!u->getUser()->isSet(User::NAT0) || !BOOLSETTING(ALLOW_NAT_TRAVERSAL)))
			{
#ifdef FLYLINKDC_BETA
				//LogManager::message("Error [part 7] $Search command = " + param + " Hub: " + getHubUrl() +
				//                                   "ignore if we or remote client don't support NAT traversal in passive mode although many NMDC hubs won't send us passive if we're in passive too, so just in case..."
				//                                  );
#endif
				return;
			}
		}
		l_search_param.init(this, p_is_passive);
		NmdcSearch(l_search_param);
		// Dead lock 2 https://code.google.com/p/flylinkdc/issues/detail?id=1428
	}
	else
	{
#ifdef FLYLINKDC_BETA
		// LogManager::message("Error [part 6] $Search command = " + param + " Hub: " + getHubUrl());
#endif
	}
}
//==========================================================================================
void NmdcHub::revConnectToMeParse(const string& param)
{
	if (state != STATE_NORMAL)
	{
		return;
	}
	
	string::size_type j = param.find(' ');
	if (j == string::npos)
	{
		return;
	}
	
	OnlineUserPtr u = findUser(param.substr(0, j));
	if (!u)
		return;
		
	if (isActive())
	{
		connectToMe(*u);
	}
	else if (BOOLSETTING(ALLOW_NAT_TRAVERSAL) && u->getUser()->isSet(User::NAT0))
	{
		bool secure = CryptoManager::getInstance()->TLSOk() && u->getUser()->isSet(User::TLS);
		// NMDC v2.205 supports "$ConnectToMe sender_nick remote_nick ip:port", but many NMDC hubsofts block it
		// sender_nick at the end should work at least in most used hubsofts
		if (m_client_sock->getLocalPort() == 0)
		{
			LogManager::message("Error [3] $ConnectToMe port = 0 : ");
			CFlyServerJSON::pushError(22, "Error [3] $ConnectToMe port = 0 :");
		}
		else
		{
			send("$ConnectToMe " + fromUtf8(u->getIdentity().getNick()) + ' ' + getLocalIp() + ':' + Util::toString(m_client_sock->getLocalPort()) + (secure ? "NS " : "N ") + fromUtf8(getMyNick()) + '|');
		}
	}
	else
	{
		// [!] IRainman fix.
		if (!u->getUser()->isSet(User::NMDC_FILES_PASSIVE))
		{
			// [!] IRainman fix: You can not reset the user to flag as if we are passive, not him!
			// [-] u->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
			// [-] Notify the user that we're passive too...
			// [-] updated(u); [-]
			revConnectToMe(*u);
			
			return;
		}
		// [~] IRainman fix.
	}
	
}
//==========================================================================================
void NmdcHub::connectToMeParse(const string& param)
{
	string senderNick;
	string port;
	string server;
	while (true)
	{
		if (state != STATE_NORMAL)
		{
			break;
		}
		string::size_type i = param.find(' ');
		string::size_type j;
		if (i == string::npos || (i + 1) >= param.size())
		{
			break;
		}
		i++;
		j = param.find(':', i);
		if (j == string::npos)
		{
			break;
		}
		server = param.substr(i, j - i);
		if (j + 1 >= param.size())
		{
			break;
		}
		
		i = param.find(' ', j + 1);
		if (i == string::npos)
		{
			port = param.substr(j + 1);
		}
		else
		{
			senderNick = param.substr(i + 1);
			port = param.substr(j + 1, i - j - 1);
		}
		
		bool secure = false;
		if (port[port.size() - 1] == 'S')
		{
			port.erase(port.size() - 1);
			if (CryptoManager::getInstance()->TLSOk())
			{
				secure = true;
			}
		}
		
		if (BOOLSETTING(ALLOW_NAT_TRAVERSAL))
		{
			if (port[port.size() - 1] == 'N')
			{
				if (senderNick.empty())
					break;
					
				port.erase(port.size() - 1);
				
				// Trigger connection attempt sequence locally ...
				ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), m_client_sock->getLocalPort(),
				                                              BufferedSocket::NAT_CLIENT, getMyNick(), getHubUrl(),
				                                              getEncoding(),
				                                              secure);
				// ... and signal other client to do likewise.
				if (m_client_sock->getLocalPort() == 0)
				{
					LogManager::message("Error [2] $ConnectToMe port = 0 : ");
					CFlyServerJSON::pushError(22, "Error [2] $ConnectToMe port = 0 :");
				}
				else
				{
					send("$ConnectToMe " + senderNick + ' ' + getLocalIp() + ':' + Util::toString(m_client_sock->getLocalPort()) + (secure ? "RS|" : "R|"));
				}
				break;
			}
			else if (port[port.size() - 1] == 'R')
			{
				port.erase(port.size() - 1);
				
				// Trigger connection attempt sequence locally
				ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), m_client_sock->getLocalPort(),
				                                              BufferedSocket::NAT_SERVER, getMyNick(), getHubUrl(),
				                                              getEncoding(),
				                                              secure);
				break;
			}
		}
		
		if (port.empty())
			break;
			
		// For simplicity, we make the assumption that users on a hub have the same character encoding
		ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), getMyNick(), getHubUrl(),
		                                              getEncoding(),
		                                              secure);
		break; // Все ОК тут брек хороший
	}
#ifdef FLYLINKDC_USE_COLLECT_STAT
	const string l_hub = getHubUrl();
	CFlylinkDBManager::getInstance()->push_dc_command_statistic(l_hub.empty() ? "-" : l_hub, param, server, port, senderNick);
#endif
}
//==========================================================================================
void NmdcHub::chatMessageParse(const string& p_line)
{
	// Check if we're being banned...
	if (state != STATE_NORMAL)
	{
		if (Util::findSubString(p_line, "banned") != string::npos)
		{
			setAutoReconnect(false);
		}
	}
	// [+] IRainman fix.
	const string l_utf8_line = toUtf8(unescape(p_line));
	
	if ((l_utf8_line.find("Hub-Security") != string::npos) && (l_utf8_line.find("was kicked by") != string::npos))
	{
		fly_fire3(ClientListener::StatusMessage(), this, l_utf8_line, ClientListener::FLAG_IS_SPAM);
		return;
	}
	else if ((l_utf8_line.find("is kicking") != string::npos) && (l_utf8_line.find("because:") != string::npos))
	{
		fly_fire3(ClientListener::StatusMessage(), this, l_utf8_line, ClientListener::FLAG_IS_SPAM);
		return;
	}
	// [~] IRainman fix.
	string nick;
	string message;
	bool bThirdPerson = false;
	
	
	if ((l_utf8_line.size() > 1 && l_utf8_line.compare(0, 2, "* ", 2) == 0) || (l_utf8_line.size() > 2 && l_utf8_line.compare(0, 3, "** ", 3) == 0))
	{
		size_t begin = l_utf8_line[1] == '*' ? 3 : 2;
		size_t end = l_utf8_line.find(' ', begin);
		if (end != string::npos)
		{
			nick = l_utf8_line.substr(begin, end - begin);
			message = l_utf8_line.substr(end + 1);
			bThirdPerson = true;
		}
	}
	else if (l_utf8_line[0] == '<')
	{
		string::size_type pos = l_utf8_line.find("> ");
		
		if (pos != string::npos)
		{
			nick = l_utf8_line.substr(1, pos - 1);
			message = l_utf8_line.substr(pos + 2);
			
			if (message.empty())
				return;
				
			if (isFloodCommand("<Nick>", p_line))
				return;
		}
	}
	if (nick.empty())
	{
		fly_fire2(ClientListener::StatusMessage(), this, l_utf8_line);
		return;
	}
	
	if (message.empty())
	{
		message = l_utf8_line;
	}
	
	const auto l_user = findUser(nick);
#ifdef _DEBUG
	if (message.find("&#124") != string::npos)
	{
		dcassert(0);
	}
#endif
	
	std::unique_ptr<ChatMessage> chatMessage(new ChatMessage(message, l_user));
	chatMessage->thirdPerson = bThirdPerson;
	if (!l_user)
	{
		chatMessage->m_text = l_utf8_line; // fix http://code.google.com/p/flylinkdc/issues/detail?id=944
		// если юзер подставной - не создаем его в списке
	}
	// [~] IRainman fix.
	
	if (!chatMessage->m_from)
	{
		if (l_user)
		{
			chatMessage->m_from = l_user; ////getUser(nick, false, false); // Тут внутри снова идет поиск findUser(nick)
			chatMessage->m_from->getIdentity().setHub();
			fly_fire1(ClientListener::UserUpdated(), chatMessage->m_from);
		}
	}
	if (!isSupressChatAndPM())
	{
		chatMessage->translate_me();
		if (!allowChatMessagefromUser(*chatMessage, nick)) // [+] IRainman fix.
			return;
		fly_fire2(ClientListener::Message(), this, chatMessage);
	}
	return;
	
}
//==========================================================================================
void NmdcHub::hubNameParse(const string& p_param)
{
	// Workaround replace newlines in topic with spaces, to avoid funny window titles
	// If " - " found, the first part goes to hub name, rest to description
	// If no " - " found, first word goes to hub name, rest to description
	string l_param = p_param;
	string::size_type i;
	boost::replace_all(l_param, "\r\n", " ");
	std::replace(l_param.begin(), l_param.end(), '\n', ' ');
	
	i = l_param.find(" - ");
	if (i == string::npos)
	{
		i = l_param.find(' ');
		if (i == string::npos)
		{
			getHubIdentity().setNick(unescape(l_param));
			getHubIdentity().setDescription(Util::emptyString);
		}
		else
		{
			getHubIdentity().setNick(unescape(l_param.substr(0, i)));
			getHubIdentity().setDescription(unescape(l_param.substr(i + 1)));
		}
	}
	else
	{
		getHubIdentity().setNick(unescape(l_param.substr(0, i)));
		getHubIdentity().setDescription(unescape(l_param.substr(i + 3)));
	}
	fly_fire1(ClientListener::HubUpdated(), this);
}
//==========================================================================================
void NmdcHub::supportsParse(const string& param)
{
	const StringTokenizer<string> st(param, ' '); // TODO убрать токены. сделать поиском.
	const StringList& sl = st.getTokens();
	for (auto i = sl.cbegin(); i != sl.cend(); ++i)
	{
		if (*i == "UserCommand")
		{
			m_supportFlags |= SUPPORTS_USERCOMMAND;
		}
		else if (*i == "NoGetINFO")
		{
			m_supportFlags |= SUPPORTS_NOGETINFO;
		}
		else if (*i == "UserIP2")
		{
			m_supportFlags |= SUPPORTS_USERIP2;
		}
		else if (*i == "NickRule")
		{
			m_supportFlags |= SUPPORTS_NICKRULE;
		}
		
#ifdef FLYLINKDC_USE_EXT_JSON
		else if (*i == "ExtJSON")
		{
			m_supportFlags |= SUPPORTS_EXTJSON;
			fly_fire1(ClientListener::FirstExtJSON(), this);
		}
#endif
	}
	// if (!(m_supportFlags & SUPPORTS_NICKRULE))
	/*
	<Mer> [00:27:44] *** Соединён
	- [00:27:47] <MegaHub> Время работы: 129 дней 8 часов 23 минут 21 секунд. Пользователей онлайн: 10109
	- [00:27:51] <MegaHub> Operation timeout (ValidateNick)
	- [00:27:52] *** [Hub = dchub://hub.o-go.ru] Соединение закрыто
	{
	    const auto l_nick = getMyNick();
	    OnlineUserPtr ou = getUser(l_nick, false, true);
	    sendValidateNick(ou->getIdentity().getNick());
	}
	*/
}
//==========================================================================================
void NmdcHub::userCommandParse(const string& param)
{
	string::size_type i = 0;
	string::size_type j = param.find(' ');
	if (j == string::npos)
		return;
		
	int type = Util::toInt(param.substr(0, j));
	i = j + 1;
	if (type == UserCommand::TYPE_SEPARATOR || type == UserCommand::TYPE_CLEAR)
	{
		int ctx = Util::toInt(param.substr(i));
		fly_fire5(ClientListener::HubUserCommand(), this, type, ctx, Util::emptyString, Util::emptyString);
	}
	else if (type == UserCommand::TYPE_RAW || type == UserCommand::TYPE_RAW_ONCE)
	{
		j = param.find(' ', i);
		if (j == string::npos)
			return;
		int ctx = Util::toInt(param.substr(i));
		i = j + 1;
		j = param.find('$');
		if (j == string::npos)
			return;
		string l_name = unescape(param.substr(i, j - i));
		// NMDC uses '\' as a separator but both ADC and our internal representation use '/'
		Util::replace("/", "//", l_name);
		Util::replace("\\", "/", l_name);
		i = j + 1;
		string command = unescape(param.substr(i, param.length() - i));
		fly_fire5(ClientListener::HubUserCommand(), this, type, ctx, l_name, command);
	}
}
//==========================================================================================
void NmdcHub::lockParse(const string& aLine)
{
	if (state != STATE_PROTOCOL || aLine.size() < 6)
	{
		return;
	}
	dcassert(m_users.empty());
	state = STATE_IDENTIFY;
	
	// Param must not be toUtf8'd...
	const string param = aLine.substr(6);
	
	if (!param.empty())
	{
		/* [-] IRainman fix: old proto code: http://mydc.ru/topic915.html?p=6719#entry6719
		   [-]string::size_type j = param.find(" Pk=");
		   [-]string lock, pk;
		   [-]if (j != string::npos)
		   [-]{
		   [-]  lock = param.substr(0, j);
		   [-]  pk = param.substr(j + 4);
		   [-]}
		   [-]else
		   [-] */
		// [-]{
		const auto j = param.find(' '); // [!]
		const auto lock = (j != string::npos) ? param.substr(0, j) : param; // [!]
		// [-]}
		// [~] IRainman fix.
		
		if (CryptoManager::isExtended(lock))
		{
			StringList feat;
#ifdef FLYLINKDC_USE_EXT_JSON
			feat.reserve(9);
#else
			feat.reserve(8);
#endif
			feat.push_back("UserCommand");
			feat.push_back("NoGetINFO");
			feat.push_back("NoHello");
			feat.push_back("UserIP2");
			feat.push_back("TTHSearch");
			feat.push_back("ZPipe0");
#ifdef FLYLINKDC_USE_EXT_JSON
			feat.push_back("ExtJSON");
#endif
			feat.push_back("NickRule");
			
			if (CryptoManager::getInstance()->TLSOk())
			{
				feat.push_back("TLS");
			}
#ifdef STRONG_USE_DHT
			if (BOOLSETTING(USE_DHT))
			{
				feat.push_back("DHT0");
			}
#endif
			supports(feat);
		}
		
		key(CryptoManager::getInstance()->makeKey(lock));
		
		const auto l_nick = getMyNick();
		OnlineUserPtr ou = getUser(l_nick, false, true);
		sendValidateNick(ou->getIdentity().getNick());
		
	}
	else
	{
		dcassert(0);
	}
}
//==========================================================================================
void NmdcHub::helloParse(const string& param)
{
	if (!param.empty())
	{
		OnlineUserPtr ou = getUser(param, false, false); // [!] IRainman fix: use OnlineUserPtr
		
		if (isMe(ou))
		{
			// [!] IRainman fix.
			if (isActive())
			{
				ou->getUser()->unsetFlag(User::NMDC_FILES_PASSIVE);
				ou->getUser()->unsetFlag(User::NMDC_SEARCH_PASSIVE);
			}
			else
			{
				ou->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
				ou->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
			}
			// [~] IRainman fix.
			
			if (state == STATE_IDENTIFY)
			{
				state = STATE_NORMAL;
				updateCounts(false);
				
				version();
				getNickList();
				myInfo(true);
				// [-] IRainman.
				//SetEvent(m_hEventClientInitialized);
			}
		}
		fly_fire1(ClientListener::UserUpdated(), ou); // !SMT!-fix
	}
}
//==========================================================================================
void NmdcHub::userIPParse(const string& p_ip_list)
{
	if (!p_ip_list.empty())
	{
		OnlineUserList v;
		const StringTokenizer<string> t(p_ip_list, "$$", p_ip_list.size() / 30);
		const StringList& sl = t.getTokens();
		{
			// [-] brain-ripper
			// I can't see any work with ClientListener
			// in this block, so I commented LockInstance,
			// because it caused deadlock in some situations.
			
			//ClientManager::LockInstance lc; // !SMT!-fix
			
			// Perhaps we should use Lock(cs) here, because
			// some elements of this class can be (theoretically)
			// changed in other thread
			
			// CFlyLock(cs); [-] IRainman fix.
			for (auto it = sl.cbegin(); it != sl.cend(); ++it)
			{
				string::size_type j = 0;
				if ((j = it->find(' ')) == string::npos)
					continue;
				if ((j + 1) == it->length())
					continue;
					
				const string l_ip = it->substr(j + 1);
				const string l_user = it->substr(0, j);
				if (l_user == getMyNick())
				{
					const bool l_is_private_ip = Util::isPrivateIp(l_ip);
					setTypeHub(l_is_private_ip);
					if (l_is_private_ip)
					{
						LogManager::message("Detect local hub: " + getHubUrl() + " private UserIP = " + l_ip + " User = " + l_user);
					}
				}
				OnlineUserPtr ou = findUser(l_user);
				
				if (!ou)
					continue;
					
				if (l_ip.size() > 15)
				{
					ou->getIdentity().setIP6(l_ip);
					ou->getIdentity().setUseIP6();
					ou->getIdentity().m_is_real_user_ip_from_hub = true;
				}
				else
				{
					dcassert(!l_ip.empty());
					ou->getIdentity().setIp(l_ip);
					ou->getIdentity().m_is_real_user_ip_from_hub = true;
					{
						CFlyFastLock(m_cs_virus);
						if (m_virus_nick.find(l_user) == m_virus_nick.end())
						{
							if (CFlylinkDBManager::getInstance()->is_virus_bot(l_user, ou->getIdentity().getBytesShared(), ou->getIdentity().getIpRAW()))
							{
								m_virus_nick.insert(l_user);
							}
						}
					}
					
				}
				// "FlylinkDC-dev"
				if (m_isAutobanAntivirusIP || m_isAutobanAntivirusNick
				        // && getHubUrl().find("dc.fly-server.ru") != string::npos
				   )
				{
					const auto l_avdb_result = ou->getIdentity().calcVirusType(true);
					if ((l_avdb_result & Identity::VT_SHARE) && (l_avdb_result & Identity::VT_IP) ||
					        (l_avdb_result & Identity::VT_NICK) && (l_avdb_result & Identity::VT_IP) ||
					        (l_avdb_result & Identity::VT_NICK) && (l_avdb_result & Identity::VT_SHARE)
					   )
					{
						const string l_size = Util::toString(ou->getIdentity().getBytesShared());
						const bool l_is_nick_share = (l_avdb_result & Identity::VT_NICK) && (l_avdb_result & Identity::VT_SHARE) && !(l_avdb_result & Identity::VT_IP);
						const bool l_is_ip_share = (l_avdb_result & Identity::VT_IP) && (l_avdb_result & Identity::VT_SHARE) && !(l_avdb_result & Identity::VT_NICK);
						const bool l_is_nick_ip = (l_avdb_result & Identity::VT_NICK) && (l_avdb_result & Identity::VT_IP) && !(l_avdb_result & Identity::VT_SHARE);
						const bool l_is_all_field = (l_avdb_result & Identity::VT_NICK) && (l_avdb_result & Identity::VT_SHARE) && (l_avdb_result & Identity::VT_IP);
						bool l_is_avdb_callback = false;
						if (
						    // (l_avdb_result & Identity::VT_NICK) && (l_avdb_result & Identity::VT_SHARE) &&  (l_avdb_result & Identity::VT_IP) || - это остылать не нужно они и так в базе уже есть
						    l_is_nick_share ||
						    l_is_ip_share ||
						    l_is_nick_ip
						    //! нельзя проверять только по IP (l_avdb_result & Identity::VT_IP)
						)
							if (!CFlyServerConfig::g_antivirus_db_url.empty())
							{
								// http://te-home.net/avdb.php?do=send&size=<размер шары>&addr=<ип адрес, не обязательно>&nick=<ник юзера>&path=<путь к вирусам, не обязательно>
								const auto l_encode_nick = ZenLib::Format::Http::URL_Encoded_Encode(l_user);
								const string l_get_avdb_query =
								    CFlyServerConfig::g_antivirus_db_url +
								    "/avdb.php?do=send"
								    "&size=" + l_size +
								    "&addr=" + l_ip +
								    "&nick=" + l_encode_nick;
								if (l_get_avdb_query != m_last_antivirus_detect_url)
								{
									m_last_antivirus_detect_url = l_get_avdb_query;
									std::vector<byte> l_binary_data;
									CFlyHTTPDownloader l_http_downloader;
									auto l_result_size = l_http_downloader.getBinaryDataFromInet(l_get_avdb_query, l_binary_data, 300);
									dcassert(l_result_size == 1);
									string l_value;
									if (l_result_size == 1)
										l_value += l_binary_data[0];
									dcassert(l_value == "1");
									const string l_log_message = "[ " + getMyNick() + " ] Update antivirus DB! result size =" + Util::toString(l_result_size) + " Value: [" + l_value +
									                             "], [" + l_get_avdb_query + " ] Hub: " + getHubUrl();
									CFlyServerJSON::pushError(40, l_log_message);
									LogManager::virus_message(l_log_message);
									l_is_avdb_callback = true;
								}
							}
						const auto l_avd_report = ou->getIdentity().getVirusDesc();
						string l_ban_command;
						string l_info = l_ip + " " + getMyNick() + " Autoban virus-bot! Nick:[ " + l_user + " ] IP: [" + l_ip + " ] Share: [ " + Util::toString(ou->getIdentity().getBytesShared()) +
						                " ] AVDB: " + l_avd_report + " Hub: " + getHubUrl() + " Time: " + Util::formatDigitalClock(GET_TIME()) + " Detect: ";
						if (l_is_nick_share)
							l_info += "Nick+Share";
						if (l_is_ip_share)
							l_info += "IP+Share";
						if (l_is_nick_ip)
							l_info += "Nick+IP";
						if (l_is_all_field)
							l_info += "Nick+IP+Share";
						if (l_is_avdb_callback)
							l_info += " + update AVDB";
						if (m_isAutobanAntivirusNick)
						{
							/*
							<FlylinkDC-dev> 13:38:03 Hub:   [Outgoing][162.211.230.164:411]     $To: ork5005 From: FlylinkDC-dev $<FlylinkDC-dev> You are being kicked because: virus|<FlylinkDC-dev> is kicking ork5005 because: virus|$Kick ork5005|
							13:38:03 Hub:   [Incoming][162.211.230.164:411]     <FlylinkDC-dev> is kicking ork5005 because: virus
							13:38:03 Hub:   [Incoming][162.211.230.164:411]     <PtokaX> *** ork5005 с IP 95.183.29.221 был кикнут FlylinkDC-dev.
							13:38:03 Hub:   [Incoming][162.211.230.164:411]     $Quit ork5005
							*/
							l_ban_command = "$Kick " + l_user + "|";
							send(l_ban_command);
							l_ban_command += " Info:" + l_info;
						}
						else
						{
							l_ban_command = !m_AntivirusCommandIP.empty() ? m_AntivirusCommandIP + " " : string("!banip ") + l_info;
							hubMessage(l_ban_command);
						}
						CFlyServerJSON::pushError(39, l_ban_command);
						LogManager::virus_message(l_ban_command);
					}
				}
				v.push_back(ou);
			}
		}
		fire_user_updated(v); // TODO - слать сообщения о смене только IP
		/*
		if (getMyNick() == "FlylinkDC-dev" && getHubUrl().find("dc.fly-server.ru") != string::npos)
		{
		for (auto j = v.cbegin(); j != v.cend(); ++j)
		{
		const auto ou = *j;
		const string l_user = ou->getUser()->getLastNick();
		const string l_ip   = ou->getIdentity().getIpAsString();
		const auto l_avdb_result = ou->getIdentity().calcVirusType(true);
		if ((l_avdb_result & Identity::VT_SHARE) || (l_avdb_result & Identity::VT_IP)) // TODO VT_NICK
		{
		const auto l_avd_report = ou->getIdentity().getVirusDesc();
		const string l_ban_command = "!banip " + l_ip + " Remove virus-bot: Nick:[ " + l_user + " ] IP: [" + l_ip + " ] AVDB: " + l_avd_report + " Hub: " + getHubUrl() + " |";
		CFlyServerJSON::pushError(39, l_ban_command);
		LogManager::message(l_ban_command);
		LogManager::virus_message(l_ban_command);
		hubMessage(l_ban_command);
		}
		}
		}
		}
		*/
	}
}
//==========================================================================================
void NmdcHub::botListParse(const string& param)
{
	OnlineUserList v;
	const StringTokenizer<string> t(param, "$$");
	const StringList& sl = t.getTokens();
	for (auto it = sl.cbegin(); it != sl.cend(); ++it)
	{
		if (it->empty())
			continue;
		OnlineUserPtr ou = getUser(*it, false, false);
		if (ou)
		{
			ou->getIdentity().setBot();
			v.push_back(ou);
		}
	}
	fire_user_updated(v);
}
//==========================================================================================
void NmdcHub::nickListParse(const string& param)
{
	if (!param.empty())
	{
		OnlineUserList v;
		const StringTokenizer<string> t(param, "$$");
		const StringList& sl = t.getTokens();
		{
			// [-] brain-ripper
			// I can't see any work with ClientListener
			// in this block, so I commented LockInstance,
			// because it caused deadlock in some situations.
			
			//ClientManager::LockInstance l; // !SMT!-fix
			
			// Perhaps we should use Lock(cs) here, because
			// some elements of this class can be (theoretically)
			// changed in other thread
			
			// CFlyLock(cs); [-] IRainman fix: no needs lock here!
			
			for (auto it = sl.cbegin(); it != sl.cend(); ++it)
			{
				if (it->empty())
					continue;
				OnlineUserPtr ou = getUser(*it, false, false);
				v.push_back(ou);// [!] IRainman fix: use OnlineUserPtr
			}
			
			if (!(m_supportFlags & SUPPORTS_NOGETINFO))
			{
				string tmp;
				// Let's assume 10 characters per nick...
				tmp.reserve(v.size() * (11 + 10 + getMyNick().length()));
				string n = ' ' +  fromUtf8(getMyNick()) + '|';
				for (auto i = v.cbegin(); i != v.cend(); ++i)
				{
					tmp += "$GetINFO ";
					tmp += fromUtf8((*i)->getIdentity().getNick());
					tmp += n;
				}
				if (!tmp.empty())
				{
					send(tmp);
				}
			}
		}
		fire_user_updated(v);
	}
}
//==========================================================================================
void NmdcHub::opListParse(const string& param)
{
	if (!param.empty())
	{
		OnlineUserList v;
		const StringTokenizer<string> t(param, "$$");
		const StringList& sl = t.getTokens();
		{
			// [-] brain-ripper
			// I can't see any work with ClientListener
			// in this block, so I commented LockInstance,
			// because it caused deadlock in some situations.
			
			//ClientManager::LockInstance l; // !SMT!-fix
			
			// Perhaps we should use Lock(cs) here, because
			// some elements of this class can be (theoretically)
			// changed in other thread
			
			// CFlyLock(cs); [-] IRainman fix: no needs any lock here!
			
			for (auto it = sl.cbegin(); it != sl.cend(); ++it) // fix copy-paste
			{
				if (it->empty())
					continue;
					
				OnlineUserPtr ou = getUser(*it, false, false);
				if (ou)
				{
					ou->getIdentity().setOp(true);
					v.push_back(ou);
				}
			}
		}
		fire_user_updated(v);
		updateCounts(false);
		
		// Special...to avoid op's complaining that their count is not correctly
		// updated when they log in (they'll be counted as registered first...)
		myInfo(false);
	}
}
//==========================================================================================
void NmdcHub::getUserList(OnlineUserList& p_list) const
{
	CFlyReadLock(*m_cs);
	p_list.reserve(m_users.size());
	for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
	{
		p_list.push_back(i->second);
	}
}
//==========================================================================================
void NmdcHub::AutodetectInit()
{
#ifdef RIP_USE_CONNECTION_AUTODETECT
	m_bAutodetectionPending = true;
	m_iRequestCount = 0;
	resetDetectActiveConnection();
#endif
	m_bLastMyInfoCommand = DIDNT_GET_YET_FIRST_MYINFO;
}
//==========================================================================================
#ifdef RIP_USE_CONNECTION_AUTODETECT
void NmdcHub::AutodetectComplete()
{
	m_bAutodetectionPending = false;
	m_iRequestCount = 0;
	setDetectActiveConnection();
	// send MyInfo, to update state on hub
	myInfo(true);
}
#endif // RIP_USE_CONNECTION_AUTODETECT
//==========================================================================================
void NmdcHub::toParse(const string& param)
{
	if (isSupressChatAndPM())
		return;
		
	string::size_type pos_a = param.find(" From: ");
	
	if (pos_a == string::npos)
		return;
		
	pos_a += 7;
	string::size_type pos_b = param.find(" $<", pos_a);
	
	if (pos_b == string::npos)
		return;
		
	const string rtNick = param.substr(pos_a, pos_b - pos_a);
	
	if (rtNick.empty())
		return;
		
	const auto l_user_for_message = findUser(rtNick);
	
	if (l_user_for_message == nullptr)
	{
		LogManager::flood_message("NmdcHub::toParse $To: invalid user: rtNick = " + rtNick + " param = " + param + " Hub = " + getHubUrl());
		return; // todo: here we dont get private message from unknown user
	}
	
	pos_a = pos_b + 3;
	pos_b = param.find("> ", pos_a);
	
	if (pos_b == string::npos)
	{
		dcassert(0);
#ifdef FLYLINKDC_BETA
		LogManager::flood_message("NmdcHub::toParse pos_b == string::npos param = " + param + " Hub = " + getHubUrl());
#endif
		return;
	}
	
	const string fromNick = param.substr(pos_a, pos_b - pos_a);
	
	if (fromNick.empty())
	{
#ifdef FLYLINKDC_BETA
		LogManager::message("NmdcHub::toParse fromNick.empty() param = " + param + " Hub = " + getHubUrl());
#endif
		dcassert(0);
		return;
	}
	
	const string msgText = param.substr(pos_b + 2);
	
	if (msgText.empty())
	{
#ifdef FLYLINKDC_BETA
		LogManager::message("NmdcHub::toParse msgText.empty() param = " + param + " Hub = " + getHubUrl());
#endif
		dcassert(0);
		return;
	}
	
	unique_ptr<ChatMessage> message(new ChatMessage(unescape(msgText), findUser(fromNick), nullptr, l_user_for_message));
	
	//if (message.replyTo == nullptr || message.from == nullptr) [-] IRainman fix.
	{
		if (message->m_replyTo == nullptr)
		{
			// Assume it's from the hub
			message->m_replyTo = getUser(rtNick, false, false); // [!] IRainman fix: use OnlineUserPtr
			message->m_replyTo->getIdentity().setHub();
			fly_fire1(ClientListener::UserUpdated(), message->m_replyTo); // !SMT!-fix
		}
		if (message->m_from == nullptr)
		{
			// Assume it's from the hub
			message->m_from = getUser(fromNick, false, false); // [!] IRainman fix: use OnlineUserPtr
			message->m_from->getIdentity().setHub();
			fly_fire1(ClientListener::UserUpdated(), message->m_from); // !SMT!-fix
		}
		
		// Update pointers just in case they've been invalidated
		// message.replyTo = findUser(rtNick); [-] IRainman fix. Imposibru!!!
		// message.from = findUser(fromNick); [-] IRainman fix. Imposibru!!!
	}
	
	message->m_to = getMyOnlineUser(); // !SMT!-S [!] IRainman fix.
	
	// [+]IRainman fix: message from you to you is not allowed! Block magical spam.
	if (message->m_to->getUser() == message->m_from->getUser() && message->m_from->getUser() == message->m_replyTo->getUser())
	{
		fly_fire3(ClientListener::StatusMessage(), this, message->m_text, ClientListener::FLAG_IS_SPAM);
		LogManager::message("Magic spam message (from you to you) filtered on hub: " + getHubUrl() + ".");
		return;
	}
	if (!allowPrivateMessagefromUser(*message)) // [+] IRainman fix.
		return;
		
	fly_fire2(ClientListener::Message(), this, message); // [+]
}
//==========================================================================================
void NmdcHub::onLine(const string& aLine)
{
	if (aLine.empty())
		return;
		
#ifdef _DEBUG
//	if (aLine.find("$Search") == string::npos)
//	{
//		LogManager::message("[NmdcHub::onLine][" + getHubUrl() + "] aLine = " + aLine);
//	}
#endif

	if (aLine[0] != '$')
	{
		chatMessageParse(aLine);
		return;
	}
	
	string cmd;
	string param;
	string::size_type x = aLine.find(' ');
	bool l_is_search  = false;
	bool l_is_passive = false;
#ifdef _DEBUG
//			if(aLine.find("Marvels.Agents.of.S.H.I.E.L.D") != string::npos)
//			{
//				l_is_passive = false;
//			}
#endif
	if (x == string::npos)
	{
		cmd = aLine.substr(1);
	}
	else
	{
		cmd = aLine.substr(1, x - 1);
		param = toUtf8(aLine.substr(x + 1));
		l_is_search = cmd == "Search"; // TODO - этого больше не будет - похерить
		if (l_is_search && ClientManager::isStartup() == false)
		{
			if (getHideShare())
			{
				return;
			}
			dcassert(0);
			if (aLine.size() > 12)
			{
				l_is_passive = aLine.compare(8, 4, "Hub:", 4) == 0;
			}
#ifdef _DEBUG
			if (l_is_passive)
			{
				// TODO - searchParseTTHPassive(param, i);
			}
			else
			{
				// Отработка наиболее частого запроса вида
				// "$Search x.x.x.x:yyyy F?T?0?9?TTH:A3VSWSWKCVC4N6EP2GX47OEMGT5ZL52BOS2LAHA"
				const auto i = param.find("?9?TTH:");
				if (i != string::npos)
				{
					dcassert(0);
					// Отрабатывается в другом методе
					// searchParseTTHActive(param, i);
				}
			}
#endif
		}
	}
	if (l_is_search == false && isFloodCommand(cmd, param) == true)
	{
		return;
	}
#ifdef FLYLINKDC_USE_COLLECT_STAT
	{
		string l_tth;
		const auto l_tth_pos = param.find("TTH:");
		if (l_tth_pos != string::npos)
			l_tth = param.substr(l_tth_pos + 4, 39);
		CFlylinkDBManager::getInstance()->push_event_statistic("command-nmdc", cmd, param, getIpAsString(), "", getHubUrlAndIP(), l_tth);
	}
#endif
	
	bool bMyInfoCommand = false;
	if (l_is_search && ClientManager::isStartup() == false)
	{
		dcassert(0);  // Используем void NmdcHub::on(BufferedSocketListener::SearchArrayFile
		searchParse(param, l_is_passive);
	}
	else if (cmd == "MyINFO")
	{
		bMyInfoCommand = true;
		myInfoParse(param); // [+]PPA http://code.google.com/p/flylinkdc/issues/detail?id=1384
	}
#ifdef FLYLINKDC_USE_EXT_JSON
	else if (cmd == "ExtJSON")
	{
		//bMyInfoCommand = false;
		extJSONParse(param);
	}
#endif
	else if (cmd == "Quit")
	{
		if (!param.empty())
		{
			putUser(param);
		}
		else
		{
			//dcassert(0);
		}
	}
	else if (cmd == "ConnectToMe")
	{
		connectToMeParse(param);
		return;
	}
	else if (cmd == "RevConnectToMe")
	{
		revConnectToMeParse(param);
	}
	else if (cmd == "SR")
	{
		SearchManager::getInstance()->onSearchResult(aLine);
	}
	else if (cmd == "HubName")
	{
		hubNameParse(param);
	}
	else if (cmd == "Supports")
	{
		supportsParse(param);
	}
	else if (cmd == "UserCommand")
	{
		userCommandParse(param);
	}
	else if (cmd == "Lock")
	{
		lockParse(aLine); // aLine!
	}
	else if (cmd == "Hello")
	{
		helloParse(param);
	}
	else if (cmd == "ForceMove")
	{
		dcassert(m_client_sock);
		if (m_client_sock)
			m_client_sock->disconnect(false);
		fly_fire2(ClientListener::Redirect(), this, param);
	}
	else if (cmd == "HubIsFull")
	{
		fly_fire1(ClientListener::HubFull(), this);
	}
	else if (cmd == "ValidateDenide")        // Mind the spelling...
	{
		dcassert(m_client_sock);
		if (m_client_sock)
			m_client_sock->disconnect(false);
		fly_fire1(ClientListener::NickTaken(), this);
		//m_count_validate_denide++;
	}
	else if (cmd == "UserIP")
	{
		userIPParse(param);
	}
	else if (cmd == "BotList")
	{
		botListParse(param);
	}
	else if (cmd == "NickList") // TODO - убить
	{
		nickListParse(param);
	}
	else if (cmd == "OpList")
	{
		opListParse(param);
	}
	else if (cmd == "To:")
	{
		toParse(param);
	}
	else if (cmd == "GetPass")
	{
		// [!] IRainman fix.
		getUser(getMyNick(), false, false); // [!] use OnlineUserPtr, don't delete this line.
		setRegistered(); // [!]
		// setMyIdentity(ou->getIdentity()); [-]
		processingPassword(); // [!]
		// [~] IRainman fix.
	}
	else if (cmd == "BadPass")
	{
		setPassword(Util::emptyString);
	}
	else if (cmd == "ZOn")
	{
		dcassert(0); // Обработку ZOn перенес в BufferedSocket чтобы не звать лишний Listener
	}
	else if (cmd == "HubTopic")
	{
		fly_fire2(ClientListener::HubTopic(), this, param);
	}
	// [+] IRainman.
	else if (cmd == "LogedIn")
	{
		messageYouHaweRightOperatorOnThisHub();
	}
	// [~] IRainman.
	//else if (cmd == "myinfo")
	//{
	//}
	else if (cmd == "UserComman" || cmd == "myinfo")
	{
		// Где-то ошибка в плагине - много спама идет на сервер - отрубил нахрен
		const string l_message = "NmdcHub::onLine first unknown command! hub = [" + getHubUrl() + "], command = [" + cmd + "], param = [" + param + "]";
		LogManager::message(l_message);
	}
	else if (cmd == "BadNick")
	{
	
		/*
		$BadNick TooLong 64        -- ник слишком длинный, максимальная допустимая длина ника 64 символа     (флай считает сколько у него в нике символов и убирает лишние, так чтоб осталось максимум 64)
		$BadNick TooShort 3        -- ник слишком короткий, минимальная допустимая длина ника 3 символа     (флай считает сколько у него в нике символов и добавляет нехватающие, так чтоб было минимум 3)
		$BadNick BadPrefix        -- у ника лишний префикс, хаб хочет ник без префикса      (флай уберает все префиксы из ника)
		$BadNick BadPrefix [ISP1] [ISP2]        -- у ника нехватает префикса, хаб хочет ник с префиксом [ISP1] или [ISP2]      (флай добавляет случайный из перечисленых префиксов к нику)
		$BadNick BadChar 32 36        -- ник содержит запрещенные хабом символы, хаб хочет ник в котором не будет перечисленых символов      (флай убирает из ника все перечисленые байты символов)
		*/
		dcassert(m_client_sock);
		if (m_client_sock)
			m_client_sock->disconnect(false);
		{
			auto l_nick = getMyNick();
			m_nick_rule.convert_nick(l_nick);
			setMyNick(l_nick);
		}
		fly_fire1(ClientListener::NickTaken(), this);
		//m_count_validate_denide++;
	}
	else if (cmd == "NickRule")
	{
		m_nick_rule = CFlyNickRule();
		const StringTokenizer<string> l_nick_rule(param, "$$", 4);
		const StringList& sl = l_nick_rule.getTokens();
		for (auto it = sl.cbegin(); it != sl.cend(); ++it)
		{
			auto l_pos = it->find(' ');
			if (l_pos != string::npos && l_pos < it->size() + 1)
			{
				const string l_key = it->substr(0, l_pos);
				if (l_key == "Min")
				{
					m_nick_rule.m_nick_rule_min = Util::toInt(it->substr(l_pos + 1));
				}
				else if (l_key == "Max")
				{
					m_nick_rule.m_nick_rule_max = Util::toInt(it->substr(l_pos + 1));
				}
				else if (l_key == "Char")
				{
					const StringTokenizer<string> l_char(it->substr(l_pos + 1), " ");
					const StringList& l = l_char.getTokens();
					for (auto j = l.cbegin(); j != l.cend(); ++j)
					{
						if (!j->empty())
							m_nick_rule.m_invalid_char.push_back(uint8_t(Util::toInt(*j)));
					}
				}
				else if (l_key == "Pref")
				{
					const StringTokenizer<string> l_pref(it->substr(l_pos + 1), " ");
					const StringList& l = l_pref.getTokens();
					for (auto j = l.cbegin(); j != l.cend(); ++j)
					{
						if (!j->empty())
						{
							m_nick_rule.m_prefix.push_back(*j);
							break; // TODO - зачем клиенту знать список разрешенных префиксов?
						}
					}
				}
			}
			else
			{
				dcassert(0);
			}
		}
		if ((m_supportFlags & SUPPORTS_NICKRULE))
		{
			auto l_nick = getMyNick();
			m_nick_rule.convert_nick(l_nick);
			setMyNick(l_nick);
			// Тут пока не пашет.
			//OnlineUserPtr ou = getUser(l_nick, false, true);
			//sendValidateNick(ou->getIdentity().getNick());
		}
	}
	else
	{
		//dcassert(0);
		dcdebug("NmdcHub::onLine Unknown command %s\n", aLine.c_str());
		string l_message;
		{
			CFlyFastLock(g_unknown_cs);
			g_unknown_command_array[getHubUrl()][cmd]++;
			auto& l_item = g_unknown_command[cmd + "[" + getHubUrl() + "]"];
			l_item.second++;
			if (l_item.first.empty())
			{
				l_item.first = aLine;
				l_message = "NmdcHub::onLine first unknown command! hub = [" + getHubUrl() + "], command = [" + cmd + "], param = [" + param + "]";
			}
		}
		if (!l_message.empty())
		{
			LogManager::message(l_message + " Raw = " + aLine);
			CFlyServerJSON::pushError(24, "NmdcHub::onLine first unknown command:" + l_message);
		}
	}
	processAutodetect(bMyInfoCommand);
}
string NmdcHub::get_all_unknown_command()
{
	CFlyFastLock(g_unknown_cs);
	string l_message;
	for (auto i = g_unknown_command_array.cbegin(); i != g_unknown_command_array.cend(); ++i)
	{
		l_message += "Hub: " + i->first + " Invalid command: ";
		string l_separator;
		for (auto j = i->second.cbegin(); j != i->second.cend(); ++j)
		{
			l_message += l_separator + j->first + " ( count: " + Util::toString(j->second) + ") ";
			l_separator = " ";
		}
	}
	return l_message;
}
void NmdcHub::log_all_unknown_command()
{
	{
		CFlyFastLock(g_unknown_cs);
		for (auto i = g_unknown_command.cbegin(); i != g_unknown_command.cend(); ++i)
		{
			const string l_message = "NmdcHub::onLine summary unknown command! Count = " +
			                         Util::toString(i->second.second) + " Key = [" + i->first + "], first value = [" + i->second.first + "]";
			LogManager::message(l_message);
			CFlyServerJSON::pushError(24, "NmdcHub::onLine summary unknown command:" + l_message);
		}
		g_unknown_command.clear();
	}
	CFlyServerJSON::pushError(25, get_all_unknown_command());
}
void NmdcHub::processAutodetect(bool p_is_myinfo)
{
	if (!p_is_myinfo && m_bLastMyInfoCommand == FIRST_MYINFO)
	{
#ifdef RIP_USE_CONNECTION_AUTODETECT
		// This is first command after $MyInfo.
		// Do autodetection now, because at least VerliHub hag such a bug:
		// when hub sends $myInfo for each user on handshake sequence, it may skip
		// much connection requests (or may be also other commands), so it is better not to send
		// anything to it when receiving $myInfos is in progress
		RequestConnectionForAutodetect();
#endif
		m_bLastMyInfoCommand = ALREADY_GOT_MYINFO;
	}
	if (p_is_myinfo && m_bLastMyInfoCommand == DIDNT_GET_YET_FIRST_MYINFO)
	{
		m_bLastMyInfoCommand = FIRST_MYINFO;
	}
}

void NmdcHub::checkNick(string& aNick)
{
	for (size_t i = 0; i < aNick.size(); ++i)
	{
		if (static_cast<uint8_t>(aNick[i]) <= 32 || aNick[i] == '|' || aNick[i] == '$' || aNick[i] == '<' || aNick[i] == '>')
		{
			aNick[i] = '_';
		}
	}
}

void NmdcHub::connectToMe(const OnlineUser& aUser
#ifdef RIP_USE_CONNECTION_AUTODETECT
                          , ExpectedMap::DefinedExpectedReason reason /* = ExpectedMap::REASON_DEFAULT */
#endif
                         )
{
	checkstate();
	dcdebug("NmdcHub::connectToMe %s\n", aUser.getIdentity().getNick().c_str());
	const string nick = fromUtf8(aUser.getIdentity().getNick());
	ConnectionManager::getInstance()->nmdcExpect(nick, getMyNick(), getHubUrl()
#ifdef RIP_USE_CONNECTION_AUTODETECT
	                                             , reason
#endif
	                                            );
	ConnectionManager::g_ConnToMeCount++;
	
	const bool secure = CryptoManager::getInstance()->TLSOk() && aUser.getUser()->isSet(User::TLS);
	const uint16_t port = secure ? ConnectionManager::getInstance()->getSecurePort() : ConnectionManager::getInstance()->getPort();
	
	if (port == 0)
	{
		dcassert(0);
		LogManager::message("Error [2] $ConnectToMe port = 0 : ");
		CFlyServerJSON::pushError(22, "Error [2] $ConnectToMe port = 0 :");
	}
	else
	{
		send("$ConnectToMe " + nick + ' ' + getLocalIp() + ':' + Util::toString(port) + (secure ? "S|" : "|"));
	}
}

void NmdcHub::revConnectToMe(const OnlineUser& aUser)
{
	checkstate();
	dcdebug("NmdcHub::revConnectToMe %s\n", aUser.getIdentity().getNick().c_str());
	send("$RevConnectToMe " + fromUtf8(getMyNick()) + ' ' + fromUtf8(aUser.getIdentity().getNick()) + '|'); //[1] https://www.box.net/shared/f8330d2c54b2d7dcf3e4
}

void NmdcHub::hubMessage(const string& aMessage, bool thirdPerson)
{
	checkstate();
	send(fromUtf8Chat('<' + getMyNick() + "> " + escape(thirdPerson ? "/me " + aMessage : aMessage) + '|')); // IRAINMAN_USE_UNICODE_IN_NMDC
}

bool NmdcHub::resendMyINFO(bool p_always_send, bool p_is_force_passive)
{
	if (p_is_force_passive)
	{
		if (m_modeChar == 'P')
			return false; // Уходим из обновления MyINFO - уже находимся в пассивном режиме
	}
	myInfo(p_always_send, p_is_force_passive);
	return true;
}

void NmdcHub::myInfo(bool p_always_send, bool p_is_force_passive)
{
	const uint64_t l_limit = 2 * 60 * 1000; // http://code.google.com/p/flylinkdc/issues/detail?id=1370
	const uint64_t currentTick = GET_TICK(); // [!] IRainman opt.
	if (p_is_force_passive == false && p_always_send == false && m_lastUpdate + l_limit > currentTick)
	{
		return; // antispam
	}
	checkstate();
	const FavoriteHubEntry *fhe = reloadSettings(false);
	char l_modeChar;
	if (p_is_force_passive)
	{
		l_modeChar = 'P';
	}
	else
	{
		if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)
		{
			l_modeChar = '5';
		}
		else if (isActive())
		{
			l_modeChar = 'A';
		}
		else
		{
			l_modeChar = 'P';
		}
	}
	const int64_t upLimit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getUploadLimitInKBytes() : 0;// [!] IRainman SpeedLimiter
	const string uploadSpeed = (upLimit > 0) ? Util::toString(upLimit) + " KiB/s" : SETTING(UPLOAD_SPEED); // [!] IRainman SpeedLimiter
	
	char status = NmdcSupports::NORMAL;
	
	if (Util::getAway())
	{
		status |= NmdcSupports::AWAY;
	}
	if (UploadManager::getInstance()->getIsFileServerStatus())
	{
		status |= NmdcSupports::SERVER;
	}
	if (UploadManager::getInstance()->getIsFireballStatus())
	{
		status |= NmdcSupports::FIREBALL;
	}
	if (BOOLSETTING(ALLOW_NAT_TRAVERSAL) && !isActive())
	{
		status |= NmdcSupports::NAT0;
	}
	
	if (CryptoManager::getInstance()->TLSOk())
	{
		status |= NmdcSupports::TLS;
	}
	const string currentCounts = fhe && fhe->getExclusiveHub() ? getCountsIndivid() : getCounts();
	
	// IRAINMAN_USE_UNICODE_IN_NMDC
	string l_currentMyInfo;
	l_currentMyInfo.resize(256);
	const string l_version = getClientName() + " V:" + getTagVersion();
	string l_ExtJSONSupport;
	if (m_supportFlags & SUPPORTS_EXTJSON)
	{
		l_ExtJSONSupport = MappingManager::getPortmapInfo(false, false);
		if (isFlySupportHub())
		{
			if (CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_autoAddSupportHub))
			{
				l_ExtJSONSupport += "+Promo";
			}
			if (CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_autoAddFirstSupportHub))
			{
				l_ExtJSONSupport += "+PromoF";
			}
			if (CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_autoAdd1251SupportHub))
			{
				l_ExtJSONSupport += "+PromoL";
			}
			if (isDetectActiveConnection())
			{
				l_ExtJSONSupport += "+TCP(ok)";
			}
			static string g_VID;
			static bool g_VID_check = false;
			if (g_VID_check == false)
			{
				g_VID_check = true;
				g_VID = Util::getRegistryCommaSubkey(_T("VID"));
			}
			if (!g_VID.empty()
			        && g_VID != "50000000"
			        && g_VID != "60000000"
			        && g_VID != "70000000"
			        && g_VID != "30000000"
			        && g_VID != "40000000"
			        && g_VID != "10000000"
			        && g_VID != "20000000"
			        && g_VID != "501"
			        && g_VID != "502"
			        && g_VID != "503"
			        && g_VID != "401"
			        && g_VID != "402"
			        && g_VID != "901"
			        && g_VID != "902")
			{
				l_ExtJSONSupport += "+VID:" + g_VID;
			}
		}
		if (CompatibilityManager::g_is_teredo)
		{
			l_ExtJSONSupport += "+Teredo";
		}
		if (CompatibilityManager::g_is_ipv6_enabled)
		{
			l_ExtJSONSupport += "+IPv6";
		}
		
	}
	l_currentMyInfo.resize(_snprintf(&l_currentMyInfo[0], l_currentMyInfo.size() - 1, "$MyINFO $ALL %s %s<%s,M:%c,H:%s,S:%d"
	                                 ">$ $%s%c$%s$",
	                                 fromUtf8(getMyNick()).c_str(),
	                                 fromUtf8Chat(escape(getCurrentDescription())).c_str(),
	                                 l_version.c_str(), // [!] IRainman mimicry function.
	                                 l_modeChar,
	                                 currentCounts.c_str(),
	                                 UploadManager::getSlots(),
	                                 uploadSpeed.c_str(), status,
	                                 fromUtf8Chat(escape(getCurrentEmail())).c_str()));
	                                 
	const int64_t l_currentBytesShared =
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	    getHideShare() ? 0 :
#endif
	    ShareManager::getShareSize();
	    
#ifdef FLYLINKDC_BETA
	if (p_is_force_passive == true && l_currentBytesShared == m_lastBytesShared && !m_lastMyInfo.empty() && l_currentMyInfo == m_lastMyInfo)
	{
		dcassert(0);
		LogManager::message("Duplicate send MyINFO = " + l_currentMyInfo + " hub: " + getHubUrl());
	}
#endif
	const bool l_is_change_my_info = (l_currentBytesShared != m_lastBytesShared && m_lastUpdate + l_limit < currentTick) || l_currentMyInfo != m_lastMyInfo;
	const bool l_is_change_fly_info = g_version_fly_info != m_version_fly_info || m_lastExtJSONInfo.empty() || m_lastExtJSONSupport != l_ExtJSONSupport;
	if (p_always_send || l_is_change_my_info || l_is_change_fly_info)
	{
		if (l_is_change_my_info)
		{
			m_lastMyInfo = l_currentMyInfo;
			m_lastBytesShared = l_currentBytesShared;
			send(m_lastMyInfo + Util::toString(l_currentBytesShared) + "$|");
			m_lastUpdate = currentTick;
		}
#ifdef FLYLINKDC_USE_EXT_JSON
		if ((m_supportFlags & SUPPORTS_EXTJSON) && l_is_change_fly_info)
		{
			m_lastExtJSONSupport = l_ExtJSONSupport;
			m_version_fly_info = g_version_fly_info;
			Json::Value l_json_info;
			if (!SETTING(FLY_LOCATOR_COUNTRY).empty())
				l_json_info["Country"] = SETTING(FLY_LOCATOR_COUNTRY).substr(0, 30);
			if (!SETTING(FLY_LOCATOR_CITY).empty())
				l_json_info["City"] = SETTING(FLY_LOCATOR_CITY).substr(0, 30);
			if (!SETTING(FLY_LOCATOR_ISP).empty())
				l_json_info["ISP"] = SETTING(FLY_LOCATOR_ISP).substr(0, 30);
			l_json_info["Gender"] = SETTING(FLY_GENDER) + 1;
			if (!l_ExtJSONSupport.empty())
				l_json_info["Support"] = l_ExtJSONSupport;
			if (!getHideShare())
			{
				if (const auto l_count_files = ShareManager::getLastSharedFiles())
				{
					l_json_info["Files"] = l_count_files;
				}
			}
			if (ShareManager::getLastSharedDate())
			{
				l_json_info["LastDate"] = ShareManager::getLastSharedDate();
			}
			extern int g_RAM_WorkingSetSize;
			if (g_RAM_WorkingSetSize)
			{
				l_json_info["RAM"] = g_RAM_WorkingSetSize;
			}
			if (CompatibilityManager::getFreePhysMemory())
			{
				l_json_info["RAMFree"] = CompatibilityManager::getFreePhysMemory() / 1024 / 1024;
			}
			if (g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_GUI])
			{
				l_json_info["StartGUI"] = unsigned(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_GUI]);
			}
			if (g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE])
			{
				l_json_info["StartCore"] = unsigned(g_fly_server_stat.m_time_mark[CFlyServerStatistics::TIME_START_CORE]);
			}
			if (CFlylinkDBManager::getCountQueueFiles())
			{
				l_json_info["QueueFiles"] = CFlylinkDBManager::getCountQueueFiles();
			}
			if (CFlylinkDBManager::getCountQueueSources())
			{
				l_json_info["QueueSrc"] = CFlylinkDBManager::getCountQueueSources();
			}
			extern int g_RAM_PeakWorkingSetSize;
			if (g_RAM_PeakWorkingSetSize)
			{
				l_json_info["RAMPeak"] = g_RAM_PeakWorkingSetSize;
			}
			extern int64_t g_SQLiteDBSize;
			if (const int l_value = g_SQLiteDBSize / 1024 / 1024)
			{
				l_json_info["SQLSize"] = l_value;
			}
			extern int64_t g_SQLiteDBSizeFree;
			if (const int l_value = g_SQLiteDBSizeFree / 1024 / 1024)
			{
				l_json_info["SQLFree"] = l_value;
			}
			extern int64_t g_TTHLevelDBSize;
			if (const int l_value = g_TTHLevelDBSize / 1024 / 1024)
			{
				l_json_info["LDBHistSize"] = l_value;
			}
#ifdef FLYLINKDC_USE_IPCACHE_LEVELDB
			extern int64_t g_IPCacheLevelDBSize;
			if (const int l_value = g_IPCacheLevelDBSize / 1024 / 1024)
			{
				l_json_info["LDBIPCacheSize"] = l_value;
			}
#endif

			string l_json_str = l_json_info.toStyledString();
			
			boost::replace_all(l_json_str, "\r", " "); // TODO убрать внутрь jsoncpp
			boost::replace_all(l_json_str, "\n", " ");
			
			boost::replace_all(l_json_str, "  ", " ");
			boost::replace_all(l_json_str, "$", "");
			boost::replace_all(l_json_str, "|", "");
			
			const string l_lastExtJSONInfo = "$ExtJSON $ALL " + getMyNick() + " " + l_json_str;
			if (m_lastExtJSONInfo != l_lastExtJSONInfo)
			{
				m_lastExtJSONInfo = l_lastExtJSONInfo;
				send(m_lastExtJSONInfo + "$|");
				m_lastUpdate = currentTick;
			}
		}
#endif // FLYLINKDC_USE_EXT_JSON
		m_modeChar = l_modeChar;
	}
}

void NmdcHub::search_token(const SearchParamToken& p_search_param)
{
	checkstate();
	const char c1 = (p_search_param.m_size_mode == Search::SIZE_DONTCARE || p_search_param.m_size_mode == Search::SIZE_EXACT) ? 'F' : 'T';
	const char c2 = p_search_param.m_size_mode == Search::SIZE_ATLEAST ? 'F' : 'T';
	string tmp = p_search_param.m_file_type == Search::TYPE_TTH ? g_tth + p_search_param.m_filter : fromUtf8(escape(p_search_param.m_filter)); // [!] IRainman opt.
	string::size_type i;
	while ((i = tmp.find(' ')) != string::npos)
	{
		tmp[i] = '$';
	}
	string tmp2;
	bool l_is_passive = p_search_param.m_is_force_passive || BOOLSETTING(SEARCH_PASSIVE);
	if (SearchManager::getSearchPortUint() == 0)
	{
		l_is_passive = true;
		LogManager::message("Error search port = 0 : ");
		CFlyServerJSON::pushError(21, "Error search port = 0 :");
	}
	const bool l_is_active = isActive();
	if (l_is_active && !l_is_passive)
	{
		tmp2 = calcExternalIP();
	}
	else
	{
		tmp2 = "Hub:" + fromUtf8(getMyNick());
	}
	const string l_search_command = "$Search " + tmp2 + ' ' + c1 + '?' + c2 + '?' + Util::toString(p_search_param.m_size) + '?' + Util::toString(p_search_param.m_file_type + 1) + '?' + tmp + '|';
#ifdef _DEBUG
	const string l_debug_string = "[Search:" + l_search_command + "][" + (l_is_active ? string("Active") : string("Passive")) + " search][Client:" + getHubUrl() + "]";
	dcdebug("[NmdcHub::search] %s \r\n", l_debug_string.c_str());
#endif
	g_last_search_string.clear();
	if (l_is_active)
	{
		g_last_search_string = "UDP port: " + tmp2;
	}
	{
		if (SearchManager::getSearchPortUint() == 0)
			g_last_search_string += " [InvalidPort=0]";
		if (p_search_param.m_is_force_passive)
			g_last_search_string += " [AutoPasive]";
		if (BOOLSETTING(SEARCH_PASSIVE))
			g_last_search_string += " [GlobalPassive]";
		if (SETTING(FORCE_PASSIVE_INCOMING_CONNECTIONS))
			g_last_search_string += " [ForcePassive]";
		//if (m_isActivMode)
		//  g_last_search_string += " [Client:ActiveFirst]";
	}
	//LogManager::message(l_debug_string);
	// TODO - check flood (BETA)
	send(l_search_command);
}

string NmdcHub::validateMessage(string tmp, bool reverse)
{
	string::size_type i = 0;
	
	if (reverse)
	{
		while ((i = tmp.find("&#36;", i)) != string::npos)
		{
			tmp.replace(i, 5, "$");
			i++;
		}
		i = 0;
		while ((i = tmp.find("&#124;", i)) != string::npos)
		{
			tmp.replace(i, 6, "|");
			i++;
		}
		i = 0;
		while ((i = tmp.find("&amp;", i)) != string::npos)
		{
			tmp.replace(i, 5, "&");
			i++;
		}
	}
	else
	{
		i = 0;
		while ((i = tmp.find("&amp;", i)) != string::npos)
		{
			tmp.replace(i, 1, "&amp;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find("&#36;", i)) != string::npos)
		{
			tmp.replace(i, 1, "&amp;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find("&#124;", i)) != string::npos)
		{
			tmp.replace(i, 1, "&amp;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find('$', i)) != string::npos)
		{
			tmp.replace(i, 1, "&#36;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find('|', i)) != string::npos)
		{
			tmp.replace(i, 1, "&#124;");
			i += 5;
		}
	}
	return tmp;
}

void NmdcHub::privateMessage(const string& nick, const string& message, bool thirdPerson)
{
	send("$To: " + fromUtf8(nick) + " From: " + fromUtf8(getMyNick()) + " $" + fromUtf8Chat(escape('<' + getMyNick() + "> " + (thirdPerson ? "/me " + message : message))) + '|'); // IRAINMAN_USE_UNICODE_IN_NMDC
}

void NmdcHub::privateMessage(const OnlineUserPtr& aUser, const string& aMessage, bool thirdPerson)   // !SMT!-S
{
	if (isSupressChatAndPM())
		return;
	checkstate();
	
	privateMessage(aUser->getIdentity().getNick(), aMessage, thirdPerson);
	// Emulate a returning message...
	// CFlyLock(cs); // !SMT!-fix: no data to lock
	
	// [!] IRainman fix.
	const OnlineUserPtr& me = getMyOnlineUser();
	// fly_fire1(ClientListener::PrivateMessage(), this, getMyNick(), me, aUser, me, '<' + getMyNick() + "> " + aMessage, thirdPerson); // !SMT!-S [-] IRainman fix.
	
	unique_ptr<ChatMessage> message(new ChatMessage(aMessage, me, aUser, me, thirdPerson));
	if (!allowPrivateMessagefromUser(*message))
		return;
		
	fly_fire2(ClientListener::Message(), this, message);
	// [~] IRainman fix.
}

void NmdcHub::sendUserCmd(const UserCommand& command, const StringMap& params)
{
	checkstate();
	string cmd = Util::formatParams(command.getCommand(), params, false);
	if (command.isChat())
	{
		if (command.getTo().empty())
		{
			hubMessage(cmd);
		}
		else
		{
			privateMessage(command.getTo(), cmd, false);
		}
	}
	else
	{
		send(fromUtf8(cmd));
	}
}
void NmdcHub::on(BufferedSocketListener::Connected) noexcept
{
	Client::on(Connected());
	
	if (state != STATE_PROTOCOL)
	{
		return;
	}
	m_version_fly_info = 0;
	m_modeChar = 0;
	m_supportFlags = 0;
	m_lastMyInfo.clear();
	m_lastBytesShared = 0;
	m_lastUpdate = 0;
	m_lastExtJSONInfo.clear();
}
#ifdef FLYLINKDC_USE_EXT_JSON
bool NmdcHub::extJSONParse(const string& param, bool p_is_disable_fire /*= false */)
{
	string::size_type i = 5;
	string::size_type j = param.find(' ', i);
	if (j == string::npos || j == i)
		return false;
	string l_nick = param.substr(i, j - i);
	
	dcassert(!l_nick.empty())
	if (l_nick.empty())
	{
		dcassert(0);
		return false;
	}
	if (p_is_disable_fire == false)
	{
		/*
		CFlyWriteLock(*m_cs);
		if (m_ext_json_deferred.find(l_nick) == m_ext_json_deferred.end())
		{
		    m_ext_json_deferred.insert(std::make_pair(l_nick, param));
		    return false;
		}
		*/
	}
	
//#ifdef _DEBUG
//	string l_json_result = "{ \"City\":[\"$ForceMove\", \"abc.com\", \"&#124; \"] } | ";
//#else
	string l_json_result = param.substr(i + l_nick.size() + 1);
//#endif
	try
	{
		Json::Value l_root;
		Json::Reader l_reader(Json::Features::strictMode());
		const bool l_parsingSuccessful = l_reader.parse(l_json_result, l_root);
		if (!l_parsingSuccessful && !l_json_result.empty())
		{
			dcassert(0);
			LogManager::message("Failed to parse json ExtJSON:" + l_json_result);
			return false;
		}
		else
		{
			OnlineUserPtr ou = getUser(l_nick, false, false);
			ou->getIdentity().setStringParam("F1", l_root["Country"].asString());
			ou->getIdentity().setStringParam("F2", l_root["City"].asString());
			ou->getIdentity().setStringParam("F3", l_root["ISP"].asString());
			ou->getIdentity().setStringParam("F4", l_root["Gender"].asString());
			ou->getIdentity().setExtJSONSupportInfo(l_root["Support"].asString());
			ou->getIdentity().setExtJSONRAMWorkingSet(l_root["RAM"].asInt());
			ou->getIdentity().setExtJSONRAMPeakWorkingSet(l_root["RAMPeak"].asInt());
			ou->getIdentity().setExtJSONRAMFree(l_root["RAMFree"].asInt());
			//ou->getIdentity().setExtJSONGDI(l_root["GDI"].asInt());
			ou->getIdentity().setExtJSONCountFiles(l_root["Files"].asInt());
			ou->getIdentity().setExtJSONLastSharedDate(l_root["LastDate"].asInt64());
			ou->getIdentity().setExtJSONSQLiteDBSize(l_root["SQLSize"].asInt());
			ou->getIdentity().setExtJSONlevelDBHistSize(l_root["LDBHistSize"].asInt());
			ou->getIdentity().setExtJSONSQLiteDBSizeFree(l_root["SQLFree"].asInt());
			ou->getIdentity().setExtJSONQueueFiles(l_root["QueueFiles"].asInt());
			ou->getIdentity().setExtJSONQueueSrc(l_root["QueueSrc"].asInt64()); //TODO - временны баг - тут 32 бита
			ou->getIdentity().setExtJSONTimesStartCore(l_root["StartCore"].asInt());
			ou->getIdentity().setExtJSONTimesStartGUI(l_root["StartGUI"].asInt());
			
			if (!ClientManager::isShutdown())
			{
				if (p_is_disable_fire == false)
				{
					fly_fire1(ClientListener::UserUpdated(), ou); // TODO обновлять только JSON
				}
			}
		}
		return true;
	}
	catch (std::runtime_error& e)
	{
		dcassert(0);
		CFlyServerJSON::pushError(50, "NmdcHub::extJSONParse error JSON =  " + l_json_result + " error = " + string(e.what()));
	}
	return false;
}
#endif // FLYLINKDC_USE_EXT_JSON

void NmdcHub::myInfoParse(const string& param)
{
	dcassert(!ClientManager::isShutdown());
	string::size_type i = 5;
	string::size_type j = param.find(' ', i);
	if (j == string::npos || j == i)
		return;
	string l_nick = param.substr(i, j - i);
	
	dcassert(!l_nick.empty())
	if (l_nick.empty())
	{
		dcassert(0);
		return;
	}
	i = j + 1;
	
	OnlineUserPtr ou = getUser(l_nick, false, m_bLastMyInfoCommand == DIDNT_GET_YET_FIRST_MYINFO); // При первом коннекте исключаем поиск
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	string l_my_info_before_change;
	if (ou->m_raw_myinfo != param)
	{
		if (ou->m_raw_myinfo.empty())
		{
			// LogManager::message("[!!!!!!!!!!!] First MyINFO = " + param);
		}
		else
		{
			l_my_info_before_change = ou->m_raw_myinfo;
			// LogManager::message("[!!!!!!!!!!!] Change MyINFO New = " + param + " Old = " + ou->m_raw_myinfo);
		}
		ou->m_raw_myinfo = param;
	}
	else
	{
		//dcassert(0);
#ifdef _DEBUG
		LogManager::message("[!!!!!!!!!!!] Dup MyINFO = " + param + " hub = " + getHubUrl());
#endif
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	j = param.find('$', i);
	dcassert(j != string::npos)
	if (j == string::npos)
		return;
	bool l_is_only_desc_change = false;
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	if (!l_my_info_before_change.empty())
	{
		const string::size_type l_pos_begin_tag = param.find('<', i);
		if (l_pos_begin_tag != string::npos)
		{
			const string::size_type l_pos_begin_tag_old = l_my_info_before_change.find('<', i);
			if (l_pos_begin_tag_old != string::npos)
			{
			
				if (strcmp(param.c_str() + l_pos_begin_tag, l_my_info_before_change.c_str() + l_pos_begin_tag_old) == 0)
				{
					l_is_only_desc_change = true;
#ifdef _DEBUG
					LogManager::message("[!!!!!!!!!!!] Only change Description New = " +
					                    param.substr(0, l_pos_begin_tag) + " old = " +
					                    l_my_info_before_change.substr(0, l_pos_begin_tag_old));
#endif
				}
			}
		}
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO 
	string tmpDesc = unescape(param.substr(i, j - i));
	// Look for a tag...
	if (!tmpDesc.empty() && tmpDesc[tmpDesc.size() - 1] == '>')
	{
		const string::size_type x = tmpDesc.rfind('<');
		if (x != string::npos)
		{
			// Hm, we have something...disassemble it...
			//dcassert(tmpDesc.length() > x + 2)
			if (tmpDesc.length()  > x + 2 && l_is_only_desc_change == false)
			{
				const string l_tag = tmpDesc.substr(x + 1, tmpDesc.length() - x - 2);
				updateFromTag(ou->getIdentity(), l_tag); // тяжелая операция с токенами. TODO - оптимизнуть
			}
			ou->getIdentity().setDescription(tmpDesc.erase(x));
		}
	}
	else
	{
		ou->getIdentity().setDescription(tmpDesc); //
	}
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	if (l_is_only_desc_change && !ClientManager::isShutdown())
	{
		fly_fire1(ClientListener::UserDescUpdated(), ou);
		return;
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO 
	
	i = j + 3;
	j = param.find('$', i);
	if (j == string::npos)
		return;
		
	// [!] IRainman fix.
	if (i == j || j - i - 1 == 0)
	{
		// No connection = bot...
		ou->getIdentity().setBot();
		NmdcSupports::setStatus(ou->getIdentity(), param[j - 1]);
	}
	else
	{
		NmdcSupports::setStatus(ou->getIdentity(), param[j - 1], param.substr(i, j - i - 1));
	}
	// [~] IRainman fix.
	
	i = j + 1;
	j = param.find('$', i);
	
	if (j == string::npos)
		return;
	if (j != i)
	{
		ou->getIdentity().setEmail(unescape(param.substr(i, j - i)));
	}
	else
	{
		ou->getIdentity().setEmail(Util::emptyString);
	}
	
	i = j + 1;
	j = param.find('$', i);
	if (j == string::npos)
		return;
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
// Проверим что меняетс только шара
	bool l_is_change_only_share = false;
	if (!l_my_info_before_change.empty())
	{
		if (i < l_my_info_before_change.size())
		{
			if (strcmp(param.c_str() + i, l_my_info_before_change.c_str() + i) != 0)
			{
				if (strncmp(param.c_str(), l_my_info_before_change.c_str(), i) == 0)
				{
					l_is_change_only_share = true;
#ifdef _DEBUG
					LogManager::message("[!!!!!!!!!!!] Only change Share New = " +
					                    param.substr(i) + " old = " +
					                    l_my_info_before_change.substr(i) + " l_nick = " + l_nick + " hub = " + getHubUrl());
#endif
				}
			}
		}
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	
	auto l_share_size = Util::toInt64(param.c_str() + i); // Иногда шара бывает == -1 http://www.flickr.com/photos/96019675@N02/9732534452/
	if (l_share_size < 0)
	{
		dcassert(l_share_size >= 0);
		l_share_size = 0;
		LogManager::message("ShareSize < 0 !, param = " + param);
	}
	changeBytesSharedL(ou->getIdentity(), l_share_size);
	{
		CFlyFastLock(m_cs_virus);
		if (m_virus_nick.find(l_nick) == m_virus_nick.end())
		{
			if (CFlylinkDBManager::getInstance()->is_virus_bot(l_nick, l_share_size, ou->getIdentity().m_is_real_user_ip_from_hub ? ou->getIdentity().getIpRAW() : boost::asio::ip::address_v4()))
			{
				m_virus_nick.insert(l_nick);
			}
		}
	}
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
	if (l_is_change_only_share && !ClientManager::isShutdown())
	{
		fly_fire1(ClientListener::UserShareUpdated(), ou);
		return;
	}
#endif // FLYLINKDC_USE_CHECK_CHANGE_MYINFO 
	
	if (!ClientManager::isShutdown())
	{
#ifdef FLYLINKDC_USE_EXT_JSON
		/*
		string l_ext_json_param;
		{
		    CFlyReadLock(*m_cs);
		    const auto l_find_ext_json = m_ext_json_deferred.find(l_nick);
		    if (l_find_ext_json != m_ext_json_deferred.end())
		    {
		        l_ext_json_param = l_find_ext_json->second;
		    }
		}
		if (!l_ext_json_param.empty())
		{
		    extJSONParse(l_ext_json_param, true);
		    CFlyWriteLock(*m_cs);
		    //m_ext_json_deferred.erase(l_nick);
		}
		*/
		
#endif
		dcassert(!ClientManager::isShutdown());
		if (!ClientManager::isShutdown())
		{
			fly_fire1(ClientListener::UserUpdated(), ou); // !SMT!-fix
		}
	}
}

void NmdcHub::on(BufferedSocketListener::SearchArrayTTH, CFlySearchArrayTTH& p_search_array) noexcept
{
	if (!ClientManager::isShutdown())
	{
		string l_ip;
		if (isActive())
		{
			l_ip = calcExternalIP();
		}
		for (auto k = m_delay_search.begin(); k != m_delay_search.end(); ++k)
		{
			k->m_is_skip = false;
			p_search_array.push_back(*k);
		}
		m_delay_search.clear();
		if (ShareManager::searchTTHArray(p_search_array, this) == false)
		{
			for (auto j = p_search_array.begin(); j != p_search_array.end(); ++j)
			{
				if (j->m_is_skip)
				{
					m_delay_search.push_back(*j); // https://drdump.com/DumpGroup.aspx?DumpGroupID=264417
				}
			}
			return;
		}
		static int g_id_search_array = 0;
		g_id_search_array++;
		unique_ptr<Socket> l_udp;
		for (auto i = p_search_array.begin(); i != p_search_array.end(); ++i)
		{
			if (i->m_toSRCommand)
			{
				string str = *i->m_toSRCommand;
				// TODO
				// ClientManager::getInstance()->fireIncomingSearch(aSeeker, aString, ClientManagerListener::SEARCH_HIT);
				if (i->m_is_passive)
				{
					// Сформируем ответ на пассивный запрос
					string l_nick = i->m_search.substr(4); //-V112
					// Good, we have a passive seeker, those are easier...
					str[str.length() - 1] = 5;
					str += Text::fromUtf8(l_nick, getEncoding());
					str += '|';
					send(str);
				}
				else
				{
					// Запросы по TTH - покидываем через коротко-живущий фильтр, чтобы исключить лишний дубликатный
					// поиск и паразитный UDP трафик в обратную сторону
					if (i->m_search == l_ip || ConnectionManager::checkDuplicateSearchTTH(i->m_search, i->m_tth))
					{
#ifdef FLYLINKDC_USE_COLLECT_STAT
						CFlylinkDBManager::getInstance()->push_event_statistic("search-a-skip-dup-tth-search", "TTH", param, getIpAsString(), "", getHubUrlAndIP(), l_tth);
#endif
						COMMAND_DEBUG("[~][" + Util::toString(g_id_search_array) + "]$SR [SkipUDP-TTH] " + *i->m_toSRCommand, DebugTask::HUB_IN, getIpPort());
						safe_delete(i->m_toSRCommand);
						continue;
					}
					if (!l_udp)
					{
						l_udp = std::unique_ptr<Socket>(new Socket);
					}
					sendUDPSR(*l_udp, i->m_search, *i->m_toSRCommand, this);
				}
				safe_delete(i->m_toSRCommand);
				COMMAND_DEBUG("[+][" + Util::toString(g_id_search_array) + "]$Search " + i->m_search + " F?T?0?9?TTH:" + i->m_tth.toBase32(), DebugTask::HUB_IN, getIpPort());
			}
			else
			{
				COMMAND_DEBUG("[-][" + Util::toString(g_id_search_array) + "]$Search" + i->m_search + " F?T?0?9?TTH:" + i->m_tth.toBase32(), DebugTask::HUB_IN, getIpPort());
			}
		}
	}
}

void NmdcHub::on(BufferedSocketListener::SearchArrayFile, const CFlySearchArrayFile& p_search_array) noexcept
{
	if (!ClientManager::isShutdown())
	{
		for (auto i = p_search_array.cbegin(); i != p_search_array.end(); ++i)
		{
			// dcassert(i->find(" F?T?0?9?TTH:") == string::npos);
			// dcassert(i->find("?9?TTH:") == string::npos);
			// TODO - научится обрабатывать - поиск по TTH с ограничениями по размеру
			// "x.x.x.x:yyy T?F?57671680?9?TTH:A3VSWSWKCVC4N6EP2GX47OEMGT5ZL52BOS2LAHA"
			if (!ClientManager::isShutdown())
			{
				searchParse(i->m_raw_search, i->m_is_passive); // TODO - у нас уже есть распарсенное
				COMMAND_DEBUG("$Search " + i->m_raw_search, DebugTask::HUB_IN, getIpPort());
			}
		}
	}
}

void NmdcHub::on(BufferedSocketListener::DDoSSearchDetect, const string& p_error) noexcept
{
	fly_fire1(ClientListener::DDoSSearchDetect(), p_error);
}

void NmdcHub::on(BufferedSocketListener::MyInfoArray, StringList& p_myInfoArray) noexcept
{
	for (auto i = p_myInfoArray.cbegin(); i != p_myInfoArray.end() && !ClientManager::isShutdown(); ++i)
	{
		const auto l_utf_line = toUtf8MyINFO(*i);
		myInfoParse(l_utf_line);
		COMMAND_DEBUG("$MyINFO " + *i, DebugTask::HUB_IN, getIpPort());
	}
	p_myInfoArray.clear();
	processAutodetect(true);
}

void NmdcHub::on(BufferedSocketListener::Line, const string& aLine) noexcept
{
	if (!ClientManager::isShutdown())
	{
		Client::on(Line(), aLine); // TODO skip Start
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		if (BOOLSETTING(NMDC_DEBUG))
		{
			fly_fire2(ClientListener::StatusMessage(), this, "<NMDC>" + toUtf8(aLine) + "</NMDC>");
		}
#endif
		onLine(aLine);
	}
}

void NmdcHub::on(BufferedSocketListener::Failed, const string& aLine) noexcept
{
	clearUsers();
	Client::on(Failed(), aLine);
	updateCounts(true);
}

#ifdef RIP_USE_CONNECTION_AUTODETECT
void NmdcHub::RequestConnectionForAutodetect()
{
	const unsigned c_MAX_CONNECTION_REQUESTS_COUNT = 3;
	
	if (m_bAutodetectionPending && m_iRequestCount < c_MAX_CONNECTION_REQUESTS_COUNT)
	{
		bool bWantAutodetect = false;
		const auto l_fav = FavoriteManager::getFavoriteHubEntry(getHubUrl());
		if (ClientManager::getMode(l_fav, &bWantAutodetect) == SettingsManager::INCOMING_FIREWALL_PASSIVE)
		{
			if (bWantAutodetect)
			{
			
				CFlyReadLock(*m_cs);
				for (auto i = m_users.cbegin(); i != m_users.cend() && m_iRequestCount < c_MAX_CONNECTION_REQUESTS_COUNT; ++i)
				{
					if (i->second->getIdentity().isBot() ||
					        i->second->getUser()->getFlags() & User::NMDC_FILES_PASSIVE ||
					        i->second->getUser()->getFlags() & User::NMDC_SEARCH_PASSIVE ||
					        i->first == getMyNick())
						continue;
					// TODO optimize:
					// request for connection from users with fastest connection, or operators
					connectToMe(*i->second, ExpectedMap::REASON_DETECT_CONNECTION);
#ifdef _DEBUG
					dcdebug("[!!!!!!!!!!!!!!] AutoDetect connectToMe! Nick = %s Hub = %s", i->first.c_str(), + getHubUrl().c_str());
					LogManager::message("AutoDetect connectToMe - Nick = " + i->first + " Hub = " + getHubUrl());
#endif
					++m_iRequestCount;
				}
			}
		}
	}
}
#endif // RIP_USE_CONNECTION_AUTODETECT

/**
 * @file
 * $Id: nmdchub.cpp 578 2011-10-04 14:27:51Z bigmuscle $
 */
