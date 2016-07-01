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

/*
 * Automatic Directory Listing Search
 * Henrik Engström, henrikengstrom at home se
 */

#include "stdinc.h"
#include "ADLSearch.h"
#include "QueueManager.h"
#include "StringTokenizer.h"

ADLSearch::ADLSearch() :
	searchString("<Enter string>"),
	isActive(true),
	isAutoQueue(false),
	sourceType(OnlyFile),
	minFileSize(-1),
	maxFileSize(-1),
	typeFileSize(SizeBytes),
	destDir("ADLSearch"),
	ddIndex(0),
	isForbidden(false),
	raw(0)
{
}

ADLSearch::SourceType ADLSearch::StringToSourceType(const string& s)
{
	if (stricmp(s.c_str(), "Filename") == 0)
	{
		return OnlyFile;
	}
	else if (stricmp(s.c_str(), "Directory") == 0)
	{
		return OnlyDirectory;
	}
	else if (stricmp(s.c_str(), "Full Path") == 0)
	{
		return FullPath;
	}
	else
	{
		return OnlyFile;
	}
}

const string& ADLSearch::SourceTypeToString(SourceType t)
{
	// [!] IRainman fix: is its static data;
	switch (t)
	{
		default:
		case OnlyFile:
		{
			static const string g_fileName = "Filename";
			return g_fileName;
		}
		case OnlyDirectory:
		{
			static const string g_directory = "Directory";
			return g_directory;
		}
		case FullPath:
		{
			static const string g_fullPath = "Full Path";
			return g_fullPath;
		}
	}
}

const tstring& ADLSearch::SourceTypeToDisplayString(SourceType t)
{
	switch (t)
	{
		default:
		case OnlyFile:
			return TSTRING(FILENAME);
		case OnlyDirectory:
			return TSTRING(DIRECTORY);
		case FullPath:
			return TSTRING(ADLS_FULL_PATH);
	}
}

ADLSearch::SizeType ADLSearch::StringToSizeType(const string& s)
{
	if (stricmp(s.c_str(), "B") == 0)
	{
		return SizeBytes;
	}
	else if (stricmp(s.c_str(), "kB") == 0)
	{
		return SizeKiloBytes;
	}
	else if (stricmp(s.c_str(), "MB") == 0)
	{
		return SizeMegaBytes;
	}
	else if (stricmp(s.c_str(), "GB") == 0)
	{
		return SizeGigaBytes;
	}
	else
	{
		return SizeBytes;
	}
}

const string& ADLSearch::SizeTypeToString(SizeType t)
{
	// [!] IRainman fix: is its static data;
	switch (t)
	{
		case SizeKiloBytes:
		{
			static const string g_kB("kB");
			return g_kB;
		}
		
		case SizeMegaBytes:
		{
			static const string g_MB("MB");
			return g_MB;
		}
		
		case SizeGigaBytes:
		{
			static const string g_GB("GB");
			return g_GB;
		}
		
		default:
		case SizeBytes:
		{
			static const string g_B("B");
			return g_B;
		}
		
	}
}

const tstring& ADLSearch::SizeTypeToDisplayString(ADLSearch::SizeType t)
{
	switch (t)
	{
		default:
		case SizeBytes:
			return TSTRING(B);
		case SizeKiloBytes:
			return TSTRING(KB);
		case SizeMegaBytes:
			return TSTRING(MB);
		case SizeGigaBytes:
			return TSTRING(GB);
	}
}

int64_t ADLSearch::GetSizeBase() const
{
	switch (typeFileSize)
	{
		default:
		case SizeBytes:
			return (int64_t)1;
		case SizeKiloBytes:
			return (int64_t)1024;
		case SizeMegaBytes:
			return (int64_t)1024 * (int64_t)1024;
		case SizeGigaBytes:
			return (int64_t)1024 * (int64_t)1024 * (int64_t)1024;
	}
}

void ADLSearch::prepare(StringMap& params)
{
	// Prepare quick search of substrings
	stringSearches.clear();
	
	// Replace parameters such as %[nick]
	const string s = Util::formatParams(searchString, params, false);
	
	// Split into substrings
	const StringTokenizer<string> st(s, ' ');
	for (auto i = st.getTokens().cbegin(), iend = st.getTokens().cend(); i != iend; ++i)
	{
		if (!i->empty())
		{
			// Add substring search
			stringSearches.push_back(StringSearch(*i));
		}
	}
}

inline void ADLSearch::unprepare()
{
	stringSearches.clear();
}

