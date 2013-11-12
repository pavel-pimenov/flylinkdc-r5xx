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
#include "ShareManager.h"
#include "CryptoManager.h"
#include "ClientManager.h"
#include "LogManager.h"
#include "HashManager.h"
#include "QueueManager.h"
#include "debug.h"
#include "AdcHub.h"
#include "SimpleXML.h"
#include "File.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#include "Wildcards.h"
#include "Transfer.h"
#include "Download.h"
#include "HashBloom.h"
#include "SearchResult.h"
#include "../FlyFeatures/flyServer.h"
#include "../client/CFlylinkDBManager.h"

#ifdef STRONG_USE_DHT
#include "../dht/IndexManager.h"
#endif

#ifdef _WIN32
# include <ShlObj.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fnmatch.h>
#endif

#include <boost/algorithm/string.hpp>

bool ShareManager::g_isShutdown = false;
size_t ShareManager::g_hits = 0;

ShareManager::ShareManager() : xmlListLen(0), bzXmlListLen(0),
	xmlDirty(true), forceXmlRefresh(false), refreshDirs(false), update(false), initial(true), listN(0), /* refreshing(false), [-] IRainman */
	lastXmlUpdate(0), lastFullUpdate(GET_TICK()), m_bloom(1 << 20), sharedSize(0),
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
	m_sweep_guard(false),
#endif
	m_sweep_path(false),
#ifdef _DEBUG
	m_CurrentShareSize(-1), // Чтобы словить в отладке отрицательную шару
#else
	m_CurrentShareSize(0),
#endif
	m_isNeedsUpdateShareSize(true) // [+]IRainman opt.
{
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	// [!] IRainman TODO: needs refactoring.
	const string emptyXmlName = getEmptyBZXmlFile();
	if (!File::isExist(emptyXmlName))
	{
		FilteredOutputStream<BZFilter, true> emptyXmlFile(new File(emptyXmlName, File::WRITE, File::TRUNCATE | File::CREATE));
		emptyXmlFile.write(SimpleXML::utf8Header);
		emptyXmlFile.write("<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"/\" Generator=\"DC++ " DCVERSIONSTRING "\">\r\n"); // Hide Share Mod
		emptyXmlFile.write("</FileListing>");
		emptyXmlFile.flush();
	}
#endif // IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	
	SettingsManager::getInstance()->addListener(this);
	TimerManager::getInstance()->addListener(this);
	QueueManager::getInstance()->addListener(this);
	HashManager::getInstance()->addListener(this);
}

ShareManager::~ShareManager()
{
	dcassert(isShutdown());
	
	SettingsManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);
	QueueManager::getInstance()->removeListener(this);
	HashManager::getInstance()->removeListener(this);
	
	join();
	
	if (bzXmlRef.get())
	{
		bzXmlRef.reset();
		// File::deleteFile(getBZXmlFile()); [-] IRainman fix: don't delete this file!
	}
	// [+] IRainman fix.
	const string& l_curFileName = getBZXmlFile();
	const string& l_DefaultFileName = getDefaultBZXmlFile();
	if (!l_curFileName.empty() && l_curFileName != l_DefaultFileName && File::isExist(l_curFileName))
	{
		try
		{
			File::renameFile(l_curFileName, l_DefaultFileName);
		}
		catch (const FileException&)
		{
			//[+]PPA log
		}
	}
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	File::deleteFile(getEmptyBZXmlFile()); // needs to update Client version in empty file list.
#endif
	// [~] IRainman fix.
}

ShareManager::Directory::Directory(const string& aName, const ShareManager::Directory::Ptr& aParent) :
	size(0),
	parent(aParent.get()),
	fileTypes(1 << SearchManager::TYPE_DIRECTORY)
{
	setName(aName);
}

string ShareManager::Directory::getADCPath() const noexcept
{
    Directory* l_Current = getParent();
    if (!l_Current)
    return '/' + getName() + '/';
    return l_Current->getADCPath() + getName() + '/';
}

string ShareManager::Directory::getFullName() const noexcept
	{
	    Directory* l_Current = getParent();
	    if (!l_Current)
	    return getName() + '\\';
	    return l_Current->getFullName() + getName() + '\\';
	}
	
	void ShareManager::Directory::addType(uint32_t type) noexcept
	{
		if (!hasType(type))
		{
			fileTypes |= (1 << type);
			if (getParent())
				getParent()->addType(type);
		}
	}

string ShareManager::Directory::getRealPath(const std::string& path) const
{
	Directory* l_Current = getParent();
	if (l_Current)
	{
		return l_Current->getRealPath(getName() + PATH_SEPARATOR_STR + path);
	}
	else
	{
		return ShareManager::getInstance()->findRealRoot(getName(), path);
	}
}

string ShareManager::findRealRoot(const string& virtualRoot, const string& virtualPath) const
{
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		if (stricmp(i->second, virtualRoot) == 0)
		{
			std::string name = i->first + virtualPath;
			dcdebug("Matching %s\n", name.c_str());
			if (FileFindIter(name) != FileFindIter::end)
			{
				return name;
			}
		}
	}
	
	throw ShareException(UserConnection::FILE_NOT_AVAILABLE, virtualPath);
}

int64_t ShareManager::Directory::getSize() const noexcept
{
    dcassert(!isShutdown());
    int64_t tmp = size;
    for (auto i = directories.cbegin(); i != directories.cend(); ++i)
{
tmp += i->second->getSize();
}
return tmp;
}

string ShareManager::toRealPath(const TTHValue& tth) const
{
	SharedLock l(cs);
	const auto& i = tthIndex.find(tth);
	if (i != tthIndex.end())
	{
		try
		{
			return i->second->getRealPath();
		}
		catch (const ShareException&) {}
	}
	return Util::emptyString;
}

#ifdef _DEBUG
string ShareManager::toVirtual(const TTHValue& tth) const
{
	if (tth == bzXmlRoot)
	{
		return Transfer::g_user_list_name_bz;
	}
	else if (tth == xmlRoot)
	{
		return Transfer::g_user_list_name;
	}
	{
		SharedLock l(cs);
		const auto& i = tthIndex.find(tth);
		if (i != tthIndex.end())
		{
			return i->second->getADCPath();
		}
	}
	
	throw ShareException(UserConnection::FILE_NOT_AVAILABLE, "ShareManager::toVirtual: " + tth.toBase32());
}
#endif

string ShareManager::toReal(const string& virtualFile
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
                            , bool ishidingShare
#endif
                           )
{
	//Lock l(cs); [-] IRainman opt.
	if (virtualFile == "MyList.DcLst")
	{
		throw ShareException("NMDC-style lists no longer supported, please upgrade your client", virtualFile);
	}
	else if (virtualFile == Transfer::g_user_list_name_bz || virtualFile == Transfer::g_user_list_name)
	{
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		if (ishidingShare)
		{
			return getEmptyBZXmlFile();
		}
#endif // IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		generateXmlList();
		{
			// [-] ReadLock l(cs); // [~] IRainman opt.
			return getBZXmlFile();
		}
	}
	
	SharedLock l(cs); // [+] IRainman opt.
	return findFile(virtualFile)->getRealPath();
}

TTHValue ShareManager::getTTH(const string& virtualFile) const
{
	// Lock l(cs); [-] IRainman opt.
	if (virtualFile == Transfer::g_user_list_name_bz)
	{
		return bzXmlRoot;
	}
	else if (virtualFile == Transfer::g_user_list_name)
	{
		return xmlRoot;
	}
	SharedLock l(cs); // [+] IRainman opt.
	return findFile(virtualFile)->getTTH();
}

MemoryInputStream* ShareManager::getTree(const string& virtualFile) const
{
	TigerTree tree;
	if (virtualFile.compare(0, 4, "TTH/", 4) == 0)
	{
		if (!CFlylinkDBManager::getInstance()->getTree(TTHValue(virtualFile.substr(4)), tree))
			return 0;
	}
	else
	{
		try
		{
			TTHValue tth = getTTH(virtualFile);
			CFlylinkDBManager::getInstance()->getTree(tth, tree);
		}
		catch (const Exception&)
		{
			return 0;
		}
	}
	
	ByteVector buf = tree.getLeafData();
	return new MemoryInputStream(&buf[0], buf.size());
}

AdcCommand ShareManager::getFileInfo(const string& aFile)
{
	if (aFile == Transfer::g_user_list_name)
	{
		generateXmlList();
		
		AdcCommand cmd(AdcCommand::CMD_RES);
		cmd.addParam("FN", aFile);
		cmd.addParam("SI", Util::toString(xmlListLen));
		cmd.addParam("TR", xmlRoot.toBase32());
		return cmd;
	}
	else if (aFile == Transfer::g_user_list_name_bz)
	{
		generateXmlList();
		
		AdcCommand cmd(AdcCommand::CMD_RES);
		cmd.addParam("FN", aFile);
		cmd.addParam("SI", Util::toString(bzXmlListLen));
		cmd.addParam("TR", bzXmlRoot.toBase32());
		return cmd;
	}
	
	if (aFile.compare(0, 4, "TTH/", 4) != 0)
		throw ShareException(UserConnection::FILE_NOT_AVAILABLE, aFile);
		
	TTHValue val(aFile.c_str() + 4); //[+]FlylinkDC++
	SharedLock l(cs);
	const auto& i = tthIndex.find(val);
	if (i == tthIndex.end())
	{
		throw ShareException(UserConnection::FILE_NOT_AVAILABLE, aFile);
	}
	
	const Directory::File& f = *i->second;
	AdcCommand cmd(AdcCommand::CMD_RES);
	cmd.addParam("FN", f.getADCPath());
	cmd.addParam("SI", Util::toString(f.getSize()));
	cmd.addParam("TR", f.getTTH().toBase32());
	return cmd;
}
pair<ShareManager::Directory::Ptr, string> ShareManager::splitVirtual(const string& virtualPath) const
{
	if (virtualPath.empty() || virtualPath[0] != '/')
	{
		throw ShareException(UserConnection::FILE_NOT_AVAILABLE , virtualPath);
	}
	
	string::size_type i = virtualPath.find('/', 1);
	if (i == string::npos || i == 1)
	{
		throw ShareException(UserConnection::FILE_NOT_AVAILABLE, virtualPath);
	}
	
	DirList::const_iterator dmi = getByVirtual(virtualPath.substr(1, i - 1));
	if (dmi == directories.end())
	{
		throw ShareException(UserConnection::FILE_NOT_AVAILABLE, virtualPath);
	}
	
	Directory::Ptr d = *dmi;
	
	string::size_type j = i + 1;
	while ((i = virtualPath.find('/', j)) != string::npos)
	{
		Directory::MapIter mi = d->directories.find(virtualPath.substr(j, i - j));
		j = i + 1;
		if (mi == d->directories.end())
			throw ShareException(UserConnection::FILE_NOT_AVAILABLE, virtualPath);
		d = mi->second;
	}
	
	return make_pair(d, virtualPath.substr(j));
}

