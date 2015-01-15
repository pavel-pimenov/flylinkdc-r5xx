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
#include "UploadManager.h"
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
bool ShareManager::g_ignoreFileSizeHFS = false; // http://www.flylinkdc.ru/2015/01/hfs-mac-windows.html
size_t ShareManager::g_hits = 0;
std::unique_ptr<webrtc::RWLockWrapper> ShareManager::g_csShare = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ShareManager::g_csShareNotExists = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
QueryNotExistsMap ShareManager::g_file_not_exists_map;

ShareManager::ShareManager() : xmlListLen(0), bzXmlListLen(0),
	xmlDirty(true), forceXmlRefresh(false), refreshDirs(false), update(false), initial(true), m_listN(0), m_count_sec(0),
	m_lastXmlUpdate(0), m_lastFullUpdate(GET_TICK()), m_bloom(1 << 20), m_sharedSize(0),
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
	m_sweep_guard(false),
#endif
	m_sweep_path(false),
#ifdef _DEBUG
	m_CurrentShareSize(-1), // „тобы словить в отладке отрицательную шару
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
		try
		{
			FilteredOutputStream<BZFilter, true> emptyXmlFile(new File(emptyXmlName, File::WRITE, File::TRUNCATE | File::CREATE));
			emptyXmlFile.write(SimpleXML::utf8Header);
			emptyXmlFile.write("<FileListing Version=\"1\" CID=\"" + ClientManager::getMyCID().toBase32() + "\" Base=\"/\" Generator=\"DC++ " DCVERSIONSTRING "\">\r\n"); // Hide Share Mod
			emptyXmlFile.write("</FileListing>");
			emptyXmlFile.flush();
		}
		catch (const Exception& e) // fix https://crash-server.com/Problem.aspx?ProblemID=51912
		{
			LogManager::getInstance()->message("Error create: " + emptyXmlName + " error = " + e.getError());
		}
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
	CFlylinkDBManager::getInstance()->flush_hash();
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
	m_fileTypes_bitmap(1 << Search::TYPE_DIRECTORY)
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
	
	void ShareManager::Directory::addType(Search::TypeModes type) noexcept
	{
		if (!hasType(type))
		{
			m_fileTypes_bitmap |= (1 << type);
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
		return ShareManager::getInstance()->findRealRootL(getName(), path);
	}
}

string ShareManager::findRealRootL(const string& virtualRoot, const string& virtualPath) const
{
	for (auto i = m_shares.cbegin(); i != m_shares.cend(); ++i)
	{
		if (stricmp(i->second.m_synonym, virtualRoot) == 0)
		{
			const std::string name = i->first + virtualPath;
			dcdebug("Matching %s\n", name.c_str());
			if (FileFindIter(name) != FileFindIter::end)
			{
				return name;
			}
		}
	}
	
	throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualPath);
}

int64_t ShareManager::Directory::getSizeL() const noexcept
{
    dcassert(!isShutdown());
    int64_t tmp = size;
    for (auto i = m_directories.cbegin(); i != m_directories.cend(); ++i)
{
tmp += i->second->getSizeL();
}
return tmp;
}
bool ShareManager::destinationShared(const string& file_or_dir_name) const // [+] IRainman opt.
{
	webrtc::ReadLockScoped l(*g_csShare);
	for (auto i = m_shares.cbegin(); i != m_shares.cend(); ++i)
		if (strnicmp(i->first, file_or_dir_name, i->first.size()) == 0 && file_or_dir_name[i->first.size() - 1] == PATH_SEPARATOR)
			return true;
	return false;
}

bool ShareManager::getRealPathAndSize(const TTHValue& tth, string& path, int64_t& size) const // [+] IRainman TODO.
{
	webrtc::ReadLockScoped l(*g_csShare);
	const auto& i = m_tthIndex.find(tth);
	if (i != m_tthIndex.cend())
	{
		try
		{
			const auto& f = i->second;
			path = f->getRealPath();
			size = f->getSize();
			return true;
		}
		catch (const ShareException&) { }
	}
	return false;
}

string ShareManager::toRealPath(const TTHValue& tth) const
{
	webrtc::ReadLockScoped l(*g_csShare);
	const auto& i = m_tthIndex.find(tth);
	if (i != m_tthIndex.end())
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
		webrtc::ReadLockScoped l(*g_csShare);
		const auto& i = m_tthIndex.find(tth);
		if (i != m_tthIndex.end())
		{
			return i->second->getADCPath();
		}
	}
	
	throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, "ShareManager::toVirtual: " + tth.toBase32());
}
#endif

string ShareManager::toReal(const string& virtualFile
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
                            , bool ishidingShare
#endif
                           )
{
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
			return getBZXmlFile();
		}
	}
	
	webrtc::ReadLockScoped l(*g_csShare);
	return findFileL(virtualFile)->getRealPath();
}

TTHValue ShareManager::getTTH(const string& virtualFile) const
{
	if (virtualFile == Transfer::g_user_list_name_bz)
	{
		return bzXmlRoot;
	}
	else if (virtualFile == Transfer::g_user_list_name)
	{
		return xmlRoot;
	}
	webrtc::ReadLockScoped l(*g_csShare);
	return findFileL(virtualFile)->getTTH();
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
			const TTHValue tth = getTTH(virtualFile);
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

void ShareManager::getFileInfo(AdcCommand& cmd, const string& aFile)
{
	if (aFile == Transfer::g_user_list_name)
	{
		generateXmlList();
		
		cmd.addParam("FN", aFile);
		cmd.addParam("SI", Util::toString(xmlListLen));
		cmd.addParam("TR", xmlRoot.toBase32());
		return;
	}
	else if (aFile == Transfer::g_user_list_name_bz)
	{
		generateXmlList();
		
		cmd.addParam("FN", aFile);
		cmd.addParam("SI", Util::toString(bzXmlListLen));
		cmd.addParam("TR", bzXmlRoot.toBase32());
		return;
	}
	
	if (aFile.compare(0, 4, "TTH/", 4) != 0)
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, aFile);
		
	TTHValue val(aFile.c_str() + 4); //[+]FlylinkDC++
	webrtc::ReadLockScoped l(*g_csShare);
	const auto& i = m_tthIndex.find(val);
	if (i == m_tthIndex.end())
	{
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, aFile);
	}
	
	const auto& f = *i->second;
	cmd.addParam("FN", f.getADCPath());
	cmd.addParam("SI", Util::toString(f.getSize()));
	cmd.addParam("TR", f.getTTH().toBase32());
}
pair<ShareManager::Directory::Ptr, string> ShareManager::splitVirtualL(const string& virtualPath) const
{
	if (virtualPath.empty() || virtualPath[0] != '/')
	{
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE , virtualPath);
	}
	
	string::size_type i = virtualPath.find('/', 1);
	if (i == string::npos || i == 1)
	{
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualPath);
	}
	
	auto dmi = getByVirtualL(virtualPath.substr(1, i - 1));
	if (dmi == m_list_directories.end())
	{
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualPath);
	}
	
	Directory::Ptr d = *dmi;
	
	string::size_type j = i + 1;
	while ((i = virtualPath.find('/', j)) != string::npos)
	{
		const auto& mi = d->m_directories.find(virtualPath.substr(j, i - j));
		j = i + 1;
		if (mi == d->m_directories.end())
			throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualPath);
		d = mi->second;
	}
	
	return make_pair(d, virtualPath.substr(j));
}

