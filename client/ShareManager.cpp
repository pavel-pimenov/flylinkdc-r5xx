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

bool ShareManager::g_ignoreFileSizeHFS = false; // http://www.flylinkdc.ru/2015/01/hfs-mac-windows.html
size_t ShareManager::g_hits = 0;
int ShareManager::g_RebuildIndexes = 0;
std::unique_ptr<webrtc::RWLockWrapper> ShareManager::g_csBloom = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ShareManager::g_csDirList = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ShareManager::g_csTTHIndex = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ShareManager::g_csShare = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ShareManager::g_csShareNotExists = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ShareManager::g_csShareCache = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
QueryNotExistsSet ShareManager::g_file_not_exists_set;
QueryCacheMap ShareManager::g_file_cache_map;
ShareManager::HashFileMap ShareManager::g_tthIndex;
ShareManager::ShareMap ShareManager::g_shares;
ShareManager::ShareMap ShareManager::g_lost_shares;
int64_t ShareManager::g_lastSharedDate = 0;
unsigned ShareManager::g_lastSharedFiles = 0;
StringList ShareManager::g_notShared;
bool ShareManager::g_isNeedsUpdateShareSize;
int64_t ShareManager::g_CurrentShareSize = -1;
bool ShareManager::g_is_initial = true;
ShareManager::DirList ShareManager::g_list_directories;
BloomFilter<5> ShareManager::g_bloom(1 << 20);

ShareManager::ShareManager() : xmlListLen(0), bzXmlListLen(0),
	m_is_xmlDirty(true), m_is_forceXmlRefresh(false), m_is_refreshDirs(false), m_is_update(false), m_listN(0), m_count_sec(11),
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
	m_sweep_guard(false),
#endif
	m_sweep_path(false)
	//m_updateXmlListInProcess(false), //
	//m_is_refreshing(false)
{
	m_lastXmlUpdate = m_lastFullUpdate = GET_TICK();
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
			LogManager::message("Error create: " + emptyXmlName + " error = " + e.getError());
		}
	}
#endif // IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	
	TimerManager::getInstance()->addListener(this);
	QueueManager::getInstance()->addListener(this);
	HashManager::getInstance()->addListener(this);
}

ShareManager::~ShareManager()
{
	dcassert(ClientManager::isBeforeShutdown());
	
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
void ShareManager::shutdown()
{
	dcassert(CFlylinkDBManager::isValidInstance());
	if (g_CurrentShareSize > 0 && CFlylinkDBManager::isValidInstance())
	{
		CFlylinkDBManager::getInstance()->set_registry_variable_int64(e_LastShareSize, g_CurrentShareSize);
	}
	internalClearCache(true);
}

ShareManager::Directory::Directory(const string& aName, const ShareManager::Directory::Ptr& aParent) :
	CFlyLowerName(aName),
	m_size(0),
	parent(aParent.get()),
	m_fileTypes_bitmap(1 << Search::TYPE_DIRECTORY)
{
	initLowerName();
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
	
	void ShareManager::Directory::addType(Search::TypeModes p_type) noexcept
	{
		if (!hasType(p_type))
		{
			m_fileTypes_bitmap |= (1 << p_type);
			if (getParent())
			{
				getParent()->addType(p_type);
			}
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
		return ShareManager::findRealRootL(getName(), path);
	}
}

string ShareManager::findRealRootL(const string& virtualRoot, const string& virtualPath)
{
	for (auto i = g_shares.cbegin(); i != g_shares.cend(); ++i)
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

int64_t ShareManager::Directory::getDirSizeL() const noexcept
{
    dcassert(!ClientManager::isBeforeShutdown());
    int64_t l_tmp = m_size;
    for (auto i = m_share_directories.cbegin(); i != m_share_directories.cend(); ++i)
{
l_tmp += i->second->getDirSizeL();
}
#ifdef _DEBUG
LogManager::message("ShareManager::Directory::getDirSizeL = " + Util::toString(l_tmp) + " getRealPath = " + getFullName());
#endif
return l_tmp;
}

bool ShareManager::destinationShared(const string& file_or_dir_name) // [+] IRainman opt.
{
	CFlyReadLock(*g_csShare);
	for (auto i = g_shares.cbegin(); i != g_shares.cend(); ++i)
	{
		if (strnicmp(i->first, file_or_dir_name, i->first.size()) == 0 && file_or_dir_name[i->first.size() - 1] == PATH_SEPARATOR)
		{
			return true;
		}
	}
	return false;
}

#if 0
bool ShareManager::getRealPathAndSize(const TTHValue& tth, string& path, int64_t& size)
{
	CFlyReadLock(*g_csTTHIndex);
	const auto& i = g_tthIndex.find(tth);
	if (i != g_tthIndex.cend())
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
#endif

bool ShareManager::isTTHShared(const TTHValue& tth)
{
	if (!ClientManager::isBeforeShutdown())
	{
		CFlyReadLock(*g_csTTHIndex);
		return g_tthIndex.find(tth) != g_tthIndex.end();
	}
	return false;
}
string ShareManager::toRealPath(const TTHValue& tth)
{
	CFlyReadLock(*g_csShare);
	{
		CFlyReadLock(*g_csTTHIndex);
		const auto i = g_tthIndex.find(tth);
		if (i != g_tthIndex.end())
		{
			try
			{
				return i->second->getRealPath();
			}
			catch (const ShareException&) {}
		}
	}
	return Util::emptyString;
}

#ifdef _DEBUG
string ShareManager::toVirtual(const TTHValue& tth)
{
	if (tth == bzXmlRoot)
	{
		return Transfer::g_user_list_name_bz;
	}
	else if (tth == xmlRoot)
	{
		return Transfer::g_user_list_name;
	}
	CFlyReadLock(*g_csShare);
	{
		CFlyReadLock(*g_csTTHIndex);
		const auto& i = g_tthIndex.find(tth);
		if (i != g_tthIndex.end())
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
	TTHValue l_tth;
	const auto l_real_path = findFileAndRealPath(virtualFile, l_tth, false);
	return l_real_path;
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
	TTHValue l_tth;
	findFileAndRealPath(virtualFile, l_tth, true);
	return l_tth;
}

MemoryInputStream* ShareManager::getTree(const string& virtualFile) const
{
	TigerTree tree;
	__int64 l_block_size;
	if (virtualFile.compare(0, 4, "TTH/", 4) == 0)
	{
		if (!CFlylinkDBManager::getInstance()->get_tree(TTHValue(virtualFile.substr(4)), tree, l_block_size))
			return 0;
	}
	else
	{
		try
		{
			const TTHValue tth = getTTH(virtualFile);
			CFlylinkDBManager::getInstance()->get_tree(tth, tree, l_block_size);
		}
		catch (const Exception&)
		{
			return 0;
		}
	}
	
	ByteVector buf;
	tree.getLeafData(buf);
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
	CFlyReadLock(*g_csTTHIndex);
	const auto& i = g_tthIndex.find(val);
	if (i == g_tthIndex.end())
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
	
	CFlyReadLock(*g_csDirList);
	auto dmi = getByVirtualL(virtualPath.substr(1, i - 1));
	if (dmi == g_list_directories.end())
	{
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualPath);
	}
	
	Directory::Ptr d = *dmi;
	
	string::size_type j = i + 1;
	while ((i = virtualPath.find('/', j)) != string::npos)
	{
		const auto& mi = d->m_share_directories.find(virtualPath.substr(j, i - j));
		j = i + 1;
		if (mi == d->m_share_directories.end())
			throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualPath);
		d = mi->second;
	}
	
	return make_pair(d, virtualPath.substr(j));
}
void ShareManager::checkShutdown(const string& virtualFile) const
{
	if (ClientManager::isBeforeShutdown())
	{
		// fix https://drdump.com/Problem.aspx?ProblemID=185332
		throw ShareException("FlylinkDC++ shutdown in progress", virtualFile);
	}
}
string ShareManager::findFileAndRealPath(const string& virtualFile, TTHValue& p_tth, bool p_is_fetch_tth) const
{
	CFlyReadLock(*g_csShare);
	if (virtualFile.compare(0, 4, "TTH/", 4) == 0)
	{
		CFlyReadLock(*g_csTTHIndex);
		const auto i = g_tthIndex.find(TTHValue(virtualFile.substr(4)));
		if (i == g_tthIndex.end())
		{
			throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualFile);
		}
		checkShutdown(virtualFile);
		if (p_is_fetch_tth)
		{
			p_tth = i->second->getTTH(); // https://drdump.com/DumpGroup.aspx?DumpGroupID=555791&Login=guest
		}
		return i->second->getRealPath();
	}
	
	const auto v = splitVirtualL(virtualFile);
	const auto it = std::find_if(v.first->m_share_files.begin(), v.first->m_share_files.end(),
	                             [&](const Directory::ShareFile & p_file) -> bool {return stricmp(p_file.getName(), v.second) == 0;}
	                             //Directory::ShareFile::StringComp(v.second)
	                            );
	if (it == v.first->m_share_files.end())
	{
		throw ShareException(UserConnection::g_FILE_NOT_AVAILABLE, virtualFile);
	}
	checkShutdown(virtualFile);
	if (p_is_fetch_tth)
	{
		p_tth = it->getTTH();
	}
	return it->getRealPath();
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

void ShareManager::load(SimpleXML& aXml)
{
	CFlyBusy l_busy(g_RebuildIndexes);
	CFlyWriteLock(*g_csShare);
#ifdef FLYLINKDC_USE_OLD_INNOSETUP_WIZARD
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
				g_lost_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, 0)));
				continue;
			}
			g_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, 0)));
			{
				CFlyWriteLock(*g_csDirList);
				if (getByVirtualL(vName) == g_list_directories.end())
				{
					g_list_directories.push_back(Directory::create(vName));
#ifdef FLYLINKDC_USE_OLD_INNOSETUP_WIZARD
					l_count_dir++;
#endif
				}
			}
		}
		aXml.stepOut();
	}
	aXml.resetCurrentChild();
	if (aXml.findChild("NoShare"))
	{
		aXml.stepIn();
		while (aXml.findChild("Directory"))
			g_notShared.push_back(aXml.getChildData());
			
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
			if (g_shares.find(realPath) == g_shares.end())
			{
				const string virtualName = aXml.getChildAttrib("Virtual");
				const string vName = validateVirtual(virtualName.empty() ? Util::getLastDir(realPath) : virtualName);
				g_lost_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, 0)));
				if (File::isExist(realPath)) // ≈сли каталог по€вилс€ - добавим его в запрос на возврат в шару...
				{
					string l_message_lost_share = "\r\n< " + virtualName + " >  " + realPath + "\r\n";
					tstring l_message = TSTRING(RESTORE_LOST_SHARE);
					l_message += Text::toT(l_message_lost_share);
					g_lost_shares.erase(realPath);
					if (MessageBox(NULL, l_message.c_str() , getFlylinkDCAppCaptionWithVersionT().c_str(), MB_YESNO | MB_ICONQUESTION | MB_TOPMOST) == IDYES)
					{
						g_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, 0)));
					}
				}
			}
		}
		aXml.stepOut();
	}
