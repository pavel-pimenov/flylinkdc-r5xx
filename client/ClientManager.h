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

#ifndef DCPLUSPLUS_DCPP_CLIENT_MANAGER_H
#define DCPLUSPLUS_DCPP_CLIENT_MANAGER_H

#include "Client.h"
#include "AdcSupports.h"
#include "DirectoryListing.h"
#include "FavoriteManager.h"

class UserCommand;

class ClientManager : public Speaker<ClientManagerListener>,
	private ClientListener, public Singleton<ClientManager>,
	private TimerManagerListener
{
	public:
		Client* getClient(const string& aHubURL);
		void putClient(Client* aClient);
		StringList getHubs(const CID& cid, const string& hintUrl) const;
		StringList getHubNames(const CID& cid, const string& hintUrl) const;
		StringList getNicks(const CID& cid, const string& hintUrl) const;
#ifndef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
		struct HubInfo
		{
			string m_hub_url;
			string m_hub_name;
			bool m_is_op;
		};
		typedef std::vector<HubInfo> HubInfoArray;
		void getConnectedHubInfo(HubInfoArray& p_hub_info) const
		{
			SharedLock l(g_csClients);
			for (auto i = m_clients.cbegin(); i != m_clients.cend(); ++i)
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
		void getConnectedHubUrls(StringList& p_hub_url) const
		{
			SharedLock l(g_csClients);
			for (auto i = m_clients.cbegin(); i != m_clients.cend(); ++i)
			{
				if (i->second->isConnected())
					p_hub_url.push_back(i->second->getHubUrl());
			}
		}
#endif // IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
		size_t getTotalUsers() const // [+] IRainman.
		{
			size_t users = 0;
			SharedLock l(g_csClients);
			for (auto i = m_clients.cbegin(); i != m_clients.cend(); ++i)
			{
				users += i->second->getUserCount();
			}
			return users;
		}
		
		StringList getHubs(const CID& cid, const string& hintUrl, bool priv) const;
		StringList getHubNames(const CID& cid, const string& hintUrl, bool priv) const;
		StringList getNicks(const CID& cid, const string& hintUrl, bool priv) const;
		string getStringField(const CID& cid, const string& hintUrl, const char* field) const; // [!] IRainman fix.
		StringList getNicks(const HintedUser& user) const
		{
			// [!] IRainman fix: Calling this function with an empty by the user is not correct!
			dcassert(user.user);
			return getNicks(user.user->getCID(), user.hint);
		}
		StringList getHubNames(const HintedUser& user) const
		{
			// [!] IRainman fix: Calling this function with an empty by the user is not correct!
			dcassert(user.user);
			return getHubNames(user.user->getCID(), user.hint);
		}
		
		// [-] string getConnection(const CID& cid) const; [-] IRainman: deprecated.
		uint8_t getSlots(const CID& cid) const;
		
		bool isConnected(const string& aUrl) const
		{
			SharedLock l(g_csClients);
			return m_clients.find(aUrl) != m_clients.end();
		}
		Client* findClient(const string& p_Url) const;
//[+] FlylinkDC
		bool isOnline(const UserPtr& aUser) const
		{
			SharedLock l(g_csOnlineUsers);
			return g_onlineUsers.find(aUser->getCID()) != g_onlineUsers.end();
		}
//[~] FlylinkDC
		void search(Search::SizeModes aSizeMode, int64_t aSize, int aFileType, const string& aString, const string& aToken, void* aOwner = nullptr);
		uint64_t search(const StringList& who, Search::SizeModes aSizeMode, int64_t aSize, int aFileType, const string& aString, const string& aToken, const StringList& aExtList, void* aOwner = nullptr);
		
		void cancelSearch(void* aOwner);
		
		void infoUpdated();
		
		UserPtr getUser(const string& p_Nick, const string& p_HubURL
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		                , uint32_t p_HubID
#endif
		               ) noexcept;
		UserPtr getUser(const CID& cid, bool p_create = true) noexcept;
		
		string findHub(const string& ipPort) const;
		string findHubEncoding(const string& aUrl) const;
		
		/**
		* @param priv discard any user that doesn't match the hint.
		* @return OnlineUser* found by CID and hint; might be only by CID if priv is false.
		*/
		OnlineUser* findOnlineUserL(const HintedUser& user, bool priv) const
		{
			// [!] IRainman: This function need to external lock.
			return findOnlineUserL(user.user->getCID(), user.hint, priv);
		}
		OnlineUser* findOnlineUserL(const CID& cid, const string& hintUrl, bool priv) const
		{
			// [!] IRainman: This function need to external lock.
			OnlinePairC p;
			OnlineUser* u = findOnlineUserHintL(cid, hintUrl, p);
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
		
		UserPtr findUser(const string& aNick, const string& aHubUrl) const
		{
			return findUser(makeCid(aNick, aHubUrl));
		}
		UserPtr findUser(const CID& cid) const noexcept;
		UserPtr findLegacyUser(const string& aNick
#ifndef IRAINMAN_USE_NICKS_IN_CM
		                       , const string& aHubUrl
#endif
		                      ) const noexcept;
		                      
		void updateNick(const UserPtr& user, const string& nick) noexcept;
#ifdef IRAINMAN_USE_NICKS_IN_CM
	private:
		void updateNick_internal(const UserPtr& user, const string& nick) noexcept; // [+] IRainman fix.
	public:
#endif
		const string& getMyNick(const string& hubUrl) const; // [!] IRainman opt.
		
		// [+] brain-ripper
		bool getUserParams(const UserPtr& user, uint64_t& p_bytesShared, int& p_slots, int& p_limit, std::string& p_ip) const
		{
			SharedLock l(g_csOnlineUsers);
			const OnlineUser* u = getOnlineUserL(user);
			if (u)
			{
				// [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'u->getIdentity()' expression repeatedly. clientmanager.h 160
				const auto& i = u->getIdentity();
				p_bytesShared = i.getBytesShared();
				p_slots = i.getSlots();
				p_limit = i.getLimit();
				p_ip = i.getIp();
				
				return true;
			}
			return false;
		}
		// [+] IRainman fix.
		struct UserParams
		{
			uint64_t bytesShared;
			int slots;
			int limit;
			std::string ip;
		};
		
		bool getUserParams(const UserPtr& user, UserParams& p_params) const
		{
			return getUserParams(user, p_params.bytesShared, p_params.slots, p_params.limit, p_params.ip);
		}
		// [~] IRainman fix.
		
		// !PPA!
#define CREATE_LOCK_INSTANCE_CM(scope, CS)\
	class LockInstance##CS \
	{\
		public:\
			LockInstance##CS() \
			{\
				ClientManager::##scope##_cs##CS.lockShared();\
			}\
			~LockInstance##CS()\
			{\
				ClientManager::##scope##_cs##CS.unlockShared();\
			}\
			ClientManager* operator->()\
			{\
				return ClientManager::getInstance();\
			}\
	}
		// [!] IRainman opt.
		CREATE_LOCK_INSTANCE_CM(g, Clients);
		CREATE_LOCK_INSTANCE_CM(g, OnlineUsers);
		CREATE_LOCK_INSTANCE_CM(g, Users);
		// [~] IRainman opt.
#undef CREATE_LOCK_INSTANCE_CM
		
		void setIPUser(const UserPtr& p_user, const string& p_ip, const uint16_t p_udpPort = 0)
		{
			// [!] TODO FlylinkDC++ Team - Зачем этот метод?
			// Нужен! r8622
			// L: Данный метод предназначен для обновления IP всем юзерам с этим CID.
			if (p_ip.empty())
				return;
				
			SharedLock l(g_csOnlineUsers);
			const OnlinePairC p = g_onlineUsers.equal_range(p_user->getCID());
			for (auto i = p.first; i != p.second; ++i)
			{
				i->second->getIdentity().setIp(p_ip);
				if (p_udpPort != 0)
					i->second->getIdentity().setUdpPort(p_udpPort);
			}
		}
		
		UserPtr getUserByIp(const string &ip) const
		{
			{
				SharedLock l(g_csOnlineUsers);
				for (auto i = g_onlineUsers.cbegin(); i != g_onlineUsers.cend(); ++i)
					if (i->second->getIdentity().getIp() == ip)
						return i->second->getUser();
			}
			return nullptr;
		}
#ifndef IRAINMAN_IDENTITY_IS_NON_COPYABLE
		Identity getIdentity(const UserPtr& user) const
		{
			SharedLock l(g_csOnlineUsers);
			const OnlineUser* ou = getOnlineUserL(user);
			if (ou)
				return  ou->getIdentity(); // https://www.box.net/shared/1w3v80olr2oro7s1gqt4
			else
				return Identity();
		}
#endif // IRAINMAN_IDENTITY_IS_NON_COPYABLE
		OnlineUser* getOnlineUserL(const UserPtr& p) const
		{
			// Lock l(cs);  - getOnlineUser() return value should be guarded outside
			if (p == nullptr)
				return nullptr;
				
			OnlineIterC i = g_onlineUsers.find(p->getCID());
			if (i == g_onlineUsers.end())
				return nullptr;
				
			return i->second;
		}
		//[~]FlylinkDC
		/* [-] IRainman fix: deprecated.
		int64_t getBytesShared(const UserPtr& p) const //[+]PPA
		{
		    int64_t l_share = 0;
		    {
		        SharedLock l(g_csOnlineUsers);
		        OnlineIterC i = g_onlineUsers.find(p->getCID());
		        if (i != g_onlineUsers.end())
		            l_share = i->second->getIdentity().getBytesShared();
		    }
		    return l_share;
		}
		  [-] IRainman fix */
		bool isOp(const UserPtr& aUser, const string& aHubUrl) const;
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
		bool isStealth(const string& aHubUrl) const;
#endif
		
		/** Constructs a synthetic, hopefully unique CID */
		static CID makeCid(const string& nick, const string& hubUrl);
		
		void putOnline(const OnlineUserPtr& ou) noexcept; // [!] IRainman fix.
		void putOffline(const OnlineUserPtr& ou, bool disconnect = false) noexcept; // [!] IRainman fix.
		
		// [!] IRainman fix: getMe() is deprecated in FlylinkDC++. For identification user of as the this client please use isMe()
		// [-] const UserPtr& getMe();
		static bool isMe(const CID& cid)
		{
			return cid == getMyCID();
		}
		static bool isMe(const UserPtr& user)
		{
			dcassert(user); // If there crashes: means forgotten somewhere above cause getFlylinkDCUser() or getFlylinkDCIdentity() ;)
			return isMe(user->getCID());
		}
		static bool isMe(const OnlineUserPtr& user)
		{
			dcassert(user); // If there crashes: means forgotten somewhere above cause getFlylinkDCUser() or getFlylinkDCIdentity() ;)
			return isMe(user->getUser());
		}
		static const UserPtr& getMe_UseOnlyForNonHubSpecifiedTasks() // [!] IRainman fix.
		{
			dcassert(g_me);
			return g_me;
		}
		static const UserPtr& getFlylinkDCUser()
		{
			dcassert(g_uflylinkdc);
			return g_uflylinkdc;
		}
		static const Identity& getFlylinkDCIdentity()
		{
			dcassert(g_uflylinkdc);
			return g_iflylinkdc;
		}
	private:
		void createMe(const string& cid, const string& nick);
	public:
		// [~] IRainman fix.
		
		void send(AdcCommand& c, const CID& to);
		
		void connect(const HintedUser& user, const string& token);
		void privateMessage(const HintedUser& user, const string& msg, bool thirdPerson);
		void userCommand(const HintedUser& user, const UserCommand& uc, StringMap& params, bool compatibility);
		
		int getMode(const string& aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
		            , bool *pbWantAutodetect = NULL
#endif
		           ) const;
		bool isActive(const string& aHubUrl = Util::emptyString
#ifdef RIP_USE_CONNECTION_AUTODETECT
		                                      , bool *pbWantAutodetect = NULL
#endif
		             ) const
		{
			return getMode(aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
			               , pbWantAutodetect
#endif
			              ) != SettingsManager::INCOMING_FIREWALL_PASSIVE;
		}
		/* [-] IRainman
		void lock() noexcept
		{
		    cs.lock();
		}
		void unlock() noexcept
		{
		    cs.unlock();
		}
		*/
#ifdef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
		const Client::List& getClientsL() const
		{
			return m_clients;
		}
#endif
		// [!] IRainman fix.
		static const CID& getMyCID() // [!] IRainman fix: add const link.
		{
			dcassert(g_me);
			return g_me->getCID();
		}
		static const CID& getMyPID()
		{
			dcassert(!g_pid.isZero());
			return g_pid;
		}
		// [~] IRainman fix.
		// fake detection methods
#include "CheatManager.h"
		
#ifdef STRONG_USE_DHT
		OnlineUserPtr findDHTNode(const CID& cid) const;
#endif
		void shutdown()
		{
			dcassert(!isShutdown());
			g_isShutdown = true;
			TimerManager::getInstance()->removeListener(this);
		}
		void clear()
		{
			{
				UniqueLock l(g_csOnlineUsers);
				g_onlineUsers.clear();
			}
			{
				UniqueLock l(g_csUsers);
#ifdef IRAINMAN_USE_NICKS_IN_CM
				g_nicks.clear();
#endif
				g_users.clear();
			}
		}
		static bool isShutdown()
		{
			return g_isShutdown;
		}
		
	private:
	
		//mutable CriticalSection cs; [-] IRainman opt.
		// =================================================
		Client::List m_clients;
		static SharedCriticalSection g_csClients; // [+] IRainman opt.
		// =================================================
#ifdef IRAINMAN_NON_COPYABLE_USER_DATA_IN_CLIENT_MANAGER
# ifdef IRAINMAN_USE_NICKS_IN_CM
		typedef unordered_map<const CID*, std::string&> NickMap;
# endif
		typedef unordered_map<const CID*, const UserPtr> UserMap;
#else
# ifdef IRAINMAN_USE_NICKS_IN_CM
		typedef unordered_map<CID, std::string> NickMap;
# endif
		typedef unordered_map<CID, const UserPtr> UserMap;
#endif
		static UserMap g_users;
#ifdef IRAINMAN_USE_NICKS_IN_CM
		static NickMap g_nicks;
#endif
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_CLIENT_MANAGER
		static SharedCriticalSection g_csUsers; // [+] IRainman opt.
#else
		static SharedCriticalSection& g_csUsers; // [+] IRainman opt.
#endif
		// =================================================
#ifdef IRAINMAN_NON_COPYABLE_USER_DATA_IN_CLIENT_MANAGER
		typedef std::unordered_multimap<const CID*, OnlineUser*> OnlineMap; // TODO: not allow to replace UserPtr in Identity.
#else
		typedef std::unordered_multimap<CID, OnlineUser*> OnlineMap;
#endif
		typedef OnlineMap::iterator OnlineIter;
		typedef OnlineMap::const_iterator OnlineIterC;
		typedef pair<OnlineIter, OnlineIter> OnlinePair;
		typedef pair<OnlineIterC, OnlineIterC> OnlinePairC;
		
		static OnlineMap g_onlineUsers;
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_CLIENT_MANAGER
		static SharedCriticalSection g_csOnlineUsers; // [+] IRainman opt.
#else
		static SharedCriticalSection& g_csOnlineUsers; // [+] IRainman opt.
#endif
		// =================================================
		static UserPtr g_me; // [!] IRainman fix: this is static object.
		static UserPtr g_uflylinkdc; // [+] IRainman fix.
		static Identity g_iflylinkdc; // [+] IRainman fix.
		static CID g_pid; // [!] IRainman fix this is static object.
		
		friend class Singleton<ClientManager>;
		
		ClientManager()
		{
			TimerManager::getInstance()->addListener(this);
			//quickTick = GET_TICK();  [-] IRainman.
			createMe(SETTING(PRIVATE_ID), SETTING(NICK)); // [+] IRainman fix.
		}
		
		~ClientManager()
		{
			dcassert(isShutdown());
		}
		
		void updateNick(const OnlineUserPtr& user) noexcept;
		
		/// @return OnlineUser* found by CID and hint; discard any user that doesn't match the hint.
		OnlineUser* findOnlineUserHintL(const CID& cid, const string& hintUrl) const
		{
			// [!] IRainman: This function need to external lock.
			OnlinePairC p;
			return findOnlineUserHintL(cid, hintUrl, p);
		}
		/**
		* @param p OnlinePair of all the users found by CID, even those who don't match the hint.
		* @return OnlineUser* found by CID and hint; discard any user that doesn't match the hint.
		*/
		OnlineUser* findOnlineUserHintL(const CID& cid, const string& hintUrl, OnlinePairC& p) const;
		
		// ClientListener
		void on(Connected, const Client* c) noexcept;
		void on(UserUpdated, const Client*, const OnlineUserPtr& user) noexcept;
		void on(UsersUpdated, const Client* c, const OnlineUserList&) noexcept;
		void on(Failed, const Client*, const string&) noexcept;
		void on(HubUpdated, const Client* c) noexcept;
		void on(HubUserCommand, const Client*, int, int, const string&, const string&) noexcept;
		void on(NmdcSearch, Client* aClient, const string& aSeeker, Search::SizeModes aSizeMode, int64_t aSize,
		        int aFileType, const string& aString, bool isPassive) noexcept;
		void on(AdcSearch, const Client* c, const AdcCommand& adc, const CID& from) noexcept;
		// TimerManagerListener
		void on(TimerManagerListener::Minute, uint64_t aTick) noexcept;
		
		/** Indication that the application is being closed */
		static bool g_isShutdown;
};

#endif // !defined(CLIENT_MANAGER_H)

/**
 * @file
 * $Id: ClientManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
