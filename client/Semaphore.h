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

#pragma once


#ifndef DCPLUSPLUS_DCPP_SEMAPHORE_H
#define DCPLUSPLUS_DCPP_SEMAPHORE_H

#include "Util.h"

class Semaphore
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		Semaphore() noexcept
		{
			h = CreateSemaphore(NULL, 0, MAXLONG, NULL);
			dcassert(h != INVALID_HANDLE_VALUE);
			if (h == INVALID_HANDLE_VALUE)
			{
				const auto l_error_code = GetLastError();
				if (l_error_code)
				{
					dcassert(l_error_code == 0);
					dcdebug("[Semaphore] CreateSemaphore = error_code = %d", l_error_code);
				}
			}
		}
		
		void signal() noexcept
		{
			if (!ReleaseSemaphore(h, 1, NULL))
			{
				const auto l_error_code = GetLastError();
				if (l_error_code)
				{
					dcassert(l_error_code == 0);
					dcdebug("[Semaphore] ReleaseSemaphore = error_code = %d", l_error_code);
				}
			}
		}
		
		bool wait() noexcept
		{
			dcassert(h != INVALID_HANDLE_VALUE);
			return WaitForSingleObject(h, INFINITE) == WAIT_OBJECT_0;
		}
		bool wait(uint32_t millis) noexcept
		{
			dcassert(h != INVALID_HANDLE_VALUE);
			return WaitForSingleObject(h, millis) == WAIT_OBJECT_0;
		}
		
		~Semaphore() noexcept
		{
			if (!CloseHandle(h))
			{
				h = INVALID_HANDLE_VALUE;
				const auto l_error_code = GetLastError();
				if (l_error_code)
				{
					dcassert(l_error_code == 0);
					dcdebug("[Semaphore] CloseHandle = error_code = %d", l_error_code);
				}
			}
		}
	private:
		HANDLE h;
};

#endif // DCPLUSPLUS_DCPP_SEMAPHORE_H

/**
 * @file
 * $Id: Semaphore.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
