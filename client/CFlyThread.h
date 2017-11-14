/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#ifndef DCPLUSPLUS_DCPP_THREAD_H
#define DCPLUSPLUS_DCPP_THREAD_H

#pragma once

#pragma warning(disable:4456)

#ifdef FLYLINKDC_USE_BOOST_LOCK
#include <boost/utility.hpp>
#include <boost/thread.hpp>
#endif
#ifdef _DEBUG
#include <boost/noncopyable.hpp>
#include <set>
#endif

#include "Exception.h"

#ifdef FLYLINKDC_BETA
#include <fstream>
#include <ctime>
#endif

#include "CFlyLockProfiler.h"

#define CRITICAL_SECTION_SPIN_COUNT 2000 // [+] IRainman opt. http://msdn.microsoft.com/en-us/library/windows/desktop/ms683476(v=vs.85).aspx You can improve performance significantly by choosing a small spin count for a critical section of short duration. For example, the heap manager uses a spin count of roughly 4,000 for its per-heap critical sections.

STANDARD_EXCEPTION(ThreadException);

// [+] IRainman fix: moved from Thread.
class BaseThread
{
	public:
		enum Priority
		{
			IDLE = THREAD_PRIORITY_IDLE,
			LOW = THREAD_PRIORITY_BELOW_NORMAL,
			NORMAL = THREAD_PRIORITY_NORMAL
			//HIGH = THREAD_PRIORITY_ABOVE_NORMAL
		};
		static long safeInc(volatile long& v)
		{
			return InterlockedIncrement(&v);
		}
		static long safeDec(volatile long& v)
		{
			return InterlockedDecrement(&v);
		}
		static long safeExchange(volatile long& target, long value)
		{
			return InterlockedExchange(&target, value);
		}
};

// [+] IRainman fix: detect long waits.
#ifdef _DEBUG
# define TRACING_LONG_WAITS
# ifdef TRACING_LONG_WAITS
#  define TRACING_LONG_WAITS_TIME_MS (1 * 60 * 1000)
#  define DEBUG_WAITS_INIT(maxSpinCount) int _debugWaits = 0 - maxSpinCount;
#  define DEBUG_WAITS(waitTime) {\
		if (++_debugWaits == waitTime)\
		{\
			dcdebug("Thread %d waits a lockout condition for more than " #waitTime " ms.\n", ::GetCurrentThreadId());\
			/*dcassert(0);*/\
		} }
# else
#  define TRACING_LONG_WAITS_TIME_MS
#  define DEBUG_WAITS_INIT(maxSpinCount)
#  define DEBUG_WAITS(waitTime)
# endif // TRACING_LONG_WAITS
#endif
// [~] IRainman fix.

class Thread : public BaseThread
#ifdef _DEBUG
	, private boost::noncopyable
#endif
{
	public:
		// [+] IRainman fix.
		static void sleepWithSpin(unsigned int& spin)
		{
			if (spin)
			{
				--spin;
				yield();
			}
			else
			{
				sleep(1);
			}
		}
		
		static bool failStateLock(volatile long& state)
		{
			return safeExchange(state, 1) == 1;
		}
		static void waitLockStateWithSpin(volatile long& state)
		{
			dcdrun(DEBUG_WAITS_INIT(CRITICAL_SECTION_SPIN_COUNT));
			unsigned int spin = CRITICAL_SECTION_SPIN_COUNT;
			while (failStateLock(state))
			{
				dcdrun(DEBUG_WAITS(TRACING_LONG_WAITS_TIME_MS));
				sleepWithSpin(spin);
			}
		}
		static void waitLockState(volatile long& p_state
#ifdef _DEBUG
		                          , bool p_is_log = false
#endif
		                         )
		{
			while (failStateLock(p_state))
			{
				yield();
#ifdef _DEBUG
				if (p_is_log)
				{
					dcdebug("waitLockState");  // TODO в дебаге сделать лог для вывода инфы о конкуренции
				}
#endif
			}
		}
		static void unlockState(volatile long& state)
		{
			dcassert(state == 1);
			safeExchange(state, 0);
		}
		
		static void waitConditionWithSpin(const volatile long& condition)
		{
			dcdrun(DEBUG_WAITS_INIT(CRITICAL_SECTION_SPIN_COUNT));
			unsigned int spin = CRITICAL_SECTION_SPIN_COUNT;
			while (condition)
			{
				dcdrun(DEBUG_WAITS(TRACING_LONG_WAITS_TIME_MS));
				sleepWithSpin(spin);
			}
		}
		static void waitCondition(const volatile long& condition)
		{
			while (condition)
			{
				yield();
			}
		}
		
#ifdef _DEBUG
		class ConditionLocker
#ifdef _DEBUG
			: private boost::noncopyable
#endif
		{
			public:
				explicit ConditionLocker(volatile long& condition) : m_condition(condition)
				{
					waitLockState(m_condition);
				}
				
				~ConditionLocker()
				{
					unlockState(m_condition);
				}
			private:
				volatile long& m_condition;
		};
#endif // _DEBUG        
		// [~] IRainman fix.
		explicit Thread() : m_threadHandle(INVALID_HANDLE_VALUE) { }
		virtual ~Thread()
		{
			close_handle();
		}
		
		void start(unsigned int p_stack_size, const char* p_name = nullptr);
		void join(const DWORD dwMilliseconds = INFINITE);
		bool is_active(int p_wait = 0) const;
		static int getThreadsCount();
		void setThreadPriority(Priority p);
		static void sleep(DWORD p_millis)
		{
			::Sleep(p_millis);
		}
		static void yield()
		{
			::Sleep(0);
		}
		
	protected:
		virtual int run() = 0;
		
	private:
	
		HANDLE m_threadHandle;
		void close_handle();
		static unsigned int  WINAPI starter(void* p);
};

