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

#include "stdinc.h"
#include "FavoriteManager.h"
#include "ClientManager.h"
#include "HttpConnection.h"
#include "StringTokenizer.h"
#include "SimpleXML.h"
#include "UserCommand.h"
#include "BZUtils.h"
#include "FilteredFile.h"
#include "ConnectionManager.h"
#include "LogManager.h"

// [+] FlylinkDC supports hub
const string FavoriteManager::g_SupportsHubUrl = "adcs://adcs.flylinkdc.com:2780";
bool FavoriteManager::m_SupportsHubExist = false;
// [+] IRainman mimicry function
const FavoriteManager::mimicrytag FavoriteManager::g_MimicryTags[] =
{
	// Up from http://ru.wikipedia.org/wiki/DC%2B%2B
	FavoriteManager::mimicrytag("++", DCVERSIONSTRING),     // Version from core.
	FavoriteManager::mimicrytag("EiskaltDC++", "2.2.8"),    // 25 jun 2013 http://code.google.com/p/eiskaltdc/
	FavoriteManager::mimicrytag("AirDC++", "2.50"),         // 1 jun 2013 http://www.airdcpp.net/
	FavoriteManager::mimicrytag("RSX++", "1.21"),           // 14 apr 2011 http://rsxplusplus.sourceforge.net/
	FavoriteManager::mimicrytag("ApexDC++", "1.5.6"),       // 7 apr 2013 http://www.apexdc.net/changes/
	FavoriteManager::mimicrytag("PWDC++", "0.41"),          // 29th Dec 2005: Project discontinued
	FavoriteManager::mimicrytag("IceDC++", "1.01a"),        // 17 jul 2009 http://sourceforge.net/projects/icedc/
	FavoriteManager::mimicrytag("StrgDC++", "2.43"),        // latest public beta (project possible dead) http://strongdc.sourceforge.net/download.php?lang=eng
	FavoriteManager::mimicrytag("OlympP2P", "4.0 RC3"),     // Project discontinued
	FavoriteManager::mimicrytag(nullptr, nullptr),          // terminating, don't delete this
};

void FavoriteManager::splitClientId(const string& p_id, string& p_name, string& p_version)
{
	const auto l_space = p_id.find(' ');
	if (l_space != string::npos)
	{
		p_name = p_id.substr(0, l_space);
		p_version = p_id.substr(l_space + 1);
		const auto l_vMarker = p_version.find("V:");
		if (l_vMarker != string::npos)
		{
			p_version = p_version.substr(l_vMarker + 2);
		}
	}
}
// [~] IRainman mimicry function

FavoriteManager::FavoriteManager() : m_isNotEmpty(false), m_lastId(0),
#ifndef IRAINMAN_USE_SEPARATE_CS_IN_FAVORITE_MANAGER
	csDirs(csUsers), csHubs(csUsers), csPublicHubs(csUsers), csUserCommand(csUsers),
#endif
#ifdef IRAINMAN_ENABLE_HUB_LIST
	m_useHttp(false), m_running(false), c(nullptr), m_lastServer(0), m_listType(TYPE_NORMAL),
#endif
	m_dontSave(false)
{
	SettingsManager::getInstance()->addListener(this);
	ClientManager::getInstance()->addListener(this);
	
	File::ensureDirectory(Util::getHubListsPath());
}

FavoriteManager::~FavoriteManager()
{
	ClientManager::getInstance()->removeListener(this);
	SettingsManager::getInstance()->removeListener(this);
	shutdown();
	
	for_each(favoriteHubs.begin(), favoriteHubs.end(), DeleteFunction());
	for_each(recentHubs.begin(), recentHubs.end(), DeleteFunction());
	for_each(previewApplications.begin(), previewApplications.end(), DeleteFunction());
}

UserCommand FavoriteManager::addUserCommand(int type, int ctx, Flags::MaskType flags, const string& name, const string& command, const string& to, const string& hub)
{
	UserCommand uc(m_lastId++, type, ctx, flags, name, command, to, hub);
	{
		// No dupes, add it...
		FastUniqueLock l(csUserCommand);
		userCommands.push_back(uc);
	}
	if (!uc.isSet(UserCommand::FLAG_NOSAVE))
		save();
		
	return uc;
}

bool FavoriteManager::getUserCommand(int cid, UserCommand& uc) const
{
	FastSharedLock l(csUserCommand);
	for (auto i = userCommands.cbegin(); i != userCommands.cend(); ++i)
	{
		if (i->getId() == cid)
		{
			uc = *i;
			return true;
		}
	}
	return false;
}

bool FavoriteManager::moveUserCommand(int cid, int pos)
{
	dcassert(pos == -1 || pos == 1);
	FastUniqueLock l(csUserCommand);
	for (auto i = userCommands.begin(); i != userCommands.end(); ++i)
	{
		if (i->getId() == cid)
		{
			std::swap(*i, *(i + pos));
			return true;
		}
	}
	return false;
}

void FavoriteManager::updateUserCommand(const UserCommand& uc)
{
	{
		FastUniqueLock l(csUserCommand);
		for (auto i = userCommands.begin(); i != userCommands.end(); ++i)
		{
			if (i->getId() == uc.getId())
			{
				*i = uc;
				if (uc.isSet(UserCommand::FLAG_NOSAVE))
					return;
				else
					break;
			}
		}
	}
	save();
}

int FavoriteManager::findUserCommand(const string& aName, const string& aUrl) const
{
	FastSharedLock l(csUserCommand);
	for (auto i = userCommands.cbegin(); i != userCommands.cend(); ++i)
	{
		if (i->getName() == aName && i->getHub() == aUrl)
		{
			return i->getId();
		}
	}
	return -1;
}

void FavoriteManager::removeUserCommand(int cid)
{
	{
		FastUniqueLock l(csUserCommand);
		for (auto i = userCommands.cbegin(); i != userCommands.cend(); ++i)
		{
			if (i->getId() == cid)
			{
				const bool l_nosave = i->isSet(UserCommand::FLAG_NOSAVE);
				userCommands.erase(i);
				if (l_nosave)
					return;
				else
					break;
			}
		}
	}
	save();
}
void FavoriteManager::removeUserCommand(const string& srv)
{
	FastUniqueLock l(csUserCommand);
	for (auto i = userCommands.cbegin(); i != userCommands.cend();)
	{
		if (i->isSet(UserCommand::FLAG_NOSAVE) && i->getHub() == srv) // [!] IRainman opt: reordering conditions.
		{
			i = userCommands.erase(i);
		}
		else
		{
			++i;
		}
	}
}

void FavoriteManager::removeHubUserCommands(int ctx, const string& hub)
{
	FastUniqueLock l(csUserCommand);
	for (auto i = userCommands.cbegin(); i != userCommands.cend();)
	{
		if (i->isSet(UserCommand::FLAG_NOSAVE) && (i->getCtx() & ctx) != 0 && i->getHub() == hub) // [!] IRainman opt: reordering conditions.
		{
			i = userCommands.erase(i);
		}
		else
		{
			++i;
		}
	}
}

// [+] SSA addUser (Unified)
bool FavoriteManager::addUserL(const UserPtr& aUser, FavoriteMap::iterator& iUser, bool create /*= true*/)
{
	dcassert(!ClientManager::isShutdown());
	// [!] always use external lock for this function.
	iUser = m_users.find(aUser->getCID());
	if (iUser == m_users.end() && create)
	{
		StringList hubs = ClientManager::getInstance()->getHubs(aUser->getCID(), Util::emptyString);
		StringList nicks = ClientManager::getInstance()->getNicks(aUser->getCID(), Util::emptyString);
		
		/// @todo make this an error probably...
		if (hubs.empty())
			hubs.push_back(Util::emptyString);
		if (nicks.empty())
			nicks.push_back(Util::emptyString);
			
		iUser = m_users.insert(make_pair(aUser->getCID(), FavoriteUser(aUser, nicks[0], hubs[0]))).first;
		updateEmptyState();
		return true;
	}
	return false;
}

