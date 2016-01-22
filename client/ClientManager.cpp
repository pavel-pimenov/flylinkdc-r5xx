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
#include "ShareManager.h"
#include "SearchManager.h"
#include "CryptoManager.h"
#include "SimpleXML.h"
#include "SearchResult.h"
#include "AdcHub.h"
#include "NmdcHub.h"
#include "QueueManager.h"
#include "MappingManager.h"
#include "../FlyFeatures/flyServer.h"

#ifdef STRONG_USE_DHT
#include "../dht/dht.h"
#endif
#ifndef _DEBUG
#include "DbgHelp.h"
#include "../doctor-dump/CrashRpt.h"
#endif


UserPtr ClientManager::g_uflylinkdc; // [+] IRainman fix: User for message from client.
Identity ClientManager::g_iflylinkdc; // [+] IRainman fix: Identity for User for message from client.
UserPtr ClientManager::g_me; // [+] IRainman fix: this is static object.
CID ClientManager::g_pid; // [+] IRainman fix: this is static object.
bool g_isShutdown = false;
bool g_isStartupProcess = true;
bool ClientManager::g_isSpyFrame = false;
ClientManager::ClientList ClientManager::g_clients;
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
OnlineUserList ClientManager::g_UserUpdateQueue;
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csOnlineUsersUpdateQueue = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#endif

std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csClients = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csOnlineUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());

ClientManager::OnlineMap ClientManager::g_onlineUsers;
ClientManager::UserMap ClientManager::g_users;
#ifdef IRAINMAN_USE_NICKS_IN_CM
ClientManager::NickMap ClientManager::g_nicks;
#endif

ClientManager::ClientManager()
{
	TimerManager::getInstance()->addListener(this);
	createMe(SETTING(PRIVATE_ID), SETTING(NICK)); // [+] IRainman fix.
}

ClientManager::~ClientManager()
{
	dcassert(isShutdown());
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
	dcassert(g_UserUpdateQueue.empty());
#endif
}

Client* ClientManager::getClient(const string& p_HubURL, bool p_is_auto_connect)
{
	dcassert(p_HubURL == Text::toLower(p_HubURL));
	Client* c;
	if (Util::isAdc(p_HubURL))
	{
		c = new AdcHub(p_HubURL, false, p_is_auto_connect);
	}
	else if (Util::isAdcS(p_HubURL))
	{
		c = new AdcHub(p_HubURL, true, p_is_auto_connect);
	}
	else if (Util::isNmdcS(p_HubURL))
	{
		c = new NmdcHub(p_HubURL, true, p_is_auto_connect);
	}
	else
	{
		c = new NmdcHub(p_HubURL, false, p_is_auto_connect);
	}
	
	{
		CFlyWriteLock(*g_csClients);
		g_clients.insert(make_pair(c->getHubUrl(), c));
	}
	
	c->addListener(this);
	
	return c;
}
std::map<string, CFlyClientStatistic > ClientManager::getClientStat()
{
	std::map<string, CFlyClientStatistic> l_stat;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		CFlyClientStatistic l_item;
		if (i->second->isConnected())
		{
			l_item.m_count_user = i->second->getUserCount();
			l_item.m_share_size = i->second->getAvailableBytes();
			l_item.m_message_count = i->second->getMessagesCount();
			l_item.m_is_active = i->second->isActive();
			if (l_item.m_message_count)
			{
				i->second->clearMessagesCount();
			}
		}
		l_stat[i->first] = l_item;
	}
	return l_stat;
}

#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
void ClientManager::resetAntivirusInfo()
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		i->second->resetAntivirusInfo();
	}
}
#endif
void ClientManager::shutdown()
{
	dcassert(!isShutdown());
	::g_isShutdown = true;
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
	{
		CFlyWriteLock(*g_csOnlineUsersUpdateQueue);
		g_UserUpdateQueue.clear();
	}
#endif
	TimerManager::getInstance()->removeListener(this);
	/*
	{
	        CFlyLog l_log("[Store last IP]");
	        CFlyReadLock(*g_csUsers);
	        UserMap::const_iterator i = g_users.begin();
	        while (i != g_users.end())
	        {
	            i->second->flushRatio();
	            ++i;
	        }
	    }
	*/
	
}

void ClientManager::clear()
{
	{
		CFlyWriteLock(*g_csOnlineUsers);
		g_onlineUsers.clear();
	}
	{
		CFlyWriteLock(*g_csUsers);
#ifdef IRAINMAN_USE_NICKS_IN_CM
		g_nicks.clear();
#endif
		g_users.clear();
	}
}

size_t ClientManager::getTotalUsers()
{
	size_t users = 0;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		users += i->second->getUserCount();
	}
	return users;
}
void ClientManager::setIPUser(const UserPtr& p_user, const string& p_ip, const uint16_t p_udpPort /* = 0 */)
{
	// [!] TODO FlylinkDC++ Team - Зачем этот метод?
	// Нужен! r8622
	// L: Данный метод предназначен для обновления IP всем юзерам с этим CID.
	if (p_ip.empty())
		return;
		
	CFlyWriteLock(*g_csOnlineUsers);
	const OnlinePairC p = g_onlineUsers.equal_range(p_user->getCID());
	for (auto i = p.first; i != p.second; ++i)
	{
#ifdef _DEBUG
		const auto l_old_ip = i->second->getIdentity().getIpAsString();
		if (l_old_ip != p_ip)
		{
			LogManager::message("ClientManager::setIPUser, p_user = " + p_user->getLastNick() + " old ip = " + l_old_ip + " ip = " + p_ip);
		}
#endif
		i->second->getIdentity().setIp(p_ip);
		if (p_udpPort != 0)
		{
			i->second->getIdentity().setUdpPort(p_udpPort);
		}
	}
}

bool ClientManager::getUserParams(const UserPtr& user, UserParams& p_params)
{
	CFlyReadLock(*g_csOnlineUsers);
	const OnlineUser* u = getOnlineUserL(user);
	if (u)
	{
		// [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'u->getIdentity()' expression repeatedly. clientmanager.h 160
		const auto& i = u->getIdentity();
		p_params.m_bytesShared = i.getBytesShared();
		p_params.m_slots = i.getSlots();
		p_params.m_limit = i.getLimit();
		p_params.m_ip = i.getIpAsString();
		p_params.m_tag = i.getTag();
		p_params.m_nick = i.getNick();
		
		return true;
	}
	return false;
}

#ifndef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
void ClientManager::getConnectedHubUrls(StringList& p_hub_url)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
			p_hub_url.push_back(i->second->getHubUrl());
	}
}