ShareManager::Directory::File::Set::const_iterator ShareManager::findFile(const string& virtualFile) const
{
	if (virtualFile.compare(0, 4, "TTH/", 4) == 0)
	{
		const auto& i = tthIndex.find(TTHValue(virtualFile.substr(4)));
		if (i == tthIndex.end())
		{
			throw ShareException(UserConnection::FILE_NOT_AVAILABLE, virtualFile);
		}
		return i->second;
	}
	
	pair<Directory::Ptr, string> v = splitVirtual(virtualFile);
	Directory::File::Set::const_iterator it = find_if(v.first->files.begin(), v.first->files.end(),
	                                                  Directory::File::StringComp(v.second));
	if (it == v.first->files.end())
		throw ShareException(UserConnection::FILE_NOT_AVAILABLE, virtualFile);
	return it;
}

string ShareManager::validateVirtual(const string& aVirt) const noexcept
{
    string tmp = aVirt;
    string::size_type idx = 0;

    while ((idx = tmp.find_first_of("\\/"), idx) != string::npos)
{
tmp[idx] = '_';
}
return tmp;
}

bool ShareManager::hasVirtual(const string& virtualName) const noexcept
{
    SharedLock l(cs);
    return getByVirtual(virtualName) != directories.end();
}

void ShareManager::load(SimpleXML& aXml)
{
	UniqueLock l(cs);
#ifdef PPA_INCLUDE_OLD_INNOSETUP_WIZARD
	int l_count_dir = 0;
#endif
	aXml.resetCurrentChild();
	if (aXml.findChild("Share"))
	{
		aXml.stepIn();
		while (aXml.findChild("Directory"))
		{
			string realPath = aXml.getChildData();
			if (realPath.empty())
			{
				continue;
			}
			// make sure realPath ends with a PATH_SEPARATOR
			AppendPathSeparator(realPath); //[+]PPA
			
			if (!File::isExist(realPath))
				continue;
				
			const string& virtualName = aXml.getChildAttrib("Virtual");
			string vName = validateVirtual(virtualName.empty() ? Util::getLastDir(realPath) : virtualName);
			shares.insert(std::make_pair(realPath, vName));
			if (getByVirtual(vName) == directories.end())
			{
				directories.push_back(Directory::create(vName));
#ifdef PPA_INCLUDE_OLD_INNOSETUP_WIZARD
				l_count_dir++;
#endif
			}
		}
		aXml.stepOut();
	}
	if (aXml.findChild("NoShare"))
	{
		aXml.stepIn();
		while (aXml.findChild("Directory"))
			m_notShared.push_back(aXml.getChildData());
			
		aXml.stepOut();
	}
#ifdef PPA_INCLUDE_OLD_INNOSETUP_WIZARD
	if (l_count_dir == 0)
	{
		const string l_dir = Util::getRegistryValueString("DownloadDir", true);
		if (!l_dir.empty())
		{
			shares.insert(std::make_pair(l_dir, "DC++Downloads"));
			l_count_dir++;
		}
	}
#endif
}

static const string SDIRECTORY = "Directory";
static const string SFILE = "File";
static const string SNAME = "Name";
static const string SSIZE = "Size";
static const string STTH = "TTH";
static const string SHIT = "HIT";
static const string STS = "TS";
static const string SBR = "BR";
static const string SWH = "WH";
static const string SMVideo = "MV";
static const string SMAudio = "MA";

struct ShareLoader : public SimpleXMLReader::CallBack
{
		ShareLoader(ShareManager::DirList& aDirs) : dirs(aDirs), cur(0), depth(0) { }
		void startTag(const string& p_name, StringPairList& p_attribs, bool p_simple)
		{
			if (p_name == SDIRECTORY)
			{
				const string& name = getAttrib(p_attribs, SNAME, 0);
				if (!name.empty())
				{
					if (depth == 0)
					{
						for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
						{
							if (stricmp((*i)->getName(), name) == 0)
							{
								cur = *i;
								break;
							}
						}
					}
					else if (cur)
					{
						cur = ShareManager::Directory::create(name, cur);
						cur->getParent()->directories[cur->getName()] = cur;
					}
				}
				
				if (p_simple)
				{
					if (cur)
					{
						cur = cur->getParent();
					}
				}
				else
				{
					depth++;
				}
			}
			else if (cur && p_name == SFILE)
			{
				const string& fname = getAttrib(p_attribs, SNAME, 0);
				const string& size = getAttrib(p_attribs, SSIZE, 1);
				const string& root = getAttrib(p_attribs, STTH, 2);
				if (fname.empty() || size.empty() || (root.size() != 39))
				{
					dcdebug("Invalid file found: %s\n", fname.c_str());
					return;
				}
				CFlyMediaInfo l_mediaXY;
				const string& l_ts = getAttrib(p_attribs, STS, 3);
				const string& l_hit = getAttrib(p_attribs, SHIT, 3);
				const string& l_br = getAttrib(p_attribs, SBR, 4);
				l_mediaXY.init(getAttrib(p_attribs, SWH, 3), atoi(l_br.c_str()));
				l_mediaXY.m_audio = getAttrib(p_attribs, SMAudio, 3);
				l_mediaXY.m_video = getAttrib(p_attribs, SMVideo, 3);
				cur->files.insert(ShareManager::Directory::File(fname, Util::toInt64(size), cur, TTHValue(root),
				                                                atoi(l_hit.c_str()),
				                                                atoi(l_ts.c_str()),
				                                                ShareManager::getFType(fname),
				                                                l_mediaXY
				                                               )
				                 );
			}
		}
		void endTag(const string& p_name, const string&)
		{
			if (p_name == SDIRECTORY)
			{
				depth--;
				if (cur)
				{
					cur = cur->getParent();
				}
			}
		}
		
	private:
		ShareManager::DirList& dirs;
		ShareManager::Directory::Ptr cur;
		size_t depth;
};

bool ShareManager::loadCache() noexcept
{
	try
	{
		CFlyLog l_cache_loader_log("[Share cache loader]");
		ShareLoader loader(directories);
		SimpleXMLReader xml(&loader);
		const string cacheFile = getDefaultBZXmlFile();
		File ff(cacheFile, File::READ, File::OPEN); // [!] FlylinkDC: getDefaultBZXmlFile()
		FilteredInputStream<UnBZFilter, false> f(&ff);
		l_cache_loader_log.step("read and uncompress " + cacheFile + " done");
		
		xml.parse(f);
		l_cache_loader_log.step("parse xml done");
		
		for (auto i = directories.cbegin(); i != directories.cend(); ++i)
		{
			const Directory::Ptr& d = *i;
			updateIndices(*d);
		}
		l_cache_loader_log.step("update index done");
		internal_calcShareSize(); // [+] IRainman opt.
		return true;
	}
	catch (const Exception& e)
	{
		dcdebug("%s\n", e.getError().c_str());
	}
	return false;
}

void ShareManager::save(SimpleXML& aXml)
{
	SharedLock l(cs);
	
	aXml.addTag("Share");
	aXml.stepIn();
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		aXml.addTag("Directory", i->first);
		aXml.addChildAttrib("Virtual", i->second);
	}
	aXml.stepOut();
	aXml.addTag("NoShare");
	aXml.stepIn();
	for (auto j = m_notShared.cbegin(); j != m_notShared.cend(); ++j)
	{
		aXml.addTag("Directory", *j);
	}
	aXml.stepOut();
}
// [+] IRainman
struct ForbiddenPath
{
	Util::SysPaths m_pathId;
	const char* m_descr;
};
#ifdef _WIN32
static const ForbiddenPath g_forbiddenPaths[] =
{
	{Util::WINDOWS, "CSIDL_WINDOWS"},
	{Util::PROGRAM_FILES, "PROGRAM_FILES"},
	{Util::PROGRAM_FILESX86, "PROGRAM_FILESX86"},
	{Util::APPDATA, "APPDATA"},
	{Util::LOCAL_APPDATA, "LOCAL_APPDATA"},
	{Util::COMMON_APPDATA, "COMMON_APPDATA"},
};
#endif
// [~] IRainman
void ShareManager::addDirectory(const string& realPath, const string& virtualName)
{
	if (realPath.empty() || virtualName.empty())
	{
		throw ShareException(STRING(NO_DIRECTORY_SPECIFIED), realPath);
	}
	
	if (!File::isExist(realPath)) // [5] https://www.box.net/shared/406c1493be1501a9ea61
	{
		throw ShareException(STRING(DIRECTORY_NOT_EXIST), realPath); // [+] IRainman
	}
	
	const string l_realPathWoPS = realPath.substr(0, realPath.size() - 1); // [+] IRainman opt.
	
	if (!checkHidden(l_realPathWoPS))
	{
		throw ShareException(STRING(DIRECTORY_IS_HIDDEN), l_realPathWoPS);
	}
//[+]IRainman
	if (!checkSystem(l_realPathWoPS))
	{
		throw ShareException(STRING(DIRECTORY_IS_SYSTEM), l_realPathWoPS);
	}
	
	if (!checkVirtual(l_realPathWoPS))  // [5] https://www.box.net/shared/a877c0d7ef9f93b93c55  TODO - Запретить выброс исключений из мастера или гасить их в нем.
	{
		throw ShareException(STRING(DIRECTORY_IS_VIRTUAL), l_realPathWoPS);
	}
//[~]IRainman
	const string& l_temp_download_dir = SETTING(TEMP_DOWNLOAD_DIRECTORY);
	if (stricmp(l_temp_download_dir, realPath) == 0)
	{
		throw ShareException(STRING(DONT_SHARE_TEMP_DIRECTORY), l_realPathWoPS);
	}
	
#ifdef _WIN32
	// [!] FlylinkDC.
	for (size_t i = 0; i < _countof(g_forbiddenPaths); ++i)
	{
		if (Util::locatedInSysPath(g_forbiddenPaths[i].m_pathId, realPath))
		{
			string l_error;
			l_error.resize(FULL_MAX_PATH);
			snprintf(&l_error[0], l_error.size(), CSTRING(CHECK_FORBIDDEN_DIR), realPath.c_str(), g_forbiddenPaths[i].m_descr);
			throw ShareException(l_error, realPath); // [14] https://www.box.net/shared/a7b3712835bebeea8a46
			// Wizard - 2012-04-29_06-52-32_Y2577HXWPTRKFMIKQFFIWZACQZ7SL7WCXWKKVPQ_9A7CA00D_crash-stack-r502-beta23-build-9860.dmp
			// Wizard - 2012-05-03_22-00-59_Z6RGCCG4JZVMG24NSQTF7MM3FB55VKJEMCXFO5I_00595FCE_crash-stack-r502-beta24-build-9900.dmp
		}
	}
	// [~] FlylinkDC.
#endif
	
	list<string> removeList;
	{
		StringMap a;
		{
			SharedLock l(cs);
			a = shares;
		}
		
		for (auto i = a.cbegin(); i != a.cend(); ++i)
		{
			if (
			    strnicmp(realPath, i->first, i->first.length()) == 0 || // Trying to share an already shared directory
			    strnicmp(realPath, i->first, realPath.length()) == 0    // Trying to share a parent directory
			)
			{
				removeList.push_front(i->first);
			}
		}
	}
	
	for (auto i = removeList.cbegin(); i != removeList.cend(); ++i)
	{
		removeDirectory(*i);
	}
	
	HashManager::HashPauser pauser;
	Directory::Ptr dp = buildTree(realPath, Directory::Ptr(), true);
	
	const string vName = validateVirtual(virtualName);
	dp->setName(vName);
	
	{
		UniqueLock l(cs);
		
		shares.insert(std::make_pair(realPath, vName));
		updateIndices(*merge(dp));
		
		setDirty();
	}
}

