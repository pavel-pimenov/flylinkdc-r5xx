/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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
#include "QueueManager.h"
#include "SearchManager.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#include "ShareManager.h"
#include "../FlyFeatures/flyServer.h"

#ifdef FLYLINKDC_USE_DIRLIST_FILE_EXT_STAT
std::unordered_map<string, DirectoryListing::CFlyStatExt> DirectoryListing::g_ext_stat;
#endif
DirectoryListing::DirectoryListing(const HintedUser& aUser) :
	hintedUser(aUser), abort(false), root(new Directory(this, nullptr, BaseUtil::emptyString, false, false, true)),
	includeSelf(false), m_is_mediainfo(false), m_is_own_list(false)
{
}

DirectoryListing::~DirectoryListing()
{
	delete root;
}

UserPtr DirectoryListing::getUserFromFilename(const string& fileName)
{
	// General file list name format: [username].[CID].[xml|xml.bz2]
	
	string name = Util::getFileName(fileName);
	string ext = Text::toLower(Util::getFileExt(name));
	
	if (ext == ".dcls" || ext == ".dclst")
	{
		auto l_user = std::make_shared<User>(CID(), name, 0);
		return l_user;
	}
	
	// Strip off any extensions
	if (ext == ".bz2")
	{
		name.erase(name.length() - 4);
		ext = Text::toLower(Util::getFileExt(name));
	}
	
	if (ext == ".xml")
	{
		name.erase(name.length() - 4);
	}
	// Find CID
	string::size_type i = name.rfind('.');
	if (i == string::npos)
	{
		return ClientManager::getUser(name, "Unknown Hub", 0);
	}
	
	size_t n = name.length() - (i + 1);
	// CID's always 39 chars long...
	if (n != 39)
	{
		return ClientManager::getUser(name, "Unknown Hub", 0);
	}
	
	const CID cid(name.substr(i + 1));
	if (cid.isZero())
	{
		return ClientManager::getUser(name, "Unknown Hub", 0);
	}
	
	UserPtr u = ClientManager::createUser(cid, name.substr(0, i), 0);
	return u;
}

void DirectoryListing::loadFile(const string& p_file, bool p_own_list)
{
#ifdef _DEBUG
	LogManager::message("DirectoryListing::loadFile = " + p_file);
#endif
	m_file = p_file;
	// For now, we detect type by ending...
	const string ext = Util::getFileExt(p_file);
	if (!ClientManager::isBeforeShutdown())
	{
		::File ff(p_file, ::File::READ, ::File::OPEN);
		if (stricmp(ext, ".bz2") == 0
		        || stricmp(ext, ".dcls") == 0
		        || stricmp(ext, ".dclst") == 0
		   )
		{
			FilteredInputStream<UnBZFilter, false> f(&ff);
			loadXML(f, false, p_own_list);
		}
		else if (stricmp(ext, ".xml") == 0)
		{
			loadXML(ff, false, p_own_list);
		}
	}
}

class ListLoader : public SimpleXMLReader::CallBack
{
	public:
		ListLoader(DirectoryListing* aList, DirectoryListing::Directory* root,
		           bool aUpdating, const UserPtr& p_user, bool p_own_list)
			: m_list(aList), m_cur(root), m_base("/"), m_is_in_listing(false),
			  m_is_updating(aUpdating), m_user(p_user), m_is_own_list(p_own_list),
			  m_is_mediainfo_list(false), m_is_first_check_mediainfo_list(false),
			  m_empty_file_name_counter(0)
		{
		}
		
		~ListLoader() { }
		
		void startTag(const string& name, StringPairList& attribs, bool simple);
		void endTag(const string& name, const string& data);
		
		const string& getBase() const
		{
			return m_base;
		}
		bool isMediainfoList() const
		{
			return m_is_first_check_mediainfo_list ? m_is_mediainfo_list : true;
		}
	private:
#ifdef _DEBUG
		static CFlyCacheMediaInfo g_cache_mediainfo;
#endif
		DirectoryListing* m_list;
		DirectoryListing::Directory* m_cur;
		UserPtr m_user;
		
