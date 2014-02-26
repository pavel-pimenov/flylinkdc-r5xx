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
#include "LogManager.h"
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
#include "UserInfoColumns.h"
#endif

enum Tasks
{
	ADD_STATUS_LINE,
#ifndef FLYLINKDC_ADD_CHAT_LINE_USE_WIN_MESSAGES_Q
	ADD_CHAT_LINE,
#endif
#ifndef FLYLINKDC_REMOVE_USER_WIN_MESSAGES_Q
	REMOVE_USER,
#endif
#ifndef FLYLINKDC_PRIVATE_MESSAGE_USE_WIN_MESSAGES_Q
	PRIVATE_MESSAGE,
#endif
#ifndef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
	UPDATE_USER_JOIN,
#endif
	GET_PASSWORD,
	STATS,
#ifdef RIP_USE_CONNECTION_AUTODETECT
	DIRECT_MODE_DETECTED
#endif
};

#ifndef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
struct OnlineUserTask : public Task
{
	explicit OnlineUserTask(const OnlineUserPtr& p_ou) : m_ou(p_ou)
	{
	}
	const OnlineUserPtr m_ou;
};
#endif

#ifndef FLYLINKDC_PRIVATE_MESSAGE_USE_WIN_MESSAGES_Q
struct MessageTask : public Task
{
	explicit MessageTask(ChatMessage* p_message_ptr) : m_message_ptr(p_message_ptr)
	{
	}
	ChatMessage* m_message_ptr;
};
#endif
class UserInfo : public UserInfoBase
#ifdef _DEBUG
	, virtual NonDerivable<UserInfo> // [+] IRainman fix.
#endif
{
	private:
		const OnlineUserPtr m_ou; // [!] IRainman fix: use online user here!
		Util::CustomNetworkIndex m_location; // [+] IRainman opt.
	public:
	
		explicit UserInfo(const OnlineUserPtr& p_ou) : m_ou(p_ou)
		{
		}
		static int compareItems(const UserInfo* a, const UserInfo* b, int col);
		bool is_update(int sortCol)
		{
#ifdef IRAINMAN_USE_NG_FAST_USER_INFO
			return (m_ou->getIdentity().getChanges() & (1 << sortCol)) != 0; // [!] IRAINMAN_USE_NG_FAST_USER_INFO
#else
			return true;
#endif
		}
		tstring getText(int p_col) const;
		bool isOP() const
		{
			return getIdentity().isOp();
		}
		string getIpAsString() const
		{
			return getIdentity().getIpAsString();
		}
		boost::asio::ip::address_v4 getIp() const
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
		void calcLocation();
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
		typedef std::unordered_map<OnlineUserPtr, UserInfo*, OnlineUser::Hash> OnlineUserMapBase; // [!] IRainman fix: use online user here.
		class OnlineUserMap : public OnlineUserMapBase
#ifdef _DEBUG
			, virtual NonDerivable<OnlineUserMap>, boost::noncopyable // [+] IRainman fix.
#endif
		{
			public:
				UserInfo* findUser(const OnlineUserPtr& p_user) const // [!] IRainman fix: use online user here.
				{
					if (p_user->isFirstFind())
					{
						//    LogManager::getInstance()->message("[++++++++++++++++++ UserInfo* findUser] Hub = " + p_user->getClient().getHubUrl() + " First user = " + p_user->getUser()->getLastNick() );
						dcassert(find(p_user) == end());
						return nullptr;
					}
					else
					{
						const auto i = find(p_user);
						// LogManager::getInstance()->message("[================== UserInfo* findUser] Hub = " + p_user->getClient().getHubUrl() + " First user = " + p_user->getUser()->getLastNick() );
						return i == end() ? nullptr : i->second;
					}
				}
		};
};


#endif //USERINFO_H