ShareManager::Directory::Ptr ShareManager::merge(const Directory::Ptr& directory)
{
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		if (stricmp((*i)->getName(), directory->getName()) == 0)
		{
			dcdebug("Merging directory %s\n", directory->getName().c_str());
			(*i)->merge(directory);
			return *i;
		}
	}
	
	dcdebug("Adding new directory %s\n", directory->getName().c_str());
	
	directories.push_back(directory);
	return directory;
}

void ShareManager::Directory::merge(const Directory::Ptr& source)
{
	for (auto i = source->directories.cbegin(); i != source->directories.cend(); ++i)
	{
		Directory::Ptr subSource = i->second;
		
		MapIter ti = directories.find(subSource->getName());
		if (ti == directories.end())
		{
			if (findFile(subSource->getName()) != files.end())
			{
				dcdebug("File named the same as directory");
			}
			else
			{
				directories.insert(std::make_pair(subSource->getName(), subSource));
				subSource->parent = this;
			}
		}
		else
		{
			Directory::Ptr subTarget = ti->second;
			subTarget->merge(subSource);
		}
	}
	
	// All subdirs either deleted or moved to target...
	source->directories.clear();
	
	for (auto i = source->files.cbegin(); i != source->files.cend(); ++i)
	{
		if (findFile(i->getName()) == files.end())
		{
			if (directories.find(i->getName()) != directories.end())
			{
				dcdebug("Directory named the same as file");
			}
			else
			{
				std::pair<File::Set::iterator, bool> added = files.insert(*i);
				if (added.second)
				{
					const_cast<File&>(*added.first).setParent(this);
				}
			}
		}
	}
}

void ShareManager::removeDirectory(const string& realPath)
{
	if (realPath.empty())
		return;
		
	HashManager::getInstance()->stopHashing(realPath);
	
	UniqueLock l(cs);
	
	auto i = shares.find(realPath);
	if (i == shares.end())
	{
		return;
	}
	
	const string l_Name = i->second; // Ссылку не делать. fix http://www.flickr.com/photos/96019675@N02/9515345001/
	for (auto j = directories.cbegin(); j != directories.cend();)
	{
		if (stricmp((*j)->getName(), l_Name) == 0)
		{
			directories.erase(j++);
		}
		else
		{
			++j;
		}
	}
	
	shares.erase(i);
	
	HashManager::HashPauser pauser;
	
	// Readd all directories with the same vName
	for (i = shares.begin(); i != shares.end(); ++i)
	{
		if (stricmp(i->second, l_Name) == 0 && checkAttributs(i->first))// [!]IRainman checkHidden(i->first)
		{
			Directory::Ptr dp = buildTree(i->first, 0, true);
			dp->setName(i->second);
			merge(dp);
		}
	}
	rebuildIndices();
	setDirty();
}

void ShareManager::renameDirectory(const string& realPath, const string& virtualName)
{
	removeDirectory(realPath);
	addDirectory(realPath, virtualName);
}

ShareManager::DirList::const_iterator ShareManager::getByVirtual(const string& virtualName) const noexcept
{
    for (auto i = directories.cbegin(); i != directories.cend(); ++i)
{
if (stricmp((*i)->getName(), virtualName) == 0)
	{
		return i;
	}
}
return directories.end();
}

int64_t ShareManager::getShareSize(const string& realPath) const noexcept
{
    dcassert(!isShutdown());
    dcassert(!realPath.empty());
    SharedLock l(cs);
    const StringMap::const_iterator i = shares.find(realPath);

    if (i != shares.end())
{
const DirList::const_iterator j = getByVirtual(i->second);
	if (j != directories.end())
	{
		return (*j)->getSize();
	}
}
return -1;
}

void ShareManager::internal_calcShareSize() noexcept // [!] IRainman opt.
{
	if (m_isNeedsUpdateShareSize)
	{
		dcassert(!isShutdown());
#ifndef FLYLINKDC_HE
		if (!isShutdown()) // fix https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=37756
#endif
		{
			m_isNeedsUpdateShareSize = false;
			int64_t l_CurrentShareSize = 0;
			{
				SharedLock l(cs);
				for (auto i = tthIndex.cbegin(); i != tthIndex.cend(); ++i)
				{
					l_CurrentShareSize += i->second->getSize();
				}
			}
			m_CurrentShareSize = l_CurrentShareSize;
		}
	}
}