		StringMap m_params;
		string m_base;
		bool m_is_in_listing;
		bool m_is_updating;
		bool m_is_own_list;
		bool m_is_mediainfo_list;
		bool m_is_first_check_mediainfo_list;
		int m_empty_file_name_counter;
};

#ifdef _DEBUG
CFlyCacheMediaInfo ListLoader::g_cache_mediainfo;
#endif

string DirectoryListing::updateXML(const string& xml, bool p_own_list)
{
	MemoryInputStream mis(xml);
	return loadXML(mis, true, p_own_list);
}
void DirectoryListing::print_stat()
{
#ifdef FLYLINKDC_USE_DIRLIST_FILE_EXT_STAT
	std::multimap<unsigned, std::pair< std::string, CFlyStatExt> > l_order_count;
	for (auto j = g_ext_stat.cbegin(); j != g_ext_stat.cend(); ++j)
	{
		const auto& l_item = *j;
		l_order_count.insert(std::make_pair(j->second.m_count, std::make_pair(j->first, j->second)));
	}
	for (auto i = l_order_count.cbegin(); i != l_order_count.cend(); ++i)
	{
		LogManager::message("Count files: " + Util::toString(i->first) +
		                    " max size = " + Util::toString(i->second.second.m_max_size) +
		                    " min size = " + Util::toString(i->second.second.m_min_size) +
		                    " ext: ." + i->second.first + string(CFlyServerConfig::isCompressExt(i->second.first) ? string("   [compress]") : string()));
	}
#endif // FLYLINKDC_USE_DIRLIST_FILE_EXT_STAT
}

string DirectoryListing::loadXML(InputStream& is, bool updating, bool p_is_own_list)
{
	CFlyLog l_log("[loadXML]");
	ListLoader ll(this, getRoot(), updating, getUser(), p_is_own_list);
	//l_log.step("start parse");
	SimpleXMLReader(&ll).parse(is);
	l_log.step("Stop parse file:" + m_file);
	m_is_mediainfo = ll.isMediainfoList();
	m_is_own_list = p_is_own_list;
	return ll.getBase();
}

static const string sFileListing = "FileListing";
static const string sBase = "Base";
static const string sCID = "CID";
static const string sGenerator = "Generator";
static const string sIncludeSelf = "IncludeSelf";
static const string sIncomplete = "Incomplete";

/*
TODO - убрать копипаст
extern const string g_SDirectory;
extern const string g_SFile;
extern const string g_SName;
extern const string g_SSize;
extern const string g_STTH;
extern const string g_SHit;
extern const string g_STS;
extern const string g_SBR;
extern const string g_SWH;
extern const string g_SMVideo;
extern const string g_SMAudio;
*/

const string g_SDirectory = "Directory";
const string g_SFile = "File";
const string g_SName = "Name";
const string g_SSize = "Size";
const string g_STTH = "TTH";
const string g_SHit = "HIT";
const string g_STS = "TS";
const string g_SBR = "BR";
const string g_SWH = "WH";
const string g_SMVideo = "MV";
const string g_SMAudio = "MA";


const string g_SShared = "Shared";