bool FavoriteManager::getUploadLimit(const UserPtr& aUser, int& p_uploadLimit) const // [+] FlylinkDC
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastSharedLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		
		const auto l_user = m_users.find(aUser->getCID());
		if (l_user != m_users.end())
		{
			p_uploadLimit = getUploadLimitL(l_user);
			return true;
		}
	}
	return false;
}

bool FavoriteManager::getFavUserParam(const UserPtr& aUser, FavoriteUser::MaskType& p_flags, int& p_uploadLimit) const // [+] IRainman opt.
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastSharedLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		const auto l_user = m_users.find(aUser->getCID());
		if (l_user != m_users.end())
		{
			p_flags = l_user->second.getFlags();
			p_uploadLimit = getUploadLimitL(l_user);
			return true;
		}
	}
	return false;
}

bool FavoriteManager::isNoFavUserOrUserIgnorePrivate(const UserPtr& aUser) const // [+] IRainman opt.
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastSharedLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		const auto l_user = m_users.find(aUser->getCID());
		return l_user == m_users.end() || l_user->second.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE);
	}
	return true;
}

bool FavoriteManager::isNoFavUserOrUserBanUpload(const UserPtr& aUser) const // [+] IRainman opt.
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastSharedLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		const auto l_user = m_users.find(aUser->getCID());
		return l_user == m_users.end() || getUploadLimitL(l_user) == FavoriteUser::UL_BAN;
	}
	return true;
}

bool FavoriteManager::getFavoriteUser(const UserPtr& p_user, FavoriteUser& p_favuser) const // [+] IRainman opt.
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastSharedLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		const auto l_user = m_users.find(p_user->getCID());
		if (l_user != m_users.end())
		{
			p_favuser = l_user->second;
			return true;
		}
	}
	return false;
}

bool FavoriteManager::isFavoriteUser(const UserPtr& aUser) const
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastSharedLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		return m_users.find(aUser->getCID()) != m_users.end();
	}
	return false;
}
#ifndef IRAINMAN_NON_COPYABLE_FAV_USERS
void FavoriteManager::getFavoriteUsersNames(StringSet& p_users) const // TODO оптимизировать упаковку в уникальные ники отложенно в момент измени€ базовой мапы users.
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
		Lock l(csUsers);
		for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
		{
			dcassert(!i->second.getNick().empty());
			if (!i->second.getNick().empty()) // Ќики могут дублировать и могут быть пустыми. https://www.box.com/shared/p4p0scdeh92wpxf9bohi
			{
				p_users.insert(i->second.getNick());
			}
		}
	}
}
#endif
void FavoriteManager::addFavoriteUser(const UserPtr& aUser)
{
	// [!] SSA see _addUserIfnotExist function
	{
		FavoriteMap::iterator i;
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastUniqueLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		if (!addUserL(aUser, i))
			return;
			
		fire(FavoriteManagerListener::UserAdded(), i->second);
	}
	save();
}

void FavoriteManager::removeFavoriteUser(const UserPtr& aUser)
{
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastUniqueLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		const FavoriteMap::const_iterator i = m_users.find(aUser->getCID());
		if (i == m_users.end())
			return;
			
		fire(FavoriteManagerListener::UserRemoved(), i->second);
		m_users.erase(i);
		updateEmptyState();
	}
	save();
}

string FavoriteManager::getUserUrl(const UserPtr& aUser) const
{
	if (isNotEmpty()) // [+]PPA
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastSharedLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		FavoriteMap::const_iterator i = m_users.find(aUser->getCID());
		if (i != m_users.end())
		{
			const FavoriteUser& fu = i->second;
			return fu.getUrl();
		}
	}
	return Util::emptyString;
}

FavoriteHubEntry* FavoriteManager::addFavorite(const FavoriteHubEntry& aEntry, const AutoStartType p_autostart/* = NOT_CHANGE*/) // [!] IRainman fav options
{
	if (FavoriteHubEntry* l_fhe = getFavoriteHubEntry(aEntry.getServer()))
	{
		// [+] IRainman fav options
		if (p_autostart != NOT_CHANGE)
		{
			l_fhe->setConnect(p_autostart == ADD);
			fire(FavoriteManagerListener::FavoriteAdded(), (FavoriteHubEntry*)NULL); // rebuild fav hubs list
		}
		return l_fhe;
		// [~] IRainman
	}
	FavoriteHubEntry* f = new FavoriteHubEntry(aEntry);
	f->setConnect(p_autostart == ADD);// [+] IRainman fav options
	{
		FastUniqueLock l(csHubs); // [+] IRainman fix.
		favoriteHubs.push_back(f);
	}
	fire(FavoriteManagerListener::FavoriteAdded(), f);
	save();
	return f;
}

void FavoriteManager::removeFavorite(const FavoriteHubEntry* entry)
{
	{
		FastUniqueLock l(csHubs); // [+] IRainman fix.
		FavoriteHubEntryList::iterator i = find(favoriteHubs.begin(), favoriteHubs.end(), entry);
		if (i == favoriteHubs.end())
			return;
			
		favoriteHubs.erase(i);
	}
	fire(FavoriteManagerListener::FavoriteRemoved(), entry);
	delete entry;
	save();
}

bool FavoriteManager::addFavoriteDir(string aDirectory, const string& aName, const string& aExt)
{
	AppendPathSeparator(aDirectory); //[+]PPA
	
	{
		FastUniqueLock l(csDirs);
		for (auto i = favoriteDirs.cbegin(); i != favoriteDirs.cend(); ++i)
		{
			if ((strnicmp(aDirectory, i->dir, i->dir.length()) == 0) && (strnicmp(aDirectory, i->dir, aDirectory.length()) == 0))
			{
				return false;
			}
			if (stricmp(aName, i->name) == 0)
			{
				return false;
			}
			if (!aExt.empty() && stricmp(aExt, Util::toSettingString(i->ext)) == 0) // [!] IRainman opt.
			{
				return false;
			}
		}
		FavoriteDirectory favDir = { Util::splitSettingAndReplaceSpace(aExt), aDirectory, aName };
		favoriteDirs.push_back(favDir);
	}
	save();
	return true;
}

bool FavoriteManager::removeFavoriteDir(const string& aName)
{
	string d(aName);
	
	AppendPathSeparator(d); //[+]PPA
	
	bool upd = false;
	{
		FastUniqueLock l(csDirs);
		for (auto j = favoriteDirs.cbegin(); j != favoriteDirs.cend(); ++j)
		{
			if (stricmp(j->dir.c_str(), d.c_str()) == 0)
			{
				favoriteDirs.erase(j);
				upd = true;
				break;
			}
		}
	}
	if (upd)
	{
		save();
	}
	return upd;
}

bool FavoriteManager::renameFavoriteDir(const string& aName, const string& anotherName)
{
	bool upd = false;
	{
		FastUniqueLock l(csDirs);
		for (auto j = favoriteDirs.begin(); j != favoriteDirs.end(); ++j)
		{
			if (stricmp(j->name.c_str(), aName.c_str()) == 0)
			{
				j->name = anotherName;
				upd = true;
				break;
			}
		}
	}
	if (upd)
	{
		save();
	}
	return upd;
}

