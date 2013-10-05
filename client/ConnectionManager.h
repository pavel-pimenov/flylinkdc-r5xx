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

#ifndef DCPLUSPLUS_DCPP_CONNECTION_MANAGER_H
#define DCPLUSPLUS_DCPP_CONNECTION_MANAGER_H

#include "TimerManager.h"

#include "UserConnection.h"
#include "Singleton.h"
#include "ConnectionManagerListener.h"
#include "HintedUser.h"

class SocketException;

class ConnectionQueueItem
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		typedef ConnectionQueueItem* Ptr;
		typedef FlyLinkVector<Ptr> List;
		typedef List::const_iterator Iter;
		
		enum State
		{
			CONNECTING,                 // Recently sent request to connect
			WAITING,                    // Waiting to send request to connect
			NO_DOWNLOAD_SLOTS,          // Not needed right now
			ACTIVE                      // In one up/downmanager
		};
		
		ConnectionQueueItem(const UserPtr& aUser, bool aDownload) : token(Util::toString(Util::rand())),
			lastAttempt(0), errors(0), state(WAITING), download(aDownload), m_user(aUser) { }
			
		const string& getToken() const
		{
			return token;
		}
		GETSET(uint64_t, lastAttempt, LastAttempt);
		GETSET(int, errors, Errors); // Number of connection errors, or -1 after a protocol error
		GETSET(State, state, State);
		bool getDownload() const
		{
			return download;
		}
		
		UserPtr& getUser()
		{
			return m_user;
		}
		const UserPtr& getUser() const
		{
			return m_user;
		}
	private:
		const string token;
		UserPtr m_user;
		const bool download;
};

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
			FastLock l(cs);
			expectedConnections.insert(make_pair(aNick, NickHubPair(aMyNick, aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
			                                                        , reason
#endif
			                                                       )));
		}
		
		NickHubPair remove(const string& aNick)
		{
			FastLock l(cs);
			ExpectMap::iterator i = expectedConnections.find(aNick);
			
			if (i == expectedConnections.end())
				return NickHubPair(Util::emptyString, Util::emptyString
#ifdef RIP_USE_CONNECTION_AUTODETECT
				                   , REASON_DEFAULT
#endif
				                  );
				                  
			NickHubPair tmp = i->second;
			expectedConnections.erase(i);
			
			return tmp;
		}
		
	private:
		/** Nick -> myNick, hubUrl for expected NMDC incoming connections */
		typedef map<string, NickHubPair> ExpectMap;
		ExpectMap expectedConnections;
		
		FastCriticalSection cs; // [!] IRainman opt: use spin lock here.
};

// Comparing with a user...
inline bool operator==(ConnectionQueueItem::Ptr ptr, const UserPtr& aUser)
{
	return ptr->getUser() == aUser;
}

