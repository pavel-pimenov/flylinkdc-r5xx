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

#ifndef DCPLUSPLUS_DCPP_THREAD_H
#define DCPLUSPLUS_DCPP_THREAD_H

#include "w.h"
// [+] IRainman fix.
#define GetSelfThreadID() ::GetCurrentThreadId()
typedef DWORD ThreadID;
// [~] IRainman fix.

#ifdef FLYLINKDC_USE_BOOST_LOCK
#include <boost/utility.hpp>
#include <boost/thread.hpp>
#endif
#ifdef _DEBUG
#include <boost/noncopyable.hpp>
#include <set>
#endif

#include "Exception.h"

#ifdef RIP_USE_THREAD_POOL
#define BASE_THREAD ThreadPool
#else
#define BASE_THREAD Thread
#endif

STANDARD_EXCEPTION(ThreadException);

// [+] IRainman fix: moved from Thread.
class BaseThread
{
	public:
		enum Priority
		{
			IDLE = THREAD_PRIORITY_IDLE,
			LOW = THREAD_PRIORITY_BELOW_NORMAL,
			NORMAL = THREAD_PRIORITY_NORMAL,
			HIGH = THREAD_PRIORITY_ABOVE_NORMAL
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

// [+] brain-ripper
// ThreadPool implementation
#ifdef RIP_USE_THREAD_POOL
class ThreadPool : public BaseThread
#ifdef _DEBUG
	, private boost::noncopyable
#endif
{
		static DWORD WINAPI starter(LPVOID p)
		{
			ThreadPool* t = (ThreadPool*)p;
			t->m_hDoneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			t->run();
			SetEvent(t->m_hDoneEvent);
			return 0;
		}
		
		HANDLE m_hDoneEvent;
		
	protected:
		virtual int run() = 0;
		
	public:
		ThreadPool();
		~ThreadPool();
		
		void start(bool executeLongTime = false) throw(ThreadException);
		
		void setThreadPriority(Priority /*p*/)
		{
			// TODO.
		}
		
		static void sleep(uint64_t millis)
		{
			::SleepEx(static_cast<DWORD>(millis), TRUE);
		}
		static void yield()
		{
			::SleepEx(0, TRUE);
		}
		
		void join() throw(ThreadException)
		{
			if (m_hDoneEvent == INVALID_HANDLE_VALUE)
			{
				return;
			}
			
			WaitForSingleObject(m_hDoneEvent, INFINITE);
			HANDLE l_thread = m_hDoneEvent;
			m_hDoneEvent = INVALID_HANDLE_VALUE;
			CloseHandle(l_thread);
		}
};
#endif // RIP_USE_THREAD_POOL

// [+] IRainman fix: detect long waits.
#ifdef _DEBUG
# define TRACING_LONG_WAITS
# ifdef TRACING_LONG_WAITS
#  define TRACING_LONG_WAITS_TIME_MS 1 * 60 * 1000
#  define DEBUG_WAITS_INIT(maxSpinCount) int _debugWaits = 0 - maxSpinCount;
#  define DEBUG_WAITS(waitTime) {\
		if (++_debugWaits == waitTime)\
		{\
			dcdebug("Thread %d waits a lockout condition for more than " #waitTime " ms.\n", GetSelfThreadID());\
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
		static void waitLockState(volatile long& state)
		{
			while (failStateLock(state))
			{
				yield();
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
		
		class ConditionLocker
#ifdef _DEBUG
			: private boost::noncopyable
#endif
		{
			public:
				ConditionLocker(volatile long& condition) : m_condition(condition)
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
		
		class ConditionLockerWithSpin
#ifdef _DEBUG
			: private boost::noncopyable
#endif
		{
			public:
				ConditionLockerWithSpin(volatile long& condition) : m_condition(condition)
				{
					waitLockStateWithSpin(m_condition);
				}
				
				~ConditionLockerWithSpin()
				{
					unlockState(m_condition);
				}
			private:
				volatile long& m_condition;
		};
		// [~] IRainman fix.
		explicit Thread() : m_threadHandle(INVALID_HANDLE_VALUE) { }
		virtual ~Thread()
		{
			if (m_threadHandle != INVALID_HANDLE_VALUE)
			{
				HANDLE l_thread = m_threadHandle;
				m_threadHandle  = INVALID_HANDLE_VALUE;
				CloseHandle(l_thread);
			}
		}
		
		void start(unsigned int p_stack_size, const char* p_name = nullptr);
		void join()
		{
			if (m_threadHandle != INVALID_HANDLE_VALUE)
			{
				WaitForSingleObject(m_threadHandle, INFINITE);
				HANDLE l_thread = m_threadHandle;
				m_threadHandle = INVALID_HANDLE_VALUE;
				CloseHandle(l_thread);
			}
		}
		static int getThreadsCount();
		bool is_active(int p_wait = 0) const
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
		
		void setThreadPriority(Priority p);
		
		static void sleep(uint64_t millis)
		{
			::Sleep(static_cast<DWORD>(millis));
		}
		static void yield()
		{
			::Sleep(0);
		}
		
	protected:
		virtual int run() = 0;
		
	private:
	
		HANDLE m_threadHandle;
		static unsigned int  WINAPI starter(void* p)
		{
//#ifdef _DEBUG
// [-] VLD      _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
//#endif
			if (Thread* t = reinterpret_cast<Thread*>(p))
				t->run();
			return 0;
		}
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
#ifdef FLYLINKDC_HE
typedef boost::detail::spinlock FastCriticalSection;
typedef boost::lock_guard<boost::detail::spinlock> FastLock;
#else
typedef CriticalSection FastCriticalSection;
typedef Lock FastLock;
#endif // FLYLINKDC_HE
#else
class CriticalSection
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		void lock()
		{
			//dcassert(cs.RecursionCount == 0 || (cs.RecursionCount > 0 && tryLock() == true));
			EnterCriticalSection(&cs);
		}
		void unlock()
		{
			LeaveCriticalSection(&cs);
			//dcassert(cs.RecursionCount == 0 || (cs.RecursionCount > 0 && tryLock() == true));
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
			InitializeCriticalSectionAndSpinCount(&cs, CRITICAL_SECTION_SPIN_COUNT); // [!] IRainman: InitializeCriticalSectionAndSpinCount
		}
		~CriticalSection()
		{
			DeleteCriticalSection(&cs);
		}
	private:
		CRITICAL_SECTION cs;
#ifndef IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
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
#endif // IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
};

// [+] IRainman fix: detect spin lock recursive entry
#ifdef _DEBUG
# define SPIN_LOCK_TRACE_RECURSIVE_ENTRY
# ifdef SPIN_LOCK_TRACE_RECURSIVE_ENTRY
#  define DEBUG_SPIN_LOCK_DECL() std::set<ThreadID> _debugOwners; volatile long _debugOwnersState
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
		explicit FastCriticalSection() : state(0)
		{
			dcdrun(DEBUG_SPIN_LOCK_INIT());
		}

		void lock()
		{
			dcdrun(DEBUG_SPIN_LOCK_INSERT());
			Thread::waitLockState(state); // [!] IRainman fix.
		}
		void unlock()
		{
			Thread::unlockState(state); // [!] IRainman fix.
			dcdrun(DEBUG_SPIN_LOCK_ERASE());
		}
	private:
		volatile long state;
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
template<class T>
class LockBase
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		explicit LockBase(T& aCs) : cs(aCs)
		{
			cs.lock();
		} // https://www.box.net/shared/81bdfde50b7c189f8240
		~LockBase()
		{
			cs.unlock();
		}
	private:
		T& cs;
};
typedef LockBase<CriticalSection> Lock;
#ifdef IRAINMAN_USE_SPIN_LOCK
typedef LockBase<FastCriticalSection> FastLock;
#else
typedef Lock FastLock;
#endif // IRAINMAN_USE_SPIN_LOCK
#endif

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
#ifndef IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
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
#endif // IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
};

class Lock
{
		LPCRITICAL_SECTION m_pCS;
	public:
		Lock(LPCRITICAL_SECTION pCS) : m_pCS(pCS)
		{
			_Lock();
		}
		Lock(CBaseLock& lock) : m_pCS(&lock.m_CS)
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

class FastSharedCriticalSection
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	private:
		dcdrun(DEBUG_SPIN_LOCK_DECL());
		volatile long sharedOwners;
		volatile long uniqueOwner;
	public:
		FastSharedCriticalSection() : sharedOwners(0), uniqueOwner(0)
		{
			dcdrun(DEBUG_SPIN_LOCK_INIT());
		}
		
		void lockShared()
		{
			dcdrun(DEBUG_SPIN_LOCK_INSERT());
			Thread::waitCondition(uniqueOwner);
			Thread::safeInc(sharedOwners);
		}
		
		void unlockShared()
		{
			Thread::safeDec(sharedOwners);
			dcdrun(DEBUG_SPIN_LOCK_ERASE());
		}
		
		void lockUnique()
		{
			dcdrun(DEBUG_SPIN_LOCK_INSERT());
			Thread::waitLockState(uniqueOwner);
			Thread::waitCondition(sharedOwners);
		}
		
		bool tryLockUnique()
		{
			if (Thread::failStateLock(uniqueOwner))
			{
				return false;
			}
			else
			{
				if (sharedOwners)
				{
					unlockUnique();
					return false;
				}
				else
				{
					dcdrun(DEBUG_SPIN_LOCK_INSERT());
					return true;
				}
			}
		}
		
		void unlockUnique()
		{
			Thread::unlockState(uniqueOwner);
			dcdrun(DEBUG_SPIN_LOCK_ERASE());
		}
};

#ifdef _DEBUG
//# define RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
# define RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
# ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
#  define RECURSIVE_SHARED_CRITICAL_SECTION_NOT_ALLOW_UNIQUE_RECUSIVE_ENTRY_AFTER_SHARED_LOCK // potential not save.
# endif // RECURSIVE_SHARED_CRITICAL_SECTION_DEAD_LOCK_TRACE
#endif

#ifdef IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION

// TODO портировать часть из WebRTC
// chromium\src\third_party\webrtc\system_wrappers\source\rw_lock_win.cc
// https://github.com/rillian/webrtc/blob/f9f128a6306634d0b66f81dca71dac69f0f8fe00/webrtc/system_wrappers/source/rw_lock_win.cc
// Реализация динамически детектирует винду а если Vista или выше используется нативные RWСекции ядра винды!

class SharedCriticalSection
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	private:
		std::multiset<ThreadID> sharedOwners;
		FastCriticalSection sharedOwnersSL;
		
		volatile long uniqueOwnerIsSet;
		volatile long /*ThreadID*/ uniqueOwnerId;
		volatile int uniqueOwnerRecursivCount;
		
		bool isUniqueOwner(const ThreadID currentThreadId) const
		{
#pragma warning(push)
#pragma warning(disable:4389)
			return uniqueOwnerId == currentThreadId;
#pragma warning(pop)
		}
		
		void addSharedOwner(const ThreadID currentThreadId)
		{
			FastLock l(sharedOwnersSL);
			sharedOwners.insert(currentThreadId);
#ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
			dcassert(sharedOwners.size() < 25);
#endif
		}
		
		void deleteSharedOwner(const ThreadID currentThreadId)
		{
			FastLock l(sharedOwnersSL);
#ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
			const auto i = sharedOwners.find(currentThreadId);
			if (i == sharedOwners.cend())
			{
				dcassert(0); // remove zombie.
			}
			sharedOwners.erase(i);
#else
			sharedOwners.erase(sharedOwners.find(currentThreadId));
#endif
		}
		
	public:
#pragma warning(push)
#pragma warning(disable:4245)
		SharedCriticalSection() : uniqueOwnerIsSet(0), uniqueOwnerRecursivCount(0), uniqueOwnerId(-1)
		{
		}
#pragma warning(pop)
		
		~SharedCriticalSection()
		{
			dcassert(sharedOwners.size() == 0);
			dcassert(uniqueOwnerIsSet == 0);
			dcassert(uniqueOwnerRecursivCount == 0);
			dcassert(uniqueOwnerId == -1);
		}
		
		void lockShared()
		{
			const ThreadID currentThreadId = GetSelfThreadID();
			if (isUniqueOwner(currentThreadId))
			{
				addSharedOwner(currentThreadId); // recursive entry after write.
			}
			else
			{
				{
					FastLock l(sharedOwnersSL);
					const auto i = sharedOwners.find(currentThreadId);
					if (i != sharedOwners.cend())
					{
						sharedOwners.insert(i, currentThreadId);
						return; // recursive entry after read.
					}
				}
				Thread::ConditionLockerWithSpin l(uniqueOwnerIsSet);
				addSharedOwner(currentThreadId); // non recursive entry.
			}
		}
		
		void unlockShared()
		{
			const ThreadID currentThreadId = GetSelfThreadID();
			deleteSharedOwner(currentThreadId);
		}
		
		bool tryLockUnique()
		{
			const ThreadID currentThreadId = GetSelfThreadID();
			if (isUniqueOwner(currentThreadId))
			{
				uniqueOwnerRecursivCount++;
#ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
				dcassert(uniqueOwnerRecursivCount > 0);
#endif
				return true; // recursive entry after write.
			}
			
			if (Thread::failStateLock(uniqueOwnerIsSet))
			{
				return false;
			}
			
			{
				FastLock l(sharedOwnersSL);
				if (sharedOwners.size() == sharedOwners.count(currentThreadId))
				{
					return true; // non recursive entry or recursive entry after read.
				}
			}
			
			return false;
		}
		
		void lockUnique()
		{
			const ThreadID currentThreadId = GetSelfThreadID();
			if (isUniqueOwner(currentThreadId))
			{
				uniqueOwnerRecursivCount++;
#ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
				dcassert(uniqueOwnerRecursivCount > 0);
#endif
				return; // recursive entry after write.
			}
			
			Thread::waitLockStateWithSpin(uniqueOwnerIsSet);
			
#ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
			dcassert(uniqueOwnerRecursivCount == 0);
#pragma warning(push)
#pragma warning(disable:4245)
			dcassert(uniqueOwnerId == -1);
#pragma warning(pop)
#endif // RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
			Thread::safeExchange(uniqueOwnerId, currentThreadId);
			
			size_t recursivSharedCountOfCurrentThreadId;
			{
				FastLock l(sharedOwnersSL);
				recursivSharedCountOfCurrentThreadId = sharedOwners.count(currentThreadId);
			}
			
			unsigned int spin = CRITICAL_SECTION_SPIN_COUNT;
			if (recursivSharedCountOfCurrentThreadId)
			{
#ifdef RECURSIVE_SHARED_CRITICAL_SECTION_NOT_ALLOW_UNIQUE_RECUSIVE_ENTRY_AFTER_SHARED_LOCK
				dcdebug("Attention, potential deadlock! Attempting unique recursive lock after a shared lock on thread %d", currentThreadId);
				dcassert(0);
#endif
				while (true)
				{
					{
						FastLock l(sharedOwnersSL);
						if (recursivSharedCountOfCurrentThreadId == sharedOwners.size())
						{
							return; // recursive entry after read.
						}
					}
					Thread::sleepWithSpin(spin);
				}
			}
			else
			{
				while (true)
				{
					{
						FastLock l(sharedOwnersSL);
						if (sharedOwners.empty())
						{
							return; // non recursive entry.
						}
					}
					Thread::sleepWithSpin(spin);
				}
			}
		}
		
		void unlockUnique()
		{
#ifdef RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
			const ThreadID currentThreadId = GetSelfThreadID();
			dcassert(uniqueOwnerIsSet == 1);
#pragma warning(push)
#pragma warning(disable:4389)
			dcassert(uniqueOwnerId == currentThreadId);
#pragma warning(pop)
			dcassert(uniqueOwnerRecursivCount >= 0);
#endif // RECURSIVE_SHARED_CRITICAL_SECTION_DEBUG
			if (uniqueOwnerRecursivCount == 0) // exit from the initial entry.
			{
#pragma warning(push)
#pragma warning(disable:4245)
				Thread::safeExchange(uniqueOwnerId, -1);
#pragma warning(pop)
				
				Thread::safeExchange(uniqueOwnerIsSet, 0);
			}
			else // exit after recursive entry.
			{
				uniqueOwnerRecursivCount--;
			}
		}
};
#else
typedef CriticalSection SharedCriticalSection;
#endif // IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION

template<class T>
class SharedLockBase
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	private:
		T& cs;
	public:
		SharedLockBase(T& shared_cs) : cs(shared_cs)
		{
			cs.lockShared();
		}
		
		~SharedLockBase()
		{
			cs.unlockShared();
		}
};

typedef SharedLockBase<FastSharedCriticalSection> FastSharedLock;

#ifdef IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
typedef SharedLockBase<SharedCriticalSection> SharedLock;
#else
typedef Lock SharedLock;
#endif

template<class T>
class UniqueLockBase
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	private:
		T& cs;
	public:
		UniqueLockBase(T& shared_cs) : cs(shared_cs)
		{
			cs.lockUnique();
		}
		
		~UniqueLockBase()
		{
			cs.unlockUnique();
		}
};

typedef UniqueLockBase<FastSharedCriticalSection> FastUniqueLock;

#ifdef IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
typedef UniqueLockBase<SharedCriticalSection> UniqueLock;
#else
typedef Lock UniqueLock;
#endif

template<class T>
class BaseTryUniqueLock
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	private:
		T& cs;
		const bool succes;
	public:
		BaseTryUniqueLock(T& shared_cs) : cs(shared_cs), succes(cs.tryLockUnique())
		{
		}
		
		bool locked() const
		{
			return succes;
		}
		
		~BaseTryUniqueLock()
		{
			if (succes)
				cs.unlockUnique();
		}
};

typedef BaseTryUniqueLock<FastSharedCriticalSection> TryFastUniqueLock;

typedef BaseTryUniqueLock<SharedCriticalSection> TryUniqueLock;

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