void ClientManager::getConnectedHubInfo(HubInfoArray& p_hub_info)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
		{
			HubInfo l_info;
			l_info.m_hub_url  = i->second->getHubUrl();
			l_info.m_hub_name = i->second->getHubName();
			l_info.m_is_op = i->second->getMyIdentity().isOp();
			p_hub_info.push_back(l_info);
		}
	}
}
#endif // IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
void ClientManager::prepareClose()
{
	// http://www.flickr.com/photos/96019675@N02/11475592005/
	/*
	{
	    CFlyReadLock(*g_csClients);
	    for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	    {
	        i->second->removeListeners();
	    }
	}
	*/
	{
		CFlyWriteLock(*g_csClients);
		g_clients.clear();
	}
}
void ClientManager::putClient(Client* p_client)
{
	p_client->removeListeners();
	{
		CFlyWriteLock(*g_csClients);
		g_clients.erase(p_client->getHubUrl());
	}
	if (!isShutdown()) // При закрытии не шлем уведомление (на него подписан только фрейм поиска)
	{
		fly_fire1(ClientManagerListener::ClientDisconnected(), p_client);
	}
	p_client->shutdown();
	delete p_client;
}

StringList ClientManager::getHubs(const CID& cid, const string& hintUrl)
{
	return getHubs(cid, hintUrl, FavoriteManager::getInstance()->isPrivate(hintUrl));
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl)
{
	return getHubNames(cid, hintUrl, FavoriteManager::getInstance()->isPrivate(hintUrl));
}

StringList ClientManager::getNicks(const CID& cid, const string& hintUrl)
{
	return getNicks(cid, hintUrl, FavoriteManager::getInstance()->isPrivate(hintUrl));
}

StringList ClientManager::getHubs(const CID& cid, const string& hintUrl, bool priv)
{
	//CFlyLock(cs); [-] IRainman opt.
	StringList lst;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
		const OnlinePairC op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubUrl());
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(cid, hintUrl);
		if (u)
			lst.push_back(u->getClientBase().getHubUrl());
	}
	return lst;
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl, bool priv)
{
	//CFlyLock(cs); [-] IRainman opt.
#ifdef _DEBUG
	//LogManager::message("[!!!!!!!] ClientManager::getHubNames cid = " + cid.toBase32() + " hintUrl = " + hintUrl + " priv = " + Util::toString(priv));
#endif
	StringList lst;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
		const OnlinePairC op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubName()); // https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=114958
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(cid, hintUrl);
		if (u)
			lst.push_back(u->getClientBase().getHubName());
	}
	return lst;
}
//[+]FlylinkDC
StringList ClientManager::getAntivirusNicks(const CID& p_cid)
{
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	StringSet ret;
	CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
	const OnlinePairC op = g_onlineUsers.equal_range(p_cid);
	for (auto i = op.first; i != op.second; ++i)
	{
		if (i->second->getIdentity().calcVirusType() & ~Identity::VT_CALC_AVDB)
		{
			ret.insert(i->second->getIdentity().getVirusDesc());
		}
	}
	if (!ret.empty())
		return StringList(ret.begin(), ret.end());
	else
		return StringList();
#else
	return StringList();
#endif
}
StringList ClientManager::getNicks(const CID& p_cid, const string& hintUrl, bool priv)
{
	//CFlyLock(cs); [-] IRainman opt.
	StringSet ret;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
		const OnlinePairC op = g_onlineUsers.equal_range(p_cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			ret.insert(i->second->getIdentity().getNick());
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(p_cid, hintUrl);
		if (u)
			ret.insert(u->getIdentity().getNick());
	}
	if (ret.empty())
	{
		// offline
#ifdef IRAINMAN_USE_NICKS_IN_CM
		CFlyReadLock(*g_csUsers);
		NickMap::const_iterator i = g_nicks.find(p_cid);
		if (i != g_nicks.end())
		{
			ret.insert(i->second);
		}
		else
#endif
		{
			ret.insert('{' + p_cid.toBase32() + '}');
		}
	}
	return StringList(ret.begin(), ret.end());
}

string ClientManager::getStringField(const CID& cid, const string& hint, const char* field) // [!] IRainman fix.
{
	CFlyReadLock(*g_csOnlineUsers);
	
	OnlinePairC p;
	auto u = findOnlineUserHintL(cid, hint, p);
	if (u)
	{
		auto value = u->getIdentity().getStringParam(field);
		if (!value.empty())
		{
			return value;
		}
	}
	
	for (auto i = p.first; i != p.second; ++i)
	{
		auto value = i->second->getIdentity().getStringParam(field);
		if (!value.empty())
		{
			return value;
		}
	}
	return Util::emptyString;
}
/* [-] IRainman: deprecated.
string ClientManager::getConnection(const CID& cid) const
{
    CFlyReadLock(*g_csOnlineUsers);
    OnlineIterC i = g_onlineUsers.find(cid);
    if (i != g_onlineUsers.end())
    {
        const auto limit = i->second->getIdentity().getLimit();
        if (limit != 0)
            return Util::formatBytes(Util::toString(limit)) + '/' + STRING(S);
        else
            return Util::emptyString;
    }
    return STRING(OFFLINE);
}
*/
uint8_t ClientManager::getSlots(const CID& cid) const
{
	CFlyReadLock(*g_csOnlineUsers);
	const OnlineIterC i = g_onlineUsers.find(cid);
	if (i != g_onlineUsers.end())
	{
		return i->second->getIdentity().getSlots();
	}
	return 0;
}
/*
// !SMT!-S
Client* ClientManager::findClient(const string& aUrl) const
{
    CFlyReadLock(*g_csClients);
    for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
    {
        if (i->second->getHubUrl() == aUrl)
        {
            return i->second;
        }
    }
    return nullptr;
}
*/
Client* ClientManager::findClient(const string& p_url) const
{
	dcassert(!p_url.empty());
	CFlyReadLock(*g_csClients);
	const auto& i = g_clients.find(p_url);
	if (i != g_clients.end())
	{
		return i->second;
	}
	return nullptr;
}

string ClientManager::findHub(const string& ipPort)
{
#ifdef IRAINMAN_CORRRECT_CALL_FOR_CLIENT_MANAGER_DEBUG
	dcassert(!ipPort.empty());
#endif
	if (ipPort.empty()) //[+]FlylinkDC++ Team
		return Util::emptyString;
		
	// [-] CFlyLock(cs); IRainman opt.
	
	string ip_or_host;
	uint16_t port = 411;
	Util::parseIpPort(ipPort, ip_or_host, port);
	string url;
	boost::system::error_code ec;
	const auto l_ip = boost::asio::ip::address_v4::from_string(ip_or_host, ec);
	//dcassert(!ec);
	CFlyReadLock(*g_csClients); // [+] IRainman opt.
	for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
	{
		const Client* c = j->second;
		if (c->getPort() == port) // [!] IRainman opt.
		{
			// If exact match is found, return it
			if (ec)
			{
				if (c->getAddress() == ip_or_host)
				{
					url = c->getHubUrl();
					break;
				}
			}
			else if (c->getIp() == l_ip)
			{
				url = c->getHubUrl();
				break;
			}
		}
	}
#ifdef IRAINMAN_CORRRECT_CALL_FOR_CLIENT_MANAGER_DEBUG
	dcassert(!url.empty());
#endif
	return url;
}