bool FavoriteManager::updateFavoriteDir(const string& aName, const string& dir, const string& ext) // [!] IRainman opt.
{
	bool upd = false;
	{
		FastUniqueLock l(csDirs);
		for (auto j = favoriteDirs.begin(); j != favoriteDirs.end(); ++j)
		{
			if (stricmp(j->name.c_str(), aName.c_str()) == 0)
			{
				j->dir = dir;
				j->ext = Util::splitSettingAndReplaceSpace(ext);
				j->name = aName;
				upd = true;
				break;
			}
		}
	}
	if (upd)
	{
		save();
	}
	return upd;
}

string FavoriteManager::getDownloadDirectory(const string& ext) const
{
	if (ext.size() > 1)
	{
		FastSharedLock l(csDirs);
		for (auto i = favoriteDirs.cbegin(); i != favoriteDirs.cend(); ++i)
		{
			for (auto j = i->ext.cbegin(); j != i->ext.cend(); ++j) // [!] IRainman opt.
			{
				if (stricmp(ext.substr(1).c_str(), j->c_str()) == 0)
					return i->dir;
			}
		}
	}
	return SETTING(DOWNLOAD_DIRECTORY);
}

void FavoriteManager::addRecent(const RecentHubEntry& aEntry)
{
	RecentHubEntry::Iter i = getRecentHub(aEntry.getServer());
	if (i != recentHubs.end())
	{
		return;
	}
	RecentHubEntry* f = new RecentHubEntry(aEntry);
	recentHubs.push_back(f);
	fire(FavoriteManagerListener::RecentAdded(), f);
	recentsave();
}

void FavoriteManager::removeRecent(const RecentHubEntry* entry)
{
	RecentHubEntry::List::iterator i = find(recentHubs.begin(), recentHubs.end(), entry);
	if (i == recentHubs.end())
	{
		return;
	}
	
	fire(FavoriteManagerListener::RecentRemoved(), entry);
	recentHubs.erase(i);
	delete entry;
	recentsave();
}

void FavoriteManager::updateRecent(const RecentHubEntry* entry)
{
	RecentHubEntry::Iter i = find(recentHubs.begin(), recentHubs.end(), entry);
	if (i == recentHubs.end())
	{
		return;
	}
	
	fire(FavoriteManagerListener::RecentUpdated(), entry);
	recentsave();
}

#ifdef IRAINMAN_ENABLE_HUB_LIST
class XmlListLoader : public SimpleXMLReader::CallBack
{
	public:
		XmlListLoader(HubEntryList& lst) : publicHubs(lst) { }
		~XmlListLoader() { }
		void startTag(const string& p_name, StringPairList& p_attribs, bool)
		{
			if (p_name == "Hub")
			{
				const string& l_name = getAttrib(p_attribs, "Name", 0); // Local declaration of 'name' hides declaration of the same name in outer scope.
				const string& server = getAttrib(p_attribs, "Address", 1);
				const string& description = getAttrib(p_attribs, "Description", 2);
				const string& users = getAttrib(p_attribs, "Users", 3);
				const string& country = getAttrib(p_attribs, "Country", 4); //-V112
				const string& shared = getAttrib(p_attribs, "Shared", 5);
				const string& minShare = getAttrib(p_attribs, "Minshare", 5);
				const string& minSlots = getAttrib(p_attribs, "Minslots", 5);
				const string& maxHubs = getAttrib(p_attribs, "Maxhubs", 5);
				const string& maxUsers = getAttrib(p_attribs, "Maxusers", 5);
				const string& reliability = getAttrib(p_attribs, "Reliability", 5);
				const string& rating = getAttrib(p_attribs, "Rating", 5);
				publicHubs.push_back(HubEntry(l_name, server, description, users, country, shared, minShare, minSlots, maxHubs, maxUsers, reliability, rating));
			}
		}
		void endTag(const string&, const string&)
		{
		
		}
	private:
		HubEntryList& publicHubs;
};

bool FavoriteManager::onHttpFinished(bool fromHttp) noexcept
{
	MemoryInputStream mis(m_downloadBuf);
	bool success = true;
	
	FastUniqueLock l(csPublicHubs);
	HubEntryList& list = publicListMatrix[publicListServer];
	list.clear();
	
	try
	{
		XmlListLoader loader(list);
		
		if ((m_listType == TYPE_BZIP2) && (!m_downloadBuf.empty()))
		{
			FilteredInputStream<UnBZFilter, false> f(&mis);
			SimpleXMLReader(&loader).parse(f);
		}
		else
		{
			SimpleXMLReader(&loader).parse(mis);
		}
	}
	catch (const Exception&)
	{
		success = false;
		fire(FavoriteManagerListener::Corrupted(), fromHttp ? publicListServer : Util::emptyString);
	}
	
	if (fromHttp)
	{
		try
		{
			File f(Util::getHubListsPath() + Util::validateFileName(publicListServer), File::WRITE, File::CREATE | File::TRUNCATE);
			f.write(m_downloadBuf);
			f.close();
		}
		catch (const FileException&) { } //-V565
	}
	
	m_downloadBuf.clear();
	
	return success;
}
#endif // IRAINMAN_ENABLE_HUB_LIST

