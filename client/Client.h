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


#ifndef DCPLUSPLUS_DCPP_CLIENT_H
#define DCPLUSPLUS_DCPP_CLIENT_H

#include <atomic>
#include "ClientListener.h"
#include "DebugManager.h"
#include "SearchQueue.h"
#include "OnlineUser.h"
#include "BufferedSocket.h"
#include "ChatMessage.h"

struct CFlyClientStatistic
{
	uint32_t  m_count_user;
	uint32_t  m_message_count;
	int64_t   m_share_size;
	bool m_is_active;
	CFlyClientStatistic() : m_count_user(0), m_share_size(0), m_message_count(0), m_is_active(false)
	{
	}
	bool empty() const
	{
		return m_count_user == 0 && m_message_count == 0 && m_share_size == 0;
	}
};
class ClientBase
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
#ifdef RIP_USE_CONNECTION_AUTODETECT
		bool m_is_detect_active_connection;
#endif
	public:
		ClientBase() : m_type(DIRECT_CONNECT), m_is_detect_active_connection(false)
			//  , m_isActivMode(false)
		{ }
		virtual ~ClientBase() {} // [cppcheck]
		
		enum P2PType { DIRECT_CONNECT };
	protected:
		P2PType m_type;
	public:
#ifdef RIP_USE_CONNECTION_AUTODETECT
		bool isDetectActiveConnection() const
		{
			return m_is_detect_active_connection;
		}
		void setDetectActiveConnection()
		{
			m_is_detect_active_connection = true;
		}
		void resetDetectActiveConnection()
		{
			m_is_detect_active_connection = false;
		}
#endif
		bool isActive() const;
		virtual bool resendMyINFO(bool p_always_send, bool p_is_force_passive) = 0;
		P2PType getType() const
		{
			return m_type;
		}
		
		virtual const string getHubUrl() const = 0;
		virtual const string getHubName() const = 0; // [!] IRainman opt.
		virtual bool isOp() const = 0;
		virtual void connect(const OnlineUser& user, const string& p_token, bool p_is_force_passive) = 0;
		virtual void privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson = false) = 0;
};

