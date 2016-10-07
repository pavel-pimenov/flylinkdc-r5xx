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


#ifndef DCPLUSPLUS_DCPP_SHARE_MANAGER_H
#define DCPLUSPLUS_DCPP_SHARE_MANAGER_H

#include <ShlObj.h>

#include "SearchManager.h"
#include "LogManager.h"
#include "HashManager.h"
#include "QueueManagerListener.h"
#include "BloomFilter.h"
#include "Pointer.h"
#include "CFlylinkDBManager.h"

#ifdef STRONG_USE_DHT
namespace dht
{
class IndexManager;
}
#endif

STANDARD_EXCEPTION_ADD_INFO(ShareException); // [!] FlylinkDC++

class SimpleXML;
class Client;
class File;
class OutputStream;
class MemoryInputStream;
class SearchResultBaseTTH;

struct ShareLoader;
typedef std::vector<SearchResult> SearchResultList;
typedef boost::unordered_set<std::string> QueryNotExistsSet;
typedef boost::unordered_map<std::string, SearchResultList> QueryCacheMap;

class ShareManager : public Singleton<ShareManager>, private Thread, private TimerManagerListener,
	private HashManagerListener, private QueueManagerListener
{
	public:
		/**
		 * @param aDirectory Physical directory location
		 * @param aName Virtual name
		 */
		void addDirectory(const string& realPath, const string &virtualName, bool p_is_job);
		void removeDirectory(const string& realPath);
		void renameDirectory(const string& realPath, const string& virtualName);
		
		static string toRealPath(const TTHValue& tth);
		static bool checkType(const string& aString, Search::TypeModes aType);
		
		static bool destinationShared(const string& file_or_dir_name);
#if 0
		static bool getRealPathAndSize(const TTHValue& tth, string& path, int64_t& size);
#endif
#ifdef _DEBUG
		string toVirtual(const TTHValue& tth);
#endif
		string toReal(const string& virtualFile
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		              , bool ishidingShare
#endif
		             );
		TTHValue getTTH(const string& virtualFile) const;
		
		void refresh_share(bool dirs = false, bool aUpdate = true) noexcept;
		void setDirty()
		{
			m_is_xmlDirty = true;
			g_isNeedsUpdateShareSize = true;
		}
		void setPurgeTTH()
		{
			m_sweep_path = true;
		}
		static bool isShareFolder(const string& path, bool thoroughCheck = false);
		static int64_t removeExcludeFolder(const string &path, bool returnSize = true);
		static int64_t addExcludeFolder(const string &path);
		
		static bool   searchTTHArray(CFlySearchArrayTTH& p_tth_aray, const Client* p_client);
		static bool   isUnknownTTH(const TTHValue& p_tth);
		static bool   isUnknownFile(const string& p_search);
		static void   addUnknownFile(const string& p_search);
		static bool   isCacheFile(const string& p_search, SearchResultList& p_search_result);
		static void   addCacheFile(const string& p_search, const SearchResultList& p_search_result);
	private:
		bool   search_tth(const TTHValue& p_tth, SearchResultList& aResults, bool p_is_check_parent);
	public:
		void   search(SearchResultList& aResults, const SearchParam& p_search_param) noexcept;
		void   search_max_result(SearchResultList& aResults, const StringList& params, StringList::size_type maxResults, StringSearch::List& reguest) noexcept;
		
		bool findByRealPathName(const string& realPathname, TTHValue* outTTHPtr, string* outfilenamePtr = NULL, int64_t* outSizePtr = NULL); // [+] SSA
		
		static void getDirectories(CFlyDirItemArray& p_dirs);
		
		MemoryInputStream* generatePartialList(const string& dir, bool recurse
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		                                       , bool ishidingShare
#endif
		                                      ) const;
		                                      
		MemoryInputStream* getTree(const string& virtualFile) const;
		
		void getFileInfo(AdcCommand& p_cmd, const string& aFile);
		// [!] IRainman opt.
		static int64_t getShareSize();
	private:
		void internalCalcShareSize();
		static void internalClearCache(bool p_is_force);
	public:
		// [~] IRainman opt.
		static int64_t getShareSize(const string& realPath);
		
		static unsigned getLastSharedFiles()
		{
			return g_lastSharedFiles;
		}
		static int64_t getLastSharedDate()
		{
			return g_lastSharedDate;
		}
		static string getShareSizeString()
		{
			return Util::toString(getShareSize());
		}
		static tstring getShareSizeformatBytesW()
		{
			return Util::formatBytesW(getShareSize());
		}
		string getShareSizeString(const string& aDir)
		{
			return Util::toString(getShareSize(aDir));
		}
		
		static void getBloom(ByteVector& v, size_t k, size_t m, size_t h);
		
		static Search::TypeModes getFType(const string& p_fileName, bool p_include_flylinkdc_ext = false) noexcept;
		static string validateVirtual(const string& aVirt) noexcept;
		
		static void incHits()
		{
			++g_hits;
		}
		static size_t getHits()
		{
			return g_hits;
		}
		static void setHits(size_t p_hit)
		{
			g_hits = p_hit;
		}
		
		const string& getOwnListFile()
		{
			generateXmlList();
			return bzXmlFile;
		}
		
		static bool isTTHShared(const TTHValue& tth);
		
		GETSET(string, bzXmlFile, BZXmlFile);
		
	private:
		static size_t g_hits;
		static int64_t g_lastSharedDate;
		
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		static string getEmptyBZXmlFile()
		{
			return Util::getConfigPath() + "Emptyfiles.xml.bz2";
		}
#endif
		static string getDefaultBZXmlFile() // [+] IRainman fix.
		{
			return Util::getConfigPath() + "files.xml.bz2";
		}
		struct AdcSearch;
		class CFlyLowerName
		{
				string m_name;
				string m_low_name;
			public:
				CFlyLowerName(const string& p_name): m_name(p_name)
				{
				}
				
				//CFlyLowerName(const string& p_name, const string& p_low_name):
				//  m_name(p_name), m_low_name(p_low_name)
				//{
				//}
				virtual ~CFlyLowerName()
				{
					if (!m_name.empty())
					{
						//dcassert(!m_low_name.empty());
					}
				}
				const string& getName() const
				{
					return m_name;
				}
				const string& getLowName() const // http://flylinkdc.blogspot.com/2010/08/1.html
				{
					dcassert(!m_low_name.empty());
					return m_low_name;
				}
				void setNameAndLower(const string& p_name)
				{
					//dcassert(m_name.empty());
					m_name = p_name;
					initLowerName();
				}
				void initLowerName()
				{
					dcassert(!m_name.empty());
					m_low_name = Text::toLower(m_name);
				}
				/*
				            protected:
				                void init(const CFlyLowerName& p_n)
				                {
				                    m_name = p_n.m_name;
				                    m_low_name = p_n.m_low_name;
				                }
				*/
		};
		
		class Directory : public intrusive_ptr_base<Directory>, public CFlyLowerName
#ifdef _DEBUG
			, boost::noncopyable // [+] IRainman fix.
#endif
		{
			public:
				typedef boost::intrusive_ptr<Directory> Ptr;
				typedef std::map<string, Ptr> DirectoryMap;
				
				struct ShareFile : public CFlyLowerName
#ifdef _DEBUG
						//, boost::noncopyable // TODO - сделать чтобы объект был не копируемым - boost::noncopyable
#endif
				{
						/*
						struct StringComp
						#ifdef _DEBUG
						    //: private boost::noncopyable
						#endif
						
						{
						        explicit StringComp(const string& s) : m_a(s) { }
						        bool operator()(const ShareFile& b) const
						        {
						            return stricmp(m_a, b.getName()) == 0;
						        }
						    private:
						        const string& m_a;
						};
						*/
						
						struct FileTraits
						{
							size_t operator()(const ShareFile& a) const
							{
								return std::hash<std::string>()(a.getName());
							}
							bool operator()(const ShareFile &a, const ShareFile& b) const
							{
								return a.getName() == b.getName();
							}
						};
						typedef boost::unordered_set<ShareFile, FileTraits, FileTraits> Set;
						
						ShareFile(const string& aName, int64_t aSize, Directory::Ptr aParent, const TTHValue& aRoot, uint32_t aHit, uint32_t aTs,
						          Search::TypeModes aftype) :
							CFlyLowerName(aName), m_tth(aRoot), size(aSize), parent(aParent.get()), m_hit(aHit), ts(aTs), ftype(aftype), m_media_ptr(nullptr)
						{
							dcassert(aName.find('\\') == string::npos);
						}
					public:
						~ShareFile()
						{
							//dcdebug("~ShareFile() %s\n", getName().c_str() );
						}
						string getADCPath() const
						{
							return parent->getADCPath() + getName();
						}
						string getFullName() const
						{
							return parent->getFullName() + getName();
						}
						string getRealPath() const
						{
							return parent->getRealPath(getName());
						}
						
						GETSET(int64_t, size, Size);
						GETSET(Directory*, parent, Parent);
						GETC(uint32_t, m_hit, Hit);
						GETSET(uint32_t, ts, TS);
						std::shared_ptr<CFlyMediaInfo> m_media_ptr;
						
						void initMediainfo(std::shared_ptr<CFlyMediaInfo>& p_media_ptr)
						{
							dcassert(m_media_ptr == nullptr);
							m_media_ptr = p_media_ptr;
						}
						const TTHValue& getTTH() const
						{
							return m_tth;
						}
						void setTTH(const TTHValue& p_tth)
						{
							m_tth = p_tth;
						}
						Search::TypeModes getFType() const
						{
							return ftype;
						}
					private:
						TTHValue m_tth;
						Search::TypeModes ftype;
				};
				
				DirectoryMap m_share_directories;
				ShareFile::Set m_share_files;
				int64_t m_size;
				
				static Ptr create(const string& aName, const Ptr& aParent = Ptr())
				{
					return Ptr(new Directory(aName, aParent));
				}
				
				bool hasType(Search::TypeModes p_type) const noexcept
				{
				    return (p_type == Search::TYPE_ANY) || (m_fileTypes_bitmap & (1 << p_type));
				}
				void addType(Search::TypeModes type) noexcept;
				
				string getADCPath() const noexcept;
				string getFullName() const noexcept;
				string getRealPath(const std::string& path) const;
				
				int64_t getDirSizeL() const noexcept;
				int64_t getDirSizeFast() const noexcept
				{
				    return m_size;
				}
				
				void search(SearchResultList& aResults, StringSearch::List& aStrings, const SearchParamBase& p_search_param) const noexcept;
				void search(SearchResultList& aResults, AdcSearch& aStrings, StringList::size_type maxResults) const noexcept;
				
				void toXml(OutputStream& xmlFile, string& indent, string& tmp2, bool fullList) const;
				void filesToXml(OutputStream& xmlFile, string& indent, string& tmp2) const;
				
				ShareFile::Set::const_iterator findFileIterL(const string& aFile) const
				{
					const auto l_res = std::find_if(m_share_files.begin(), m_share_files.end(), [&](const ShareFile & p_file) -> bool {return stricmp(p_file.getName(), aFile) == 0;});
					return l_res;
					//Directory::ShareFile::StringComp(aFile));
				}
				
				void mergeL(const Ptr& source);
				
				GETSET(Directory*, parent, Parent);
			private:
				friend void intrusive_ptr_release(intrusive_ptr_base<Directory>*);
				
				Directory(const string& aName, const Ptr& aParent);
				~Directory() { }
				/** Set of flags that say which Search::TYPE_* a directory contains */
				uint16_t m_fileTypes_bitmap;
		};
		
		friend class Directory;
		friend struct ShareLoader;
		
		friend class Singleton<ShareManager>;
		ShareManager();
		~ShareManager();
		
		struct AdcSearch
		{
			explicit AdcSearch(const StringList& params);
			
			bool isExcluded(const string& str);
			bool hasExt(const string& name);
			
			StringSearch::List* m_includePtr;
			StringSearch::List m_includeX;
			StringSearch::List m_exclude;
			StringList m_exts;
			StringList m_noExts;
			
			int64_t m_gt;
			int64_t m_lt;
			
			TTHValue m_root;
			bool m_hasRoot;
			bool m_isDirectory;
		};
		
		boost::atomic_flag m_updateXmlListInProcess; // [+] IRainman opt.
		
		int64_t xmlListLen;
		TTHValue xmlRoot;
		int64_t bzXmlListLen;
		TTHValue bzXmlRoot;
		unique_ptr<File> bzXmlRef;
		
		bool m_is_xmlDirty;
		bool m_is_forceXmlRefresh; /// bypass the 15-minutes guard
		bool m_is_refreshDirs;
		bool m_is_update;
		friend BufferedSocket;
		static bool g_is_initial;
		
		unsigned m_listN;
		
		boost::atomic_flag m_is_refreshing;
		
		uint64_t m_lastXmlUpdate;
		uint64_t m_lastFullUpdate;
		
		static std::unique_ptr<webrtc::RWLockWrapper> g_csTTHIndex;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csBloom;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csShare;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csDirList;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csShareNotExists;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csShareCache;
		
		// List of root directory items
		typedef std::list<Directory::Ptr> DirList; // только list - vector нельзя!
		static DirList g_list_directories;
		
		/** Map real name to virtual name - multiple real names may be mapped to a single virtual one */
		typedef boost::unordered_map<string, CFlyBaseDirItem> ShareMap;
		static ShareMap g_shares;
		static ShareMap g_lost_shares;
		
#ifdef STRONG_USE_DHT
		friend class ::dht::IndexManager;
#endif
		
		typedef boost::unordered_map<TTHValue, Directory::ShareFile::Set::const_iterator> HashFileMap;
		
		static HashFileMap g_tthIndex;
		static unsigned g_lastSharedFiles;
		static QueryNotExistsSet g_file_not_exists_set;
		static QueryCacheMap g_file_cache_map;
		
		//[+]IRainman opt.
		static bool g_isNeedsUpdateShareSize;
		static int64_t g_CurrentShareSize;
		static bool g_ignoreFileSizeHFS;
		static int g_RebuildIndexes;
		//[~]IRainman
		
		static BloomFilter<5> g_bloom;
		
		string findFileAndRealPath(const string& virtualFile, TTHValue& p_tth, bool p_is_fetch_tth) const;
		void checkShutdown(const string& virtualFile) const;
		
		Directory::Ptr buildTreeL(__int64& p_path_id, const string& p_path, const Directory::Ptr& p_parent, bool p_is_job);
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
		bool m_sweep_guard;
#endif
		bool m_sweep_path;
		bool checkHidden(const string& aName) const;
		//[+]IRainman
		bool checkSystem(const string& aName) const;
		bool checkVirtual(const string& aName) const;
		bool checkAttributs(const string& aName) const;
		//[~]IRainman
		void rebuildIndicesL();
		
		bool updateIndicesDirL(Directory& aDirectory);
		bool updateIndicesFileL(Directory& dir, const Directory::ShareFile::Set::iterator& i);
		
		Directory::Ptr get_mergeL(const Directory::Ptr& directory);
		
		void generateXmlList();
		static StringList g_notShared;
		bool loadCache() noexcept;
		static DirList::const_iterator getByVirtualL(const string& virtualName);
		pair<Directory::Ptr, string> splitVirtualL(const string& virtualPath) const;
		static string findRealRootL(const string& virtualRoot, const string& virtualLeaf);
		
		static Directory::Ptr getDirectoryL(const string& fname);
		
		int run();
	public:
#ifdef USE_REBUILD_MEDIAINFO
		struct MediainfoFile
		{
			string m_path;
			string m_file_name;
			__int64  m_size;
			__int64  m_path_id;
			TTHValue m_tth;
		};
		typedef std::vector<MediainfoFile> MediainfoFileArray;
		__int64 rebuildMediainfo(CFlyLog& p_log);
		__int64 rebuildMediainfo(Directory& p_dir, CFlyLog& p_log, ShareManager::MediainfoFileArray& p_result);
#endif
		void shutdown();
		static void setIgnoreFileSizeHFS()
		{
			g_ignoreFileSizeHFS = true;
		}
		static bool isFileInSharedDirectoryL(const string& p_fname)
		{
			return getDirectoryL(p_fname) != NULL;
		}
		static void load(SimpleXML& aXml);
		static void save(SimpleXML& aXml);
		
	private:
		// QueueManagerListener
		void on(QueueManagerListener::FileMoved, const string& n) noexcept override;
		
		// HashManagerListener
		void on(HashManagerListener::TTHDone, const string& fname, const TTHValue& root,
		        int64_t aTimeStamp, const CFlyMediaInfo& p_out_media, int64_t p_size) noexcept override;
		        
		bool isInSkipList(const string& lowerName) const;
		bool isSkipListEmpty() const
		{
			return m_skipList.empty();
		}
		void rebuildSkipList();
		StringList m_skipList;
		int m_count_sec;
		mutable FastCriticalSection m_csSkipList;
		// [~] IRainman opt.
		
		// TimerManagerListener
		void on(TimerManagerListener::Minute, uint64_t tick) noexcept override;
		void on(TimerManagerListener::Second, uint64_t tick) noexcept override;
		
};

#endif // !defined(SHARE_MANAGER_H)

/**
 * @file
 * $Id: ShareManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