string ClientManager::findHubEncoding(const string& aUrl)
{
	if (!aUrl.empty()) //[+]FlylinkDC++ Team
	{
		CFlyReadLock(*g_csClients);
		const auto& i = g_clients.find(aUrl);
		if (i != g_clients.end())
			return i->second->getEncoding();
	}
	return Text::g_systemCharset;
}

UserPtr ClientManager::findLegacyUser(const string& aNick
#ifndef IRAINMAN_USE_NICKS_IN_CM
                                      , const string& aHubUrl
#endif
                                     )
{
	dcassert(!aNick.empty());
	if (!aNick.empty())
	{
#ifdef IRAINMAN_USE_NICKS_IN_CM
		CFlyReadLock(*g_csUsers);
#ifdef _DEBUG
		static int g_count = 0;
		if (++g_count % 1000 == 0)
		{
			dcdebug("ClientManager::findLegacyUser count = %d g_nicks.size() = %d\n", g_count, g_nicks.size());
		}
#endif
		// this be slower now, but it's not called so often
		for (auto i = g_nicks.cbegin(); i != g_nicks.cend(); ++i)
		{
			// [!] IRainman fix: can not be used here insensitive search! Using stricmp replaced by the string comparison operator. https://code.google.com/p/flylinkdc/source/detail?r=14247
			if (i->second == aNick) // TODO - https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=12550  (> 200 падений)
			{
				UserMap::const_iterator u = g_users.find(i->first);
				if (u != g_users.end() && u->second->getCID() == i->first)
					return u->second;
			}
		}
#else // IRAINMAN_USE_NICKS_IN_CM
// [+] IRainman fix.
		CFlyReadLock(*g_csClients);
		if (!aHubUrl.empty())
		{
			const auto& i = g_clients.find(aHubUrl);
			if (i != g_clients.end())
			{
				const auto& ou = i->second->findUser(aNick);
				if (ou)
				{
					return ou->getUser();
				}
			}
		}
		// http://code.google.com/p/flylinkdc/issues/detail?id=1426
		for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
		{
			const auto& ou = j->second->findUser(aNick);
			if (ou)
			{
				return ou->getUser();
			}
		}
		// [~] IRainman fix.
#endif // IRAINMAN_USE_NICKS_IN_CM
	}
	return UserPtr();
}

UserPtr ClientManager::getUser(const string& p_Nick, const string& p_HubURL
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
                               , uint32_t p_HubID
#endif
                               , bool p_first_load
                              )
{
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	dcassert(p_HubID);
#endif
	dcassert(!p_Nick.empty());
	const CID cid = makeCid(p_Nick, p_HubURL);
	
	CFlyWriteLock(*g_csUsers);
//	dcassert(p_first_load == false || p_first_load == true && g_users.find(cid) == g_users.end())
	const auto& l_result_insert = g_users.insert(make_pair(cid, nullptr));
	if (!l_result_insert.second)
	{
		const auto &l_user = l_result_insert.first->second;
#ifdef IRAINMAN_USE_NICKS_IN_CM
		updateNick_internal(l_user, p_Nick); // [!] IRainman fix.
#else
		l_user->setLastNick(p_Nick);
#endif
		l_user->setFlag(User::NMDC); // TODO тут так можно? L: тут так обязательно нужно - этот метод только для nmdc протокола!
		// TODO-2 зачем второй раз прописывать флаг на NMDC
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		//dcassert(l_user->getHubID());
		if (!l_user->getHubID())
		{
			l_user->setHubID(p_HubID); // TODO-3 а это зачем повторно. оно разве может поменяться?
		}
#endif
		return l_user;
	}
	UserPtr p(new User(cid));
	p->setFlag(User::NMDC); // TODO тут так можно? L: тут так обязательно нужно - этот метод только для nmdc протокола!
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	p->setHubID(p_HubID);
#endif
#ifdef IRAINMAN_USE_NICKS_IN_CM
	updateNick_internal(p, p_Nick); // [!] IRainman fix.
#else
	p->setLastNick(p_Nick);
#endif
	l_result_insert.first->second = p;
	return p;
}

UserPtr ClientManager::getUser(const CID& cid, bool p_create)
{
	dcassert(!ClientManager::isShutdown());
	CFlyWriteLock(*g_csUsers);
	const UserMap::const_iterator ui = g_users.find(cid);
	if (ui != g_users.end())
	{
		return ui->second;
	}
	if (p_create)
	{
		UserPtr p(new User(cid));
		g_users.insert(make_pair(p->getCID(), p)); // https://drdump.com/DumpGroup.aspx?DumpGroupID=239463&Login=guest
		return p;
	}
	return UserPtr();
}

UserPtr ClientManager::findUser(const CID& cid)
{
	CFlyReadLock(*g_csUsers);
	const auto& ui = g_users.find(cid);
	if (ui != g_users.end())
	{
		return ui->second;
	}
	return UserPtr();
}

// deprecated
bool ClientManager::isOp(const UserPtr& user, const string& aHubUrl)
{
	CFlyReadLock(*g_csOnlineUsers);
	const OnlinePairC p = g_onlineUsers.equal_range(user->getCID());
	for (auto i = p.first; i != p.second; ++i)
	{
		const auto& l_hub = i->second->getClient().getHubUrl();
		if (l_hub == aHubUrl)
			return i->second->getIdentity().isOp();
	}
	return false;
}

CID ClientManager::makeCid(const string& aNick, const string& aHubUrl)
{
	// [!] IRainman opt: https://code.google.com/p/flylinkdc/source/detail?r=14247
	// [-] const string n = Text::toLower(aNick);
	TigerHash th;
	th.update(aNick.c_str(), aNick.length());
	th.update(aHubUrl.c_str(), aHubUrl.length());
	// Construct hybrid CID from the bits of the tiger hash - should be
	// fairly random, and hopefully low-collision
	return CID(th.finalize());
}

void ClientManager::putOnline(const OnlineUserPtr& ou, bool p_is_fire_online) noexcept
{
	//dcassert(!isShutdown());
	if (!isShutdown()) // Вернул проверку на всякий случай.
	{
		// [!] IRainman fix: don't put any hub to online or offline! Any hubs as user is always offline!
		const auto &user = ou->getUser();// [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'ou->getUser()' expression repeatedly. clientmanager.cpp 464
		dcassert(ou->getIdentity().getSID() != AdcCommand::HUB_SID);
		dcassert(!user->getCID().isZero());
		// [~] IRainman fix.
		{
			CFlyWriteLock(*g_csOnlineUsers);
			g_onlineUsers.insert(make_pair(user->getCID(), ou.get()));
		}
		
		if (!user->isOnline())
		{
			user->setFlag(User::ONLINE);
			if (p_is_fire_online && !ClientManager::isShutdown())
			{
				fly_fire1(ClientManagerListener::UserConnected(), user);
			}
		}
	}
}

