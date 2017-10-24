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


#ifndef DCPLUSPLUS_DCPP_CONNECTION_MANAGER_H
#define DCPLUSPLUS_DCPP_CONNECTION_MANAGER_H

#include <boost/asio/ip/address_v4.hpp>
#include "TimerManager.h"

#include "UserConnection.h"
#include "Singleton.h"
#include "ConnectionManagerListener.h"
#include "HintedUser.h"

class TokenManager
{
	public:
		string getToken() noexcept;
		string makeToken() noexcept;
		string toString() noexcept;
		bool addToken(const string& aToken) noexcept;
		bool isToken(const string& aToken) noexcept;
		void removeToken(const string& aToken) noexcept;
		unsigned countToken() noexcept;
		~TokenManager()
		{
#ifdef _DEBUG
			//CFlyFastLock(m_cs);
			//dcassert(m_tokens.empty());
#endif
		}
	private:
		StringSet m_tokens;
		FastCriticalSection m_cs;
};

class ConnectionQueueItem
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
	
		enum State
		{
			CONNECTING,                 // Recently sent request to connect
			WAITING,                    // Waiting to send request to connect
			NO_DOWNLOAD_SLOTS,          // Not needed right now
			ACTIVE                      // In one up/downmanager
		};
		
		ConnectionQueueItem(const HintedUser& aHintedUser, bool aDownload, const string& aToken) :
			m_connection_queue_token(aToken),
			lastAttempt(0),
			errors(0), state(WAITING), m_is_download(aDownload), m_hinted_user(aHintedUser), m_is_active_client(false)
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
			, m_count_waiting(0), m_is_force_passive(false)
#endif
		{
		}
		const string& getConnectionQueueToken() const
		{
			return m_connection_queue_token;
		}
		GETSET(uint64_t, lastAttempt, LastAttempt);
		GETSET(int, errors, Errors); // Number of connection errors, or -1 after a protocol error
		GETSET(State, state, State);
		//GETSET(string, hubUrl, HubUrl); // TODO - пока не доконца работает и не везде прокидывается
		bool m_is_active_client;
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
		unsigned short m_count_waiting;
		bool m_is_force_passive;
#endif
		bool isDownload() const
		{
			return m_is_download;
		}
		const UserPtr& getUser() const
		{
			return m_hinted_user.user;
		}
		const HintedUser& getHintedUser() const
		{
			return m_hinted_user;
		}
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
		void addAutoPassiveStatus(string& p_status) const
		{
			if (m_count_waiting > 1)
			{
				p_status += " (count: " + Util::toString(m_count_waiting) + ")";
			}
		}
#endif
		
	private:
		const string m_connection_queue_token;
		const HintedUser m_hinted_user;
		const bool m_is_download;
};

typedef std::shared_ptr<ConnectionQueueItem> ConnectionQueueItemPtr;

class ExpectedMap
{
	public:
#ifdef RIP_USE_CONNECTION_AUTODETECT
		enum DefinedExpectedReason {REASON_DEFAULT, REASON_DETECT_CONNECTION};
#endif
		
		/** Nick -> myNick, hubUrl for expected NMDC incoming connections */
		struct NickHubPair
		{
			const string m_Nick;
			const string m_HubUrl;
#ifdef RIP_USE_CONNECTION_AUTODETECT
			const DefinedExpectedReason reason;
#endif
			
			NickHubPair(const string& p_Nick, const string& p_HubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
			            , DefinedExpectedReason p_reason = REASON_DEFAULT
#endif
			           ):
				m_Nick(p_Nick),
				m_HubUrl(p_HubUrl)
#ifdef RIP_USE_CONNECTION_AUTODETECT
				, reason(p_reason)
#endif
			{
			}
		};
		//[~]FlylinkDC
		void add(const string& aNick, const string& aMyNick, const string& aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
		         , ExpectedMap::DefinedExpectedReason reason = ExpectedMap::REASON_DEFAULT
#endif
		        )
		{
			CFlyFastLock(cs);
			m_expectedConnections.insert(make_pair(aNick, NickHubPair(aMyNick, aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
			                                                          , reason
#endif
			                                                         )));
		}
		
		NickHubPair remove(const string& aNick)
		{
			CFlyFastLock(cs);
			const auto& i = m_expectedConnections.find(aNick);
			if (i == m_expectedConnections.end())
				return NickHubPair(Util::emptyString, Util::emptyString
#ifdef RIP_USE_CONNECTION_AUTODETECT
				                   , REASON_DEFAULT
#endif
				                  );
				                  
			const NickHubPair tmp = i->second;
			m_expectedConnections.erase(i);
			return tmp;
		}
		
