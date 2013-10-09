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

SharedCriticalSection ClientManager::g_csClients;
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_CLIENT_MANAGER
SharedCriticalSection ClientManager::g_csOnlineUsers;
SharedCriticalSection ClientManager::g_csUsers;
#else
SharedCriticalSection& ClientManager::g_csOnlineUsers = g_csClients;
SharedCriticalSection& ClientManager::g_csUsers = g_csClients;
#endif

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
		UniqueLock l(g_csClients);
		m_clients.insert(make_pair(c->getHubUrl(), c));
	}
	
	c->addListener(this);
	
	return c;
}

void ClientManager::putClient(Client* aClient)
{
	fire(ClientManagerListener::ClientDisconnected(), aClient);
	aClient->removeListeners();
	{
		UniqueLock l(g_csClients);
		m_clients.erase(aClient->getHubUrl());
	}
	aClient->shutdown();
	delete aClient;
}

StringList ClientManager::getHubs(const CID& cid, const string& hintUrl) const
{
	return getHubs(cid, hintUrl, FavoriteManager::getInstance()->isPrivate(hintUrl));
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl) const
{
	return getHubNames(cid, hintUrl, FavoriteManager::getInstance()->isPrivate(hintUrl));
}

StringList ClientManager::getNicks(const CID& cid, const string& hintUrl) const
{
	return getNicks(cid, hintUrl, FavoriteManager::getInstance()->isPrivate(hintUrl));
}

