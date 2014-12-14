/*
 * Copyright (C) 2011-2013 Alexey Solomin, a.rainman on gmail point com
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

#include "stdinc.h"
#include "UserManager.h"
#include "CFlylinkDBManager.h"
#include "QueueManager.h"
#include "Client.h"
#include "Wildcards.h"

UserManager::IgnoreMap UserManager::g_ignoreList;
bool UserManager::g_isEmptyIgnoreList = true;
dcdrun(bool UserManager::g_ignoreListLoaded = false);

UserManager::CheckedUserSet UserManager::checkedPasswordUsers;
UserManager::WaitingUserMap UserManager::waitingPasswordUsers;

FastCriticalSection UserManager::g_csPsw;
std::unique_ptr<webrtc::RWLockWrapper> UserManager::g_csIgnoreList = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> UserManager::g_csProtectedUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());

StringList UserManager::g_protectedUsersLower;

void UserManager::saveIgnoreList()
{

	webrtc::ReadLockScoped l(*g_csIgnoreList);
	dcassert(g_ignoreListLoaded); // [!] IRainman fix: You can not save the ignore list if it was not pre-loaded - it will erase the data!
	CFlylinkDBManager::getInstance()->save_ignore(g_ignoreList);
	g_isEmptyIgnoreList = g_ignoreList.empty();
}

UserManager::UserManager()
{
	webrtc::WriteLockScoped l(*g_csIgnoreList);
	dcassert(!g_ignoreListLoaded);
	CFlylinkDBManager::getInstance()->load_ignore(g_ignoreList);
	g_isEmptyIgnoreList = g_ignoreList.empty();
	dcdrun(g_ignoreListLoaded = true);
	
	SettingsManager::getInstance()->addListener(this);
}

UserManager::~UserManager()
{
	SettingsManager::getInstance()->removeListener(this);
}

UserManager::PasswordStatus UserManager::checkPrivateMessagePassword(const ChatMessage& pm)
{
	const UserPtr& user = pm.m_replyTo->getUser();
	FastLock l(g_csPsw);
	if (checkedPasswordUsers.find(user) != checkedPasswordUsers.cend())
	{
		return FREE;
	}
	else if (pm.m_text == SETTING(PM_PASSWORD))
	{
		waitingPasswordUsers.erase(user);
		checkedPasswordUsers.insert(user);
		return CHECKED;
	}
	else if (waitingPasswordUsers.find(user) != waitingPasswordUsers.cend())
	{
		return WAITING;
	}
	else
	{
		waitingPasswordUsers.insert(make_pair(user, true));
		return FIRST;
	}
}

#ifdef IRAINMAN_INCLUDE_USER_CHECK
void UserManager::checkUser(const OnlineUserPtr& user)
{
	if (BOOLSETTING(CHECK_NEW_USERS))
	{
		if (!ClientManager::getInstance()->isMe(user))
		{
			const Client& client = user->getClient();
			if (!client.getExcludeCheck() && client.isOp() &&
			        (client.isActive() || user->getIdentity().isTcpActive()))
			{
				if (!BOOLSETTING(PROT_FAVS) || !FavoriteManager::getInstance()->isNoFavUserOrUserBanUpload(user->getUser()))   // !SMT!-opt
				{
					if (!isInProtectedUserList(user->getIdentity().getNick()))
					{
						try
						{
							QueueManager::getInstance()->addList(HintedUser(user->getUser(), client.getHubUrl()), QueueItem::FLAG_USER_CHECK);
						}
						catch (const Exception& e)
						{
							LogManager::getInstance()->message(e.getError());
						}
					}
				}
			}
		}
	}
}
#endif // IRAINMAN_INCLUDE_USER_CHECK

void UserManager::on(SettingsManagerListener::UsersChanges) noexcept
{
	auto protUsers = SPLIT_SETTING_AND_LOWER(PROT_USERS);
	
	webrtc::WriteLockScoped l(*g_csProtectedUsers);
	swap(g_protectedUsersLower, protUsers);
}
bool UserManager::expectPasswordFromUser(const UserPtr& user)
{
	FastLock l(g_csPsw);
	auto i = waitingPasswordUsers.find(user);
	if (i == waitingPasswordUsers.end())
	{
		return false;
	}
	else if (i->second)
	{
		i->second = false;
		return true;
	}
	else
	{
		waitingPasswordUsers.erase(user);
		checkedPasswordUsers.insert(user);
		return false;
	}
}
tstring UserManager::getIgnoreListAsString()
{
	tstring l_result;
	webrtc::ReadLockScoped l(*g_csIgnoreList);
	for (auto i = g_ignoreList.cbegin(); i != g_ignoreList.cend(); ++i)
	{
		l_result += _T(' ') + Text::toT((*i));
	}
	return l_result;
}
