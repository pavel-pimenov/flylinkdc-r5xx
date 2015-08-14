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

#ifndef DCPLUSPLUS_DCPP_DIRECTORY_LISTING_H
#define DCPLUSPLUS_DCPP_DIRECTORY_LISTING_H

#include "SimpleXML.h"
#include "QueueItem.h"
#include "CFlyMediaInfo.h"
#include "UserInfoBase.h"

class ListLoader;
class DirectoryListingFrame;
STANDARD_EXCEPTION(AbortException);

class DirectoryListing : public UserInfoBase
{
	public:
		class Directory;
		// !SMT!-UI  dupe/downloads search results in both class File and class Directory
		enum
		{
			FLAG_SHARED             = 1 << 0,
			FLAG_NOT_SHARED         = 1 << 1,
			FLAG_DOWNLOAD           = 1 << 2, //[+]PPA
			FLAG_OLD_TTH            = 1 << 3, //[+]PPA
			FLAG_DOWNLOAD_FOLDER    = 1 << 4, //[+]PPA
			FLAG_SHARED_OWN         = 1 << 5, //[+]NightOrion TODO do flag at file-list, not in every file in this list
			FLAG_VIRUS_FILE         = 1 << 6,
			FLAG_VIRUS_FILE_FOLDER  = 1 << 7 // TODO
		};
		
		class File :
			public Flags
#ifdef _DEBUG
			, boost::noncopyable // [+] IRainman fix.
#endif
		{
			public:
				typedef File* Ptr;
				struct FileSort
				{
					bool operator()(const Ptr& a, const Ptr& b) const
					{
						return _stricmp(a->getName().c_str(), b->getName().c_str()) < 0;
					}
				};
				typedef vector<Ptr> List;
				
			File(Directory* p_Dir, const string& p_Name, int64_t p_Size, const TTHValue& p_TTH, uint32_t p_Hit, uint32_t p_ts, std::shared_ptr<CFlyMediaInfo>& p_media) noexcept :
				name(p_Name), size(p_Size), parent(p_Dir), tthRoot(p_TTH), hit(p_Hit), ts(p_ts), m_media(p_media), adls(false)
				{
				}
				File(const File& rhs, bool _adls = false) : name(rhs.name), size(rhs.size), parent(rhs.parent), tthRoot(rhs.tthRoot),
					hit(rhs.hit), ts(rhs.ts), adls(_adls), m_media(rhs.m_media)
				{
				}
				~File()
				{
				}
				
				GETSET(string, name, Name);
				GETSET(int64_t, size, Size);
				GETSET(Directory*, parent, Parent);
				GETSET(TTHValue, tthRoot, TTH);
				GETSET(uint64_t, hit, Hit);
				GETSET(int64_t, ts, TS);
				std::shared_ptr<CFlyMediaInfo> m_media;
				GETSET(bool, adls, Adls);
		};
		class CFlyVirusDetector
		{
			public:
				CFlyVirusDetector() : m_sum_size_exe(0), m_count_others(0), m_count_exe(0), m_max_size_exe(0), m_min_size_exe(std::numeric_limits<int64_t>::max())
				{
				}
				uint32_t m_count_exe;
				uint32_t m_count_others;
				int64_t m_max_size_exe;
				int64_t m_min_size_exe;
				int64_t m_sum_size_exe;
				
				bool is_virus_dir() const;
				static bool is_virus_file(const string& p_file, int64_t p_size);
				void add(const string& p_file, int64_t p_size);
		};
		