#if defined(IRAINMAN_USE_SPIN_LOCK) || defined(IRAINMAN_USE_SHARED_SPIN_LOCK)
// TODO: must be rewritten to support the diagnosis of a spinlock - ie diagnosis recursive entry.
// However, there is a problem that it is not clear how to solve:
// SharedSpinLock diagnose using conventional critical sections will be difficult and will have to sculpt Hells design of two critical sections :)
# define _NO_DEADLOCK_TRACE 1
#endif

#if !defined(_DEBUG) || defined(_NO_DEADLOCK_TRACE)
#ifdef FLYLINKDC_USE_BOOST_LOCK
typedef boost::recursive_mutex  CriticalSection;
typedef boost::lock_guard<boost::recursive_mutex> Lock;
typedef CriticalSection FastCriticalSection;
typedef Lock FastLock;
#else
class CriticalSection
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
#ifdef FLYLINKDC_BETA
		string formatDigitalClock(const string &p_msg, const time_t& p_t)
		{
			tm* l_loc = localtime(&p_t);
			if (!l_loc)
			{
				return "";
			}
			const size_t l_bufsize = p_msg.size() + 15;
			string l_buf;
			l_buf.resize(l_bufsize + 1);
			const size_t l_len = strftime(&l_buf[0], l_bufsize, p_msg.c_str(), l_loc);
			if (!l_len)
				return p_msg;
			else
			{
				l_buf.resize(l_len);
				return l_buf;
			}
		}
#endif

		void log(const char* p_add_info)
		{
#ifdef FLYLINKDC_BETA
			extern bool g_UseCSRecursionLog;
			if (g_UseCSRecursionLog)
			{
				if (cs.RecursionCount > 1)
				{
					g_UseCSRecursionLog = true;
				}
				std::ofstream l_fs;
				l_fs.open(_T("flylinkdc-critical-section.log"), std::ifstream::out | std::ifstream::app);
				if (l_fs.good())
				{
					l_fs << "[this = " << this << "][" << formatDigitalClock("time =", time(nullptr));
					if (cs.RecursionCount > 1)
						l_fs << "] cs.RecursionCount = " << cs.RecursionCount;
					if (!string(p_add_info).empty())
						l_fs << " p_add_info: " << p_add_info << std::endl;
				}
			}
#endif
		}
	public:
		void lock()
		{
			//dcassert(cs.RecursionCount == 0 || (cs.RecursionCount > 0 && tryLock() == true));
			EnterCriticalSection(&cs);
			log("lock");
		}
		LONG getLockCount() const
		{
			return cs.LockCount;
		}
		LONG getRecursionCount() const
		{
			return cs.RecursionCount;
		}
		void unlock()
		{
			LeaveCriticalSection(&cs);
			//dcassert(cs.RecursionCount == 0 || (cs.RecursionCount > 0 && tryLock() == true));
			log("unlock");
#ifdef FLYLINKDC_BETA
			extern bool g_UseCSRecursionLog;
			if (g_UseCSRecursionLog)
			{
			}
#endif
		}
		bool tryLock()
		{
			if (TryEnterCriticalSection(&cs) != FALSE)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
		explicit CriticalSection()
		{
			const auto l_result = InitializeCriticalSectionAndSpinCount(&cs, CRITICAL_SECTION_SPIN_COUNT); // [!] IRainman: InitializeCriticalSectionAndSpinCount
			if (l_result == 0)
			{
				dcassert(l_result);
			}
			log("construct");
		}
		~CriticalSection()
		{
			DeleteCriticalSection(&cs);
			log("destruct");
		}
	private:
		CRITICAL_SECTION cs;
};