void ListLoader::startTag(const string& name, StringPairList& attribs, bool simple)
{
#ifdef _DEBUG
	static size_t g_max_attribs_size = 0;
	if (g_max_attribs_size != attribs.size())
	{
		g_max_attribs_size = attribs.size();
		// dcdebug("ListLoader::startTag g_max_attribs_size = %d , attribs.capacity() = %d\n", g_max_attribs_size, attribs.capacity());
	}
#endif
	if (ClientManager::isBeforeShutdown())
	{
		throw AbortException("ListLoader::startTag - ClientManager::isBeforeShutdown()");
	}
	if (m_list->getAbort())
	{
		throw AbortException("ListLoader::startTag - " + STRING(ABORT_EM));
	}
	
	if (m_is_in_listing)
	{
		if (name == g_SFile)
		{
			dcassert(attribs.size() >= 3); // Иногда есть Shared - 4-тый атрибут.
			// это тэг от грея. его тоже можно обработать и записать в TS. хотя там 64 битное время
			const string l_name = getAttrib(attribs, g_SName, 0);
			if (l_name.empty())
			{
				dcassert(0);
				return;
			}
			
			const string l_s = getAttrib(attribs, g_SSize, 1);
			if (l_s.empty())
			{
				dcassert(0);
				return;
			}
			const auto l_size = Util::toInt64(l_s);
			
			const string l_h = getAttrib(attribs, g_STTH, 2);
			
			if (l_h.empty() || (m_is_own_list == false && l_h.compare(0, 39, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 39) == 0))
			{
				//dcassert(0);
				return;
			}
			const TTHValue l_tth(l_h); /// @todo verify validity?
			dcassert(l_tth != TTHValue());
			
			if (m_is_updating)
			{
				// just update the current file if it is already there.
				for (auto i = m_cur->m_files.cbegin(); i != m_cur->m_files.cend(); ++i)
				{
					auto& file = **i;
					/// @todo comparisons should be case-insensitive but it takes too long - add a cache
					if (file.getName() == l_name || file.getTTH() == l_tth)
					{
						file.setName(l_name);
						file.setSize(l_size);
						file.setTTH(l_tth);
						return;
					}
				}
			}
			std::shared_ptr<CFlyMediaInfo> l_mediaXY;
			uint32_t l_i_ts = 0;
			int l_i_hit     = 0;
			string l_hit;
#ifdef FLYLINKDC_USE_DIRLIST_FILE_EXT_STAT
			auto& l_item = DirectoryListing::g_ext_stat[Util::getFileExtWithoutDot(Text::toLower(l_name))];
			l_item.m_count++;
			if (l_size > l_item.m_max_size)
				l_item.m_max_size = l_size;
			if (l_size < l_item.m_min_size)
				l_item.m_min_size = l_size;
				
#endif
			if (attribs.size() >= 4) // 3 - стандартный DC++, 4 - GreyLinkDC++
			{
				if (attribs.size() == 4 ||
				        attribs.size() >= 11)  // Хитрый расширенный формат от http://p2p.toom.su/fs/hms/FCYECUWQ7F5A2FABW32UTMCT6MEMI3GPXBZDQCQ/)
				{
					const string l_sharedGL = getAttrib(attribs, g_SShared, 4);
					if (!l_sharedGL.empty())
					{
						const int64_t tmp_ts = _atoi64(l_sharedGL.c_str()) - 116444736000000000L ;
						if (tmp_ts <= 0L)
							l_i_ts = 0;
						else
							l_i_ts = uint32_t(tmp_ts / 10000000L);
					}
				}
				string l_ts;
				if (l_i_ts == 0)
				{
					l_ts = getAttrib(attribs, g_STS, 3); // TODO проверить  attribs.size() >= 4 если = 4 или 3 то TS нет и можно не искать
				}
				if (!m_is_first_check_mediainfo_list)
				{
					m_is_first_check_mediainfo_list = true;
					m_is_mediainfo_list = !l_ts.empty();
				}
				if (!l_ts.empty()  // Extended tags - exists only FlylinkDC++ or StrongDC++ sqlite or clones
				        || l_i_ts // Грейлинк - время расшаривания
				   )
				{
					if (!l_ts.empty())
					{
						l_i_ts = atoi(l_ts.c_str());
					}
					
					if (attribs.size() > 4) // TODO - собрать комбинации всех случаев
					{
						l_hit = getAttrib(attribs, g_SHit, 3);
						const std::string& l_audio = getAttrib(attribs, g_SMAudio, 3);
						const std::string& l_video = getAttrib(attribs, g_SMVideo, 3);
						if (!l_audio.empty() || !l_video.empty())
						{
							const string& l_br = getAttrib(attribs, g_SBR, 4);
							l_mediaXY = std::make_shared<CFlyMediaInfo>(getAttrib(attribs, g_SWH, 3),
							                                            atoi(l_br.c_str()),
							                                            l_audio,
							                                            l_video
							                                           );
						}
					}
					
#if 0
					if (attribs.size() > 4) // TODO - собрать комбинации всех случаев
					{
						CFlyMediainfoRAW l_media_item;
						{
							l_media_item.m_audio = getAttrib(attribs, g_SMAudio, 3);
							const size_t l_pos = l_media_item.m_audio.find('|', 0);
							if (l_pos != string::npos && l_pos)
							{
								if (l_pos + 2 < l_media_item.m_audio.length())
								{
									l_media_item.m_audio = l_media_item.m_audio.substr(l_pos + 2);
								}
							}
						}
						
						l_media_item.m_video = getAttrib(attribs, g_SMVideo, 3);
						l_hit = getAttrib(attribs, g_SHit, 3);
						l_media_item.m_WH = getAttrib(attribs, g_SWH, 3);
						if (!l_media_item.m_audio.empty() || !l_media_item.m_video.empty())
						{
							l_media_item.m_br = getAttrib(attribs, g_SBR, 4);
							auto& l_find_mi = g_cache_mediainfo[l_media_item];
							if (!l_find_mi)
							{
							
								l_find_mi = std::make_shared<CFlyMediaInfo>(l_media_item.m_WH,
								                                            atoi(l_media_item.m_br.c_str()),
								                                            l_media_item.m_audio,
								                                            l_media_item.m_video
								                                           );
								l_mediaXY = l_find_mi;
							}
						}
					}
#endif
				}
				l_i_hit = l_hit.empty() ? 0 : atoi(l_hit.c_str());
			}
			auto f = new DirectoryListing::File(m_cur, l_name, l_size, l_tth, l_i_hit, l_i_ts, l_mediaXY);
			m_cur->m_virus_detect.add(l_name, l_size);
			m_cur->m_files.push_back(f);
			if (l_size)
			{
				if (m_is_own_list)
				{
					f->setFlag(DirectoryListing::FLAG_SHARED_OWN);
				}
				else
				{
					if (ShareManager::isTTHShared(f->getTTH()))
					{
						f->setFlag(DirectoryListing::FLAG_SHARED);
					}
					else
					{
						if (QueueManager::is_queue_tth(f->getTTH()))
						{
							f->setFlag(DirectoryListing::FLAG_QUEUE);
						}
						// TODO if(l_size >= 100 * 1024 *1024)
						{
							if (!CFlyServerConfig::isParasitFile(f->getName())) // TODO - опимизнуть по расширениям
							{
								f->setFlag(DirectoryListing::FLAG_NOT_SHARED);
								const auto l_status_file = CFlylinkDBManager::getInstance()->get_status_file(f->getTTH()); // TODO - унести в отдельную нитку?
								if (l_status_file & CFlylinkDBManager::PREVIOUSLY_DOWNLOADED)
									f->setFlag(DirectoryListing::FLAG_DOWNLOAD);
								if (l_status_file & CFlylinkDBManager::VIRUS_FILE_KNOWN)
									f->setFlag(DirectoryListing::FLAG_VIRUS_FILE);
								if (l_status_file & CFlylinkDBManager::PREVIOUSLY_BEEN_IN_SHARE)
									f->setFlag(DirectoryListing::FLAG_OLD_TTH);
							}
						}
					}
				}
			}
		}
		else if (name == g_SDirectory)
		{
			string l_file_name = getAttrib(attribs, g_SName, 0);
			if (l_file_name.empty())
			{
				//  throw SimpleXMLException("Directory missing name attribute");
				l_file_name = "empty_file_name_" + Util::toString(++m_empty_file_name_counter);
			}
			const bool incomp = getAttrib(attribs, sIncomplete, 1) == "1";
			DirectoryListing::Directory* d = nullptr;
			if (m_is_updating)
			{
				for (auto i  = m_cur->directories.cbegin(); i != m_cur->directories.cend(); ++i)
				{
					/// @todo comparisons should be case-insensitive but it takes too long - add a cache
					if ((*i)->getName() == l_file_name)
					{
						d = *i;
						if (!d->getComplete())
						{
							d->setComplete(!incomp);
						}
						break;
					}
				}
			}
			if (d == nullptr)
			{
				d = new DirectoryListing::Directory(m_list, m_cur, l_file_name, false, !incomp, isMediainfoList());
				m_cur->directories.push_back(d);
			}
			m_cur = d;
			
			if (simple)
			{
				// To handle <Directory Name="..." />
				endTag(name, BaseUtil::emptyString);
			}
		}
	}
	else if (name == sFileListing)
	{
		const string& b = getAttrib(attribs, sBase, 2);
		if (b.size() >= 1 && b[0] == '/' && b[b.size() - 1] == '/')
		{
			m_base = b;
		}
		if (m_base.size() > 1)
		{
			const StringTokenizer<string> sl(m_base.substr(1), '/');
			for (auto i = sl.getTokens().cbegin(); i != sl.getTokens().cend(); ++i)
			{
				DirectoryListing::Directory* d = nullptr;
				for (auto j = m_cur->directories.cbegin(); j != m_cur->directories.cend(); ++j)
				{
					if ((*j)->getName() == *i)
					{
						d = *j;
						break;
					}
				}
				if (d == nullptr)
				{
					d = new DirectoryListing::Directory(m_list, m_cur, *i, false, false, isMediainfoList());
					m_cur->directories.push_back(d);
				}
				m_cur = d;
			}
		}
		m_cur->setComplete(true);
		
		const string& l_cidStr = getAttrib(attribs, sCID, 2);
		if (l_cidStr.size() == 39)
		{
			const CID l_CID(l_cidStr);
			if (!l_CID.isZero())
			{
				if (!m_user)
				{
					m_user = ClientManager::createUser(l_CID, "", 0);
					m_list->setHintedUser(HintedUser(m_user, BaseUtil::emptyString));
				}
			}
		}
		const string& l_getIncludeSelf = getAttrib(attribs, sIncludeSelf, 2);
		m_list->setIncludeSelf(l_getIncludeSelf == "1");
		
		m_is_in_listing = true;
		
		if (simple)
		{
			// To handle <Directory Name="..." />
			endTag(name, BaseUtil::emptyString);
		}
	}
}