ShareManager::Directory::Ptr ShareManager::buildTree(const string& aName, const Directory::Ptr& aParent, bool p_is_job)
{
	__int64 l_path_id = CFlylinkDBManager::getInstance()->get_path_id(Text::toLower(aName), !p_is_job, false);
	Directory::Ptr dir = Directory::create(Util::getLastDir(aName), aParent);
	
	Directory::File::Set::iterator lastFileIter = dir->files.begin();
	
	CFlyDirMap l_dir_map;
	if (l_path_id)
		CFlylinkDBManager::getInstance()->LoadDir(l_path_id, l_dir_map);
#ifdef _WIN32
	for (FileFindIter i(aName + '*'); !isShutdown() && i != FileFindIter::end; ++i)// [!]IRainman add m_close [10] https://www.box.net/shared/067924cecdb252c9d26c
	{
#else
	//the fileiter just searches directorys for now, not sure if more
	//will be needed later
	//for(FileFindIter i(aName + '*'); i != end; ++i) {
	for (FileFindIter i(aName); !isShutdown() && i != FileFindIter::end; ++i)// [!]IRainman add m_close
	{
#endif
		if (i->isTemporary())// [+]IRainman
			continue;
		const string name = i->getFileName();
		if (name.empty())
		{
			// TODO: LogManager::getInstance()->message(str(F_("Invalid file name found while hashing folder %1%") % Util::addBrackets(aName)));
			continue;
		}
		if (name == Util::m_dot || name == Util::m_dot_dot)
			continue;
		if (i->isHidden() && !BOOLSETTING(SHARE_HIDDEN))
			continue;
		// [+]IRainman
		if (i->isSystem() && !BOOLSETTING(SHARE_SYSTEM))
			continue;
			
		if (i->isVirtual() && !BOOLSETTING(SHARE_VIRTUAL))
			continue;
		// [~]IRainman
		// [+] Flylink
		const string l_lower_name = Text::toLower(name);
		if (!skipListEmpty())
		{
			if (isInSkipList(l_lower_name))
			{
				LogManager::getInstance()->message(STRING(USER_DENIED_SHARE_THIS_FILE) + ' ' + name
				                                   + " (" + STRING(SIZE) + ": " + Util::toString(i->getSize()) + ' '
				                                   + STRING(B) + ") (" + STRING(DIRECTORY) + ": \"" + aName + "\")");
				continue;
			}
		}
		// [~] Flylink
		if (i->isDirectory())
		{
			const string newName = aName + name + PATH_SEPARATOR;
			
#ifdef _WIN32
			// don't share Windows directory
			if (Util::locatedInSysPath(Util::WINDOWS, newName))
				continue;
			//[+]IRainman don't share any AppData directory
			if (Util::locatedInSysPath(Util::APPDATA, newName))
				continue;
			if (Util::locatedInSysPath(Util::LOCAL_APPDATA, newName))
				continue;
			//[~]IRainman
			
			//[-]IRainman Not safe to do downloads folder in "Program Files" folder
			//if (::strstr(newName.c_str(), SETTING(DOWNLOAD_DIRECTORY).c_str()) == NULL)
			//{
			if (Util::locatedInSysPath(Util::PROGRAM_FILES, newName)) //[+]PPA
				continue;
			if (Util::locatedInSysPath(Util::PROGRAM_FILESX86, newName))//[+]IRainman
				continue;
			//}
#endif
			if (stricmp(newName, SETTING(TEMP_DOWNLOAD_DIRECTORY)) != 0
			        && stricmp(newName, Util::getConfigPath())  != 0//[+]IRainman
			        && stricmp(newName, SETTING(LOG_DIRECTORY)) != 0//[+]IRainman
			        && isShareFolder(newName))
			{
				dir->directories[name] = buildTree(newName, dir, p_is_job);
			}
		}
		else
		{
			// Not a directory, assume it's a file...make sure we're not sharing the settings file...
			if (!g_fly_server_config.isBlockShare(l_lower_name))
			{
				const string l_fileName = aName + name;
				if (stricmp(l_fileName, SETTING(TLS_PRIVATE_KEY_FILE)) == 0)
					continue;
				int64_t size = i->getSize();
				const uint32_t l_ts = i->getLastWriteTime();
				CFlyDirMap::iterator l_dir_item = l_dir_map.find(l_lower_name);
				bool l_is_new_file = l_dir_item == l_dir_map.end();
				if (!l_is_new_file)
					l_is_new_file = l_dir_item->second.m_size != size ||
					                l_dir_item->second.m_TimeStamp != l_ts;
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
				if (l_dir_item != l_dir_map.end())
					l_dir_item_second.m_found = true;
#endif
					
				try
				{
					if (l_is_new_file)
						HashManager::getInstance()->hashFile(l_fileName, size);
					else
					{
						const auto &l_dir_item_second = l_dir_item->second; // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'l_dir_item->second' expression repeatedly. sharemanager.cpp 1054
						lastFileIter = dir->files.insert(lastFileIter, // TODO Lock ? IRainman: is its local object, no needs lock here. 2012-05-03_22-05-14_XJDKOP2ZR6YVPFGO63FKZZH2VAO3IRCNQZNIM3A_DD5D5605_crash-stack-r502-beta24-x64-build-9900.dmp
						                                 Directory::File(name,
						                                                 l_dir_item_second.m_size,
						                                                 dir,
						                                                 l_dir_item_second.m_tth,
						                                                 l_dir_item_second.m_hit,
						                                                 uint32_t(l_dir_item_second.m_StampShare),
						                                                 SearchManager::TypeModes(l_dir_item_second.m_ftype),
						                                                 l_dir_item_second.m_media
						                                                )
						                                );
					}
				}
				catch (const HashException&)
				{
				}
			}
		}
	}
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
	if (l_path_id && !m_sweep_guard)
		CFlylinkDBManager::getInstance()->SweepFiles(l_path_id, l_dir_map);
#endif
	return dir;
}

bool ShareManager::checkHidden(const string& aName) const
{
	if (BOOLSETTING(SHARE_HIDDEN))
		return true;
		
	const FileFindIter ff(aName);
	
	if (ff != FileFindIter::end)
	{
		return !ff->isHidden();
	}
	
	return true;
}
//[+]IRainman
bool ShareManager::checkSystem(const string& aName) const
{
	if (BOOLSETTING(SHARE_SYSTEM))
		return true;
		
	const FileFindIter ff(aName);
	
	if (ff != FileFindIter::end)
	{
		return !ff->isSystem();
	}
	
	return true;
}

bool ShareManager::checkVirtual(const string& aName) const
{
	if (BOOLSETTING(SHARE_VIRTUAL))
		return true;
		
	const FileFindIter ff(aName);
	
	if (ff != FileFindIter::end)
	{
		return !ff->isVirtual();
	}
	
	return true;
}

bool ShareManager::checkAttributs(const string& aName) const
{
	if (BOOLSETTING(SHARE_HIDDEN) && BOOLSETTING(SHARE_SYSTEM) && BOOLSETTING(SHARE_VIRTUAL))
		return true;
		
	const FileFindIter ff(aName.substr(0, aName.size() - 1));
	
	if (ff != FileFindIter::end)
	{
		if (!BOOLSETTING(SHARE_HIDDEN) && ff->isHidden())
			return false;
			
		if (!BOOLSETTING(SHARE_SYSTEM) && ff->isSystem())
			return false;
			
		if (!BOOLSETTING(SHARE_VIRTUAL) && ff->isVirtual())
			return false;
	}
	return true;
}
//[~]IRainman
#ifdef USE_REBUILD_MEDIAINFO
__int64 ShareManager::rebuildMediainfo(Directory& p_dir, CFlyLog& p_log, ShareManager::MediainfoFileArray& p_result)
{
	__int64 l_count = 0;
	for (auto i = p_dir.directories.cbegin(); i != p_dir.directories.cend(); ++i)
	{
		l_count += rebuildMediainfo(*i->second, p_log, p_result);
	}
	__int64 l_path_id = 0;
	string l_real_path;
	for (auto i = p_dir.files.cbegin(); i != p_dir.files.cend(); ++i)
	{
		const Directory::File& l_file = *i;
		if (g_fly_server_config.isMediainfoExt(Util::getFileExtWithoutDot(l_file.getLowName()))) // [!] IRainman opt.
		{
			if (l_path_id == 0)
			{
				l_real_path = Util::getFilePath(l_file.getRealPath());
				l_path_id = CFlylinkDBManager::getInstance()->get_path_id(l_real_path, false, true);
			}
			dcassert(l_path_id);
			dcassert(!l_real_path.empty());
			MediainfoFile l_cur_item; // (l_real_path,l_name, l_file.getSize(), l_path_id, l_file.getTTH());
			l_cur_item.m_path = l_real_path;
			l_cur_item.m_file_name = l_file.getName();
			l_cur_item.m_size = l_file.getSize();
			l_cur_item.m_path_id = l_path_id;
			l_cur_item.m_tth = l_file.getTTH();
			p_result.push_back(l_cur_item);
			++l_count;
		}
	}
	return l_count;
}
__int64 ShareManager::rebuildMediainfo(CFlyLog& p_log)
{
	__int64 l_count = 0;
	ShareManager::MediainfoFileArray l_result;
	p_log.step("Find mediainfo files...");
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		rebuildMediainfo(**i, p_log, l_result);
	}
	p_log.step("l_result.size() = " + Util::toString(l_result.size()));
	__int64 l_last_path_id = 0;
	__int64 l_dir_count = 0;
	for (auto j = l_result.cbegin(); j != l_result.cend(); ++j)
	{
		if (g_fly_server_config.isMediainfoExt(Text::toLower(Util::getFileExtWithoutDot(j->m_file_name))))
		{
			CFlyMediaInfo l_out_media;
			if (HashManager::getInstance()->getMediaInfo(j->m_path + j->m_file_name, l_out_media, j->m_size, j->m_tth, true))
			{
				if (l_out_media.isMedia())
				{
					CFlylinkDBManager::getInstance()->rebuild_mediainfo(j->m_path_id, j->m_file_name, l_out_media, j->m_tth);
					++l_count;
				}
				else
					p_log.step(j->m_path + j->m_file_name + " l_out_media.isMedia() == false");
			}
			if (l_last_path_id != j->m_path_id)
			{
				if (l_dir_count++)
					p_log.step(" Scan: " + j->m_path + " count : " + Util::toString(l_dir_count));
				l_last_path_id = j->m_path_id;
			}
		}
	}
	return l_result.size();
}
#endif // USE_REBUILD_MEDIAINFO

void ShareManager::updateIndices(Directory& dir)
{
	m_bloom.add(Text::toLower(dir.getName()));
	
	for (auto i = dir.directories.cbegin(); i != dir.directories.cend(); ++i)
	{
		updateIndices(*i->second);
	}
	
	dir.size = 0;
	
	for (auto i = dir.files.cbegin(); i != dir.files.cend();)
	{
		updateIndices(dir, i++);
	}
}

void ShareManager::rebuildIndices()
{
	sharedSize = 0;
	m_isNeedsUpdateShareSize = true;
	tthIndex.clear();
	m_bloom.clear();
	
	for (auto i = directories.cbegin(); i != directories.cend(); ++i)
	{
		updateIndices(**i);
	}
}

void ShareManager::updateIndices(Directory& dir, const Directory::File::Set::iterator& i)
{
	const Directory::File& f = *i;
	
	const auto& j = tthIndex.find(f.getTTH());
	if (j == tthIndex.end())
	{
		dir.size += f.getSize();
		sharedSize += f.getSize();
	}
	
	dir.addType(f.getFType()); //[+]PPA TODO crash
	
	tthIndex.insert(make_pair(f.getTTH(), i)); // 2012-04-29_13-38-26_FJD54L2CVBIRAGGGJAFAAHRQDRQD7CXLLPIKIKY_4037A8DC_crash-stack-r501-build-9869.dmp
	m_isNeedsUpdateShareSize = true;
	m_bloom.add(Text::toLower(f.getName()));
	
#ifdef STRONG_USE_DHT
	dht::IndexManager* im = dht::IndexManager::getInstance();
	if (im->isTimeForPublishing())
		im->publishFile(f.getTTH(), f.getSize());
#endif
}

void ShareManager::refresh(bool dirs /* = false */, bool aUpdate /* = true */, bool block /* = false */) noexcept
{
	if (refreshing.test_and_set())
	{
		LogManager::getInstance()->message(STRING(FILE_LIST_REFRESH_IN_PROGRESS));
		return;
	}
	
	update = aUpdate;
	refreshDirs = dirs;
	join();
	bool cached;
	if (initial)
	{
		cached = loadCache();
		initial = false;
	}
	else
	{
		cached = false;
	}
	try
	{
		start(0);
		if (block && !cached)
		{
			join();
		}
		else
		{
			setThreadPriority(Thread::LOW);
		}
	}
	catch (const ThreadException& e)
	{
		LogManager::getInstance()->message(STRING(FILE_LIST_REFRESH_FAILED) + ' ' + e.getError());
	}
}

StringPairList ShareManager::getDirectories() const noexcept
{
    StringPairList ret;
    SharedLock l(cs);
    for (auto i = shares.cbegin(); i != shares.cend(); ++i)
{
ret.push_back(make_pair(i->second, i->first));
}
return ret;
}

