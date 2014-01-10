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

#ifndef DCPLUSPLUS_DCPP_CLIENT_H
#define DCPLUSPLUS_DCPP_CLIENT_H

#include "ClientListener.h"
#include "DebugManager.h"
#include "SearchQueue.h"
#include "OnlineUser.h"
#include "BufferedSocket.h"
#include "ChatMessage.h"

class ClientBase
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
	
		ClientBase() : type(DIRECT_CONNECT) { }
		virtual ~ClientBase() {} // [cppcheck]
		
		enum P2PType { DIRECT_CONNECT
#ifdef STRONG_USE_DHT
		               , DHT
#endif
		             };
		P2PType type;
		
		P2PType getType() const
		{
			return type;
		}
		
		virtual const string& getHubUrl() const = 0;
		virtual const string& getHubName() const = 0; // [!] IRainman opt.
		virtual bool isOp() const = 0;
		virtual void connect(const OnlineUser& user, const string& token) = 0;
		virtual void privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson = false) = 0;
		
};

/** Yes, this should probably be called a Hub */
class Client : public ClientBase, public Speaker<ClientListener>, public BufferedSocketListener, protected TimerManagerListener
{
	protected:
		mutable FastCriticalSection cs; // [!] IRainman opt: use spinlock here!
		void fire_user_updated(const OnlineUserList& p_list)
		{
			if (!p_list.empty())
			{
				fire(ClientListener::UsersUpdated(), this, p_list);
			}
		}
		
		// https://code.google.com/p/flylinkdc/issues/detail?id=1231
//#define CLIENT_SUMMARY_SHARE_IS_ATOMIC
//#define CLIENT_SUMMARY_SHARE_CLEANUP_IDENTITY
#define CLIENT_SUMMARY_SHARE_NOT_FULL_DIAG

		void clearAvailableBytes()
		{
#ifdef CLIENT_SUMMARY_SHARE_IS_ATOMIC
			FastLock l(cs);
#endif
#ifndef CLIENT_SUMMARY_SHARE_NOT_FULL_DIAG
			dcassert(m_availableBytes >= 0);
#endif
			m_availableBytes = 0;
		}
		void decBytesShared(Identity& p_id)
		{
#ifdef CLIENT_SUMMARY_SHARE_CLEANUP_IDENTITY
			changeBytesShared(p_id, 0);
#else
# ifdef CLIENT_SUMMARY_SHARE_IS_ATOMIC
			FastLock l(cs);
# endif
			dcdrun(const auto oldSum = m_availableBytes);
			dcassert(oldSum >= 0);
			const auto old = p_id.getBytesShared();
			dcassert(old >= 0);
			m_availableBytes -= old;
# ifndef CLIENT_SUMMARY_SHARE_NOT_FULL_DIAG
			dcassert(old <= oldSum); // bug here.
			dcassert(m_availableBytes >= 0);
# endif
#endif
		}
		void changeBytesShared(Identity& p_id, const int64_t p_bytes)
		{
			// https://code.google.com/p/flylinkdc/issues/detail?id=1231
			dcassert(p_bytes >= 0);
#ifdef CLIENT_SUMMARY_SHARE_IS_ATOMIC
			FastLock l(cs);
#endif
			dcdrun(const auto oldSum = m_availableBytes);
			dcassert(oldSum >= 0);
			const auto old = p_id.getBytesShared();
			dcassert(old >= 0);
			m_availableBytes -= old;
			p_id.setBytesShared(p_bytes);
#ifndef CLIENT_SUMMARY_SHARE_NOT_FULL_DIAG
			dcassert(old <= oldSum);
#endif
			m_availableBytes += p_bytes;
#ifndef CLIENT_SUMMARY_SHARE_NOT_FULL_DIAG
			dcassert(m_availableBytes >= 0);
#endif
		}
	public:
		int64_t getAvailableBytes() const
		{
			// https://code.google.com/p/flylinkdc/issues/detail?id=1231
#ifdef CLIENT_SUMMARY_SHARE_IS_ATOMIC
			FastLock l(cs);
#endif
#ifndef CLIENT_SUMMARY_SHARE_NOT_FULL_DIAG
			dcassert(m_availableBytes >= 0);
#endif
			return m_availableBytes;
		}
		
		typedef std::unordered_map<string, Client*, noCaseStringHash, noCaseStringEq> List;
		
		virtual void connect();
		virtual void disconnect(bool graceless);
		
