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

#ifdef STRONG_USE_DHT
#include "../dht/dht.h"
#endif

UserPtr ClientManager::g_uflylinkdc; // [+] IRainman fix: User for message from client.
Identity ClientManager::g_iflylinkdc; // [+] IRainman fix: Identity for User for message from client.
UserPtr ClientManager::g_me; // [+] IRainman fix: this is static object.
CID ClientManager::g_pid; // [+] IRainman fix: this is static object.
bool ClientManager::g_isShutdown = false;
Client::List ClientManager::g_clients;

std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csClients = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csOnlineUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());

ClientManager::OnlineMap ClientManager::g_onlineUsers;
ClientManager::UserMap ClientManager::g_users;
#ifdef IRAINMAN_USE_NICKS_IN_CM
ClientManager::NickMap ClientManager::g_nicks;
#endif

Client* ClientManager::getClient(const string& p_HubURL)
{
	dcassert(p_HubURL == Text::toLower(p_HubURL));
	Client* c;
	if (Util::isAdc(p_HubURL))
	{
		c = new AdcHub(p_HubURL, false);
	}
	else if (Util::isAdcS(p_HubURL))
	{
		c = new AdcHub(p_HubURL, true);
	}
	else if (Util::isNmdcS(p_HubURL))
	{
		c = new NmdcHub(p_HubURL, true);
	}
	else
	{
		c = new NmdcHub(p_HubURL, false);
	}
	
	{
		webrtc::WriteLockScoped l(*g_csClients);
		g_clients.insert(make_pair(c->getHubUrl(), c));
	}
	
	c->addListener(this);
	
	return c;
}
size_t ClientManager::getTotalUsers()
{
	size_t users = 0;
	webrtc::ReadLockScoped l(*g_csClients);
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
		
	webrtc::WriteLockScoped l(*g_csOnlineUsers);
	const OnlinePairC p = g_onlineUsers.equal_range(p_user->getCID());
	for (auto i = p.first; i != p.second; ++i)
	{
#ifdef _DEBUG
		const auto l_old_ip = i->second->getIdentity().getIpAsString();
		if (l_old_ip != p_ip)
		{
			LogManager::getInstance()->message("ClientManager::setIPUser, p_user = " + p_user->getLastNick() + " old ip = " + l_old_ip + " ip = " + p_ip);
		}
#endif
		i->second->getIdentity().setIp(p_ip);
		if (p_udpPort != 0)
		{
			i->second->getIdentity().setUdpPort(p_udpPort);
		}
	}
}

bool ClientManager::getUserParams(const UserPtr& user, uint64_t& p_bytesShared, int& p_slots, int& p_limit, std::string& p_ip)
{
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
	const OnlineUser* u = getOnlineUserL(user);
	if (u)
	{
		// [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'u->getIdentity()' expression repeatedly. clientmanager.h 160
		const auto& i = u->getIdentity();
		p_bytesShared = i.getBytesShared();
		p_slots = i.getSlots();
		p_limit = i.getLimit();
		p_ip = i.getIpAsString();
		
		return true;
	}
	return false;
}

#ifndef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
void ClientManager::getConnectedHubUrls(StringList& p_hub_url)
{
	webrtc::ReadLockScoped l(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		if (i->second->isConnected())
			p_hub_url.push_back(i->second->getHubUrl());
	}
}

