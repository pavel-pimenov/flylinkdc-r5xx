/*
 * Copyright (C) 2003-2005 RevConnect, http://www.revconnect.com
 * Copyright (C) 2011      Big Muscle, http://strongdc.sf.net
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

#ifndef _SHAREDFILESTREAM_H
#define _SHAREDFILESTREAM_H

#include "File.h"

struct SharedFileHandle
{
		SharedFileHandle(const string& aPath, int aAccess, int aMode);
		~SharedFileHandle();
		void init(int64_t p_file_size);
		
		FastCriticalSection m_cs;
		File  m_file;
		string m_path;
		int m_ref_cnt;
		int m_mode;
		int m_access;
		int64_t m_last_file_size;
		HANDLE m_map_file;
		char* m_map_file_ptr;
		bool m_is_map_file_error;
	private:
		void CloseMapFile();
};

class SharedFileStream : public IOStream
{

	public:
		typedef std::unordered_map<std::string, std::shared_ptr<SharedFileHandle>, noCaseStringHash, noCaseStringEq> SharedFileHandleMap;
		
		SharedFileStream(const string& aFileName, int aAccess, int aMode, int64_t p_file_size);
		~SharedFileStream();
		
		size_t write(const void* buf, size_t len) override;
		size_t read(void* buf, size_t& len) override;
		
		//int64_t getFileSize();
		int64_t getFastFileSize();
		void setSize(int64_t newSize);
		
		size_t flushBuffers(bool aForce) override;
		
		static FastCriticalSection g_shares_file_cs;
		static std::set<char> g_error_map_file;
		static std::map<std::string, unsigned> g_delete_files;
#ifdef FLYLINKDC_USE_SHARED_FILE_STREAM_RW_POOL
		static SharedFileHandleMap g_readpool;
		static SharedFileHandleMap g_writepool;
#else
		static SharedFileHandleMap g_rwpool;
#endif
		static std::unordered_set<std::string> g_shared_stream_errors;
		static void cleanup();
		static void delete_file(const std::string& p_file);
		static void check_before_destoy();
		void setPos(int64_t aPos) override;
	private:
		std::shared_ptr<SharedFileHandle> m_sfh;
		int64_t m_pos;
};


#endif  // _SHAREDFILESTREAM_H