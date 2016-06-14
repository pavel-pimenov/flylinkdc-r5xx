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

#include "CFlyThread.h"
#ifndef USE_FLY_CONSOLE_TEST
#include "ResourceManager.h"
#endif
#include "Util.h"

//#define FLYLINKDC_USE_WIN_THREAD_NAME
#ifdef FLYLINKDC_USE_WIN_THREAD_NAME

// (c) chromium\home\chrome-svn\tarball\chromium\src\third_party\webrtc\system_wrappers\source
// set_thread_name_win.h
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

static void SetThreadName(DWORD dwThreadID, const char* threadName)
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

#endif // FLYLINKDC_USE_WIN_THREAD_NAME
void Thread::join(const DWORD dwMilliseconds /*= INFINITE*/)
{
	if (m_threadHandle != INVALID_HANDLE_VALUE)
	{
		WaitForSingleObject(m_threadHandle, dwMilliseconds);
		close_handle();
	}
}
bool Thread::is_active(int p_wait /* = 0 */) const
{
	if (m_threadHandle != INVALID_HANDLE_VALUE &&
	        WaitForSingleObject(m_threadHandle, p_wait) == WAIT_TIMEOUT)
	{
		return true; // Поток еще работает. пропустим...
	}
	else
	{
		return false;
	}
}

unsigned int WINAPI Thread::starter(void* p)
{
	if (Thread* t = reinterpret_cast<Thread*>(p))
		t->run();
	return 0;
}

void Thread::close_handle()
{
	if (m_threadHandle != INVALID_HANDLE_VALUE)
	{
		HANDLE l_thread = m_threadHandle;
		m_threadHandle = INVALID_HANDLE_VALUE;
		CloseHandle(l_thread);
	}
}
void Thread::setThreadPriority(Priority p)
{
	if (m_threadHandle != INVALID_HANDLE_VALUE)
	{
		const BOOL l_res = ::SetThreadPriority(m_threadHandle, p);
		dcassert(l_res);
		if (!l_res)
		{
#if defined (_CONSOLE)
			dcdebug("Error setThreadPriority = %s", GetLastError());
#else
			dcdebug("Error setThreadPriority = %s", Util::translateError().c_str());
#endif
		}
	}
	else
	{
		dcassert(m_threadHandle != INVALID_HANDLE_VALUE);
	}
}

void Thread::start(unsigned int p_stack_size, const char* p_name /* = nullptr */)
{
	join();
	p_stack_size *= 1024;
	HANDLE h = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, p_stack_size, &starter, this, 0, nullptr));
	if (h == nullptr || h == INVALID_HANDLE_VALUE)
	{
		// [!] IRainman fix: try twice before generating an exception.
		h = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, p_stack_size ? p_stack_size / 2 : 64 * 1024, &starter, this, 0, nullptr)); // TODO убрать двойной вызов.
		if (h == nullptr || h == INVALID_HANDLE_VALUE)
		{
			const auto l_last_error = GetLastError();
#ifdef USE_FLY_CONSOLE_TEST
			throw ThreadException("UNABLE_TO_CREATE_THREAD");
#else                   // TODO - отметить маркером для передачи на флай сервер факта падения. ошибка странная и плохая.
			const string l_error = "Unable to crate thread! errno = " + Util::toString(errno) +
			                       " GetLastError() = " + Util::toString(l_last_error) +
			                       " Please send a text or a screenshot of the error to developers ppa74@ya.ru";
			// https://www.crash-server.com/DumpGroup.aspx?ClientID=ppa&Login=Guest&DumpGroupID=97752
#ifdef _DEBUG
			MessageBox(NULL, Text::toT(l_error).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR | MB_TOPMOST);
#endif
			throw ThreadException(l_error);
#endif
		}
	}
	else
	{
		m_threadHandle = h;
#ifdef FLYLINKDC_USE_WIN_THREAD_NAME
		if (p_name)
		{
			//SetThreadName(-1, p_name);
		}
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