int ShareManager::run()
{
	setThreadPriority(Thread::LOW); // [+] IRainman fix.
	
	StringPairList dirs = getDirectories();
	// Don't need to refresh if no directories are shared
	if (dirs.empty())
		refreshDirs = false;
		
	if (refreshDirs)
	{
		HashManager::HashPauser pauser;
		
		LogManager::getInstance()->message(STRING(FILE_LIST_REFRESH_INITIATED));
		sharedSize = 0;
		lastFullUpdate = GET_TICK();
		
		DirList newDirs;
		CFlylinkDBManager::getInstance()->LoadPathCache();
		for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
		{
			if (checkAttributs(i->second))//[!]IRainman checkHidden(i->second)
			{
				Directory::Ptr dp = buildTree(i->second, Directory::Ptr(), false);
				dp->setName(i->first);
				newDirs.push_back(dp);
			}
		}
		if (m_sweep_path)
		{
			m_sweep_path = false;
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
			m_sweep_guard = true;
#endif
			CFlylinkDBManager::getInstance()->SweepPath();
		}
		
		{
			UniqueLock l(cs);
			directories.clear();
			
			for (auto i = newDirs.cbegin(); i != newDirs.cend(); ++i)
			{
				merge(*i);
			}
			
			rebuildIndices();
		}
		refreshDirs = false;
		
		LogManager::getInstance()->message(STRING(FILE_LIST_REFRESH_FINISHED));
	}
	
	if (update)
	{
		ClientManager::getInstance()->infoUpdated();
	}
	
#ifdef STRONG_USE_DHT
	dht::IndexManager* im = dht::IndexManager::getInstance();
	if (im->isTimeForPublishing())
		im->setNextPublishing();
#endif
		
	refreshing.clear();
	return 0;
}

void ShareManager::getBloom(ByteVector& v, size_t k, size_t m, size_t h) const
{
	dcdebug("Creating bloom filter, k=%u, m=%u, h=%u\n", k, m, h);
	// Lock l(cs); [-] IRainman opt.
	
	HashBloom bloom;
	bloom.reset(k, m, h);
	{
		SharedLock l(cs); // [+] IRainman opt.
		for (auto i = tthIndex.cbegin(); i != tthIndex.cend(); ++i)
		{
			bloom.add(i->first);
		}
	}
	bloom.copy_to(v);
}

void ShareManager::generateXmlList()
{
	if (updateXmlListInProcess.test_and_set()) // [+] IRainman opt.
		return;
		
	// Lock l(cs); [-] IRainman opt.
	if (forceXmlRefresh || (xmlDirty && (lastXmlUpdate + 15 * 60 * 1000 < GET_TICK() || lastXmlUpdate < lastFullUpdate)))
	{
		CFlyLog l_creation_log("[Share cache creator]");
		listN++;
		
		try
		{
			string tmp2;
			string indent;
			
			string newXmlName = Util::getConfigPath() + "files" + Util::toString(listN) + ".xml.bz2";
			{
				File f(newXmlName, File::WRITE, File::TRUNCATE | File::CREATE);
				l_creation_log.step("open file done");
				// We don't care about the leaves...
				CalcOutputStream<TTFilter, false> bzTree(&f);
				FilteredOutputStream<BZFilter, false> bzipper(&bzTree);
				CountOutputStream<false> count(&bzipper);
				CalcOutputStream<TTFilter, false> newXmlFile(&count);
				l_creation_log.step("init packer done");
				newXmlFile.write(SimpleXML::utf8Header);
				newXmlFile.write("<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"/\" Generator=\"DC++ " DCVERSIONSTRING "\">\r\n"); // [!] IRainman fix.
				{
					SharedLock l(cs); // [+] IRainman opt.
					for (auto i = directories.cbegin(); i != directories.cend(); ++i)
					{
						(*i)->toXml(newXmlFile, indent, tmp2, true); // https://www.box.net/shared/e9d04cfcc59d4a4aaba7
					}
				}
				l_creation_log.step("write dir. done");
				newXmlFile.write("</FileListing>");
				newXmlFile.flush();
				l_creation_log.step("close file");
				
				xmlListLen = count.getCount();
				
				newXmlFile.getFilter().getTree().finalize();
				bzTree.getFilter().getTree().finalize();
				
				xmlRoot = newXmlFile.getFilter().getTree().getRoot();
				bzXmlRoot = bzTree.getFilter().getTree().getRoot();
			}
			
			const string l_XmlListFileName = getDefaultBZXmlFile(); // [+] IRainman
			
			if (bzXmlRef.get())
			{
				bzXmlRef.reset();
				// [!] IRainman fix: don't delete this file
				// [-] File::deleteFile(getBZXmlFile());
			}
			
			l_creation_log.step("set new file as cache");
			try
			{
				File::renameFile(newXmlName, l_XmlListFileName);
				newXmlName = l_XmlListFileName;
			}
			catch (const FileException&)
			{
				// Ignore, this is for caching only...
			}
			bzXmlRef = unique_ptr<File>(new File(newXmlName, File::READ, File::OPEN));
			setBZXmlFile(newXmlName);
			bzXmlListLen = File::getSize(newXmlName);
		}
		catch (const Exception&)
		{
			// No new file lists...
		}
		
		xmlDirty = false;
		forceXmlRefresh = false;
		lastXmlUpdate = GET_TICK();
		
		// [+] IRainman cleaning old file cache
		l_creation_log.step("clean old cache");
		const StringList& l_ToDelete = File::findFiles(Util::getConfigPath(), "files*.xml.bz2", false);
		for (auto i = l_ToDelete.cbegin(); i != l_ToDelete.cend(); ++i)
		{
			if (*i != getBZXmlFile() && *i != getDefaultBZXmlFile())
				File::deleteFile(Util::getConfigPath() + *i);
		}
		// [~] IRainman cleaning old file cache
	}
	updateXmlListInProcess.clear(); // [+] IRainman opt.
}

MemoryInputStream* ShareManager::generatePartialList(const string& dir, bool recurse
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
                                                     , bool ishidingShare
#endif
                                                    ) const
{
	if (dir[0] != '/' || dir[dir.size() - 1] != '/')
		return 0;
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	if (ishidingShare)
	{
		string xml = SimpleXML::utf8Header;
		string tmp;
		xml += "<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"" + SimpleXML::escape(dir, tmp, false) + "\" Generator=\"DC++ "  + DCVERSIONSTRING + "\">\r\n";
		StringOutputStream sos(xml);
		xml += "</FileListing>";
		return new MemoryInputStream(xml);
	}
#endif
	
	string xml = SimpleXML::utf8Header;
	string tmp;
	xml += "<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"" + SimpleXML::escape(dir, tmp, false) + "\" Generator=\"DC++ " DCVERSIONSTRING "\">\r\n"; // [!] IRainman fix.
	StringOutputStream sos(xml);
	string indent = "\t";
	
	// Lock l(cs); [-] IRainman opt.
	if (dir == "/")
	{
		SharedLock l(cs); // [+] IRainman opt.
		for (auto i = directories.cbegin(); i != directories.cend(); ++i)
		{
			tmp.clear();
			(*i)->toXml(sos, indent, tmp, recurse);
		}
	}
	else
	{
		string::size_type i = 1, j = 1;
		
		Directory::Ptr root;
		
		bool first = true;
		
		SharedLock l(cs); // [+] IRainman opt.
		while ((i = dir.find('/', j)) != string::npos)
		{
			if (i == j)
			{
				j++;
				continue;
			}
			
			if (first)
			{
				first = false;
				DirList::const_iterator it = getByVirtual(dir.substr(j, i - j));
				
				if (it == directories.end())
					return nullptr;
					
				root = *it;
			}
			else
			{
				Directory::Map::const_iterator it2 = root->directories.find(dir.substr(j, i - j));
				if (it2 == root->directories.end())
					return nullptr;
					
				root = it2->second;
			}
			j = i + 1;
		}
		if (!root)
			return 0;
			
		for (auto it2 = root->directories.cbegin(); it2 != root->directories.cend(); ++it2)
		{
			it2->second->toXml(sos, indent, tmp, recurse);
		}
		root->filesToXml(sos, indent, tmp);
	}
	
	xml += "</FileListing>";
	return new MemoryInputStream(xml);
}

#define LITERAL(n) n, sizeof(n)-1
void ShareManager::Directory::toXml(OutputStream& xmlFile, string& p_indent, string& tmp2, bool fullList) const
{
	if (!p_indent.empty())
		xmlFile.write(p_indent);
	xmlFile.write(LITERAL("<Directory Name=\""));
	xmlFile.write(SimpleXML::escape(getName(), tmp2, true)); // [2] https://www.box.net/shared/d7edf91f36b1af66b8f7
	
	if (fullList)
	{
		xmlFile.write(LITERAL("\">\r\n"));
		
		p_indent += '\t';
		for (auto i = directories.cbegin(); i != directories.cend(); ++i)
		{
			i->second->toXml(xmlFile, p_indent, tmp2, fullList);
		}
		
		filesToXml(xmlFile, p_indent, tmp2); // [?] https://www.box.net/shared/39f69aa184eea69d2087
		
		// dcassert(p_indent.length() > 1);
		if (p_indent.length() > 1)
		{
			p_indent.erase(p_indent.length() - 1);
		}
		if (!p_indent.empty())
		{
			xmlFile.write(p_indent);
		}
		xmlFile.write(LITERAL("</Directory>\r\n"));
	}
	else
	{
		if (directories.empty() && files.empty())
		{
			xmlFile.write(LITERAL("\" />\r\n"));
		}
		else
		{
			xmlFile.write(LITERAL("\" Incomplete=\"1\" />\r\n"));
		}
	}
}

void ShareManager::Directory::filesToXml(OutputStream& xmlFile, string& indent, string& tmp2) const
{
	for (auto i = files.cbegin(); i != files.cend(); ++i)
	{
		const Directory::File& f = *i;
		
		xmlFile.write(indent);
		xmlFile.write(LITERAL("<File Name=\""));
		xmlFile.write(SimpleXML::escape(f.getName(), tmp2, true));
		xmlFile.write(LITERAL("\" Size=\""));
		xmlFile.write(Util::toString(f.getSize()));
		xmlFile.write(LITERAL("\" TTH=\""));
		tmp2.clear();
		xmlFile.write(f.getTTH().toBase32(tmp2));
		if (f.getHit())
		{
			xmlFile.write(LITERAL("\" HIT=\""));
			xmlFile.write(Util::toString(f.getHit()));
		}
		xmlFile.write(LITERAL("\" TS=\""));
		xmlFile.write(Util::toString(f.getTS()));
		if (f.m_media.m_bitrate)
		{
			xmlFile.write(LITERAL("\" BR=\""));
			xmlFile.write(Util::toString(f.m_media.m_bitrate));
		}
		if (f.m_media.m_mediaX && f.m_media.m_mediaY)
		{
			xmlFile.write(LITERAL("\" WH=\""));
			xmlFile.write(f.m_media.getXY());
		}
		
		if (!f.m_media.m_audio.empty())
		{
			xmlFile.write(LITERAL("\" MA=\""));
			if (SimpleXML::needsEscapeForce(f.m_media.m_audio))
				xmlFile.write(SimpleXML::escapeForce(f.m_media.m_audio, tmp2));
			else
				xmlFile.write(f.m_media.m_audio);
		}
		if (!f.m_media.m_video.empty())
		{
			xmlFile.write(LITERAL("\" MV=\""));
			if (SimpleXML::needsEscapeForce(f.m_media.m_video))
				xmlFile.write(SimpleXML::escapeForce(f.m_media.m_video, tmp2));
			else
				xmlFile.write(f.m_media.m_video);
		}
		
		xmlFile.write(LITERAL("\"/>\r\n"));
	}
}