void FavoriteManager::save() const
{
	if (m_dontSave)
		return;
		
	// Lock l(cs); [-] IRainman opt.
	try
	{
		SimpleXML xml;
		
		xml.addTag("Favorites");
		xml.stepIn();
		
		//xml.addChildAttrib("ConfigVersion", string(A_REVISION_NUM_STR));// [+] IRainman fav options
		
		xml.addTag("Hubs");
		xml.stepIn();
		
		{
			FastSharedLock l(csHubs); // [+] IRainman fix.
			for (auto i = favHubGroups.cbegin(), iend = favHubGroups.cend(); i != iend; ++i)
			{
				xml.addTag("Group");
				xml.addChildAttrib("Name", i->first);
				xml.addChildAttrib("Private", i->second.priv);
			}
			for (auto i = favoriteHubs.cbegin(), iend = favoriteHubs.cend(); i != iend; ++i)
			{
				xml.addTag("Hub");
				xml.addChildAttrib("Name", (*i)->getName());
				xml.addChildAttrib("Connect", (*i)->getConnect());
				xml.addChildAttrib("Description", (*i)->getDescription());
				xml.addChildAttrib("Nick", (*i)->getNick(false));
				xml.addChildAttrib("Password", (*i)->getPassword());
				xml.addChildAttrib("Server", (*i)->getServer());
				xml.addChildAttrib("UserDescription", (*i)->getUserDescription());
				if (!Util::isAdcHub((*i)->getServer())) // [+] IRainman fix.
				{
					xml.addChildAttrib("Encoding", (*i)->getEncoding());
				}
				xml.addChildAttrib("AwayMsg", (*i)->getAwayMsg());
				xml.addChildAttrib("Email", (*i)->getEmail());
				xml.addChildAttrib("WindowPosX", (*i)->getWindowPosX());
				xml.addChildAttrib("WindowPosY", (*i)->getWindowPosY());
				xml.addChildAttrib("WindowSizeX", (*i)->getWindowSizeX());
				xml.addChildAttrib("WindowSizeY", (*i)->getWindowSizeY());
				xml.addChildAttrib("WindowType", (*i)->getWindowType());
				xml.addChildAttrib("ChatUserSplitSize", (*i)->getChatUserSplit());
				xml.addChildAttrib("ChatUserSplitState", (*i)->getChatUserSplitState());
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
				xml.addChildAttrib("StealthMode", (*i)->getStealth());
#endif
				xml.addChildAttrib("HideShare", (*i)->getHideShare()); // Save paramethers always IRAINMAN_INCLUDE_HIDE_SHARE_MOD
				xml.addChildAttrib("ShowJoins", (*i)->getShowJoins()); // Show joins
				xml.addChildAttrib("ExclChecks", (*i)->getExclChecks()); // Excl. from client checking
				xml.addChildAttrib("ExclusiveHub", (*i)->getExclusiveHub()); // Exclusive Hub
				xml.addChildAttrib("UserListState", (*i)->getUserListState());
				xml.addChildAttrib("HeaderOrder", (*i)->getHeaderOrder());
				xml.addChildAttrib("HeaderWidths", (*i)->getHeaderWidths());
				xml.addChildAttrib("HeaderVisible", (*i)->getHeaderVisible());
				xml.addChildAttrib("HeaderSort", (*i)->getHeaderSort());
				xml.addChildAttrib("HeaderSortAsc", (*i)->getHeaderSortAsc());
				xml.addChildAttrib("RawOne", (*i)->getRawOne());
				xml.addChildAttrib("RawTwo", (*i)->getRawTwo());
				xml.addChildAttrib("RawThree", (*i)->getRawThree());
				xml.addChildAttrib("RawFour", (*i)->getRawFour());
				xml.addChildAttrib("RawFive", (*i)->getRawFive());
				xml.addChildAttrib("Mode", Util::toString((*i)->getMode()));
				xml.addChildAttrib("IP", (*i)->getIP());
				xml.addChildAttrib("OpChat", (*i)->getOpChat());
				xml.addChildAttrib("SearchInterval", Util::toString((*i)->getSearchInterval()));
				// [!] IRainman mimicry function
				// [-] xml.addChildAttrib("CliendId", (*i)->getClientId()); // !SMT!-S
				xml.addChildAttrib("ClientName", (*i)->getClientName());
				xml.addChildAttrib("ClientVersion", (*i)->getClientVersion());
				xml.addChildAttrib("OverrideId", Util::toString((*i)->getOverrideId())); // !SMT!-S
				// [~] IRainman mimicry function
				xml.addChildAttrib("Group", (*i)->getGroup());
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
				xml.addChildAttrib("Status", (*i)->getConnectionStatus().getStatus());
				xml.addChildAttrib("LastAttempts", (*i)->getConnectionStatus().getLastAttempts());
				xml.addChildAttrib("LastSucces", (*i)->getConnectionStatus().getLastSucces());
#endif
			}
		}
		xml.stepOut();
		xml.addTag("Users");
		xml.stepIn();
		{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
			FastSharedLock l(csUsers); // [+] IRainman opt.
#else
			Lock l(csUsers);
#endif
			for (auto i = m_users.cbegin(), iend = m_users.cend(); i != iend; ++i)
			{
				const auto &u = i->second; // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'i->second' expression repeatedly. favoritemanager.cpp 687
				xml.addTag("User");
				
				xml.addChildAttrib("LastSeen", u.getLastSeen());
				
				xml.addChildAttrib("GrantSlot", u.isSet(FavoriteUser::FLAG_GRANT_SLOT));
				
				xml.addChildAttrib("SuperUser", u.getUploadLimit() == FavoriteUser::UL_SU); // [!] FlylinkDC++ compatibility mode.
				xml.addChildAttrib("UploadLimit", u.getUploadLimit()); // !SMT!-S
				
				xml.addChildAttrib("IgnorePrivate", u.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE)); // !SMT!-S
				xml.addChildAttrib("FreeAccessPM", u.isSet(FavoriteUser::FLAG_FREE_PM_ACCESS)); // !SMT!-PSW
				
				xml.addChildAttrib("UserDescription", u.getDescription());
				xml.addChildAttrib("Nick", u.getNick());
				xml.addChildAttrib("URL", u.getUrl());
				if (Util::isAdcHub(u.getUrl())) // [+] IRainman fix.
					xml.addChildAttrib("CID", i->first.toBase32());
			}
		}
		xml.stepOut();
		
		xml.addTag("UserCommands");
		xml.stepIn();
		{
			FastSharedLock l(csUserCommand); // [+] IRainman opt.
			for (auto i = userCommands.cbegin(), iend = userCommands.cend(); i != iend; ++i)
			{
				if (!i->isSet(UserCommand::FLAG_NOSAVE))
				{
					xml.addTag("UserCommand");
					xml.addChildAttrib("Type", i->getType());
					xml.addChildAttrib("Context", i->getCtx());
					xml.addChildAttrib("Name", i->getName());
					xml.addChildAttrib("Command", i->getCommand());
					xml.addChildAttrib("Hub", i->getHub());
				}
			}
		}
		xml.stepOut();
		
		//Favorite download to dirs
		xml.addTag("FavoriteDirs");
		xml.stepIn();
		{
			FastSharedLock l(csDirs);
			for (auto i = favoriteDirs.cbegin(), iend = favoriteDirs.cend(); i != iend; ++i)
			{
				xml.addTag("Directory", i->dir);
				xml.addChildAttrib("Name", i->name);
				xml.addChildAttrib("Extensions", Util::toSettingString(i->ext));
			}
		}
		xml.stepOut();
		
		xml.stepOut();
		
		string fname = getConfigFile();
		
		File f(fname + ".tmp", File::WRITE, File::CREATE | File::TRUNCATE);
		f.write(SimpleXML::utf8Header);
		f.write(xml.toXML());
		f.close();
		File::deleteFile(fname);
		File::renameFile(fname + ".tmp", fname);
		
	}
	catch (const Exception& e)
	{
		dcdebug("FavoriteManager::save: %s\n", e.getError().c_str());
	}
}