void ClientManager::putOffline(const OnlineUserPtr& ou, bool p_is_disconnect) noexcept
{
	//dcassert(!isShutdown());
	if (!isShutdown()) // Вернул проверку. падаем http://code.google.com/p/flylinkdc/source/detail?r=15119
	{
		// [!] IRainman fix: don't put any hub to online or offline! Any hubs as user is always offline!
		dcassert(ou->getIdentity().getSID() != AdcCommand::HUB_SID);
		dcassert(!ou->getUser()->getCID().isZero());
		// [~] IRainman fix.
		OnlineIter::difference_type diff = 0;
		{
			CFlyWriteLock(*g_csOnlineUsers); // [2]  https://www.box.net/shared/7b796492a460fe528961
			OnlinePair op = g_onlineUsers.equal_range(ou->getUser()->getCID()); // Ищется по одном - научиться убивать сразу массив.
			// [-] dcassert(op.first != op.second); [!] L: this is normal and means that the user is offline.
			for (OnlineIter i = op.first; i != op.second; ++i)
			{
				OnlineUser* ou2 = i->second;
				if (ou.get() == ou2)
				{
					diff = distance(op.first, op.second);
					g_onlineUsers.erase(i); //[1] https://www.box.net/shared/87529fffc02ca448431e
					break;
				}
			}
		}
		
		if (diff == 1) //last user
		{
			UserPtr& u = ou->getUser();
			u->unsetFlag(User::ONLINE);
			if (p_is_disconnect)
			{
				ConnectionManager::getInstance()->disconnect(u);
			}
			if (!ClientManager::isShutdown())
			{
				fly_fire1(ClientManagerListener::UserDisconnected(), u);
			}
		}
		else if (diff > 1 && !ClientManager::isShutdown())
		{
			addAsyncOnlineUserUpdated(ou);
		}
	}
}

OnlineUser* ClientManager::findOnlineUserHintL(const CID& cid, const string& hintUrl, OnlinePairC& p)
{
	// [!] IRainman fix: This function need to external lock.
	p = g_onlineUsers.equal_range(cid);
	
	if (p.first == p.second) // no user found with the given CID.
		return nullptr;
		
	if (!hintUrl.empty()) // [+] IRainman fix.
	{
		for (auto i = p.first; i != p.second; ++i)
		{
			OnlineUser* u = i->second;
			if (u->getClientBase().getHubUrl() == hintUrl)
			{
				return u;
			}
		}
	}
	
	return nullptr;
}
void ClientManager::resend_ext_json()
{
	NmdcHub::inc_version_fly_info();
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
		{
			i->second->resendMyINFO(true, false);
		}
	}
}
void ClientManager::upnp_error_force_passive()
{
	CFlyLog l_log("[UPNP error]");
	l_log.log("Foreach clients...");
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
		{
			if (i->second->resendMyINFO(false, true))
			{
				l_log.log("Force passive mode for :" + i->second->getHubUrlAndIP());
			}
			else
			{
				l_log.log("Skip force passive mode for :" + i->second->getHubUrlAndIP());
			}
		}
	}
}
void ClientManager::connect(const HintedUser& p_user, const string& p_token, bool p_is_force_passive, bool& p_is_active_client)
{
	p_is_active_client = false;
	dcassert(!isShutdown());
	if (!isShutdown())
	{
		const bool priv = FavoriteManager::getInstance()->isPrivate(p_user.hint);
		
		CFlyReadLock(*g_csOnlineUsers);
		OnlineUser* u = findOnlineUserL(p_user, priv);
		
		if (u)
		{
			if (p_is_force_passive)
			{
				(&u->getClientBase())->resendMyINFO(false, p_is_force_passive);
			}
			u->getClientBase().connect(*u, p_token, p_is_force_passive);
			p_is_active_client = u->getClientBase().isActive();
			if (p_is_active_client && p_is_force_passive)
			{
				// (&u->getClientBase())->resendMyINFO(false,false); // Вернем активный режим
				// Не делаем это - флуд получается
			}
		}
	}
}

void ClientManager::privateMessage(const HintedUser& user, const string& msg, bool thirdPerson)
{
	const bool priv = FavoriteManager::getInstance()->isPrivate(user.hint);
	OnlineUser* u = nullptr;
	{
		// # u->getClientBase().privateMessage Нельзя выполнять под локом - там внутри есть fire
		// Есть дампы от Mikhail Korbakov где вешаемся в дедлоке.
		// http://www.flickr.com/photos/96019675@N02/11424193335/
		CFlyReadLock(*g_csOnlineUsers);
		u = findOnlineUserL(user, priv);
	}
	if (u)
	{
		u->getClientBase().privateMessage(u, msg, thirdPerson);
	}
}

void ClientManager::userCommand(const HintedUser& hintedUser, const UserCommand& uc, StringMap& params, bool compatibility)
{
	CFlyReadLock(*g_csOnlineUsers);
	/** @todo we allow wrong hints for now ("false" param of findOnlineUser) because users
	 * extracted from search results don't always have a correct hint; see
	 * SearchManager::onRES(const AdcCommand& cmd, ...). when that is done, and SearchResults are
	 * switched to storing only reliable HintedUsers (found with the token of the ADC command),
	 * change this call to findOnlineUserHint. */
	OnlineUser* ou = findOnlineUserL(hintedUser.user->getCID(), hintedUser.hint.empty() ? uc.getHub() : hintedUser.hint, false);
	if (!ou || ou->isDHT())
		return;
		
	auto& l_сlient = ou->getClient();
	const string& opChat = l_сlient.getOpChat();
	if (opChat.find('*') == string::npos && opChat.find('?') == string::npos)
	{
		params["opchat"] = opChat;
	}
	
	ou->getIdentity().getParams(params, "user", compatibility);
	l_сlient.getHubIdentity().getParams(params, "hub", false);
	l_сlient.getMyIdentity().getParams(params, "my", compatibility);
	l_сlient.escapeParams(params);
	l_сlient.sendUserCmd(uc, params);
}