void ClientManager::getConnectedHubInfo(HubInfoArray& p_hub_info)
{
	webrtc::ReadLockScoped l(*g_csClients);
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
#endif
void ClientManager::prepareClose()
{
	// http://www.flickr.com/photos/96019675@N02/11475592005/
	/*
	{
	    webrtc::ReadLockScoped l(*g_csClients);
	    for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	    {
	        i->second->removeListeners();
	    }
	}
	*/
	{
		webrtc::WriteLockScoped l(*g_csClients);
		g_clients.clear();
	}
}
void ClientManager::putClient(Client* p_client)
{
	if (!g_isShutdown) // При закрытии не шлем уведомление (на него подписан только фрейм поиска)
	{
		fire(ClientManagerListener::ClientDisconnected(), p_client);
	}
	p_client->removeListeners();
	{
		webrtc::WriteLockScoped l(*g_csClients);
		g_clients.erase(p_client->getHubUrl());
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
	//Lock l(cs); [-] IRainman opt.
	StringList lst;
	if (!priv)
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers); // [+] IRainman opt.
		const OnlinePairC op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubUrl());
		}
	}
	else
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(cid, hintUrl);
		if (u)
			lst.push_back(u->getClientBase().getHubUrl());
	}
	return lst;
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl, bool priv)
{
	//Lock l(cs); [-] IRainman opt.
#ifdef _DEBUG
	LogManager::getInstance()->message("[!!!!!!!] ClientManager::getHubNames cid = " + cid.toBase32() + " hintUrl = " + hintUrl + " priv = " + Util::toString(priv));
#endif
	StringList lst;
	if (!priv)
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers); // [+] IRainman opt.
		const OnlinePairC op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubName()); // https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=114958
		}
	}
	else
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(cid, hintUrl);
		if (u)
			lst.push_back(u->getClientBase().getHubName());
	}
	return lst;
}
//[+]FlylinkDC
StringList ClientManager::getNicks(const CID& p_cid, const string& hintUrl, bool priv)
{
	//Lock l(cs); [-] IRainman opt.
	StringSet ret;
	if (!priv)
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers); // [+] IRainman opt.
		const OnlinePairC op = g_onlineUsers.equal_range(p_cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			ret.insert(i->second->getIdentity().getNick());
		}
	}
	else
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(p_cid, hintUrl);
		if (u)
			ret.insert(u->getIdentity().getNick());
	}
	if (ret.empty())
	{
		// offline
#ifdef IRAINMAN_USE_NICKS_IN_CM
		webrtc::ReadLockScoped l(*g_csUsers);
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
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
	
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
    webrtc::ReadLockScoped l(*g_csOnlineUsers);
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
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
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
    webrtc::ReadLockScoped l(*g_csClients);
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
	webrtc::ReadLockScoped l(*g_csClients);
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
		
	// [-] Lock l(cs); IRainman opt.
	
	string ip_or_host;
	uint16_t port;
	const auto i = ipPort.find(':');
	if (i == string::npos)
	{
		ip_or_host = ipPort;
		port = 411;
	}
	else
	{
		ip_or_host = ipPort.substr(0, i);
		port = static_cast<uint16_t>(Util::toInt(ipPort.substr(i + 1)));
	}
	
	string url;
	boost::system::error_code ec;
	const auto l_ip = boost::asio::ip::address_v4::from_string(ip_or_host, ec);
	//dcassert(!ec);
	
	webrtc::ReadLockScoped l(*g_csClients); // [+] IRainman opt.
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
		webrtc::ReadLockScoped l(*g_csClients);
		const auto& i = g_clients.find(aUrl);
		if (i != g_clients.end())
			return i->second->getEncoding();
	}
	return Text::systemCharset;
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
		webrtc::ReadLockScoped l(*g_csUsers);
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
		webrtc::ReadLockScoped l(*g_csClients);
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
	
	webrtc::WriteLockScoped l(*g_csUsers);
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
	webrtc::WriteLockScoped l(*g_csUsers);
	const UserMap::const_iterator ui = g_users.find(cid);
	if (ui != g_users.end())
	{
		return ui->second;
	}
	if (p_create)
	{
		UserPtr p(new User(cid));
		g_users.insert(make_pair(p->getCID(), p));
		return p;
	}
	return UserPtr();
}

UserPtr ClientManager::findUser(const CID& cid)
{
	webrtc::ReadLockScoped l(*g_csUsers);
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
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
	const OnlinePairC p = g_onlineUsers.equal_range(user->getCID());
	for (auto i = p.first; i != p.second; ++i)
	{
		const auto& l_hub = i->second->getClient().getHubUrl();
		if (l_hub == aHubUrl)
			return i->second->getIdentity().isOp();
	}
	return false;
}

#ifdef IRAINMAN_ENABLE_STEALTH_MODE
bool ClientManager::isStealth(const string& aHubUrl)
{
	dcassert(!aHubUrl.empty());
	webrtc::ReadLockScoped l(*g_csClients);
	const Client::Iter i = g_clients.find(aHubUrl);
	if (i != g_clients.end())
		return i->second->getStealth();
		
	return false;
}
#endif // IRAINMAN_ENABLE_STEALTH_MODE

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