		virtual void connect(const OnlineUser& user, const string& token) = 0;
		virtual void hubMessage(const string& aMessage, bool thirdPerson = false) = 0;
		virtual void privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson = false) = 0; // !SMT!-S
		virtual void sendUserCmd(const UserCommand& command, const StringMap& params) = 0;
		
		uint64_t search(Search::SizeModes aSizeMode, int64_t aSize, Search::TypeModes aFileType, const string& aString, const string& aToken, const StringList& aExtList, void* owner);
		void cancelSearch(void* aOwner)
		{
			searchQueue.cancelSearch(aOwner);
		}
		
		virtual void password(const string& pwd) = 0;
		virtual void info(bool p_force) = 0;
		
		virtual size_t getUserCount() const = 0;
		
		virtual void send(const AdcCommand& command) = 0;
		
		virtual string escape(const string& str) const
		{
			return str;
		}
		
		bool isConnected() const
		{
			return state != STATE_DISCONNECTED;
			// [!]IRainman to see other fixes, look:
			// void Client::connect()
			// void Client::on(Failed, const string& aLine)
			// void Client::disconnect(bool graceLess)
			// FastLock lock(csSock); // [+] brain-ripper
			// return sock && sock->isConnected();
		}
		
		bool isReady() const
		{
			return /*[-]IRainman fix: sock &&*/ state != STATE_CONNECTING && state != STATE_DISCONNECTED;
		}
		bool is_all_my_info_loaded() const
		{
			return sock && sock->is_all_my_info_loaded();
		}
		
		void set_all_my_info_loaded()
		{
			if (sock)
				sock->set_all_my_info_loaded();
		}
		
		bool isSecure() const;
		bool isTrusted() const;
		string getCipherName() const;
		vector<uint8_t> getKeyprint() const;
		
		bool isOp() const
		{
			return getMyIdentity().isOp(); // [!] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=923
		}
		
		bool isRegistered() const // [+] IRainman fix.
		{
			return getMyIdentity().isRegistered();
		}
		
		virtual void refreshUserList(bool) = 0;
		
		virtual void getUserList(OnlineUserList& list) const = 0;
		
		virtual OnlineUserPtr getUser(const UserPtr& aUser);
		virtual OnlineUserPtr findUser(const string& aNick) const = 0;
		
		uint16_t getPort() const
		{
			return m_port;
		}
		const string& getAddress() const
		{
			return m_address;
		}
		
		const string& getIp() const
		{
			return m_ip;
		}
		string getIpPort() const
		{
			return getIp() + ':' + Util::toString(m_port);
		}
		string getLocalIp() const;
		
		void updated(const OnlineUserPtr& aUser)
		{
			fire(ClientListener::UserUpdated(), this, aUser);    // !SMT!-fix
		}
		
		static int getTotalCounts()
		{
			return counts[COUNT_NORMAL] + counts[COUNT_REGISTERED] + counts[COUNT_OP];
		}
		
		static string getCounts()
		{
			char buf[128];
			return string(buf, snprintf(buf, _countof(buf), "%u/%u/%u", counts[COUNT_NORMAL].load(), counts[COUNT_REGISTERED].load(), counts[COUNT_OP].load()));
		}
//[+]FlylinkDC
		const string& getCountsIndivid() const
		{
			// [!] IRainman Exclusive hub, send H:1/0/0 or similar
			if (isOp())
			{
				static const string g_001 = "0/0/1";
				return g_001;
			}
			else if (isRegistered())
			{
				static const string g_010 = "0/1/0";
				return g_010;
			}
			else
			{
				static const string g_100 = "1/0/0";
				return g_100;
			}
		}
		void getCountsIndivid(uint8_t& p_normal, uint8_t& p_registered, uint8_t& p_op) const
		{
			// [!] IRainman Exclusive hub, send H:1/0/0 or similar
			p_normal = p_registered = p_op = 0;
			if (isOp())
			{
				p_op = 1;
			}
			else if (isRegistered())
			{
				p_registered = 1;
			}
			else
			{
				p_normal = 1;
			}
		}
