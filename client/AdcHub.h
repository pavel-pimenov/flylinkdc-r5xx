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

#ifndef DCPLUSPLUS_DCPP_ADC_HUB_H
#define DCPLUSPLUS_DCPP_ADC_HUB_H

#include "Client.h"
#include "AdcSupports.h"
#include "Socket.h"

class ClientManager;

class AdcHub : public Client, public CommandHandler<AdcHub>
{
	public:
		using Client::send;
		using Client::connect;
		
		void connect(const OnlineUser& user, const string& token);
		void connect(const OnlineUser& user, const string& token, bool secure);
		
		void hubMessage(const string& aMessage, bool thirdPerson = false);
		void privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson = false);
		void sendUserCmd(const UserCommand& command, const StringMap& params);
		void search(Search::SizeModes aSizeMode, int64_t aSize, int aFileType, const string& aString, const string& aToken, const StringList& aExtList);
		void password(const string& pwd);
		void info(bool alwaysSend);
		void refreshUserList(bool);
		
		size_t getUserCount() const
		{
			//FastLock l(cs); // [-] https://code.google.com/p/flylinkdc/issues/detail?id=1231 experiment!
			return m_users.size();
		}
		
		string escape(const string& str) const
		{
			return AdcCommand::escape(str, false);
		}
		void send(const AdcCommand& cmd);
		
		string getMySID()
		{
			return AdcCommand::fromSID(sid);
		}
		
		static const vector<StringList>& getSearchExts();
		static StringList parseSearchExts(int flag);
		
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool hubIsNotSupportSlot() const // [+] IRainman this function needed for Autoban working correctly
		{
			return false;
		}
#endif // IRAINMAN_ENABLE_AUTO_BAN
	private:
		friend class ClientManager;
		friend class CommandHandler<AdcHub>;
		friend class Identity;
		
		AdcHub(const string& aHubURL, bool secure);
		
		~AdcHub();
		
		/** Map session id to OnlineUser */
		typedef unordered_map<uint32_t, OnlineUser*> SIDMap;
		
		void getUserList(OnlineUserList& p_list) const
		{
			FastLock l(cs);
			for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
			{
				if (i->first != AdcCommand::HUB_SID)
				{
					p_list.push_back(i->second);
				}
			}
		}
		
		bool oldPassword;
		Socket udp;
		SIDMap m_users;
		StringMap lastInfoMap;
		
		string salt;
		uint32_t sid;
		
		unordered_set<uint32_t> forbiddenCommands;
		
		static const vector<StringList> searchExts;
		
		string checkNick(const string& nick);
		
		OnlineUserPtr getUser(const uint32_t aSID, const CID& aCID); // [!] IRainman fix return OnlineUserPtr [IntelC++ 2012 beta2] warning #1125: function "Client::getUser(const UserPtr &)" is hidden by "AdcHub::getUser" -- virtual function override intended?
		OnlineUserPtr findUser(const uint32_t sid) const; // [!] IRainman fix return OnlineUserPtr
		OnlineUserPtr findUser(const CID& cid) const; // [!] IRainman fix return OnlineUserPtr
		
		// just a workaround
		OnlineUserPtr findUser(const string& aNick) const
		{
			FastLock l(cs);
			for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
			{
				if (i->second->getIdentity().getNick() == aNick)
				{
					return i->second;
				}
			}
			return nullptr;
		}
		
		void putUser(const uint32_t sid, bool disconnect);
		
		void clearUsers();
		
		void handle(AdcCommand::SUP, AdcCommand& c) noexcept;
		void handle(AdcCommand::SID, AdcCommand& c) noexcept;
		void handle(AdcCommand::MSG, AdcCommand& c) noexcept;
		void handle(AdcCommand::INF, AdcCommand& c) noexcept;
		void handle(AdcCommand::GPA, AdcCommand& c) noexcept;
		void handle(AdcCommand::QUI, AdcCommand& c) noexcept;
		void handle(AdcCommand::CTM, AdcCommand& c) noexcept;
		void handle(AdcCommand::RCM, AdcCommand& c) noexcept;
		void handle(AdcCommand::STA, AdcCommand& c) noexcept;
		void handle(AdcCommand::SCH, AdcCommand& c) noexcept;
		void handle(AdcCommand::CMD, AdcCommand& c) noexcept;
		void handle(AdcCommand::RES, AdcCommand& c) noexcept;
		void handle(AdcCommand::GET, AdcCommand& c) noexcept;
		void handle(AdcCommand::NAT, AdcCommand& c) noexcept;
		void handle(AdcCommand::RNT, AdcCommand& c) noexcept;
		void handle(AdcCommand::PSR, AdcCommand& c) noexcept;
		void handle(AdcCommand::ZON, AdcCommand& c) noexcept;
		void handle(AdcCommand::ZOF, AdcCommand& c) noexcept;
		
		
		template<typename T> void handle(T, AdcCommand&) { }
		
		void sendSearch(AdcCommand& c);
		void sendUDP(const AdcCommand& cmd) noexcept;
		void unknownProtocol(uint32_t target, const string& protocol, const string& token);
		bool secureAvail(uint32_t target, const string& protocol, const string& token);
		
		// [-] IRainman see class Client, not rewrite this!
		//void on(Connecting) noexcept
		//{
		//  fire(ClientListener::Connecting(), this);
		//}
		void on(Connected) noexcept;
		void on(Line, const string& aLine) noexcept;
		void on(Failed, const string& aLine) noexcept;
		
		void on(Second, uint64_t aTick) noexcept;
		
};

#endif // !defined(ADC_HUB_H)

/**
 * @file
 * $Id: AdcHub.h 578 2011-10-04 14:27:51Z bigmuscle $
 */