#ifdef FLYLINKDC_USE_OLD_INNOSETUP_WIZARD
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
		ShareLoader() : cur(nullptr), m_depth(0) { }
		void startTag(const string& p_name, StringPairList& p_attribs, bool p_simple)
		{
			// CFlyLock(g_csDirList); не нужно - лочим выше
			if (p_name == g_SDirectory)
			{
				const string& name = getAttrib(p_attribs, g_SName, 0);
				if (!name.empty())
				{
					if (m_depth == 0)
					{
						for (auto i = ShareManager::g_list_directories.cbegin(); i != ShareManager::g_list_directories.cend(); ++i)
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
						cur->getParent()->m_share_directories[cur->getName()] = cur;
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
					m_depth++;
				}
			}
			else if (cur && p_name == g_SFile)
			{
				const string& fname = getAttrib(p_attribs, g_SName, 0);
				const string& size = getAttrib(p_attribs, g_SSize, 1);
				const string& root = getAttrib(p_attribs, g_STTH, 2);
				// Ёто атрибуты фла€
				const auto l_time_stamp = atoi(getAttrib(p_attribs, g_STS, 3).c_str());
				const auto l_hit_count = getAttrib(p_attribs, g_SHit, 3);
				if (fname.empty() || size.empty() || root.size() != 39)
				{
					dcdebug("Invalid file found: %s\n", fname.c_str());
					return;
				}
				auto it = cur->m_share_files.insert(ShareManager::Directory::ShareFile(fname, Util::toInt64(size), cur, TTHValue(root),
				                                                                       atoi(l_hit_count.c_str()),
				                                                                       l_time_stamp,
				                                                                       ShareManager::getFType(fname)
				                                                                      )
				                                   );
				dcassert(it.second);
				auto f = const_cast<ShareManager::Directory::ShareFile*>(&(*it.first));
				f->initLowerName();
				if (it.second && p_attribs.size() > 4) // Ёто уже наша шара. тут медиаинфа если больше 4-х.
				{
					const string& l_audio = getAttrib(p_attribs, g_SMAudio, 3);
					const string& l_video = getAttrib(p_attribs, g_SMVideo, 3);
					if (!l_audio.empty() || !l_video.empty())
					{
						auto l_media_ptr = std::make_shared<CFlyMediaInfo>(getAttrib(p_attribs, g_SWH, 3),
						                                                   atoi(getAttrib(p_attribs, g_SBR, 4).c_str()),
						                                                   l_audio,
						                                                   l_video);
						l_media_ptr->calcEscape();
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
				m_depth--;
				if (cur)
				{
					cur = cur->getParent();
				}
			}
		}
		
	private:
		ShareManager::Directory::Ptr cur;
		size_t m_depth;
};

bool ShareManager::loadCache() noexcept
{
	try
	{
		CFlyLog l_cache_loader_log("[Share cache loader]");
		{
			CFlyWriteLock(*g_csDirList);
			ShareLoader loader;
			SimpleXMLReader xml(&loader);
			const string& cacheFile = getDefaultBZXmlFile();
			{
				File ff(cacheFile, File::READ, File::OPEN); // [!] FlylinkDC: getDefaultBZXmlFile()
				FilteredInputStream<UnBZFilter, false> f(&ff);
				l_cache_loader_log.step("read and uncompress " + cacheFile + " done");
				xml.parse(f);
			}
		}
		
		l_cache_loader_log.step("parse xml done");
		{
			{
				CFlyBusy l_busy(g_RebuildIndexes);
				CFlyWriteLock(*g_csShare);
				{
					CFlyReadLock(*g_csDirList);
					for (auto i = g_list_directories.cbegin(); i != g_list_directories.cend(); ++i)
					{
						updateIndicesDirL(**i);
					}
				}
			}
			internalClearCache(true);
			l_cache_loader_log.step("update indices done");
			//internalClearCache(true);
			//l_cache_loader_log.step("internalClearCache");
			if (getShareSize() >= 0)
			{
				// ѕолучили размер шары из кэша - не выполн€ем повторный обход в internalCalcShareSize();
				g_isNeedsUpdateShareSize = false;
				g_CurrentShareSize = getShareSize();
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
		internalClearCache(true);
		dcdebug("%s\n", e.getError().c_str());
	}
	return false;
}

void ShareManager::save(SimpleXML& aXml)
{
	CFlyReadLock(*g_csShare);
	
	aXml.addTag("Share");
	aXml.stepIn();
	for (auto i = g_shares.cbegin(); i != g_shares.cend(); ++i)
	{
		aXml.addTag("Directory", i->first);
		aXml.addChildAttrib("Virtual", i->second.m_synonym);
	}
	aXml.stepOut();
	
	aXml.addTag("LostShare");
	aXml.stepIn();
	for (auto i = g_lost_shares.cbegin(); i != g_lost_shares.cend(); ++i)
	{
		aXml.addTag("Directory", i->first);
		aXml.addChildAttrib("Virtual", i->second.m_synonym);
	}
	aXml.stepOut();
	
	
	aXml.addTag("NoShare");
	aXml.stepIn();
	for (auto j = g_notShared.cbegin(); j != g_notShared.cend(); ++j)
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
			_snprintf(&l_error[0], l_error.size(), CSTRING(CHECK_FORBIDDEN_DIR), realPath.c_str(), g_forbiddenPaths[i].m_descr);
			throw ShareException(l_error, realPath); // [14] https://www.box.net/shared/a7b3712835bebeea8a46
		}
	}
	// [~] FlylinkDC.
#endif
	
	list<string> removeList;
	{
		ShareMap a;
		{
			CFlyReadLock(*g_csShare);
			a = g_shares;
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
		if (!p_is_job)
		{
			CFlylinkDBManager::getInstance()->scan_path(directories);
		}
		rebuildSkipList();
		{
			CFlyBusy l_busy(g_RebuildIndexes);
			CFlyWriteLock(*g_csShare);
			{
				__int64 l_path_id = 0;
				Directory::Ptr dp = buildTreeL(l_path_id, realPath, Directory::Ptr(), p_is_job);
				
				const string vName = validateVirtual(virtualName);
				dp->setNameAndLower(vName);
				
				g_shares.insert(std::make_pair(realPath, CFlyBaseDirItem(vName, l_path_id)));
				{
					CFlyWriteLock(*g_csDirList); // Ёта блокировка выше TTHINdex
					{
						CFlyWriteLock(*g_csTTHIndex);
						updateIndicesDirL(*get_mergeL(dp));
					}
				}
			}
		}
		setDirty();
	}
}
void ShareManager::rebuildSkipList()
{
	auto skipList = SPLIT_SETTING_AND_LOWER(SKIPLIST_SHARE);
	CFlyFastLock(m_csSkipList);
	swap(m_skipList, skipList);
}

ShareManager::Directory::Ptr ShareManager::get_mergeL(const Directory::Ptr& directory)
{
	for (auto i = g_list_directories.cbegin(); i != g_list_directories.cend(); ++i)
	{
		if (stricmp((*i)->getName(), directory->getName()) == 0) // TODO toLower()
		{
			dcdebug("Merging directory %s\n", directory->getName().c_str());
			(*i)->mergeL(directory);
			return *i;
		}
	}
	
	dcdebug("Adding new directory %s\n", directory->getName().c_str());
	
	g_list_directories.push_back(directory);
	return directory;
}

void ShareManager::Directory::mergeL(const Directory::Ptr& source)
{
	for (auto i = source->m_share_directories.cbegin(); i != source->m_share_directories.cend(); ++i)
	{
		Directory::Ptr subSource = i->second;
		
		const auto& ti = m_share_directories.find(subSource->getName());
		if (ti == m_share_directories.end())
		{
			if (findFileIterL(subSource->getName()) != m_share_files.end())
			{
				dcdebug("File named the same as directory");
			}
			else
			{
				m_share_directories.insert(std::make_pair(subSource->getName(), subSource));
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
	source->m_share_directories.clear();
	
	for (auto i = source->m_share_files.cbegin(); i != source->m_share_files.cend(); ++i)
	{
		if (findFileIterL(i->getName()) == m_share_files.end())
		{
			if (m_share_directories.find(i->getName()) != m_share_directories.end())
			{
				dcdebug("Directory named the same as file");
			}
			else
			{
				//auto& added = files.insert(*i);
				auto it = m_share_files.insert(*i);
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
		CFlyBusy l_busy(g_RebuildIndexes);
		CFlyWriteLock(*g_csShare);
		auto i = g_shares.find(realPath);
		if (i == g_shares.end())
		{
			return;
		}
		const string l_Name = i->second.m_synonym; // —сылку не делать. fix http://www.flickr.com/photos/96019675@N02/9515345001/
		{
			CFlyWriteLock(*g_csDirList);
			for (auto j = g_list_directories.cbegin(); j != g_list_directories.cend();)
			{
				if (stricmp((*j)->getName(), l_Name) == 0)
				{
					g_list_directories.erase(j++);
				}
				else
				{
					++j;
				}
			}
		}
		
		g_shares.erase(i);
		
		HashManager::HashPauser pauser;
		
		// Readd all directories with the same vName
		for (i = g_shares.begin(); i != g_shares.end(); ++i)
		{
			if (stricmp(i->second.m_synonym, l_Name) == 0 && checkAttributs(i->first))// [!]IRainman checkHidden(i->first)
			{
				Directory::Ptr dp = buildTreeL(i->second.m_path_id, i->first, Directory::Ptr(), true);
				dp->setNameAndLower(i->second.m_synonym);
				{
					CFlyWriteLock(*g_csDirList);
					get_mergeL(dp);
				}
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

ShareManager::DirList::const_iterator ShareManager::getByVirtualL(const string& virtualName)
{
	for (auto i = g_list_directories.cbegin(); i != g_list_directories.cend(); ++i)
	{
		if (stricmp((*i)->getName(), virtualName) == 0) // TODO toLower
		{
			return i;
		}
	}
	return g_list_directories.end();
}

int64_t ShareManager::getShareSize(const string& realPath)
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!realPath.empty());
	CFlyReadLock(*g_csShare);
	const auto i = g_shares.find(realPath);
	if (i != g_shares.end())
	{
		CFlyReadLock(*g_csDirList);
		const auto j = getByVirtualL(i->second.m_synonym);
		if (j != g_list_directories.end())
		{
			return (*j)->getDirSizeL();
		}
	}
	return -1;
}
int64_t ShareManager::getShareSize()
{
	if (g_CurrentShareSize == -1)
	{
		dcassert(CFlylinkDBManager::isValidInstance());
		if (CFlylinkDBManager::isValidInstance())
		{
			g_CurrentShareSize = CFlylinkDBManager::getInstance()->get_registry_variable_int64(e_LastShareSize);
		}
	}
	return g_CurrentShareSize;
}

void ShareManager::internalCalcShareSize() // [!] IRainman opt.
{
	if (g_isNeedsUpdateShareSize && g_RebuildIndexes == 0)
	{
		if (!ClientManager::isBeforeShutdown())
		{
			g_isNeedsUpdateShareSize = false;
			int64_t l_CurrentShareSize = 0;
			{
				CFlyReadLock(*g_csTTHIndex);
				for (auto i = g_tthIndex.cbegin(); i != g_tthIndex.cend(); ++i)
				{
					l_CurrentShareSize += i->second->getSize(); // https://drdump.com/DumpGroup.aspx?DumpGroupID=532748
				}
				g_lastSharedFiles = unsigned(g_tthIndex.size());
			}
			g_CurrentShareSize = l_CurrentShareSize;
		}
	}
}

ShareManager::Directory::Ptr ShareManager::buildTreeL(__int64& p_path_id, const string& aName, const Directory::Ptr& aParent, bool p_is_job)
{

	bool p_is_no_mediainfo = false;
	if (p_path_id == 0)
	{
		p_path_id = CFlylinkDBManager::getInstance()->get_path_id(Text::toLower(aName), !p_is_job, false, p_is_no_mediainfo, m_sweep_path);
	}
	Directory::Ptr dir = Directory::create(Util::getLastDir(aName), aParent);
	
	auto l_lastFileIter = dir->m_share_files.begin();
	
	CFlyDirMap l_dir_map;
	if (p_path_id)
		CFlylinkDBManager::getInstance()->load_dir(p_path_id, l_dir_map, p_is_no_mediainfo);
	for (FileFindIter i(aName + '*'); !ClientManager::isBeforeShutdown() && i != FileFindIter::end; ++i)// [!]IRainman add m_close [10] https://www.box.net/shared/067924cecdb252c9d26c
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
		if (i->isDirectory())
		{
			const string newName = aName + l_file_name + PATH_SEPARATOR;
			
			if (Util::checkForbidenFolders(newName))
				continue;
				
			if (stricmp(newName, SETTING(TEMP_DOWNLOAD_DIRECTORY)) != 0
			        && stricmp(newName, Util::getConfigPath())  != 0//[+]IRainman
			        && stricmp(newName, SETTING(LOG_DIRECTORY)) != 0//[+]IRainman
			        && isShareFolder(newName))
			{
				__int64 l_path_id = 0;
				dir->m_share_directories[l_file_name] = buildTreeL(l_path_id, newName, dir, p_is_job);
			}
		}
		else
		{
			// Not a directory, assume it's a file...make sure we're not sharing the settings file...
			const auto l_ext = Util::getFileExtWithoutDot(l_lower_name);
			if (BOOLSETTING(XXX_BLOCK_SHARE) && CFlyServerConfig::isVideoShareExt(l_ext))
			{
				bool l_is_xxx = false;
				const auto l_find_xxx = l_lower_name.find("pthc");
				if (l_find_xxx != string::npos)
				{
					if (l_find_xxx > 0 && (l_find_xxx + 5) < l_lower_name.size())
					{
						if (!isalpha(l_lower_name[l_find_xxx + 4]) &&
						        !isalpha(l_lower_name[l_find_xxx - 1]))
						{
							l_is_xxx = true;
						}
					}
					if (l_is_xxx)
					{
						LogManager::message(STRING(USER_DENIED_SHARE_THIS_FILE) + ' ' + aName + l_file_name + " (XXX - File!)");
						continue;
					}
				}
			}
			if (!g_fly_server_config.isBlockShareExt(l_lower_name, l_ext))
			{
				if (!isSkipListEmpty())
				{
					if (isInSkipList(l_lower_name))
					{
						const auto l_ext = Util::getFileExtWithoutDot(l_lower_name);
						if (l_ext != "!ut") //  && l_ext != "crdownload"
						{
							// jc!, ob!, dmf, mta, dmfr, !ut, !bt, bc!, getright, antifrag, pusd, dusd, download, crdownload
							LogManager::message(STRING(USER_DENIED_SHARE_THIS_FILE) + ' ' + l_file_name
							                    + " (" + STRING(SIZE) + ": " + Util::toString(i->getSize()) + ' '
							                    + STRING(B) + ") (" + STRING(DIRECTORY) + ": \"" + aName + "\")");
						}
						continue;
					}
				}
				const string l_PathAndFileName = aName + l_file_name;
				if (stricmp(l_PathAndFileName, SETTING(TLS_PRIVATE_KEY_FILE)) == 0) // TODO - унести проверку в другое место.
					continue;
				int64_t l_size = i->getSize();
				if (i->isLink() && l_size == 0) // https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/14
				{
					try
					{
						File l_get_size(l_PathAndFileName, File::READ, File::OPEN | File::SHARED);
						l_size = l_get_size.getSize();
					}
					catch (FileException& e)
					{
						const auto l_error_code = GetLastError();
						string l_error = "Error share link file: " + l_PathAndFileName + " error = " + e.getError();
						LogManager::message(l_error);
						// https://msdn.microsoft.com/en-us/library/windows/desktop/ms681382%28v=vs.85%29.aspx
						if (l_error_code != 1920 && l_error_code != 2 && l_error_code != 3)
						{
							CFlyServerJSON::pushError(37, l_error);
						}
						continue;
					}
				}
				const int64_t l_ts = i->getLastWriteTime();
				CFlyDirMap::iterator l_dir_item = l_dir_map.find(l_lower_name);
				bool l_is_new_file = l_dir_item == l_dir_map.end();
				if (!l_is_new_file)
				{
					l_is_new_file = l_dir_item->second.m_TimeStamp != l_ts || (l_dir_item->second.m_size != l_size && g_ignoreFileSizeHFS == false);
#if 0
					if (l_is_new_file)
					{
						LogManager::message("[!!!!!!!!][1] bool l_is_new_file = l_dir_item == l_dir_map.end(); l_lower_name = " + l_lower_name + " name = " + l_file_name);
						LogManager::message("[!!!!!!!!][1] l_dir_item->second.m_size = " + Util::toString(l_dir_item->second.m_size)
						                    + " size = " + Util::toString(l_size)
						                    + " l_dir_item->second.m_TimeStamp = " + Util::toString(l_dir_item->second.m_TimeStamp)
						                    + " l_ts = " + Util::toString(l_ts)
						                   );
					}
#endif
				}
				else
				{
					// LogManager::message("[!!!!!!!!][2] bool l_is_new_file = l_dir_item == l_dir_map.end(); l_lower_name = " + l_lower_name + " name = " + l_file_name);
				}
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
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
						l_lastFileIter = dir->m_share_files.insert(l_lastFileIter,
						                                           Directory::ShareFile(l_file_name,
						                                                                l_dir_item_second.m_size,
						                                                                dir,
						                                                                l_dir_item_second.m_tth,
						                                                                l_dir_item_second.m_hit,
						                                                                uint32_t(l_dir_item_second.m_StampShare),
						                                                                Search::TypeModes(l_dir_item_second.m_ftype)
						                                                               )
						                                          );
						auto f = const_cast<ShareManager::Directory::ShareFile*>(&(*l_lastFileIter));
						f->initLowerName();
						if (l_dir_item_second.m_StampShare > g_lastSharedDate)
						{
							g_lastSharedDate = l_dir_item_second.m_StampShare;
						}
						f->initMediainfo(l_dir_item_second.m_media_ptr);
						l_dir_item_second.m_media_ptr = nullptr;
					}
				}
				catch (const HashException&)
				{
				}
			}
		}
	}
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
	if (l_path_id && !m_sweep_guard)
		CFlylinkDBManager::getInstance()->sweep_files(l_path_id, l_dir_map);
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

bool ShareManager::updateIndicesDirL(Directory& dir)
{
	if (!ClientManager::isBeforeShutdown())
	{
		{
			CFlyWriteLock(*g_csBloom);
			dcassert(Text::toLower(dir.getName()) == dir.getLowName());
			g_bloom.add(dir.getLowName());
		}
		
		for (auto i = dir.m_share_directories.cbegin(); i != dir.m_share_directories.cend(); ++i)
		{
			if (updateIndicesDirL(*i->second) == false) // Recursion
			{
				return false;
			}
		}
		
		dir.m_size = 0;
		CFlyWriteLock(*g_csBloom);
		for (auto i = dir.m_share_files.cbegin(); i != dir.m_share_files.cend();)
		{
			if (updateIndicesFileL(dir, i++) == false)
			{
				return false;
			}
		}
		return true;
	}
	return false;
}

void ShareManager::rebuildIndicesL()
{
	if (!ClientManager::isBeforeShutdown())
	{
		{
			CFlyWriteLock(*g_csTTHIndex);
			g_tthIndex.clear();
		}
		{
			CFlyWriteLock(*g_csBloom);
			g_bloom.clear();
		}
		{
			CFlyReadLock(*g_csDirList); // Ёта блокировка выше TTHINdex
			{
				CFlyReadLock(*g_csTTHIndex);
				for (auto i = g_list_directories.cbegin(); i != g_list_directories.cend(); ++i)
				{
					if (updateIndicesDirL(**i) == false)
						break;
				}
			}
		}
		g_isNeedsUpdateShareSize = true;
	}
}

bool ShareManager::updateIndicesFileL(Directory& dir, const Directory::ShareFile::Set::iterator& i)
{
	if (!ClientManager::isBeforeShutdown())
	{
		const auto& f = *i;
		{
			auto j = g_tthIndex.find(f.getTTH());
			if (j == g_tthIndex.end())
			{
				dir.m_size += f.getSize();
				g_tthIndex.insert(make_pair(f.getTTH(), i));
				g_isNeedsUpdateShareSize = true;
			}
			else
			{
				j->second = i;
			}
			dir.addType(f.getFType());
		}
		dcassert(Text::toLower(f.getName()) == f.getLowName());
		g_bloom.add(f.getLowName());
#ifdef STRONG_USE_DHT
		if (!ClientManager::isBeforeShutdown())
		{
			if (g_is_initial == false && dht::IndexManager::isTimeForPublishing() && BOOLSETTING(USE_DHT))
			{
				dht::IndexManager::publishFile(f.getTTH(), f.getSize());
			}
		}
		else
		{
			return false;
		}
#endif
		return true;
	}
	return false;
}

void ShareManager::refresh_share(bool p_dirs /* = false */, bool aUpdate /* = true */) noexcept
{
	if (m_is_refreshing.test_and_set())
	{
		LogManager::message(STRING(FILE_LIST_REFRESH_IN_PROGRESS));
		return;
	}
	
	m_is_update = aUpdate;
	m_is_refreshDirs = p_dirs;
	join();
	bool l_is_cached;
	if (g_is_initial)
	{
		l_is_cached = loadCache();
		g_is_initial = false;
	}
	else
	{
		l_is_cached = false;
	}
	
	dht::IndexManager::setTimeForPublishing();
	
	try
	{
		start(0);
		if (!l_is_cached)
		{
			// - вешаемс€ join();
		}
		else
		{
			//setThreadPriority(Thread::LOW);
		}
	}
	catch (const ThreadException& e)
	{
		LogManager::message(STRING(FILE_LIST_REFRESH_FAILED) + ' ' + e.getError());
	}
}

void ShareManager::getDirectories(CFlyDirItemArray& p_dirs)
{
	dcassert(p_dirs.empty());
	p_dirs.clear();
	CFlyReadLock(*g_csShare);
	p_dirs.reserve(g_shares.size());
	for (auto i = g_shares.cbegin(); i != g_shares.cend(); ++i)
	{
		p_dirs.push_back(CFlyDirItem(i->second.m_synonym, i->first, 0));
	}
}

int ShareManager::run()
{
	static bool g_is_first = false;
	if (g_is_first == false)
	{
		g_is_first = true;
		for (int i = 0; i < 50 * 10; i++) // ∆дем 5 сек
		{
			::Sleep(10);
			if (ClientManager::isBeforeShutdown())
			{
				return 0;
			}
		}
	}
	CFlyDirItemArray directories;
	getDirectories(directories);
	// Don't need to refresh if no directories are shared
	if (directories.empty())
	{
		m_is_refreshDirs = false;
	}
	
	if (m_is_refreshDirs)
	{
		HashManager::HashPauser pauser;
		
		LogManager::message(STRING(FILE_LIST_REFRESH_INITIATED));
		m_lastFullUpdate = GET_TICK();
		CFlylinkDBManager::getInstance()->scan_path(directories);
		rebuildSkipList();
		DirList newDirs;
		{
			CFlyBusy l_busy(g_RebuildIndexes);
			CFlyWriteLock(*g_csShare);
			for (auto i = directories.begin(); i != directories.end(); ++i)
			{
				if (checkAttributs(i->m_path))
				{
					Directory::Ptr dp = buildTreeL(i->m_path_id, i->m_path, Directory::Ptr(), false);
					dp->setNameAndLower(i->m_synonym);
					newDirs.push_back(dp);
				}
			}
		}
		if (m_sweep_path)
		{
			m_sweep_path = false;
#ifdef FLYLINKDC_USE_ONLINE_SWEEP_DB
			m_sweep_guard = true;
#endif
			CFlylinkDBManager::getInstance()->sweep_db();
		}
		
		{
			CFlyBusy l_busy(g_RebuildIndexes);
			
			CFlyWriteLock(*g_csShare);
			{
				CFlyWriteLock(*g_csDirList);
				g_list_directories.clear();
				for (auto i = newDirs.cbegin(); i != newDirs.cend(); ++i)
				{
					get_mergeL(*i);
				}
			}
			rebuildIndicesL();
		}
		internalCalcShareSize();
		m_is_refreshDirs = false;
		LogManager::message(STRING(FILE_LIST_REFRESH_FINISHED));
	}
	if (!ClientManager::isBeforeShutdown())
	{
		if (m_is_update)
		{
			ClientManager::infoUpdated();
		}
		
#ifdef STRONG_USE_DHT
		if (dht::IndexManager::isTimeForPublishing())
		{
			dht::IndexManager::setNextPublishing();
		}
		
#endif
	}
	m_is_refreshing.clear();
	CFlylinkDBManager::getInstance()->flush_hash();
	return 0;
}

void ShareManager::getBloom(ByteVector& v, size_t k, size_t m, size_t h)
{
	dcdebug("Creating bloom filter, k=%u, m=%u, h=%u\n", unsigned(k), unsigned(m), unsigned(h));
	HashBloom bloom;
	bloom.reset(k, m, h);
	{
		CFlyReadLock(*g_csTTHIndex);
		for (auto i = g_tthIndex.cbegin(); i != g_tthIndex.cend(); ++i)
		{
			bloom.add(i->first);
		}
	}
	bloom.copy_to(v);
}

void ShareManager::generateXmlList()
{
	if (m_updateXmlListInProcess.test_and_set()) // [+] IRainman opt.
	{
		return;
	}
	
	if (m_is_forceXmlRefresh || (m_is_xmlDirty && (m_lastXmlUpdate + 15 * 60 * 1000 < GET_TICK() || m_lastXmlUpdate <= m_lastFullUpdate)))
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
					CFlyReadLock(*g_csShare);
					{
						CFlyReadLock(*g_csDirList);
						for (auto i = g_list_directories.cbegin(); i != g_list_directories.cend(); ++i)
						{
							(*i)->toXml(newXmlFile, indent, tmp2, true); // https://www.box.net/shared/e9d04cfcc59d4a4aaba7
						}
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
			
			const string l_XmlListFileName = getDefaultBZXmlFile();
			
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
		
		m_is_xmlDirty = false;
		m_is_forceXmlRefresh = false;
		m_lastXmlUpdate = GET_TICK();
		
		// [+] IRainman cleaning old file cache
		l_creation_log.step("Clean old cache");
		const StringList& l_ToDelete = File::findFiles(Util::getConfigPath(), "files*.xml.bz2", false);
		const auto l_bz_xml_file = getBZXmlFile();
		const auto l_def_bz_xml_file = getDefaultBZXmlFile();
		for (auto i = l_ToDelete.cbegin(); i != l_ToDelete.cend(); ++i)
		{
			const auto l_file = Util::getConfigPath() + *i;
			if (l_file != l_bz_xml_file && l_file != l_def_bz_xml_file)
			{
				if (File::deleteFile(l_file))
				{
					l_creation_log.log("Delete: " + l_file);
				}
				else
				{
					l_creation_log.log("Error delete: " + l_file + "[ " + Util::translateError() + "]");
				}
			}
		}
		// [~] IRainman cleaning old file cache
	}
	m_updateXmlListInProcess.clear(); // [+] IRainman opt.
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
		CFlyReadLock(*g_csDirList);
		for (auto i = g_list_directories.cbegin(); i != g_list_directories.cend(); ++i)
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
		
		CFlyReadLock(*g_csDirList);
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
				
				if (it == g_list_directories.end())
					return nullptr;
					
				root = *it;
			}
			else
			{
				const auto&  it2 = root->m_share_directories.find(dir.substr(j, i - j));
				if (it2 == root->m_share_directories.end())
				{
					return nullptr;
				}
				
				root = it2->second;
			}
			j = i + 1;
		}
		if (!root)
			return 0;
			
		for (auto it2 = root->m_share_directories.cbegin(); it2 != root->m_share_directories.cend(); ++it2)
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
		for (auto i = m_share_directories.cbegin(); i != m_share_directories.cend(); ++i)
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
		if (m_share_directories.empty() && m_share_files.empty())
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
	for (auto i = m_share_files.cbegin(); i != m_share_files.cend(); ++i)
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
                                   ".lqt",
                                   ".vqf", ".m4a"
                                 };
static const char* typeCompressed[] = { ".rar", ".zip", ".ace", ".arj", ".hqx", ".lha", ".sea", ".tar", ".tgz", ".uc2", ".bz2", ".lzh", ".cab" };
static const char* typeDocument[] = { ".htm", ".doc", ".txt", ".nfo", ".pdf", ".chm",
                                      ".rtf",
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
                                        ".msi",
                                        ".app", ".bat", ".cmd", ".jar", ".ps1", ".vbs", ".wsf"
                                      };
static const char* typePicture[] = { ".jpg", ".gif", ".png", ".eps", ".img", ".pct", ".psp", ".pic", ".tif", ".rle", ".bmp",
                                     ".pcx", ".jpe", ".dcx", ".emf", ".ico", ".psd", ".tga", ".wmf", ".xif", ".cdr", ".sfw", ".bpg"
                                   };
static const char* typeVideo[] = { ".avi", ".mpg", ".mov", ".flv", ".asf",  ".pxp", ".wmv", ".ogm", ".mkv", ".m1v", ".m2v", ".mpe", ".mps", ".mpv", ".ram", ".vob", ".mp4", ".3gp", ".asx", ".swf",
                                   ".sub", ".srt", ".ass", ".ssa", ".tta", ".3g2", ".f4v", ".m4v", ".ogv"
                                 };


static const char* typeCDImage[] = { ".iso", ".nrg", ".mdf", ".mds", ".vcd", ".bwt", ".ccd", ".cdi", ".pdi", ".cue", ".isz", ".img", ".vc4", ".cso"};

static const char* typeComics[] = { ".cba", ".cbt", ".cbz", ".cb7", ".cbw", ".cbl", ".cba", ".cbr" };
static const char* typeBooks[] = { ".pdf", ".fb2" };
static const string type2Books[] = { ".epub", ".mobi", ".djvu" };

static const string type2Audio[] = { ".au", ".it", ".ra", ".xm", ".aiff", ".flac", ".midi", ".wv"};
static const string type2Document[] = {".xlsx", ".docx", ".pptx", ".html" };
static const string type2Compressed[] = { ".7z",
                                          ".gz", ".tz", ".z"
                                        };
static const string type2Picture[] = { ".ai", ".ps", ".pict", ".jpeg", ".tiff", ".webp" };
static const string type2Video[] = { ".rm", ".divx", ".mpeg", ".mp1v", ".mp2v", ".mpv1", ".mpv2", ".qt", ".rv", ".vivo", ".rmvb", ".webm",
                                     ".ts", ".ps", ".m2ts"
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
		case Search::TYPE_COMICS:
		{
			for (size_t i = 0; i < _countof(typeComics); ++i)
			{
				if (IS_TYPE(typeComics[i]))
				{
					return true;
				}
			}
		}
		break;
		case Search::TYPE_BOOK:
		{
			for (size_t i = 0; i < _countof(typeBooks); i++)
			{
				if (IS_TYPE(typeBooks[i]))
				{
					return true;
				}
			}
			for (size_t i = 0; i < _countof(type2Books); i++)
			{
				if (IS_TYPE2(type2Books[i]))
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

Search::TypeModes ShareManager::getFType(const string& aFileName, bool p_include_flylinkdc_ext /* = false*/) noexcept
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
	
	if (p_include_flylinkdc_ext)
	{
		if (checkType(aFileName, Search::TYPE_COMICS))
			return Search::TYPE_COMICS;
		else if (checkType(aFileName, Search::TYPE_BOOK))
			return Search::TYPE_BOOK;
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
void ShareManager::Directory::search(SearchResultList& aResults, StringSearch::List& aStrings, const SearchParamBase& p_search_param) const noexcept
{
    if (ClientManager::isBeforeShutdown())
    return;
    // Skip everything if there's nothing to find here (doh! =)
    if (!hasType(p_search_param.m_file_type))
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
				p_search_param.m_client->getHubUrlAndIP(),
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
//	LogManager::message(l_buf);
#endif
const bool sizeOk = (p_search_param.m_size_mode != Search::SIZE_ATLEAST) || (p_search_param.m_size == 0);
if ((cur->empty()) &&
        (((p_search_param.m_file_type == Search::TYPE_ANY) && sizeOk) || (p_search_param.m_file_type == Search::TYPE_DIRECTORY)))
{
// We satisfied all the search words! Add the directory...(NMDC searches don't support directory size)
const SearchResult sr(SearchResult::TYPE_DIRECTORY, 0, getFullName(), TTHValue(), -1 /*token*/);
	aResults.push_back(sr);
	ShareManager::incHits();
}

if (p_search_param.m_file_type != Search::TYPE_DIRECTORY)
{
for (auto i = m_share_files.cbegin(); i != m_share_files.cend(); ++i)
	{
	
#ifdef _DEBUG
//		char l_buf[1000] = {0};
//		sprintf(l_buf, "Name = %s, i->getSize() = %lld aSize =  %lld m_sizeMode = %d\r\n", getFullName().c_str(), i->getSize(), aSize,  aSizeMode);
//		LogManager::message(l_buf);
#endif
		if (p_search_param.m_size_mode == Search::SIZE_ATLEAST && p_search_param.m_size > i->getSize())
		{
#ifdef _DEBUG
//			LogManager::message("[минимум] aSizeMode == Search::SIZE_ATLEAST && aSize > i->getSize()");
#endif
			continue;
		}
		else if (p_search_param.m_size_mode == Search::SIZE_ATMOST && p_search_param.m_size < i->getSize())
		{
#ifdef _DEBUG
//			LogManager::message("[максимум] aSizeMode == Search::SIZE_ATMOST && aSize < i->getSize()");
#endif
			continue;
		}
		auto j = cur->begin();
		for (; j != cur->end() && j->matchLower(i->getLowName()); ++j)
		{
		}
		
		if (j != cur->end())
		{
			continue;
		}
		
		// Check file type...
		if (checkType(i->getName(), p_search_param.m_file_type))
		{
			const SearchResult sr(SearchResult::TYPE_FILE, i->getSize(), getFullName() + i->getName(), i->getTTH(), -1  /*token*/);
			aResults.push_back(sr);
			ShareManager::incHits();
			if (aResults.size() >= p_search_param.m_max_results)
			{
				break;
			}
		}
	}
}
for (auto l = m_share_directories.cbegin(); l != m_share_directories.cend() && aResults.size() < p_search_param.m_max_results; ++l)
{
l->second->search(aResults, *cur, p_search_param); //TODO - Hot point
}
}
bool ShareManager::search_tth(const TTHValue& p_tth, SearchResultList& aResults, bool p_is_check_parent)
{
	CFlyReadLock(*g_csTTHIndex);
	const auto& i = g_tthIndex.find(p_tth);
	if (i == g_tthIndex.end())
		return false;
	dcassert(i->second->getParent());
	if (p_is_check_parent && !i->second->getParent())
		return false;
	if (!g_RebuildIndexes) // https://drdump.com/DumpGroup.aspx?DumpGroupID=382746&Login=guest
	{
		const auto &l_fileMap = i->second;
		// TODO - дл€ TTH сильно толстый объект  SearchResult
		const SearchResult sr(SearchResult::TYPE_FILE, l_fileMap->getSize(), l_fileMap->getParent()->getFullName() + l_fileMap->getName(), l_fileMap->getTTH(), -1/*token*/);
		incHits();
		aResults.push_back(sr);
		return true;
	}
	return false;
}
bool ShareManager::searchTTHArray(CFlySearchArrayTTH& p_all_search_array, const Client* p_client)
{
	bool l_result = true;
	CFlyReadLock(*g_csTTHIndex);
	for (auto j = p_all_search_array.begin(); j != p_all_search_array.end(); ++j)
	{
		const auto& i = g_tthIndex.find(j->m_tth);
		if (i == g_tthIndex.end())
		{
			continue;
		}
		if (!g_RebuildIndexes) // https://drdump.com/DumpGroup.aspx?DumpGroupID=382746&Login=guest
		{
			dcassert(i->second->getParent());
			const auto &l_fileMap = i->second;
			SearchResultBaseTTH l_result(SearchResult::TYPE_FILE,
			                             l_fileMap->getSize(),
			                             l_fileMap->getParent()->getFullName() + l_fileMap->getName(),
			                             l_fileMap->getTTH(),
			                             UploadManager::getSlots(),
			                             UploadManager::getFreeSlots()
			                            );
			incHits();
			j->m_toSRCommand = new string(l_result.toSR(*p_client));
			COMMAND_DEBUG("[TTH]$Search " + j->m_search + " TTH = " + j->m_tth.toBase32(), DebugTask::HUB_IN, p_client->getIpPort());
		}
		else
		{
			j->m_is_skip = true;
			l_result = false;
		}
	}
	return l_result;
}

bool ShareManager::isUnknownTTH(const TTHValue& p_tth)
{
	CFlyReadLock(*g_csTTHIndex);
	return g_tthIndex.find(p_tth) == g_tthIndex.end();
}

bool ShareManager::isUnknownFile(const string& p_search)
{
	CFlyReadLock(*g_csShareNotExists);
	return g_file_not_exists_set.find(p_search) != g_file_not_exists_set.end();
}

void ShareManager::addUnknownFile(const string& p_search)
{
	CFlyWriteLock(*g_csShareNotExists);
	g_file_not_exists_set.insert(p_search);
}
bool ShareManager::isCacheFile(const string& p_search, SearchResultList& p_search_result)
{
	CFlyReadLock(*g_csShareCache);
	const auto l_result = g_file_cache_map.find(p_search);
	if (l_result != g_file_cache_map.end())
	{
		p_search_result = l_result->second;
		return true;
	}
	return false;
}
void ShareManager::addCacheFile(const string& p_search, const SearchResultList& p_search_result)
{
	CFlyWriteLock(*g_csShareCache);
	g_file_cache_map.insert(make_pair(p_search, p_search_result));
}
void ShareManager::search(SearchResultList& aResults, const SearchParam& p_search_param) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	if (p_search_param.m_file_type == Search::TYPE_TTH)
	{
		dcassert(isTTHBase64(p_search_param.m_filter));
		// ¬етка пока работает!
		if (isTTHBase64(p_search_param.m_filter)) //[+]FlylinkDC++ opt.
		{
			const TTHValue l_tth(p_search_param.m_filter.c_str() + 4);
			if (search_tth(l_tth, aResults, true) == false)
				return;
#ifdef FLYLINKDC_USE_COLLECT_STAT
			{
				CFlylinkDBManager::getInstance()->push_event_statistic("ShareManager::search",
				"TTH",
				aString,
				"",
				"",
				p_search_param.m_client->getHubUrlAndIP(),
				m_tth.toBase32());
			}
#endif
		}
		else
		{
#ifdef FLYLINKDC_BETA
			if (p_search_param.m_client)
			{
				const string l_error = "Error TTH Search: " + p_search_param.m_filter + " Hub = " + p_search_param.m_client->getHubUrl() + " raw:" + p_search_param.getRAWQuery();
				LogManager::message(l_error);
				CFlyServerJSON::pushError(49, l_error);
			}
#endif
		}
		return;
	}
	const auto l_raw_query = p_search_param.getRAWQuery();
	if (isUnknownFile(l_raw_query))
	{
		return; // ”ходим сразу - у нас в шаре этого не по€вилось.
	}
	if (isCacheFile(l_raw_query, aResults))
	{
		return;
	}
	
	const StringTokenizer<string> t(Text::toLower(p_search_param.m_filter), '$');
	const StringList& sl = t.getTokens();
	{
		bool l_is_bloom;
		{
			CFlyReadLock(*g_csBloom);
			l_is_bloom = g_bloom.match(sl);
		}
		if (!l_is_bloom)
		{
			addUnknownFile(l_raw_query); // TODO - может вынести bloom в глобальную часть.
			return;
		}
	}
	
	StringSearch::List ssl; // TODO - кандидат засунуть в структуру
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
				p_search_param.m_client->getHubUrlAndIP(),
				"");
			}
#endif
			
		}
	}
	if (!ssl.empty())
	{
		CFlyReadLock(*g_csDirList);
		for (auto j = g_list_directories.cbegin(); j != g_list_directories.cend() && aResults.size() < p_search_param.m_max_results; ++j)
		{
			(*j)->search(aResults, ssl, p_search_param);
		}
	}
	// Ќичего не нашли - сохраним условие поиска чтобы не искать второй раз по этому-же запросу.
	if (aResults.empty())
	{
		addUnknownFile(l_raw_query);
	}
	else
	{
		addCacheFile(l_raw_query, aResults);
	}
}

inline static uint16_t toCode(char a, char b)
{
	return (uint16_t)a | ((uint16_t)b) << 8;
}

ShareManager::AdcSearch::AdcSearch(const StringList& params) : m_includePtr(&m_includeX), m_gt(0),
	m_lt(std::numeric_limits<int64_t>::max()), m_hasRoot(false), m_isDirectory(false)
{
	for (auto i = params.cbegin(); i != params.cend(); ++i)
	{
		const string& p = *i;
		if (p.length() <= 2)
			continue;
			
		const uint16_t cmd = toCode(p[0], p[1]);
		if (toCode('T', 'R') == cmd)
		{
			m_hasRoot = true;
			m_root = TTHValue(p.substr(2));
			return;
		}
		else if (toCode('A', 'N') == cmd)
		{
			m_includeX.push_back(StringSearch(p.substr(2)));
		}
		else if (toCode('N', 'O') == cmd)
		{
			m_exclude.push_back(StringSearch(p.substr(2)));
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
			m_gt = Util::toInt64(p.substr(2));
		}
		else if (toCode('L', 'E') == cmd)
		{
			m_lt = Util::toInt64(p.substr(2));
		}
		else if (toCode('E', 'Q') == cmd)
		{
			m_lt = m_gt = Util::toInt64(p.substr(2));
		}
		else if (toCode('T', 'Y') == cmd)
		{
			m_isDirectory = (p[2] == '2');
		}
	}
}

bool ShareManager::AdcSearch::isExcluded(const string& str)
{
	for (auto i = m_exclude.cbegin(); i != m_exclude.cend(); ++i)
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
    if (ClientManager::isBeforeShutdown())
    return;
    
    StringSearch::List* cur = aStrings.m_includePtr;
    StringSearch::List* old = aStrings.m_includePtr;
    
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

const bool sizeOk = (aStrings.m_gt == 0);
if (cur->empty() && aStrings.m_exts.empty() && sizeOk)
{
// We satisfied all the search words! Add the directory...
const SearchResult sr(SearchResult::TYPE_DIRECTORY, getDirSizeFast(), getFullName(), TTHValue(), -1  /*token*/);
	aResults.push_back(sr);
	ShareManager::incHits();
}

if (!aStrings.m_isDirectory)
{
for (auto i = m_share_files.cbegin(); i != m_share_files.cend() && !ClientManager::isBeforeShutdown(); ++i)
	{
	
		if (!(i->getSize() >= aStrings.m_gt))
		{
			continue;
		}
		else if (!(i->getSize() <= aStrings.m_lt))
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
			const SearchResult sr(SearchResult::TYPE_FILE, i->getSize(), getFullName() + i->getName(), i->getTTH(), -1  /*token*/);
			aResults.push_back(sr);
			ShareManager::incHits();
			if (aResults.size() >= maxResults)
			{
				return;
			}
		}
	}
}

for (auto l = m_share_directories.cbegin(); l != m_share_directories.cend() && aResults.size() < maxResults && !ClientManager::isBeforeShutdown(); ++l)
{
l->second->search(aResults, aStrings, maxResults);
}
aStrings.m_includePtr = old;
}

void ShareManager::search_max_result(SearchResultList& results, const StringList& params, StringList::size_type maxResults, StringSearch::List& reguest) noexcept // [!] IRainman add StringSearch::List& reguest
{
	if (ClientManager::isBeforeShutdown())
		return;
		
	AdcSearch srch(params);
	reguest = srch.m_includeX; // [+] IRainman
	if (srch.m_hasRoot)
	{
		search_tth(srch.m_root, results, false);
	}
	
	{
		CFlyReadLock(*g_csBloom);
		for (auto i = srch.m_includeX.cbegin(); i != srch.m_includeX.cend(); ++i)
		{
			if (!g_bloom.match(i->getPattern()))
			{
				return;
			}
		}
	}
	{
		CFlyReadLock(*g_csDirList);
		for (auto j = g_list_directories.cbegin(); j != g_list_directories.cend() && results.size() < maxResults && !ClientManager::isBeforeShutdown(); ++j)
		{
			(*j)->search(results, srch, maxResults);
		}
	}
}

ShareManager::Directory::Ptr ShareManager::getDirectoryL(const string& fname)
{
	for (auto mi = g_shares.cbegin(); mi != g_shares.cend(); ++mi)
	{
		if (strnicmp(fname, mi->first, mi->first.length()) == 0)
		{
			Directory::Ptr d;
			for (auto i = g_list_directories.cbegin(); i != g_list_directories.cend(); ++i)
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
				const auto& dmi = d->m_share_directories.find(fname.substr(j, i - j));
				j = i + 1;
				if (dmi == d->m_share_directories.end())
				{
					return Directory::Ptr();
				}
				d = dmi->second;
			}
			return d;
		}
	}
	return Directory::Ptr();
}

void ShareManager::on(QueueManagerListener::FileMoved, const string& n) noexcept
{
#if 0 ////////////////////////////////////////////////
#ifdef FLYLINKDC_USE_ADD_FINISHED_INSTANTLY
	if (BOOLSETTING(ADD_FINISHED_INSTANTLY))
#endif
	{
		// Check if finished download is supposed to be shared
		/* [-] IRainman opt.
		CFlyReadLock(*g_csShare);
		for (auto i = g_shares.cbegin(); i != g_shares.cend(); ++i)
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
#endif ///////////////////////////////////////////
}
void ShareManager::on(HashManagerListener::TTHDone, const string& fname, const TTHValue& p_root,
                      int64_t aTimeStamp, const CFlyMediaInfo& p_out_media, int64_t p_size) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	{
		CFlyBusy l_busy(g_RebuildIndexes);
		CFlyWriteLock(*g_csShare);
		{
			CFlyReadLock(*g_csDirList);
			if (Directory::Ptr d = getDirectoryL(fname)) // TODO прокинуть p_path_id и искать по нему?
			{
				const string l_file_name = Util::getFileName(fname);
				const auto i = d->findFileIterL(l_file_name);
				if (i != d->m_share_files.end())
				{
					CFlyWriteLock(*g_csTTHIndex);
					if (p_root != i->getTTH())
					{
						g_tthIndex.erase(i->getTTH());
					}
					// Get rid of false constness...
					Directory::ShareFile* f = const_cast<Directory::ShareFile*>(&(*i));
					f->setTTH(p_root);
					g_tthIndex.insert(make_pair(f->getTTH(), i));
					// TODO g_lastSharedDate =
					g_isNeedsUpdateShareSize = true;
				}
				else
				{
					const int64_t l_size = File::getSize(fname);
					dcassert(p_size == l_size);
					auto it = d->m_share_files.insert(Directory::ShareFile(l_file_name, l_size, d, p_root, 0, uint32_t(aTimeStamp), getFType(l_file_name)));
					dcassert(it.second);
					auto f = const_cast<Directory::ShareFile*>(&(*it.first));
					f->initLowerName();
					if (it.second)
					{
						auto l_media_ptr = std::make_shared<CFlyMediaInfo>(p_out_media);
						l_media_ptr->calcEscape();
						f->initMediainfo(l_media_ptr);
					}
					{
						CFlyWriteLock(*g_csTTHIndex);
						{
							CFlyWriteLock(*g_csBloom);
							updateIndicesFileL(*d, it.first);
						}
					}
				}
				setDirty();
				m_is_forceXmlRefresh = true;
			}
		}
	}
	// —бросим кэш поиска
	internalClearCache(true);
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
			refresh_share(true, true);
		}
	}
	internalCalcShareSize(); // [+]IRainman opt.
	internalClearCache(false);
#ifdef _DEBUG
	ClientManager::flushRatio(5000);
#endif
}

void ShareManager::internalClearCache(bool p_is_force)
{
	{
		CFlyWriteLock(*g_csShareNotExists);
#if 0
		for (auto i = g_file_not_exists_set.begin(); i != g_file_not_exists_set.end(); ++i)
		{
			if (i->second > 1)
			{
				LogManager::message("[ShareManager] Fix dup-search: " + i->first + ", Count = " + Util::toString(i->second) + ")");
			}
		}
#endif
		if (p_is_force || g_file_not_exists_set.size() > 1000)
		{
			g_file_not_exists_set.clear();
		}
	}
	{
		CFlyWriteLock(*g_csShareCache);
		if (p_is_force || g_file_cache_map.size() > 1000)
		{
			g_file_cache_map.clear();
		}
	}
}

bool ShareManager::isShareFolder(const string& path, bool thoroughCheck /* = false */)
{
	dcassert(!path.empty());
	if (thoroughCheck)  // check if it's part of the share before checking if it's in the exclusions
	{
		bool result = false;
		// TODO - нет лока!
		for (auto i = g_shares.cbegin(); i != g_shares.cend(); ++i)
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
	for (auto j = g_notShared.cbegin(); j != g_notShared.cend(); ++j)
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
	for (auto i = g_shares.cbegin(); i != g_shares.cend(); ++i)
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
	for (auto j = g_notShared.cbegin(); j != g_notShared.cend(); ++j)
	{
		if (path.size() >= j->size())
		{
			if (Text::isEqualsSubstringIgnoreCase(path, *j))
				return 0;
		}
	}
	
	// remove all sub folder excludes
	int64_t bytesNotCounted = 0;
	for (auto j = g_notShared.cbegin(); j != g_notShared.cend(); ++j)
	{
		if (path.size() < j->size())
		{
			if (Text::isEqualsSubstringIgnoreCase(*j, path))
			{
				bytesNotCounted += Util::getDirSize(*j);
				j = g_notShared.erase(j);
				if (g_notShared.empty()) //[+]PPA
					break;
				if (j != g_notShared.begin()) // [+]PPA fix vector iterator not decrementable
					--j;
			}
		}
	}
	
	// add it to the list
	g_notShared.push_back(path);
	
	const int64_t bytesRemoved = Util::getDirSize(path);
	
	return bytesRemoved - bytesNotCounted;
}

int64_t ShareManager::removeExcludeFolder(const string &path, bool returnSize /* = true */)
{
	int64_t bytesAdded = 0;
	// remove all sub folder excludes
	for (auto j = g_notShared.cbegin(); j != g_notShared.cend(); ++j)
	{
		if (path.size() <= j->size())
		{
			if (Text::isEqualsSubstringIgnoreCase(*j, path))
			{
				if (returnSize) // this needs to be false if the files don't exist anymore
					bytesAdded += Util::getDirSize(*j);
					
				j = g_notShared.erase(j);
				if (g_notShared.empty()) //[+]PPA
					break;
				if (j != g_notShared.begin()) // [+]PPA fix vector iterator not decrementable
					--j;
			}
		}
	}
	
	return bytesAdded;
}

bool ShareManager::findByRealPathName(const string& realPathname, TTHValue* outTTHPtr, string* outfilenamePtr /*= NULL*/, int64_t* outSizePtr/* = NULL*/) // [+] SSA
{
	// [+] IRainman fix.
	CFlyReadLock(*g_csShare);
	{
		CFlyReadLock(*g_csDirList);
		const Directory::Ptr d = ShareManager::getDirectoryL(realPathname);
		if (!d)
			return false;
		// [~] IRainman fix.
		
		const auto iFile = d->findFileIterL(Util::getFileName(realPathname));
		if (iFile != d->m_share_files.end())
		{
			if (outTTHPtr)
				*outTTHPtr = iFile->getTTH();
			if (outfilenamePtr)
				*outfilenamePtr = iFile->getName();
			if (outSizePtr)
				*outSizePtr = iFile->getSize();
			return true;
		}
	}
	return false;
}

bool ShareManager::isInSkipList(const string& lowerName) const
{
	return Wildcard::patternMatchLowerCase(lowerName, m_skipList);
}

/**
 * @file
 * $Id: ShareManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