void FavoriteManager::recentsave() const
{
	CFlyRegistryMap l_values;
	for (auto i = recentHubs.cbegin(); i != recentHubs.cend(); ++i)
	{
		string l_recentHubs_token;
		l_recentHubs_token += (*i)->getDescription();
		l_recentHubs_token += '\n';
		l_recentHubs_token += (*i)->getUsers();
		l_recentHubs_token += '\n';
		l_recentHubs_token += (*i)->getShared();
		l_recentHubs_token += '\n';
		l_recentHubs_token += (*i)->getServer();
		l_recentHubs_token += '\n';
		l_recentHubs_token += (*i)->getDateTime();
		l_values[(*i)->getName()] = l_recentHubs_token;
	}
	CFlylinkDBManager::getInstance()->save_registry(l_values, e_RecentHub);
}
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
bool FavoriteManager::load_from_url()
{

	if (BOOLSETTING(PROVIDER_USE_RESOURCES) && BOOLSETTING(PROVIDER_USE_HUBLIST))
	{
		string l_url = SETTING(ISP_RESOURCE_ROOT_URL);
		try
		{
			if (l_url.empty())
				return false;
			AppendUriSeparator(l_url);
			l_url += "ISP_favorites.xml";
			string l_data;
			const string l_log_message = "Download: " + l_url;
			const size_t l_size = Util::getDataFromInet(_T("FavoriteManager::load_from_url"), 4096, l_url, l_data);
			if (l_size == 0)
			{
				LogManager::getInstance()->message("[ISPFavorite] " + l_log_message + " [Error]");
			}
			else
			{
				LogManager::getInstance()->message("[ISPFavorite] " + l_log_message + " [Ok]");
				SimpleXML xml;
				xml.fromXML(l_data);
				if (xml.findChild("Favorites"))
				{
					xml.stepIn();
					load(xml, true);
					xml.stepOut();
					return true; //[ok]
				}
			}
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message("[ISPFavorite][Error] " + e.getError() + l_url);
			dcdebug("FavoriteManager::load_from_url: %s\n", e.getError().c_str());
		}
	}
	return false;
}
#endif // IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
void FavoriteManager::load()
{

	// Add NMDC standard op commands
	static const char kickstr[] =
	    "$To: %[userNI] From: %[myNI] $<%[myNI]> You are being kicked because: %[kickline:Reason]|<%[myNI]> is kicking %[userNI] because: %[kickline:Reason]|$Kick %[userNI]|";
	addUserCommand(UserCommand::TYPE_RAW_ONCE, UserCommand::CONTEXT_USER | UserCommand::CONTEXT_SEARCH, UserCommand::FLAG_NOSAVE,
	               STRING(KICK_USER), kickstr, "", "op");
	static const char kickfilestr[] =
	    "$To: %[userNI] From: %[myNI] $<%[myNI]> You are being kicked because: %[kickline:Reason] %[fileFN]|<%[myNI]> is kicking %[userNI] because: %[kickline:Reason] %[fileFN]|$Kick %[userNI]|";
	addUserCommand(UserCommand::TYPE_RAW_ONCE, UserCommand::CONTEXT_SEARCH, UserCommand::FLAG_NOSAVE,
	               STRING(KICK_USER_FILE), kickfilestr, "", "op");
	static const char redirstr[] =
	    "$OpForceMove $Who:%[userNI]$Where:%[line:Target Server]$Msg:%[line:Message]|";
	addUserCommand(UserCommand::TYPE_RAW_ONCE, UserCommand::CONTEXT_USER | UserCommand::CONTEXT_SEARCH, UserCommand::FLAG_NOSAVE,
	               STRING(REDIRECT_USER), redirstr, "", "op");
	               
	try
	{
		SimpleXML xml;
		Util::migrate(getConfigFile());
		xml.fromXML(File(getConfigFile(), File::READ, File::OPEN).read());
		
		if (xml.findChild("Favorites"))
		{
			xml.stepIn();
			load(xml);
			xml.stepOut();
		}
	}
	catch (const Exception& e)
	{
		LogManager::getInstance()->message("[Error] FavoriteManager::load " + e.getError());
		dcdebug("FavoriteManager::load: %s\n", e.getError().c_str());
	}
	
	// [+] RedMaster add FlylinkDC supports hub
	if (BOOLSETTING(CONNECT_TO_SUPPORT_HUB)) // [+] SSA
	{
		if (!m_SupportsHubExist)
		{
			m_SupportsHubExist = true;
			FavoriteHubEntry* e = new FavoriteHubEntry();
			e->setName(STRING(SUPPORTS_SERVER_DESC));
			e->setConnect(true);
			e->setDescription(STRING(SUPPORTS_SERVER_DESC));
			e->setServer(getSupportHubURL());
			FastUniqueLock l(csHubs);
			favoriteHubs.push_back(e);
			// [-] IRainman fix m_dontSave = false;
		}
	}
	// [~] RedMaster
	
	const bool oldConfigExist = !recentHubs.empty(); // [+] IRainman fix: FlylinkDC stores recents hubs in the sqlite database, so you need to keep the values in the database after loading the file.
	
	CFlyRegistryMap l_values;
	CFlylinkDBManager::getInstance()->load_registry(l_values, e_RecentHub);
	for (auto k = l_values.cbegin(); k != l_values.cend(); ++k)
	{
		const StringTokenizer<string> tok(k->second.m_val_str, '\n');
		RecentHubEntry* e = new RecentHubEntry();
		e->setName(k->first);
		if (tok.getTokens().size() > 3)
		{
			e->setDescription(tok.getTokens()[0]);
			e->setUsers(tok.getTokens()[1]);
			e->setShared(tok.getTokens()[2]);
			e->setServer(Util::formatDchubUrl(tok.getTokens()[3]));
			if (tok.getTokens().size() > 4) //-V112
			{
				e->setDateTime(tok.getTokens()[4]);
			}
		}
		recentHubs.push_back(e);
	}
	// [+] IRainman fix: FlylinkDC stores recents hubs in the sqlite database, so you need to keep the values in the database after loading the file.
	if (oldConfigExist)
	{
		recentsave();
	}
	// [~] IRainman fix.
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
	if (load_from_url() && !m_sync_hub_external.empty())
	{
		for (auto m = m_sync_hub_external.cbegin(); m != m_sync_hub_external.cend() && !m_sync_hub_local.empty(); ++m)
			m_sync_hub_local.erase(*m);
			
		m_sync_hub_external.clear();
		
		for (auto k = m_sync_hub_local.cbegin(); k != m_sync_hub_local.cend(); ++k)
		{
			FavoriteHubEntry* l_OldHubEntry = getFavoriteHubEntry(*k);
			l_OldHubEntry->setGroup("ISP Recycled");
			l_OldHubEntry->setConnect(false);
			LogManager::getInstance()->message("[ISPFavorite][Connect]: " + *k + "ISP Recycled");
		}
	}
#endif // IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
}

