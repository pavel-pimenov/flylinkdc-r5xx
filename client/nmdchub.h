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
typedef boost::unordered_map<string, std::pair<std::string, unsigned>>  CFlyUnknownCommand;
typedef boost::unordered_map<string, std::unordered_map<std::string, unsigned> >  CFlyUnknownCommandArray;

class NmdcHub : public Client, private Flags
{
	public:
		using Client::send;
		using Client::connect;
		
		void connect(const OnlineUser& p_user, const string& p_token, bool p_is_force_passive);
		virtual void disconnect(bool p_graceless);
		
		void hubMessage(const string& aMessage, bool thirdPerson = false);
		void privateMessage(const OnlineUserPtr& aUser, const string& aMessage, bool thirdPerson = false); // !SMT!-S
		void sendUserCmd(const UserCommand& command, const StringMap& params);
		virtual void search_token(const SearchParamToken& p_search_param);
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
			webrtc::ReadLockScoped l(*m_cs);
			p_list.reserve(m_users.size());
			for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
			{
				p_list.push_back(i->second);
			}
		}
		void AutodetectInit()
		{
#ifdef RIP_USE_CONNECTION_AUTODETECT
			m_bAutodetectionPending = true;
			m_iRequestCount = 0;
#endif
			m_bLastMyInfoCommand = DIDNT_GET_YET_FIRST_MYINFO;
		}
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
		void AutodetectComplete()
		{
			m_bAutodetectionPending = false;
			m_iRequestCount = 0;
			
			// send MyInfo, to update state on hub
			myInfo(false);
		}
		
		bool IsAutodetectPending() const
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
			SUPPORTS_USERIP2 = 0x04,
#ifdef FLYLINKDC_USE_FLYHUB
			SUPPORTS_FLYHUB = 0x08
#endif
		};
		
		// MyInfo states.
		// Used to detect end of connection to hub sequence (after gettinf list of users)
		enum DefinedMeyInfoState {DIDNT_GET_YET_FIRST_MYINFO, FIRST_MYINFO, ALREADY_GOT_MYINFO};
		
		typedef boost::unordered_map<string, OnlineUser*> NickMap;
		
		NickMap  m_users;
		string   m_lastMyInfo;
		int64_t  m_lastBytesShared;
		uint64_t m_lastUpdate;
		uint8_t  m_supportFlags;
		char m_modeChar; // last Mode MyINFO
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool m_hubSupportsSlots;//[+] FlylinkDC
#endif
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
		bool m_bAutodetectionPending;
		int m_iRequestCount;
#endif
		static CFlyUnknownCommand g_unknown_command;
		static CFlyUnknownCommandArray g_unknown_command_array;
		static FastCriticalSection g_unknown_cs;
	public:
		static void log_all_unknown_command();
		static string get_all_unknown_command();
	private:
		void processAutodetect(bool p_is_myinfo);
		
		DefinedMeyInfoState m_bLastMyInfoCommand; // [+] FlylinkDC
		
		NmdcHub(const string& aHubURL, bool secure);
		~NmdcHub();
		
		void clearUsers();
		void onLine(const string& aLine);
		void resetAntivirusInfo();
		
		OnlineUserPtr getUser(const string& aNick, bool p_hub, bool p_first_load); // [!] IRainman fix: return OnlineUserPtr and add hub
		OnlineUserPtr findUser(const string& aNick) const;
		void putUser(const string& aNick);
		
		// don't convert to UTF-8 if string is already in this encoding
		string toUtf8IfNeededMyINFO(const string& str) const
		{
			/*$ALL */
			return Text::validateUtf8(str, 4) ? str : Text::toUtf8(str, getEncoding());
		}
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
		string toUtf8MyINFO(const string& str) const
		{
			return toUtf8IfNeededMyINFO(str);
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
			send("$Version 1,0091|"); // TODO - ?
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
		                
		static void sendUDPSR(Socket& p_udp, const string& p_seeker, const string& p_sr, const Client* p_client);
		void NmdcSearch(const SearchParam& p_search_param);
		string calcExternalIP() const;
		void revConnectToMe(const OnlineUser& aUser);
		bool resendMyINFO(bool p_is_force_passive);
		void myInfo(bool p_alwaysSend, bool p_is_force_passive = false);
		void myInfoParse(const string& param);
		void searchParse(const string& param, bool p_is_passive);
		void connectToMeParse(const string& param);
		void revConnectToMeParse(const string& param);
		void hubNameParse(const string& param);
		void supportsParse(const string& param);
		void userCommandParse(const string& param);
		void lockParse(const string& aLine);
		void helloParse(const string& param);
		void userIPParse(const string& param);
		void botListParse(const string& param);
		void nickListParse(const string& param);
		void opListParse(const string& param);
		void toParse(const string& param);
		void chatMessageParse(const string& aLine);
		void supports(const StringList& feat);
		void updateFromTag(Identity& id, const string & tag);
		
		virtual void checkNick(string& p_nick);
		
		void on(BufferedSocketListener::Connected) noexcept override;
		void on(BufferedSocketListener::Line, const string& l) noexcept override;
		void on(BufferedSocketListener::MyInfoArray, StringList&) noexcept override; // [+]PPA
		void on(BufferedSocketListener::DDoSSearchDetect, const string&) noexcept override; // [+]PPA
		void on(BufferedSocketListener::SearchArrayTTH, CFlySearchArrayTTH&) noexcept override; // [+]PPA
		void on(BufferedSocketListener::SearchArrayFile, const CFlySearchArrayFile&) noexcept override; // [+]PPA
		void on(BufferedSocketListener::Failed, const string&) noexcept override;
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