void ClientManager::send(AdcCommand& cmd, const CID& cid)
{
	CFlyReadLock(*g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(cid);
	if (i != g_onlineUsers.end())
	{
		OnlineUser& u = *i->second;
		if (cmd.getType() == AdcCommand::TYPE_UDP && !u.getIdentity().isUdpActive())
		{
			if (u.getUser()->isNMDC() || u.isDHT())
				return;
				
			cmd.setType(AdcCommand::TYPE_DIRECT);
			cmd.setTo(u.getIdentity().getSID());
			u.getClient().send(cmd);
		}
		else
		{
			try
			{
				Socket l_udp;
				l_udp.writeTo(u.getIdentity().getIpAsString(), u.getIdentity().getUdpPort(), cmd.toString(getMyCID())); // [!] IRainman fix.
#ifdef FLYLINKDC_USE_COLLECT_STAT
				const string l_sr = cmd.toString(getMyCID());
				string l_tth;
				const auto l_tth_pos = l_sr.find("TTH:");
				if (l_tth_pos != string::npos)
					l_tth = l_sr.substr(l_tth_pos + 4, 39);
				CFlylinkDBManager::getInstance()->push_event_statistic("$AdcCommand", "UDP-write-adc", l_sr,
				                                                       u.getIdentity().getIpAsString(),
				                                                       Util::toString(u.getIdentity().getUdpPort()),
				                                                       u.getClient().getHubUrlAndIP(),
				                                                       l_tth);
#endif
			}
			catch (const SocketException&)
			{
				dcdebug("Socket exception sending ADC UDP command\n");
			}
		}
	}
}
void ClientManager::infoUpdated(Client* p_client)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		CFlyReadLock(*g_csClients);
		dcassert(p_client);
		if (p_client && p_client->isConnected())
		{
			p_client->info(false);
		}
	}
}
void ClientManager::infoUpdated(bool p_is_force /* = false*/)
{
#ifdef _DEBUG
	static int g_count = 0;
	dcdebug("ClientManager::infoUpdated() count = %d\n", ++g_count);
	LogManager::message("ClientManager::infoUpdated() count = " + Util::toString(g_count));
#endif
	dcassert(!ClientManager::isShutdown());
	if (ClientManager::isShutdown())
		return;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (ClientManager::isShutdown())
		{
			if (c->isConnected())
			{
				if (p_is_force && c->isFlySupportHub())
				{
					c->info(true);
				}
				else
				{
					c->info(false);
				}
			}
		}
	}
}
// TODO
/*
void ClientManager::on(TTHSearch, Client* aClient, const string& aSeeker, const TTHValue& aTTH, bool isPassive) noexcept
{
    dcassert(0);
}
*/
//=================================================================================================================
void ClientManager::fireIncomingSearch(const string& aSeeker, const string& aString, ClientManagerListener::SearchReply p_re)
{
	if (g_isSpyFrame)
	{
		Speaker<ClientManagerListener>::fly_fire3(ClientManagerListener::IncomingSearch(), aSeeker, aString, p_re);
	}
}
//=================================================================================================================
void ClientManager::on(AdcSearch, const Client* c, const AdcCommand& adc, const CID& from) noexcept
{
	bool isUdpActive = false;
	{
		CFlyReadLock(*g_csOnlineUsers);
		
		OnlinePairC op = g_onlineUsers.equal_range((from));
		for (auto i = op.first; i != op.second; ++i)
		{
			const OnlineUserPtr& u = i->second;
			if (&u->getClient() == c)
			{
				isUdpActive = u->getIdentity().isUdpActive();
				break;
			}
		}
	}
	// [!] IRainman
	const string l_Seeker = c->getIpPort();
	StringSearch::List l_reguest;
	const ClientManagerListener::SearchReply l_re = SearchManager::getInstance()->respond(adc, from, isUdpActive, l_Seeker, l_reguest);
	for (auto i = l_reguest.cbegin(); i != l_reguest.cend(); ++i)
	{
		Speaker<ClientManagerListener>::fly_fire3(ClientManagerListener::IncomingSearch(), l_Seeker, i->getPattern(), l_re);
	}
	// [~] IRainman
}

void ClientManager::search(const SearchParamOwner& p_search_param)
{
#ifdef STRONG_USE_DHT
	if (BOOLSETTING(USE_DHT) && p_search_param.m_file_type == Search::TYPE_TTH)
		dht::DHT::getInstance()->findFile(p_search_param.m_filter);
#endif
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (c->isConnected())
		{
			SearchParamToken l_search_param_token;
			l_search_param_token.m_token = p_search_param.m_token;
			l_search_param_token.m_is_force_passive_searh = p_search_param.m_is_force_passive_searh;
			l_search_param_token.m_size_mode = p_search_param.m_size_mode;
			l_search_param_token.m_size = p_search_param.m_size;
			l_search_param_token.m_file_type = p_search_param.m_file_type;
			l_search_param_token.m_filter = p_search_param.m_filter;
			l_search_param_token.m_owner  = p_search_param.m_owner;
			l_search_param_token.m_ext_list.clear();
			c->search_internal(l_search_param_token);
		}
	}
}

uint64_t ClientManager::multi_search(const SearchParamTokenMultiClient& p_search_param)
{
#ifdef STRONG_USE_DHT
	if (BOOLSETTING(USE_DHT) && p_search_param.m_file_type == Search::TYPE_TTH)
	{
		dht::DHT::getInstance()->findFile(p_search_param.m_filter, p_search_param.m_token);
	}
#endif
	//CFlyLock(cs); [-] IRainman opt.
	uint64_t estimateSearchSpan = 0;
	if (p_search_param.m_clients.empty())
	{
		CFlyReadLock(*g_csClients); // [+] IRainman opt.
		for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
			if (i->second->isConnected())
			{
				const uint64_t ret = i->second->search_internal(p_search_param);
				estimateSearchSpan = max(estimateSearchSpan, ret);
			}
	}
	else
	{
		CFlyReadLock(*g_csClients); // [+] IRainman opt.
		for (auto it = p_search_param.m_clients.cbegin(); it != p_search_param.m_clients.cend(); ++it)
		{
			const string& client = *it;
			
			const auto& i = g_clients.find(client);
			if (i != g_clients.end() && i->second->isConnected())
			{
				const uint64_t ret = i->second->search_internal(p_search_param);
				estimateSearchSpan = max(estimateSearchSpan, ret);
			}
		}
	}
	return estimateSearchSpan;
}

void ClientManager::getOnlineClients(StringSet& p_onlineClients)
{
	CFlyReadLock(*g_csClients);
	p_onlineClients.clear();
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
			p_onlineClients.insert(i->second->getHubUrl());
	}
}
void ClientManager::addAsyncOnlineUserUpdated(const OnlineUserPtr& p_ou)
{
	if (!isShutdown())
	{
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
		CFlyWriteLock(*g_csOnlineUsersUpdateQueue);
		g_UserUpdateQueue.push_back(p_ou);
#else
		fly_fire1(ClientManagerListener::UserUpdated(), p_ou);
#endif
	}
}

#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
void ClientManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	dcassert(!isShutdown());
	if (!isShutdown())
	{
		CFlyReadLock(*g_csOnlineUsersUpdateQueue);
		for (auto i = g_UserUpdateQueue.cbegin(); i != g_UserUpdateQueue.cend(); ++i)
		{
			fly_fire1(ClientManagerListener::UserUpdated(), *i);
		}
	}
	CFlyWriteLock(*g_csOnlineUsersUpdateQueue);
	g_UserUpdateQueue.clear();
}
#endif