	private:
		/** Nick -> myNick, hubUrl for expected NMDC incoming connections */
		typedef boost::unordered_map<string, NickHubPair> ExpectMap;
		ExpectMap m_expectedConnections;
		
		FastCriticalSection cs; // [!] IRainman opt: use spin lock here.
};

// Comparing with a user...
inline bool operator==(const ConnectionQueueItemPtr& ptr, const UserPtr& aUser)
{
	return ptr->getUser() == aUser;
}

class ConnectionManager :
	public Speaker<ConnectionManagerListener>,
	private UserConnectionListener,
	private ClientManagerListener,
	private TimerManagerListener,
	public Singleton<ConnectionManager>
{
	public:
		static TokenManager g_tokens_manager;
		void nmdcExpect(const string& aNick, const string& aMyNick, const string& aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
		                , ExpectedMap::DefinedExpectedReason reason = ExpectedMap::REASON_DEFAULT
#endif
		               )
		{
			m_expectedConnections.add(aNick, aMyNick, aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
			                          , reason
#endif
			                         );
		}
		
		void nmdcConnect(const string& aIPServer, uint16_t aPort, const string& aMyNick, const string& hubUrl, const string& encoding,
		                 bool secure);
		void nmdcConnect(const string& aIPServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aNick, const string& hubUrl, const string& encoding,
		                 bool secure);
		void adcConnect(const OnlineUser& aUser, uint16_t aPort, const string& aToken, bool secure);
		void adcConnect(const OnlineUser& aUser, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aToken, bool secure);
		
		void getDownloadConnection(const UserPtr& aUser);
		void force(const UserPtr& aUser);
		static void setUploadLimit(const UserPtr& aUser, int lim);
		static void test_tcp_port();
		static bool g_is_test_tcp_port;
		
		static void disconnect(const UserPtr& aUser);
		static void disconnect(const UserPtr& aUser, bool isDownload); // [!] IRainman fix.
		
		void shutdown();
		static bool isShuttingDown()
		{
			return g_shuttingDown;
		}
		
		/** Find a suitable port to listen on, and start doing it */
		void start_tcp_tls_listener();
		void disconnect();
		
		uint16_t getPort() const
		{
			return server ? static_cast<uint16_t>(server->getServerPort()) : 0;
		}
		uint16_t getSecurePort() const
		{
			return secureServer ? static_cast<uint16_t>(secureServer->getServerPort()) : 0;
		}
		
		static uint16_t g_ConnToMeCount;
		// [-] SSA private:
		
		class Server : public Thread
		{
			public:
				Server(bool p_is_secure, uint16_t p_port, const string& p_ip = "0.0.0.0");
				uint16_t getServerPort() const
				{
					dcassert(m_server_port);
					return m_server_port;
				}
				~Server()
				{
					m_die = true;
					join();
				}
			private:
				int run() noexcept;
				
				Socket m_sock;
				uint16_t m_server_port;
				string m_server_ip; // TODO - в DC++ этого уже нет.
				bool m_is_secure;
				volatile bool m_die; // [!] IRainman fix: this variable is volatile.
		};
		
		friend class Server;
		// [+] SSA private:
	private:
		static std::unique_ptr<webrtc::RWLockWrapper> g_csConnection;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csDownloads;
		//static std::unique_ptr<webrtc::RWLockWrapper> g_csUploads;
		static CriticalSection g_csUploads;
		static FastCriticalSection g_csDdosCheck;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csDdosCTM2HUBCheck;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csTTHFilter;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csFileFilter;
		
		/** All ConnectionQueueItems */
		static std::set<ConnectionQueueItemPtr> g_downloads;
		static std::set<ConnectionQueueItemPtr> g_uploads;
		
		/** All active connections */
		static boost::unordered_set<UserConnection*> g_userConnections;
		
		struct CFlyDDOSkey
		{
			std::string m_server;
			boost::asio::ip::address_v4 m_ip;
			CFlyDDOSkey(const std::string& p_server, boost::asio::ip::address_v4 p_ip) : m_server(p_server), m_ip(p_ip)
			{
			}
			bool operator < (const CFlyDDOSkey& p_val) const
			{
				return (m_ip < p_val.m_ip) || (m_ip == p_val.m_ip && m_server < p_val.m_server);
			}
		};
		class CFlyTickDetect
		{
			public:
				uint64_t m_first_tick;
				uint64_t m_last_tick;
				uint16_t m_count_connect;
				uint16_t m_block_id;
				CFlyTickDetect(): m_first_tick(0), m_last_tick(0), m_count_connect(0), m_block_id(0)
				{
				}
		};
		class CFlyTickTTH : public CFlyTickDetect
		{
		};
		class CFlyTickFile : public CFlyTickDetect
		{
		};
		class CFlyDDoSTick : public CFlyTickDetect
		{
			public:
				std::string m_type_block;
				boost::unordered_set<uint16_t> m_ports;
				boost::unordered_map<std::string, uint32_t> m_original_query_for_debug;
				CFlyDDoSTick()
				{
				}
				string getPorts() const
				{
					string l_ports;
					string l_sep;
					for (auto i = m_ports.cbegin(); i != m_ports.cend(); ++i)
					{
						l_ports += l_sep;
						l_ports += Util::toString(*i);
						l_sep = ",";
					}
					return " Port: " + l_ports;
				}
		};
		static std::map<CFlyDDOSkey, CFlyDDoSTick> g_ddos_map;
		static boost::unordered_set<string> g_ddos_ctm2hub; // $Error CTM2HUB
	public:
		static void addCTM2HUB(const string& p_server_port, const HintedUser& p_hinted_user);
	private:
		static boost::unordered_map<string, CFlyTickTTH> g_duplicate_search_tth;
		static boost::unordered_map<string, CFlyTickFile> g_duplicate_search_file;
		
#define USING_IDLERS_IN_CONNECTION_MANAGER // [!] IRainman fix: don't disable this.
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		UserList m_checkIdle;
#endif
		
		StringList nmdcFeatures;
		StringList adcFeatures;
		
		static FastCriticalSection g_cs_update;
		static UserSet g_users_for_update;
		void addOnUserUpdated(const UserPtr& aUser);
		void flushOnUserUpdated();
		
		ExpectedMap m_expectedConnections;
		
		uint64_t m_floodCounter;
		
		Server* server;
		Server* secureServer;
		
		static bool g_shuttingDown;
		
		friend class Singleton<ConnectionManager>;
		ConnectionManager();
		
		~ConnectionManager();
		
		static void setIP(UserConnection* p_conn, const ConnectionQueueItemPtr& p_qi);
		UserConnection* getConnection(bool aNmdc, bool secure) noexcept;
		void putConnection(UserConnection* p_conn);
		void deleteConnection(UserConnection* p_conn)
		{
			putConnection(p_conn);
			delete p_conn;
		}
		
		void addUploadConnection(UserConnection* p_conn);
		void addDownloadConnection(UserConnection* p_conn);
		
		ConnectionQueueItemPtr getCQI_L(const HintedUser& aHintedUser, bool download);
		void putCQI_L(ConnectionQueueItemPtr& cqi);
		
		void accept(const Socket& sock, bool secure, Server* p_server) noexcept;
		
		bool checkKeyprint(UserConnection *aSource);
		
		void failed(UserConnection* aSource, const string& aError, bool protocolError);
		
	public:
		//static bool getCipherNameAndIP(UserConnection* p_conn, string& p_chiper_name, string& p_ip);
		
		static bool checkIpFlood(const string& aIPServer, uint16_t aPort, const boost::asio::ip::address_v4& p_ip_hub, const string& userInfo, const string& p_HubInfo);
		static bool checkDuplicateSearchTTH(const string& p_search_command, const TTHValue& p_tth);
		static bool checkDuplicateSearchFile(const string& p_search_command);
	private:
	
		static void cleanupDuplicateSearchTTH(const uint64_t p_tick);
		static void cleanupDuplicateSearchFile(const uint64_t p_tick);
		static void cleanupIpFlood(const uint64_t p_tick);
		
		// UserConnectionListener
		void on(Connected, UserConnection*) noexcept override;
		void on(Failed, UserConnection*, const string&) noexcept override;
		void on(ProtocolError, UserConnection*, const string&) noexcept override;
		void on(CLock, UserConnection*, const string&) noexcept override;
		void on(Key, UserConnection*, const string&) noexcept override;
		void on(Direction, UserConnection*, const string&, const string&) noexcept override;
		void on(MyNick, UserConnection*, const string&) noexcept override;
		void on(Supports, UserConnection*, StringList &) noexcept override;
		
		void on(AdcCommand::SUP, UserConnection*, const AdcCommand&) noexcept override;
		void on(AdcCommand::INF, UserConnection*, const AdcCommand&) noexcept override;
		void on(AdcCommand::STA, UserConnection*, const AdcCommand&) noexcept override;
		
		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept override;
		void on(TimerManagerListener::Minute, uint64_t aTick) noexcept override;
// DEAD_CODE
		// ClientManagerListener
		void on(ClientManagerListener::UserConnected, const UserPtr& aUser) noexcept override;
		void on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept override;
		
		void onUserUpdated(const UserPtr& aUser);
		
};

#endif // !defined(CONNECTION_MANAGER_H)

/**
 * @file
 * $Id: ConnectionManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
