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

#ifndef _BOOTSTRAPMANAGER_H
#define _BOOTSTRAPMANAGER_H

#ifdef STRONG_USE_DHT

#include "KBucket.h"

namespace dht
{


class BootstrapManager 
{
	public:
		BootstrapManager(void);
		~BootstrapManager(void);
		
		static bool bootstrap();
		static bool process();
		static void shutdown();
		static void live_check_process();
		static void inc_live_check()
		{
			++g_count_dht_test_ok;
		}
		static unsigned get_live_check()
		{
			return g_count_dht_test_ok;
		}
		static void addBootstrapNode(const string& ip, uint16_t udpPort, const CID& targetCID, const UDPKey& udpKey);
		
	private:
		static void flush_live_check();
		static string create_url_for_dht_server();
		static void dht_live_check(const char* p_operation, const string& p_param);

		static unsigned g_count_dht_test_ok;
		static std::unordered_map<string, std::pair<int, uint64_t> > g_dht_bootstrap_count; // Счетчик + временная метка
		static std::string g_user_agent;
		static CriticalSection g_cs;
		static deque<BootstrapNode> g_bootstrapNodes;
};

}
#endif // STRONG_USE_DHT

#endif  // _BOOTSTRAPMANAGER_H