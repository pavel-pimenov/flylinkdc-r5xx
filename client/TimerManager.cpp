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
#include "TimerManager.h"
#ifdef _DEBUG
#include "ClientManager.h"
#endif

#ifdef _DEBUG
// [!] IRainman fix.
//#define TIMER_MANAGER_DEBUG // For diagnosis long-running events.
//#define USE_LONG_SECONDS // Simple, but not very accurate generation of ticks. If disabled uses real seconds.
#endif

#include <boost/date_time/posix_time/ptime.hpp>

using namespace boost::posix_time;
static auto g_start = microsec_clock::universal_time(); // [!] IRainamn fix.
static volatile bool g_running = false; // [+] IRainman fix.

bool TimerManager::g_isStartupShutdownProcess = false;

TimerManager::TimerManager()
{
	// This mutex will be unlocked only upon shutdown
	m_mtx.lock();
	g_running = true; // [+] IRainman fix.
}

TimerManager::~TimerManager()
{
}

void TimerManager::shutdown()
{
	m_mtx.unlock();
	join();
	// [+] IRainman fix.
	while (g_running)
	{
		sleep(10);
	}
	// [~] IRainman fix.
}

int TimerManager::run()
{
	// [!] IRainman TimerManager fix.
	// 1) events are generated every second.
	// 2) if the current event handlers ran more than a second - the next event will be produced immediately.
	
	auto now = microsec_clock::universal_time();
	// shows the time of planned launch event.
	auto nextSecond = now + seconds(1);
	auto nextMin = now + minutes(1);
	auto nextHour = now + hours(1);
	// ~shows the time of planned launch event.
	while (!m_mtx.timed_lock(nextSecond))
	{
		// ======================================================
		now = microsec_clock::universal_time();
#ifdef USE_LONG_SECONDS
		nextSecond = now + seconds(1);
#else
		nextSecond += seconds(1);
		if (nextSecond <= now)
		{
			dcdebug("TimerManager warning: Previous cycle executed " U64_FMT " ms.\n", (now + seconds(1) - nextSecond).total_milliseconds());
			nextSecond = now + seconds(1);
		}
#endif
		// ======================================================
		const auto t = (now - g_start).total_milliseconds();
#ifdef TIMER_MANAGER_DEBUG
		dcdebug("TimerManagerListener::Second() with tick=%u\n", t);
#endif
		dcassert(!ClientManager::isShutdown());
		if (g_isStartupShutdownProcess == true) // Пока стартуем не тикаем таймером
			continue;
		fire(TimerManagerListener::Second(), t);
		// ======================================================
		if (nextMin <= now)
		{
			nextMin += minutes(1);
#ifdef TIMER_MANAGER_DEBUG
			dcdebug("TimerManagerListener::Minute() with tick=%u\n", t);
#endif
			fire(TimerManagerListener::Minute(), t);
			// ======================================================
			if (nextHour <= now)
			{
				nextHour += hours(1);
#ifdef TIMER_MANAGER_DEBUG
				dcdebug("TimerManagerListener::Hour() with tick=%u\n", t);
#endif
				fire(TimerManagerListener::Hour(), t);
			}
			// ======================================================
		}
		// ======================================================
	}
	// [~] IRainman fix
	
	m_mtx.unlock();
	dcdebug("TimerManager done\n");
	g_running = false; // [+] IRainman fix.
	return 0;
}

uint64_t TimerManager::getTick()
{
	return (microsec_clock::universal_time() - g_start).total_milliseconds();
	// [2] https://www.box.net/shared/d5b52c09bf8af16676a4 https://www.box.net/shared/02663109fbecb2d2a18d
	// boost::throw_exception(std::runtime_error("could not convert calendar time to UTC time"));
}

/**
 * @file
 * $Id: TimerManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