class ConnectionManager : public Speaker<ConnectionManagerListener>,
	public UserConnectionListener, TimerManagerListener,
	public Singleton<ConnectionManager>
{
	public:
		void nmdcExpect(const string& aNick, const string& aMyNick, const string& aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
		                , ExpectedMap::DefinedExpectedReason reason = ExpectedMap::REASON_DEFAULT
#endif
		               )
		{
			expectedConnections.add(aNick, aMyNick, aHubUrl
#ifdef RIP_USE_CONNECTION_AUTODETECT
			                        , reason
#endif
			                       );
		}
		
		void nmdcConnect(const string& aServer, uint16_t aPort, const string& aMyNick, const string& hubUrl, const string& encoding,
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
		                 bool stealth,
#endif
		                 bool secure);
		void nmdcConnect(const string& aServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aNick, const string& hubUrl, const string& encoding,
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
		                 bool stealth,
#endif
		                 bool secure);
		void adcConnect(const OnlineUser& aUser, uint16_t aPort, const string& aToken, bool secure);
		void adcConnect(const OnlineUser& aUser, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aToken, bool secure);
		
		void getDownloadConnection(const UserPtr& aUser);
		void force(const UserPtr& aUser);
		void setUploadLimit(const UserPtr& aUser, int lim);
		
		void disconnect(const UserPtr& aUser);
		void disconnect(const UserPtr& aUser, bool isDownload); // [!] IRainman fix.
		
		void shutdown();
		bool isShuttingDown() const
		{
			return shuttingDown;
		}
		
		/** Find a suitable port to listen on, and start doing it */
		void listen();
		void disconnect() noexcept;
		
		uint16_t getPort() const
		{
			return server ? static_cast<uint16_t>(server->getPort()) : 0;
		}
		uint16_t getSecurePort() const
		{
			return secureServer ? static_cast<uint16_t>(secureServer->getPort()) : 0;
		}
		
		static uint16_t iConnToMeCount;
		// [-] SSA private:
		
		class Server : public BASE_THREAD
		{
			public:
				Server(bool p_secure, uint16_t p_port, const string& p_ip = "0.0.0.0");
				uint16_t getPort() const
				{
					return m_port;
				}
				~Server()
				{
					m_die = true;
					join();
				}
			private:
				int run() noexcept;
				
				Socket m_sock;
				uint16_t m_port;
				string m_ip;
				bool m_secure;
				volatile bool m_die; // [!] IRainman fix: this variable is volatile.
		};
		
		friend class Server;
		// [+] SSA private:
	private:
		SharedCriticalSection cs; // [!] IRainman fix.
		
		/** All ConnectionQueueItems */
		ConnectionQueueItem::List downloads;
		ConnectionQueueItem::List uploads;
		
		/** All active connections */
		UserConnectionList userConnections;
		
#define USING_IDLERS_IN_CONNECTION_MANAGER // [!] IRainman fix: don't disable this.
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		UserList checkIdle;
#endif
		
		StringList nmdcFeatures;
		StringList adcFeatures;
		
		ExpectedMap expectedConnections;
		
		uint64_t floodCounter;
		
		Server* server;
		Server* secureServer;
		
		bool shuttingDown;
		
		friend class Singleton<ConnectionManager>;
		ConnectionManager();
		
		~ConnectionManager();
		
		void setIP(UserConnection* p_uc, ConnectionQueueItem* p_qi); // [+]PPA
		UserConnection* getConnection(bool aNmdc, bool secure) noexcept;
		void putConnection(UserConnection* aConn);
		
		void addUploadConnection(UserConnection* uc);
		void addDownloadConnection(UserConnection* uc);
		
		ConnectionQueueItem* getCQI(const UserPtr& aUser, bool download);
		void putCQI(ConnectionQueueItem* cqi);
		
		void accept(const Socket& sock, bool secure) noexcept;
		
		bool checkKeyprint(UserConnection *aSource);
		
		void failed(UserConnection* aSource, const string& aError, bool protocolError);
		
		bool checkIpFlood(const string& aServer, uint16_t aPort, const string& userInfo);
		
		// UserConnectionListener
		void on(Connected, UserConnection*) noexcept;
		void on(Failed, UserConnection*, const string&) noexcept;
		void on(ProtocolError, UserConnection*, const string&) noexcept;
		void on(CLock, UserConnection*, const string&
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
		        , const string&
#endif
		       ) noexcept;
		void on(Key, UserConnection*, const string&) noexcept;
		void on(Direction, UserConnection*, const string&, const string&) noexcept;
		void on(MyNick, UserConnection*, const string&) noexcept;
		void on(Supports, UserConnection*, StringList &&) noexcept; // [!] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
		
		void on(AdcCommand::SUP, UserConnection*, const AdcCommand&) noexcept;
		void on(AdcCommand::INF, UserConnection*, const AdcCommand&) noexcept;
		void on(AdcCommand::STA, UserConnection*, const AdcCommand&) noexcept;
		
		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept;
		void on(TimerManagerListener::Minute, uint64_t aTick) noexcept;
		
};

#endif // !defined(CONNECTION_MANAGER_H)

/**
 * @file
 * $Id: ConnectionManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
