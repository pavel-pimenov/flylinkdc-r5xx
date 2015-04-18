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

#include "../FlyFeatures/flyServer.h"

CFlyUnknownCommand NmdcHub::g_unknown_command;
CFlyUnknownCommandArray NmdcHub::g_unknown_command_array;
FastCriticalSection NmdcHub::g_unknown_cs;

NmdcHub::NmdcHub(const string& aHubURL, bool secure) : Client(aHubURL, '|', false), m_supportFlags(0), m_modeChar(0),
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
	clearUsers();
}


#define checkstate() if(state != STATE_NORMAL) return

void NmdcHub::disconnect(bool p_graceless)
{
	Client::disconnect(p_graceless);
	clearUsers();
}

void NmdcHub::connect(const OnlineUser& p_user, const string& p_token, bool p_is_force_passive)
{
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
	webrtc::ReadLockScoped l(*m_cs);
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
			webrtc::ReadLockScoped l(*m_cs);
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
	FastLock l(cs);
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
			webrtc::WriteLockScoped l(*m_cs);
			ou = m_users.insert(make_pair(aNick, getHubOnlineUser().get())).first->second;
			ou->inc();
		}
		ou->getIdentity().setNick(aNick);
	}
	else if (aNick == getMyNick())
	{
		{
			webrtc::WriteLockScoped l(*m_cs);
			ou = m_users.insert(make_pair(aNick, getMyOnlineUser().get())).first->second;
			ou->inc();
		}
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
			webrtc::WriteLockScoped l(*m_cs);
			ou = m_users.insert(make_pair(aNick, newUser)).first->second; //2012-06-09_18-19-42_JBOQDRXR35PEW7OJOPLNPXJSQDETX4IUV3SHOHA_DEF32407_crash-stack-r501-x64-build-10294.dmp
			ou->inc();
		}
		ou->getIdentity().setNick(aNick);
	}
	
	if (!ou->getUser()->getCID().isZero()) // [+] IRainman fix.
	{
		cm->putOnline(ou);
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
	webrtc::ReadLockScoped l(*m_cs);
	const auto& i = m_users.find(aNick); // [3] https://www.box.net/shared/03a0bb07362fc510b8a5
	return i == m_users.end() ? nullptr : i->second; // 2012-04-29_13-38-26_EJMPFXUHZAKEQON7Y6X7EIKZVS3S3GMF43CWO3Y_C95F3090_crash-stack-r501-build-9869.dmp
}

void NmdcHub::putUser(const string& aNick)
{
	OnlineUserPtr ou;
	{
		webrtc::WriteLockScoped l(*m_cs);
		const auto& i = m_users.find(aNick);
		if (i == m_users.end())
			return;
		ou = i->second;
		m_users.erase(i);
		decBytesSharedL(ou->getIdentity());
	}
	
	if (!ou->getUser()->getCID().isZero()) // [+] IRainman fix.
	{
		ClientManager::getInstance()->putOffline(ou); // [2] https://www.box.net/shared/7b796492a460fe528961
	}
	
	fire(ClientListener::UserRemoved(), this, ou); // [+] IRainman fix.
	ou->dec();// [!] IRainman fix memoryleak
}