void ListLoader::endTag(const string& name, const string&)
{
	if (m_is_in_listing)
	{
		if (name == g_SDirectory)
		{
			m_cur = m_cur->getParent();
		}
		else if (name == sFileListing)
		{
			// cur should be root now...
			m_is_in_listing = false;
		}
	}
}

string DirectoryListing::getPath(const Directory* d) const
{
	if (d == root)
		return BaseUtil::emptyString;
		
	string dir;
	dir.reserve(128);
	dir.append(d->getName());
	dir.append(1, '\\');
	
	Directory* cur = d->getParent();
	while (cur != root)
	{
		dir.insert(0, cur->getName() + '\\');
		cur = cur->getParent();
	}
	return dir;
}

void DirectoryListing::download(Directory* aDir, const string& aTarget, bool highPrio, QueueItem::Priority prio, bool p_first_file)
{
	string target = (aDir == getRoot()) ? aTarget : aTarget + aDir->getName() + PATH_SEPARATOR;
	if (!aDir->getComplete())
	{
		// folder is not completed (partial list?), so we need to download it first
		QueueManager::getInstance()->addDirectory(BaseUtil::emptyString, hintedUser, target, prio);
	}
	else
	{
		// First, recurse over the directories
		const Directory::List& lst = aDir->directories;
		for (auto j = lst.cbegin(); j != lst.cend(); ++j)
		{
			download(*j, target, highPrio, prio, p_first_file);
			p_first_file = false;
		}
		// Then add the files
		const File::List& l = aDir->m_files;
		for (auto i = l.cbegin(); i != l.cend(); ++i)
		{
			const File* file = *i;
			try
			{
				download(file, target + file->getName(), false, highPrio, prio, false, p_first_file);
				p_first_file = false;
			}
			catch (const QueueException& e)
			{
				LogManager::message("DirectoryListing::download - QueueException:" + e.getError());
			}
			catch (const FileException& e)
			{
				LogManager::message("DirectoryListing::download - FileException:" + e.getError());
			}
		}
	}
}