// These ones we can look up as ints (4 bytes...)...

static const char* typeAudio[] = { ".mp3", ".mp2", ".mid", ".wav", ".ogg", ".wma", ".669", ".aac", ".aif", ".amf", ".ams", ".ape", ".dbm", ".dsm", ".far", ".mdl", ".med", ".mod", ".mol", ".mp1", ".mpa", ".mpc", ".mpp", ".mtm", ".nst", ".okt", ".psm", ".ptm", ".rmi", ".s3m", ".stm", ".ult", ".umx", ".wow",
                                   ".lqt", // [+] FlylinkDC++
                                   ".vqf", ".m4a"
                                 };
static const char* typeCompressed[] = { ".rar", ".zip", ".ace", ".arj", ".hqx", ".lha", ".sea", ".tar", ".tgz", ".uc2", ".bz2", ".lzh" };
static const char* typeDocument[] = { ".htm", ".doc", ".txt", ".nfo", ".pdf", ".chm",
                                      ".rtf", // [+] FlylinkDC++
                                      ".xls",
                                      ".ppt",
                                      ".odt",
                                      ".ods",
                                      ".odf",
                                      ".odp",
                                      ".xml",
                                      ".xps"
                                    };
static const char* typeExecutable[] = { ".exe", ".com",
                                        ".msi", //[+]PPA
                                        ".app", ".bat", ".cmd", ".jar", ".ps1", ".vbs", ".wsf"
                                      };
static const char* typePicture[] = { ".jpg", ".gif", ".png", ".eps", ".img", ".pct", ".psp", ".pic", ".tif", ".rle", ".bmp", ".pcx", ".jpe", ".dcx", ".emf", ".ico", ".psd", ".tga", ".wmf", ".xif", ".cdr", ".sfw" };
static const char* typeVideo[] = { ".avi", ".mpg", ".mov", ".flv", ".asf",  ".pxp", ".wmv", ".ogm", ".mkv", ".m1v", ".m2v", ".mpe", ".mps", ".mpv", ".ram", ".vob", ".mp4", ".3gp", ".asx", ".swf",
                                   ".sub", ".srt", ".ass", ".ssa" // [+] IRainman sub. types.
                                 };
static const char* typeCDImage[] = { ".iso", ".nrg", ".mdf", ".mds", ".vcd", ".bwt", ".ccd", ".cdi", ".pdi", ".cue", ".isz", ".img", ".vc4", ".cso"};  // [+] FlylinkDC++

static const string type2Audio[] = { ".au", ".it", ".ra", ".xm", ".aiff", ".flac", ".midi"};
static const string type2Document[] = {".xlsx", ".docx", ".pptx", ".html" }; //[+]Drakon (Office 2007)
static const string type2Compressed[] = { ".7z", //[+]PPA
                                          ".gz", ".tz", ".z"
                                        };
static const string type2Picture[] = { ".ai", ".ps", ".pict", ".jpeg", ".tiff", ".webp" };
static const string type2Video[] = { ".rm", ".divx", ".mpeg", ".mp1v", ".mp2v", ".mpv1", ".mpv2", ".qt", ".rv", ".vivo", ".rmvb", ".webm",
                                     ".ts", ".ps" // Transport stream and Packages stream MPEG-2 file
                                   };

#define IS_TYPE(x)  type == (*((uint32_t*)x))
#define IS_TYPE2(x) stricmp(aString.c_str() + aString.length() - x.length(), x.c_str()) == 0

#define DEBUG_CHECK_TYPE(x)  for (size_t i = 0; i < _countof(x); i++) \
	{ \
		dcassert(Text::isAscii(x[i])); \
		dcassert(x[i][0] == '.' && strlen(x[i]) == 4); \
	}
#define DEBUG_CHECK_TYPE2(x)  for (size_t i = 0; i < _countof(x); i++) \
	{ \
		dcassert(Text::isAscii(x[i])); \
		dcassert(!x[i].empty() && x[i][0] == '.' && x[i].length() != 4); \
	}

static bool checkType(const string& aString, int aType)
{
	if (aType == SearchManager::TYPE_ANY)
		return true;
		
	if (aString.length() < 5)
		return false;
		
	const char* c = aString.c_str() + aString.length() - 3;
	if (!Text::isAscii(c))
		return false;
		
	const uint32_t type = '.' | (Text::asciiToLower(c[0]) << 8) | (Text::asciiToLower(c[1]) << 16) | (((uint32_t)Text::asciiToLower(c[2])) << 24);
	
#ifdef _DEBUG
// Все расширения 1-го типа должны быть с точкой и длиной 4
	DEBUG_CHECK_TYPE(typeAudio)
	DEBUG_CHECK_TYPE(typeCompressed)
	DEBUG_CHECK_TYPE(typeDocument)
	DEBUG_CHECK_TYPE(typeExecutable)
	DEBUG_CHECK_TYPE(typePicture)
	DEBUG_CHECK_TYPE(typeCDImage)
// Все расширения 2-го типа должны быть тоже с точкой но с длиной отличной от 4
	DEBUG_CHECK_TYPE2(type2Audio)
	DEBUG_CHECK_TYPE2(type2Document)
	DEBUG_CHECK_TYPE2(type2Compressed)
	DEBUG_CHECK_TYPE2(type2Picture)
	DEBUG_CHECK_TYPE2(type2Video)
#endif
	switch (aType)
	{
		case SearchManager::TYPE_AUDIO:
		{
			for (size_t i = 0; i < _countof(typeAudio); i++)
			{
				if (IS_TYPE(typeAudio[i]))
				{
					return true;
				}
			}
			for (size_t i = 0; i < _countof(type2Audio); i++)
			{
				if (IS_TYPE2(type2Audio[i]))
				{
					return true;
				}
			}
		}
		break;
		case SearchManager::TYPE_COMPRESSED:
		{
			for (size_t i = 0; i < _countof(typeCompressed); i++)
			{
				if (IS_TYPE(typeCompressed[i]))
				{
					return true;
				}
			}
			for (size_t i = 0; i < _countof(type2Compressed); i++)
			{
				if (IS_TYPE2(type2Compressed[i]))
				{
					return true;
				}
			}
		}
		break;
		case SearchManager::TYPE_DOCUMENT:
			for (size_t i = 0; i < _countof(typeDocument); i++)
			{
				if (IS_TYPE(typeDocument[i]))
				{
					return true;
				}
			}
			for (size_t i = 0; i < _countof(type2Document); i++)
			{
				if (IS_TYPE2(type2Document[i]))
					return true;
			}
			break;
		case SearchManager::TYPE_EXECUTABLE:
			for (size_t i = 0; i < _countof(typeExecutable); i++)
			{
				if (IS_TYPE(typeExecutable[i]))
				{
					return true;
				}
			}
			break;
		case SearchManager::TYPE_PICTURE:
		{
			for (size_t i = 0; i < _countof(typePicture); i++)
			{
				if (IS_TYPE(typePicture[i]))
				{
					return true;
				}
			}
			for (size_t i = 0; i < _countof(type2Picture); i++)
			{
				if (IS_TYPE2(type2Picture[i]))
				{
					return true;
				}
			}
		}
		break;
		case SearchManager::TYPE_VIDEO:
		{
			for (size_t i = 0; i < _countof(typeVideo); i++)
			{
				if (IS_TYPE(typeVideo[i]))
				{
					return true;
				}
			}
			for (size_t i = 0; i < _countof(type2Video); i++)
			{
				if (IS_TYPE2(type2Video[i]))
				{
					return true;
				}
			}
		}
		break;
		//[+] FlylinkDC++
		case SearchManager::TYPE_CD_IMAGE:
		{
			for (size_t i = 0; i < _countof(typeCDImage); ++i)
			{
				if (IS_TYPE(typeCDImage[i]))
				{
					return true;
				}
			}
		}
		break;
		default:
			dcassert(0);
			break;
	}
	return false;
}

SearchManager::TypeModes ShareManager::getFType(const string& aFileName) noexcept
{
	if (aFileName[aFileName.length() - 1] == PATH_SEPARATOR)
	{
		return SearchManager::TYPE_DIRECTORY;
	}
	
	if (checkType(aFileName, SearchManager::TYPE_VIDEO))
		return SearchManager::TYPE_VIDEO;
	else if (checkType(aFileName, SearchManager::TYPE_AUDIO))
		return SearchManager::TYPE_AUDIO;
	else if (checkType(aFileName, SearchManager::TYPE_COMPRESSED))
		return SearchManager::TYPE_COMPRESSED;
	else if (checkType(aFileName, SearchManager::TYPE_DOCUMENT))
		return SearchManager::TYPE_DOCUMENT;
	else if (checkType(aFileName, SearchManager::TYPE_EXECUTABLE))
		return SearchManager::TYPE_EXECUTABLE;
	else if (checkType(aFileName, SearchManager::TYPE_PICTURE))
		return SearchManager::TYPE_PICTURE;
	else if (checkType(aFileName, SearchManager::TYPE_CD_IMAGE)) //[+] from FlylinkDC++
		return SearchManager::TYPE_CD_IMAGE;
		
	return SearchManager::TYPE_ANY;
}

/**
 * Alright, the main point here is that when searching, a search string is most often found in
 * the filename, not directory name, so we want to make that case faster. Also, we want to
 * avoid changing StringLists unless we absolutely have to --> this should only be done if a string
 * has been matched in the directory name. This new stringlist should also be used in all descendants,
 * but not the parents...
 */