void ClientManager::putOnline(const OnlineUserPtr& ou) noexcept
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
			webrtc::WriteLockScoped l(*g_csOnlineUsers);
			g_onlineUsers.insert(make_pair(user->getCID(), ou.get()));
		}
		
		if (!user->isOnline())
		{
			user->setFlag(User::ONLINE);
			fire(ClientManagerListener::UserConnected(), user);
		}
	}
}

void ClientManager::putOffline(const OnlineUserPtr& ou, bool disconnect) noexcept
{
	dcassert(!isShutdown());
	if (!isShutdown()) // Вернул проверку. падаем http://code.google.com/p/flylinkdc/source/detail?r=15119
	{
		// [!] IRainman fix: don't put any hub to online or offline! Any hubs as user is always offline!
		dcassert(ou->getIdentity().getSID() != AdcCommand::HUB_SID);
		dcassert(!ou->getUser()->getCID().isZero());
		// [~] IRainman fix.
		OnlineIter::difference_type diff = 0;
		{
			webrtc::WriteLockScoped l(*g_csOnlineUsers); // [2]  https://www.box.net/shared/7b796492a460fe528961
			OnlinePair op = g_onlineUsers.equal_range(ou->getUser()->getCID()); // Ищется по одном - научиться убивать сразу массив.
			// [-] dcassert(op.first != op.second); [!] L: this is normal and means that the user is offline.
			for (OnlineIter i = op.first; i != op.second; ++i) // 2012-04-29_13-38-26_VJ7NL3IIKFGQ5D34D4RJGIBVWITPBAX7UKSF6RI_3258847B_crash-stack-r501-build-9869.dmp
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
			if (disconnect)
				ConnectionManager::getInstance()->disconnect(u);
				
			fire(ClientManagerListener::UserDisconnected(), u);
		}
		else if (diff > 1)
		{
			fire(ClientManagerListener::UserUpdated(), ou);
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

void ClientManager::connect(const HintedUser& user, const string& token)
{
	dcassert(!isShutdown());
	if (!isShutdown())
	{
		const bool priv = FavoriteManager::getInstance()->isPrivate(user.hint);
		
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		OnlineUser* u = findOnlineUserL(user, priv);
		
		if (u)
		{
			u->getClientBase().connect(*u, token);
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
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		u = findOnlineUserL(user, priv);
	}
	if (u)
	{
		u->getClientBase().privateMessage(u, msg, thirdPerson);
	}
}

void ClientManager::userCommand(const HintedUser& hintedUser, const UserCommand& uc, StringMap& params, bool compatibility)
{
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
	/** @todo we allow wrong hints for now ("false" param of findOnlineUser) because users
	 * extracted from search results don't always have a correct hint; see
	 * SearchManager::onRES(const AdcCommand& cmd, ...). when that is done, and SearchResults are
	 * switched to storing only reliable HintedUsers (found with the token of the ADC command),
	 * change this call to findOnlineUserHint. */
	OnlineUser* ou = findOnlineUserL(hintedUser.user->getCID(), hintedUser.hint.empty() ? uc.getHub() : hintedUser.hint, false);
	if (!ou
#ifdef STRONG_USE_DHT
	        || ou->getClientBase().m_type == ClientBase::DHT
#endif
	   )
		return;
		
	auto& l_сlient = ou->getClient(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou->getClient()' expression repeatedly. clientmanager.cpp 591
	const string& opChat = l_сlient.getOpChat();
	if (opChat.find('*') == string::npos && opChat.find('?') == string::npos)
		params["opchat"] = opChat;
		
	ou->getIdentity().getParams(params, "user", compatibility);
	l_сlient.getHubIdentity().getParams(params, "hub", false);
	l_сlient.getMyIdentity().getParams(params, "my", compatibility);
	l_сlient.escapeParams(params);
	l_сlient.sendUserCmd(uc, params);
}

void ClientManager::send(AdcCommand& cmd, const CID& cid)
{
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(cid);
	if (i != g_onlineUsers.end())
	{
		OnlineUser& u = *i->second;
		if (cmd.getType() == AdcCommand::TYPE_UDP && !u.getIdentity().isUdpActive())
		{
			if (u.getUser()->isNMDC()
#ifdef STRONG_USE_DHT
			        || u.getClientBase().m_type == Client::DHT
#endif
			   )
				return;
				
			cmd.setType(AdcCommand::TYPE_DIRECT);
			cmd.setTo(u.getIdentity().getSID());
			u.getClient().send(cmd);
		}
		else
		{
			try
			{
				Socket udp;
				udp.writeTo(u.getIdentity().getIpAsString(), u.getIdentity().getUdpPort(), cmd.toString(getMyCID())); // [!] IRainman fix.
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
	webrtc::ReadLockScoped l(*g_csClients);
	dcassert(p_client);
	if (p_client && p_client->isConnected())
	{
		p_client->info(false);
	}
}
void ClientManager::infoUpdated()
{
#ifdef _DEBUG
	static int g_count = 0;
	dcdebug("ClientManager::infoUpdated() count = %d\n", ++g_count);
	LogManager::getInstance()->message("ClientManager::infoUpdated() count = " + Util::toString(g_count));
#endif
	webrtc::ReadLockScoped l(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (c->isConnected())
		{
			c->info(false);
		}
	}
}

void ClientManager::on(NmdcSearch, Client* aClient, const string& aSeeker, Search::SizeModes aSizeMode, int64_t aSize,
                       Search::TypeModes aFileType, const string& aString, bool isPassive) noexcept
{

	ClientManagerListener::SearchReply l_re = ClientManagerListener::SEARCH_MISS; // !SMT!-S
	
	SearchResultList l;
#ifdef PPA_USE_HIGH_LOAD_FOR_SEARCH_ENGINE_IN_DEBUG
	ShareManager::getInstance()->search(l, aString, aSizeMode, aSize, aFileType, aClient, isPassive ? 100 : 200);
#else
	ShareManager::getInstance()->search(l, aString, aSizeMode, aSize, aFileType, aClient, isPassive ? 5 : 10);
#endif
	if (!l.empty())
	{
		l_re = ClientManagerListener::SEARCH_HIT;
		if (isPassive)
		{
			string name = aSeeker.substr(4); //-V112
			// Good, we have a passive seeker, those are easier...
			string str;
			for (auto i = l.cbegin(); i != l.cend(); ++i)
			{
				const SearchResultPtr& sr = *i;
				str += sr->toSR(*aClient);
				str[str.length() - 1] = 5;
//#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
//				str += name;
//#else
				str += Text::fromUtf8(name, aClient->getEncoding());
//#endif
				str += '|';
			}
			
			if (!str.empty())
				aClient->send(str);
				
		}
		else
		{
			try
			{
				// Часто делаем?
				Socket udp;
				string ip, file, proto, query, fragment;
				uint16_t port = 0;
				Util::decodeUrl(aSeeker, proto, ip, port, file, query, fragment);
				ip = Socket::resolve(ip);
				//
				if (port == 0)
					port = 412;
				for (auto i = l.cbegin(); i != l.cend(); ++i)
				{
					const SearchResultPtr& sr = *i;
					udp.writeTo(ip, port, sr->toSR(*aClient));
					
#ifdef FLYLINKDC_USE_COLLECT_STAT
					const string l_sr = sr->toSR(*aClient);
					string l_tth;
					const auto l_tth_pos = l_sr.find("TTH:");
					if (l_tth_pos != string::npos)
						l_tth = l_sr.substr(l_tth_pos + 4, 39);
					CFlylinkDBManager::getInstance()->push_event_statistic("$SR", "UDP-write-dc", l_sr, ip, Util::toString(port), aClient->getHubUrl(), l_tth);
#endif
				}
			}
			catch (Exception& e)
			{
#ifdef _DEBUG
				LogManager::getInstance()->message("ClientManager::on(NmdcSearch, Search caught error= " + e.getError());
#endif
				dcdebug("Search caught error = %s\n", + e.getError().c_str());
			}
		}
	}
	else if (!isPassive && (aFileType == Search::TYPE_TTH) && isTTHBase64(aString)) //[+]FlylinkDC++ opt.
	{
		PartsInfo partialInfo;
		TTHValue aTTH(aString.c_str() + 4);  //[+]FlylinkDC++ opt. //-V112
		if (QueueManager::getInstance()->handlePartialSearch(aTTH, partialInfo))
		{
			l_re = ClientManagerListener::SEARCH_PARTIAL_HIT; // !SMT!-S
			string ip, file, proto, query, fragment;
			uint16_t port = 0;
			Util::decodeUrl(aSeeker, proto, ip, port, file, query, fragment); // TODO - зачем тут такая штука?
			
			try
			{
				AdcCommand cmd = SearchManager::getInstance()->toPSR(true, aClient->getMyNick(), aClient->getIpPort(), aTTH.toBase32(), partialInfo);
				Socket s;
				s.writeTo(Socket::resolve(ip), port, cmd.toString(getMyCID())); // [!] IRainman fix.
			}
			catch (Exception& e)
			{
#ifdef _DEBUG
				LogManager::getInstance()->message("ClientManager::on(NmdcSearch, Partial search caught error = " + e.getError());
				dcdebug("Partial search caught error\n");
#endif
			}
		}
	}
	Speaker<ClientManagerListener>::fire(ClientManagerListener::IncomingSearch(), aSeeker, aString, l_re);
}

void ClientManager::on(AdcSearch, const Client* c, const AdcCommand& adc, const CID& from) noexcept
{
	bool isUdpActive = false;
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		
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
	// [!] IRainman-S
	const string l_Seeker = c->getIpPort();
	StringSearch::List l_reguest;
	ClientManagerListener::SearchReply l_re = SearchManager::getInstance()->respond(adc, from, isUdpActive, l_Seeker, l_reguest);
	for (auto i = l_reguest.cbegin(); i != l_reguest.cend(); ++i)
		Speaker<ClientManagerListener>::fire(ClientManagerListener::IncomingSearch(), l_Seeker, i->getPattern(), l_re);
	// [~] IRainman-S
}

void ClientManager::search(Search::SizeModes aSizeMode, int64_t aSize, Search::TypeModes aFileType, const string& aString, const string& aToken, void* aOwner, bool p_is_force_passive)
{
#ifdef STRONG_USE_DHT
	if (BOOLSETTING(USE_DHT) && aFileType == Search::TYPE_TTH)
		dht::DHT::getInstance()->findFile(aString);
#endif
	webrtc::ReadLockScoped l(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (c->isConnected())
		{
			c->search(aSizeMode, aSize, aFileType, aString, aToken, StringList() /*ExtList*/, aOwner, p_is_force_passive);
		}
	}
}

uint64_t ClientManager::search(const StringList& who, Search::SizeModes aSizeMode, int64_t aSize, Search::TypeModes aFileType, const string& aString, const string& aToken, const StringList& aExtList, void* aOwner, bool p_is_force_passive)
{
#ifdef STRONG_USE_DHT
	if (BOOLSETTING(USE_DHT) && aFileType == Search::TYPE_TTH)
		dht::DHT::getInstance()->findFile(aString, aToken);
#endif
	//Lock l(cs); [-] IRainman opt.
	uint64_t estimateSearchSpan = 0;
	if (who.empty())
	{
		webrtc::ReadLockScoped l(*g_csClients); // [+] IRainman opt.
		for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
			if (i->second->isConnected())
			{
				const uint64_t ret = i->second->search(aSizeMode, aSize, aFileType, aString, aToken, aExtList, aOwner, p_is_force_passive);
				estimateSearchSpan = max(estimateSearchSpan, ret);
			}
	}
	else
	{
		webrtc::ReadLockScoped l(*g_csClients); // [+] IRainman opt.
		for (auto it = who.cbegin(); it != who.cend(); ++it)
		{
			const string& client = *it;
			
			const auto& i = g_clients.find(client);
			if (i != g_clients.end() && i->second->isConnected())
			{
				const uint64_t ret = i->second->search(aSizeMode, aSize, aFileType, aString, aToken, aExtList, aOwner, p_is_force_passive);
				estimateSearchSpan = max(estimateSearchSpan, ret);
			}
		}
	}
	return estimateSearchSpan;
}

void ClientManager::on(TimerManagerListener::Minute, uint64_t /*aTick*/) noexcept
{
	//Lock l(cs); [-] IRainman opt.
	{
		webrtc::WriteLockScoped l(*g_csUsers);
		// Collect some garbage...
#ifdef _DEBUG
		CFlyLog l_log("[ClientManager::Minute GC]");
#endif
		UserMap::const_iterator i = g_users.begin();
		while (i != g_users.end())
		{
			if (i->second->unique()) // [3] https://www.box.net/shared/e9e7f84166facfeaacc8
			{
#ifdef IRAINMAN_USE_NICKS_IN_CM
				const auto n = g_nicks.find(i->second->getCID());
				if (n != g_nicks.end())
					g_nicks.erase(n); // TODO 2012-04-18_11-27-14_NZXC23Y2UPQCJZTZP57ZL56NFBSPCWUY42OHBHA_8926815E_crash-stack-r502-beta18-x64-build-9768.dmp
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
	
	TigerHash tiger;
	tiger.update(g_pid.data(), CID::SIZE);
	const CID mycid = CID(tiger.finalize());
	g_me = new User(mycid);
	
	g_uflylinkdc = new User(g_pid);
	
	g_iflylinkdc.setSID(AdcCommand::HUB_SID);
#ifdef IRAINMAN_USE_HIDDEN_USERS
	g_iflylinkdc.setHidden();
#endif
	g_iflylinkdc.setHub();
	g_iflylinkdc.setUser(g_uflylinkdc);
	// [~] IRainman fix.
	g_me->setLastNick(nick);
	g_users.insert(make_pair(g_me->getCID(), g_me));
}
/* [-]
const CID& ClientManager::getMyPID()
{
    if (pid.isZero())
        pid = CID(SETTING(PRIVATE_ID));
    return pid;
}

CID ClientManager::getMyCID()
{
    TigerHash tiger;
    tiger.update(getMyPID().data(), CID::SIZE);
    return CID(tiger.finalize());
}
[~] IRainman fix */

#ifdef IRAINMAN_USE_NICKS_IN_CM
void ClientManager::updateNick(const UserPtr& p_user, const string& p_nick) noexcept
{
	dcassert(!isShutdown());
	//if (isShutdown()) return;
	
	// [-] dcassert(!p_nick.empty()); [-] IRainman fix: this is normal, if the user is offline.
	if (!p_nick.empty())
	{
		webrtc::WriteLockScoped l(*g_csUsers);
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

const string& ClientManager::getMyNick(const string& hubUrl) const // [!] IRainman opt.
{
#ifdef IRAINMAN_CORRRECT_CALL_FOR_CLIENT_MANAGER_DEBUG
	dcassert(!hubUrl.empty());
#endif
	webrtc::ReadLockScoped l(*g_csClients);
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
		*pbWantAutodetect = false;
#endif
	if (!p_hub)
		return SETTING(INCOMING_CONNECTIONS);
		
	int mode = 0;
	if (p_hub)
	{
		switch (p_hub->getMode())
		{
			case 1 :
				mode = SettingsManager::INCOMING_DIRECT;
				break;
			case 2 :
				mode = SettingsManager::INCOMING_FIREWALL_PASSIVE;
				break;
			default:
			{
				mode = SETTING(INCOMING_CONNECTIONS);
#ifdef RIP_USE_CONNECTION_AUTODETECT
				// If autodetection turned on, use passive mode until
				// active mode detected
				if (mode != SettingsManager::INCOMING_FIREWALL_PASSIVE && SETTING(INCOMING_AUTODETECT_FLAG) &&
				        !(Util::isAdcHub(aHubUrl)) // [!] IRainman temporary fix http://code.google.com/p/flylinkdc/issues/detail?id=363
				   )
				{
					mode = SettingsManager::INCOMING_FIREWALL_PASSIVE;
					if (pbWantAutodetect)
						*pbWantAutodetect = true;
				}
#endif
			}
		}
	}
	else
	{
		mode = SETTING(INCOMING_CONNECTIONS);
	}
	return mode;
}

void ClientManager::cancelSearch(void* aOwner)
{
	webrtc::ReadLockScoped l(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		i->second->cancelSearch(aOwner);
	}
}

#ifdef STRONG_USE_DHT
OnlineUserPtr ClientManager::findDHTNode(const CID& cid)
{
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
	
	OnlinePairC op = g_onlineUsers.equal_range(cid);
	for (auto i = op.first; i != op.second; ++i)
	{
		OnlineUser* ou = i->second;
		
		// user not in DHT, so don't bother with other hubs
		if (!ou->getUser()->isSet(User::DHT0))
			break;
			
		if (ou->getClientBase().m_type == Client::DHT)
			return ou;
	}
	return nullptr;
}
#endif

void ClientManager::on(Connected, const Client* c) noexcept
{
	fire(ClientManagerListener::ClientConnected(), c);
}

void ClientManager::on(UserUpdated, const Client*, const OnlineUserPtr& user) noexcept
{
	if (!isShutdown())
	{
		fire(ClientManagerListener::UserUpdated(), user);
	}
}

void ClientManager::on(UsersUpdated, const Client* client, const OnlineUserList& l) noexcept
{
	dcassert(!isShutdown());
	for (auto i = l.cbegin(), iend = l.cend(); i != iend; ++i)
	{
		updateNick(*i); // TODO проверить что меняется именно ник - иначе не звать. или разбить UsersUpdated на UsersUpdated + UsersUpdatedNick
#ifdef _DEBUG
//		LogManager::getInstance()->message("ClientManager::on(UsersUpdated nick = " + (*i)->getUser()->getLastNick());
#endif
		// [-] fire(ClientManagerListener::UserUpdated(), *i); [-] IRainman fix: No needs to update user twice.
	}
}

void ClientManager::updateNick(const OnlineUserPtr& p_online_user)
{
	const string& l_nick_from_identity = p_online_user->getIdentity().getNick();
	if (p_online_user->getUser()->getLastNick().empty())
	{
		p_online_user->getUser()->setLastNick(l_nick_from_identity);
		dcassert(p_online_user->getUser()->getLastNick() != l_nick_from_identity); // TODO поймать когда это бывает?
	}
	else
	{
#ifdef _DEBUG
		//dcassert(0);
//			LogManager::getInstance()->message("[DUP] updateNick(const OnlineUserPtr& p_online_user) ! nick==nick == "
//				+ l_nick_from_identity + " p_online_user->getUser()->getLastNick() = " + p_online_user->getUser()->getLastNick());
#endif
	}
}

void ClientManager::on(HubUpdated, const Client* c) noexcept
{
	dcassert(!isShutdown());
	fire(ClientManagerListener::ClientUpdated(), c);
}

void ClientManager::on(Failed, const Client* client, const string&) noexcept
{
	fire(ClientManagerListener::ClientDisconnected(), client);
}

void ClientManager::on(HubUserCommand, const Client* client, int aType, int ctx, const string& name, const string& command) noexcept
{
	if (BOOLSETTING(HUB_USER_COMMANDS))
	{
		if (aType == UserCommand::TYPE_REMOVE)
		{
			int cmd = FavoriteManager::getInstance()->findUserCommand(name, client->getHubUrl());
			if (cmd != -1)
			{
				FavoriteManager::getInstance()->removeUserCommand(cmd);
			}
		}
		else if (aType == UserCommand::TYPE_CLEAR)
		{
			FavoriteManager::getInstance()->removeHubUserCommands(ctx, client->getHubUrl());
		}
		else
		{
			FavoriteManager::getInstance()->addUserCommand(aType, ctx, UserCommand::FLAG_NOSAVE, name, command, "", client->getHubUrl());
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
	string rawCommand = ou.getClient().getRawCommand(aRawCommand);
	if (!rawCommand.empty())
	{
		StringMap ucParams;
		
		UserCommand uc = UserCommand(0, 0, 0, 0, "", rawCommand, "", "");
		userCommand(HintedUser(ou.getUser(), ou.getClient().getHubUrl()), uc, ucParams, true);
	}
}

void ClientManager::setListLength(const UserPtr& p, const string& listLen)
{
	webrtc::ReadLockScoped l(*g_csOnlineUsers); // TODO Write
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
void ClientManager::fileListDisconnected(const UserPtr& p)
{
	string report;
	Client* c = nullptr;
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end()
#ifdef STRONG_USE_DHT
		        && i->second->getClientBase().m_type != ClientBase::DHT
#endif
		   )
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
				getInstance()->sendRawCommand(*ou, SETTING(DISCONNECT_RAW));
			}
		}
	}
	cheatMessage(c, report);
}

void ClientManager::connectionTimeout(const UserPtr& p)
{
	string report;
	bool remove = false;
	Client* c = nullptr;
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end()
#ifdef STRONG_USE_DHT
		        && i->second->getClientBase().m_type != ClientBase::DHT
#endif
		   )
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
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end()
#ifdef STRONG_USE_DHT
		        || i->second->getClientBase().m_type == ClientBase::DHT
#endif
		   )
			return;
			
		ou = i->second;
		auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou->getIdentity()' expression repeatedly. cheatmanager.h 127
		
		const int64_t statedSize = id.getBytesShared();
		const int64_t realSize = dl->getTotalSize();
		
		const double multiplier = (100 + double(SETTING(PERCENT_FAKE_SHARE_TOLERATED))) / 100;
		const int64_t sizeTolerated = (int64_t)(realSize * multiplier);
#ifdef FLYLINKDC_USE_REALSHARED_IDENTITY
		id.setRealBytesShared(realSize);
#endif
		
		if (statedSize > sizeTolerated)
		{
			id.setFakeCardBit(Identity::BAD_LIST | Identity::CHECKED, true);
			string detectString = STRING(CHECK_MISMATCHED_SHARE_SIZE) + " - ";
			if (realSize == 0)
			{
				detectString += STRING(CHECK_0BYTE_SHARE);
			}
			else
			{
				const double qwe = double(statedSize) / double(realSize);
				char buf[128];
				buf[0] = 0;
				snprintf(buf, _countof(buf), CSTRING(CHECK_INFLATED), Util::toString(qwe).c_str()); //-V111
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
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
		if (report.empty())
			report = id.updateClientType(*ou);
		else
#endif
			id.updateClientType(*ou);
			
		client = &(ou->getClient());
	}
	client->updated(ou);
	cheatMessage(client, report);
}

void ClientManager::setClientStatus(const UserPtr& p, const string& aCheatString, const int aRawCommand, bool aBadClient)
{
	Client* client;
	OnlineUserPtr ou;
	string report;
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end()
#ifdef STRONG_USE_DHT
		        || i->second->getClientBase().m_type == ClientBase::DHT
#endif
		   )
			return;
			
		ou = i->second;
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
		report = ou->getIdentity().updateClientType(*ou);
#else
		ou->getIdentity().updateClientType(*ou);
#endif
		if (!aCheatString.empty())
		{
			report += ou->getIdentity().setCheat(ou->getClientBase(), aCheatString, aBadClient);
		}
		if (aRawCommand != -1)
			sendRawCommand(*ou.get(), aRawCommand);
			
		client = &(ou->getClient());
	}
	client->updated(ou);
	cheatMessage(client, report);
}

#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
void ClientManager::setPkLock(const UserPtr& p
                              , const string& aPk, const string& aLock
                             )
{
	Client *client; // !SMT!-fix
	OnlineUserPtr ou;
	{
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end())
			return;
		ou = i->second;
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
		ou->getIdentity().setStringParam("PK", aPk);
		ou->getIdentity().setStringParam("LO", aLock);
#endif
		client = &(ou->getClient()); // !SMT!-fix
	}
	client->updated(ou);
}
#endif // IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY

void ClientManager::setSupports(const UserPtr& p, StringList & aSupports, const uint8_t knownUcSupports) // [!] IRainamn fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
{
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
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
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
void ClientManager::setGenerator(const UserPtr& p, const string& aGenerator)
{
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
		i->second->getIdentity().setStringParam("GE", aGenerator);
}
#endif
void ClientManager::setUnknownCommand(const UserPtr& p, const string& aUnknownCommand)
{
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
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
		webrtc::ReadLockScoped l(*g_csOnlineUsers);
		OnlineUser* ou = findOnlineUserL(user.user->getCID(), user.hint, priv);
		if (!ou
#ifdef STRONG_USE_DHT
		        || ou->getClientBase().m_type == ClientBase::DHT
#endif
		   )
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
	webrtc::ReadLockScoped l(*g_csOnlineUsers);
	const OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		auto& id = i->second->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'i->second' expression repeatedly. cheatmanager.h 285
		id.setCheat(i->second->getClient(), aCheatString, false);
	}
}

/**
 * @file
 * $Id: ClientManager.cpp 571 2011-07-27 19:18:04Z bigmuscle $
 */
