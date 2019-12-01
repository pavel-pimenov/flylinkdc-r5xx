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

#include "stdinc.h"
#include "ShareManager.h"
#include "AdcHub.h"
#include "NmdcHub.h"
#include "MappingManager.h"
#include "../FlyFeatures/flyServer.h"
#include "../FlyFeatures/AutoUpdate.h"

#ifndef _DEBUG
#include "DbgHelp.h"
#include "../doctor-dump/CrashRpt.h"
#endif


UserPtr ClientManager::g_uflylinkdc;
Identity ClientManager::g_iflylinkdc;
UserPtr ClientManager::g_me;
CID ClientManager::g_pid;
volatile bool g_isShutdown = false;
volatile bool g_isBeforeShutdown = false;
bool g_isStartupProcess = true;
bool ClientManager::g_isSpyFrame = false;
ClientManager::ClientList ClientManager::g_clients;
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
OnlineUserList ClientManager::g_UserUpdateQueue;
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csOnlineUsersUpdateQueue = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#endif

std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csClients = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csOnlineUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ClientManager::g_csUsers = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());

ClientManager::OnlineMap ClientManager::g_onlineUsers;
ClientManager::UserMap ClientManager::g_users;

ClientManager::ClientManager()
{
	if (SETTING(NICK).empty())
	{
		SET_SETTING(NICK, "[fly]" + Util::getRandomNick(15));
	}
	dcassert(!SETTING(NICK).empty());
	createMe(SETTING(PRIVATE_ID), SETTING(NICK));
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
	::g_isBeforeShutdown = true; // ��� ����������
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
	{
		CFlyWriteLock(*g_csOnlineUsersUpdateQueue);
		g_UserUpdateQueue.clear();
	}
#endif
}
void ClientManager::before_shutdown()
{
	dcassert(!isBeforeShutdown());
	::g_isBeforeShutdown = true;
}

void ClientManager::clear()
{
	{
		CFlyWriteLock(*g_csOnlineUsers);
		g_onlineUsers.clear();
	}
	{
		CFlyWriteLock(*g_csUsers);
		//CFlyLock(g_csUsers);
		g_users.clear();
	}
}

unsigned ClientManager::getTotalUsers()
{
	unsigned l_users = 0;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		l_users += i->second->getUserCount();
	}
	return l_users;
}
void ClientManager::setIPUser(const UserPtr& p_user, const string& p_ip, const uint16_t p_udpPort /* = 0 */)
{
	if (p_ip.empty())
		return;
		
	CFlyWriteLock(*g_csOnlineUsers);
	const auto p = g_onlineUsers.equal_range(p_user->getCID());
	for (auto i = p.first; i != p.second; ++i)
	{
#ifdef _DEBUG
//		const auto l_old_ip = i->second->getIdentity().getIpAsString();
//		if (l_old_ip != p_ip)
//		{
//			LogManager::message("ClientManager::setIPUser, p_user = " + p_user->getLastNick() + " old ip = " + l_old_ip + " ip = " + p_ip);
//		}
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
	const OnlineUserPtr u = getOnlineUserL(user);
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
	if (!isBeforeShutdown()) // ��� �������� �� ���� ����������� (�� ���� �������� ������ ����� ������)
	{
		fly_fire1(ClientManagerListener::ClientDisconnected(), p_client);
	}
	p_client->shutdown();
	delete p_client;
}

StringList ClientManager::getHubs(const CID& cid, const string& hintUrl)
{
	return getHubs(cid, hintUrl, FavoriteManager::isPrivate(hintUrl));
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl)
{
	return getHubNames(cid, hintUrl, FavoriteManager::isPrivate(hintUrl));
}

StringList ClientManager::getNicks(const CID& cid, const string& hintUrl)
{
	return getNicks(cid, hintUrl, FavoriteManager::isPrivate(hintUrl));
}

StringList ClientManager::getHubs(const CID& cid, const string& hintUrl, bool priv)
{
	StringList lst;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubUrl());
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers);
		const OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl);
		if (u)
		{
			lst.push_back(u->getClientBase().getHubUrl());
		}
	}
	return lst;
}