ShareManager::Directory::ShareFile::Set::const_iterator ShareManager::findFileL(const string& virtualFile) const
{
	if (virtualFile.compare(0, 4, "TTH/", 4) == 0)
	{
		const auto& i = m_tthIndex.find(TTHValue(virtualFile.substr(4)));
		if (i == m_tthIndex.end())
		{
			throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualFile);
		}
		return i->second;
	}
	
	const auto v = splitVirtualL(virtualFile);
	auto it = std::find_if(v.first->m_files.begin(), v.first->m_files.end(),
	                       [&](const Directory::ShareFile & p_file) -> bool {return stricmp(p_file.getName(), v.second) == 0;}
	                       //Directory::ShareFile::StringComp(v.second)
	                      );
	if (it == v.first->m_files.end())
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualFile);
	return it;
}

string ShareManager::validateVirtual(const string& aVirt) noexcept
{
	string tmp = aVirt;
	string::size_type idx = 0;
	
	while ((idx = tmp.find_first_of("\\/"), idx) != string::npos) // TODO - replace_all ?
	{
		tmp[idx] = '_';
	}
	return tmp;
}

bool ShareManager::hasVirtual(const string& virtualName) const noexcept
{
    webrtc::ReadLockScoped l(*g_csShare);
    return getByVirtualL(virtualName) != m_list_directories.end();
}

void ShareManager::load(SimpleXML& aXml)
{
	webrtc::WriteLockScoped l(*g_csShare);
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
			
			const string virtualName = aXml.getChildAttrib("Virtual");
			const string vName = validateVirtual(virtualName.empty() ? Util::getLastDir(realPath) : virtualName);
			if (!File::isExist(realPath))
			{
				m_lost_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, 0)));
				continue;
			}
			m_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, 0)));
			if (getByVirtualL(vName) == m_list_directories.end())
			{
				m_list_directories.push_back(Directory::create(vName));
#ifdef PPA_INCLUDE_OLD_INNOSETUP_WIZARD
				l_count_dir++;
#endif
			}
		}
		aXml.stepOut();
	}
	aXml.resetCurrentChild();
	if (aXml.findChild("NoShare"))
	{
		aXml.stepIn();
		while (aXml.findChild("Directory"))
			m_notShared.push_back(aXml.getChildData());
			
		aXml.stepOut();
	}
	aXml.resetCurrentChild();
	if (aXml.findChild("LostShare"))
	{
		aXml.stepIn();
		while (aXml.findChild("Directory"))
		{
			string realPath = aXml.getChildData();
			if (realPath.empty())
			{
				continue;
			}
			AppendPathSeparator(realPath);
			if (m_shares.find(realPath) == m_shares.end())
			{
				const string virtualName = aXml.getChildAttrib("Virtual");
				const string vName = validateVirtual(virtualName.empty() ? Util::getLastDir(realPath) : virtualName);
				m_lost_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, 0)));
				if (File::isExist(realPath)) // ≈сли каталог по€вилс€ - добавим его в запрос на возврат в шару...
				{
					string l_message_lost_share = " < " + virtualName + " >  " + realPath + "\r\n";
					tstring l_message = TSTRING(RESTORE_LOST_SHARE);
					l_message += Text::toT(l_message_lost_share);
					m_lost_shares.erase(realPath);
					if (MessageBox(NULL, l_message.c_str() , _T(APPNAME) _T(" ") T_VERSIONSTRING, MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES)
					{
						m_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, 0)));
					}
				}
			}
		}
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

struct ShareLoader : public SimpleXMLReader::CallBack
{
		ShareLoader(ShareManager::DirList& aDirs) : dirs(aDirs), cur(0), depth(0) { }
		void startTag(const string& p_name, StringPairList& p_attribs, bool p_simple)
		{
			if (p_name == g_SDirectory)
			{
				const string& name = getAttrib(p_attribs, g_SName, 0);
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
						cur->getParent()->m_directories[cur->getName()] = cur;
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
			else if (cur && p_name == g_SFile)
			{
				const string& fname = getAttrib(p_attribs, g_SName, 0);
				const string& size = getAttrib(p_attribs, g_SSize, 1);
				const string& root = getAttrib(p_attribs, g_STTH, 2);
				if (fname.empty() || size.empty() || (root.size() != 39))
				{
					dcdebug("Invalid file found: %s\n", fname.c_str());
					return;
				}
				auto it = cur->m_files.insert(ShareManager::Directory::ShareFile(fname, Util::toInt64(size), cur, TTHValue(root),
				                                                                 atoi(getAttrib(p_attribs, g_SHit, 3).c_str()),
				                                                                 atoi(getAttrib(p_attribs, g_STS, 3).c_str()),
				                                                                 ShareManager::getFType(fname)
				                                                                )
				                             );
				dcassert(it.second);
				if (it.second && p_attribs.size() > 4) // Ёто уже наша шара. тут медиаинфа если больше 4-х.
				{
					const string& l_audio = getAttrib(p_attribs, g_SMAudio, 3);
					const string& l_video = getAttrib(p_attribs, g_SMVideo, 3);
					if (!l_audio.empty() || !l_video.empty())
					{
						auto l_media_ptr = std::make_shared<CFlyMediaInfo>(CFlyMediaInfo(getAttrib(p_attribs, g_SWH, 3),
						                                                                 atoi(getAttrib(p_attribs, g_SBR, 4).c_str()),
						                                                                 l_audio,
						                                                                 l_video));
						l_media_ptr->calcEscape();
						auto f = const_cast<ShareManager::Directory::ShareFile*>(&(*it.first));
						f->initMediainfo(l_media_ptr);
					}
					else
					{
						dcassert(!(!l_audio.empty() || !l_video.empty())); // Ётого не должно быть?
					}
				}
			}
		}
		void endTag(const string& p_name, const string&)
		{
			if (p_name == g_SDirectory)
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
		ShareLoader loader(m_list_directories);
		SimpleXMLReader xml(&loader);
		const string& cacheFile = getDefaultBZXmlFile();
		{
			File ff(cacheFile, File::READ, File::OPEN); // [!] FlylinkDC: getDefaultBZXmlFile()
			FilteredInputStream<UnBZFilter, false> f(&ff);
			l_cache_loader_log.step("read and uncompress " + cacheFile + " done");
			xml.parse(f);
		}
		
		l_cache_loader_log.step("parse xml done");
		{
			{
				webrtc::WriteLockScoped l(*g_csShare);
				for (auto i = m_list_directories.cbegin(); i != m_list_directories.cend(); ++i)
				{
					updateIndicesL(**i);
				}
			}
			l_cache_loader_log.step("update indices done");
			//internalClearShareNotExists(true);
			//l_cache_loader_log.step("internalClearShareNotExists");
			// https://code.google.com/p/flylinkdc/issues/detail?id=1545
			if (getSharedSize() > 0)
			{
				// ѕолучили размер шары из кэша - не выполн€ем повторный обход в internalCalcShareSize();
				m_isNeedsUpdateShareSize = false;
				m_CurrentShareSize = getSharedSize(); // TODO зачем нам хранить два значени€ размера шары
			}
			else
			{
				internalCalcShareSize();
				l_cache_loader_log.step("Recalc share size");
			}
		}
		l_cache_loader_log.step("update index done");
		return true;
	}
	catch (const Exception& e)
	{
		dcdebug("%s\n", e.getError().c_str());
	}
	return false;
}

void ShareManager::on(SettingsManagerListener::Save, SimpleXML& xml)
{
	save(xml);
}
void ShareManager::on(SettingsManagerListener::Load, SimpleXML& xml)
{
	on(SettingsManagerListener::ShareChanges());
	load(xml);
}
// [+] IRainman opt.
void ShareManager::on(SettingsManagerListener::ShareChanges) noexcept
{
	auto skipList = SPLIT_SETTING_AND_LOWER(SKIPLIST_SHARE);
	
	FastLock l(m_csSkipList);
	swap(m_skipList, skipList);
}

void ShareManager::save(SimpleXML& aXml)
{
	webrtc::ReadLockScoped l(*g_csShare);
	
	aXml.addTag("Share");
	aXml.stepIn();
	for (auto i = m_shares.cbegin(); i != m_shares.cend(); ++i)
	{
		aXml.addTag("Directory", i->first);
		aXml.addChildAttrib("Virtual", i->second.m_synonym);
	}
	aXml.stepOut();
	
	aXml.addTag("LostShare");
	aXml.stepIn();
	for (auto i = m_lost_shares.cbegin(); i != m_lost_shares.cend(); ++i)
	{
		aXml.addTag("Directory", i->first);
		aXml.addChildAttrib("Virtual", i->second.m_synonym);
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
void ShareManager::addDirectory(const string& realPath, const string& virtualName, bool p_is_job)
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
	
	if (!checkVirtual(l_realPathWoPS))  // [5] https://www.box.net/shared/a877c0d7ef9f93b93c55  TODO - «апретить выброс исключений из мастера или гасить их в нем.
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
		ShareMap a;
		{
			webrtc::ReadLockScoped l(*g_csShare);
			a = m_shares;
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
	{
		CFlyDirItemArray directories;
		directories.push_back(CFlyDirItem(virtualName, realPath, 0));
		CFlylinkDBManager::getInstance()->scan_path(directories);
		webrtc::WriteLockScoped l(*g_csShare);
		__int64 l_path_id = 0;
		Directory::Ptr dp = buildTreeL(l_path_id, realPath, Directory::Ptr(), p_is_job);
		
		const string vName = validateVirtual(virtualName);
		dp->setName(vName);
		
		m_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, l_path_id)));
		updateIndicesL(*mergeL(dp));
		setDirty();
	}
}

