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
#include "FavoriteManager.h"
#include "Wildcards.h"


UserManager::IgnoreMap UserManager::g_ignoreList;
bool UserManager::g_isEmptyIgnoreList = true;
dcdrun(bool UserManager::g_ignoreListLoaded = false);

UserManager::CheckedUserSet UserManager::checkedPasswordUsers;
UserManager::WaitingUserMap UserManager::waitingPasswordUsers;

FastCriticalSection UserManager::g_csPsw;
std::unique_ptr<webrtc::RWLockWrapper> UserManager::g_csIgnoreList = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
#ifdef IRAINMAN_ENABLE_AUTO_BAN
std::unique_ptr<webrtc::RWLockWrapper> UserManager::g_csProtectedUsers = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
StringList UserManager::g_protectedUsersLower;
#endif

void UserManager::saveIgnoreList()
{

	CFlyReadLock(*g_csIgnoreList);
	dcassert(g_ignoreListLoaded); // [!] IRainman fix: You can not save the ignore list if it was not pre-loaded - it will erase the data!
	CFlylinkDBManager::getInstance()->save_ignore(g_ignoreList);
	g_isEmptyIgnoreList = g_ignoreList.empty();
}

UserManager::UserManager()
{
	CFlyWriteLock(*g_csIgnoreList);
	dcassert(!g_ignoreListLoaded);
	CFlylinkDBManager::getInstance()->load_ignore(g_ignoreList);
	g_isEmptyIgnoreList = g_ignoreList.empty();
	dcdrun(g_ignoreListLoaded = true);
}

UserManager::~UserManager()
{
}

UserManager::PasswordStatus UserManager::checkPrivateMessagePassword(const ChatMessage& pm)
{
	const UserPtr& user = pm.m_replyTo->getUser();
	CFlyFastLock(g_csPsw);
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
				if (!BOOLSETTING(PROT_FAVS) || !FavoriteManager::isNoFavUserOrUserBanUpload(user->getUser()))   // !SMT!-opt
				{
					if (!isInProtectedUserList(user->getIdentity().getNick()))
					{
						try
						{
							QueueManager::getInstance()->addList(HintedUser(user->getUser(), client.getHubUrl()), QueueItem::FLAG_USER_CHECK);
						}
						catch (const Exception& e)
						{
							LogManager::message(e.getError());
						}
					}
				}
			}
		}
	}
}
#endif // IRAINMAN_INCLUDE_USER_CHECK

void UserManager::getIgnoreList(StringSet& p_ignoreList)
{
	CFlyReadLock(*g_csIgnoreList);
	dcassert(g_ignoreListLoaded);
	p_ignoreList = g_ignoreList;
}
void UserManager::addToIgnoreList(const string& userName)
{
	{
		CFlyWriteLock(*g_csIgnoreList);
		dcassert(g_ignoreListLoaded);
		g_ignoreList.insert(userName);
	}
	saveIgnoreList();
}
void UserManager::removeFromIgnoreList(const string& userName)
{
	{
		CFlyWriteLock(*g_csIgnoreList);
		dcassert(g_ignoreListLoaded);
		g_ignoreList.erase(userName);
	}
	saveIgnoreList();
}
bool UserManager::isInIgnoreList(const string& nick)
{
	// dcassert(!nick.empty());
	if (!g_isEmptyIgnoreList && !nick.empty())
	{
		dcassert(!nick.empty());
		CFlyReadLock(*g_csIgnoreList);
		dcassert(g_ignoreListLoaded);
		return g_ignoreList.find(nick) != g_ignoreList.cend();
	}
	else
	{
		return false;
	}
}
void UserManager::setIgnoreList(const IgnoreMap& newlist)
{
	{
		CFlyWriteLock(*g_csIgnoreList);
		g_ignoreList = newlist;
	}
	saveIgnoreList();
}

#ifdef IRAINMAN_ENABLE_AUTO_BAN
void UserManager::reloadProtUsers()
{
	auto protUsers = SPLIT_SETTING_AND_LOWER(PROT_USERS);
	CFlyWriteLock(*g_csProtectedUsers);
	swap(g_protectedUsersLower, protUsers);
}
#endif

bool UserManager::expectPasswordFromUser(const UserPtr& user)
{
	CFlyFastLock(g_csPsw);
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
	CFlyReadLock(*g_csIgnoreList);
	for (auto i = g_ignoreList.cbegin(); i != g_ignoreList.cend(); ++i)
	{
		l_result += _T(' ') + Text::toT((*i));
	}
	return l_result;
}

void UserManager::openUserUrl(const UserPtr& aUser)
{
	const string& url = FavoriteManager::getUserUrl(aUser);
	if (!url.empty())
	{
		fly_fire1(UserManagerListener::OpenHub(), url);
	}
}
#ifdef IRAINMAN_ENABLE_AUTO_BAN
bool UserManager::isInProtectedUserList(const string& userName)
{
	CFlyReadLock(*g_csProtectedUsers);
	return Wildcard::patternMatchLowerCase(Text::toLower(userName), g_protectedUsersLower, false);
}
#endif