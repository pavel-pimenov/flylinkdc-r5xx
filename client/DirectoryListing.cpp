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
#include "DirectoryListing.h"
#include "QueueManager.h"
#include "SearchManager.h"
#include "StringTokenizer.h"
#include "SimpleXML.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#include "CryptoManager.h"
#include "SimpleXMLReader.h"
#include "User.h"
#include "ShareManager.h"
#include "../FlyFeatures/flyServer.h"


DirectoryListing::DirectoryListing(const HintedUser& aUser) :
	hintedUser(aUser), abort(false), root(new Directory(nullptr, Util::emptyString, false, false, true)),
	includeSelf(false), m_is_mediainfo(false), m_own_list(false)
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
	
	if (ext == ".dcls" || ext == ".dclst")  // [+] IRainman dclst support
	{
		UserPtr aUser(new User(CID()));
		aUser->setLastNick(name);
		return aUser;
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
		// return UserPtr();
		return ClientManager::getUser(name, "Unknown Hub"
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		                              , 0
#endif
		                              , false
		                             );
	}
	
	size_t n = name.length() - (i + 1);
	// CID's always 39 chars long...
	if (n != 39)
	{
		// return UserPtr();
		return ClientManager::getUser(name, "Unknown Hub"
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		                              , 0
#endif
		                              , false
		                             );
	}
	
	CID cid(name.substr(i + 1));
	if (cid.isZero())
	{
		// return UserPtr();
		return ClientManager::getUser(name, "Unknown Hub"
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		                              , 0
#endif
		                              , false
		                             );
	}
	
	UserPtr u = ClientManager::getUser(cid, true);
	u->initLastNick(name.substr(0, i)); // [!] IRainman fix.
	return u;
}

