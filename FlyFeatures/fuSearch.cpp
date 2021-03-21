/*
 * Copyright (C) 2011-2021 FlylinkDC++ Team http://flylinkdc.com
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
#include "fuSearch.h"

bool FileUpdateSearch::Scan()
{
	// Check for folder exists
	
	_fileArray.clear();
	_folderArray.clear();
	
	wstring searchPattern = _folderForUpdate;
	searchPattern += L"\\";
	
	ProcessFolder(searchPattern);
	
	return false;
}

// !!! Folder should always ends with "\" !!!
bool FileUpdateSearch::ProcessFolder(const wstring& folder)
{
	auto ffd = std::make_unique<WIN32_FIND_DATA >();;
	wstring searchPattern = folder;
	searchPattern += L'*';	
	HANDLE hFind = FindFirstFileEx(searchPattern.c_str(),
	                               FindExInfoStandard, // CompatibilityManager::g_find_file_level,
	                               &(*ffd),
	                               FindExSearchNameMatch,
	                               NULL,
	                               0);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	do
	{
		if (ffd->cFileName[0] != 0 && ffd->cFileName[0] != L'.')
			// [!] PVS V805 Decreased performance. It is inefficient to identify an empty string by using 'wcslen(str) > 0' construct.
			// A more efficient way is to check: str[0] != '\0'.    FlyFeatures fusearch.cpp    57  False
		{
			if (ffd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				wstring newSearchPattern = folder;
				newSearchPattern += ffd->cFileName;
				newSearchPattern += L"\\";
				_folderArray.push_back(newSearchPattern);
				ProcessFolder(newSearchPattern);				
			}
			else
			{
				// Add to vector
				wstring toPush = folder;
				toPush += ffd->cFileName;
				_fileArray.push_back(toPush);
			}
		}
	}
	while (FindNextFile(hFind, &(*ffd)));
	
	FindClose(hFind);
	
	return true;
}

DWORD
FileUpdateSearch::CopyFileWithFolders(size_t i, const wstring& destination)
{
	const wstring sourceName = _fileArray[i];
	
	// TODO!
	//File::ensureDirectory(destination);
	
	// Parse destination on folders and create'em
	// C:\Program Files\FlylinkDC++\myfile.exe
	wstring::size_type pos = destination.find(L'\\');
	while (pos != wstring::npos)
	{
		wstring folder = destination.substr(0, pos);
		CreateDirectory(folder.c_str(), NULL);
		pos = destination.find(L'\\', pos + 1);
	}
	// ~TODO
	return CopyFile(sourceName.c_str(), destination.c_str(), FALSE);
}

void FileUpdateSearch::DeleteAll()
{
	const size_t fileUpdateCount = GetFilesCount();
	for (int i = fileUpdateCount - 1 ; i >= 0 ; i--)
	{
		::DeleteFile(GetAbsoluteFilename(i).c_str());
	}
	const size_t folderCounts = GetFoldersCount();
	for (int i = folderCounts - 1; i >= 0 ; i--)
	{
		::RemoveDirectory(GetAbsoluteFoldername(i).c_str());
	}
}

// [!] Needs for FlyUpdate utility!
const wstring FileUpdateSearch::GetRelativeFilename(size_t i)
{
	wstring retValue;
	
	wstring relativeName = _fileArray[i];
	// Remove _folderForUpdate with "\\";
	wstring folderUpdate = _folderForUpdate;
	folderUpdate += L"\\";
	auto pos = relativeName.find_first_of(folderUpdate);
	if (pos == 0)
	{
		retValue = relativeName.substr(folderUpdate.length());
	}
	
	return retValue;
}

const wstring FileUpdateSearch::GetRelativeFoldername(size_t i)
{
	wstring retValue;
	
	wstring relativeName = _folderArray[i];
	// Remove _folderForUpdate with "\\";
	wstring folderUpdate = _folderForUpdate;
	folderUpdate += L"\\";
	auto pos = relativeName.find_first_of(folderUpdate);
	if (pos == 0)
	{
		retValue = relativeName.substr(folderUpdate.length());
	}
	
	return retValue;
}