//[~]FlylinkDC
		const string& getRawCommand(const int aRawCommand) const
		{
			switch (aRawCommand)
			{
				case 1:
					return rawOne;
				case 2:
					return rawTwo;
				case 3:
					return rawThree;
				case 4:
					return rawFour;
				case 5:
					return rawFive;
			}
			return Util::emptyString;
		}
		// [+] IRainman fix.
		bool allowPrivateMessagefromUser(const ChatMessage& message);
		bool allowChatMessagefromUser(const ChatMessage& message);
		
		void processingPassword()
		{
			if (!getPassword().empty())
			{
				password(getPassword());
				fire(ClientListener::StatusMessage(), this, STRING(STORED_PASSWORD_SENT));
			}
			else
			{
				fire(ClientListener::GetPassword(), this);
			}
		}
		// [~] IRainman fix.
		StringMap& escapeParams(StringMap& sm)
		{
			for (auto i = sm.begin(); i != sm.end(); ++i)
			{
				i->second = escape(i->second);
			}
			return sm;
		}
		
		void setSearchInterval(uint32_t aInterval)
		{
			// min interval is 2 seconds in FlylinkDC
			searchQueue.interval = max(aInterval, (uint32_t)(2000)); // [!] FlylinkDC
		}
		
		uint32_t getSearchInterval() const
		{
			return searchQueue.interval;
		}
		
		void cheatMessage(const string& msg)
		{
			fire(ClientListener::CheatMessage(), msg);
		}
		/* [!] IRainman fix
		void reportUser(const Identity& i)
		{
		    fire(ClientListener::UserReport(), this, i);
		}
		*/
		void reportUser(const string& report) // TODO: use onlineuser here?
		{
			fire(ClientListener::UserReport(), this, report);
		}
		// [~] IRainman fix
		void reconnect();
		void shutdown();
		bool isActive() const
		{
			// return ClientManager::isActive(hubUrl); [-] IRainman opt.
			return m_isActivMode; // [+] IRainman opt.
			// [!]IRainman Fixed auto-adjust the connection type
		}
		bool getExcludeCheck() const // [+] IRainman fix.
		{
			return m_exclChecks;
		}
		void send(const string& aMessage)
		{
			send(aMessage.c_str(), aMessage.length());
		}
		void send(const char* aMessage, size_t aLen);
		
		const string& getMyNick() const // [!] IRainman opt.
		{
			return getMyIdentity().getNick();
		}
		const string& getHubName() const // [!] IRainman opt.
		{
			const string& ni = getHubIdentity().getNick();
			return ni.empty() ? getHubUrl() : ni;
		}
		string getHubDescription() const
		{
			return getHubIdentity().getDescription();
		}
		
		virtual const string& getHubUrl() const
		{
			return m_HubURL;
		}
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		uint32_t getHubID() const
		{
			return m_HubID;
		}
#endif
//[+]FlylinkDC
		/* [-] IRainman what is it?
		bool WaitForInitialization(DWORD dwTimeout) const
		{
		    return WaitForSingleObject(m_hEventClientInitialized, dwTimeout) == WAIT_OBJECT_0;
		}
		*/
//[~]FlylinkDC
		/* [!] IRainman fix
		[-] GETSET(Identity, myIdentity, MyIdentity);
		[-] GETSET(Identity, hubIdentity, HubIdentity);
		[-] Identity& getHubIdentity()
		[-] {
		[-]     return hubIdentity;
		[-] }
		*/
	protected:
		OnlineUserPtr m_myOnlineUser;
		OnlineUserPtr m_hubOnlineUser;
	public:
		bool isMe(const OnlineUserPtr& ou)
		{
			return ou == getMyOnlineUser();
		}
		const OnlineUserPtr& getMyOnlineUser() const
		{
			return m_myOnlineUser;
		}
		const Identity& getMyIdentity() const
		{
			return getMyOnlineUser()->getIdentity();
		}
		Identity& getMyIdentity()
		{
			return getMyOnlineUser()->getIdentity();
		}
		const OnlineUserPtr& getHubOnlineUser() const
		{
			return m_hubOnlineUser;
		}
		const Identity& getHubIdentity() const
		{
			return getHubOnlineUser()->getIdentity();
		}
		// [~] IRainman fix.
		
		GETSET(string, defpassword, Password);
		
		// [!] IRainman fix
		// [-] GETSET(string, currentNick, CurrentNick);
		// [-] GETSET(string, currentDescription, CurrentDescription);
		void setCurrentNick(const string& p_nick)
		{
			getMyIdentity().setNick(p_nick);
		}
		const string getCurrentDescription() const
		{
			return getMyIdentity().getDescription();
		}
		void setCurrentDescription(const string& descr)
		{
			getMyIdentity().setDescription(descr);
		}
		// [~] IRainman fix.
		GETSET(string, name, Name)
		GETSET(string, rawOne, RawOne);
		GETSET(string, rawTwo, RawTwo);
		GETSET(string, rawThree, RawThree);
		GETSET(string, rawFour, RawFour);
		GETSET(string, rawFive, RawFive);
		GETSET(string, favIp, FavIp);
		// [!] IRainman mimicry function
		// [-] GETSET(string, clientId, ClientId); // !SMT!-S
		GETSET(string, m_clientName, ClientName);
		GETSET(string, m_clientVersion, ClientVersion);
		// [~] IRainman mimicry function
		
		GETM(uint64_t, m_lastActivity, LastActivity);
		GETSET(uint32_t, m_reconnDelay, ReconnDelay);