void NmdcHub::clearUsers()
{
	if (ClientManager::isShutdown())
	{
		webrtc::WriteLockScoped l(*m_cs);
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
			webrtc::WriteLockScoped l(*m_cs);
			u2.swap(m_users);
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
			dcassert(j > 1);
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
			FastLock l(NmdcSupports::g_debugCsUnknownNmdcTagParam);
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
void NmdcHub::NmdcSearch(const SearchParam& p_search_param) noexcept
{
	ClientManagerListener::SearchReply l_re = ClientManagerListener::SEARCH_MISS; // !SMT!-S
	SearchResultList l;
	dcassert(p_search_param.m_max_results > 0);
	dcassert(p_search_param.m_client);
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
			const string name = p_search_param.m_seeker.substr(4); //-V112
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
				str += Text::fromUtf8(name, getEncoding());
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
	LogManager::message("NmdcHub::calcExternalIP() = " + l_result);
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
		const auto i = l_search_param.m_seeker.rfind(':');
		if (i != string::npos)
		{
			const auto k = param.find("?9?TTH:", i); // Если идет запрос по TTH - пропускаем без проверки
			if (k == string::npos)
			{
				if (ConnectionManager::getInstance()->checkIpFlood(l_search_param.m_seeker.substr(0, i), Util::toInt(l_search_param.m_seeker.substr(i + 1)), getIp(), param, getHubUrlAndIP()))
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
void NmdcHub::chatMessageParse(const string& aLine)
{
	// Check if we're being banned...
	if (state != STATE_NORMAL)
	{
		if (Util::findSubString(aLine, "banned") != string::npos)
		{
			setAutoReconnect(false);
		}
	}
	// [+] IRainman fix.
	const string l_utf8_line = toUtf8(aLine);
	
	if ((l_utf8_line.find("Hub-Security") != string::npos) && (l_utf8_line.find("was kicked by") != string::npos))
	{
		fire(ClientListener::StatusMessage(), this, unescape(l_utf8_line), ClientListener::FLAG_IS_SPAM);
		return;
	}
	else if ((l_utf8_line.find("is kicking") != string::npos) && (l_utf8_line.find("because:") != string::npos))
	{
		fire(ClientListener::StatusMessage(), this, unescape(l_utf8_line), ClientListener::FLAG_IS_SPAM);
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
		string::size_type i = l_utf8_line.find('>', 2);
		
		if (i != string::npos)
		{
			if (l_utf8_line.size() <= i + 2)
			{
				// there is nothing after last '>'
				nick.clear();
			}
			else
			{
				nick = l_utf8_line.substr(1, i - 1);
				message = l_utf8_line.substr(i + 2);
				if (isFloodCommand("<Nick>", aLine) == true)
				{
					return;
				}
			}
		}
	}
	if (nick.empty())
	{
		fire(ClientListener::StatusMessage(), this, unescape(l_utf8_line));
		return;
	}
	
	if (message.empty())
	{
		message = l_utf8_line;
	}
	const auto l_user = findUser(nick);
	std::unique_ptr<ChatMessage> chatMessage(new ChatMessage(unescape(message), l_user));
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
			fire(ClientListener::UserUpdated(), chatMessage->m_from);
		}
	}
	chatMessage->translate_me();
	if (!allowChatMessagefromUser(*chatMessage, nick)) // [+] IRainman fix.
		return;
		
	fire(ClientListener::Message(), this, chatMessage);
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
	fire(ClientListener::HubUpdated(), this);
}
//==========================================================================================
void NmdcHub::supportsParse(const string& param)
{
	const StringTokenizer<string> st(param, ' '); // TODO убрать текены. сделать поиском. http://code.google.com/p/flylinkdc/issues/detail?id=1112
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
#ifdef FLYLINKDC_USE_FLYHUB
		else if (*i == "FlyHUB")
		{
			m_supportFlags |= SUPPORTS_FLYHUB;
		}
#endif
	}
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
		fire(ClientListener::HubUserCommand(), this, type, ctx, Util::emptyString, Util::emptyString);
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
		string name = unescape(param.substr(i, j - i));
		// NMDC uses '\' as a separator but both ADC and our internal representation use '/'
		Util::replace("/", "//", name);
		Util::replace("\\", "/", name);
		i = j + 1;
		string command = unescape(param.substr(i, param.length() - i));
		fire(ClientListener::HubUserCommand(), this, type, ctx, name, command);
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
#ifdef FLYLINKDC_USE_FLYHUB
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
#ifdef FLYLINKDC_USE_FLYHUB
			feat.push_back("FlyHUB");
#endif
			
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
		OnlineUserPtr ou = getUser(getMyNick(), false, false); // [!] IRainman fix: use OnlineUserPtr
		validateNick(ou->getIdentity().getNick());
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
		fire(ClientListener::UserUpdated(), ou); // !SMT!-fix
	}
}
//==========================================================================================
void NmdcHub::userIPParse(const string& param)
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
			
			//ClientManager::LockInstance lc; // !SMT!-fix
			
			// Perhaps we should use Lock(cs) here, because
			// some elements of this class can be (theoretically)
			// changed in other thread
			
			// Lock _l(cs); [-] IRainman fix.
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
				}
				else
				{
					dcassert(!l_ip.empty());
					ou->getIdentity().setIp(l_ip);
				}
				v.push_back(ou);
			}
		}
		fire_user_updated(v);
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
			
			// Lock _l(cs); [-] IRainman fix: no needs lock here!
			
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
			
			// Lock _l(cs); [-] IRainman fix: no needs any lock here!
			
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
void NmdcHub::toParse(const string& param)
{
	string::size_type i = param.find("From: "); // [!] IRainman fix: with space after!
	if (i == string::npos)
		return;
		
	i += 6;
	string::size_type j = param.find('$', i);
	if (j == string::npos)
		return;
		
	const string rtNick = param.substr(i, j - 1 - i);
	if (rtNick.empty())
		return;
	const auto l_user_for_message = findUser(rtNick);
	if (l_user_for_message == nullptr)
	{
		LogManager::flood_message("NmdcHub::onLine $To: invalid user - RoLex flood?: rtNick = " + rtNick + " param = " + param);
		return;
	}
	i = j + 1;
	
	if (param.size() < i + 3 || param[i] != '<')
		return;
		
	j = param.find('>', i);
	if (j == string::npos)
		return;
		
	const string fromNick = param.substr(i + 1, j - i - 1);
	if (fromNick.empty() || param.size() < j + 2)
		return;
	unique_ptr<ChatMessage> message(new ChatMessage(unescape(param.substr(j + 2)), findUser(fromNick), nullptr, l_user_for_message));
	//if (message.replyTo == nullptr || message.from == nullptr) [-] IRainman fix.
	{
		if (message->m_replyTo == nullptr)
		{
			// Assume it's from the hub
			message->m_replyTo = getUser(rtNick, false, false); // [!] IRainman fix: use OnlineUserPtr
			message->m_replyTo->getIdentity().setHub();
			fire(ClientListener::UserUpdated(), message->m_replyTo); // !SMT!-fix
		}
		if (message->m_from == nullptr)
		{
			// Assume it's from the hub
			message->m_from = getUser(fromNick, false, false); // [!] IRainman fix: use OnlineUserPtr
			message->m_from->getIdentity().setHub();
			fire(ClientListener::UserUpdated(), message->m_from); // !SMT!-fix
		}
		
		// Update pointers just in case they've been invalidated
		// message.replyTo = findUser(rtNick); [-] IRainman fix. Imposibru!!!
		// message.from = findUser(fromNick); [-] IRainman fix. Imposibru!!!
	}
	
	message->m_to = getMyOnlineUser(); // !SMT!-S [!] IRainman fix.
	
	// [+]IRainman fix: message from you to you is not allowed! Block magical spam.
	if (message->m_to->getUser() == message->m_from->getUser() && message->m_from->getUser() == message->m_replyTo->getUser())
	{
		fire(ClientListener::StatusMessage(), this, message->m_text, ClientListener::FLAG_IS_SPAM);
		LogManager::message("Magic spam message (from you to you) filtered on hub: " + getHubUrl() + ".");
		return;
	}
	if (!allowPrivateMessagefromUser(*message)) // [+] IRainman fix.
		return;
		
	fire(ClientListener::Message(), this, message); // [+]
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
		if (l_is_search)
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
	if (l_is_search)
	{
		dcassert(0);  // Используем void NmdcHub::on(BufferedSocketListener::SearchArrayFile
		searchParse(param, l_is_passive);
	}
	else if (cmd == "MyINFO")
	{
		bMyInfoCommand = true;
		myInfoParse(param); // [+]PPA http://code.google.com/p/flylinkdc/issues/detail?id=1384
	}
#ifdef FLYLINKDC_USE_FLYHUB
	else if (cmd == "FlyINFO")
	{
		bMyInfoCommand = false;
	}
#endif
	else if (cmd == "Quit")
	{
		if (!param.empty())
		{
			putUser(param);
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
	else if(cmd == "UserCommand")
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
		fire(ClientListener::Redirect(), this, param);
	}
	else if (cmd == "HubIsFull")
	{
		fire(ClientListener::HubFull(), this);
	}
	else if (cmd == "ValidateDenide")        // Mind the spelling...
	{
		dcassert(m_client_sock);
		if (m_client_sock)
			m_client_sock->disconnect(false);
		fire(ClientListener::NickTaken(), this);
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
		fire(ClientListener::HubTopic(), this, param);
	}
	// [+] IRainman.
	else if (cmd == "LogedIn")
	{
		messageYouHaweRightOperatorOnThisHub();
	}
	// [~] IRainman.
	else
	if (cmd == "UserComman")
	{
		// Где-то ошибка в плагине - много спама идет на сервер - отрубил нахрен
		const string l_message = "NmdcHub::onLine first unknown command! hub = [" + getHubUrl() + "], command = [" + cmd + "], param = [" + param + "]";
		LogManager::message(l_message);
	}
	else
	{
		//dcassert(0);
		dcdebug("NmdcHub::onLine Unknown command %s\n", aLine.c_str());
		string l_message;
		{
			FastLock l(g_unknown_cs);
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
	FastLock l(g_unknown_cs);
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
		FastLock l(g_unknown_cs);
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

bool NmdcHub::resendMyINFO(bool p_is_force_passive)
{
	if (p_is_force_passive)
	{
		if (m_modeChar == 'P')
			return false; // Уходим из обновления MyINFO - уже находимся в пассивном режиме
	}
	myInfo(false, p_is_force_passive);
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
			l_modeChar = '5';
		else if (isActive())
			l_modeChar = 'A';
		else
			l_modeChar = 'P';
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
#ifndef IRAINMAN_TEMPORARY_DISABLE_XXX_ICON
	switch (SETTING(FLY_GENDER))
	{
		case 1:
			status |= 0x20;
			break;
		case 2:
			status |= 0x40;
			break;
		case 3:
			status |= 0x60;
			break;
	}
#endif
//[+]FlylinkDC
	const string currentCounts = fhe && fhe->getExclusiveHub() ? getCountsIndivid() : getCounts();
//[~]FlylinkDC

	// IRAINMAN_USE_UNICODE_IN_NMDC
	string l_currentMyInfo;
	l_currentMyInfo.resize(256);
	const string l_version = getClientName() + " V:" + getTagVersion();
	string l_description;
	if (isFlySupportHub())
	{
		l_description = MappingManager::getPortmapInfo(false, false);
	}
	else
	{
		l_description = getCurrentDescription();
	}
	l_currentMyInfo.resize(snprintf(&l_currentMyInfo[0], l_currentMyInfo.size() - 1, "$MyINFO $ALL %s %s<%s,M:%c,H:%s,S:%d"
	                                ">$ $%s%c$%s$",
	                                fromUtf8(getMyNick()).c_str(),
	                                fromUtf8Chat(escape(l_description)).c_str(),
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
	if (p_always_send ||
	        (l_currentBytesShared != m_lastBytesShared && m_lastUpdate + l_limit < currentTick) ||
	        l_currentMyInfo != m_lastMyInfo)  // [!] IRainman opt.
	{
		m_lastMyInfo = l_currentMyInfo;
		m_lastBytesShared = l_currentBytesShared;
		send(m_lastMyInfo + Util::toString(l_currentBytesShared) + "$|");
#ifdef FLYLINKDC_USE_FLYHUB
#ifdef _DEBUG
		string m_lastFlyInfo = "$FlyINFO $ALL " + getMyNick() + " Russia$Lipetsk$Beeline";
		send(m_lastFlyInfo + "$|");
#endif
#endif
		m_modeChar = l_modeChar;
		m_lastUpdate = currentTick;
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
	if (isActive() && !l_is_passive)
	{
		tmp2 = calcExternalIP();
	}
	else
	{
		tmp2 = "Hub:" + fromUtf8(getMyNick());
	}
	const string l_search_command = "$Search " + tmp2 + ' ' + c1 + '?' + c2 + '?' + Util::toString(p_search_param.m_size) + '?' + Util::toString(p_search_param.m_file_type + 1) + '?' + tmp + '|';
#ifdef _DEBUG
	const string l_debug_string =  "[Search:" + l_search_command + "][" + (isActive() ? string("Active") : string("Passive")) + " search][Client:" + getHubUrl() + "]";
	dcdebug("[NmdcHub::search] %s \r\n", l_debug_string.c_str());
#endif
	g_last_search_string.clear();
	if (isActive())
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
		if (m_isActivMode)
			g_last_search_string += " [Client:Active]";
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
	checkstate();
	
	privateMessage(aUser->getIdentity().getNick(), aMessage, thirdPerson);
	// Emulate a returning message...
	// Lock l(cs); // !SMT!-fix: no data to lock
	
	// [!] IRainman fix.
	const OnlineUserPtr& me = getMyOnlineUser();
	// fire(ClientListener::PrivateMessage(), this, getMyNick(), me, aUser, me, '<' + getMyNick() + "> " + aMessage, thirdPerson); // !SMT!-S [-] IRainman fix.
	
	unique_ptr<ChatMessage> message(new ChatMessage(aMessage, me, aUser, me, thirdPerson));
	if (!allowPrivateMessagefromUser(*message))
		return;
		
	fire(ClientListener::Message(), this, message);
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
	m_modeChar = 0;
	m_supportFlags = 0;
	m_lastMyInfo.clear();
	m_lastBytesShared = 0;
	m_lastUpdate = 0;
}
// TODO - сделать массовый разбор стартовой MyInfo и сброс напрямую в окно без листенеров?
void NmdcHub::myInfoParse(const string& param) noexcept
{
	string::size_type i = 5;
	string::size_type j = param.find(' ', i);
	if (j == string::npos || j == i)
		return;
	string nick = param.substr(i, j - i);
	
	dcassert(!nick.empty())
	if (nick.empty())
		return;
		
	i = j + 1;
	
	OnlineUserPtr ou = getUser(nick, false, m_bLastMyInfoCommand == DIDNT_GET_YET_FIRST_MYINFO); // При первом коннекте исключаем поиск
	
	j = param.find('$', i);
	dcassert(j != string::npos)
	if (j == string::npos)
		return;
		
	string tmpDesc = unescape(param.substr(i, j - i));
	// Look for a tag...
	if (!tmpDesc.empty() && tmpDesc[tmpDesc.size() - 1] == '>')
	{
		const string::size_type x = tmpDesc.rfind('<');
		if (x != string::npos)
		{
			// Hm, we have something...disassemble it...
			//dcassert(tmpDesc.length() > x + 2)
			if (tmpDesc.length()  > x + 2)
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
		
	auto l_share_size = Util::toInt64(param.c_str() + i); // Иногда шара бывает == -1 http://www.flickr.com/photos/96019675@N02/9732534452/
	if (l_share_size < 0)
	{
		dcassert(l_share_size >= 0);
		l_share_size = 0;
		LogManager::message("ShareSize < 0 !, param = " + param);
	}
	changeBytesSharedL(ou->getIdentity(), l_share_size);
	if (!ClientManager::isShutdown())
	{
		fire(ClientListener::UserUpdated(), ou); // !SMT!-fix
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
		ShareManager::searchTTHArray(p_search_array, this); // https://drdump.com/DumpGroup.aspx?DumpGroupID=264417
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
			searchParse(i->m_raw_search, i->m_is_passive); // TODO - у нас уже есть распасенное
			COMMAND_DEBUG("$Search " + i->m_raw_search, DebugTask::HUB_IN, getIpPort());
		}
	}
}

void NmdcHub::on(BufferedSocketListener::DDoSSearchDetect, const string& p_error) noexcept
{
	fire(ClientListener::DDoSSearchDetect(), p_error);
}

void NmdcHub::on(BufferedSocketListener::MyInfoArray, StringList& p_myInfoArray) noexcept
{
	if (!ClientManager::isShutdown())
	{
		for (auto i = p_myInfoArray.cbegin(); i != p_myInfoArray.end(); ++i)
		{
			const auto l_utf_line = toUtf8MyINFO(*i);
			myInfoParse(l_utf_line);
			COMMAND_DEBUG("$MyINFO " + *i, DebugTask::HUB_IN, getIpPort());
		}
		p_myInfoArray.clear();
		processAutodetect(true);
	}
}

void NmdcHub::on(BufferedSocketListener::Line, const string& aLine) noexcept
{
	if (!ClientManager::isShutdown())
	{
		Client::on(Line(), aLine); // TODO skip Start
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		if (BOOLSETTING(NMDC_DEBUG))
		{
			fire(ClientListener::StatusMessage(), this, "<NMDC>" + toUtf8(aLine) + "</NMDC>");
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
			
				webrtc::ReadLockScoped l(*m_cs);
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