// [+] IRainman fix: detect spin lock recursive entry
#ifdef _DEBUG
# define SPIN_LOCK_TRACE_RECURSIVE_ENTRY
# ifdef SPIN_LOCK_TRACE_RECURSIVE_ENTRY
#  define DEBUG_SPIN_LOCK_DECL() std::set<DWORD> _debugOwners; volatile long _debugOwnersState
#  define DEBUG_SPIN_LOCK_INIT() _debugOwnersState = 0;
#  define DEBUG_SPIN_LOCK_INSERT() {\
		Thread::ConditionLocker cl(_debugOwnersState);\
		const auto s = _debugOwners.insert(GetCurrentThreadId());\
		dcassert (s.second); /* spin lock prevents recursive entry! */\
	}
#  define DEBUG_SPIN_LOCK_ERASE() {\
		Thread::ConditionLocker cl(_debugOwnersState);\
		/*const auto n = */_debugOwners.erase(GetCurrentThreadId());\
		/*dcassert(n);*/ /* remove zombie owner */\
	}
# else
#  define DEBUG_SPIN_LOCK_DECL()
#  define DEBUG_SPIN_LOCK_INIT()
#  define DEBUG_SPIN_LOCK_INSERT()
#  define DEBUG_SPIN_LOCK_ERASE()
# endif // SPIN_LOCK_TRACE_RECURSIVE_ENTRY
#endif // _DEBUG
// [~] IRainman fix.

#ifdef IRAINMAN_USE_SPIN_LOCK

/**
 * A fast, non-recursive and unfair implementation of the Critical Section.
 * It is meant to be used in situations where the risk for lock conflict is very low,
 * i e locks that are held for a very short time. The lock is _not_ recursive, i e if
 * the same thread will try to grab the lock it'll hang in a never-ending loop. The lock
 * is not fair, i e the first to try to enter a locked lock is not guaranteed to be the
 * first to get it when it's freed...
 */

class FastCriticalSection
#ifdef _DEBUG
	: boost::noncopyable
#endif
{

	public:
		explicit FastCriticalSection() : m_state(0)
#ifdef _DEBUG
			, m_use_log(false)
#endif
		{
			dcdrun(DEBUG_SPIN_LOCK_INIT());
		}
#ifdef _DEBUG
		void use_log()
		{
			m_use_log = true;
		}
#endif
		void lock()
		{
			dcdrun(DEBUG_SPIN_LOCK_INSERT());
#ifdef _DEBUG
			Thread::waitLockState(m_state, m_use_log); // [!] IRainman fix.
#else
			Thread::waitLockState(m_state);
#endif
		}
		void unlock()
		{
			Thread::unlockState(m_state); // [!] IRainman fix.
			dcdrun(DEBUG_SPIN_LOCK_ERASE());
		}
		LONG getLockCount() const
		{
			return 0;
		}
		LONG getRecursionCount() const
		{
			return 0;
		}

	private:
		volatile long m_state;
#ifdef _DEBUG
		bool m_use_log;
#endif
		dcdrun(DEBUG_SPIN_LOCK_DECL());
#ifndef IRAINMAN_USE_SHARED_SPIN_LOCK
	public:
		void lockShared()
		{
			lock();
		}
		void unlockShared()
		{
			unlock();
		}
		void lockUnique()
		{
			lock();
		}
		void unlockUnique()
		{
			unlock();
		}
#endif // IRAINMAN_USE_SHARED_SPIN_LOCK
};
#else
typedef CriticalSection FastCriticalSection;
#endif // IRAINMAN_USE_SPIN_LOCK

template<class T>  class LockBase
#ifdef FLYLINKDC_USE_PROFILER_CS
	: public CFlyLockProfiler
