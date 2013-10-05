/*
 * Copyright (C) 2003-2005 RevConnect, http://www.revconnect.com
 * Copyright (C) 2011      Big Muscle, http://strongdc.sf.net
 * Copyright (C) 2012      Alexey Solomin, a.rainman@gmail.com
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

#include "SharedFileStream.h"

#ifdef _WIN32
# include "Winioctl.h"
#endif

FastCriticalSection SharedFileStream::file_handle_poolCS;
SharedFileStream::SharedFileHandleMap SharedFileStream::file_handle_pool;

SharedFileStream::SharedFileHandle::SharedFileHandle(const string& aFileName, int access, int mode) : File(aFileName, access, mode), ref_cnt(1)
{
#ifdef _WIN32
	if (!SETTING(ANTI_FRAG))
	{
		// avoid allocation of large ranges of zeroes for unused segments
		DWORD bytesReturned;
		DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &bytesReturned, NULL);
	}
#endif
}

SharedFileStream::SharedFileStream(const string& aFileName, int access, int mode) : pos(-1)
{
	FastLock l(file_handle_poolCS);
	const auto i = file_handle_pool.find(aFileName);
	if (i != file_handle_pool.end())
	{
		shared_handle_ptr = i->second;
		shared_handle_ptr->ref_cnt++;
	}
	else
	{
		shared_handle_ptr = new SharedFileHandle(aFileName, access, mode);
		file_handle_pool.insert(make_pair(aFileName, shared_handle_ptr));
	}
}

SharedFileStream::~SharedFileStream()
{
	FastLock l(file_handle_poolCS);
	shared_handle_ptr->ref_cnt--;
	if (shared_handle_ptr->ref_cnt == 0)
	{
		for (auto i = file_handle_pool.cbegin(); i != file_handle_pool.cend(); ++i)
		{
			if (i->second == shared_handle_ptr)
			{
				file_handle_pool.erase(i);
				delete shared_handle_ptr;
				return;
			}
		}
		
		dcassert(0);
	}
}