bool ADLSearch::matchesFile(const string& f, const string& fp, int64_t size) const
{
	// Check status
	if (!isActive)
	{
		return false;
	}
	
	// Check size for files
	if (size >= 0 && (sourceType == OnlyFile || sourceType == FullPath))
	{
		if (minFileSize >= 0 && size < minFileSize * GetSizeBase())
		{
			// Too small
			return false;
		}
		if (maxFileSize >= 0 && size > maxFileSize * GetSizeBase())
		{
			// Too large
			return false;
		}
	}
	
	// Do search
	switch (sourceType)
	{
		default:
		case OnlyDirectory:
			return false;
		case OnlyFile:
			return searchAll(f);
		case FullPath:
			return searchAll(fp);
	}
}

bool ADLSearch::matchesDirectory(const string& d) const
{
	// Check status
	if (!isActive)
	{
		return false;
	}
	if (sourceType != OnlyDirectory)
	{
		return false;
	}
	
	// Do search
	return searchAll(d);
}

bool ADLSearch::searchAll(const string& s) const
{
	try
	{
		std::regex reg(searchString, std::regex_constants::icase);
		return std::regex_search(s, reg);
	}
	catch (...) {}
	
	// Match all substrings
	for (auto i = stringSearches.cbegin(), iend = stringSearches.cend(); i != iend; ++i)
	{
		if (!i->match(s))
		{
			return false;
		}
	}
	return !stringSearches.empty();
}

ADLSearchManager::ADLSearchManager() : breakOnFirst(false), sentRaw(false)
{
	load();
}

ADLSearchManager::~ADLSearchManager()
{
	save();
}

void ADLSearchManager::load()
{
	// Clear current
	collection.clear();
	
	// Load file as a string
	try
	{
		SimpleXML xml;
		xml.fromXML(File(getConfigFile(), File::READ, File::OPEN).read());
		
		if (xml.findChild("ADLSearch"))
		{
			xml.stepIn();
			
			// Predicted several groups of searches to be differentiated
			// in multiple categories. Not implemented yet.
			if (xml.findChild("SearchGroup"))
			{
				xml.stepIn();
				
				// Loop until no more searches found
				while (xml.findChild("Search"))
				{
					xml.stepIn();
					
					// Found another search, load it
					ADLSearch search;
					
					if (xml.findChild("SearchString"))
					{
						search.searchString = xml.getChildData();
					}
					if (xml.findChild("SourceType"))
					{
						search.sourceType = search.StringToSourceType(xml.getChildData());
					}
					if (xml.findChild("DestDirectory"))
					{
						search.destDir = xml.getChildData();
					}
					if (xml.findChild("IsActive"))
					{
						search.isActive = (Util::toInt(xml.getChildData()) != 0);
					}
					if (xml.findChild("IsForbidden"))
					{
						search.isForbidden = (Util::toInt(xml.getChildData()) != 0);
					}
					if (xml.findChild("Raw"))
					{
						search.raw = Util::toInt(xml.getChildData());
					}
					if (xml.findChild("MaxSize"))
					{
						search.maxFileSize = Util::toInt64(xml.getChildData());
					}
					if (xml.findChild("MinSize"))
					{
						search.minFileSize = Util::toInt64(xml.getChildData());
					}
					if (xml.findChild("SizeType"))
					{
						search.typeFileSize = search.StringToSizeType(xml.getChildData());
					}
					if (xml.findChild("IsAutoQueue"))
					{
						search.isAutoQueue = (Util::toInt(xml.getChildData()) != 0);
					}
					
					// Add search to collection
					if (!search.searchString.empty())
					{
						collection.push_back(search);
					}
					
					// Go to next search
					xml.stepOut();
				}
			}
		}
	}
	catch (const SimpleXMLException&) {}
	catch (const FileException&) {}
}

