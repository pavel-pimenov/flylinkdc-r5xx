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

#ifndef DCPLUSPLUS_DCPP_TIMER_MANAGER_H
#define DCPLUSPLUS_DCPP_TIMER_MANAGER_H

#include "Speaker.h"
#include "Singleton.h"
#include <boost/thread/mutex.hpp>

#ifndef _WIN32
#include <sys/time.h>
#endif


class TimerManagerListener
{
	public:
		virtual ~TimerManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Second;
		typedef X<1> Minute;
		typedef X<2> Hour;
		
		virtual void on(Second, uint64_t) noexcept { }
		virtual void on(Minute, uint64_t) noexcept { }
		virtual void on(Hour, uint64_t) noexcept { }
};

class TimerManager : public Speaker<TimerManagerListener>, public Singleton<TimerManager>, public Thread
{
	public:
		void shutdown();
		
		static time_t getTime()
		{
			return time(nullptr);
		}
		static uint64_t getTick();
		static bool g_isStartupShutdownProcess;
		static bool g_isRun;
	private:
		friend class Singleton<TimerManager>;
		boost::timed_mutex m_mtx;
		TimerManager();
		~TimerManager();
		
		int run();
};

#define GET_TICK() TimerManager::getTick()
#define GET_TIME() TimerManager::getTime()

#endif // DCPLUSPLUS_DCPP_TIMER_MANAGER_H

/**
 * @file
 * $Id: TimerManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