//#ifndef IRAINMAN_USE_UNICODE_IN_NMDC
		GETSET(string, m_encoding, Encoding);
//#endif
		// [!] IRainman fix.
		// [-] GETSET(bool, registered, Registered);
		void setRegistered() // [+]
		{
			getMyIdentity().setRegistered(true);
		}
		void resetRegistered() // [+]
		{
			getMyIdentity().setRegistered(false);
		}
		void resetOp() // [+]
		{
			getMyIdentity().setOp(false);
		}
	public:
		// [~] IRainman fix.
		GETSET(bool, autoReconnect, AutoReconnect);
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
		GETSET(bool, stealth, Stealth); // [-] IRainamn: very old code.
#endif
//[+]FlylinkDC
		// [!] IRainman fix.
		// [-] GETSET(string, currentEmail, CurrentEmail);
		string getCurrentEmail() const
		{
			return getMyIdentity().getEmail();
		}
		void setCurrentEmail(const string& email)
		{
			getMyIdentity().setEmail(email);
		}
		// [~] IRainman fix.
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		GETSET(bool, hideShare, HideShare);
#endif
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		virtual bool hubIsNotSupportSlot() const = 0;// [+]IRainman
#endif // IRAINMAN_ENABLE_AUTO_BAN
//[~]FlylinkDC
	protected:
		friend class ClientManager;
		Client(const string& hubURL, char separator, bool secure_);
		virtual ~Client();
		
		enum CountType
		{
			COUNT_NORMAL,
			COUNT_REGISTERED,
			COUNT_OP,
			COUNT_UNCOUNTED,
		};
		
		static boost::atomic<uint16_t> counts[COUNT_UNCOUNTED];
		
		enum States
		{
			STATE_CONNECTING,   ///< Waiting for socket to connect
			STATE_PROTOCOL,     ///< Protocol setup
			STATE_IDENTIFY,     ///< Nick setup
			STATE_VERIFY,       ///< Checking password
			STATE_NORMAL,       ///< Running
			STATE_DISCONNECTED  ///< Nothing in particular
		} state;
		
		SearchQueue searchQueue;
		BufferedSocket* sock;
		void reset_socket(); //[+]FlylinkDC++ Team
		
		// [+] brain-ripper
		// initialization event, to delay processing of command line
		// until client is initialized
		//HANDLE m_hEventClientInitialized; [-] IRainman.
		
		// [+] brain-ripper
		// need to protect socket:
		// shutdown may be called while calling
		// function that uses a sock && sock->... expression
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		mutable FastCriticalSection csSock; // [!] IRainman opt: no needs recursive mutex here.
#endif
		
		int64_t m_availableBytes;
		
		void updateCounts(bool aRemove);
		void updateActivity()
		{
			m_lastActivity = GET_TICK();
		}
		
		/** Reload details from favmanager or settings */
		const FavoriteHubEntry* reloadSettings(bool updateNick);
		
		virtual void checkNick(string& p_nick) = 0;
		virtual void search(Search::SizeModes aSizeMode, int64_t aSize, Search::TypeModes aFileType, const string& aString, const string& aToken, const StringList& aExtList) = 0;
		
		// TimerManagerListener
		virtual void on(Second, uint64_t aTick) noexcept;
		// BufferedSocketListener
		virtual void on(Connecting) noexcept
		{
			fire(ClientListener::Connecting(), this);
		}
		virtual void on(Connected) noexcept;
		virtual void on(Line, const string& aLine) noexcept;
		virtual void on(Failed, const string&) noexcept;
		
		void messageYouHaweRightOperatorOnThisHub(); // [+] IRainman.
		
		const string& getOpChat() const // [+] IRainman fix.
		{
			return m_opChat;
		}
	private:
		bool isInOperatorList(const string& userName) const // [+] IRainman fix.
		{
			return Wildcard::patternMatch(userName, m_opChat, ';', false);
		}
		
		const string m_HubURL; // [!] IRainman fix: this is const member.
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		uint32_t m_HubID;
#endif
		string m_address;
		string m_ip;
		string m_keyprint;
		// [+] IRainman fix.
		string m_opChat;
		bool m_exclChecks;
		// [~] IRainman fix.
		
		bool m_isActivMode;// [+] IRainman opt.
		
		uint16_t m_port;
		char m_separator;
		bool m_secure;
		CountType m_countType;
	public:
		static string g_last_search_string;
};

#endif // !defined(CLIENT_H)

/**
 * @file
 * $Id: Client.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