#endif
{
	public:
		explicit LockBase(T& aCs
#ifdef FLYLINKDC_USE_PROFILER_CS
		                  , const char* p_function
		                  , int p_line
#endif
		                 ) : cs(aCs)
#ifdef FLYLINKDC_USE_PROFILER_CS
			, CFlyLockProfiler(p_function, p_line)
#endif
		{
			cs.lock();
#ifdef FLYLINKDC_USE_PROFILER_CS
			log("D:\\CriticalSectionLog-lock.txt", cs.getRecursionCount());
#endif
		}
		~LockBase()
		{
#ifdef FLYLINKDC_USE_PROFILER_CS
			log("D:\\CriticalSectionLog-unlock.txt", cs.getRecursionCount(), true);
#endif
			cs.unlock();
		}
	private:
		T& cs;
};
typedef LockBase<CriticalSection> Lock;
#ifdef FLYLINKDC_USE_PROFILER_CS
#define CFlyLock(cs) Lock l_lock(cs,__FUNCTION__, __LINE__);
#define CFlyLockLine(cs, line) Lock l_lock(cs,line,0);
#else
#define CFlyLock(cs) Lock l_lock(cs);
#endif
#ifdef IRAINMAN_USE_SPIN_LOCK
typedef LockBase<FastCriticalSection> FastLock;
#else
typedef Lock FastLock;
#endif // IRAINMAN_USE_SPIN_LOCK

#ifdef FLYLINKDC_USE_PROFILER_CS
#define CFlyFastLock(cs) FastLock l_lock(cs,__FUNCTION__,__LINE__);
#else
#define CFlyFastLock(cs) FastLock l_lock(cs);
#endif

#endif // FLYLINKDC_USE_BOOST_LOCK

#else
#define DEADLOCK_TIMEOUT 30000
#define CS_DEBUG 1

// Создаем на лету событие для операций ожидания,
// но никогда его не освобождаем. Так удобней для отладки
static inline HANDLE _CriticalSectionGetEvent(LPCRITICAL_SECTION pcs)
{
	HANDLE ret = pcs->LockSemaphore;
	if (!ret)
	{
		HANDLE sem = ::CreateEvent(NULL, false, false, NULL);
		dcassert(sem);
		ret = (HANDLE)::InterlockedCompareExchangePointer(&pcs->LockSemaphore, sem, NULL);
		if (!ret)
			ret = sem;
		else if (sem)
			::CloseHandle(sem); // Кто-то успел раньше
	}
	return ret;
}

// Ждем, пока критическая секция не освободится либо время ожидания
// будет превышено
static inline VOID _WaitForCriticalSectionDbg(LPCRITICAL_SECTION pcs)
{
	HANDLE sem = _CriticalSectionGetEvent(pcs);

	DWORD dwWait;
	do
	{
		dwWait = ::WaitForSingleObject(sem, DEADLOCK_TIMEOUT);
		if (WAIT_TIMEOUT == dwWait)
		{
			dcdebug("Critical section timeout (%u msec):"
			        " tid %u owner tid %u\n", DEADLOCK_TIMEOUT,
			        ::GetCurrentThreadId(), pcs->OwningThread);
		}
	}
	while (WAIT_TIMEOUT == dwWait);
	dcassert(WAIT_OBJECT_0 == dwWait);
}

// Выставляем событие в активное состояние
static inline VOID _UnWaitCriticalSectionDbg(LPCRITICAL_SECTION pcs)
{
	HANDLE sem = _CriticalSectionGetEvent(pcs);
	BOOL b = ::SetEvent(sem);
	dcassert(b);
}

// Заполучаем критическую секцию в свое пользование
inline VOID EnterCriticalSectionDbg(LPCRITICAL_SECTION pcs)
{
	if (::InterlockedIncrement(&pcs->LockCount))
	{
		// LockCount стал больше нуля.
		// Проверяем идентификатор нити
		if (pcs->OwningThread == (HANDLE)::GetCurrentThreadId())
		{
			// Нить та же самая. Критическая секция наша.
			pcs->RecursionCount++;
			return;
		}

		// Критическая секция занята другой нитью.
		// Придется подождать
		_WaitForCriticalSectionDbg(pcs);
	}

	// Либо критическая секция была "свободна",
	// либо мы дождались. Сохраняем идентификатор текущей нити.
	pcs->OwningThread = (HANDLE)::GetCurrentThreadId();
	pcs->RecursionCount = 1;
}

// Заполучаем критическую секцию, если она никем не занята
inline BOOL TryEnterCriticalSectionDbg(LPCRITICAL_SECTION pcs)
{
	if (-1L == ::InterlockedCompareExchange(&pcs->LockCount, 0, -1))
	{
		// Это первое обращение к критической секции
		pcs->OwningThread = (HANDLE)::GetCurrentThreadId();
		pcs->RecursionCount = 1;
	}
	else if (pcs->OwningThread == (HANDLE)::GetCurrentThreadId())
	{
		// Это не первое обращение, но из той же нити
		::InterlockedIncrement(&pcs->LockCount);
		pcs->RecursionCount++;
	}
	else
		return FALSE; // Критическая секция занята другой нитью

	return TRUE;
}

