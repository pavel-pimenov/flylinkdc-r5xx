/*
 * Copyright (C) 2011-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#ifndef _FU_SEARCH_H_
#define _FU_SEARCH_H_
#pragma once

#include <vector>

using std::wstring;
typedef std::vector<wstring> FilesArray;

class FileUpdateSearch
{
	public:
		explicit FileUpdateSearch(const wstring& folderForUpdate) : _folderForUpdate(folderForUpdate) {}
		~FileUpdateSearch()
		{
		}
		
		bool Scan();
		size_t GetFilesCount() const
		{
			return _fileArray.size();
		}
		const wstring GetAbsoluteFilename(size_t i) const
		{
			return _fileArray[i];
		}
		DWORD CopyFileWithFolders(size_t i, const wstring& destination);
		void DeleteAll();
		
		size_t GetFoldersCount() const
		{
			return _folderArray.size();
		}
		const wstring GetAbsoluteFoldername(size_t i) const
		{
			return _folderArray[i];
		}
		// [!] Needs for FlyUpdate utility!
		const wstring GetRelativeFoldername(size_t i);
		const wstring GetRelativeFilename(size_t i);
		// [~] Needs for FlyUpdate utility!
		
	protected:
		bool ProcessFolder(const wstring& folder);
		
	private:
		wstring _folderForUpdate;
		FilesArray _fileArray;
		FilesArray _folderArray;
		
};

#endif //_FU_SEARCH_H_