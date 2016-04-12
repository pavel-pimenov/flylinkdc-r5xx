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


#pragma once

#ifndef DCPLUSPLUS_DCPP_FAVORITE_MANAGER_H
#define DCPLUSPLUS_DCPP_FAVORITE_MANAGER_H

#include <boost/unordered/unordered_set.hpp>
#include <boost/unordered/unordered_map.hpp>
#include "SettingsManager.h"

#include "UserCommand.h"
#include "FavoriteUser.h"
#include "ClientManagerListener.h"
#include "FavoriteManagerListener.h"
#include "HubEntry.h"
#include "FavHubGroup.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"

class PreviewApplication
#ifdef _DEBUG
	: public boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		typedef vector<PreviewApplication*> List;
		
		PreviewApplication() noexcept {}
		PreviewApplication(const string& n, const string& a, const string& r, const string& e) : name(n), application(a), arguments(r), extension(Text::toLower(e)) // [!] IRainman fix: call toLower.
		{
		} // [!] IRainman opt add links.
		~PreviewApplication() noexcept { }
		
		GETSET(string, name, Name);
		GETSET(string, application, Application);
		GETSET(string, arguments, Arguments);
		GETSET(string, extension, Extension);
};

class SimpleXML;

/**
 * Public hub list, favorites (hub&user). Assumed to be called only by UI thread.
 */