StringList ClientManager::getHubs(const CID& cid, const string& hintUrl, bool priv) const
{
	//Lock l(cs); [-] IRainman opt.
	StringList lst;
	if (!priv)
	{
		SharedLock l(g_csOnlineUsers); // [+] IRainman opt.
		OnlinePairC op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubUrl());
		}
	}
	else
	{
		SharedLock l(g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(cid, hintUrl);
		if (u)
			lst.push_back(u->getClientBase().getHubUrl());
	}
	return lst;
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl, bool priv) const
{
	//Lock l(cs); [-] IRainman opt.
	StringList lst;
	if (!priv)
	{
		SharedLock l(g_csOnlineUsers); // [+] IRainman opt.
		OnlinePairC op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubName());
		}
	}
	else
	{
		SharedLock l(g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(cid, hintUrl);
		if (u)
			lst.push_back(u->getClientBase().getHubName());
	}
	return lst;
}
//[+]FlylinkDC
StringList ClientManager::getNicks(const CID& p_cid, const string& hintUrl, bool priv) const
{
	//Lock l(cs); [-] IRainman opt.
	StringSet ret;
	if (!priv)
	{
		SharedLock l(g_csOnlineUsers); // [+] IRainman opt.
		OnlinePairC op = g_onlineUsers.equal_range(p_cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			// [-] if (i->second) [-] IRainman fix.
			ret.insert(i->second->getIdentity().getNick());
		}
	}
	else
	{
		SharedLock l(g_csOnlineUsers); // [+] IRainman opt.
		OnlineUser* u = findOnlineUserHintL(p_cid, hintUrl);
		if (u)
			ret.insert(u->getIdentity().getNick());
	}
	if (ret.empty())
	{
		// offline
#ifdef IRAINMAN_USE_NICKS_IN_CM
		SharedLock l(g_csUsers); // [+] IRainman opt.
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

string ClientManager::getStringField(const CID& cid, const string& hint, const char* field) const // [!] IRainman fix.
{
	SharedLock l(g_csOnlineUsers);
	
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
    SharedLock l(g_csOnlineUsers);
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
	SharedLock l(g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(cid);
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
    Lock l(cs);
    for (auto i = m_clients.cbegin(); i != m_clients.cend(); ++i)
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
	SharedLock l(g_csClients);
	Client::Iter i = m_clients.find(p_url);
	if (i != m_clients.end())
	{
		return i->second;
	}
	return nullptr;
}

string ClientManager::findHub(const string& ipPort) const
{
#ifdef IRAINMAN_CORRRECT_CALL_FOR_CLIENT_MANAGER_DEBUG
	dcassert(!ipPort.empty());
#endif
	if (ipPort.empty()) //[+]FlylinkDC++ Team
		return Util::emptyString;
		
	// [-] Lock l(cs); IRainman opt.
	
	string ip;
	uint16_t port;
	const auto i = ipPort.find(':');
	if (i == string::npos)
	{
		ip = ipPort;
		port = 411;
	}
	else
	{
		ip = ipPort.substr(0, i);
		port = static_cast<uint16_t>(Util::toInt(ipPort.substr(i + 1)));
	}
	
	string url;
	SharedLock l(g_csClients); // [+] IRainman opt.
	for (auto j = m_clients.cbegin(); j != m_clients.cend(); ++j)
	{
		const Client* c = j->second;
		if (c->getPort() == port) // [!] IRainman opt.
		{
			// If exact match is found, return it
			if (c->getIp() == ip) // [!] IRainman opt.
			{
				url = c->getHubUrl();
				break;
			}
			// [-] IRainman fix: port is always correct! If the port does not match the first time, then on the same IP address, there are several hubs.
			// [-] // Port is not always correct, so use this as a best guess...
			// [-] url = c->getHubUrl();
			// [~]
		}
	}
#ifdef IRAINMAN_CORRRECT_CALL_FOR_CLIENT_MANAGER_DEBUG
	dcassert(!url.empty());
#endif
	return url;
}

string ClientManager::findHubEncoding(const string& aUrl) const
{
	if (!aUrl.empty()) //[+]FlylinkDC++ Team
	{
		SharedLock l(g_csClients);
		Client::Iter i = m_clients.find(aUrl);
		if (i != m_clients.end())
			return i->second->getEncoding();
	}
	return Text::systemCharset;
}

UserPtr ClientManager::findLegacyUser(const string& aNick
#ifndef IRAINMAN_USE_NICKS_IN_CM
                                      , const string& aHubUrl
#endif
                                     ) const noexcept
{
    dcassert(!aNick.empty());
    if (!aNick.empty())
{
#ifdef IRAINMAN_USE_NICKS_IN_CM
SharedLock l(g_csUsers);
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
SharedLock l(g_csClients);
	const auto i = m_clients.find(aHubUrl);
	if (i != m_clients.end())
	{
		const auto& ou = i->second->findUser(aNick);
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
                              ) noexcept
{
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	dcassert(p_HubID);
#endif
	dcassert(!p_Nick.empty());
	const CID cid = makeCid(p_Nick, p_HubURL);
	
	UniqueLock l(g_csUsers);
	UserMap::const_iterator ui = g_users.find(cid); // 2012-04-29_06-52-32_326HFGI4AAB7UBIMH5QHBGD2AAUTW3MRTX4QLJI_11B3C5DD_crash-stack-r502-beta23-build-9860.dmp
	if (ui != g_users.end())
	{
		const auto &l_user = ui->second; // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'ui->second' expression repeatedly. clientmanager.cpp 375
#ifdef IRAINMAN_USE_NICKS_IN_CM
		updateNick_internal(l_user, p_Nick); // [!] IRainman fix.
#else
		l_user->setLastNick(p_Nick);
#endif
		l_user->setFlag(User::NMDC); // TODO тут так можно? L: тут так обязательно нужно - этот метод только для nmdc протокола!
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		if (!l_user->getHubID())
		{
			l_user->setHubID(p_HubID);
		}
#endif
		return l_user;
	}
	UserPtr p(new User(cid));
	p->setFlag(User::NMDC); // TODO тут так можно? L: тут так обязательно нужно - этот метод только для nmdc протокола!
	g_users.insert(make_pair(p->getCID(), p)); // [2] https://www.box.net/shared/ed0a17c14c625047593a
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	p->setHubID(p_HubID);
#endif
#ifdef IRAINMAN_USE_NICKS_IN_CM
	updateNick_internal(p, p_Nick); // [!] IRainman fix.
#else
	p->setLastNick(p_Nick);
#endif
	return p;
}

UserPtr ClientManager::getUser(const CID& cid, bool p_create /* = true */) noexcept
{
	dcassert(!ClientManager::isShutdown());
	UniqueLock l(g_csUsers);
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

UserPtr ClientManager::findUser(const CID& cid) const noexcept
{
    SharedLock l(g_csUsers);
    const UserMap::const_iterator ui = g_users.find(cid);
    if (ui != g_users.end())
    return ui->second;
    
    return UserPtr();
}

// deprecated
bool ClientManager::isOp(const UserPtr& user, const string& aHubUrl) const
	{
		SharedLock l(g_csOnlineUsers);
		OnlinePairC p = g_onlineUsers.equal_range(user->getCID());
		for (auto i = p.first; i != p.second; ++i)
			if (i->second->getClient().getHubUrl() == aHubUrl)
				return i->second->getIdentity().isOp();
				
		return false;
	}

#ifdef IRAINMAN_ENABLE_STEALTH_MODE
bool ClientManager::isStealth(const string& aHubUrl) const
{
	dcassert(!aHubUrl.empty());
	SharedLock l(g_csClients);
	const Client::Iter i = m_clients.find(aHubUrl);
	if (i != m_clients.end())
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
	dcassert(!isShutdown());
	if (!isShutdown()) // Вернул проверку на всякий случай.
	{
		// [!] IRainman fix: don't put any hub to online or offline! Any hubs as user is always offline!
		const auto &user = ou->getUser();// [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'ou->getUser()' expression repeatedly. clientmanager.cpp 464
		dcassert(ou->getIdentity().getSID() != AdcCommand::HUB_SID);
		dcassert(!user->getCID().isZero());
		// [~] IRainman fix.
		{
			UniqueLock l(g_csOnlineUsers);
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
			UniqueLock l(g_csOnlineUsers); // [2]  https://www.box.net/shared/7b796492a460fe528961
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
			// [-] updateNick(ou); [-] FlylinkDC++ fix.
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

OnlineUser* ClientManager::findOnlineUserHintL(const CID& cid, const string& hintUrl, OnlinePairC& p) const
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
		
		SharedLock l(g_csOnlineUsers);
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
	
	SharedLock l(g_csOnlineUsers);
	OnlineUser* u = findOnlineUserL(user, priv);
	
	if (u)
	{
		u->getClientBase().privateMessage(u, msg, thirdPerson);
	}
}

void ClientManager::userCommand(const HintedUser& hintedUser, const UserCommand& uc, StringMap& params, bool compatibility)
{
	SharedLock l(g_csOnlineUsers);
	/** @todo we allow wrong hints for now ("false" param of findOnlineUser) because users
	 * extracted from search results don't always have a correct hint; see
	 * SearchManager::onRES(const AdcCommand& cmd, ...). when that is done, and SearchResults are
	 * switched to storing only reliable HintedUsers (found with the token of the ADC command),
	 * change this call to findOnlineUserHint. */
	OnlineUser* ou = findOnlineUserL(hintedUser.user->getCID(), hintedUser.hint.empty() ? uc.getHub() : hintedUser.hint, false);
	if (!ou || ou->getClientBase().type == ClientBase::DHT)
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
	SharedLock l(g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(cid);
	if (i != g_onlineUsers.end())
	{
		OnlineUser& u = *i->second;
		if (cmd.getType() == AdcCommand::TYPE_UDP && !u.getIdentity().isUdpActive())
		{
			if (u.getUser()->isNMDC() || u.getClientBase().type == Client::DHT)
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
				udp.writeTo(u.getIdentity().getIp(), u.getIdentity().getUdpPort(), cmd.toString(getMyCID())); // [!] IRainman fix.
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
	SharedLock l(g_csClients);
	dcassert(p_client);
	if (p_client && p_client->isConnected())
	{
		p_client->info(false);
	}
}
void ClientManager::infoUpdated()
{
	SharedLock l(g_csClients);
#ifdef _DEBUG
	static int g_count = 0;
	dcdebug("ClientManager::infoUpdated() count = %d\n", ++g_count);
	LogManager::getInstance()->message("ClientManager::infoUpdated() count = " + Util::toString(g_count));
#endif
	for (auto i = m_clients.cbegin(); i != m_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (c->isConnected())
		{
			c->info(false);
		}
	}
}

void ClientManager::on(NmdcSearch, Client* aClient, const string& aSeeker, Search::SizeModes aSizeMode, int64_t aSize,
                       int aFileType, const string& aString, bool isPassive) noexcept
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
				Socket udp;
				string ip, file, proto, query, fragment;
				uint16_t port = 0;
				Util::decodeUrl(aSeeker, proto, ip, port, file, query, fragment);
				ip = Socket::resolve(ip);
				
				if (port == 0)
					port = 412;
				for (auto i = l.cbegin(); i != l.cend(); ++i)
				{
					const SearchResultPtr& sr = *i;
#ifdef PPA_USE_HIGH_LOAD_FOR_SEARCH_ENGINE_IN_DEBUG
					LogManager::getInstance()->message("udp.writeTo(ip, port, sr->toSR(*aClient))| toSR = "
					                                   + sr->toSR(*aClient) + " ,ip = " + ip + " port = " + Util::toString(port));
#endif
					udp.writeTo(ip, port, sr->toSR(*aClient));
				}
			}
			catch (...)
			{
				// TODO - Log
				dcdebug("Search caught error\n");
			}
		}
	}
	else if (!isPassive && (aFileType == SearchManager::TYPE_TTH) && isTTHBase64(aString)) //[+]FlylinkDC++ opt.
	{
		PartsInfo partialInfo;
		TTHValue aTTH(aString.c_str() + 4);  //[+]FlylinkDC++ opt. //-V112
		if (QueueManager::getInstance()->handlePartialSearch(aTTH, partialInfo))
		{
			l_re = ClientManagerListener::SEARCH_PARTIAL_HIT; // !SMT!-S
			string ip, file, proto, query, fragment;
			uint16_t port = 0;
			Util::decodeUrl(aSeeker, proto, ip, port, file, query, fragment);
			
			try
			{
				AdcCommand cmd = SearchManager::getInstance()->toPSR(true, aClient->getMyNick(), aClient->getIpPort(), aTTH.toBase32(), partialInfo);
				Socket s;
				s.writeTo(Socket::resolve(ip), port, cmd.toString(getMyCID())); // [!] IRainman fix.
			}
			catch (...)
			{
// TODO - Log
				dcdebug("Partial search caught error\n");
			}
		}
	}
	Speaker<ClientManagerListener>::fire(ClientManagerListener::IncomingSearch(), aSeeker, aString, l_re);
}

void ClientManager::on(AdcSearch, const Client* c, const AdcCommand& adc, const CID& from) noexcept
{
	bool isUdpActive = false;
	{
		SharedLock l(g_csOnlineUsers);
		
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

void ClientManager::search(Search::SizeModes aSizeMode, int64_t aSize, int aFileType, const string& aString, const string& aToken, void* aOwner)
{
#ifdef STRONG_USE_DHT
	if (BOOLSETTING(USE_DHT) && aFileType == SearchManager::TYPE_TTH)
		dht::DHT::getInstance()->findFile(aString);
#endif
		
	SharedLock l(g_csClients);
	
	for (auto i = m_clients.cbegin(); i != m_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (c->isConnected())
		{
			c->search(aSizeMode, aSize, aFileType, aString, aToken, StringList() /*ExtList*/, aOwner);
		}
	}
}

uint64_t ClientManager::search(const StringList& who, Search::SizeModes aSizeMode, int64_t aSize, int aFileType, const string& aString, const string& aToken, const StringList& aExtList, void* aOwner)
{
#ifdef STRONG_USE_DHT
	if (BOOLSETTING(USE_DHT) && aFileType == SearchManager::TYPE_TTH)
		dht::DHT::getInstance()->findFile(aString, aToken);
#endif
	//Lock l(cs); [-] IRainman opt.
	uint64_t estimateSearchSpan = 0;
	if (who.empty())
	{
		SharedLock l(g_csClients); // [+] IRainman opt.
		for (auto i = m_clients.cbegin(); i != m_clients.cend(); ++i)
			if (i->second->isConnected())
			{
				const uint64_t ret = i->second->search(aSizeMode, aSize, aFileType, aString, aToken, aExtList, aOwner);
				estimateSearchSpan = max(estimateSearchSpan, ret);
			}
	}
	else
	{
		SharedLock l(g_csClients); // [+] IRainman opt.
		for (auto it = who.cbegin(); it != who.cend(); ++it)
		{
			const string& client = *it;
			
			Client::Iter i = m_clients.find(client);
			if (i != m_clients.end() && i->second->isConnected())
			{
				const uint64_t ret = i->second->search(aSizeMode, aSize, aFileType, aString, aToken, aExtList, aOwner);
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
		UniqueLock l(g_csUsers); // [+] IRainman opt.
		// Collect some garbage...
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

void ClientManager::updateNick(const OnlineUserPtr& user) noexcept
{
	updateNick(user->getUser(), user->getIdentity().getNick());
}
#ifndef IRAINMAN_USE_NICKS_IN_CM
void ClientManager::updateNick(const UserPtr& p_user, const string& p_nick) noexcept
{
	p_user->setLastNick(p_nick);
}
#else
void ClientManager::updateNick(const UserPtr& p_user, const string& p_nick) noexcept
{
	dcassert(!isShutdown());
	//if (isShutdown()) return;

	// [-] dcassert(!p_nick.empty()); [-] IRainman fix: this is normal, if the user is offline.
	if (!p_nick.empty())
	{
		UniqueLock l(g_csUsers);
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
	SharedLock l(g_csClients);
	Client::Iter i = m_clients.find(hubUrl);
	if (i != m_clients.end())
		return i->second->getMyNick(); // [!] IRainman opt.
		
	return Util::emptyString;
}

int ClientManager::getMode(const FavoriteHubEntry* p_hub
#ifdef RIP_USE_CONNECTION_AUTODETECT
                           , bool *pbWantAutodetect
#endif
                          ) const
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
	SharedLock l(g_csClients);
	
	for (auto i = m_clients.cbegin(); i != m_clients.cend(); ++i)
	{
		i->second->cancelSearch(aOwner);
	}
}

#ifdef STRONG_USE_DHT
OnlineUserPtr ClientManager::findDHTNode(const CID& cid) const
{
	SharedLock l(g_csOnlineUsers);
	
	OnlinePairC op = g_onlineUsers.equal_range(cid);
	for (auto i = op.first; i != op.second; ++i)
	{
		OnlineUser* ou = i->second;
		
		// user not in DHT, so don't bother with other hubs
		if (!ou->getUser()->isSet(User::DHT0))
			break;
			
		if (ou->getClientBase().type == Client::DHT)
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
	if (!g_isShutdown)
	{
		fire(ClientManagerListener::UserUpdated(), user);
	}
}

void ClientManager::on(UsersUpdated, const Client* client, const OnlineUserList& l) noexcept
{
	dcassert(!g_isShutdown);
	for (auto i = l.cbegin(), iend = l.cend(); i != iend; ++i)
	{
		updateNick(*i);
		// [-] fire(ClientManagerListener::UserUpdated(), *i); [-] IRainman fix: No needs to update user twice.
	}
}

void ClientManager::on(HubUpdated, const Client* c) noexcept
{
	dcassert(!g_isShutdown);
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

/**
 * @file
 * $Id: ClientManager.cpp 571 2011-07-27 19:18:04Z bigmuscle $
 */