void ClientManager::on(TimerManagerListener::Minute, uint64_t /*aTick*/) noexcept
{
	//CFlyLock(cs); [-] IRainman opt.
	{
		CFlyWriteLock(*g_csUsers);
		// Collect some garbage...
#ifdef _DEBUG
		// CFlyLog l_log("[ClientManager::Minute GC]");
#endif
		int l_max_count_flush = 200;
		UserMap::const_iterator i = g_users.begin();
		while (i != g_users.end() && !isShutdown())
		{
			if (l_max_count_flush > 0 && i->second->flushRatio())
			{
				l_max_count_flush--;
			}
			if (i->second.unique()) // [3] https://www.box.net/shared/e9e7f84166facfeaacc8
			{
#ifdef IRAINMAN_USE_NICKS_IN_CM
				const auto n = g_nicks.find(i->second->getCID());
				if (n != g_nicks.end())
					g_nicks.erase(n);
#endif
				g_users.erase(i++);
			}
			else
			{
				++i;
			}
		}
	}
	//[-]PPA infoUpdated();
}
// [!] IRainman fix.
void ClientManager::createMe(const string& cid, const string& nick)
{
	// [!] IRainman fix.
	// [-] me = new User(getMyCID());
	dcassert(!g_me); // [+] IRainman fix: please not init me twice!
	dcassert(g_pid.isZero()); // [+] IRainman fix: please not init pid twice!
	
	g_pid = CID(cid);
	
	TigerHash l_tiger;
	l_tiger.update(g_pid.data(), CID::SIZE);
	const CID l_myCID = CID(l_tiger.finalize());
	g_me = std::make_shared<User>(User(l_myCID));
	
	
#ifndef _DEBUG
	static bool g_is_first = false;
	if (g_is_first == false)
	{
		g_is_first = true;
		extern crash_rpt::CrashRpt g_crashRpt;
		g_crashRpt.AddUserInfoToReport(L"PID", Text::toT(g_pid.toBase32()).c_str());
		g_crashRpt.AddUserInfoToReport(L"CID", Text::toT(l_myCID.toBase32()).c_str());
	}
#endif
	
	g_uflylinkdc = std::make_shared<User>(User(g_pid));
	
	g_iflylinkdc.setSID(AdcCommand::HUB_SID);
#ifdef IRAINMAN_USE_HIDDEN_USERS
	g_iflylinkdc.setHidden();
#endif
	g_iflylinkdc.setHub();
	g_iflylinkdc.setUser(g_uflylinkdc);
	// [~] IRainman fix.
	g_me->setLastNick(nick);
	{
		CFlyWriteLock(*g_csUsers);
		g_users.insert(make_pair(g_me->getCID(), g_me));
	}
}
const CID& ClientManager::getMyCID()
{
	dcassert(g_me);
	return g_me->getCID();
}
const CID& ClientManager::getMyPID()
{
	dcassert(!g_pid.isZero());
	return g_pid;
}

#ifdef IRAINMAN_USE_NICKS_IN_CM
void ClientManager::updateNick(const UserPtr& p_user, const string& p_nick) noexcept
{
	dcassert(!isShutdown());
	//if (isShutdown()) return;
	
	// [-] dcassert(!p_nick.empty()); [-] IRainman fix: this is normal, if the user is offline.
	if (!p_nick.empty())
	{
		CFlyWriteLock(*g_csUsers);
		updateNick_internal(p_user, p_nick); // [!] IRainman fix.
	}
}

void ClientManager::updateNick_internal(const UserPtr& p_user, const string& p_nick) noexcept // [!] IRainman fix.
{
	dcassert(!isShutdown());
	//if (isShutdown()) return;
	
# ifdef _DEBUG
	static int g_count = 0;
	++g_count;
	dcdebug("!ClientManager::updateNick_internal count = %d nicks.size() = %d nick = %s\n", g_count, g_nicks.size(), p_nick.c_str());
# endif
	
	// [+] IRainman fix.
	p_user->setLastNick(p_nick); // [+]
	if (p_user->isSet(User::NMDC)) // [+]
	{
		// [~] IRainman fix.
		auto i = g_nicks.find(p_user->getCID());
		if (i == g_nicks.end())
		{
			// [!] IRainman opt.
			g_nicks.insert(make_pair(p_user->getCID(),
#ifdef IRAINMAN_NON_COPYABLE_USER_DATA_IN_CLIENT_MANAGER
			p_user->getLastNick()
#else
			p_nick
#endif
			                        )); // [+]
			// [-] nicks[(user->getCID())] = nick; // bad_alloc - https://www.box.net/shared/6ed8b4d3217992f740a5
			// [~]
		}
		else
		{
			i->second = p_nick; // [!] IRainman fix.
		}
	}
}
#endif // IRAINMAN_USE_NICKS_IN_CM

const string& ClientManager::findMyNick(const string& hubUrl)
{
#ifdef IRAINMAN_CORRRECT_CALL_FOR_CLIENT_MANAGER_DEBUG
	dcassert(!hubUrl.empty());
#endif
	CFlyReadLock(*g_csClients);
	const auto& i = g_clients.find(hubUrl);
	if (i != g_clients.end())
		return i->second->getMyNick(); // [!] IRainman opt.
	return Util::emptyString;
}

int ClientManager::getMode(const FavoriteHubEntry* p_hub
#ifdef RIP_USE_CONNECTION_AUTODETECT
                           , bool *pbWantAutodetect
#endif
                          )
{
#ifdef RIP_USE_CONNECTION_AUTODETECT
	if (pbWantAutodetect)
	{
		*pbWantAutodetect = false;
	}
#endif
	int l_mode = 0;
	const bool l_is_tcp_port_firewall = CFlyServerJSON::isTestPortOK(SETTING(TCP_PORT), "tcp", true) == false;
	const auto l_type = SETTING(INCOMING_CONNECTIONS);
	if (!p_hub)
	{
		if (l_is_tcp_port_firewall || l_type == SettingsManager::INCOMING_FIREWALL_UPNP && MappingManager::getExternaIP().empty())
		{
#ifdef RIP_USE_CONNECTION_AUTODETECT
			if (pbWantAutodetect)
			{
				*pbWantAutodetect = true;
			}
#endif
			return SettingsManager::INCOMING_FIREWALL_PASSIVE;
		}
		return l_type;
	}
	else
	{
		switch (p_hub->getMode())
		{
			case 1 :
				l_mode = SettingsManager::INCOMING_DIRECT;
				break;
			case 2 :
				l_mode = SettingsManager::INCOMING_FIREWALL_PASSIVE;
				break;
			default:
			{
				l_mode = l_type;
				if (l_is_tcp_port_firewall == true)
				{
					l_mode = SettingsManager::INCOMING_FIREWALL_PASSIVE;
					if (pbWantAutodetect)
					{
						*pbWantAutodetect = true;
					}
				}
				/*
				#ifdef RIP_USE_CONNECTION_AUTODETECT
				                                // If autodetection turned on, use passive mode until
				                                // active mode detected
				                                if (l_mode != SettingsManager::INCOMING_FIREWALL_PASSIVE && SETTING(INCOMING_AUTODETECT_FLAG) &&
				                                        !Util::isAdcHub(p_hub->getServer()) // [!] IRainman temporary fix http://code.google.com/p/flylinkdc/issues/detail?id=363
				                                   )
				                                {
				                                    l_mode = SettingsManager::INCOMING_FIREWALL_PASSIVE;
				                                    if (pbWantAutodetect)
				                                    {
				                                        *pbWantAutodetect = true;
				                                    }
				                                }
				                #endif
				*/
			}
		}
	}
	return l_mode;
}
bool ClientManager::isActive(const FavoriteHubEntry* p_hub
#ifdef RIP_USE_CONNECTION_AUTODETECT
                             , bool *pbWantAutodetect /* = NULL */
#endif
                            )
{
	const auto l_mode = getMode(p_hub
#ifdef RIP_USE_CONNECTION_AUTODETECT
	                            , pbWantAutodetect
#endif
	                           );
	if (l_mode != SettingsManager::INCOMING_FIREWALL_PASSIVE)
		return true;
	else
		return false;
}

