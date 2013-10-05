/*
 *
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

#ifndef USERINFO_H
#define USERINFO_H

#include "ClientManager.h"
#include "TaskQueue.h"
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
#include "UserInfoColumns.h"
#endif

enum Tasks { UPDATE_USER_JOIN, UPDATE_USER, REMOVE_USER, ADD_CHAT_LINE,
             ADD_STATUS_LINE, ADD_SILENT_STATUS_LINE, SET_WINDOW_TITLE, GET_PASSWORD,
             PRIVATE_MESSAGE, STATS, CONNECTED, DISCONNECTED, CHEATING_USER, USER_REPORT,
             GET_SHUTDOWN, SET_SHUTDOWN, KICK_MSG,
#ifdef RIP_USE_CONNECTION_AUTODETECT
             DIRECT_MODE_DETECTED
#endif
           };

struct OnlineUserTask : public Task // [!] IRainman fix.
#ifdef _DEBUG
		, virtual NonDerivable<OnlineUserTask>
#endif
{
	explicit OnlineUserTask(const OnlineUserPtr& ouser) : m_ouser(ouser)
	{
	}
	GETC(OnlineUserPtr, m_ouser, OnlineUser); // [!] IRainman fix: is its online user!!!
};

struct MessageTask : public Task // [!] IRainman fix.
#ifdef _DEBUG
		, virtual NonDerivable<MessageTask>
#endif
{
	explicit MessageTask(const ChatMessage& message) : m_message(message)
	{
	}
	GETC(ChatMessage, m_message, Message);
};

class UserInfo : public UserInfoBase
#ifdef _DEBUG
	, virtual NonDerivable<UserInfo> // [+] IRainman fix.
#endif
{
	private:
		const OnlineUserPtr m_ou; // [!] IRainman fix: use online user here!
		Util::CustomNetworkIndex m_location; // [+] IRainman opt.
	public:
	
		explicit UserInfo(const OnlineUserTask& u) : m_ou(u.getOnlineUser())
		{
		}
		static int compareItems(const UserInfo* a, const UserInfo* b, int col);
		bool update(int sortCol)
		{
			return (m_ou->getIdentity().getChanges() & (1 << sortCol)) != 0; // [!] IRAINMAN_USE_NG_FAST_USER_INFO
		}
		tstring getText(int p_col) const;
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		bool isLastIP() const
		{
			return getUser()->isLastIP();
		}
#endif
		bool isOP() const
		{
			return getIdentity().isOp();
		}
		const string& getIp() const
		{
			return getIdentity().getIp();
		}
		uint8_t getImageIndex() const
		{
			return UserInfoBase::getImage(*m_ou);
		}
		// [+] IRainman opt.
		const Util::CustomNetworkIndex& getLocation() const
		{
			return m_location;
		}
		void setLocation(const Util::CustomNetworkIndex& p_location)
		{
			m_location = p_location;
		}
		// [~] IRainman opt.
		const string& getNick() const
		{
			return m_ou->getIdentity().getNick();
		}
#ifdef IRAINMAN_USE_HIDDEN_USERS
		bool isHidden() const
		{
			return m_ou->getIdentity().isHidden();
		}
#endif
		const OnlineUserPtr& getOnlineUser() const
		{
			return m_ou;
		}
		const UserPtr& getUser() const
		{
			return m_ou->getUser();
		}
		const Identity& getIdentity() const
		{
			return m_ou->getIdentity();
		}
		tstring getHubs() const
		{
			return m_ou->getIdentity().getHubs();
		}
		static tstring formatSpeedLimit(const uint32_t limit);
		tstring getLimit() const;
		tstring getDownloadSpeed() const;
		typedef unordered_map<OnlineUserPtr, UserInfo*, OnlineUser::Hash> OnlineUserMapBase; // [!] IRainman fix: use online user here.
		class OnlineUserMap : public OnlineUserMapBase
#ifdef _DEBUG
			, virtual NonDerivable<OnlineUserMap>, boost::noncopyable // [+] IRainman fix.
#endif
		{
			public:
				UserInfo* findUser(const OnlineUserPtr& p_user) const // [!] IRainman fix: use online user here.
				{
					const auto i = find(p_user);
					return i == end() ? nullptr : i->second;
				}
		};
};


#endif //USERINFO_H
