/*
 * Copyright (C) 2009-2011 Big Muscle, http://strongdc.sf.net
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

#ifndef _TASKMANAGER_H
#define _TASKMANAGER_H

#ifdef STRONG_USE_DHT

#include "KBucket.h"

#include "../client/Singleton.h"
#include "../client/TimerManager.h"

namespace dht
{

class TaskManager :
	public Singleton<TaskManager>, private TimerManagerListener
{
	public:
		TaskManager(void);
		~TaskManager(void);
		
		void start();
		void stop();
		
		void addZone(RoutingTable* zone)
		{
			CFlyFastLock(cs);
			zones.push_back(zone);
		}
		void removeZone(RoutingTable* zone)
		{
			CFlyFastLock(cs);
			zones.erase(remove(zones.begin(), zones.end(), zone), zones.end());
		}
#ifdef _DEBUG
		bool isDebugTimerExecute() const 
		{
			return m_debugIsTimerExecute != 0;
		}
#endif
		
	private:
	
		/** Time of publishing next file in queue */
		uint64_t nextPublishTime;
		
		/** When running searches will be processed */
		uint64_t nextSearchTime;
		
		/** When initiate searching for myself */
		uint64_t nextSelfLookup;
		
		/** When request next firewall check */
		uint64_t nextFirewallCheck;
		
		uint64_t lastBootstrap;
		uint64_t m_lastDownloadDHTError;
		dcdrun(volatile long m_debugIsTimerExecute;) // [+] FlylinkDC++ test.

		
		std::vector<RoutingTable*> zones;
		FastCriticalSection cs; // [!] IRainman opt: use spin lock here.
		
		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept override;
		void on(TimerManagerListener::Minute, uint64_t aTick) noexcept override;
};

}

#endif // STRONG_USE_DHT

#endif  // _TASKMANAGER_H