ShareManager::Directory::Ptr ShareManager::mergeL(const Directory::Ptr& directory)
{
	for (auto i = m_list_directories.cbegin(); i != m_list_directories.cend(); ++i)
	{
		if (stricmp((*i)->getName(), directory->getName()) == 0) // TODO toLower()
		{
			dcdebug("Merging directory %s\n", directory->getName().c_str());
			(*i)->mergeL(directory);
			return *i;
		}
	}
	
	dcdebug("Adding new directory %s\n", directory->getName().c_str());
	
	m_list_directories.push_back(directory);
	return directory;
}

void ShareManager::Directory::mergeL(const Directory::Ptr& source)
{
	for (auto i = source->m_directories.cbegin(); i != source->m_directories.cend(); ++i)
	{
		Directory::Ptr subSource = i->second;
		
		const auto& ti = m_directories.find(subSource->getName());
		if (ti == m_directories.end())
		{
			if (findFileL(subSource->getName()) != m_files.end())
			{
				dcdebug("File named the same as directory");
			}
			else
			{
				m_directories.insert(std::make_pair(subSource->getName(), subSource));
				subSource->parent = this;
			}
		}
		else
		{
			Directory::Ptr subTarget = ti->second;
			subTarget->mergeL(subSource);
		}
	}
	
	// All subdirs either deleted or moved to target...
	source->m_directories.clear();
	
	for (auto i = source->m_files.cbegin(); i != source->m_files.cend(); ++i)
	{
		if (findFileL(i->getName()) == m_files.end())
		{
			if (m_directories.find(i->getName()) != m_directories.end())
			{
				dcdebug("Directory named the same as file");
			}
			else
			{
				//auto& added = files.insert(*i);
				auto it = m_files.insert(*i);
				if (it.second)
				{
					const_cast<ShareFile&>(*it.first).setParent(this);
					//added.first->setParent(this);
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
	
	{
		webrtc::WriteLockScoped l(*g_csShare);
		auto i = m_shares.find(realPath);
		if (i == m_shares.end())
		{
			return;
		}
		const string l_Name = i->second.m_synonym; // —сылку не делать. fix http://www.flickr.com/photos/96019675@N02/9515345001/
		for (auto j = m_list_directories.cbegin(); j != m_list_directories.cend();)
		{
			if (stricmp((*j)->getName(), l_Name) == 0)
			{
				m_list_directories.erase(j++);
			}
			else
			{
				++j;
			}
		}
		
		m_shares.erase(i);
		
		HashManager::HashPauser pauser;
		
		// Readd all directories with the same vName
		for (i = m_shares.begin(); i != m_shares.end(); ++i)
		{
			if (stricmp(i->second.m_synonym, l_Name) == 0 && checkAttributs(i->first))// [!]IRainman checkHidden(i->first)
			{
				Directory::Ptr dp = buildTreeL(i->second.m_path_id, i->first, Directory::Ptr(), true);
				dp->setName(i->second.m_synonym);
				mergeL(dp);
			}
		}
		rebuildIndicesL();
	}
	internalCalcShareSize();
	setDirty();
}

void ShareManager::renameDirectory(const string& realPath, const string& virtualName)
{
	removeDirectory(realPath);
	addDirectory(realPath, virtualName, false);
}

ShareManager::DirList::const_iterator ShareManager::getByVirtualL(const string& virtualName) const
{
	for (auto i = m_list_directories.cbegin(); i != m_list_directories.cend(); ++i)
	{
		if (stricmp((*i)->getName(), virtualName) == 0) // TODO toLower
		{
			return i;
		}
	}
	return m_list_directories.end();
}

int64_t ShareManager::getShareSize(const string& realPath) const noexcept
{
    dcassert(!isShutdown());
    dcassert(!realPath.empty());
    webrtc::ReadLockScoped l(*g_csShare);
    const auto i = m_shares.find(realPath);
    if (i != m_shares.end())
{
const auto j = getByVirtualL(i->second.m_synonym);
	if (j != m_list_directories.end())
	{
		return (*j)->getSizeL();
	}
}
return -1;
}

void ShareManager::internalCalcShareSize() // [!] IRainman opt.
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
				webrtc::ReadLockScoped l(*g_csShare);
				for (auto i = m_tthIndex.cbegin(); i != m_tthIndex.cend(); ++i)
				{
					l_CurrentShareSize += i->second->getSize();
				}
			}
			m_CurrentShareSize = l_CurrentShareSize;
		}
	}
	dcassert(m_sharedSize == m_CurrentShareSize);
}