void FavoriteManager::load(SimpleXML& aXml
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
                           , bool p_is_url /* = false*/
#endif
                          )
{
	m_dontSave = true;
	bool needSave = false;
	
	//const int l_configVersion = Util::toInt(aXml.getChildAttrib("ConfigVersion"));// [+] IRainman fav options
	aXml.resetCurrentChild();
	if (aXml.findChild("Hubs"))
	{
		aXml.stepIn();
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
		if (!p_is_url)
#endif
		{
			FastUniqueLock l(csHubs); // [+] IRainman fix.
			while (aXml.findChild("Group"))
			{
				const string& name = aXml.getChildAttrib("Name");
				if (name.empty())
					continue;
				const FavHubGroupProperties props = { aXml.getBoolChildAttrib("Private") };
				favHubGroups[name] = props;
			}
		}
		aXml.resetCurrentChild();
		while (aXml.findChild("Hub"))
		{
			// [+] RedMaster add FlylinkDC supports hub
			const string& l_CurrentServerUrl = Util::formatDchubUrl(aXml.getChildAttrib("Server"));
			if (l_CurrentServerUrl == getSupportHubURL())
				m_SupportsHubExist = true;
			else if (l_CurrentServerUrl == "adc://adchub.com:1687") // TODO - black list for spammers hub?
				continue; // [!] IRainman fix - delete SEO hub.
				
			FavoriteHubEntry* e = new FavoriteHubEntry();
			const string& l_Name = aXml.getChildAttrib("Name");
			e->setName(l_Name);
			const bool l_connect = aXml.getBoolChildAttrib("Connect");
			e->setConnect(l_connect);
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
			const bool l_ISPDelete  = aXml.getBoolChildAttrib("ISPDelete");
			const string& l_ISPMode = aXml.getChildAttrib("ISPMode");
			if (!l_ISPMode.empty())
				e->setMode(Util::toInt(aXml.getChildAttrib(l_ISPMode)));
#endif
			const string& l_Description = aXml.getChildAttrib("Description");
			const string& l_Group = aXml.getChildAttrib("Group");
			e->setDescription(l_Description);
			e->setServer(l_CurrentServerUrl);
			e->setSearchInterval(Util::toUInt32(aXml.getChildAttrib("SearchInterval")));
			// [!] IRainman fix.
			if (Util::isAdcHub(l_CurrentServerUrl))
			{
				e->setEncoding(Text::g_utf8);
			}
			else
			{
				e->setEncoding(aXml.getChildAttrib("Encoding"));
			}
			// [~] IRainman fix.
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
			if (!p_is_url)
			{
				if (l_Group == "ISP")
					m_sync_hub_local.insert(l_CurrentServerUrl);
#endif // IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
				e->setNick(aXml.getChildAttrib("Nick"));
				e->setPassword(aXml.getChildAttrib("Password"));
				e->setUserDescription(aXml.getChildAttrib("UserDescription"));
				e->setAwayMsg(aXml.getChildAttrib("AwayMsg"));
				e->setEmail(aXml.getChildAttrib("Email"));
				e->setWindowPosX(aXml.getIntChildAttrib("WindowPosX"));
				e->setWindowPosY(aXml.getIntChildAttrib("WindowPosY"));
				e->setWindowSizeX(aXml.getIntChildAttrib("WindowSizeX"));
				e->setWindowSizeY(aXml.getIntChildAttrib("WindowSizeY"));
				e->setWindowType(aXml.getIntChildAttrib("WindowType", "3")); // ≈сли ке€ нет - SW_MAXIMIZE
				e->setChatUserSplit(aXml.getIntChildAttrib("ChatUserSplitSize"));
				e->setChatUserSplitState(aXml.getBoolChildAttrib("ChatUserSplitState"));
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
				e->setStealth(aXml.getBoolChildAttrib("StealthMode"));
#endif
				e->setHideShare(aXml.getBoolChildAttrib("HideShare")); // Hide Share Mod
				e->setShowJoins(aXml.getBoolChildAttrib("ShowJoins")); // Show joins
				e->setExclChecks(aXml.getBoolChildAttrib("ExclChecks")); // Excl. from client checking
				e->setExclusiveHub(aXml.getBoolChildAttrib("ExclusiveHub")); // Exclusive Hub Mod
				e->setUserListState(aXml.getBoolChildAttrib("UserListState"));
				e->setHeaderOrder(aXml.getChildAttrib("HeaderOrder", SETTING(HUBFRAME_ORDER)));
				e->setHeaderWidths(aXml.getChildAttrib("HeaderWidths", SETTING(HUBFRAME_WIDTHS)));
				e->setHeaderVisible(aXml.getChildAttrib("HeaderVisible", SETTING(HUBFRAME_VISIBLE)));
				e->setHeaderSort(aXml.getIntChildAttrib("HeaderSort", "-1"));
				e->setHeaderSortAsc(aXml.getBoolChildAttrib("HeaderSortAsc"));
				e->setRawOne(aXml.getChildAttrib("RawOne"));
				e->setRawTwo(aXml.getChildAttrib("RawTwo"));
				e->setRawThree(aXml.getChildAttrib("RawThree"));
				e->setRawFour(aXml.getChildAttrib("RawFour"));
				e->setRawFive(aXml.getChildAttrib("RawFive"));
				e->setMode(Util::toInt(aXml.getChildAttrib("Mode")));
				e->setIP(aXml.getChildAttribTrim("IP"));
				e->setOpChat(aXml.getChildAttrib("OpChat"));
				// [+] IRainman mimicry function
				//if (l_configVersion <= xxxx)
				const string& l_ClientID = aXml.getChildAttrib("CliendId"); // !SMT!-S
				if (!l_ClientID.empty())
				{
					string l_ClientName, l_ClientVersion;
					splitClientId(l_ClientID, l_ClientName, l_ClientVersion);
					e->setClientName(l_ClientName);
					e->setClientVersion(l_ClientVersion);
				}
				else
				{
					e->setClientName(aXml.getChildAttrib("ClientName"));
					e->setClientVersion(aXml.getChildAttrib("ClientVersion"));
				}
				e->setOverrideId(Util::toInt(aXml.getChildAttrib("OverrideId")) != 0); // !SMT!-S
				// [~] IRainman mimicry function
				e->setGroup(l_Group);
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
				e->setSavedConnectionStatus(Util::toInt(aXml.getChildAttrib("Status")),
				                            Util::toInt64(aXml.getChildAttrib("LastAttempts")),
				                            Util::toInt64(aXml.getChildAttrib("LastSucces")));
#endif
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
			}
			else
			{
				if (m_sync_hub_external.empty() && favHubGroups.empty())
				{
					FavHubGroupProperties props = { aXml.getBoolChildAttrib("Public") };
					
					FastUniqueLock l(csHubs); // [+] IRainman fix.
					
					favHubGroups["ISP"] = props;
					favHubGroups["ISP Recycled"] = props;
				}
				m_sync_hub_external.insert(l_CurrentServerUrl);
			}
			FavoriteHubEntry* l_HubEntry = getFavoriteHubEntry(l_CurrentServerUrl);
			if (!l_HubEntry)
			{
				if (l_ISPDelete)
					delete e;
				else
				{
					{
						FastUniqueLock l(csHubs);
						favoriteHubs.push_back(e);
					}
					if (p_is_url)
					{
						e->setGroup("ISP");
						e->setConnect(true);
						needSave = true;
					}
				}
			}
			else
			{
				if (l_HubEntry->getGroup().empty() || l_HubEntry->getGroup() == "ISP")
				{
					l_HubEntry->setName(l_Name);
					l_HubEntry->setDescription(l_Description);
					l_HubEntry->setGroup("ISP");
				}
				if (l_ISPDelete)
				{
					needSave = true;
					FastUniqueLock l(csHubs);
					FavoriteHubEntryList::iterator i = find(favoriteHubs.begin(), favoriteHubs.end(), l_HubEntry);
					if (i != favoriteHubs.end())
					{
						favoriteHubs.erase(i);
						delete l_HubEntry;
						LogManager::getInstance()->message("[ISPDelete] FavoriteHubEntry server = " + l_CurrentServerUrl);
					}
				}
				delete e;
			}
#else
				{
					FastUniqueLock l(csHubs);
					favoriteHubs.push_back(e);
				}
#endif // IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
		}
		aXml.stepOut();
	}
	aXml.resetCurrentChild();
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
	if (!p_is_url)
#endif
	{
		if (aXml.findChild("Users"))
		{
			aXml.stepIn();
			while (aXml.findChild("User"))
			{
				UserPtr u;
				// [!] FlylinkDC
				const string& nick = aXml.getChildAttrib("Nick");
				
				const string hubUrl = Util::formatDchubUrl(aXml.getChildAttrib("URL")); // [!] IRainman fix: toLower already called in formatDchubUrl ( decodeUrl )
				
				const string cid = Util::isAdcHub(hubUrl) ? aXml.getChildAttrib("CID") : ClientManager::makeCid(nick, hubUrl).toBase32();
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
				const uint32_t l_hub_id = CFlylinkDBManager::getInstance()->get_dic_hub_id(hubUrl);
#endif
				// [~] FlylinkDC
				
				if (cid.length() != 39)
				{
					if (nick.empty() || hubUrl.empty())
						continue;
					u = ClientManager::getInstance()->getUser(nick, hubUrl
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
					                                          , l_hub_id
#endif
					                                         );
				}
				else
				{
					u = ClientManager::getInstance()->getUser(CID(cid));
				}
				
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
				FastUniqueLock l(csUsers);
#else
				Lock l(csUsers);
#endif
				FavoriteMap::iterator i = m_users.insert(make_pair(u->getCID(), FavoriteUser(u, nick, hubUrl))).first;
				
				if (aXml.getBoolChildAttrib("IgnorePrivate")) // !SMT!-S
					i->second.setFlag(FavoriteUser::FLAG_IGNORE_PRIVATE);
				if (aXml.getBoolChildAttrib("FreeAccessPM")) // !SMT!-PSW
					i->second.setFlag(FavoriteUser::FLAG_FREE_PM_ACCESS);
					
				i->second.setUploadLimit(aXml.getIntChildAttrib("UploadLimit"));
				if (aXml.getBoolChildAttrib("SuperUser")) // [!] FlylinkDC++ compatibility mode.
					i->second.setUploadLimit(FavoriteUser::UL_SU);
					
				if (aXml.getBoolChildAttrib("GrantSlot"))
					i->second.setFlag(FavoriteUser::FLAG_GRANT_SLOT);
					
				i->second.setLastSeen(aXml.getInt64ChildAttrib("LastSeen")); // [!] IRainman fix.
				
				i->second.setDescription(aXml.getChildAttrib("UserDescription"));
			}
			updateEmptyState();
			aXml.stepOut();
		}
		aXml.resetCurrentChild();
		if (aXml.findChild("UserCommands"))
		{
			aXml.stepIn();
			while (aXml.findChild("UserCommand"))
			{
				addUserCommand(aXml.getIntChildAttrib("Type"), aXml.getIntChildAttrib("Context"), 0, aXml.getChildAttrib("Name"),
				               aXml.getChildAttrib("Command"), aXml.getChildAttrib("To"), aXml.getChildAttrib("Hub"));
			}
			aXml.stepOut();
		}
		//Favorite download to dirs
		aXml.resetCurrentChild();
		if (aXml.findChild("FavoriteDirs"))
		{
			aXml.stepIn();
			while (aXml.findChild("Directory"))
			{
				const auto& virt = aXml.getChildAttrib("Name");
				const auto& ext = aXml.getChildAttrib("Extensions");
				const auto& d = aXml.getChildData();
				addFavoriteDir(d, virt, ext);
			}
			aXml.stepOut();
		}
	}
	m_dontSave = false;
	if (needSave)
		save();
}

