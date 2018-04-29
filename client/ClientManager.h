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

#pragma once


#ifndef DCPLUSPLUS_DCPP_CLIENT_MANAGER_H
#define DCPLUSPLUS_DCPP_CLIENT_MANAGER_H

#include "Client.h"
#include "AdcSupports.h"
#include "DirectoryListing.h"
#include "FavoriteManager.h"

class UserCommand;

class ClientManager : public Speaker<ClientManagerListener>,
	private ClientListener, public Singleton<ClientManager>
//,private TimerManagerListener
{
		friend class SpyFrame;
	public:
		Client* getClient(const string& aHubURL, bool p_is_auto_connect);
		void putClient(Client* p_client);
		void prepareClose();
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
		static unsigned getTotalUsers(); // [+] IRainman.
		static std::map<string, CFlyClientStatistic> getClientStat();
		static StringList getHubs(const CID& cid, const string& hintUrl, bool priv);
		static StringList getHubNames(const CID& cid, const string& hintUrl, bool priv);
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		static StringList getAntivirusNicks(const CID& cid);
#endif
		static StringList getNicks(const CID& cid, const string& hintUrl, bool priv);
		static string getStringField(const CID& cid, const string& hintUrl, const char* field); // [!] IRainman fix.
		static StringList getNicks(const HintedUser& user);
		static StringList getHubNames(const HintedUser& user);
		static bool isConnected(const string& aUrl);
		static Client* findClient(const string& p_Url);
		static bool isOnline(const UserPtr& aUser);
		static uint8_t getSlots(const CID& cid);
		static void search(const SearchParamOwner& p_search_param);
		static uint64_t multi_search(const SearchParamTokenMultiClient& p_search_param);
		static void cancelSearch(void* aOwner);
		static void infoUpdated(bool p_is_force = false);
		static void infoUpdated(Client* p_client);
		
		static UserPtr getUser(const string& p_Nick, const string& p_HubURL
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		                       , uint32_t p_HubID
#endif
		                      );
		static UserPtr createUser(const CID& cid, const string& p_nick, uint32_t p_hub_id);
		
		static string findHub(const string& ipPort);
		static string findHubEncoding(const string& aUrl);
		
		/**
		* @param priv discard any user that doesn't match the hint.
		* @return OnlineUser* found by CID and hint; might be only by CID if priv is false.
		*/
		static OnlineUserPtr findOnlineUserL(const HintedUser& user, bool priv);
		static OnlineUserPtr findOnlineUserL(const CID& cid, const string& hintUrl, bool priv);
		static UserPtr findUser(const string& aNick, const string& aHubUrl);
		static UserPtr findUser(const CID& cid);
		static UserPtr findLegacyUser(const string& aNick, const string& aHubUrl);
		
		static const string findMyNick(const string& hubUrl);
		
		// [+] brain-ripper
		// [+] IRainman fix.
		struct UserParams
		{
			int64_t m_bytesShared;
			int m_slots;
			int m_limit;
			std::string m_ip;
			std::string m_tag;
			std::string m_nick;
			
			
			tstring getTagIP()
			{
				if (!m_ip.empty())
				{
					string dns;
#ifdef FLYLINKDC_USE_DNS
					dns = Socket::nslookup(ip);
					if (m_ip == dns)
						dns = "no DNS"; // TODO translate
					if (!dns.empty())
						dns = " / " + dns;
#endif
					return Text::toT(m_tag + " IP: " + m_ip + dns);
				}
				else
				{
					return Text::toT(m_tag);
				}
			}
		};
		
		static bool getUserParams(const UserPtr& user, UserParams& p_params);
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
		//CREATE_LOCK_INSTANCE_CM(g, Users);
		// [~] IRainman opt.
#undef CREATE_LOCK_INSTANCE_CM
		
		static void setIPUser(const UserPtr& p_user, const string& p_ip, const uint16_t p_udpPort = 0);
		
		static StringList getUsersByIp(const string &p_ip);
#ifndef IRAINMAN_IDENTITY_IS_NON_COPYABLE
		static Identity getIdentity(const UserPtr& user);
#endif // IRAINMAN_IDENTITY_IS_NON_COPYABLE
		static OnlineUserPtr getOnlineUserL(const UserPtr& p);
		static bool isOp(const UserPtr& aUser, const string& aHubUrl);
		/** Constructs a synthetic, hopefully unique CID */
		static CID makeCid(const string& nick, const string& hubUrl);
		
		void putOnline(const OnlineUserPtr& ou, bool p_is_fire_online) noexcept; // [!] IRainman fix.
		void putOffline(const OnlineUserPtr& ou, bool p_is_disconnect = false) noexcept; // [!] IRainman fix.
		
		static bool isMe(const CID& p_cid)
		{
			return p_cid == getMyCID();
		}
		static bool isMe(const UserPtr& p_user)
		{
			dcassert(p_user);
			return isMe(p_user->getCID());
		}
		static bool isMe(const OnlineUserPtr& p_user)
		{
			dcassert(p_user);
			return isMe(p_user->getUser());
		}
		static const UserPtr& getMe_UseOnlyForNonHubSpecifiedTasks() // [!] IRainman fix.
		{
			dcassert(g_me);
			return g_me;
		}
		static const Identity& getFlylinkDCIdentity()
		{
			dcassert(g_uflylinkdc);
			return g_iflylinkdc;
		}
		static void getOnlineClients(StringSet& p_onlineClients);
	private:
		void createMe(const string& cid, const string& nick);
		static void cheatMessage(Client* p_client, const string& p_report);
		static void userCommandL(const HintedUser& user, const UserCommand& uc, StringMap& params, bool compatibility);
		static void sendRawCommandL(const OnlineUser& ou, const int aRawCommand);
	public:
		// [~] IRainman fix.
		
		static void send(AdcCommand& c, const CID& to);
		static void upnp_error_force_passive();
		static void resend_ext_json();
		void connect(const HintedUser& user, const string& p_token, bool p_is_force_passive, bool& p_is_active_client);
		static void privateMessage(const HintedUser& user, const string& msg, bool thirdPerson);
		static void userCommand(const HintedUser& user, const UserCommand& uc, StringMap& params, bool compatibility);
		
		static int getMode(const FavoriteHubEntry* p_hub, bool& pbWantAutodetect);
		static bool isActive(const FavoriteHubEntry* p_hub, bool& pbWantAutodetect);
#ifdef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
		const Client::List& getClientsL() const
		{
			return g_clients;
		}
#endif
		static const CID& getMyCID();
		static void generateNewMyCID();
		static const CID& getMyPID();
		
		static void setListLength(const UserPtr& p, const string& listLen);
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		static void fileListDisconnected(const UserPtr& p);
#endif
		static void connectionTimeout(const UserPtr& p);
#ifdef FLYLINKDC_USE_DETECT_CHEATING
		static void checkCheating(const UserPtr& p, DirectoryListing* dl);
#endif
		static void setClientStatus(const UserPtr& p, const string& aCheatString, const int aRawCommand, bool aBadClient);
		
		static void setSupports(const UserPtr& p, const StringList& aSupports, const uint8_t knownUcSupports);
		static void setUnknownCommand(const UserPtr& p, const string& aUnknownCommand);
		static void reportUser(const HintedUser& user);
		
		static void shutdown();
		static void before_shutdown();
		static void clear();
		static bool isShutdown()
		{
			extern volatile bool g_isShutdown;
			return g_isShutdown;
		}
		static bool isBeforeShutdown()
		{
			extern volatile bool g_isBeforeShutdown;
			return g_isBeforeShutdown;
		}
		static bool isStartup()
		{
			extern bool g_isStartupProcess;
			return g_isStartupProcess;
		}
		static void stopStartup()
		{
			extern bool g_isStartupProcess;
			g_isStartupProcess = false;
		}
		static void flushRatio(int p_max_count_flush);
		static void usersCleanup();
	private:
	
		typedef std::unordered_map<string, Client*, noCaseStringHash, noCaseStringEq> ClientList;
		static ClientList g_clients;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csClients;
		
		typedef boost::unordered_map<CID, UserPtr> UserMap;
		
		static UserMap g_users;
		
		static std::unique_ptr<webrtc::RWLockWrapper> g_csUsers;
		typedef std::multimap<CID, OnlineUserPtr> OnlineMap;
		typedef OnlineMap::iterator OnlineIter;
		typedef OnlineMap::const_iterator OnlineIterC;
		typedef pair<OnlineIter, OnlineIter> OnlinePair;
		typedef pair<OnlineIterC, OnlineIterC> OnlinePairC;
		
		static OnlineMap g_onlineUsers;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csOnlineUsers;
#ifdef FLYLINKDC_USE_ASYN_USER_UPDATE
		static OnlineUserList g_UserUpdateQueue;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csOnlineUsersUpdateQueue;
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept override;
#endif
		//static
		void addAsyncOnlineUserUpdated(const OnlineUserPtr& p_ou);
		static UserPtr g_me; // [!] IRainman fix: this is static object.
		static UserPtr g_uflylinkdc; // [+] IRainman fix.
		static Identity g_iflylinkdc; // [+] IRainman fix.
		static CID g_pid; // [!] IRainman fix this is static object.
		
		friend class Singleton<ClientManager>;
		friend class NmdcHub;
		
		ClientManager();
		~ClientManager();
		
		static void updateNick(const OnlineUserPtr& p_online_user);
		
		/// @return OnlineUserPtr found by CID and hint; discard any user that doesn't match the hint.
		static OnlineUserPtr findOnlineUserHintL(const CID& cid, const string& hintUrl)
		{
			// [!] IRainman: This function need to external lock.
			OnlinePairC p;
			return findOnlineUserHintL(cid, hintUrl, p);
		}
		/**
		* @param p OnlinePair of all the users found by CID, even those who don't match the hint.
		* @return OnlineUserPtr found by CID and hint; discard any user that doesn't match the hint.
		*/
		static OnlineUserPtr findOnlineUserHintL(const CID& cid, const string& hintUrl, OnlinePairC& p);
		
		void fireIncomingSearch(const string&, const string&, ClientManagerListener::SearchReply);
		// ClientListener
		void on(Connected, const Client* c) noexcept override;
		void on(UserUpdatedMyINFO, const OnlineUserPtr& user) noexcept override;
		void on(UsersUpdated, const Client* c, const OnlineUserList&) noexcept override;
		void on(ClientFailed, const Client*, const string&) noexcept override;
		void on(HubUpdated, const Client* c) noexcept override;
		void on(HubUserCommand, const Client*, int, int, const string&, const string&) noexcept override;
		// TODO void on(TTHSearch, Client* aClient, const string& aSeeker, const TTHValue& aTTH, bool isPassive) noexcept override;
		void on(AdcSearch, const Client* c, const AdcCommand& adc, const CID& from) noexcept override;
		// TimerManagerListener
		// void on(TimerManagerListener::Minute, uint64_t aTick) noexcept override;
		
		/** Indication that the application is being closed */
		static bool g_isSpyFrame;
};

#endif // !defined(CLIENT_MANAGER_H)

/**
 * @file
 * $Id: ClientManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