void ClientManager::cancelSearch(void* aOwner)
{
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		i->second->cancelSearch(aOwner);
	}
}

#ifdef STRONG_USE_DHT
OnlineUserPtr ClientManager::findDHTNode(const CID& cid)
{
	CFlyReadLock(*g_csOnlineUsers);
	
	OnlinePairC op = g_onlineUsers.equal_range(cid);
	for (auto i = op.first; i != op.second; ++i)
	{
		OnlineUser* ou = i->second;
		
		// user not in DHT, so don't bother with other hubs
		if (!ou->getUser()->isSet(User::DHT0))
			break;
			
		if (ou->isDHT())
			return ou;
	}
	return nullptr;
}
#endif

void ClientManager::on(Connected, const Client* c) noexcept
{
	fly_fire1(ClientManagerListener::ClientConnected(), c);
}

void ClientManager::on(UserUpdatedMyINFO, const OnlineUserPtr& p_ou) noexcept
{
	addAsyncOnlineUserUpdated(p_ou);
}

void ClientManager::on(UsersUpdated, const Client* client, const OnlineUserList& l) noexcept
{
	dcassert(!isShutdown());
	if (!ClientManager::isShutdown())
	{
		for (auto i = l.cbegin(); i != l.cend(); ++i)
		{
			updateNick(*i); // TODO проверить что меняется именно ник - иначе не звать. или разбить UsersUpdated на UsersUpdated + UsersUpdatedNick
#ifdef _DEBUG
			//      LogManager::message("ClientManager::on(UsersUpdated nick = " + (*i)->getUser()->getLastNick());
#endif
			// [-] fly_fire1(ClientManagerListener::UserUpdated(), *i); [-] IRainman fix: No needs to update user twice.
		}
	}
}

void ClientManager::updateNick(const OnlineUserPtr& p_online_user)
{
	if (p_online_user->getUser()->getLastNick().empty())
	{
		const string& l_nick_from_identity = p_online_user->getIdentity().getNick();
		p_online_user->getUser()->setLastNick(l_nick_from_identity);
		dcassert(p_online_user->getUser()->getLastNick() != l_nick_from_identity); // TODO поймать когда это бывает?
	}
	else
	{
#ifdef _DEBUG
		//dcassert(0);
		const string& l_nick_from_identity = p_online_user->getIdentity().getNick();
		LogManager::message("[DUP] updateNick(const OnlineUserPtr& p_online_user) ! nick==nick == "
		                    + l_nick_from_identity + " p_online_user->getUser()->getLastNick() = " + p_online_user->getUser()->getLastNick());
#endif
	}
}

void ClientManager::on(HubUpdated, const Client* c) noexcept
{
	dcassert(!isShutdown());
	fly_fire1(ClientManagerListener::ClientUpdated(), c);
}

void ClientManager::on(Failed, const Client* client, const string&) noexcept
{
	fly_fire1(ClientManagerListener::ClientDisconnected(), client);
}

void ClientManager::on(HubUserCommand, const Client* client, int aType, int ctx, const string& name, const string& command) noexcept
{
	if (BOOLSETTING(HUB_USER_COMMANDS))
	{
		if (aType == UserCommand::TYPE_REMOVE)
		{
			const int cmd = FavoriteManager::findUserCommand(name, client->getHubUrl());
			if (cmd != -1)
			{
				FavoriteManager::removeUserCommandCID(cmd);
			}
		}
		else if (aType == UserCommand::TYPE_CLEAR)
		{
			FavoriteManager::removeHubUserCommands(ctx, client->getHubUrl());
		}
		else
		{
			FavoriteManager::addUserCommand(aType, ctx, UserCommand::FLAG_NOSAVE, name, command, "", client->getHubUrl());
		}
	}
}
////////////////////
/**
 * This file is a part of client manager.
 * It has been divided but shouldn't be used anywhere else.
 */

void ClientManager::sendRawCommand(const OnlineUser& ou, const int aRawCommand)
{
	const string rawCommand = ou.getClient().getRawCommand(aRawCommand);
	if (!rawCommand.empty())
	{
		StringMap ucParams;
		
		UserCommand uc = UserCommand(0, 0, 0, 0, "", rawCommand, "", "");
		userCommand(HintedUser(ou.getUser(), ou.getClient().getHubUrl()), uc, ucParams, true);
	}
}

void ClientManager::setListLength(const UserPtr& p, const string& listLen)
{
	CFlyReadLock(*g_csOnlineUsers); // TODO Write
	OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		i->second->getIdentity().setStringParam("LL", listLen);
	}
}
void ClientManager::cheatMessage(Client* p_client, const string& p_report)
{
	if (p_client && !p_report.empty() && BOOLSETTING(DISPLAY_CHEATS_IN_MAIN_CHAT))
	{
		p_client->cheatMessage(p_report);
	}
}
#ifdef IRAINMAN_INCLUDE_USER_CHECK
void ClientManager::fileListDisconnected(const UserPtr& p)
{
	string report;
	Client* c = nullptr;
	{
		CFlyReadLock(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end()  && !i->second->isDHT())
		{
			OnlineUser* ou = i->second;
			auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou->getIdentity()' expression repeatedly. cheatmanager.h 43
			
			auto fileListDisconnects = id.incFileListDisconnects(); // 8 бит не мало?
			
			if (SETTING(ACCEPTED_DISCONNECTS) == 0)
				return;
				
			if (fileListDisconnects == SETTING(ACCEPTED_DISCONNECTS))
			{
				c = &ou->getClient();
				report = id.setCheat(ou->getClientBase(), "Disconnected file list " + Util::toString(fileListDisconnects) + " times", false);
				sendRawCommand(*ou, SETTING(DISCONNECT_RAW));
			}
		}
	}
	cheatMessage(c, report);
}
#endif // IRAINMAN_INCLUDE_USER_CHECK