void ADLSearchManager::save() const
{
	// Prepare xml string for saving
	try
	{
		SimpleXML xml;
		
		xml.addTag("ADLSearch");
		xml.stepIn();
		
		// Predicted several groups of searches to be differentiated
		// in multiple categories. Not implemented yet.
		xml.addTag("SearchGroup");
		xml.stepIn();
		
		// Save all searches
		for (auto i = collection.cbegin(); i != collection.cend(); ++i)
		{
			const ADLSearch& search = *i;
			if (search.searchString.empty())
			{
				continue;
			}
			xml.addTag("Search");
			xml.stepIn();
			
			xml.addTag("SearchString", search.searchString);
			xml.addTag("SourceType", search.SourceTypeToString(search.sourceType));
			xml.addTag("DestDirectory", search.destDir);
			xml.addTag("IsActive", search.isActive);
			xml.addTag("IsForbidden", search.isForbidden);
			xml.addTag("Raw", search.raw);
			xml.addTag("MaxSize", search.maxFileSize);
			xml.addTag("MinSize", search.minFileSize);
			xml.addTag("SizeType", search.SizeTypeToString(search.typeFileSize));
			xml.addTag("IsAutoQueue", search.isAutoQueue);
			xml.stepOut();
		}
		
		xml.stepOut();
		
		xml.stepOut();
		
		// Save string to file
		try
		{
			File fout(getConfigFile(), File::WRITE, File::CREATE | File::TRUNCATE);
			fout.write(SimpleXML::utf8Header);
			fout.write(xml.toXML());
			fout.close();
		}
		catch (const FileException&) {   }
	}
	catch (const SimpleXMLException&) { }
}

void ADLSearchManager::matchesFile(DestDirList& destDirVector, DirectoryListing::File *currentFile, const string& fullPath)
{
	// Add to any substructure being stored
	for (auto id = destDirVector.begin(); id != destDirVector.end(); ++id)
	{
		if (id->subdir != NULL)
		{
			auto copyFile = new DirectoryListing::File(*currentFile, true);
			copyFile->setFlags(currentFile->getFlags()); // [+] NightOrion to issues http://code.google.com/p/flylinkdc/issues/detail?id=31
			dcassert(id->subdir->getAdls());
			
			id->subdir->files.push_back(copyFile);
		}
		id->fileAdded = false;  // Prepare for next stage
	}
	
	// Prepare to match searches
	if (currentFile->getName().size() < 1)
	{
		return;
	}
	
	string filePath = fullPath + "\\" + currentFile->getName();// TODO Crash
	// Match searches
	for (auto is = collection.cbegin(); is != collection.cend(); ++is)
	{
		if (destDirVector[is->ddIndex].fileAdded)
		{
			continue;
		}
		if (is->matchesFile(currentFile->getName(), filePath, currentFile->getSize()))
		{
			auto copyFile = new DirectoryListing::File(*currentFile, true);
			copyFile->setFlags(currentFile->getFlags()); // [+] NightOrion to issues http://code.google.com/p/flylinkdc/issues/detail?id=31
#ifdef IRAINMAN_INCLUDE_USER_CHECK
			if (is->isForbidden && !getSentRaw())
			{
				AutoArray<char> buf(FULL_MAX_PATH);
				_snprintf(buf, FULL_MAX_PATH, CSTRING(CHECK_FORBIDDEN), currentFile->getName().c_str());
				
				ClientManager::setClientStatus(user, buf.data(), is->raw, false);
				
				setSentRaw(true);
			}
#endif
			
			destDirVector[is->ddIndex].dir->files.push_back(copyFile);
			destDirVector[is->ddIndex].fileAdded = true;
			
			if (is->isAutoQueue)
			{
				try
				{
					QueueManager::getInstance()->add(0,/* [-] IRainman needs for support download to specify extension dir. SETTING(DOWNLOAD_DIRECTORY) + */currentFile->getName(),
					                                 currentFile->getSize(), currentFile->getTTH(), getUser()/*, Util::emptyString*/);
				}
				catch (const Exception&)
				{
					// ...
				}
			}
			
			if (breakOnFirst)
			{
				// Found a match, search no more
				break;
			}
		}
	}
}

void ADLSearchManager::matchesDirectory(DestDirList& destDirVector, DirectoryListing::Directory* currentDir, const string& fullPath) const
{
	// Add to any substructure being stored
	for (auto id = destDirVector.begin(); id != destDirVector.end(); ++id)
	{
		if (id->subdir != NULL)
		{
			DirectoryListing::Directory* newDir =
			    new DirectoryListing::AdlDirectory(fullPath, id->subdir, currentDir->getName());
			id->subdir->directories.push_back(newDir);
			id->subdir = newDir;
		}
	}
	
	// Prepare to match searches
	if (currentDir->getName().size() < 1)
	{
		return;
	}
	
	// Match searches
	for (auto is = collection.cbegin(); is != collection.cend(); ++is)
	{
		if (destDirVector[is->ddIndex].subdir != NULL)
		{
			continue;
		}
		if (is->matchesDirectory(currentDir->getName()))
		{
			destDirVector[is->ddIndex].subdir =
			    new DirectoryListing::AdlDirectory(fullPath, destDirVector[is->ddIndex].dir, currentDir->getName());
			destDirVector[is->ddIndex].dir->directories.push_back(destDirVector[is->ddIndex].subdir);
			if (breakOnFirst)
			{
				// Found a match, search no more
				break;
			}
		}
	}
}