void DirectoryListing::download(const string& aDir, const string& aTarget, bool highPrio, QueueItem::Priority prio)
{
	if (aDir.size() <= 2)
	{
		LogManager::message("[error] DirectoryListing::download aDir.size() <= 2 aDir=" + aDir + " aTarget = " + aTarget);
		return;
	}
	dcassert(aDir.size() > 2);
	dcassert(aDir[aDir.size() - 1] == '\\'); // This should not be PATH_SEPARATOR
	Directory* d = find(aDir, getRoot());
	if (d != nullptr)
		download(d, aTarget, highPrio, prio);
}

void DirectoryListing::download(const File* aFile, const string& aTarget, bool view, bool highPrio, QueueItem::Priority prio, bool p_isDCLST, bool p_first_file)
{
	const Flags::MaskType flags = (Flags::MaskType)(view ? ((p_isDCLST ? QueueItem::FLAG_DCLST_LIST : QueueItem::FLAG_TEXT) | QueueItem::FLAG_CLIENT_VIEW) : 0);
	try
	{
		QueueManager::getInstance()->add(0, aTarget, aFile->getSize(), aFile->getTTH(), getUser(), flags, true, p_first_file); // TODO
	}
	catch (const Exception& e)
	{
		LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
	}
	
	if (highPrio || (prio != QueueItem::DEFAULT))
	{
		QueueManager::getInstance()->setPriority(aTarget, highPrio ? QueueItem::HIGHEST : prio);
	}
}