StringList ClientManager::getHubNames(const CID& cid, const string& hintUrl, bool priv)
{
#ifdef _DEBUG
	//LogManager::message("[!!!!!!!] ClientManager::getHubNames cid = " + cid.toBase32() + " hintUrl = " + hintUrl + " priv = " + Util::toString(priv));
#endif
	StringList lst;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			lst.push_back(i->second->getClientBase().getHubName()); // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=114958
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers);
		const OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl);
		if (u)
		{
			lst.push_back(u->getClientBase().getHubName());
		}
	}
	return lst;
}

#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
StringList ClientManager::getAntivirusNicks(const CID& p_cid)
{
	StringSet ret;
	CFlyReadLock(*g_csOnlineUsers);
	const auto op = g_onlineUsers.equal_range(p_cid);
	for (auto i = op.first; i != op.second; ++i)
	{
		// ����� ��������� � ���� ������ - ��������
		//if (i->second->getIdentity().calcVirusType() & ~Identity::VT_CALC_AVDB)
		{
			ret.insert(i->second->getIdentity().getVirusDesc());
		}
	}
	if (!ret.empty())
		return StringList(ret.begin(), ret.end());
	else
		return StringList();
	return StringList();
}
#endif

StringList ClientManager::getNicks(const CID& p_cid, const string& hintUrl, bool priv)
{
	StringSet ret;
	if (!priv)
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto op = g_onlineUsers.equal_range(p_cid);
		for (auto i = op.first; i != op.second; ++i)
		{
			ret.insert(i->second->getIdentity().getNick());
		}
	}
	else
	{
		CFlyReadLock(*g_csOnlineUsers);
		const OnlineUserPtr u = findOnlineUserHintL(p_cid, hintUrl);
		if (u)
		{
			ret.insert(u->getIdentity().getNick());
		}
	}
	if (ret.empty())
	{
		ret.insert('{' + p_cid.toBase32() + '}');
	}
	if (ret.empty())
		return StringList();
	else
		return StringList(ret.begin(), ret.end());
}
StringList ClientManager::getNicks(const HintedUser& user)
{
	dcassert(user.user);
	if (user.user)
		return getNicks(user.user->getCID(), user.hint);
	else
		return StringList();
}
StringList ClientManager::getHubNames(const HintedUser& user)
{
	dcassert(user.user);
	if (user.user)
		return getHubNames(user.user->getCID(), user.hint);
	else
		return StringList();
}
bool ClientManager::isConnected(const string& aUrl)
{
	CFlyReadLock(*g_csClients);
	return g_clients.find(aUrl) != g_clients.end();
}
bool ClientManager::isOnline(const UserPtr& aUser)
{
	CFlyReadLock(*g_csOnlineUsers);
	return g_onlineUsers.find(aUser->getCID()) != g_onlineUsers.end();
}
OnlineUserPtr ClientManager::findOnlineUserL(const HintedUser& user, bool priv)
{
	if (user.user)
		return findOnlineUserL(user.user->getCID(), user.hint, priv);
	else
		return OnlineUserPtr();
}
UserPtr ClientManager::findUser(const string& aNick, const string& aHubUrl)
{
	return findUser(makeCid(aNick, aHubUrl));
}
OnlineUserPtr ClientManager::findOnlineUserL(const CID& cid, const string& hintUrl, bool priv)
{
	OnlinePairC p;
	OnlineUserPtr u = findOnlineUserHintL(cid, hintUrl, p);
	if (u) // found an exact match (CID + hint).
		return u;
		
	if (p.first == p.second) // no user found with the given CID.
		return nullptr;
		
	// if the hint hub is private, don't allow connecting to the same user from another hub.
	if (priv)
		return nullptr;
		
	// ok, hub not private, return a random user that matches the given CID but not the hint.
	return p.first->second;
}