void ShareManager::Directory::search(SearchResultList& aResults, StringSearch::List& aStrings, Search::SizeModes aSizeMode, int64_t aSize, int aFileType, Client* aClient, StringList::size_type maxResults) const noexcept
{
    // Skip everything if there's nothing to find here (doh! =)
    if (!hasType(aFileType))
    return;
    
    StringSearch::List* cur = &aStrings;
    unique_ptr<StringSearch::List> newStr;
    
    // Find any matches in the directory name
    for (auto k = aStrings.cbegin(); k != aStrings.cend(); ++k)
{
	if (k->matchLower(getLowName())) // http://flylinkdc.blogspot.com/2010/08/1.html
		{
			if (!newStr.get())
			{
				newStr = unique_ptr<StringSearch::List>(new StringSearch::List(aStrings));
			}
			newStr->erase(remove(newStr->begin(), newStr->end(), *k), newStr->end());
		}
	}

if (newStr.get() != 0)
{
cur = newStr.get();
}

#ifdef _DEBUG
//	char l_buf[1000] = {0};
//	sprintf(l_buf, "Name = %s, limit = %lld m_sizeMode = %d\r\n", getFullName().c_str(), aSize,  aSizeMode);
//	LogManager::getInstance()->message(l_buf);
#endif
const bool sizeOk = (aSizeMode != Search::SIZE_ATLEAST) || (aSize == 0);
if ((cur->empty()) &&
        (((aFileType == SearchManager::TYPE_ANY) && sizeOk) || (aFileType == SearchManager::TYPE_DIRECTORY)))
{
// We satisfied all the search words! Add the directory...(NMDC searches don't support directory size)
SearchResultPtr sr(new SearchResult(SearchResult::TYPE_DIRECTORY, 0, getFullName(), TTHValue()));
	aResults.push_back(sr);
	ShareManager::incHits();
}

if (aFileType != SearchManager::TYPE_DIRECTORY)
{
for (auto i = files.cbegin(); i != files.cend(); ++i)
	{
	
#ifdef _DEBUG
//		char l_buf[1000] = {0};
//		sprintf(l_buf, "Name = %s, i->getSize() = %lld aSize =  %lld m_sizeMode = %d\r\n", getFullName().c_str(), i->getSize(), aSize,  aSizeMode);
//		LogManager::getInstance()->message(l_buf);
#endif
		if (aSizeMode == Search::SIZE_ATLEAST && aSize > i->getSize())
		{
#ifdef _DEBUG
//			LogManager::getInstance()->message("[минимум] aSizeMode == Search::SIZE_ATLEAST && aSize > i->getSize()");
#endif
			continue;
		}
		else if (aSizeMode == Search::SIZE_ATMOST && aSize < i->getSize())
		{
#ifdef _DEBUG
//			LogManager::getInstance()->message("[максимум] aSizeMode == Search::SIZE_ATMOST && aSize < i->getSize()");
#endif
			continue;
		}
		StringSearch::List::const_iterator j = cur->begin();
		for (; j != cur->end() && j->matchLower(i->getLowName()); ++j) // http://flylinkdc.blogspot.com/2010/08/1.html
			;   // Empty
			
		if (j != cur->end())
			continue;
			
		// Check file type...
		if (checkType(i->getName(), aFileType))
		{
			SearchResultPtr sr(new SearchResult(SearchResult::TYPE_FILE, i->getSize(), getFullName() + i->getName(), i->getTTH()));
			aResults.push_back(sr);
			ShareManager::incHits();
			if (aResults.size() >= maxResults)
			{
				break;
			}
		}
	}
}

for (auto l = directories.cbegin(); l != directories.cend() && aResults.size() < maxResults; ++l)
{
if (l->second)
		l->second->search(aResults, *cur, aSizeMode, aSize, aFileType, aClient, maxResults); //TODO - Hot point
}
}

void ShareManager::search(SearchResultList& results, const string& aString, Search::SizeModes aSizeMode, int64_t aSize, int aFileType, Client* aClient, StringList::size_type maxResults) noexcept
{
	// Lock l(cs); [-] IRainman opt.
	if (aFileType == SearchManager::TYPE_TTH)
	{
		if (isTTHBase64(aString)) //[+]FlylinkDC++ opt.
		{
			TTHValue tth(aString.c_str() + 4);  //[+]FlylinkDC++ opt. //-V112
			SearchResultPtr sr;
			{
				SharedLock l(cs); // [+] IRainman opt.
				const auto& i = tthIndex.find(tth);
				if (i == tthIndex.end() || !i->second->getParent()) // [!] IRainman opt.
					return;
				const auto &l_fileMap = i->second; // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'i->second' expression repeatedly. sharemanager.cpp 2012
				sr = new SearchResult(SearchResult::TYPE_FILE, l_fileMap->getSize(),
				l_fileMap->getParent()->getFullName() + l_fileMap->getName(), l_fileMap->getTTH());
				incHits();
			}
			results.push_back(sr);
		}
		return;
	}
	const StringTokenizer<string> t(Text::toLower(aString), '$'); // 2012-05-03_22-05-14_YNJS7AEGAWCUMRBY2HTUTLYENU4OS2PKNJXT6ZY_F4B220A1_crash-stack-r502-beta24-x64-build-9900.dmp
	const StringList& sl = t.getTokens();
	{
		SharedLock l(cs); // [+] IRainman opt.
		if (!m_bloom.match(sl))
			return;
	}
	
	StringSearch::List ssl;
	ssl.reserve(sl.size());
	for (auto i = sl.cbegin(); i != sl.cend(); ++i)
	{
		if (!i->empty())
		{
			ssl.push_back(StringSearch(*i));
		}
	}
	if (ssl.empty())
		return;
		
	{
		SharedLock l(cs); // [+] IRainman opt.
		for (auto j = directories.cbegin(); j != directories.cend() && results.size() < maxResults; ++j)
		{
			(*j)->search(results, ssl, aSizeMode, aSize, aFileType, aClient, maxResults);
		}
	}
}

inline static uint16_t toCode(char a, char b)
{
	return (uint16_t)a | ((uint16_t)b) << 8;
}

ShareManager::AdcSearch::AdcSearch(const StringList& params) : include(&includeX), gt(0),
	lt(std::numeric_limits<int64_t>::max()), hasRoot(false), isDirectory(false)
{
	for (auto i = params.cbegin(); i != params.cend(); ++i)
	{
		const string& p = *i;
		if (p.length() <= 2)
			continue;
			
		const uint16_t cmd = toCode(p[0], p[1]);
		if (toCode('T', 'R') == cmd)
		{
			hasRoot = true;
			root = TTHValue(p.substr(2));
			return;
		}
		else if (toCode('A', 'N') == cmd)
		{
			includeX.push_back(StringSearch(p.substr(2)));
		}
		else if (toCode('N', 'O') == cmd)
		{
			exclude.push_back(StringSearch(p.substr(2)));
		}
		else if (toCode('E', 'X') == cmd)
		{
			ext.push_back(p.substr(2));
		}
		else if (toCode('G', 'R') == cmd)
		{
			auto exts = AdcHub::parseSearchExts(Util::toInt(p.substr(2)));
			ext.insert(ext.begin(), exts.begin(), exts.end());
		}
		else if (toCode('R', 'X') == cmd)
		{
			noExt.push_back(p.substr(2));
		}
		else if (toCode('G', 'E') == cmd)
		{
			gt = Util::toInt64(p.substr(2));
		}
		else if (toCode('L', 'E') == cmd)
		{
			lt = Util::toInt64(p.substr(2));
		}
		else if (toCode('E', 'Q') == cmd)
		{
			lt = gt = Util::toInt64(p.substr(2));
		}
		else if (toCode('T', 'Y') == cmd)
		{
			isDirectory = (p[2] == '2');
		}
	}
}

bool ShareManager::AdcSearch::isExcluded(const string& str)
{
	for (auto i = exclude.cbegin(); i != exclude.cend(); ++i)
	{
		if (i->match(str))
			return true;
	}
	return false;
}

bool ShareManager::AdcSearch::hasExt(const string& name)
{
	if (ext.empty())
		return true;
	if (!noExt.empty())
	{
		ext = StringList(ext.begin(), set_difference(ext.begin(), ext.end(), noExt.begin(), noExt.end(), ext.begin()));
		noExt.clear();
	}
	for (auto i = ext.cbegin(), iend = ext.cend(); i != iend; ++i)
	{
		if (name.length() >= i->length() && stricmp(name.c_str() + name.length() - i->length(), i->c_str()) == 0)
			return true;
	}
	return false;
}

void ShareManager::Directory::search(SearchResultList& aResults, AdcSearch& aStrings, StringList::size_type maxResults) const noexcept
{
    StringSearch::List* cur = aStrings.include;
    StringSearch::List* old = aStrings.include;

    unique_ptr<StringSearch::List> newStr;

    // Find any matches in the directory name
    for (auto k = cur->cbegin(); k != cur->cend(); ++k)
{
if (k->matchLower(getLowName()) && !aStrings.isExcluded(getName())) // http://flylinkdc.blogspot.com/2010/08/1.html
	{
		if (!newStr.get())
		{
			newStr = unique_ptr<StringSearch::List>(new StringSearch::List(*cur));
		}
		newStr->erase(remove(newStr->begin(), newStr->end(), *k), newStr->end());
	}
}

if (newStr.get() != 0)
{
cur = newStr.get();
}

bool sizeOk = (aStrings.gt == 0);
if (cur->empty() && aStrings.ext.empty() && sizeOk)
{
// We satisfied all the search words! Add the directory...
SearchResultPtr sr(new SearchResult(SearchResult::TYPE_DIRECTORY, getSize(), getFullName(), TTHValue()));
	aResults.push_back(sr);
	ShareManager::incHits();
}

if (!aStrings.isDirectory)
{
for (auto i = files.cbegin(); i != files.cend(); ++i)
	{
	
		if (!(i->getSize() >= aStrings.gt))
		{
			continue;
		}
		else if (!(i->getSize() <= aStrings.lt))
		{
			continue;
		}
		
		if (aStrings.isExcluded(i->getName()))
			continue;
			
		StringSearch::List::const_iterator j = cur->begin();
		for (; j != cur->end() && j->matchLower(i->getLowName()); ++j) // http://flylinkdc.blogspot.com/2010/08/1.html
			;   // Empty
			
		if (j != cur->end())
			continue;
			
		// Check file type...
		if (aStrings.hasExt(i->getName()))
		{
		
			SearchResultPtr sr(new SearchResult(SearchResult::TYPE_FILE,
			                                    i->getSize(), getFullName() + i->getName(), i->getTTH()));
			aResults.push_back(sr);
			ShareManager::incHits();
			if (aResults.size() >= maxResults)
			{
				return;
			}
		}
	}
}

for (auto l = directories.cbegin(); (l != directories.cend()) && (aResults.size() < maxResults); ++l)
{
l->second->search(aResults, aStrings, maxResults);
}
aStrings.include = old;
}