/** Yes, this should probably be called a Hub */
class Client : public ClientBase, public Speaker<ClientListener>, public BufferedSocketListener, protected TimerManagerListener
{
	protected:
		std::unique_ptr<webrtc::RWLockWrapper> m_cs;
		void fire_user_updated(const OnlineUserList& p_list);
		void clearAvailableBytesL();
		void decBytesSharedL(int64_t p_bytes_shared);
		bool changeBytesSharedL(Identity& p_id, const int64_t p_bytes);
	public:
		bool isChangeAvailableBytes() const
		{
			return m_isChangeAvailableBytes;
		}
		int64_t getAvailableBytes() const
		{
			return m_availableBytes;
		}
		bool isFlySupportHub() const
		{
			return m_is_fly_support_hub;
		}
		uint8_t getVIPIconIndex() const
		{
			return m_vip_icon_index;
		}
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		bool isFlyAntivirusHub() const
		{
			return  m_isAutobanAntivirusIP || m_isAutobanAntivirusNick;
		}
#endif
		bool isSupressChatAndPM() const
		{
			return m_is_suppress_chat_and_pm;
		}
		void setSuppressChatAndPM(bool p_value)
		{
			m_is_suppress_chat_and_pm = p_value;
		}
		bool getSuppressChatAndPM() const
		{
			return m_is_suppress_chat_and_pm;
		}
		bool isLocalHub() const
		{
			return m_is_local_hub.value == boost::logic::tribool::true_value;
		}
		bool isGlobalHub() const
		{
			return m_is_local_hub.value == boost::logic::tribool::false_value;
		}
	protected:
		void setTypeHub(bool p_hub_type)
		{
			m_is_local_hub = p_hub_type;
		}
	public:
		virtual void resetAntivirusInfo() = 0;
		virtual void connect();
		virtual void disconnect(bool graceless);
		virtual void connect(const OnlineUser& p_user, const string& p_token, bool p_is_force_passive) = 0;
		virtual void hubMessage(const string& aMessage, bool thirdPerson = false) = 0;
		virtual void privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson = false) = 0; // !SMT!-S
		virtual void sendUserCmd(const UserCommand& command, const StringMap& params) = 0;
		
		uint64_t search_internal(const SearchParamToken& p_search_param);
		void cancelSearch(void* aOwner)
		{
			m_searchQueue.cancelSearch(aOwner);
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
		}
		
		bool isReady() const
		{
			return state != STATE_CONNECTING && state != STATE_DISCONNECTED;
		}
		bool is_all_my_info_loaded() const
		{
			return m_client_sock && m_client_sock->is_all_my_info_loaded();
		}
		
		void set_all_my_info_loaded()
		{
			if (m_client_sock)
			{
				m_client_sock->set_all_my_info_loaded();
			}
		}
		
		bool isSecure() const;
		bool isTrusted() const;
		string getCipherName() const;
		vector<uint8_t> getKeyprint() const;
		
		bool isOp() const
		{
			return getMyIdentity().isOp();
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
		string getIpAsString() const
		{
			return m_ip.to_string();
		}
		const string& getAddress() const
		{
			return m_address;
		}
		boost::asio::ip::address_v4 getIp() const
		{
			return m_ip;
		}
		string getIpPort() const
		{
			return getIpAsString() + ':' + Util::toString(m_port);
		}
		string getHubUrlAndIP() const
		{
			return "[Hub: " + getHubUrl() + ", " + getIpPort() + "]";
		}
		string getLocalIp() const;
		
		void updatedMyINFO(const OnlineUserPtr& aUser);
		
		/*
		std::deque<OnlineUserPtr> m_update_online_user_deque;
		FastCriticalSection m_fs_update_online_user;
		void getNewMyINFO(std::deque<OnlineUserPtr>&    p_ou_array)
		{
		    CFlyFastLock(m_fs_update_online_user);
		    p_ou_array.swap(m_update_online_user_deque);
		}
		*/
		static int getTotalCounts()
		{
			return g_counts[COUNT_NORMAL] + g_counts[COUNT_REGISTERED] + g_counts[COUNT_OP];
		}
		
		static string getCounts();
		const string& getCountsIndivid() const;
		void getCountsIndivid(uint8_t& p_normal, uint8_t& p_registered, uint8_t& p_op) const;
		const string& getRawCommand(const int aRawCommand) const;
		bool allowPrivateMessagefromUser(const ChatMessage& message);
		bool allowChatMessagefromUser(const ChatMessage& message, const string& p_nick);
		
		void processingPassword();
		void escapeParams(StringMap& sm) const;
		void setSearchInterval(uint32_t aInterval, bool p_is_search_rule);
		void setSearchIntervalPassive(uint32_t aInterval, bool p_is_search_rule);
		
		uint32_t getSearchInterval() const
		{
			return m_searchQueue.m_interval;
		}
		uint32_t getSearchIntervalPassive() const
		{
			return m_searchQueue.m_interval_passive;
		}
		
		void cheatMessage(const string& msg)
		{
			fly_fire1(ClientListener::CheatMessage(), msg);
		}
		
		void reportUser(const string& report)
		{
			fly_fire2(ClientListener::UserReport(), this, report);
		}
		// [~] IRainman fix
		void reconnect();
		void shutdown();
		bool getExcludeCheck() const // [+] IRainman fix.
		{
			return m_exclChecks;
		}
		void send(const string& aMessage)
		{
			send(aMessage.c_str(), aMessage.length());
		}
		void send(const char* aMessage, size_t aLen);
		
		void setMyNick(const string& p_nick)
		{
			getMyIdentity().setNick(p_nick);
		}
		const string& getMyNick() const
		{
			return getMyIdentity().getNick();
		}
		const string getHubName() const
		{
			const string ni = getHubIdentity().getNick();
			return ni.empty() ? getHubUrl() : ni;
		}
		string getHubDescription() const
		{
			return getHubIdentity().getDescription();
		}
		virtual size_t getMaxLenNick() const
		{
			return 0;
		}
		virtual const string getHubUrl() const
		{
			return m_HubURL;
		}
		string toUtf8IfNeeded(const string& str) const
		{
			return Text::validateUtf8(str) ? str : Text::toUtf8(str, getEncoding());
		}
		// don't convert to UTF-8 if string is already in this encoding
		string toUtf8IfNeededMyINFO(const string& str) const
		{
			/*$ALL */
			return Text::validateUtf8(str, 4) ? str : Text::toUtf8(str, getEncoding());
		}
		string fromUtf8(const string& str) const
		{
			return Text::fromUtf8(str, getEncoding());
		}
#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
		string toUtf8(const string& str) const
		{
			string out;
			string::size_type i = 0;
			while (true)
			{
				auto f = str.find_first_of("\n\r" /* "\n\r\t:<>[] |" */, i);
				if (f == string::npos)
				{
					out += toUtf8IfNeeded(str.substr(i));
					break;
				}
				else
				{
					++f;
					out += toUtf8IfNeeded(str.substr(i, f - i));
					i = f;
				}
			};
			return out;
		}
		const string& fromUtf8Chat(const string& str) const
		{
			return str;
		}
#else
		string toUtf8(const string& str) const
		{
			return toUtf8IfNeeded(str);
		}
		string toUtf8MyINFO(const string& str) const
		{
			return toUtf8IfNeededMyINFO(str);
		}
		string fromUtf8Chat(const string& str) const
		{
			return fromUtf8(str);
		}
#endif
		
		bool NmdcPartialSearch(const SearchParam& p_search_param);
		
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		uint32_t getHubID() const
		{
			return m_HubID;
		}
#endif
	private:
		uint32_t m_message_count;
		uint8_t m_vip_icon_index;
		boost::logic::tribool m_is_local_hub;
		bool m_is_fly_support_hub;
		bool m_is_suppress_chat_and_pm;
		
		struct CFlyFloodCommand
		{
			std::vector<std::pair<string, int64_t>> m_flood_command;
			int64_t  m_start_tick;
			int64_t  m_tick;
			uint32_t m_count;
			bool m_is_ban;
			CFlyFloodCommand() : m_start_tick(0), m_tick(0), m_count(0), m_is_ban(false)
			{
			}
		};
		typedef boost::unordered_map<string, CFlyFloodCommand> CFlyFloodCommandMap;
		CFlyFloodCommandMap m_flood_detect;
	protected:
		bool isFloodCommand(const string& p_command, const string& p_line);
		
		OnlineUserPtr m_myOnlineUser;
		OnlineUserPtr m_hubOnlineUser;
	public:
		bool isMeCheck(const OnlineUserPtr& ou);
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
		Identity& getHubIdentity()
		{
			return getHubOnlineUser()->getIdentity();
		}
		const Identity& getHubIdentity() const
		{
			return getHubOnlineUser()->getIdentity();
		}
		// [~] IRainman fix.
		
		GETSET(string, defpassword, Password);
		const string getCurrentDescription() const
		{
			return getMyIdentity().getDescription();
		}
		void setCurrentDescription(const string& descr)
		{
			getMyIdentity().setDescription(descr);
		}
		GETSET(string, randomTempNick, RandomTempNick)
		GETSET(string, name, Name)
		GETSET(string, rawOne, RawOne);
		GETSET(string, rawTwo, RawTwo);
		GETSET(string, rawThree, RawThree);
		GETSET(string, rawFour, RawFour);
		GETSET(string, rawFive, RawFive);
		GETSET(string, favIp, FavIp);
		GETSET(string, m_clientName, ClientName);
		GETSET(string, m_clientVersion, ClientVersion);
		GETM(uint64_t, m_lastActivity, LastActivity);
		GETSET(uint32_t, m_reconnDelay, ReconnDelay);
		uint32_t getMessagesCount() const
		{
			return m_message_count;
		}
		void incMessagesCount()
		{
			++m_message_count;
		}
		void clearMessagesCount()
		{
			m_message_count = 0;
		}
		
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
		
		// [~] IRainman fix.
		GETSET(bool, autoReconnect, AutoReconnect);
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
		
		bool getHideShare() const
		{
			return m_is_hide_share;
		}
		void setHideShare(bool p_is_hide_share)
		{
			m_is_hide_share = p_is_hide_share;
			if (m_client_sock)
			{
				m_client_sock->set_is_hide_share(p_is_hide_share);
			}
		}
	private:
		bool m_is_hide_share;
		
#endif
	private:
		bool m_is_override_name;
	protected:
		string getTagVersion() const;
		
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		virtual bool hubIsNotSupportSlot() const = 0;// [+]IRainman
#endif // IRAINMAN_ENABLE_AUTO_BAN
//[~]FlylinkDC

		friend class ClientManager;
		friend class User;
		Client(const string& p_HubURL, char p_separator_, bool p_is_secure,
             bool p_is_auto_connect, Socket::Protocol proto_);
		virtual ~Client();
		
		enum CountType
		{
			COUNT_NORMAL,
			COUNT_REGISTERED,
			COUNT_OP,
			COUNT_UNCOUNTED,
		};
		
		static std::atomic<std::uint32_t> g_counts[COUNT_UNCOUNTED];
		
		enum States
		{
			STATE_CONNECTING,   ///< Waiting for socket to connect
			STATE_PROTOCOL,     ///< Protocol setup
			STATE_IDENTIFY,     ///< Nick setup
			STATE_VERIFY,       ///< Checking password
			STATE_NORMAL,       ///< Running
			STATE_DISCONNECTED  ///< Nothing in particular
		} state;
		
		SearchQueue m_searchQueue;
		BufferedSocket* m_client_sock;
		void reset_socket(); //[+]FlylinkDC++ Team
		
		// [+] brain-ripper
		// need to protect socket:
		// shutdown may be called while calling
		// function that uses a sock && sock->... expression
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		mutable FastCriticalSection csSock; // [!] IRainman opt: no needs recursive mutex here.
#endif
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		mutable FastCriticalSection m_cs_virus;
#endif
		int64_t m_availableBytes;
		bool    m_isChangeAvailableBytes;
		//unsigned m_count_validate_denide;
		
		void updateCounts(bool aRemove);
		void updateActivity()
		{
			m_lastActivity = GET_TICK();
		}
		
		/** Reload details from favmanager or settings */
		const FavoriteHubEntry* reloadSettings(bool updateNick);
		
		virtual void checkNick(string& p_nick) = 0;
		virtual void search_token(const SearchParamToken& p_search_param) = 0;
		
		// TimerManagerListener
		virtual void on(TimerManagerListener::Second, uint64_t aTick) noexcept override;
		virtual void on(TimerManagerListener::Minute, uint64_t aTick) noexcept override;
		// BufferedSocketListener
		virtual void on(Connecting) noexcept override
		{
			fly_fire1(ClientListener::Connecting(), this);
		}
		virtual void on(Connected) noexcept override;
		virtual void on(Line, const string& aLine) noexcept override;
		virtual void on(Failed, const string&) noexcept override;
		
		void messageYouHaweRightOperatorOnThisHub(); // [+] IRainman.
		
		const string& getOpChat() const // [+] IRainman fix.
		{
			return m_opChat;
		}
	protected:
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		bool m_isAutobanAntivirusIP;
		bool m_isAutobanAntivirusNick;
		boost::unordered_set<string> m_virus_nick;
		string m_AntivirusCommandIP;
#endif
#ifdef FLYLINKDC_USE_VIRUS_CHECK_DEBUG
		boost::unordered_set<string> m_virus_nick_checked;
		boost::unordered_map<string, string> m_check_myinfo_dup;
#endif
	private:
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		uint32_t m_HubID;
#endif
		const string m_HubURL; // [!] IRainman fix: this is const member.
		string m_address;
		boost::asio::ip::address_v4 m_ip;
		uint16_t m_port;
		
		string m_keyprint;
		// [+] IRainman fix.
		string m_opChat;
		//boost::unordered_set<string> m_auto_open_pm;
		bool m_exclChecks;
		// [~] IRainman fix.
		
		const char m_separator;
        Socket::Protocol m_proto;

		bool m_is_secure_connect;
		CountType m_countType;
	public:
		bool isSecureConnect() const
		{
			return m_is_secure_connect;
		}
		bool isInOperatorList(const string& userName) const;
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		unsigned getVirusBotCount() const
		{
			CFlyFastLock(m_cs_virus);
			return m_virus_nick.size();
		}
#endif
		
		static string g_last_search_string;
};

#endif // !defined(CLIENT_H)

/**
 * @file
 * $Id: Client.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
