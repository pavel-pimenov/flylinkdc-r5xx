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
		
		void getUserList(OnlineUserList& p_list) const;
		void AutodetectInit();
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
		void AutodetectComplete();
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
#ifdef FLYLINKDC_USE_EXT_JSON
			SUPPORTS_EXTJSON = 0x08,
#endif
			SUPPORTS_NICKRULE = 0x10
		};
		
		// MyInfo states.
		// Used to detect end of connection to hub sequence (after gettinf list of users)
		enum DefinedMeyInfoState {DIDNT_GET_YET_FIRST_MYINFO, FIRST_MYINFO, ALREADY_GOT_MYINFO};
		
		typedef boost::unordered_map<string, OnlineUser*> NickMap;
		
		NickMap  m_users;
		string   m_lastMyInfo;
		string   m_lastExtJSONInfo;
		string   m_lastExtJSONSupport;
		int64_t  m_lastBytesShared;
		uint64_t m_lastUpdate;
		uint8_t  m_supportFlags;
		uint8_t  m_version_fly_info;
		CFlySearchArrayTTH m_delay_search;
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
		static uint8_t g_version_fly_info;
	public:
		static void inc_version_fly_info()
		{
			++g_version_fly_info;
		}
		static void log_all_unknown_command();
		static string get_all_unknown_command();
	private:
		void processAutodetect(bool p_is_myinfo);
		
		DefinedMeyInfoState m_bLastMyInfoCommand; // [+] FlylinkDC
		string m_last_antivirus_detect_url;
		struct CFlyNickRule
		{
			uint8_t m_nick_rule_min;
			uint8_t m_nick_rule_max;
			vector<char> m_invalid_char;
			vector<string> m_prefix;
			CFlyNickRule() : m_nick_rule_min(0), m_nick_rule_max(0)
			{
			}
			void convert_nick(string& p_nick)
			{
				for (auto i = m_invalid_char.cbegin(); i != m_invalid_char.cend(); ++i)
				{
					std::replace(p_nick.begin(), p_nick.end(), *i, '_');
				}
				if (!m_prefix.empty())
				{
					const auto l_pref_pos = p_nick.find(m_prefix[0]);
					if (l_pref_pos != 0 && l_pref_pos == string::npos)
					{
						p_nick = m_prefix[0] + p_nick;
					}
				}
				if (m_nick_rule_max && p_nick.length() > m_nick_rule_max)
				{
					p_nick = p_nick.substr(0, m_nick_rule_max);
				}
				if (m_nick_rule_min && p_nick.length() < m_nick_rule_min)
				{
					p_nick += "_R";
					while (p_nick.length() < m_nick_rule_min)
					{
						const auto l_nick_len = int(m_nick_rule_min - p_nick.length());
						p_nick += Util::toString(Util::rand()).substr(0, l_nick_len);
					}
				}
			}
		};
		CFlyNickRule m_nick_rule;
		NmdcHub(const string& aHubURL, bool secure, bool p_is_auto_connect);
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
		void sendValidateNick(const string& aNick)
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
		bool resendMyINFO(bool p_always_send, bool p_is_force_passive);
		void myInfo(bool p_alwaysSend, bool p_is_force_passive = false);
		void myInfoParse(const string& param);
#ifdef FLYLINKDC_USE_EXT_JSON
		bool extJSONParse(const string& param, bool p_is_disable_fire = false);
		//std::unordered_map<string, string> m_ext_json_deferred;
#endif
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