void DirectoryListing::loadFile(const string& name, bool p_own_list)
{
	// For now, we detect type by ending...
	const string ext = Util::getFileExt(name);
	
	::File ff(name, ::File::READ, ::File::OPEN);
	if (stricmp(ext, ".bz2") == 0
	        || stricmp(ext, ".dcls") == 0 // [+] IRainman dclst support
	        || stricmp(ext, ".dclst") == 0 // [+] SSA dclst support
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

class ListLoader : public SimpleXMLReader::CallBack
{
	public:
		ListLoader(DirectoryListing* aList, DirectoryListing::Directory* root,
		           bool aUpdating, const UserPtr& p_user, bool p_own_list)
			: list(aList), cur(root), base("/"), m_is_in_listing(false),
			  m_is_updating(aUpdating), user(p_user), m_own_list(p_own_list),
			  m_is_mediainfo_list(false), m_is_first_check_mediainfo_list(false),
			  m_empty_file_name_counter(0)
		{
		}
		
		~ListLoader() { }
		
		void startTag(const string& name, StringPairList& attribs, bool simple);
		void endTag(const string& name, const string& data);
		
		const string& getBase() const
		{
			return base;
		}
		bool isMediainfoList() const
		{
			return m_is_first_check_mediainfo_list ? m_is_mediainfo_list : true;
		}
	private:
		DirectoryListing* list;
		DirectoryListing::Directory* cur;
		UserPtr user;
		
		StringMap params;
		string base;
		bool m_is_in_listing;
		bool m_is_updating;
		bool m_own_list;
		bool m_is_mediainfo_list;
		bool m_is_first_check_mediainfo_list;
		int m_empty_file_name_counter;
};

string DirectoryListing::updateXML(const string& xml, bool p_own_list)
{
	MemoryInputStream mis(xml);
	return loadXML(mis, true, p_own_list);
}

string DirectoryListing::loadXML(InputStream& is, bool updating, bool p_own_list)
{
	CFlyLog l_log("[loadXML]");
	ListLoader ll(this, getRoot(), updating, getUser(), p_own_list);
	l_log.step("start parse");
	SimpleXMLReader(&ll).parse(is);
	l_log.step("stop parse");
	m_is_mediainfo = ll.isMediainfoList();
	m_own_list = p_own_list;
	return ll.getBase();
}

static const string sFileListing = "FileListing";
static const string sBase = "Base";
static const string sCID = "CID";// [+] IRainman Delayed loading (dclst support)
static const string sGenerator = "Generator";
static const string sIncludeSelf = "IncludeSelf"; // [+] SSA IncludeSelf attrib (dclst support)
static const string sDirectory = "Directory";
static const string sIncomplete = "Incomplete";
static const string sFile = "File";
static const string sName = "Name";
static const string sSize = "Size";
static const string sTTH = "TTH";
static const string sHIT = "HIT";
static const string sTS = "TS";
static const string sShared = "Shared";
static const string sBR = "BR";
static const string sWH = "WH";
static const string sMVideo = "MV";
static const string sMAudio = "MA";

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
	if (ShareManager::isShutdown())
	{
		throw AbortException("ListLoader::startTag - ShareManager::isShutdown()");
	}
	if (ClientManager::isShutdown())
	{
		throw AbortException("ListLoader::startTag - ClientManager::isShutdown()");
	}
	if (list->getAbort())
	{
		throw AbortException("ListLoader::startTag - " + STRING(ABORT_EM));
	}
	
	if (m_is_in_listing)
	{
		if (name == sFile)
		{
			dcassert(attribs.size() >= 3); // Иногда есть Shared - 4-тый атрибут.
			// это тэг от грея. его тоже можно обработать и записать в TS. хотя там 64 битное время
			const string& l_name = getAttrib(attribs, sName, 0);
			if (l_name.empty())
				return;
				
			const string& l_s = getAttrib(attribs, sSize, 1);
			if (l_s.empty())
				return;
			const auto l_size = Util::toInt64(l_s);
			
			const string& l_h = getAttrib(attribs, sTTH, 2);
			if (l_h.empty())
				return;
			const TTHValue l_tth(l_h); /// @todo verify validity?
			
			if (m_is_updating)
			{
				// just update the current file if it is already there.
				for (auto i = cur->files.cbegin(), iend = cur->files.cend(); i != iend; ++i)
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
			// [+] FlylinkDC
			std::shared_ptr<CFlyMediaInfo> l_mediaXY;
			uint32_t l_i_ts = 0;
			int l_i_hit     = 0;
			string l_hit;
			if (attribs.size() >= 4) // 3 - стандартный DC++, 4 - GreyLinkDC++
			{
				if (attribs.size() == 4) // http://code.google.com/p/flylinkdc/issues/detail?id=1402
				{
					const string l_sharedGL = getAttrib(attribs, sShared, 4);
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
					l_ts = getAttrib(attribs, sTS, 3); // TODO проверить  attribs.size() >= 4 если = 4 или 3 то TS нет и можно не искать
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
						l_hit = getAttrib(attribs, sHIT, 3);
						const std::string& l_audio = getAttrib(attribs, sMAudio, 3);
						const std::string& l_video = getAttrib(attribs, sMVideo, 3);
						if (!l_audio.empty() || !l_video.empty())
						{
							const string& l_br = getAttrib(attribs, sBR, 4);
							l_mediaXY = std::make_shared<CFlyMediaInfo> (getAttrib(attribs, sWH, 3),
							                                             atoi(l_br.c_str()),
							                                             l_audio,
							                                             l_video
							                                            );
						}
					}
				}
				l_i_hit = l_hit.empty() ? 0 : atoi(l_hit.c_str());
			}
			DirectoryListing::File* f = new DirectoryListing::File(cur, l_name, l_size, l_tth, l_i_hit, l_i_ts, l_mediaXY);
			cur->files.push_back(f);
			if (l_size) // http://code.google.com/p/flylinkdc/issues/detail?id=1098
			{
				if (m_own_list)//[+] FlylinkDC++
				{
					f->setFlag(DirectoryListing::FLAG_SHARED_OWN);  // TODO - убить FLAG_SHARED_OWN
				}
				else
				{
					if (ShareManager::getInstance()->isTTHShared(f->getTTH()))
					{
						f->setFlag(DirectoryListing::FLAG_SHARED);
					}
					else
					{
						if (!CFlyServerConfig::isParasitFile(f->getName())) // TODO - опимизнуть по расширениям
						{
							f->setFlag(DirectoryListing::FLAG_NOT_SHARED);
							// TODO - копипаста
							const auto l_status_file = CFlylinkDBManager::getInstance()->get_status_file(f->getTTH()); // TODO - унести в отдельную нитку?
							if (l_status_file & CFlylinkDBManager::PREVIOUSLY_DOWNLOADED)
								f->setFlag(DirectoryListing::FLAG_DOWNLOAD);
							if (l_status_file & CFlylinkDBManager::PREVIOUSLY_BEEN_IN_SHARE)
								f->setFlag(DirectoryListing::FLAG_OLD_TTH);
						}
					}
				}//[+] FlylinkDC++
			}
		}
		else if (name == sDirectory)
		{
			string l_file_name = getAttrib(attribs, sName, 0);
			if (l_file_name.empty())
			{
				//  throw SimpleXMLException("Directory missing name attribute");
				// http://code.google.com/p/flylinkdc/issues/detail?id=1101
				l_file_name = "empty_file_name_" + Util::toString(++m_empty_file_name_counter);
			}
			const bool incomp = getAttrib(attribs, sIncomplete, 1) == "1";
			DirectoryListing::Directory* d = nullptr;
			if (m_is_updating)
			{
				for (auto i  = cur->directories.cbegin(); i != cur->directories.cend(); ++i)
				{
					/// @todo comparisons should be case-insensitive but it takes too long - add a cache
					if ((*i)->getName() == l_file_name) // 2012-05-03_22-00-59_4VI7DMJ7TF5ACPEQLDFERQRFILMVMSHBJBEWDNI_D84240F5_crash-stack-r502-beta24-build-9900.dmp
					{
						d = *i;
						if (!d->getComplete())
							d->setComplete(!incomp);
						break;
					}
				}
			}
			if (d == nullptr)
			{
				d = new DirectoryListing::Directory(cur, l_file_name, false, !incomp, isMediainfoList());
				cur->directories.push_back(d);
			}
			cur = d;
			
			if (simple)
			{
				// To handle <Directory Name="..." />
				endTag(name, Util::emptyString);
			}
		}
	}
	else if (name == sFileListing)
	{
		const string& b = getAttrib(attribs, sBase, 2);
		if (b.size() >= 1 && b[0] == '/' && b[b.size() - 1] == '/')
		{
			base = b;
		}
		if (base.size() > 1) // [+]PPA fix for [4](("Version", "1"),("CID", "EDI7OWB6TZWH6X6L2D3INC6ORQSG6RQDJ6AJ5QY"),("Base", "/"),("Generator", "DC++ 0.785"))
		{
			const StringTokenizer<string> sl(base.substr(1), '/');
			for (auto i = sl.getTokens().cbegin(); i != sl.getTokens().cend(); ++i)
			{
				DirectoryListing::Directory* d = nullptr;
				for (auto j = cur->directories.cbegin(); j != cur->directories.cend(); ++j)
				{
					if ((*j)->getName() == *i)
					{
						d = *j;
						break;
					}
				}
				if (d == nullptr)
				{
					d = new DirectoryListing::Directory(cur, *i, false, false, isMediainfoList());
					cur->directories.push_back(d);
				}
				cur = d;
			}
		}
		cur->setComplete(true);
		
		// [+] IRainman Delayed loading (dclst support)
		const string& l_cidStr = getAttrib(attribs, sCID, 2);
		if (l_cidStr.size() == 39)
		{
			CID l_CID(l_cidStr);
			if (!l_CID.isZero())
			{
				if (!user)
				{
					user = ClientManager::getUser(l_CID, true);
					list->setHintedUser(HintedUser(user, Util::emptyString));
				}
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
				const string& generator = getAttrib(attribs, sGenerator, 2);
				ClientManager::getInstance()->setGenerator(user, generator);
#endif
			}
		}
		const string& l_getIncludeSelf = getAttrib(attribs, sIncludeSelf, 2);
		list->setIncludeSelf(l_getIncludeSelf == "1");
		// [~] IRainman Delayed loading (dclst support)
		
		m_is_in_listing = true;
		
		if (simple)
		{
			// To handle <Directory Name="..." />
			endTag(name, Util::emptyString);
		}
	}
}

void ListLoader::endTag(const string& name, const string&)
{
	if (m_is_in_listing)
	{
		if (name == sDirectory)
		{
			cur = cur->getParent();
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
		return Util::emptyString;
		
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
		QueueManager::getInstance()->addDirectory(Util::emptyString, hintedUser, target, prio);
	}
	else
	{
		// First, recurse over the directories
		const Directory::List& lst = aDir->directories;
		//[!] sort(lst.begin(), lst.end(), Directory::DirSort()); //[-] FlylinkDC++ Team - пусть качаются диры в порядке файл-листа.
		for (auto j = lst.cbegin(); j != lst.cend(); ++j)
		{
			download(*j, target, highPrio, prio, p_first_file);
			p_first_file = false;
		}
		// Then add the files
		File::List& l = aDir->files;
		//[!] sort(l.begin(), l.end(), File::FileSort());  //[-] FlylinkDC++ Team - сортировка файлов по алфавиту тормозит при кол-ва файлов > 10 тыс
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
				LogManager::getInstance()->message("DirectoryListing::download - QueueException:" + e.getError());
			}
			catch (const FileException& e)
			{
				LogManager::getInstance()->message("DirectoryListing::download - FileException:" + e.getError());
			}
		}
	}
}