void FavoriteManager::userUpdated(const OnlineUser& info)
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
		{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
			FastUniqueLock l(csUsers);
#else
			Lock l(csUsers);
#endif
			FavoriteMap::iterator i = m_users.find(info.getUser()->getCID());
			if (i == m_users.end())
				return;
				
			i->second.update(info);
		}
		save();
	}
}

FavoriteHubEntry* FavoriteManager::getFavoriteHubEntry(const string& aServer) const
{
	FastSharedLock l(csHubs); // [+] IRainman fix.
	for (auto i = favoriteHubs.cbegin(); i != favoriteHubs.cend(); ++i)
	{
		if ((*i)->getServer() == aServer)
			return (*i);
	}
	return nullptr;
}

FavoriteHubEntryList FavoriteManager::getFavoriteHubs(const string& group) const
{
	FavoriteHubEntryList ret;
	FastSharedLock l(csHubs);
	for (auto i = favoriteHubs.cbegin(), iend = favoriteHubs.cend(); i != iend; ++i)
	{
		if ((*i)->getGroup() == group)
			ret.push_back(*i);
	}
	return ret;
}

bool FavoriteManager::isPrivate(const string& p_url) const
{
//	dcassert(!p_url.empty());
	if (!p_url.empty())
	{
		FavoriteHubEntry* fav = getFavoriteHubEntry(p_url); // https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=23193
		if (fav)
		{
			const string& name = fav->getGroup();
			if (!name.empty())
			{
				FastSharedLock l(csHubs); // [+] IRainman fix.
				FavHubGroups::const_iterator group = favHubGroups.find(name);
				if (group != favHubGroups.end())
				{
					return group->second.priv;
				}
			}
		}
	}
	return false;
}

// !SMT!-S
void FavoriteManager::setUploadLimit(const UserPtr& aUser, int lim, bool createUser/* = true*/)
{
	ConnectionManager::getInstance()->setUploadLimit(aUser, lim);
	{
		FavoriteMap::iterator i;
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastUniqueLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		const bool added = addUserL(aUser, i, createUser);
		if (i == m_users.end())
			return;
			
		i->second.setUploadLimit(lim);
		speakUserUpdate(added, i);
	}
	save();
}
// !SMT!-S
bool FavoriteManager::getFlag(const UserPtr& aUser, FavoriteUser::Flags f) const
{
	dcassert(!ClientManager::isShutdown());
	if (isNotEmpty()) // [+]PPA
	{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastSharedLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		FavoriteMap::const_iterator i = m_users.find(aUser->getCID());
		if (i != m_users.end())
		{
			return i->second.isSet(f);
		}
	}
	return false;
}
// !SMT!-S
void FavoriteManager::setFlag(const UserPtr& aUser, FavoriteUser::Flags f, bool value, bool createUser /*= true*/)
{
	dcassert(!ClientManager::isShutdown());
	{
		FavoriteMap::iterator i;
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
		FastUniqueLock l(csUsers);
#else
		Lock l(csUsers);
#endif
		const bool added = addUserL(aUser, i, createUser);
		if (i == m_users.end())
			return;
			
		if (value)
			i->second.setFlag(f);
		else
			i->second.unsetFlag(f);
			
		speakUserUpdate(added, i);
	}
	save();
}

void FavoriteManager::setUserDescription(const UserPtr& aUser, const string& description)
{
	if (isNotEmpty()) // [+]PPA
	{
		{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
			FastUniqueLock l(csUsers);
#else
			Lock l(csUsers);
#endif
			FavoriteMap::iterator i = m_users.find(aUser->getCID());
			if (i == m_users.end())
				return;
				
			i->second.setDescription(description);
		}
		save();
	}
}

void FavoriteManager::recentload(SimpleXML& aXml)
{
	aXml.resetCurrentChild();
	if (aXml.findChild("Hubs"))
	{
		aXml.stepIn();
		while (aXml.findChild("Hub"))
		{
			RecentHubEntry* e = new RecentHubEntry();
			e->setName(aXml.getChildAttrib("Name"));
			e->setDescription(aXml.getChildAttrib("Description"));
			e->setUsers(aXml.getChildAttrib("Users"));
			e->setShared(aXml.getChildAttrib("Shared"));
			e->setServer(Util::formatDchubUrl(aXml.getChildAttrib("Server")));
			e->setDateTime(aXml.getChildAttrib("DateTime"));
			recentHubs.push_back(e);
		}
		aXml.stepOut();
	}
}

#ifdef IRAINMAN_ENABLE_HUB_LIST
RecentHubEntry::Iter FavoriteManager::getRecentHub(const string& aServer) const
{
	for (auto i = recentHubs.cbegin(); i != recentHubs.cend(); ++i)
	{
		if ((*i)->getServer() == aServer)
			return i;
	}
	return recentHubs.end();
}

void FavoriteManager::setHubList(int aHubList)
{
	m_lastServer = aHubList;
	refresh();
}

