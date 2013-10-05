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
#include <process.h>
#include <tlhelp32.h>

#include "Thread.h"
#ifndef USE_FLY_CONSOLE_TEST
#include "ResourceManager.h"
#endif
#include "Util.h"


#if USE_WIN_THREAD_NAME

const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)

static void SetThreadName(DWORD dwThreadID, char* threadName)
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = threadName;
	info.dwThreadID = dwThreadID;
	info.dwFlags = 0;
	
	__try
	{
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
}

#endif // USE_WIN_THREAD_NAME

void Thread::start()
{
	join();
	HANDLE h = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, &starter, this, 0, nullptr));
	if (h == nullptr || h == INVALID_HANDLE_VALUE)
	{
		// [!] IRainman fix: try twice before generating an exception.
		h = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, &starter, this, 0, nullptr)); // TODO убрать двойной вызов.
		if (h == nullptr || h == INVALID_HANDLE_VALUE)
		{
#ifdef USE_FLY_CONSOLE_TEST
			throw ThreadException("UNABLE_TO_CREATE_THREAD");
#else
			throw ThreadException(STRING(UNABLE_TO_CREATE_THREAD) + " errno = " + Util::toString(errno));
#endif
		}
	}
	else
	{
		m_threadHandle = h;
#ifdef USE_WIN_THREAD_NAME
		// TODO - SetThreadName http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
#endif
	}
}

int Thread::getThreadsCount()
{
	int l_count = 0;
	const DWORD l_currentProcessId = GetCurrentProcessId();
	const HANDLE l_h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (l_h != INVALID_HANDLE_VALUE)
	{
		THREADENTRY32 l_te;
		l_te.dwSize = sizeof(l_te);
		if (Thread32First(l_h, &l_te))
		{
			do
			{
				if (l_te.th32OwnerProcessID == l_currentProcessId)
				{
					++l_count;
				}
			}
			while (Thread32Next(l_h, &l_te));
		}
		CloseHandle(l_h);
	}
	return l_count;
}

#ifdef RIP_USE_THREAD_POOL

ThreadPool::ThreadPool():
	m_hDoneEvent(INVALID_HANDLE_VALUE)
{
}

ThreadPool::~ThreadPool()
{
	if (m_hDoneEvent != INVALID_HANDLE_VALUE)
		CloseHandle(m_hDoneEvent);
}

void ThreadPool::start(bool executeLongTime /*= false*/) throw(ThreadException)
{
	join();
	
	// http://msdn.microsoft.com/en-us/library/windows/desktop/ms684957(v=vs.85).aspx
	
	if (!QueueUserWorkItem(starter, this, executeLongTime ? WT_EXECUTELONGFUNCTION : 0))
	{
		throw ThreadException(STRING(UNABLE_TO_CREATE_THREAD));
	}
}

#endif // RIP_USE_THREAD_POOL

/**
 * @file
 * $Id: Thread.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