void ClientManager::connectionTimeout(const UserPtr& p)
{
	string report;
	bool remove = false;
	Client* c = nullptr;
	{
		CFlyReadLock(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end()  && !i->second->isDHT())
		{
			OnlineUser& ou = *i->second;
			auto& id = ou.getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou.getIdentity()' expression repeatedly. cheatmanager.h 80
			
			auto connectionTimeouts = id.incConnectionTimeouts(); // 8 бит не мало?
			
			if (SETTING(ACCEPTED_TIMEOUTS) == 0)
				return;
				
			if (connectionTimeouts == SETTING(ACCEPTED_TIMEOUTS))
			{
				c = &ou.getClient();
				report = id.setCheat(ou.getClientBase(), "Connection timeout " + Util::toString(connectionTimeouts) + " times", false);
				remove = true;
				sendRawCommand(ou, SETTING(TIMEOUT_RAW));
			}
		}
	}
	if (remove)
	{
		/*try
		{
		    // TODO: remove user check
		}
		catch (...)
		{
		}*/
	}
	cheatMessage(c, report);
}

void ClientManager::checkCheating(const UserPtr& p, DirectoryListing* dl)
{
	Client* client;
	string report;
	OnlineUserPtr ou;
	{
		CFlyReadLock(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end() || i->second->isDHT())
			return;
			
		ou = i->second;
		auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou->getIdentity()' expression repeatedly. cheatmanager.h 127
		
		const int64_t l_statedSize = id.getBytesShared();
		const int64_t l_realSize = dl->getTotalSize();
		
		const double l_multiplier = (100 + double(SETTING(PERCENT_FAKE_SHARE_TOLERATED))) / 100;
		const int64_t l_sizeTolerated = (int64_t)(l_realSize * l_multiplier);
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
		id.setRealBytesShared(realSize);
#endif
		
		if (l_statedSize > l_sizeTolerated)
		{
			id.setFakeCardBit(Identity::BAD_LIST | Identity::CHECKED, true);
			string detectString = STRING(CHECK_MISMATCHED_SHARE_SIZE) + " - ";
			if (l_realSize == 0)
			{
				detectString += STRING(CHECK_0BYTE_SHARE);
			}
			else
			{
				const double qwe = double(l_statedSize) / double(l_realSize);
				char buf[128];
				buf[0] = 0;
				_snprintf(buf, _countof(buf), CSTRING(CHECK_INFLATED), Util::toString(qwe).c_str()); //-V111
				detectString += buf;
			}
			detectString += STRING(CHECK_SHOW_REAL_SHARE);
			
			report = id.setCheat(ou->getClientBase(), detectString, false);
			sendRawCommand(*ou.get(), SETTING(FAKESHARE_RAW));
		}
		else
		{
			id.setFakeCardBit(Identity::CHECKED, true);
		}
		id.updateClientType(*ou);
		
		client = &(ou->getClient());
	}
	//client->updatedMyINFO(ou); // тут тоже не нужна нотификация всем подписчикам
	cheatMessage(client, report);
}

void ClientManager::setClientStatus(const UserPtr& p, const string& aCheatString, const int aRawCommand, bool aBadClient)
{
	Client* client;
	OnlineUserPtr ou;
	string report;
	{
		CFlyReadLock(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end() || i->second->isDHT())
			return;
			
		ou = i->second;
		ou->getIdentity().updateClientType(*ou);
		if (!aCheatString.empty())
		{
			report += ou->getIdentity().setCheat(ou->getClientBase(), aCheatString, aBadClient);
		}
		if (aRawCommand != -1)
		{
			sendRawCommand(*ou.get(), aRawCommand);
		}
		
		client = &(ou->getClient());
	}
	//client->updatedMyINFO(ou); // Не шлем обновку подписчикам!
	cheatMessage(client, report);
}

void ClientManager::setSupports(const UserPtr& p, StringList & aSupports, const uint8_t knownUcSupports) // [!] IRainamn fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
{
	CFlyReadLock(*g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		// [!] IRainman fix.
		auto& id = i->second->getIdentity();
		id.setKnownUcSupports(knownUcSupports);
		/*
		if (p->isNMDC())
		{
		    NmdcSupports::setSupports(id, move(aSupports));
		}
		else
		*/
		{
			AdcSupports::setSupports(id, aSupports);
		}
		// [~] IRainman fix.
	}
}
void ClientManager::setUnknownCommand(const UserPtr& p, const string& aUnknownCommand)
{
	CFlyReadLock(*g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
		i->second->getIdentity().setStringParam("UC", aUnknownCommand);
}

void ClientManager::reportUser(const HintedUser& user)
{
	const bool priv = FavoriteManager::getInstance()->isPrivate(user.hint);
	string report;// [+] FlylinkDC report
	Client* client; // [+] IRainman fix
	{
		CFlyReadLock(*g_csOnlineUsers);
		OnlineUser* ou = findOnlineUserL(user.user->getCID(), user.hint, priv);
		if (!ou || ou->isDHT())
			return;
			
		ou->getIdentity().getReport(report);// [+] FlylinkDC report
		client = &(ou->getClient()); // [!] IRainman fix
		
		// [-] IRainman fix
		// ou->getClient().reportUser(ou->getIdentity());
	}
	client->reportUser(report); // [+] IRainman fix
}

// [+] FlylinkDC
void ClientManager::setFakeList(const UserPtr& p, const string& aCheatString)
{
	CFlyReadLock(*g_csOnlineUsers);
	const OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		auto& id = i->second->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'i->second' expression repeatedly. cheatmanager.h 285
		id.setCheat(i->second->getClient(), aCheatString, false);
	}
}

StringList ClientManager::getUserByIp(const string &p_ip) // TODO - boost
{
	StringList l_result;
	l_result.reserve(1);
	std::unordered_set<string> l_fix_dup;
	CFlyReadLock(*g_csOnlineUsers);
	for (auto i = g_onlineUsers.cbegin(); i != g_onlineUsers.cend(); ++i)
	{
		if (i->second->getIdentity().getIpAsString() == p_ip) // TODO - boost
		{
			const auto l_res = l_fix_dup.insert(i->second->getUser()->getLastNick());
			if (l_res.second == true)
			{
				l_result.push_back(i->second->getUser()->getLastNick());
			}
		}
	}
	return l_result;
}

/**
 * @file
 * $Id: ClientManager.cpp 571 2011-07-27 19:18:04Z bigmuscle $
 */