void ADLSearchManager::stepUpDirectory(DestDirList& destDirVector) const
{
	for (auto id = destDirVector.begin(); id != destDirVector.end(); ++id)
	{
		if (id->subdir != nullptr)
		{
			id->subdir = id->subdir->getParent();
			if (id->subdir == id->dir)
			{
				id->subdir = nullptr;
			}
		}
	}
}

void ADLSearchManager::prepareDestinationDirectories(DestDirList& destDirVector, DirectoryListing::Directory* root, StringMap& params)
{
	// Load default destination directory (index = 0)
	destDirVector.clear();
	auto id = destDirVector.insert(destDirVector.end(), DestDir());
	id->name = "ADLSearch";
	id->dir  = new DirectoryListing::Directory(nullptr, root, "<<<" + id->name + ">>>", true, true, true);
	
	// Scan all loaded searches
	for (auto is = collection.begin(); is != collection.end(); ++is)
	{
		// Check empty destination directory
		if (is->destDir.empty())
		{
			// Set to default
			is->ddIndex = 0;
			continue;
		}
		
		// Check if exists
		bool isNew = true;
		long ddIndex = 0;
		for (id = destDirVector.begin(); id != destDirVector.end(); ++id, ++ddIndex)
		{
			if (stricmp(is->destDir.c_str(), id->name.c_str()) == 0)
			{
				// Already exists, reuse index
				is->ddIndex = ddIndex;
				isNew = false;
				break;
			}
		}
		
		if (isNew)
		{
			// Add new destination directory
			id = destDirVector.insert(destDirVector.end(), DestDir());
			id->name = is->destDir;
			id->dir  = new DirectoryListing::Directory(nullptr, root, "<<<" + id->name + ">>>", true, true, true);
			is->ddIndex = ddIndex;
		}
	}
	// Prepare all searches
	for (auto ip = collection.begin(); ip != collection.end(); ++ip)
	{
		ip->prepare(params);
	}
}

void ADLSearchManager::finalizeDestinationDirectories(DestDirList& destDirVector, DirectoryListing::Directory* root)
{
	string szDiscard("<<<" + STRING(ADLS_DISCARD) + ">>>");
	
	// Add non-empty destination directories to the top level
	for (auto id = destDirVector.begin(); id != destDirVector.end(); ++id)
	{
		if (id->dir->files.empty() && id->dir->directories.empty())
		{
			safe_delete(id->dir);
		}
		else if (stricmp(id->dir->getName(), szDiscard) == 0)
		{
			safe_delete(id->dir);
		}
		else
		{
			root->directories.push_back(id->dir);
		}
	}
	
	for (auto ip = collection.begin(); ip != collection.end(); ++ip)
	{
		ip->unprepare();
	}
}

void ADLSearchManager::matchListing(DirectoryListing& aDirList) noexcept
{
	StringMap params;
	if (aDirList.getUser())
	{
		params["userNI"] = ClientManager::getNicks(aDirList.getUser()->getCID(), Util::emptyString)[0];
		params["userCID"] = aDirList.getUser()->getCID().toBase32();
	}
	setUser(aDirList.getUser());
	setSentRaw(false);
	
	DestDirList destDirs;
	prepareDestinationDirectories(destDirs, aDirList.getRoot(), params);
	setBreakOnFirst(BOOLSETTING(ADLS_BREAK_ON_FIRST));
	
	string path(aDirList.getRoot()->getName());
	matchRecurse(destDirs, aDirList.getRoot(), path);
	
	finalizeDestinationDirectories(destDirs, aDirList.getRoot());
}

void ADLSearchManager::matchRecurse(DestDirList &aDestList, DirectoryListing::Directory* aDir, const string &aPath)
{
	for (auto dirIt = aDir->directories.cbegin(); dirIt != aDir->directories.cend(); ++dirIt)
	{
		string tmpPath = aPath + "\\" + (*dirIt)->getName();
		matchesDirectory(aDestList, *dirIt, tmpPath);
		matchRecurse(aDestList, *dirIt, tmpPath);
	}
	for (auto fileIt = aDir->files.cbegin(); fileIt != aDir->files.cend(); ++fileIt)
	{
		matchesFile(aDestList, *fileIt, aPath);
	}
	stepUpDirectory(aDestList);
}

string ADLSearchManager::getConfigFile()
{
	return Util::getConfigPath() + "ADLSearch.xml";
}

/**
 * @file
 * $Id: ADLSearch.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
