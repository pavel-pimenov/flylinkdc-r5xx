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

#ifndef DCPLUSPLUS_DCPP_NMDC_HUB_H
#define DCPLUSPLUS_DCPP_NMDC_HUB_H

#include "SettingsManager.h"
#include "forward.h"
#include "User.h"
#include "Text.h"
#include "ConnectionManager.h"
#include "UploadManager.h"
#include "ZUtils.h"

class ClientManager;

class NmdcHub : public Client, private Flags
{
	public:
		using Client::send;
		using Client::connect;
		
		void connect(const OnlineUser& aUser, const string&);
		
		void hubMessage(const string& aMessage, bool thirdPerson = false);
		void privateMessage(const OnlineUserPtr& aUser, const string& aMessage, bool thirdPerson = false); // !SMT!-S
		void sendUserCmd(const UserCommand& command, const StringMap& params);
		void search(Search::SizeModes aSizeType, int64_t aSize, int aFileType, const string& aString, const string& aToken, const StringList& aExtList);
		void password(const string& aPass)
		{
			send("$MyPass " + fromUtf8(aPass) + '|');
		}
		void info(bool p_force)
		{
			myInfo(p_force);
		}
		
		size_t getUserCount() const
		{
			//FastLock l(cs); // [-] https://code.google.com/p/flylinkdc/issues/detail?id=1231 experiment!
			return m_users.size();
		}
		
		string escape(const string& str) const
		{
			return validateMessage(str, false);
		}
		static string unescape(const string& str)
		{
			return validateMessage(str, true);
		}
		
		void send(const AdcCommand&)
		{
			dcassert(0);
		}
		
		static string validateMessage(string tmp, bool reverse);
		void refreshUserList(bool);
		
		void getUserList(OnlineUserList& p_list) const
		{
			FastLock l(cs);
			for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
			{
				p_list.push_back(i->second);
			}
		}
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
		void AutodetectComplete()
		{
			m_bAutodetectionPending = false;
			m_iRequestCount = 0;
			
			// send MyInfo, to update state on hub
			myInfo(false);
		}
		
		bool IsAutodetectPending()
		{
			return m_bAutodetectionPending;
		}
		
		void RequestConnectionForAutodetect();
#endif
		
	private:
		friend class ClientManager;
		enum SupportFlags
		{
			SUPPORTS_USERCOMMAND = 0x01,
			SUPPORTS_NOGETINFO = 0x02,
			SUPPORTS_USERIP2 = 0x04
		};
		
		// MyInfo states.
		// Used to detect end of connection to hub sequence (after gettinf list of users)
		enum DefinedMeyInfoState {DIDNT_GET_YET_FIRST_MYINFO, FIRST_MYINFO, ALREADY_GOT_MYINFO};
		
		
		typedef unordered_map<string, OnlineUser*, noCaseStringHash, noCaseStringEq> NickMap;
		typedef NickMap::const_iterator NickIter;
		
		NickMap m_users;
		
		string lastmyinfo;
		int64_t lastbytesshared;
		uint64_t lastUpdate;
		uint8_t m_supportFlags;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool m_hubSupportsSlots;//[+] FlylinkDC
#endif
		
#ifdef IRAINMAN_USE_SEARCH_FLOOD_FILTER
		typedef list<pair<string, uint64_t> > FloodMap;
		FloodMap seekers;
		FloodMap flooders;
#endif
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
#define MAX_CONNECTION_REQUESTS_COUNT 3
		bool m_bAutodetectionPending;
		int m_iRequestCount;
#endif
		
		DefinedMeyInfoState m_bLastMyInfoCommand; // [+] FlylinkDC
		
		NmdcHub(const string& aHubURL, bool secure);
		~NmdcHub();
		
		void clearUsers();
		void onLine(const string& aLine) noexcept;
		
		OnlineUserPtr getUser(const string& aNick, bool hub = false); // [!] IRainman fix: return OnlineUserPtr and add hub
		OnlineUserPtr findUser(const string& aNick) const;
		void putUser(const string& aNick);
		
		// don't convert to UTF-8 if string is already in this encoding
		string toUtf8IfNeeded(const string& str) const
		{
			return Text::validateUtf8(str) ? str : Text::toUtf8(str, getEncoding());
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
				auto f = str.find_first_of("\n\r" /* "\n\r\t:<>[] |" */ , i);
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
		string fromUtf8Chat(const string& str) const
		{
			return fromUtf8(str);
		}
#endif
		void privateMessage(const string& nick, const string& aMessage, bool thirdPerson);
		void validateNick(const string& aNick)
		{
			send("$ValidateNick " + fromUtf8(aNick) + '|');
		}
		void key(const string& aKey)
		{
			send("$Key " + aKey + '|');
		}
		void version()
		{
			send("$Version 1,0091|");
		}
		void getNickList()
		{
			send("$GetNickList|");
		}
		void connectToMe(const OnlineUser& aUser
#ifdef RIP_USE_CONNECTION_AUTODETECT
		                 , ExpectedMap::DefinedExpectedReason reason = ExpectedMap::REASON_DEFAULT
#endif
		                );
		void revConnectToMe(const OnlineUser& aUser);
		void myInfo(bool alwaysSend);
		void supports(const StringList& feat);
#ifdef IRAINMAN_USE_SEARCH_FLOOD_FILTER
		void clearFlooders(uint64_t tick);
#endif
		
		void updateFromTag(Identity& id, string && tag); // [!] IRainman opt.
		
		string checkNick(const string& aNick);
		
		// TimerManagerListener
		void on(Second, uint64_t aTick) noexcept;
		
		void on(Connected) noexcept;
		void on(Line, const string& l) noexcept;
		void on(Failed, const string&) noexcept;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	public:
		bool hubIsNotSupportSlot() const //[+]FlylinkDC
		{
			return m_hubSupportsSlots;
		}
#endif // IRAINMAN_ENABLE_AUTO_BAN
};

#endif // !defined(NMDC_HUB_H)

/**
 * @file
 * $Id: nmdchub.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