class FavoriteManager : public Speaker<FavoriteManagerListener>,
	public Singleton<FavoriteManager>,
	private ClientManagerListener
{
	public:
		// [+] IRainman mimicry function
		struct mimicrytag
		{
			mimicrytag(char* p_tag, char* p_ver) : tag(p_tag), version(p_ver) { }
			const char* tag;
			const char* version;
		};
		
		static const mimicrytag g_MimicryTags[];
		static void splitClientId(const string& p_id, string& p_name, string& p_version);
		static string createClientId(const string& p_name, const string& p_version, const string& p_adress)
		{
			return createClientId(p_name, p_version, Util::isAdcHub(p_adress));
		}
		static string createClientId(const string& p_name, const string& p_version, const bool p_isAdc)
		{
			return p_name + (p_isAdc ? " " : " V:") + p_version;
		}
		// [~] IRainman mimicry function
		
// Favorite Users
		typedef boost::unordered_map<CID, FavoriteUser> FavoriteMap;
		class LockInstanceUsers
		{
			public:
				LockInstanceUsers()
				{
					FavoriteManager::g_csFavUsers->AcquireLockShared();
				}
				~LockInstanceUsers()
				{
					FavoriteManager::g_csFavUsers->ReleaseLockShared();
				}
				const FavoriteMap& getFavoriteUsersL() const
				{
					return FavoriteManager::g_fav_users_map;
				}
				const StringSet& getFavoriteNames() const
				{
					return FavoriteManager::g_fav_users;
				}
		};
		static const PreviewApplication::List& getPreviewApps()
		{
			return g_previewApplications;
		}
		
		void addFavoriteUser(const UserPtr& aUser);
		static bool isFavoriteUser(const UserPtr& aUser, bool& p_is_ban);
		static bool getFavoriteUser(const UserPtr& p_user, FavoriteUser& p_favuser); // [+] IRainman opt.
		static bool isNoFavUserOrUserBanUpload(const UserPtr& aUser); // [+] IRainman opt.
		static bool isNoFavUserOrUserIgnorePrivate(const UserPtr& aUser); // [+] IRainman opt.
		static bool getFavUserParam(const UserPtr& aUser, FavoriteUser::MaskType& p_flags, int& p_uploadLimit); // [+] IRainman opt.
		
		static bool hasAutoGrantSlot(FavoriteUser::MaskType p_flags) // [+] IRainman opt.
		{
			return (p_flags & FavoriteUser::FLAG_GRANT_SLOT) != 0;
		}
		
		static bool hasUploadBan(int limit) // [+] IRainman opt.
		{
			return limit == FavoriteUser::UL_BAN;
		}
		
		static bool hasIgnorePM(FavoriteUser::MaskType p_flags) // [+] IRainman opt.
		{
			return (p_flags & FavoriteUser::FLAG_IGNORE_PRIVATE) != 0;
		}
		static bool hasFreePM(FavoriteUser::MaskType p_flags) // [+] IRainman opt.
		{
			return (p_flags & FavoriteUser::FLAG_FREE_PM_ACCESS) != 0;
		}
		
		void removeFavoriteUser(const UserPtr& aUser);
		
		void setUserDescription(const UserPtr& aUser, const string& description);
		static bool hasAutoGrantSlot(const UserPtr& aUser)
		{
			return getFlag(aUser, FavoriteUser::FLAG_GRANT_SLOT);
		}
		void setAutoGrantSlot(const UserPtr& aUser, bool grant)
		{
			setFlag(aUser, FavoriteUser::FLAG_GRANT_SLOT, grant);
		}
		static void userUpdated(const OnlineUser& info);
		static string getUserUrl(const UserPtr& aUser);
		
		// !SMT!-S
		void setUploadLimit(const UserPtr& aUser, int lim, bool createUser = true);
		bool hasUploadBan(const UserPtr& aUser) const
		{
			int limit;
			FavoriteUser::MaskType l_flags;
			return getFavUserParam(aUser, l_flags, limit) ? limit == FavoriteUser::UL_BAN : false;
		}
		void setUploadBan(const UserPtr& aUser, bool ban) // [+] IRainman fix.
		{
			setUploadLimit(aUser, ban ? FavoriteUser::UL_BAN : FavoriteUser::UL_NONE);
		}
		void setUploadSuperUser(const UserPtr& aUser, bool superUser) // [+] IRainman fix.
		{
			setUploadLimit(aUser, superUser ? FavoriteUser::UL_SU : FavoriteUser::UL_NONE);
		}
		
		static bool getFlag(const UserPtr& aUser, FavoriteUser::Flags);
		void setFlag(const UserPtr& aUser, FavoriteUser::Flags, bool flag, bool createUser = true);
		
		bool hasIgnorePM(const UserPtr& aUser) const
		{
			return getFlag(aUser, FavoriteUser::FLAG_IGNORE_PRIVATE);
		}
		void setIgnorePM(const UserPtr& aUser, bool ignorePrivate)
		{
			setFlag(aUser, FavoriteUser::FLAG_IGNORE_PRIVATE, ignorePrivate);
		}
		bool hasFreePM(const UserPtr& aUser) const
		{
			return getFlag(aUser, FavoriteUser::FLAG_FREE_PM_ACCESS);
		}
		void setFreePM(const UserPtr& aUser, bool grant)
		{
			setFlag(aUser, FavoriteUser::FLAG_FREE_PM_ACCESS, grant);
		}
		void setNormalPM(const UserPtr& aUser) // [+] IRainman fix.
		{
			setFlag(aUser, FavoriteUser::Flags(FavoriteUser::FLAG_FREE_PM_ACCESS | FavoriteUser::FLAG_IGNORE_PRIVATE), false);
		}
		
// Favorite Hubs

		enum AutoStartType
		{
			REMOVE = -1,
			NOT_CHANGE = 0,
			ADD = 1,
		};
		
		FavoriteHubEntry* addFavorite(const FavoriteHubEntry& aEntry, const AutoStartType p_autostart = NOT_CHANGE); // [!] IRainman fav options
		void removeFavorite(const FavoriteHubEntry* entry);
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		static void changeConnectionStatus(const string& hubUrl, ConnectionStatus::Status status);
#endif
		
		static FavoriteHubEntry* getFavoriteHubEntry(const string& aServer);
		
		static bool isPrivate(const string& p_url);
// Favorite hub groups
		// [!] IRainman fix.
		class LockInstanceHubs
		{
				const bool m_unique;
			public:
				explicit LockInstanceHubs(const bool unique = false) : m_unique(unique)
				{
					if (m_unique)
						FavoriteManager::g_csHubs->AcquireLockExclusive();
					else
						FavoriteManager::g_csHubs->AcquireLockShared();
				}
				~LockInstanceHubs()
				{
					if (m_unique)
						FavoriteManager::g_csHubs->ReleaseLockExclusive();
					else
						FavoriteManager::g_csHubs->ReleaseLockShared();
				}
				static const FavHubGroups& getFavHubGroups()
				{
					return FavoriteManager::g_favHubGroups;
				}
				static FavoriteHubEntryList& getFavoriteHubs()
				{
					return FavoriteManager::g_favoriteHubs;
				}
		};
		static void setFavHubGroups(FavHubGroups& p_favHubGroups)
		{
			CFlyWriteLock(*g_csHubs);
			swap(g_favHubGroups, p_favHubGroups);
		}
		// [!] IRainman fix.
		
		static FavoriteHubEntryList getFavoriteHubs(const string& group);
		
// Favorite Directories
		struct FavoriteDirectory
		{
			StringList ext; // [!] IRainman opt: split only time.
			string dir;
			string name;
		};
		typedef vector<FavoriteDirectory> FavDirList;
		
		static bool addFavoriteDir(string aDirectory, const string& aName, const string& aExt);
		static bool removeFavoriteDir(const string& aName);
		static bool renameFavoriteDir(const string& aName, const string& anotherName);
		static bool updateFavoriteDir(const string& aName, const string& dir, const string& ext); // [!] IRainman opt.
		static string getDownloadDirectory(const string& ext);
		static size_t getFavoriteDirsCount()
		{
			//FastSharedLock l(csDirs); no needs. TODO
			return g_favoriteDirs.size();
		}
		class LockInstanceDirs
		{
			public:
				LockInstanceDirs()
				{
					FavoriteManager::g_csDirs->AcquireLockShared();
				}
				~LockInstanceDirs()
				{
					FavoriteManager::g_csDirs->ReleaseLockShared();
				}
				static const FavDirList& getFavoriteDirsL()
				{
					return FavoriteManager::g_favoriteDirs;
				}
		};
		
// Recent Hubs
		static const RecentHubEntry::List& getRecentHubs()
		{
			return g_recentHubs;
		}
		
		RecentHubEntry* addRecent(const RecentHubEntry& aEntry);
		void removeRecent(const RecentHubEntry* entry);
		void updateRecent(const RecentHubEntry* entry);
		
		static RecentHubEntry* getRecentHubEntry(const string& aServer);
		static PreviewApplication* addPreviewApp(const string& name, const string& application, const string& arguments, string p_extension);
		static void removePreviewApp(const size_t index);
		static PreviewApplication* getPreviewApp(const size_t index);
		static void removeallRecent();
		
// User Commands
		static UserCommand addUserCommand(int type, int ctx, Flags::MaskType flags, const string& name, const string& command, const string& to, const string& p_Hub);
		static bool getUserCommand(int cid, UserCommand& uc);
		static int findUserCommand(const string& aName, const string& p_Hub);
		static bool moveUserCommand(int cid, int pos);
		static void updateUserCommand(const UserCommand& uc);
		static void removeUserCommandCID(int cid);
		static void removeUserCommand(const string& p_Hub);
		static size_t countUserCommand(const string& p_Hub);
		static void removeHubUserCommands(int ctx, const string& hub);
		
		static UserCommand::List getUserCommands()
		{
			CFlyReadLock(*g_csUserCommand);
			return g_userCommands;
		}
		UserCommand::List getUserCommands(int ctx, const StringList& hub/* [-] IRainman fix, bool& op*/) const;
		
		static void load();
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
		static bool load_from_url();
#endif
		static void save();
		static void recentsave();
		static size_t getCountFavsUsers();
		static bool isISPDelete(const string& p_server)
		{
			auto i = g_sync_hub_isp_delete.find(p_server);
			if (i == g_sync_hub_isp_delete.end())
				return false;
			else
				return true;
		}
	private:
		static void getFavoriteUsersNamesL(StringSet& p_users, bool p_is_ban);
		static bool isUserExistL(const UserPtr& aUser)
		{
			auto i = g_fav_users_map.find(aUser->getCID());
			if (i == g_fav_users_map.end())
				return false;
			else
				return true;
		}
		static FavoriteHubEntryList g_favoriteHubs;
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
		static StringSet g_sync_hub_local;
		static StringSet g_sync_hub_isp_delete;
		static StringSet g_sync_hub_external;
#endif
		static FavDirList g_favoriteDirs; // [~] InfinitySky. Code from Apex.
		static FavHubGroups g_favHubGroups;
		static RecentHubEntry::List g_recentHubs;
		static bool g_recent_dirty;
		static PreviewApplication::List g_previewApplications;
		static UserCommand::List g_userCommands;
#ifdef PPA_USER_COMMANDS_HUBS_SET
		static boost::unordered_set<string> g_userCommandsHubUrl;
		static bool isHubExistsL(const string& p_Hub)
		{
			return g_userCommandsHubUrl.find(p_Hub) != g_userCommandsHubUrl.end();
		}
#endif
		static int g_lastId;
		
		// [!] Fasts response if contact list empty.
		static bool g_isNotEmpty;
		static bool isNotEmpty()
		{
			return g_isNotEmpty;
		}
		static void updateEmptyStateL();
		// [~] Fasts response if contact list empty.
		
		static FavoriteMap g_fav_users_map;
		static StringSet   g_fav_users;
		// [!] IRainman opt: replace one recursive mutex to multiply shared spin locks.
		static std::unique_ptr<webrtc::RWLockWrapper> g_csFavUsers;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csHubs; //
		static std::unique_ptr<webrtc::RWLockWrapper> g_csDirs; //
		static std::unique_ptr<webrtc::RWLockWrapper> g_csUserCommand;
		
		static uint16_t g_dontSave;
		static unsigned g_count_hub;
	public:
		void prepareClose();
		void shutdown();
		static void connectToFlySupportHub();
		
		static void recentload(SimpleXML& aXml);
		static void previewload(SimpleXML& aXml);
		static void previewsave(SimpleXML& aXml);
		
	private:
		/** Used during loading to prevent saving. */
		
		friend class Singleton<FavoriteManager>;
		
		FavoriteManager();
		~FavoriteManager();
		
		static RecentHubEntry::Iter getRecentHub(const string& aServer);
		
		// ClientManagerListener
		void on(UserUpdated, const OnlineUserPtr& user) noexcept override;
		void on(UserConnected, const UserPtr& user) noexcept override;
		void on(UserDisconnected, const UserPtr& user) noexcept override;
		
		static void load(SimpleXML& aXml
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
		                 , bool p_is_url = false
#endif
		                );
		                
		static string getConfigFavoriteFile()
		{
			return Util::getConfigPath() + "Favorites.xml";
		}
		
		// [+] SSA addUser
		bool addUserL(const UserPtr& aUser, FavoriteMap::iterator& iUser, bool create = true);
		
		void speakUserUpdate(const bool added, FavoriteMap::iterator& i); // [+] IRainman
		
		static bool g_SupportsHubExist;
		static std::unordered_set<std::string> g_AllHubUrls;
		static bool replaceDeadHub();
	public:
		static std::string g_DefaultHubUrl;
		
};

#endif // !defined(FAVORITE_MANAGER_H)

/**
 * @file
 * $Id: FavoriteManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
