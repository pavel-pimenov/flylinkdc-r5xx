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

#ifndef DCPLUSPLUS_DCPP_SEMAPHORE_H
#define DCPLUSPLUS_DCPP_SEMAPHORE_H

#ifndef _WIN32
#include <sys/time.h>
#endif

#include "Util.h"

class Semaphore
#ifdef _DEBUG
	: virtual NonDerivable<Semaphore>, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
#ifdef _WIN32
		Semaphore() noexcept
		{
			h = CreateSemaphore(NULL, 0, MAXLONG, NULL);
		}
		
		void signal() noexcept
		{
			ReleaseSemaphore(h, 1, NULL);
		}
		
		bool wait() noexcept
		{
			return WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0;
		}
		bool wait(uint32_t millis) noexcept
		{
			return WaitForSingleObject(h, millis) == WAIT_OBJECT_0;
		}
		
		~Semaphore() noexcept
		{
			CloseHandle(h);
		}
	private:
		HANDLE h;
#else
		Semaphore() noexcept
		{
			sem_init(&semaphore, 0, 0);
		}
		
		~Semaphore()
		{
			sem_destroy(&semaphore);
		}
		
		void signal() noexcept
		{
			sem_post(&semaphore);
		}
		
		bool wait() noexcept
		{
			int retval = 0;
			do
			{
				retval = sem_wait(&semaphore);
			}
			while (retval != 0);
			return true;
		}
		
		bool wait(uint32_t millis) noexcept
		{
			timeval timev;
			timespec t;
			gettimeofday(&timev, NULL);
			millis += timev.tv_usec / 1000;
			t.tv_sec = timev.tv_sec + (millis / 1000);
			t.tv_nsec = (millis % 1000) * 1000 * 1000;
			int ret;
			do
			{
				ret =  sem_timedwait(&semaphore, &t);
			}
			while (ret != 0 && errno == EINTR);
			if (ret != 0)
			{
				return false;
			}
		}
		
	private:
		sem_t semaphore;
#endif
		
};

#endif // DCPLUSPLUS_DCPP_SEMAPHORE_H

/**
 * @file
 * $Id: Semaphore.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