void ShareManager::search(SearchResultList& results, const StringList& params, StringList::size_type maxResults, StringSearch::List& reguest) noexcept // [!] IRainman-S add StringSearch::List& reguest
{
	AdcSearch srch(params);
	reguest = srch.includeX; // [+] IRainman-S
	
	// Lock l(cs); [-] IRainman opt.
	
	if (srch.hasRoot)
	{
		SearchResultPtr sr;
		{
			SharedLock l(cs); // [+] IRainman opt.
			const auto& i = tthIndex.find(srch.root);
			if (i == tthIndex.end())
				return;
			const auto &l_fileMap = i->second; // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'i->second' expression repeatedly. sharemanager.cpp 2240
			sr = new SearchResult(SearchResult::TYPE_FILE,
			l_fileMap->getSize(), l_fileMap->getParent()->getFullName() + l_fileMap->getName(),
			l_fileMap->getTTH());
			
			incHits();
		}
		results.push_back(sr);
		return;
	}
	
	SharedLock l(cs); // [+] IRainman opt.
	
	for (auto i = srch.includeX.cbegin(); i != srch.includeX.cend(); ++i)
	{
		if (!m_bloom.match(i->getPattern()))
			return;
	}
	
	for (auto j = directories.cbegin(); j != directories.cend() && results.size() < maxResults; ++j)
	{
		(*j)->search(results, srch, maxResults);
	}
}

ShareManager::Directory::Ptr ShareManager::getDirectory(const string& fname) const
{
	for (auto mi = shares.cbegin(); mi != shares.cend(); ++mi)
	{
		if (strnicmp(fname, mi->first, mi->first.length()) == 0)
		{
			Directory::Ptr d;
			for (auto i = directories.cbegin(); i != directories.cend(); ++i)
			{
				if (stricmp((*i)->getName(), mi->second) == 0)
				{
					d = *i;
					break; // [+] IRainman opt.
				}
			}
			
			if (!d)
			{
				return Directory::Ptr();
			}
			
			string::size_type i;
			string::size_type j = mi->first.length();
			while ((i = fname.find(PATH_SEPARATOR, j)) != string::npos)
			{
				Directory::MapIter dmi = d->directories.find(fname.substr(j, i - j));
				j = i + 1;
				if (dmi == d->directories.end())
					return Directory::Ptr();
				d = dmi->second;
			}
			return d;
		}
	}
	return Directory::Ptr();
}

void ShareManager::on(QueueManagerListener::FileMoved, const string& n) noexcept
{
#ifdef PPA_INCLUDE_ADD_FINISHED_INSTANTLY
	if (BOOLSETTING(ADD_FINISHED_INSTANTLY))
#endif
	{
		// Check if finished download is supposed to be shared
		/* [-] IRainman opt.
		Lock l(cs);
		
		for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		{
		    if (strnicmp(i->first, n, i->first.size()) == 0 && n[i->first.size() - 1] == PATH_SEPARATOR)
		    {
		*/
		// [+] IRainman opt.
		if (destinationShared(n))
		{
			// [~] IRainman opt.
			try
			{
				// Schedule for hashing, it'll be added automatically later on...
				const string fname = Text::toLower(Util::getFileName(n));
				const string fpath = Text::toLower(Util::getFilePath(n));
				const __int64 l_path_id = CFlylinkDBManager::getInstance()->get_path_id(fpath, true, false);
				TTHValue l_tth;
				HashManager::getInstance()->checkTTH(fname, fpath, l_path_id, File::getSize(n), 0, l_tth);
			}
			catch (const Exception&)
			{
				// Not a vital feature...
			}
			return; // [!] IRainman opt: return.
		}
	}
}
void ShareManager::on(HashManagerListener::TTHDone, const string& fname, const TTHValue& root,
                      int64_t aTimeStamp, const CFlyMediaInfo& p_out_media, int64_t p_size) noexcept
{
	UniqueLock l(cs);
	if (Directory::Ptr d = getDirectory(fname))
	{
		Directory::File::Set::const_iterator i = d->findFile(Util::getFileName(fname));
		if (i != d->files.end())
		{
			if (root != i->getTTH())
				tthIndex.erase(i->getTTH());
			// Get rid of false constness...
			Directory::File* f = const_cast<Directory::File*>(&(*i));
			f->setTTH(root);
			tthIndex.insert(make_pair(f->getTTH(), i));
			m_isNeedsUpdateShareSize = true;
		}
		else
		{
			const string name = Util::getFileName(fname);
			const int64_t size = File::getSize(fname);
			Directory::File::Set::iterator it =
			    d->files.insert(Directory::File(name, size, d, root, 0, uint32_t(aTimeStamp),
			                                    ShareManager::getFType(name),
			                                    p_out_media
			                                   )).first;
			updateIndices(*d, it);
		}
		setDirty();
		forceXmlRefresh = true;
	}
}

void ShareManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	if (SETTING(AUTO_REFRESH_TIME) > 0)
	{
		if (lastFullUpdate + SETTING(AUTO_REFRESH_TIME) * 60 * 1000 < tick)
		{
			refresh(true, true);
		}
	}
	internal_calcShareSize(); // [+]IRainman opt.
}

bool ShareManager::isShareFolder(const string& path, bool thoroughCheck /* = false */) const
{
	dcassert(!path.empty());
	if (thoroughCheck)  // check if it's part of the share before checking if it's in the exclusions
	{
		bool result = false;
		for (auto i = shares.cbegin(); i != shares.cend(); ++i)
		{
			const auto &l_shares = i->first; // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'i->first' expression repeatedly. sharemanager.cpp 2391
			// is it a perfect match
			if (path.size() == l_shares.size() && stricmp(path, l_shares) == 0)
				return true;
			else if (path.size() > l_shares.size()) // this might be a subfolder of a shared folder
			{
				// if the left-hand side matches and there is a \ in the remainder then it is a subfolder
				if (Text::isEqualsSubstringIgnoreCase(path, l_shares) && path.find('\\', l_shares.size()) != string::npos)
				{
					result = true;
					break;
				}
			}
		}
		
		if (!result)
			return false;
	}
	
	// check if it's an excluded folder or a sub folder of an excluded folder
	for (auto j = m_notShared.cbegin(); j != m_notShared.cend(); ++j)
	{
		if (stricmp(path, *j) == 0)
			return false;
			
		if (thoroughCheck)
		{
			if (path.size() > j->size())
			{
				if (Text::isEqualsSubstringIgnoreCase(path, *j) && path[j->size()] == '\\')
					return false;
			}
		}
	}
	return true;
}

int64_t ShareManager::addExcludeFolder(const string &path)
{
	HashManager::getInstance()->stopHashing(path);
	
	// make sure this is a sub folder of a shared folder
	bool result = false;
	for (auto i = shares.cbegin(); i != shares.cend(); ++i)
	{
		if (path.size() > i->first.size())
		{
			if (Text::isEqualsSubstringIgnoreCase(path, i->first) == 0)
			{
				result = true;
				break;
			}
		}
	}
	
	if (!result)
		return 0;
		
	// Make sure this not a subfolder of an already excluded folder
	for (auto j = m_notShared.cbegin(); j != m_notShared.cend(); ++j)
	{
		if (path.size() >= j->size())
		{
			if (Text::isEqualsSubstringIgnoreCase(path, *j))
				return 0;
		}
	}
	
	// remove all sub folder excludes
	int64_t bytesNotCounted = 0;
	for (auto j = m_notShared.cbegin(); j != m_notShared.cend(); ++j)
	{
		if (path.size() < j->size())
		{
			if (Text::isEqualsSubstringIgnoreCase(*j, path))
			{
				bytesNotCounted += Util::getDirSize(*j);
				j = m_notShared.erase(j);
				if (m_notShared.empty()) //[+]PPA
					break;
				--j;
			}
		}
	}
	
	// add it to the list
	m_notShared.push_back(path);
	
	const int64_t bytesRemoved = Util::getDirSize(path);
	
	return bytesRemoved - bytesNotCounted;
}

int64_t ShareManager::removeExcludeFolder(const string &path, bool returnSize /* = true */)
{
	int64_t bytesAdded = 0;
	// remove all sub folder excludes
	for (auto j = m_notShared.cbegin(); j != m_notShared.cend(); ++j)
	{
		if (path.size() <= j->size())
		{
			if (Text::isEqualsSubstringIgnoreCase(*j, path))
			{
				if (returnSize) // this needs to be false if the files don't exist anymore
					bytesAdded += Util::getDirSize(*j);
					
				j = m_notShared.erase(j);
				if (m_notShared.empty()) //[+]PPA
					break;
				--j; // TODO
			}
		}
	}
	
	return bytesAdded;
}

bool ShareManager::findByRealPathName(const string& realPathname, TTHValue* outTTHPtr, string* outfilenamePtr /*= NULL*/, int64_t* outSizePtr/* = NULL*/) // [+] SSA
{
	// [+] IRainman fix.
	SharedLock l(cs);
	
	const Directory::Ptr d = ShareManager::getDirectory(realPathname);
	if (!d)
		return false;
	// [~] IRainman fix.
	
	Directory::File::Set::const_iterator iFile  =  d->findFile(Util::getFileName(realPathname));
	if (iFile != d->files.end())
	{
		if (outTTHPtr)
			*outTTHPtr = iFile->getTTH();
		if (outfilenamePtr)
			*outfilenamePtr = iFile->getName();
		if (outSizePtr)
			*outSizePtr = iFile->getSize();
		return true;
	}
	return false;
}

/**
 * @file
 * $Id: ShareManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