DirectoryListing::Directory* DirectoryListing::find(const string& aName, Directory* current)
{
	string::size_type end = aName.find('\\');
	dcassert(end != string::npos);
	if (end != string::npos)
	{
		const string name = aName.substr(0, end);
		auto i = std::find(current->directories.begin(), current->directories.end(), name);
		if (i != current->directories.end())
		{
			if (end == (aName.size() - 1))
				return *i;
			else
				return find(aName.substr(end + 1), *i);
		}
	}
	return nullptr;
}
void DirectoryListing::logMatchedFiles(const UserPtr& p_user, int p_count)
{
	const size_t l_BUF_SIZE = STRING(MATCHED_FILES).size() + 16;
	string l_tmp;
	l_tmp.resize(l_BUF_SIZE);
	_snprintf(&l_tmp[0], l_tmp.size(), CSTRING(MATCHED_FILES), p_count);
	// Util::toString(ClientManager::getNicks(p_user->getCID(), BaseUtil::emptyString)) падает https://www.crash-server.com/Problem.aspx?ClientID=guest&Login=Guest&ProblemID=58736
	// падаем со слов пользователей при клике на магнит в чате и выборе "Добавить в очередь для скачивания"
	const string l_last_nick = p_user->getLastNick();
	LogManager::message(l_last_nick + string(": ") + l_tmp.c_str());
}

struct HashContained
{
		explicit HashContained(const DirectoryListing::Directory::TTHSet& l) : tl(l) { }
		bool operator()(const DirectoryListing::File::Ptr i) const
		{
			return tl.count((i->getTTH())) && (DeleteFunction()(i), true);
		}
	private:
		void operator=(HashContained&);
		const DirectoryListing::Directory::TTHSet& tl;
};

struct DirectoryEmpty
{
	bool operator()(const DirectoryListing::Directory::Ptr i) const
	{
		const bool r = i->getFileCount() + i->directories.size() == 0;
		if (r)
		{
			DeleteFunction()(i);
		}
		return r;
	}
};

DirectoryListing::Directory::~Directory()
{

	if (m_directory_list && m_virus_detect.is_virus_dir())
	{
		CFlyVirusFileList l_file_list;
		l_file_list.m_virus_path = m_directory_list->getPath(this);
		l_file_list.m_hub_url = m_directory_list->getHintedUser().hint;
		l_file_list.m_nick = m_directory_list->getUser()->getLastNick();
		l_file_list.m_ip = m_directory_list->getUser()->getIPAsString();
		l_file_list.m_time = GET_TIME();
		for (auto j = m_files.begin(); j != m_files.end(); ++j)
		{
			if (CFlyVirusDetector::is_virus_file((*j)->getName(), (*j)->getSize()))
			{
				CFlyTTHKey l_key((*j)->getTTH(), (*j)->getSize());
				l_file_list.m_files[l_key].push_back((*j)->getName());
			}
		}
		CFlyServerJSON::addAntivirusCounter(l_file_list);
	}
	for_each(directories.begin(), directories.end(), DeleteFunction());
	for_each(m_files.begin(), m_files.end(), DeleteFunction());
}

