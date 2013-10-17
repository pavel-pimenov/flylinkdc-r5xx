/*
 * Copyright (C) 2003-2005 RevConnect, http://www.revconnect.com
 * Copyright (C) 2011      Big Muscle, http://strongdc.sf.net
 * Copyright (C) 2012-2013 Alexey Solomin, a.rainman on gmail pt com
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

#ifndef _SHAREDFILESTREAM_H
#define _SHAREDFILESTREAM_H

#include "File.h"

class SharedFileStream : public IOStream
{
	private:
		struct SharedFileHandle : File
		{
			unsigned int ref_cnt;
			CriticalSection cs;
			SharedFileHandle(const string& aFileName, int access, int mode);
			~SharedFileHandle() noexcept { }
		};
		
		typedef boost::unordered_map<string, SharedFileHandle*> SharedFileHandleMap;
		
	public:
		SharedFileStream(const string& aFileName, int access, int mode);
		~SharedFileStream();
		
		size_t write(const void* buf, size_t len)
		{
			Lock l(shared_handle_ptr->cs);
			
			dcassert(pos != -1);
			
			shared_handle_ptr->setPos(pos);
			shared_handle_ptr->write(buf, len);
			
			pos += len;
			
			return len;
		}
		
		size_t read(void* buf, size_t& len)
		{
			Lock l(shared_handle_ptr->cs);
			
			dcassert(pos != -1);
			
			shared_handle_ptr->setPos(pos);
			len = shared_handle_ptr->read(buf, len);
			
			pos += len;
			
			return len;
		}
		
		int64_t getSize() const noexcept
		{
		    // [!] IRainman fix: Locking is not needed, because the function is called immediately after the constructor, and in fact is part of the constructor.
		    return shared_handle_ptr->getSize();
		}
		void setSize(int64_t newSize) throw(FileException)
		{
			// [!] IRainman fix: Locking is not needed, because the function is called immediately after the constructor, and in fact is part of the constructor.
			shared_handle_ptr->setSize(newSize);
		}
		
		size_t flush()
		{
			dcassert(pos != -1);
			Lock l(shared_handle_ptr->cs);
			return shared_handle_ptr->flush();
		}
		
		void setPos(int64_t _pos)
		{
			// [!] IRainman fix: Locking is not needed, because the function is called immediately after the constructor, and in fact is part of the constructor.
			dcassert(pos == -1);
			pos = _pos;
		}
		
		static FastCriticalSection file_handle_poolCS;
		static SharedFileHandleMap file_handle_pool;
		
	private:
		SharedFileHandle* shared_handle_ptr;
		int64_t pos;
};
#endif  // _SHAREDFILESTREAM_H