string ClientManager::getStringField(const CID& cid, const string& hint, const char* field)
{
	CFlyReadLock(*g_csOnlineUsers);
	
	OnlinePairC p;
	const auto u = findOnlineUserHintL(cid, hint, p);
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

uint8_t ClientManager::getSlots(const CID& cid)
{
	CFlyReadLock(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(cid);
	if (i != g_onlineUsers.end())
	{
		return i->second->getIdentity().getSlots();
	}
	return 0;
}

Client* ClientManager::findClient(const string& p_url)
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
	if (ipPort.empty())
		return Util::emptyString;
		
	string ip_or_host;
	uint16_t port = 411;
	Util::parseIpPort(ipPort, ip_or_host, port);
	string url;
	boost::system::error_code ec;
	const auto l_ip = boost::asio::ip::address_v4::from_string(ip_or_host, ec);
	//dcassert(!ec);
	CFlyReadLock(*g_csClients);
	for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
	{
		const Client* c = j->second;
		if (c->getPort() == port)
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
	if (!aUrl.empty())
	{
		CFlyReadLock(*g_csClients);
		const auto& i = g_clients.find(aUrl);
		if (i != g_clients.end())
			return i->second->getEncoding();
	}
	return Text::g_systemCharset;
}

UserPtr ClientManager::findLegacyUser(const string& aNick, const string& aHubUrl)
{
	dcassert(!aNick.empty());
	if (!aNick.empty())
	{
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
		for (auto j = g_clients.cbegin(); j != g_clients.cend(); ++j)
		{
			const auto& ou = j->second->findUser(aNick);
			if (ou)
			{
				return ou->getUser();
			}
		}
	}
	return UserPtr();
}

UserPtr ClientManager::getUser(const string& p_Nick, const string& p_HubURL, uint32_t p_HubID)
{
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	dcassert(p_HubID);
#endif
	dcassert(!p_Nick.empty());
	const CID cid = makeCid(p_Nick, p_HubURL);
	
	CFlyWriteLock(*g_csUsers);
	//CFlyLock(g_csUsers);
	//  dcassert(p_first_load == false || p_first_load == true && g_users.find(cid) == g_users.end())
	const auto& l_result_insert = g_users.insert(make_pair(cid, std::make_shared<User>(cid, p_Nick, p_HubID)));
	if (!l_result_insert.second)
	{
		const auto &l_user = l_result_insert.first->second;
		l_user->setLastNick(p_Nick);
		l_user->setFlag(User::NMDC); // TODO ��� ��� �����? L: ��� ��� ����������� ����� - ���� ����� ������ ��� nmdc ���������!
		// TODO-2 ����� ������ ��� ����������� ���� �� NMDC
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		//dcassert(l_user->getHubID());
		if (!l_user->getHubID())
		{
			l_user->setHubID(p_HubID); // TODO-3 � ��� ����� ��������. ��� ����� ����� ����������?
		}
#endif
		return l_user;
	}
	l_result_insert.first->second->setFlag(User::NMDC);
	return l_result_insert.first->second;
}

UserPtr ClientManager::createUser(const CID& p_cid, const string& p_nick, uint32_t p_hub_id)
{
	dcassert(!ClientManager::isBeforeShutdown());
	CFlyWriteLock(*g_csUsers);
	//CFlyLock(g_csUsers);
	auto l_item = g_users.insert(make_pair(p_cid, UserPtr()));
	if (l_item.second == false)
	{
		//dcassert(p_nick == l_item.first->second->getLastNick());
		return l_item.first->second;
	}
	l_item.first->second = std::make_shared<User>(p_cid, p_nick, p_hub_id);
	return l_item.first->second;
}

UserPtr ClientManager::findUser(const CID& cid)
{
	CFlyReadLock(*g_csUsers);
	//CFlyLock(g_csUsers);
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
	const auto p = g_onlineUsers.equal_range(user->getCID());
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
	TigerHash th;
	th.update(aNick.c_str(), aNick.length());
	th.update(aHubUrl.c_str(), aHubUrl.length());
	// Construct hybrid CID from the bits of the tiger hash - should be
	// fairly random, and hopefully low-collision
	return CID(th.finalize());
}

void ClientManager::putOnline(const OnlineUserPtr& ou, bool p_is_fire_online) noexcept
{
	if (!isBeforeShutdown())
	{
		const auto& user = ou->getUser();
		dcassert(ou->getIdentity().getSID() != AdcCommand::HUB_SID);
		dcassert(!user->getCID().isZero());
		{
			CFlyWriteLock(*g_csOnlineUsers);
			const auto l_res = g_onlineUsers.insert(make_pair(user->getCID(), ou));
			dcassert(l_res->second);
		}
		
		if (!user->isOnline())
		{
			user->setFlag(User::ONLINE);
			if (p_is_fire_online && !ClientManager::isBeforeShutdown())
			{
				fly_fire1(ClientManagerListener::UserConnected(), user);
			}
		}
	}
}

void ClientManager::putOffline(const OnlineUserPtr& ou, bool p_is_disconnect) noexcept
{
	if (!isBeforeShutdown())
	{
		dcassert(ou->getIdentity().getSID() != AdcCommand::HUB_SID);
		dcassert(!ou->getUser()->getCID().isZero());
		
		OnlineIter::difference_type diff = 0;
		{
			CFlyWriteLock(*g_csOnlineUsers);
			auto op = g_onlineUsers.equal_range(ou->getUser()->getCID());
			for (auto i = op.first; i != op.second; ++i)
			{
				if (ou == i->second)
				{
					diff = distance(op.first, op.second);
					g_onlineUsers.erase(i);
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
				ConnectionManager::disconnect(u);
			}
			if (!ClientManager::isBeforeShutdown())
			{
				fly_fire1(ClientManagerListener::UserDisconnected(), u);
			}
		}
		else if (diff > 1 && !ClientManager::isBeforeShutdown())
		{
			addAsyncOnlineUserUpdated(ou);
		}
	}
}

OnlineUserPtr ClientManager::findOnlineUserHintL(const CID& cid, const string& hintUrl, OnlinePairC& p)
{
	p = g_onlineUsers.equal_range(cid);
	
	if (p.first == p.second) // no user found with the given CID.
		return nullptr;
		
	if (!hintUrl.empty())
	{
		for (auto i = p.first; i != p.second; ++i)
		{
			const OnlineUserPtr u = i->second;
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
	dcassert(!isBeforeShutdown());
	if (!isBeforeShutdown())
	{
		const bool priv = FavoriteManager::isPrivate(p_user.hint);
		
		CFlyReadLock(*g_csOnlineUsers);
		OnlineUserPtr u = findOnlineUserL(p_user, priv);
		
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
				// (&u->getClientBase())->resendMyINFO(false,false); // ������ �������� �����
				// �� ������ ��� - ���� ����������
			}
		}
	}
}

void ClientManager::privateMessage(const HintedUser& user, const string& msg, bool thirdPerson)
{
	const bool priv = FavoriteManager::isPrivate(user.hint);
	OnlineUserPtr u;
	{
		// # u->getClientBase().privateMessage ������ ��������� ��� ����� - ��� ������ ���� fire
		// ���� ����� �� Mikhail Korbakov ��� �������� � �������.
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
	userCommandL(hintedUser, uc, params, compatibility);
}

void ClientManager::userCommandL(const HintedUser& hintedUser, const UserCommand& uc, StringMap& params, bool compatibility)
{
	/** @todo we allow wrong hints for now ("false" param of findOnlineUser) because users
	 * extracted from search results don't always have a correct hint; see
	 * SearchManager::onRES(const AdcCommand& cmd, ...). when that is done, and SearchResults are
	 * switched to storing only reliable HintedUsers (found with the token of the ADC command),
	 * change this call to findOnlineUserHint. */
	if (hintedUser.user)
	{
		OnlineUserPtr ou = findOnlineUserL(hintedUser.user->getCID(), hintedUser.hint.empty() ? uc.getHub() : hintedUser.hint, false);
		if (!ou)
			return;
			
		auto& l_client = ou->getClient();
		const string opChat = l_client.getOpChat();
		if (opChat.find('*') == string::npos && opChat.find('?') == string::npos)
		{
			params["opchat"] = opChat;
		}
		
		ou->getIdentity().getParams(params, "user", compatibility);
		l_client.getHubIdentity().getParams(params, "hub", false);
		l_client.getMyIdentity().getParams(params, "my", compatibility);
		l_client.escapeParams(params);
		l_client.sendUserCmd(uc, params); // TODO - ���� ����� ��� Lock-��
	}
}

void ClientManager::send(AdcCommand& cmd, const CID& cid)
{
	string l_ip;
	uint16_t l_port = 0;
	bool l_is_send = false;
	OnlineUserPtr u;
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(cid);
		if (i != g_onlineUsers.end())
		{
			u = i->second;
			if (cmd.getType() == AdcCommand::TYPE_UDP && !u->getIdentity().isUdpActive())
			{
				if (u->getUser()->isNMDC())
					return;
					
				cmd.setType(AdcCommand::TYPE_DIRECT);
				cmd.setTo(u->getIdentity().getSID());
				l_is_send = true;
			}
			else
			{
				l_ip = u->getIdentity().getIpAsString();
				l_port = u->getIdentity().getUdpPort();
			}
		}
	}
	if (l_is_send)
	{
		l_is_send = true;
		u->getClient().send(cmd);
	}
	if (l_port && !l_ip.empty())
	{
		try
		{
			Socket l_udp;
			l_udp.writeTo(l_ip, l_port, cmd.toString(getMyCID()));
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
		catch (const SocketException& e)
		{
			dcdebug("Socket exception sending ADC UDP command\n");
			LogManager::message("ClientManager::send - Socket exception sending ADC UDP command " + e.getError());
		}
	}
}
void ClientManager::infoUpdated(Client* p_client)
{
	dcassert(p_client);
	if (p_client)
	{
		dcassert(!ClientManager::isBeforeShutdown());
		if (!ClientManager::isBeforeShutdown())
		{
			CFlyReadLock(*g_csClients);
			dcassert(p_client);
			if (p_client && p_client->isConnected())
			{
				p_client->info(false);
			}
		}
	}
}
void ClientManager::infoUpdated(bool p_is_force /* = false*/)
{
#ifdef _DEBUG
	static int g_count = 0;
	dcdebug("ClientManager::infoUpdated() count = %d\n", ++g_count);
	//LogManager::message("ClientManager::infoUpdated() count = " + Util::toString(g_count));
#endif
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
		return;
	CFlyReadLock(*g_csClients);
	for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
	{
		Client* c = i->second;
		if (!ClientManager::isBeforeShutdown())
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
		const auto op = g_onlineUsers.equal_range((from));
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
	const string l_Seeker = c->getIpPort();
	StringSearch::List l_reguest;
	const ClientManagerListener::SearchReply l_re = SearchManager::getInstance()->respond(adc, from, isUdpActive, l_Seeker, l_reguest);
	for (auto i = l_reguest.cbegin(); i != l_reguest.cend(); ++i)
	{
		Speaker<ClientManagerListener>::fly_fire3(ClientManagerListener::IncomingSearch(), l_Seeker, i->getPattern(), l_re);
	}
}

void ClientManager::search(const SearchParamOwner& p_search_param)
{
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
	uint64_t estimateSearchSpan = 0;
	if (p_search_param.m_clients.empty())
	{
		CFlyReadLock(*g_csClients);
		for (auto i = g_clients.cbegin(); i != g_clients.cend(); ++i)
			if (i->second->isConnected())
			{
				const uint64_t ret = i->second->search_internal(p_search_param);
				estimateSearchSpan = std::max(estimateSearchSpan, ret);
			}
	}
	else
	{
		CFlyReadLock(*g_csClients);
		for (auto it = p_search_param.m_clients.cbegin(); it != p_search_param.m_clients.cend(); ++it)
		{
			const string& client = *it;
			
			const auto& i = g_clients.find(client);
			if (i != g_clients.end() && i->second->isConnected())
			{
				const uint64_t ret = i->second->search_internal(p_search_param);
				estimateSearchSpan = std::max(estimateSearchSpan, ret);
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
	if (!isBeforeShutdown())
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
	if (!isBeforeShutdown())
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

void ClientManager::flushRatio(int p_max_count_flush)
{
	static bool g_isBusy = false;
	int l_count_flush = 0;
	if (g_isBusy == false)
	{
		CFlyBusyBool l_busy(g_isBusy);
#ifdef FLYLINKDC_BETA
		//CFlyLog l_log("[ClientManager::flushRatio]");
#endif
		std::vector<UserPtr> l_users;
		{
#ifdef _DEBUG
			//CFlyLog l_log_debug("[ClientManager::flushRatio - read all USERS - _DEBUG]");
#endif
			CFlyReadLock(*g_csUsers);
			//CFlyLock(g_csUsers);
			auto i = g_users.cbegin();
			while (i != g_users.cend())
			{
				if (i->second->isDirty())
				{
					l_users.push_back(i->second);
				}
				++i;
			}
#ifdef _DEBUG
			//l_log_debug.step("l_users.size() =" + Util::toString(l_users.size()));
#endif
		}
		for (auto i : l_users)
		{
			if (i->flushRatio() && !isBeforeShutdown() && !AutoUpdate::getExitOnUpdate())
			{
				l_count_flush++;
#ifdef FLYLINKDC_BETA
#ifdef _DEBUG
				//l_log.log("Flush for user: " + i->getLastNick() + " Hub = " + Util::toString(i->getHubID()));
				// +   " ip = " + i->getIPAsString() + " CountMessages = " + Util::toString(i->getMessageCount()));
#endif
#endif
			}
		}
#ifdef FLYLINKDC_BETA
		if (l_count_flush)
		{
			// l_log.log("Flush for " + Util::toString(l_count_flush) + " users...");
		}
		else
		{
			//l_log.m_skip_stop = true;
		}
#endif
	}
}
void ClientManager::usersCleanup()
{
	//CFlyLog l_log("[ClientManager::usersCleanup]");
	CFlyWriteLock(*g_csUsers);
	//CFlyLock(g_csUsers);
	auto i = g_users.begin();
	while (i != g_users.end() && !isBeforeShutdown())
	{
		if (i->second.unique())
		{
#ifdef _DEBUG
			//LogManager::message("g_users.erase(i++); - Nick = " + i->second->getLastNick());
#endif
			g_users.erase(i++);
		}
		else
		{
			++i;
		}
	}
}

void ClientManager::createMe(const string& p_cid, const string& p_nick)
{
	dcassert(!g_me);
	dcassert(g_pid.isZero());
	
	g_pid = CID(p_cid);
	
	TigerHash l_tiger;
	l_tiger.update(g_pid.data(), CID::SIZE);
	const CID l_myCID = CID(l_tiger.finalize());
	g_me = std::make_shared<User>(l_myCID, p_nick, 0);
	
	
#ifndef _DEBUG
	/*
	static bool g_is_first = false;
	    if (g_is_first == false)
	    {
	        g_is_first = true;
	        extern crash_rpt::CrashRpt g_crashRpt;
	        g_crashRpt.AddUserInfoToReport(L"PID", Text::toT(g_pid.toBase32()).c_str());
	        g_crashRpt.AddUserInfoToReport(L"CID", Text::toT(l_myCID.toBase32()).c_str());
	    }
	*/
#endif
	
	g_uflylinkdc = std::make_shared<User>(g_pid, p_nick, 0);
	
	g_iflylinkdc.setSID(AdcCommand::HUB_SID);
#ifdef IRAINMAN_USE_HIDDEN_USERS
	g_iflylinkdc.setHidden();
#endif
	g_iflylinkdc.setHub();
	g_iflylinkdc.setUser(g_uflylinkdc);
	
	{
		CFlyWriteLock(*g_csUsers);
		//CFlyLock(g_csUsers);
		g_users.insert(make_pair(g_me->getCID(), g_me));
	}
}
void ClientManager::generateNewMyCID()
{
	dcassert(g_me);
	g_me->generateNewMyCID();
}
const CID& ClientManager::getMyCID()
{
	dcassert(g_me);
	if (g_me)
		return g_me->getCID();
	else
	{
		static CID g_CID;
		return g_CID;
	}
}
const CID& ClientManager::getMyPID()
{
	dcassert(!g_pid.isZero());
	return g_pid;
}

const string ClientManager::findMyNick(const string& hubUrl)
{
#ifdef IRAINMAN_CORRRECT_CALL_FOR_CLIENT_MANAGER_DEBUG
	dcassert(!hubUrl.empty());
#endif
	CFlyReadLock(*g_csClients);
	const auto& i = g_clients.find(hubUrl);
	if (i != g_clients.end())
		return i->second->getMyNick();
	return Util::emptyString;
}

int ClientManager::getMode(const FavoriteHubEntry* p_hub, bool& p_isWantAutodetect)
{
	const bool l_is_tcp_port_firewall = ConnectionManager::g_is_test_tcp_port == true &&
	                                    CFlyServerJSON::isTestPortOK(SETTING(TCP_PORT), "tcp", true) == false;
	p_isWantAutodetect = l_is_tcp_port_firewall;
	int l_mode = 0;
	const auto l_type = SETTING(INCOMING_CONNECTIONS);
	if (!p_hub)
	{
		if (l_is_tcp_port_firewall || l_type == SettingsManager::INCOMING_FIREWALL_UPNP && MappingManager::getExternaIP().empty())
		{
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
				p_isWantAutodetect = false;
				break;
			default:
			{
				l_mode = l_type;
				if (l_is_tcp_port_firewall == true)
				{
					l_mode = SettingsManager::INCOMING_FIREWALL_PASSIVE;
					p_isWantAutodetect = false;
				}
			}
		}
	}
	return l_mode;
}
bool ClientManager::isActive(const FavoriteHubEntry* p_hub, bool& p_isWantAutodetect)
{
	const auto l_mode = getMode(p_hub, p_isWantAutodetect);
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
	dcassert(!isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		for (auto i = l.cbegin(); i != l.cend(); ++i)
		{
			updateNick(*i);
#ifdef _DEBUG
			//      LogManager::message("ClientManager::on(UsersUpdated nick = " + (*i)->getUser()->getLastNick());
#endif
		}
	}
}

void ClientManager::updateNick(const OnlineUserPtr& p_online_user)
{
	if (p_online_user->getUser()->getLastNick().empty())
	{
		const string& l_nick_from_identity = p_online_user->getIdentity().getNick();
		p_online_user->getUser()->setLastNick(l_nick_from_identity);
		dcassert(p_online_user->getUser()->getLastNick() != l_nick_from_identity); // TODO ������� ����� ��� ������?
	}
	else
	{
#ifdef _DEBUG
		//dcassert(0);
		//const string& l_nick_from_identity = p_online_user->getIdentity().getNick();
		//LogManager::message("[DUP] updateNick(const OnlineUserPtr& p_online_user) ! nick==nick == "
		//                    + l_nick_from_identity + " p_online_user->getUser()->getLastNick() = " + p_online_user->getUser()->getLastNick());
#endif
	}
}

void ClientManager::on(HubUpdated, const Client* c) noexcept
{
	dcassert(!isBeforeShutdown());
	fly_fire1(ClientManagerListener::ClientUpdated(), c);
}

void ClientManager::on(ClientFailed, const Client* client, const string&) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		fly_fire1(ClientManagerListener::ClientDisconnected(), client);
	}
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
OnlineUserPtr ClientManager::getOnlineUserL(const UserPtr& p)
{
	if (p == nullptr)
		return nullptr;
		
	const auto i = g_onlineUsers.find(p->getCID());
	if (i == g_onlineUsers.end())
		return OnlineUserPtr();
		
	return i->second;
}

void ClientManager::sendRawCommandL(const OnlineUser& ou, const int aRawCommand)
{
	const string rawCommand = ou.getClient().getRawCommand(aRawCommand);
	if (!rawCommand.empty())
	{
		StringMap ucParams;
		
		const UserCommand uc = UserCommand(0, 0, 0, 0, "", rawCommand, "", "");
		userCommandL(HintedUser(ou.getUser(), ou.getClient().getHubUrl()), uc, ucParams, true);
	}
}

void ClientManager::setListLength(const UserPtr& p, const string& listLen)
{
	CFlyWriteLock(*g_csOnlineUsers); // TODO Write
	const auto i = g_onlineUsers.find(p->getCID());
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
		const auto i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end())
		{
			OnlineUser* ou = i->second;
			auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou->getIdentity()' expression repeatedly. cheatmanager.h 43
			
			auto fileListDisconnects = id.incFileListDisconnects(); // 8 ��� �� ����?
			
			if (SETTING(ACCEPTED_DISCONNECTS) == 0)
				return;
				
			if (fileListDisconnects == SETTING(ACCEPTED_DISCONNECTS))
			{
				c = &ou->getClient();
				report = id.setCheat(ou->getClientBase(), "Disconnected file list " + Util::toString(fileListDisconnects) + " times", false);
				sendRawCommandL(*ou, SETTING(DISCONNECT_RAW));
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
		const auto i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end())
		{
			OnlineUserPtr ou = i->second;
			auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou.getIdentity()' expression repeatedly. cheatmanager.h 80
			
			auto connectionTimeouts = id.incConnectionTimeouts(); // 8 ��� �� ����?
			
			if (SETTING(ACCEPTED_TIMEOUTS) == 0)
				return;
				
			if (connectionTimeouts == SETTING(ACCEPTED_TIMEOUTS))
			{
				c = &ou->getClient();
#ifdef FLYLINKDC_USE_DETECT_CHEATING
				report = id.setCheat(ou.getClientBase(), "Connection timeout " + Util::toString(connectionTimeouts) + " times", false);
#else
				report = "Connection timeout " + Util::toString(connectionTimeouts) + " times";
#endif
				remove = true;
				sendRawCommandL(*ou, SETTING(TIMEOUT_RAW));
			}
		}
	}
	cheatMessage(c, report);
}

#ifdef FLYLINKDC_USE_DETECT_CHEATING
void ClientManager::checkCheating(const UserPtr& p, DirectoryListing* dl)
{
	Client* client;
	string report;
	OnlineUserPtr ou;
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end())
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
			sendRawCommandL(*ou, SETTING(FAKESHARE_RAW));
		}
		else
		{
			id.setFakeCardBit(Identity::CHECKED, true);
		}
		id.updateClientType(*ou);
		
		client = &(ou->getClient());
	}
	//client->updatedMyINFO(ou); // ��� ���� �� ����� ����������� ���� �����������
	cheatMessage(client, report);
}
#endif // FLYLINKDC_USE_DETECT_CHEATING
#ifdef IRAINMAN_INCLUDE_USER_CHECK
void ClientManager::setClientStatus(const UserPtr& p, const string& aCheatString, const int aRawCommand, bool aBadClient)
{
	Client* client;
	OnlineUserPtr ou;
	string report;
	{
		CFlyReadLock(*g_csOnlineUsers);
		const auto i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end())
			return;
			
		ou = i->second;
		ou->getIdentity().updateClientType(*ou);
		if (!aCheatString.empty())
		{
			report += ou->getIdentity().setCheat(ou->getClientBase(), aCheatString, aBadClient);
		}
		if (aRawCommand != -1)
		{
			sendRawCommandL(*ou, aRawCommand);
		}
		
		client = &(ou->getClient());
	}
	//client->updatedMyINFO(ou); // �� ���� ������� �����������!
	cheatMessage(client, report);
}
#endif // IRAINMAN_INCLUDE_USER_CHECK
void ClientManager::setSupports(const UserPtr& p, const StringList & aSupports, const uint8_t knownUcSupports)
{
	CFlyWriteLock(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		auto& id = i->second->getIdentity();
		id.setKnownUcSupports(knownUcSupports);
		{
			AdcSupports::setSupports(id, aSupports);
		}
	}
}
void ClientManager::setUnknownCommand(const UserPtr& p, const string& aUnknownCommand)
{
	CFlyWriteLock(*g_csOnlineUsers);
	const auto i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		i->second->getIdentity().setStringParam("UC", aUnknownCommand);
	}
}

void ClientManager::reportUser(const HintedUser& user)
{
	const bool priv = FavoriteManager::isPrivate(user.hint);
	string l_report;
	Client* l_client = nullptr;
	if (user.user)
	{
		CFlyReadLock(*g_csOnlineUsers);
		OnlineUserPtr ou = findOnlineUserL(user.user->getCID(), user.hint, priv);
		if (!ou)
			return;
			
		ou->getIdentity().getReport(l_report);
		l_client = &(ou->getClient());
		
	}
	if (l_client)
	{
		l_client->reportUser(l_report);
	}
}

#ifndef IRAINMAN_IDENTITY_IS_NON_COPYABLE
Identity ClientManager::getIdentity(const UserPtr& user)
{
	CFlyReadLock(*g_csOnlineUsers);
	const OnlineUser* ou = getOnlineUserL(user);
	if (ou)
		return  ou->getIdentity(); // https://www.box.net/shared/1w3v80olr2oro7s1gqt4
	else
		return Identity();
}
#endif

StringList ClientManager::getUsersByIp(const string &p_ip) // TODO - boost
{
	StringList l_result;
	l_result.reserve(1);
	std::unordered_set<string> l_fix_dup;
	CFlyReadLock(*g_csOnlineUsers);
	for (auto i = g_onlineUsers.cbegin(); i != g_onlineUsers.cend(); ++i)
	{
		if (i->second->getUser() && i->second->getUser()->getLastIPfromRAM().to_string() == p_ip) // TODO - boost
		{
			const auto l_nick = i->second->getUser()->getLastNick();
			const auto l_res = l_fix_dup.insert(l_nick);
			if (l_res.second == true)
			{
				l_result.push_back(l_nick);
			}
		}
	}
	return l_result;
}

/**
 * @file
 * $Id: ClientManager.cpp 571 2011-07-27 19:18:04Z bigmuscle $
 */