bool DirectoryListing::CFlyVirusDetector::is_virus_dir() const
{
	if (m_count_exe > 100) // TODO - config
	{
		if (m_max_size_exe == m_min_size_exe)
			return true;
		if (m_count_others == 0)  // TODO - config
		{
			const auto l_avg = m_sum_size_exe / m_count_exe;
			if (abs(int64_t(l_avg) - m_max_size_exe) < 100 * 1024) // TODO - config
				return true;
		}
	}
	return false;
}

bool DirectoryListing::CFlyVirusDetector::is_virus_file(const string& p_file, int64_t p_size)
{
	const auto l_len = p_file.size();
	if (l_len >= 4 && p_size && p_size < 1024 * 1024 * 30) // TODO config
	{
		// const auto l_Text::toLower(Util::getFileExt(name));
		unsigned char l_ext[4];
		l_ext[0] = tolower(p_file[l_len - 4]);
		l_ext[1] = tolower(p_file[l_len - 3]);
		l_ext[2] = tolower(p_file[l_len - 2]);
		l_ext[3] = p_file[l_len - 1];
		
		if (l_ext[0] == '.' &&
		        (l_ext[1] == 'e' && l_ext[2] == 'x' && l_ext[3] == 'e') ||
		        (l_ext[1] == 'z' && l_ext[2] == 'i' && l_ext[3] == 'p') ||
		        (l_ext[1] == 'r' && l_ext[2] == 'a' && l_ext[3] == 'r'))
			return true;
	}
	return false;
}
void DirectoryListing::CFlyVirusDetector::add(const string& p_file, int64_t p_size)
{
	if (is_virus_file(p_file, p_size))
	{
		m_count_exe++;
		m_sum_size_exe += p_size;
		if (p_size > m_max_size_exe)
		{
			m_max_size_exe = p_size;
		}
		if (p_size < m_min_size_exe)
		{
			m_min_size_exe = p_size;
		}
	}
	else
	{
		m_count_others++;
	}
}

void DirectoryListing::Directory::filterList(DirectoryListing& dirList)
{
	DirectoryListing::Directory* d = dirList.getRoot();
	TTHSet l;
	d->getHashList(l);
	filterList(l);
}

void DirectoryListing::Directory::filterList(DirectoryListing::Directory::TTHSet& l)
{
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		(*i)->filterList(l);
	}
	directories.erase(std::remove_if(directories.begin(), directories.end(), DirectoryEmpty()), directories.end());
	m_files.erase(std::remove_if(m_files.begin(), m_files.end(), HashContained(l)), m_files.end());
}

void DirectoryListing::Directory::getHashList(DirectoryListing::Directory::TTHSet& l)
{
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		(*i)->getHashList(l);
	}
	for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
	{
		l.insert((*i)->getTTH());
	}
}

