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
		void putClient(Client* p_client);
		void prepareClose(); // [+]PPA
		static StringList getHubs(const CID& cid, const string& hintUrl);
		static StringList getHubNames(const CID& cid, const string& hintUrl);
		static StringList getNicks(const CID& cid, const string& hintUrl);
#ifndef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
		struct HubInfo
		{
			string m_hub_url;
			string m_hub_name;
			bool m_is_op;
		};
		typedef std::vector<HubInfo> HubInfoArray;
		static void getConnectedHubInfo(HubInfoArray& p_hub_info);
		static void getConnectedHubUrls(StringList& p_hub_url);
#endif // IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		static void resetAntivirusInfo();
#endif
		static size_t getTotalUsers(); // [+] IRainman.
		static std::map<string, CFlyClientStatistic> getClientStat();
		static StringList getHubs(const CID& cid, const string& hintUrl, bool priv);
		static StringList getHubNames(const CID& cid, const string& hintUrl, bool priv);
		static StringList getAntivirusNicks(const CID& cid);
		static StringList getNicks(const CID& cid, const string& hintUrl, bool priv);
		static string getStringField(const CID& cid, const string& hintUrl, const char* field); // [!] IRainman fix.
		static StringList getNicks(const HintedUser& user)
		{
			// [!] IRainman fix: Calling this function with an empty by the user is not correct!
			dcassert(user.user);
			return getNicks(user.user->getCID(), user.hint);
		}
		static StringList getHubNames(const HintedUser& user)
		{
			// [!] IRainman fix: Calling this function with an empty by the user is not correct!
			dcassert(user.user);
			return getHubNames(user.user->getCID(), user.hint);
		}
		
		// [-] string getConnection(const CID& cid) const; [-] IRainman: deprecated.
		uint8_t getSlots(const CID& cid) const;
		
		static bool isConnected(const string& aUrl)
		{
			webrtc::ReadLockScoped l(*g_csClients);
			return g_clients.find(aUrl) != g_clients.end();
		}
		Client* findClient(const string& p_Url) const;
//[+] FlylinkDC
		static bool isOnline(const UserPtr& aUser)
		{
			webrtc::ReadLockScoped l(*g_csOnlineUsers);
			return g_onlineUsers.find(aUser->getCID()) != g_onlineUsers.end();
		}
//[~] FlylinkDC
		void search(Search::SizeModes aSizeMode, int64_t aSize, Search::TypeModes aFileType, const string& aString, const string& aToken, void* aOwner, bool p_is_force_passive);
		uint64_t search(const StringList& who,
		                Search::SizeModes aSizeMode,
		                int64_t aSize,
		                Search::TypeModes aFileType,
		                const string& aString,
		                const string& aToken,
		                const StringList& aExtList,
		                void* aOwner,
		                bool p_is_force_passive);
		                
		static void cancelSearch(void* aOwner);
		
		static void infoUpdated();
		static void infoUpdated(Client* p_client);
		
		static UserPtr getUser(const string& p_Nick, const string& p_HubURL
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		                       , uint32_t p_HubID
#endif
		                       , bool p_first_load
		                      );
		static UserPtr getUser(const CID& cid, bool p_create);
		
		static string findHub(const string& ipPort);
		static string findHubEncoding(const string& aUrl);
		
		/**
		* @param priv discard any user that doesn't match the hint.
		* @return OnlineUser* found by CID and hint; might be only by CID if priv is false.
		*/
		static OnlineUser* findOnlineUserL(const HintedUser& user, bool priv)
		{
			// [!] IRainman: This function need to external lock.
			return findOnlineUserL(user.user->getCID(), user.hint, priv);
		}
		static OnlineUser* findOnlineUserL(const CID& cid, const string& hintUrl, bool priv)
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
		
		static UserPtr findUser(const string& aNick, const string& aHubUrl)
		{
			return findUser(makeCid(aNick, aHubUrl));
		}
		static UserPtr findUser(const CID& cid);
		static UserPtr findLegacyUser(const string& aNick
#ifndef IRAINMAN_USE_NICKS_IN_CM
		                              , const string& aHubUrl
#endif
		                             );
		                             
#ifdef IRAINMAN_USE_NICKS_IN_CM
		void updateNick(const UserPtr& user, const string& nick) noexcept;
	private:
		void updateNick_internal(const UserPtr& user, const string& nick) noexcept; // [+] IRainman fix.
	public:
#endif
		const string& getMyNick(const string& hubUrl) const; // [!] IRainman opt.
		
		// [+] brain-ripper
		static bool getUserParams(const UserPtr& user, uint64_t& p_bytesShared, int& p_slots, int& p_limit, std::string& p_ip);
		// [+] IRainman fix.
		struct UserParams
		{
			uint64_t bytesShared;
			int slots;
			int limit;
			std::string ip;
		};
		
		static bool getUserParams(const UserPtr& user, UserParams& p_params)
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
				ClientManager::##scope##_cs##CS->AcquireLockShared();\
			}\
			~LockInstance##CS()\
			{\
				ClientManager::##scope##_cs##CS->ReleaseLockShared();\
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
		
		static void setIPUser(const UserPtr& p_user, const string& p_ip, const uint16_t p_udpPort = 0);
		
		static StringList getUserByIp(const string &p_ip) // TODO - boost
		{
			StringList l_result;
			l_result.reserve(1);
			webrtc::ReadLockScoped l(*g_csOnlineUsers);
			for (auto i = g_onlineUsers.cbegin(); i != g_onlineUsers.cend(); ++i)
			{
				if (i->second->getIdentity().getIpAsString() == p_ip) // TODO - boost
				{
					l_result.push_back(i->second->getUser()->getLastNick());
				}
			}
			return l_result;
		}
#ifndef IRAINMAN_IDENTITY_IS_NON_COPYABLE
		static Identity getIdentity(const UserPtr& user)
		{
			webrtc::ReadLockScoped l(*g_csOnlineUsers);
			const OnlineUser* ou = getOnlineUserL(user);
			if (ou)
				return  ou->getIdentity(); // https://www.box.net/shared/1w3v80olr2oro7s1gqt4
			else
				return Identity();
		}
#endif // IRAINMAN_IDENTITY_IS_NON_COPYABLE
		static OnlineUser* getOnlineUserL(const UserPtr& p)
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
		static int64_t getBytesShared(const UserPtr& p) //[+]PPA
		{
		    int64_t l_share = 0;
		    {
		        webrtc::ReadLockScoped l(*g_csOnlineUsers);
		        OnlineIterC i = g_onlineUsers.find(p->getCID());
		        if (i != g_onlineUsers.end())
		            l_share = i->second->getIdentity().getBytesShared();
		    }
		    return l_share;
		}
		  [-] IRainman fix */
		static bool isOp(const UserPtr& aUser, const string& aHubUrl);
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
		static bool isStealth(const string& aHubUrl);
#endif
		
		/** Constructs a synthetic, hopefully unique CID */
		static CID makeCid(const string& nick, const string& hubUrl);
		
		void putOnline(const OnlineUserPtr& ou) noexcept; // [!] IRainman fix.
		void putOffline(const OnlineUserPtr& ou, bool disconnect = false) noexcept; // [!] IRainman fix.
		
		static bool isMe(const CID& p_cid)
		{
			return p_cid == getMyCID();
		}
		static bool isMe(const UserPtr& p_user)
		{
			dcassert(p_user); // If there crashes: means forgotten somewhere above cause getFlylinkDCUser() or getFlylinkDCIdentity() ;)
			return isMe(p_user->getCID());
		}
		static bool isMe(const OnlineUserPtr& p_user)
		{
			dcassert(p_user); // If there crashes: means forgotten somewhere above cause getFlylinkDCUser() or getFlylinkDCIdentity() ;)
			return isMe(p_user->getUser());
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
		static void cheatMessage(Client* p_client, const string& p_report);
	public:
		// [~] IRainman fix.
		
		void send(AdcCommand& c, const CID& to);
		
		void connect(const HintedUser& user, const string& p_token);
		void privateMessage(const HintedUser& user, const string& msg, bool thirdPerson);
		void userCommand(const HintedUser& user, const UserCommand& uc, StringMap& params, bool compatibility);
		
		static int getMode(const FavoriteHubEntry* p_hub
#ifdef RIP_USE_CONNECTION_AUTODETECT
		                   , bool *pbWantAutodetect = NULL
#endif
		                  );
		static bool isActive(const FavoriteHubEntry* p_hub
#ifdef RIP_USE_CONNECTION_AUTODETECT
		                     , bool *pbWantAutodetect = NULL
#endif
		                    )
		{
			return getMode(p_hub
#ifdef RIP_USE_CONNECTION_AUTODETECT
			               , pbWantAutodetect
#endif
			              ) != SettingsManager::INCOMING_FIREWALL_PASSIVE;
		}
#ifdef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
		const Client::List& getClientsL() const
		{
			return g_clients;
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
		///////////////////////
		/**
		 * This file is a part of client manager.
		 * It has been divided but shouldn't be used anywhere else.
		 */
		
		void sendRawCommand(const OnlineUser& ou, const int aRawCommand);
		void setListLength(const UserPtr& p, const string& listLen);
		void fileListDisconnected(const UserPtr& p);
		void connectionTimeout(const UserPtr& p);
		void checkCheating(const UserPtr& p, DirectoryListing* dl);
		void setClientStatus(const UserPtr& p, const string& aCheatString, const int aRawCommand, bool aBadClient);
		
// [!] IRainamn fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
		void setSupports(const UserPtr& p, StringList& aSupports, const uint8_t knownUcSupports);
		void setUnknownCommand(const UserPtr& p, const string& aUnknownCommand);
		void reportUser(const HintedUser& user);
		void setFakeList(const UserPtr& p, const string& aCheatString);
		///////////////////////
		
#ifdef STRONG_USE_DHT
		static OnlineUserPtr findDHTNode(const CID& cid);
#endif
		void shutdown()
		{
			dcassert(!isShutdown());
			g_isShutdown = true;
			TimerManager::getInstance()->removeListener(this);
		}
		static void clear()
		{
			{
				webrtc::WriteLockScoped l(*g_csOnlineUsers);
				g_onlineUsers.clear();
			}
			{
				webrtc::WriteLockScoped l(*g_csUsers);
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
		static Client::List g_clients;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csClients; // [+] IRainman opt.
		// =================================================
#ifdef IRAINMAN_NON_COPYABLE_USER_DATA_IN_CLIENT_MANAGER
# ifdef IRAINMAN_USE_NICKS_IN_CM
		typedef std::unordered_map<const CID*, std::string&> NickMap;
# endif
		typedef std::unordered_map<const CID*, const UserPtr> UserMap;
#else
# ifdef IRAINMAN_USE_NICKS_IN_CM
		typedef std::unordered_map<CID, std::string> NickMap; // TODO boost
# endif
		typedef std::unordered_map<CID, UserPtr> UserMap; // TODO boost
#endif
		static UserMap g_users;
#ifdef IRAINMAN_USE_NICKS_IN_CM
		static NickMap g_nicks;
#endif
		static std::unique_ptr<webrtc::RWLockWrapper> g_csUsers; // [+] IRainman opt.
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
		static std::unique_ptr<webrtc::RWLockWrapper> g_csOnlineUsers; // [+] IRainman opt.
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
		
		static void updateNick(const OnlineUserPtr& p_online_user);
		
		/// @return OnlineUser* found by CID and hint; discard any user that doesn't match the hint.
		static OnlineUser* findOnlineUserHintL(const CID& cid, const string& hintUrl)
		{
			// [!] IRainman: This function need to external lock.
			OnlinePairC p;
			return findOnlineUserHintL(cid, hintUrl, p);
		}
		/**
		* @param p OnlinePair of all the users found by CID, even those who don't match the hint.
		* @return OnlineUser* found by CID and hint; discard any user that doesn't match the hint.
		*/
		static OnlineUser* findOnlineUserHintL(const CID& cid, const string& hintUrl, OnlinePairC& p);
		
		// ClientListener
		void on(Connected, const Client* c) noexcept;
		void on(UserUpdated, const Client*, const OnlineUserPtr& user) noexcept;
		void on(UsersUpdated, const Client* c, const OnlineUserList&) noexcept;
		void on(Failed, const Client*, const string&) noexcept;
		void on(HubUpdated, const Client* c) noexcept;
		void on(HubUserCommand, const Client*, int, int, const string&, const string&) noexcept;
		void on(NmdcSearch, Client* aClient, const string& aSeeker, Search::SizeModes aSizeMode, int64_t aSize,
		        Search::TypeModes aFileType, const string& aString, bool isPassive) noexcept;
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