// Освобождаем критическую секцию
inline VOID LeaveCriticalSectionDbg(LPCRITICAL_SECTION pcs)
{
	// Проверяем, чтобы идентификатор текущей нити совпадал
	// с идентификатором нити-владельца.
	// Если это не так, скорее всего мы имеем дело с ошибкой
	dcassert(pcs->OwningThread == (HANDLE)::GetCurrentThreadId());

	if (--pcs->RecursionCount)
	{
		// Не последний вызов из этой нити.
		// Уменьшаем значение поля LockCount
		::InterlockedDecrement(&pcs->LockCount);
	}
	else
	{
		// Последний вызов. Нужно "разбудить" какую-либо
		// из ожидающих ниток, если таковые имеются
		dcassert(NULL != pcs->OwningThread);

		pcs->OwningThread = NULL;
		if (::InterlockedDecrement(&pcs->LockCount) >= 0)
		{
			// Имеется, как минимум, одна ожидающая нить
			_UnWaitCriticalSectionDbg(pcs);
		}
	}
}

// Удостоверяемся, что ::EnterCriticalSection() была вызвана
// до вызова этого метода
inline BOOL CheckCriticalSection(LPCRITICAL_SECTION pcs)
{
	return pcs->LockCount >= 0
	       && pcs->OwningThread == (HANDLE)::GetCurrentThreadId();
}

// Переопределяем все функции для работы с критическими секциями.
// Определение класса CLock должно быть после этих строк
#define EnterCriticalSection EnterCriticalSectionDbg
#define TryEnterCriticalSection TryEnterCriticalSectionDbg
#define LeaveCriticalSection LeaveCriticalSectionDbg

class CBaseLock
{
		friend class Lock;
		CRITICAL_SECTION m_CS;
	public:
		//[!] PPA Не включать. падает...
		//virtual ~CBaseLock() {} // [cppcheck]
		void Init()
		{
			::InitializeCriticalSection(&m_CS);
		}
		void Term()
		{
			::DeleteCriticalSection(&m_CS);
		}

		void lock()
		{
			EnterCriticalSectionDbg(&m_CS);
		}
		BOOL tryLock()
		{
			return TryEnterCriticalSectionDbg(&m_CS);
		}
		void unlock()
		{
			LeaveCriticalSectionDbg(&m_CS);
		}
		BOOL Check()
		{
			return CheckCriticalSection(&m_CS);
		}
};

class CriticalSection : public CBaseLock
{
	public:
		CriticalSection()
		{
			Init();
		}
		~CriticalSection()
		{
			Term();
		}
};

class Lock
{
		LPCRITICAL_SECTION m_pCS;
	public:
		explicit Lock(LPCRITICAL_SECTION pCS) : m_pCS(pCS)
		{
			_Lock();
		}
		explicit Lock(CBaseLock& lock) : m_pCS(&lock.m_CS)
		{
			_Lock();
		}
		~Lock()
		{
			_Unlock();
		}
	private:
		void _Lock()
		{
			EnterCriticalSectionDbg(m_pCS);
		}
		void _Unlock()
		{
			LeaveCriticalSectionDbg(m_pCS);
		}
};

typedef CriticalSection FastCriticalSection;
typedef Lock FastLock;

#endif

#if defined(IRAINMAN_USE_SHARED_SPIN_LOCK)

// Multi-reader Single-writer concurrency base class for Win32
//
// http://www.viksoe.dk/code/rwmonitor.htm
//
// This code has been modified for use in the core of FlylinkDC++.
// In addition to this was added the functional upgrade shared lock to unique lock and downgrade back,
// increased performance, also adds support for recursive entry for both locks
// in full shared-critical section ( slowly :( ).
// Author modifications Alexey Solomin (a.rainman@gmail.com), 2012.


#ifdef _DEBUG
//# define RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
# define RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
# ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
#  define RECURSIVE_SHARED_CRITICAL_SECTION_NOT_ALLOW_UNIQUE_RECUSIVE_ENTRY_AFTER_SHARED_LOCK // potential not save.
# endif // RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
#endif


#else // IRAINMAN_USE_SHARED_SPIN_LOCK

typedef CriticalSection SharedCriticalSection;
typedef Lock SharedLock;
typedef Lock UniqueLock;

typedef FastCriticalSection FastSharedCriticalSection;
typedef FastLock FastSharedLock;
typedef FastLock FastUniqueLock;

#endif // IRAINMAN_USE_SHARED_SPIN_LOCK

#endif // !defined(THREAD_H)

/**
 * @file
 * $Id: Thread.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