void FavoriteManager::refresh(bool forceDownload /* = false */)
{
	const StringList& sl = getHubLists();
	if (sl.empty())
		return;
		
	publicListServer = sl[static_cast<size_t>(m_lastServer) % sl.size()];
	if (!Util::isHttpLink(publicListServer))
	{
		m_lastServer++;
		return;
	}
	
	if (!forceDownload)
	{
		string path = Util::getHubListsPath() + Util::validateFileName(publicListServer);
		if (File::isExist(path))
		{
			m_useHttp = false;
			string fileDate;
			{
				FastUniqueLock l(csPublicHubs);
				publicListMatrix[publicListServer].clear();
			}
			m_listType = (stricmp(path.substr(path.size() - 4), ".bz2") == 0) ? TYPE_BZIP2 : TYPE_NORMAL; //-V112
			try
			{
				File cached(path, File::READ, File::OPEN);
				m_downloadBuf = cached.read();
				// [!] IRainman fix.
				fileDate = Util::formatDigitalClock(cached.getLastWriteTime());
				/* [-] IRainman fix.
				LocalArray<char, 20> l_buf;
				time_t fd = cached.getLastWriteTime();
				if (strftime(l_buf, l_buf.size(), "%x", localtime(&fd)))
				{
				    fileDate = string(l_buf);
				}
				[~] IRainman fix.*/
			}
			catch (const FileException&)
			{
				m_downloadBuf.clear();
			}
			if (!m_downloadBuf.empty())
			{
				if (onHttpFinished(false))
				{
					fire(FavoriteManagerListener::LoadedFromCache(), publicListServer, fileDate);
				}
				return;
			}
		}
	}
	
	if (!m_running)
	{
		m_running = true; // [+] IRainman fix.
		m_useHttp = true;
		{
			FastUniqueLock l(csPublicHubs);
			publicListMatrix[publicListServer].clear();
		}
		fire(FavoriteManagerListener::DownloadStarting(), publicListServer);
		if (c == NULL)
			c = new HttpConnection();
		c->addListener(this);
		c->downloadFile(publicListServer);
		//running = true; [-] IRainman fix.
	}
}
#endif // IRAINMAN_ENABLE_HUB_LIST
UserCommand::List FavoriteManager::getUserCommands(int ctx, const StringList& hubs/*[-] IRainman fix, bool& op*/) const
{
	vector<bool> isOp(hubs.size());
	
	for (size_t i = 0; i < hubs.size(); ++i)
	{
		isOp[i] = ClientManager::getInstance()->isOp(ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks(), hubs[i]);
	}
	
	UserCommand::List lst;
	{
		FastSharedLock l(csUserCommand);
		for (auto i = userCommands.cbegin(); i != userCommands.cend(); ++i)
		{
			const UserCommand& uc = *i;
			if (!(uc.getCtx() & ctx))
			{
				continue;
			}
			
			for (size_t j = 0; j < hubs.size(); ++j)
			{
				const string& hub = hubs[j];
				const bool hubAdc = Util::isAdcHub(hub);
				const bool commandAdc = Util::isAdcHub(uc.getHub());
				if (hubAdc && commandAdc)
				{
					if ((uc.getHub() == "adc://" || uc.getHub() == "adcs://") ||
					        ((uc.getHub() == "adc://op" || uc.getHub() == "adcs://op") && isOp[j]) ||
					        (uc.getHub() == hub))
					{
						lst.push_back(*i);
						break;
					}
				}
				else if ((!hubAdc && !commandAdc) || uc.isChat())
				{
					if ((uc.getHub().length() == 0) ||
					        (uc.getHub() == "op" && isOp[j]) ||
					        (uc.getHub() == hub))
					{
						lst.push_back(*i);
						break;
					}
				}
			}
		}
	}
	return lst;
}

#ifdef IRAINMAN_ENABLE_HUB_LIST
// HttpConnectionListener
void FavoriteManager::on(Data, HttpConnection*, const uint8_t* buf, size_t len) noexcept
{
	if (m_useHttp)
		m_downloadBuf.append((const char*)buf, len);
}

void FavoriteManager::on(Failed, HttpConnection*, const string& aLine) noexcept
{
	c->removeListener(this);
	m_lastServer++;
	m_running = false;
	if (m_useHttp)
	{
		m_downloadBuf.clear();
		fire(FavoriteManagerListener::DownloadFailed(), aLine);
	}
}

void FavoriteManager::on(Complete, HttpConnection*, const string& aLine
#ifdef RIP_USE_CORAL
                         , bool fromCoral
#endif
                        ) noexcept
{
	bool parseSuccess = false;
	c->removeListener(this);
	if (m_useHttp)
	{
		parseSuccess = onHttpFinished(true);
	}
	m_running = false;
	if (parseSuccess)
	{
		fire(FavoriteManagerListener::DownloadFinished(), aLine
#ifdef RIP_USE_CORAL
		, fromCoral
#endif
		    );
	}
}
void FavoriteManager::on(Redirected, HttpConnection*, const string& aLine) noexcept
{
	if (m_useHttp)
		fire(FavoriteManagerListener::DownloadStarting(), aLine);
}
void FavoriteManager::on(TypeNormal, HttpConnection*) noexcept
{
	if (m_useHttp)
		m_listType = TYPE_NORMAL;
}
void FavoriteManager::on(TypeBZ2, HttpConnection*) noexcept
{
	if (m_useHttp)
		m_listType = TYPE_BZIP2;
}
#ifdef RIP_USE_CORAL
void FavoriteManager::on(Retried, HttpConnection*, const bool Connected) noexcept
{
	if (Connected)
		downloadBuf.clear();
}
#endif

#endif // IRAINMAN_ENABLE_HUB_LIST

void FavoriteManager::on(UserUpdated, const OnlineUserPtr& user) noexcept
{
	userUpdated(*user);
}
void FavoriteManager::on(UserDisconnected, const UserPtr& user) noexcept
{
	dcassert(!ClientManager::isShutdown()); // TODO: it's normal situation https://code.google.com/p/flylinkdc/issues/detail?id=1317
	if (isNotEmpty()) // [+]PPA
	{
		{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
			FastUniqueLock l(csUsers);
#else
			Lock l(csUsers);
#endif
			FavoriteMap::iterator i = m_users.find(user->getCID());
			if (i == m_users.end())
				return;
				
			i->second.setLastSeen(GET_TIME()); // TODO: if ClientManager::isShutdown() this data is not update :( https://code.google.com/p/flylinkdc/issues/detail?id=1317
		}
		fire(FavoriteManagerListener::StatusChanged(), user);
		save();
	}
}

void FavoriteManager::on(UserConnected, const UserPtr& user) noexcept
{
	dcassert(!ClientManager::isShutdown());
	// [-] if (!ClientManager::isShutdown()) [-] IRainman: already in ClientManager.
	{
		if (isNotEmpty()) // [+]PPA
		{
			{
#ifdef IRAINMAN_USE_SHARED_SPIN_LOCK_FOR_USERS
				FastSharedLock l(csUsers);
#else
				Lock l(csUsers);
#endif
				FavoriteMap::const_iterator i = m_users.find(user->getCID());
				if (i == m_users.end())
					return;
			}
			fire(FavoriteManagerListener::StatusChanged(), user);
		}
	}
}

void FavoriteManager::previewload(SimpleXML& aXml)
{
	aXml.resetCurrentChild();
	if (aXml.findChild("PreviewApps"))
	{
		aXml.stepIn();
		while (aXml.findChild("Application"))
		{
			addPreviewApp(aXml.getChildAttrib("Name"), aXml.getChildAttrib("Application"),
			              aXml.getChildAttrib("Arguments"), aXml.getChildAttrib("Extension"));
		}
		aXml.stepOut();
	}
}

void FavoriteManager::previewsave(SimpleXML& aXml)
{
	aXml.addTag("PreviewApps");
	aXml.stepIn();
	for (auto i = previewApplications.cbegin(); i != previewApplications.cend(); ++i)
	{
		aXml.addTag("Application");
		aXml.addChildAttrib("Name", (*i)->getName());
		aXml.addChildAttrib("Application", (*i)->getApplication());
		aXml.addChildAttrib("Arguments", (*i)->getArguments());
		aXml.addChildAttrib("Extension", (*i)->getExtension());
	}
	aXml.stepOut();
}

/**
 * @file
 * $Id: FavoriteManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