ShareManager::Directory::Ptr ShareManager::buildTreeL(__int64& p_path_id, const string& aName, const Directory::Ptr& aParent, bool p_is_job)
{
	bool p_is_no_mediainfo = false;
	if (p_path_id == 0)
	{
		p_path_id = CFlylinkDBManager::getInstance()->get_path_id(Text::toLower(aName), !p_is_job, false, p_is_no_mediainfo, m_sweep_path);
	}
	Directory::Ptr dir = Directory::create(Util::getLastDir(aName), aParent);
	
	auto lastFileIter = dir->m_files.begin();
	
	CFlyDirMap l_dir_map;
	if (p_path_id)
		CFlylinkDBManager::getInstance()->LoadDir(p_path_id, l_dir_map, p_is_no_mediainfo);
	for (FileFindIter i(aName + '*'); !isShutdown() && i != FileFindIter::end; ++i)// [!]IRainman add m_close [10] https://www.box.net/shared/067924cecdb252c9d26c
	{
		if (i->isTemporary())// [+]IRainman
			continue;
		const string& l_file_name = i->getFileName();
		if (l_file_name == Util::m_dot || l_file_name == Util::m_dot_dot || l_file_name.empty())
			continue;
		if (i->isHidden() && !BOOLSETTING(SHARE_HIDDEN))
			continue;
		if (i->isSystem() && !BOOLSETTING(SHARE_SYSTEM))
			continue;
		if (i->isVirtual() && !BOOLSETTING(SHARE_VIRTUAL))
			continue;
		const string l_lower_name = Text::toLower(l_file_name);
		if (!skipListEmpty())
		{
			if (isInSkipList(l_lower_name))
			{
				LogManager::getInstance()->message(STRING(USER_DENIED_SHARE_THIS_FILE) + ' ' + l_file_name
				                                   + " (" + STRING(SIZE) + ": " + Util::toString(i->getSize()) + ' '
				                                   + STRING(B) + ") (" + STRING(DIRECTORY) + ": \"" + aName + "\")");
				continue;
			}
		}
		if (i->isDirectory())
		{
			const string newName = aName + l_file_name + PATH_SEPARATOR;
			
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
				__int64 l_path_id = 0;
				dir->m_directories[l_file_name] = buildTreeL(l_path_id, newName, dir, p_is_job);
			}
		}
		else
		{
			// Not a directory, assume it's a file...make sure we're not sharing the settings file...
			if (!g_fly_server_config.isBlockShare(l_lower_name))
			{
				const string l_PathAndFileName = aName + l_file_name;
				if (stricmp(l_PathAndFileName, SETTING(TLS_PRIVATE_KEY_FILE)) == 0) // TODO - унести проверку в другое место.
					continue;
				const int64_t l_size = i->getSize();
				const int64_t l_ts = i->getLastWriteTime();
				CFlyDirMap::iterator l_dir_item = l_dir_map.find(l_lower_name);
				bool l_is_new_file = l_dir_item == l_dir_map.end();
				if (!l_is_new_file)
				{
					l_is_new_file = l_dir_item->second.m_TimeStamp != l_ts || (l_dir_item->second.m_size != l_size && g_ignoreFileSizeHFS == false);
#if 0
					if (l_is_new_file)
					{
						LogManager::getInstance()->message("[!!!!!!!!][1] bool l_is_new_file = l_dir_item == l_dir_map.end(); l_lower_name = " + l_lower_name + " name = " + l_file_name);
						LogManager::getInstance()->message("[!!!!!!!!][1] l_dir_item->second.m_size = " + Util::toString(l_dir_item->second.m_size)
						                                   + " size = " + Util::toString(l_size)
						                                   + " l_dir_item->second.m_TimeStamp = " + Util::toString(l_dir_item->second.m_TimeStamp)
						                                   + " l_ts = " + Util::toString(l_ts)
						                                  );
					}
#endif
				}
				else
				{
					// LogManager::getInstance()->message("[!!!!!!!!][2] bool l_is_new_file = l_dir_item == l_dir_map.end(); l_lower_name = " + l_lower_name + " name = " + l_file_name);
				}
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
				if (l_dir_item != l_dir_map.end())
					l_dir_item_second.m_is_found = true;
#endif
				try
				{
					if (l_is_new_file)
					{
						HashManager::getInstance()->hashFile(p_path_id, l_PathAndFileName, l_size);
					}
					else
					{
						auto &l_dir_item_second = l_dir_item->second; // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'l_dir_item->second' expression repeatedly. sharemanager.cpp 1054
						lastFileIter = dir->m_files.insert(lastFileIter,
						                                   Directory::ShareFile(l_file_name,
						                                                        l_dir_item_second.m_size,
						                                                        dir,
						                                                        l_dir_item_second.m_tth,
						                                                        l_dir_item_second.m_hit,
						                                                        uint32_t(l_dir_item_second.m_StampShare),
						                                                        Search::TypeModes(l_dir_item_second.m_ftype)
						                                                       )
						                                  );
						auto f = const_cast<ShareManager::Directory::ShareFile*>(&(*lastFileIter));
						f->initMediainfo(l_dir_item_second.m_media_ptr);
						l_dir_item_second.m_media_ptr = nullptr;
					}
				}
				catch (const HashException& e)
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
	for (auto i = p_dir.m_directories.cbegin(); i != p_dir.m_directories.cend(); ++i)
	{
		l_count += rebuildMediainfo(*i->second, p_log, p_result);
	}
	__int64 l_path_id = 0;
	string l_real_path;
	for (auto i = p_dir.m_files.cbegin(); i != p_dir.m_files.cend(); ++i)
	{
		const auto& l_file = *i;
		if (CFlyServerConfig::isMediainfoExt(Util::getFileExtWithoutDot(l_file.getLowName()))) // [!] IRainman opt.
		{
			if (l_path_id == 0)
			{
				l_real_path = Util::getFilePath(l_file.getRealPath());
				bool l_is_no_mediaiinfo;
				l_path_id = CFlylinkDBManager::getInstance()->get_path_id(l_real_path, false, true, l_is_no_mediaiinfo);
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
		if (CFlyServerConfig::isMediainfoExt(Text::toLower(Util::getFileExtWithoutDot(j->m_file_name))))
		{
			CFlyMediaInfo l_out_media;
			if (getMediaInfo(j->m_path + j->m_file_name, l_out_media, j->m_size, j->m_tth, true))
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

void ShareManager::updateIndicesL(Directory& dir)
{
	m_bloom.add(Text::toLower(dir.getName())); // TODO сохранить им€ в нижнем?
	
	for (auto i = dir.m_directories.cbegin(); i != dir.m_directories.cend(); ++i)
	{
		updateIndicesL(*i->second);
	}
	
	dir.size = 0;
	
	for (auto i = dir.m_files.cbegin(); i != dir.m_files.cend();)
	{
		updateIndicesL(dir, i++);
	}
}

void ShareManager::rebuildIndicesL()
{
	m_sharedSize = 0;
	m_isNeedsUpdateShareSize = true;
	m_tthIndex.clear();
	m_bloom.clear();
	
	for (auto i = m_list_directories.cbegin(); i != m_list_directories.cend(); ++i)
	{
		updateIndicesL(**i);
	}
}

void ShareManager::updateIndicesL(Directory& dir, const Directory::ShareFile::Set::iterator& i)
{
	const auto& f = *i;
	
	const auto& j = m_tthIndex.find(f.getTTH());
	if (j == m_tthIndex.end())
	{
		dir.size += f.getSize();
		m_sharedSize += f.getSize();
	}
	
	dir.addType(f.getFType());
	
	m_tthIndex.insert(make_pair(f.getTTH(), i));
	m_isNeedsUpdateShareSize = true;
	dcassert(Text::toLower(f.getName()) == f.getLowName());
	m_bloom.add(Text::toLower(f.getName())); // TODO - тут заюзать  f.getLowName()
	
#ifdef STRONG_USE_DHT
	dht::IndexManager* im = dht::IndexManager::getInstance();
	if (im->isTimeForPublishing())
		im->publishFile(f.getTTH(), f.getSize());
#endif
}

void ShareManager::refresh(bool dirs /* = false */, bool aUpdate /* = true */, bool block /* = false */) noexcept
{
	if (m_is_refreshing.test_and_set())
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

void ShareManager::getDirectories(CFlyDirItemArray& p_dirs) const noexcept
{
    dcassert(p_dirs.empty());
    p_dirs.clear();
    webrtc::ReadLockScoped l(*g_csShare);
    p_dirs.reserve(m_shares.size());
    for (auto i = m_shares.cbegin(); i != m_shares.cend(); ++i)
{
p_dirs.push_back(CFlyDirItem(i->second.m_synonym, i->first, 0));
}
}

int ShareManager::run()
{
	setThreadPriority(Thread::LOW); // [+] IRainman fix.
	
	CFlyDirItemArray directories;
	getDirectories(directories);
	// Don't need to refresh if no directories are shared
	if (directories.empty())
		refreshDirs = false;
		
	if (refreshDirs)
	{
		HashManager::HashPauser pauser;
		
		LogManager::getInstance()->message(STRING(FILE_LIST_REFRESH_INITIATED));
		m_sharedSize = 0;
		m_lastFullUpdate = GET_TICK();
		CFlylinkDBManager::getInstance()->scan_path(directories);
		DirList newDirs;
		{
			webrtc::WriteLockScoped l(*g_csShare);
			for (auto i = directories.begin(); i != directories.end(); ++i)
			{
				if (checkAttributs(i->m_path))
				{
					Directory::Ptr dp = buildTreeL(i->m_path_id, i->m_path, Directory::Ptr(), false);
					dp->setName(i->m_synonym);
					newDirs.push_back(dp);
				}
			}
		}
		if (m_sweep_path)
		{
			m_sweep_path = false;
#ifdef PPA_INCLUDE_ONLINE_SWEEP_DB
			m_sweep_guard = true;
#endif
			CFlylinkDBManager::getInstance()->sweep_db();
		}
		
		{
			webrtc::WriteLockScoped l(*g_csShare);
			m_list_directories.clear();
			
			for (auto i = newDirs.cbegin(); i != newDirs.cend(); ++i)
			{
				mergeL(*i);
			}
			
			rebuildIndicesL();
		}
		internalCalcShareSize();
		refreshDirs = false;
		LogManager::getInstance()->message(STRING(FILE_LIST_REFRESH_FINISHED));
	}
	
	if (update)
	{
		ClientManager::infoUpdated();
	}
	
#ifdef STRONG_USE_DHT
	dht::IndexManager* im = dht::IndexManager::getInstance();
	if (im->isTimeForPublishing())
		im->setNextPublishing();
#endif
		
	m_is_refreshing.clear();
	CFlylinkDBManager::getInstance()->flush_hash();
	return 0;
}

void ShareManager::getBloom(ByteVector& v, size_t k, size_t m, size_t h) const
{
	dcdebug("Creating bloom filter, k=%u, m=%u, h=%u\n", k, m, h);
	HashBloom bloom;
	bloom.reset(k, m, h);
	{
		webrtc::ReadLockScoped l(*g_csShare);
		for (auto i = m_tthIndex.cbegin(); i != m_tthIndex.cend(); ++i)
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
		
	if (forceXmlRefresh || (xmlDirty && (m_lastXmlUpdate + 15 * 60 * 1000 < GET_TICK() || m_lastXmlUpdate < m_lastFullUpdate)))
	{
		CFlyLog l_creation_log("[Share cache creator]");
		m_listN++;
		
		try
		{
			string tmp2;
			string indent;
			
			string newXmlName = Util::getConfigPath() + "files" + Util::toString(m_listN) + ".xml.bz2";
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
					webrtc::ReadLockScoped l(*g_csShare);
					for (auto i = m_list_directories.cbegin(); i != m_list_directories.cend(); ++i)
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
			
			const string& l_XmlListFileName = getDefaultBZXmlFile(); // [+] IRainman
			
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
				l_creation_log.log("Error File::renameFile  newXmlName = " + newXmlName +
				                   " l_XmlListFileName = " + l_XmlListFileName + " Error = " + Util::translateError());
				// Ignore, this is for caching only...
			}
			bzXmlRef = unique_ptr<File>(new File(newXmlName, File::READ, File::OPEN));
			setBZXmlFile(newXmlName);
			bzXmlListLen = File::getSize(newXmlName);
		}
		catch (const Exception&)
		{
			//    l_creation_log.log("Error File::renameFile  newXmlName = " + newXmlName + " l_XmlListFileName = " + l_XmlListFileName + " Error = " Util::translateError());
			// No new file lists...
		}
		
		xmlDirty = false;
		forceXmlRefresh = false;
		m_lastXmlUpdate = GET_TICK();
		
		// [+] IRainman cleaning old file cache
		l_creation_log.step("Clean old cache");
		const StringList& l_ToDelete = File::findFiles(Util::getConfigPath(), "files*.xml.bz2", false);
		for (auto i = l_ToDelete.cbegin(); i != l_ToDelete.cend(); ++i)
		{
			if (*i != getBZXmlFile() && *i != getDefaultBZXmlFile())
			{
				if (File::deleteFile(Util::getConfigPath() + *i))
				{
					l_creation_log.log("Delete: " + Util::getConfigPath() + *i);
				}
				else
				{
					l_creation_log.log("Error delete: " + Util::getConfigPath() + *i + "[ " + Util::translateError() + "]");
				}
			}
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
	
	if (dir == "/")
	{
		webrtc::ReadLockScoped l(*g_csShare);
		for (auto i = m_list_directories.cbegin(); i != m_list_directories.cend(); ++i)
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
		
		webrtc::ReadLockScoped l(*g_csShare);
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
				const auto& it = getByVirtualL(dir.substr(j, i - j));
				
				if (it == m_list_directories.end())
					return nullptr;
					
				root = *it;
			}
			else
			{
				const auto&  it2 = root->m_directories.find(dir.substr(j, i - j));
				if (it2 == root->m_directories.end())
					return nullptr;
					
				root = it2->second;
			}
			j = i + 1;
		}
		if (!root)
			return 0;
			
		for (auto it2 = root->m_directories.cbegin(); it2 != root->m_directories.cend(); ++it2)
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
	xmlFile.write(SimpleXML::escapeAtrib(getName(), tmp2)); // [+]PPA - упростил операцию escape без анализа кодировки utf-8 stricmp(encoding, Text::g_utf8)
	
	if (fullList)
	{
		xmlFile.write(LITERAL("\">\r\n"));
		
		p_indent += '\t';
		for (auto i = m_directories.cbegin(); i != m_directories.cend(); ++i)
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
		if (m_directories.empty() && m_files.empty())
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
	for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
	{
		const auto& f = *i;
		if (!indent.empty())
			xmlFile.write(indent);
		xmlFile.write(LITERAL("<File Name=\""));
		xmlFile.write(SimpleXML::escapeAtrib(f.getName(), tmp2)); // [+]PPA - упростил операцию escape без анализа кодировки utf-8 stricmp(encoding, Text::g_utf8)
		xmlFile.write(LITERAL("\" Size=\""));
		xmlFile.write(Util::toString(f.getSize()));
		xmlFile.write(LITERAL("\" TTH=\""));
		tmp2.clear();
		xmlFile.write(f.getTTH().toBase32(tmp2));
		if (f.getHit() && BOOLSETTING(ENABLE_HIT_FILE_LIST))
		{
			xmlFile.write(LITERAL("\" HIT=\""));
			xmlFile.write(Util::toString(f.getHit()));
		}
		xmlFile.write(LITERAL("\" TS=\""));
		xmlFile.write(Util::toString(f.getTS()));
		if (f.m_media_ptr)
		{
			if (f.m_media_ptr->m_bitrate)
			{
				xmlFile.write(LITERAL("\" BR=\""));
				xmlFile.write(Util::toString(f.m_media_ptr->m_bitrate));
			}
			if (f.m_media_ptr->m_mediaX && f.m_media_ptr->m_mediaY)
			{
				xmlFile.write(LITERAL("\" WH=\""));
				xmlFile.write(f.m_media_ptr->getXY());
			}
			
			if (!f.m_media_ptr->m_audio.empty())
			{
				xmlFile.write(LITERAL("\" MA=\""));
				if (f.m_media_ptr->m_is_need_escape)
					xmlFile.write(SimpleXML::escapeForce(f.m_media_ptr->m_audio, tmp2));
				else
					xmlFile.write(f.m_media_ptr->m_audio);
			}
			if (!f.m_media_ptr->m_video.empty())
			{
				xmlFile.write(LITERAL("\" MV=\""));
				if (f.m_media_ptr->m_is_need_escape)
					xmlFile.write(SimpleXML::escapeForce(f.m_media_ptr->m_video, tmp2));
				else
					xmlFile.write(f.m_media_ptr->m_video);
			}
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

bool ShareManager::checkType(const string& aString, Search::TypeModes aType)
{
	if (aType == Search::TYPE_ANY)
		return true;
		
	if (aString.length() < 5)
		return false;
		
	const char* c = aString.c_str() + aString.length() - 3;
	if (!Text::isAscii(c))
		return false;
		
	const uint32_t type = '.' | (Text::asciiToLower(c[0]) << 8) | (Text::asciiToLower(c[1]) << 16) | (((uint32_t)Text::asciiToLower(c[2])) << 24);
	
#ifdef _DEBUG
// ¬се расширени€ 1-го типа должны быть с точкой и длиной 4
	DEBUG_CHECK_TYPE(typeAudio)
	DEBUG_CHECK_TYPE(typeCompressed)
	DEBUG_CHECK_TYPE(typeDocument)
	DEBUG_CHECK_TYPE(typeExecutable)
	DEBUG_CHECK_TYPE(typePicture)
	DEBUG_CHECK_TYPE(typeCDImage)
// ¬се расширени€ 2-го типа должны быть тоже с точкой но с длиной отличной от 4
	DEBUG_CHECK_TYPE2(type2Audio)
	DEBUG_CHECK_TYPE2(type2Document)
	DEBUG_CHECK_TYPE2(type2Compressed)
	DEBUG_CHECK_TYPE2(type2Picture)
	DEBUG_CHECK_TYPE2(type2Video)
#endif
	switch (aType)
	{
		case Search::TYPE_AUDIO:
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
		case Search::TYPE_COMPRESSED:
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
		case Search::TYPE_DOCUMENT:
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
		case Search::TYPE_EXECUTABLE:
			for (size_t i = 0; i < _countof(typeExecutable); i++)
			{
				if (IS_TYPE(typeExecutable[i]))
				{
					return true;
				}
			}
			break;
		case Search::TYPE_PICTURE:
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
		case Search::TYPE_VIDEO:
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
		case Search::TYPE_CD_IMAGE:
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

Search::TypeModes ShareManager::getFType(const string& aFileName) noexcept
{
	dcassert(!aFileName.empty());
	if (aFileName.empty())
	{
		return Search::TYPE_ANY;
	}
	if (aFileName[aFileName.length() - 1] == PATH_SEPARATOR)
	{
		return Search::TYPE_DIRECTORY;
	}
	
	if (checkType(aFileName, Search::TYPE_VIDEO))
		return Search::TYPE_VIDEO;
	else if (checkType(aFileName, Search::TYPE_AUDIO))
		return Search::TYPE_AUDIO;
	else if (checkType(aFileName, Search::TYPE_COMPRESSED))
		return Search::TYPE_COMPRESSED;
	else if (checkType(aFileName, Search::TYPE_DOCUMENT))
		return Search::TYPE_DOCUMENT;
	else if (checkType(aFileName, Search::TYPE_EXECUTABLE))
		return Search::TYPE_EXECUTABLE;
	else if (checkType(aFileName, Search::TYPE_PICTURE))
		return Search::TYPE_PICTURE;
	else if (checkType(aFileName, Search::TYPE_CD_IMAGE)) //[+] from FlylinkDC++
		return Search::TYPE_CD_IMAGE;
		
	return Search::TYPE_ANY;
}

/**
 * Alright, the main point here is that when searching, a search string is most often found in
 * the filename, not directory name, so we want to make that case faster. Also, we want to
 * avoid changing StringLists unless we absolutely have to --> this should only be done if a string
 * has been matched in the directory name. This new stringlist should also be used in all descendants,
 * but not the parents...
 */
void ShareManager::Directory::search(SearchResultList& aResults, StringSearch::List& aStrings, Search::SizeModes aSizeMode, int64_t aSize, Search::TypeModes aFileType, Client* aClient, StringList::size_type maxResults) const noexcept
{
    // Skip everything if there's nothing to find here (doh! =)
    if (!hasType(aFileType))
    return;
    
    StringSearch::List* cur = &aStrings;
    unique_ptr<StringSearch::List> newStr;
    
    // Find any matches in the directory name
#ifdef FLYLINKDC_USE_COLLECT_STAT
    int l_count_find = 0;
#endif
    for (auto k = aStrings.cbegin(); k != aStrings.cend(); ++k)
{
#ifdef FLYLINKDC_USE_COLLECT_STAT
	{
		string l_tth;
		const auto l_tth_pos = (*k).getPattern().find("TTH:");
			if (l_tth_pos != string::npos)
				l_tth = (*k).getPattern().c_str() + l_tth_pos + 7;
			CFlylinkDBManager::getInstance()->push_event_statistic("ShareManager::Directory::search",
			"find-" + Util::toString(++l_count_find),
			(*k).getPattern(),
			"",
			"",
			aClient->getHubUrlAndIP(),
			l_tth);
		}
#endif
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
        (((aFileType == Search::TYPE_ANY) && sizeOk) || (aFileType == Search::TYPE_DIRECTORY)))
{
// We satisfied all the search words! Add the directory...(NMDC searches don't support directory size)
SearchResultPtr sr(new SearchResult(SearchResult::TYPE_DIRECTORY, 0, getFullName(), TTHValue(), -1 /*token*/));
	aResults.push_back(sr);
	ShareManager::incHits();
}

if (aFileType != Search::TYPE_DIRECTORY)
{
for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
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
			SearchResultPtr sr(new SearchResult(SearchResult::TYPE_FILE, i->getSize(), getFullName() + i->getName(), i->getTTH(), -1  /*token*/));
			aResults.push_back(sr);
			ShareManager::incHits();
			if (aResults.size() >= maxResults)
			{
				break;
			}
		}
	}
}

for (auto l = m_directories.cbegin(); l != m_directories.cend() && aResults.size() < maxResults; ++l)
{
if (l->second)
		l->second->search(aResults, *cur, aSizeMode, aSize, aFileType, aClient, maxResults); //TODO - Hot point
}
}

void ShareManager::searchTTHArray(CFlySearchArray& p_all_search_array, const Client* p_client)
{
	webrtc::ReadLockScoped l(*g_csShare);
	const auto l_slots     = UploadManager::getInstance()->getSlots();
	const auto l_freeSlots = UploadManager::getInstance()->getFreeSlots();
	for (auto j = p_all_search_array.begin(); j != p_all_search_array.end(); ++j)
	{
		const auto& i = m_tthIndex.find(j->m_tth);
		if (i == m_tthIndex.end())
			continue;
		dcassert(i->second->getParent());
		const auto &l_fileMap = i->second;
		SearchResultBaseTTH l_result(SearchResult::TYPE_FILE,
		                             l_fileMap->getSize(),
		                             l_fileMap->getParent()->getFullName() + l_fileMap->getName(),
		                             l_fileMap->getTTH(),
		                             l_slots,
		                             l_freeSlots
		                            );
		incHits();
		j->m_toSRCommand = new string(l_result.toSR(*p_client));
		COMMAND_DEBUG("[TTH]$Search " + j->m_search + " TTH = " + j->m_tth.toBase32() , DebugTask::HUB_IN, p_client->getIpPort());
	}
}

void ShareManager::search(SearchResultList& aResults, const string& aString, Search::SizeModes aSizeMode, int64_t aSize, Search::TypeModes aFileType, Client* aClient, StringList::size_type maxResults) noexcept
{
	dcassert(!ClientManager::isShutdown());
	if (aFileType == Search::TYPE_TTH)
	{
		dcassert(isTTHBase64(aString));
		// ¬етка пока работает!
		if (isTTHBase64(aString)) //[+]FlylinkDC++ opt.
		{
			TTHValue tth(aString.c_str() + 4);  //[+]FlylinkDC++ opt. //-V112
			SearchResultPtr sr;
			{
				webrtc::ReadLockScoped l(*g_csShare);
				const auto& i = m_tthIndex.find(tth);
				if (i == m_tthIndex.end())
					return;
				dcassert(i->second->getParent());
				if (!i->second->getParent())
					return;
				const auto &l_fileMap = i->second; // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'i->second' expression repeatedly. sharemanager.cpp 2012
				// TODO - дл€ TTH сильно толстый объект  SearchResult
				sr = new SearchResult(SearchResult::TYPE_FILE, l_fileMap->getSize(),
				l_fileMap->getParent()->getFullName() + l_fileMap->getName(), l_fileMap->getTTH(), -1  /*token*/);
				incHits();
			}
#ifdef FLYLINKDC_USE_COLLECT_STAT
			{
				CFlylinkDBManager::getInstance()->push_event_statistic("ShareManager::search",
				                                                       "TTH",
				                                                       aString,
				                                                       "",
				                                                       "",
				                                                       aClient->getHubUrlAndIP(),
				                                                       tth.toBase32());
			}
#endif
			aResults.push_back(sr);
		}
		return;
	}
	{
		webrtc::ReadLockScoped l(*g_csShareNotExists);
		if (g_file_not_exists_map.find(aString) != g_file_not_exists_map.end())
		{
			return; // ”ходим сразу - у нас в шаре этого не по€вилось.
		}
	}
	
	const StringTokenizer<string> t(Text::toLower(aString), '$'); // 2012-05-03_22-05-14_YNJS7AEGAWCUMRBY2HTUTLYENU4OS2PKNJXT6ZY_F4B220A1_crash-stack-r502-beta24-x64-build-9900.dmp
	const StringList& sl = t.getTokens();
	{
		webrtc::ReadLockScoped l(*g_csShare);
		if (!m_bloom.match(sl))
			return;
	}
	
	StringSearch::List ssl;
	ssl.reserve(sl.size());
#ifdef FLYLINKDC_USE_COLLECT_STAT
	int l_count_find = 0;
#endif
	for (auto i = sl.cbegin(); i != sl.cend(); ++i)
	{
		if (!i->empty())
		{
			ssl.push_back(StringSearch(*i));
#ifdef FLYLINKDC_USE_COLLECT_STAT
			{
				CFlylinkDBManager::getInstance()->push_event_statistic("ShareManager::search",
				"FileSearch-" + Util::toString(++l_count_find),
				*i,
				"",
				"",
				aClient->getHubUrlAndIP(),
				"");
			}
#endif
			
		}
	}
	if (!ssl.empty())
	{
		webrtc::ReadLockScoped l(*g_csShare);
		for (auto j = m_list_directories.cbegin(); j != m_list_directories.cend() && aResults.size() < maxResults; ++j)
		{
			(*j)->search(aResults, ssl, aSizeMode, aSize, aFileType, aClient, maxResults);
		}
	}
	// Ќичего не нашли - сохраним условие поиска чтобы не искать второй раз по этому-же запросу.
	if (aResults.empty())
	{
		webrtc::WriteLockScoped l(*g_csShareNotExists);
		g_file_not_exists_map[aString]++;
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
			m_exts.push_back(p.substr(2));
		}
		else if (toCode('G', 'R') == cmd)
		{
			const auto l_exts = AdcHub::parseSearchExts(Util::toInt(p.substr(2)));
			m_exts.insert(m_exts.begin(), l_exts.begin(), l_exts.end());
		}
		else if (toCode('R', 'X') == cmd)
		{
			m_noExts.push_back(p.substr(2));
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
	if (m_exts.empty())
		return true;
	if (!m_noExts.empty())
	{
		m_exts = StringList(m_exts.begin(), set_difference(m_exts.begin(), m_exts.end(), m_noExts.begin(), m_noExts.end(), m_exts.begin()));
		m_noExts.clear();
	}
	for (auto i = m_exts.cbegin(), iend = m_exts.cend(); i != iend; ++i)
	{
		if (name.length() >= i->length() && stricmp(name.c_str() + name.length() - i->length(), i->c_str()) == 0)
			return true;
	}
	return false;
}

void ShareManager::Directory::search(SearchResultList& aResults, AdcSearch& aStrings, StringList::size_type maxResults) const noexcept
{
    dcassert(!ClientManager::isShutdown());

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
if (cur->empty() && aStrings.m_exts.empty() && sizeOk)
{
// We satisfied all the search words! Add the directory...
SearchResultPtr sr(new SearchResult(SearchResult::TYPE_DIRECTORY, getSizeL(), getFullName(), TTHValue(), -1  /*token*/));
	aResults.push_back(sr);
	ShareManager::incHits();
}

if (!aStrings.isDirectory)
{
for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
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
			                                    i->getSize(), getFullName() + i->getName(), i->getTTH(), -1  /*token*/));
			aResults.push_back(sr);
			ShareManager::incHits();
			if (aResults.size() >= maxResults)
			{
				return;
			}
		}
	}
}

for (auto l = m_directories.cbegin(); l != m_directories.cend() && aResults.size() < maxResults; ++l)
{
l->second->search(aResults, aStrings, maxResults);
}
aStrings.include = old;
}

void ShareManager::search(SearchResultList& results, const StringList& params, StringList::size_type maxResults, StringSearch::List& reguest) noexcept // [!] IRainman-S add StringSearch::List& reguest
{
	dcassert(!ClientManager::isShutdown());
	
	AdcSearch srch(params);
	reguest = srch.includeX; // [+] IRainman-S
	if (srch.hasRoot)
	{
		SearchResultPtr sr;
		{
			webrtc::ReadLockScoped l(*g_csShare);
			const auto& i = m_tthIndex.find(srch.root);
			if (i == m_tthIndex.end())
				return;
			const auto &l_fileMap = i->second; // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'i->second' expression repeatedly. sharemanager.cpp 2240
			sr = new SearchResult(SearchResult::TYPE_FILE,
			l_fileMap->getSize(), l_fileMap->getParent()->getFullName() + l_fileMap->getName(),
			l_fileMap->getTTH(), -1  /*token*/);
			
			incHits();
		}
		results.push_back(sr);
		return;
	}
	
	webrtc::ReadLockScoped l(*g_csShare);
	for (auto i = srch.includeX.cbegin(); i != srch.includeX.cend(); ++i)
	{
		if (!m_bloom.match(i->getPattern()))
			return;
	}
	
	for (auto j = m_list_directories.cbegin(); j != m_list_directories.cend() && results.size() < maxResults; ++j)
	{
		(*j)->search(results, srch, maxResults);
	}
}

ShareManager::Directory::Ptr ShareManager::getDirectoryL(const string& fname) const
{
	for (auto mi = m_shares.cbegin(); mi != m_shares.cend(); ++mi)
	{
		if (strnicmp(fname, mi->first, mi->first.length()) == 0)
		{
			Directory::Ptr d;
			for (auto i = m_list_directories.cbegin(); i != m_list_directories.cend(); ++i)
			{
				if (stricmp((*i)->getName(), mi->second.m_synonym) == 0) // TODO - getLower
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
				const auto& dmi = d->m_directories.find(fname.substr(j, i - j));
				j = i + 1;
				if (dmi == d->m_directories.end())
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
		webrtc::ReadLockScoped l(*g_csShare);
		for (auto i = m_shares.cbegin(); i != m_shares.cend(); ++i)
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
				bool p_is_no_mediainfo;
				const __int64 l_path_id = CFlylinkDBManager::getInstance()->get_path_id(fpath, true, false, p_is_no_mediainfo, true);
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
	dcassert(!ClientManager::isShutdown());
	{
		webrtc::WriteLockScoped l(*g_csShare);
		if (Directory::Ptr d = getDirectoryL(fname)) // TODO прокинуть p_path_id и искать по нему?
		{
			const string l_file_name = Util::getFileName(fname);
			const auto i = d->findFileL(l_file_name);
			if (i != d->m_files.end())
			{
				if (root != i->getTTH())
					m_tthIndex.erase(i->getTTH());
				// Get rid of false constness...
				Directory::ShareFile* f = const_cast<Directory::ShareFile*>(&(*i));
				f->setTTH(root);
				m_tthIndex.insert(make_pair(f->getTTH(), i));
				m_isNeedsUpdateShareSize = true;
			}
			else
			{
				const int64_t size = File::getSize(fname);
				auto it = d->m_files.insert(Directory::ShareFile(l_file_name, size, d, root, 0, uint32_t(aTimeStamp), getFType(l_file_name)));
				dcassert(it.second);
				if (it.second)
				{
					auto l_media_ptr = std::make_shared<CFlyMediaInfo>(p_out_media);
					l_media_ptr->calcEscape();
					auto f = const_cast<Directory::ShareFile*>(&(*it.first));
					f->initMediainfo(l_media_ptr);
				}
				updateIndicesL(*d, it.first);
			}
			setDirty();
			forceXmlRefresh = true;
		}
	}
	// —бросим кеш отсутствующих файлов
	internalClearShareNotExists(true);
}

void ShareManager::on(TimerManagerListener::Second, uint64_t tick) noexcept
{
	if ((++m_count_sec % 10) == 0)
	{
		CFlylinkDBManager::getInstance()->flush_hash();
	}
}

void ShareManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	if (SETTING(AUTO_REFRESH_TIME) > 0)
	{
		if (m_lastFullUpdate + SETTING(AUTO_REFRESH_TIME) * 60 * 1000 < tick)
		{
			refresh(true, true);
		}
	}
	internalCalcShareSize(); // [+]IRainman opt.
	internalClearShareNotExists(false);
}

void ShareManager::internalClearShareNotExists(bool p_is_force)
{
	webrtc::WriteLockScoped l(*g_csShareNotExists);
#if _DEBUG
	for (auto i = g_file_not_exists_map.begin(); i != g_file_not_exists_map.end(); ++i)
	{
		if (i->second > 1)
		{
			LogManager::getInstance()->message("[ShareManager] Fix dup-search: " + i->first + ", Count = " + Util::toString(i->second) + ")");
		}
	}
#endif
	if (p_is_force || g_file_not_exists_map.size() > 1000)
	{
		g_file_not_exists_map.clear();
	}
}

bool ShareManager::isShareFolder(const string& path, bool thoroughCheck /* = false */) const
{
	dcassert(!path.empty());
	if (thoroughCheck)  // check if it's part of the share before checking if it's in the exclusions
	{
		bool result = false;
		for (auto i = m_shares.cbegin(); i != m_shares.cend(); ++i)
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
	for (auto i = m_shares.cbegin(); i != m_shares.cend(); ++i)
	{
		if (path.size() > i->first.size())
		{
			if (Text::isEqualsSubstringIgnoreCase(path, i->first))
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
				if (j != m_notShared.begin()) // [+]PPA fix vector iterator not decrementable
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
				if (j != m_notShared.begin()) // [+]PPA fix vector iterator not decrementable
					--j;
			}
		}
	}
	
	return bytesAdded;
}

bool ShareManager::findByRealPathName(const string& realPathname, TTHValue* outTTHPtr, string* outfilenamePtr /*= NULL*/, int64_t* outSizePtr/* = NULL*/) // [+] SSA
{
	// [+] IRainman fix.
	webrtc::ReadLockScoped l(*g_csShare);
	
	const Directory::Ptr d = ShareManager::getDirectoryL(realPathname);
	if (!d)
		return false;
	// [~] IRainman fix.
	
	const auto iFile =  d->findFileL(Util::getFileName(realPathname));
	if (iFile != d->m_files.end())
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