void DirectoryListing::download(const string& aDir, const string& aTarget, bool highPrio, QueueItem::Priority prio)
{
	if (aDir.size() <= 2)
	{
		LogManager::getInstance()->message("[error] DirectoryListing::download aDir.size() <= 2 aDir=" + aDir + " aTarget = " + aTarget);
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
	
	QueueManager::getInstance()->add(aTarget, aFile->getSize(), aFile->getTTH(), getUser(), flags, true, p_first_file);
	
	if (highPrio || (prio != QueueItem::DEFAULT))
		QueueManager::getInstance()->setPriority(aTarget, highPrio ? QueueItem::HIGHEST : prio);
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
void DirectoryListing::logMatchedFiles(const UserPtr& p_user, int p_count) //[+]PPA
{
	const size_t l_BUF_SIZE = STRING(MATCHED_FILES).size() + 16;
	string l_tmp;
	l_tmp.resize(l_BUF_SIZE);
	snprintf(&l_tmp[0], l_tmp.size(), CSTRING(MATCHED_FILES), p_count);
	// Util::toString(ClientManager::getNicks(p_user->getCID(), Util::emptyString)) падает https://www.crash-server.com/Problem.aspx?ClientID=ppa&Login=Guest&ProblemID=58736
        // падаем со слов пользователей при клике на магнит в чате и выборе "Добавить в очередь для скачивания"
	const string l_last_nick = p_user->getLastNick();
	LogManager::getInstance()->message(l_last_nick + string(": ") + l_tmp.c_str());
}

struct HashContained
{
		explicit HashContained(const DirectoryListing::Directory::TTHSet& l) : tl(l) { }
		bool operator()(const DirectoryListing::File::Ptr i) const
		{
			return tl.count((i->getTTH())) && (DeleteFunction()(i), true);
		}
	private:
		void operator=(HashContained&); // [!] IRainman fix.
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
	for_each(directories.begin(), directories.end(), DeleteFunction());
	for_each(files.begin(), files.end(), DeleteFunction());
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
	files.erase(std::remove_if(files.begin(), files.end(), HashContained(l)), files.end());
}

void DirectoryListing::Directory::getHashList(DirectoryListing::Directory::TTHSet& l)
{
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		(*i)->getHashList(l);
	}
	for (auto i = files.cbegin(); i != files.cend(); ++i) l.insert((*i)->getTTH());
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
	for (auto i = files.cbegin(); i != files.cend(); ++i)
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
	for (auto i = files.cbegin(); i != files.cend(); ++i)
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
	for (auto i = files.cbegin(); i != files.cend(); ++i)
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
	for (auto i = files.cbegin(); i != files.cend(); ++i)
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

// !fulDC! !SMT!-UI
void DirectoryListing::Directory::checkDupes(const DirectoryListing* lst)
{
	Flags::MaskType result = 0;
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		(*i)->checkDupes(lst);
		result |= (*i)->getFlags() & (
		              FLAG_OLD_TTH | FLAG_DOWNLOAD | FLAG_SHARED | FLAG_NOT_SHARED);
	}
	if (files.size())
		result |= FLAG_DOWNLOAD_FOLDER;
	for (auto i = files.cbegin(); i != files.cend(); ++i)
	{
		//don't count 0 byte files since it'll give lots of partial dupes
		//of no interest
		if ((*i)->getSize() > 0)
		{
			result |= (*i)->getFlags() & (
			              FLAG_OLD_TTH | FLAG_DOWNLOAD |  FLAG_SHARED | FLAG_NOT_SHARED);
			if (!(*i)->isAnySet(FLAG_OLD_TTH | FLAG_DOWNLOAD |  FLAG_SHARED))
				result &= ~FLAG_DOWNLOAD_FOLDER;
		}
	}
	setFlags(result);
}

// !SMT!-UI
void DirectoryListing::checkDupes()
{
	root->checkDupes(this);
}

/**
 * @file
 * $Id: DirectoryListing.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