int64_t DirectoryListing::Directory::getTotalTS() const
{
	if (!m_is_mediainfo)
		return 0;
	int64_t x = getMaxTS();
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		x = std::max((*i)->getMaxTS(), x);
	}
	return x;
}
uint64_t DirectoryListing::Directory::getSumSize() const
{
	uint64_t x = 0;
	for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
	{
		x += (*i)->getSize();
	}
	return x;
}
uint64_t DirectoryListing::Directory::getSumHit() const
{
	if (!m_is_mediainfo)
		return 0;
	uint64_t x = 0;
	for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
	{
		x += (*i)->getHit();
	}
	return x;
}
int64_t DirectoryListing::Directory::getMaxTS() const
{
	if (!m_is_mediainfo)
		return 0;
	int64_t x = 0;
	for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
	{
		x = std::max((*i)->getTS(), x);
	}
	return x;
}
std::pair<uint32_t, uint32_t> DirectoryListing::Directory::getMinMaxBitrateFile() const
{
	std::pair<uint32_t, uint32_t> l_min_max(0, 0);
	if (!m_is_mediainfo)
		return l_min_max;
	l_min_max.first = static_cast<uint32_t>(-1);
	for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
	{
		if ((*i)->m_media)
		{
			if (const uint32_t l_tmp = (*i)->m_media->m_bitrate)
			{
				if (l_tmp < l_min_max.first)
					l_min_max.first = l_tmp;
				if (l_tmp > l_min_max.second)
					l_min_max.second = l_tmp;
			}
		}
	}
	return l_min_max;
}
tstring  DirectoryListing::Directory::getMinMaxBitrateDirAsString() const
{
	std::pair<uint32_t, uint32_t> l_dir_min_max = getMinMaxBitrateDir();
	tstring l_result;
	const bool l_min_exists = l_dir_min_max.first  && l_dir_min_max.first  != static_cast<uint32_t>(-1);
	const bool l_max_exists = l_dir_min_max.second && l_dir_min_max.second != static_cast<uint32_t>(-1);
	if (l_min_exists)
		l_result = Util::toStringW(l_dir_min_max.first);
	if (l_max_exists && l_dir_min_max.second != l_dir_min_max.first)
	{
		if (l_min_exists)
			l_result += _T('-');
		l_result += Util::toStringW(l_dir_min_max.second);
	}
	return l_result;
}

std::pair<uint32_t, uint32_t> DirectoryListing::Directory::getMinMaxBitrateDir() const
{
	std::pair<uint32_t, uint32_t> l_dir_min_max(0, 0);
	if (!m_is_mediainfo)
		return l_dir_min_max;
	l_dir_min_max = getMinMaxBitrateFile();
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		const std::pair<uint32_t, uint32_t> l_cur_min_max = (*i)->getMinMaxBitrateDir();
		if (l_cur_min_max.first < l_dir_min_max.first)
			l_dir_min_max.first = l_cur_min_max.first;
		if (l_cur_min_max.second > l_dir_min_max.second)
			l_dir_min_max.second = l_cur_min_max.second;
	}
	return l_dir_min_max;
}

uint64_t DirectoryListing::Directory::getTotalHit() const
{
	if (!m_is_mediainfo)
		return 0;
	uint64_t x = getSumHit();
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		x += (*i)->getTotalHit();
	}
	return x;
}
uint64_t DirectoryListing::Directory::getTotalSize(bool adl) const
{
	uint64_t x = getSumSize();
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		if (!(adl && (*i)->getAdls()))
			x += (*i)->getTotalSize(adls);
	}
	return x;
}

size_t DirectoryListing::Directory::getTotalFileCount(bool adl) const
{
	size_t x = getFileCount();
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		if (!(adl && (*i)->getAdls()))
			x += (*i)->getTotalFileCount(adls);
	}
	return x;
}
size_t DirectoryListing::Directory::getTotalFolderCount() const
{
	size_t x = directories.size();
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		x += (*i)->getTotalFolderCount();
	}
	return x;
}

void DirectoryListing::Directory::checkDupes(const DirectoryListing* lst)
{
	Flags::MaskType result = 0;
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		(*i)->checkDupes(lst);
		result |= (*i)->getFlags() & (
		              FLAG_OLD_TTH | FLAG_DOWNLOAD | FLAG_SHARED | FLAG_NOT_SHARED | FLAG_QUEUE);  // TODO | FLAG_VIRUS_FILE
	}
	if (m_files.size())
		result |= FLAG_DOWNLOAD_FOLDER;
	for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
	{
		//don't count 0 byte m_files since it'll give lots of partial dupes
		//of no interest
		if ((*i)->getSize() > 0)
		{
			result |= (*i)->getFlags() & (
			              FLAG_OLD_TTH | FLAG_DOWNLOAD | FLAG_SHARED | FLAG_NOT_SHARED | FLAG_QUEUE);
			if (!(*i)->isAnySet(FLAG_OLD_TTH | FLAG_DOWNLOAD | FLAG_SHARED | FLAG_QUEUE))
			{
				result &= ~FLAG_DOWNLOAD_FOLDER;
			}
		}
	}
	setFlags(result);
}


void DirectoryListing::checkDupes()
{
	root->checkDupes(this);
}

/**
 * @file
 * $Id: DirectoryListing.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