		class Directory : public Flags //!fulDC! !SMT!-UI
#ifdef _DEBUG
			, boost::noncopyable
#endif
		{
			public:
				typedef Directory* Ptr;
				struct DirSort
				{
					bool operator()(const Ptr& a, const Ptr& b) const
					{
						return _stricmp(a->getName().c_str(), b->getName().c_str()) < 0;
					}
				};
				typedef vector<Ptr> List;
				
				typedef std::unordered_set<TTHValue> TTHSet;
				
				List directories;
				
				File::List files;
				CFlyVirusDetector m_virus_detect;
				DirectoryListing* m_directory_list;
				size_t getFileCount() const
				{
					return files.size();
				}
				
				Directory(DirectoryListing* p_directory_list, Directory* aParent, const string& aName, bool _adls, bool aComplete, bool p_is_mediainfo)
					: name(aName), parent(aParent), adls(_adls), complete(aComplete), m_is_mediainfo(p_is_mediainfo), m_directory_list(p_directory_list)
				{
				}
				
				virtual ~Directory();
				
				size_t   getTotalFileCount(bool adls = false) const;
				size_t   getTotalFolderCount() const;
				uint64_t getTotalSize(bool adls = false) const;
				uint64_t getTotalHit() const;
				int64_t  getTotalTS() const;
				std::pair<uint32_t, uint32_t> getMinMaxBitrateDir() const;
				std::pair<uint32_t, uint32_t> getMinMaxBitrateFile() const;
				tstring  getMinMaxBitrateDirAsString() const;
				uint64_t getSumSize() const;
				uint64_t getSumHit() const;
				int64_t  getMaxTS() const;
				void filterList(DirectoryListing& dirList);
				void filterList(TTHSet& l);
				void getHashList(TTHSet& l);
				void checkDupes(const DirectoryListing* lst); // !SMT!-UI
				GETSET(string, name, Name);
				GETSET(Directory*, parent, Parent);
				GETSET(bool, adls, Adls);
				GETSET(bool, complete, Complete);
			private:
				bool m_is_mediainfo;
		};
		
		class AdlDirectory : public Directory
		{
			public:
				AdlDirectory(const string& aFullPath, Directory* aParent, const string& aName) : Directory(nullptr, aParent, aName, true, true, true), fullPath(aFullPath) { }
				
				GETSET(string, fullPath, FullPath);
		};
		
		explicit DirectoryListing(const HintedUser& aUser);
		~DirectoryListing();
		
		void loadFile(const string& name, bool p_own_list = false);
		
		string updateXML(const std::string&, bool p_own_list);
		string loadXML(InputStream& xml, bool updating, bool p_own_list);
		
		void download(const string& aDir, const string& aTarget, bool highPrio, QueueItem::Priority prio = QueueItem::DEFAULT);
		void download(Directory* aDir, const string& aTarget, bool highPrio, QueueItem::Priority prio, bool p_first_file = true);
		void download(const File* aFile, const string& aTarget, bool view, bool highPrio, QueueItem::Priority prio = QueueItem::DEFAULT, bool p_isDCLST = false, bool p_first_file = true);
		
		string getPath(const Directory* d) const;
		string getPath(const File* f) const
		{
			return getPath(f->getParent());
		}
		
		int64_t getTotalSize(bool adls = false)
		{
			return root->getTotalSize(adls);
		}
		size_t getTotalFileCount(bool adls = false)
		{
			return root->getTotalFileCount(adls);
		}
		size_t getTotalFolderCount()
		{
			return root->getTotalFolderCount();
		}
		
		const Directory* getRoot() const
		{
			return root;
		}
		Directory* getRoot()
		{
			return root;
		}
		
		void checkDupes(); // !fulDC!
		static UserPtr getUserFromFilename(const string& fileName);
		
		const UserPtr& getUser() const
		{
			return hintedUser.user;
		}
		
		GETSET(HintedUser, hintedUser, HintedUser);
		GETSET(bool, abort, Abort);
		GETSET(bool, includeSelf, IncludeSelf);
		void logMatchedFiles(const UserPtr& p_user, int p_count); //[+]PPA
	private:
		friend class ListLoader;
		friend class DirectoryListingFrame;
		
		Directory* root;
		bool m_is_mediainfo;
		bool m_is_own_list;
		Directory* find(const string& aName, Directory* current);
		
};

inline bool operator==(const DirectoryListing::Directory::Ptr a, const string& b)
{
	return stricmp(a->getName(), b) == 0;
}
inline bool operator==(const DirectoryListing::File::Ptr a, const string& b)
{
	return stricmp(a->getName(), b) == 0;
}

#endif // !defined(DIRECTORY_LISTING_H)

/**
 * @file
 * $Id: DirectoryListing.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
