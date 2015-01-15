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

#ifndef DCPLUSPLUS_DCPP_MAPPING_MANAGER_H
#define DCPLUSPLUS_DCPP_MAPPING_MANAGER_H

#include <boost/atomic.hpp>

#include "forward.h"
#include "typedefs.h"
#include "Mapper.h"
#include "TimerManager.h"
#include "Util.h"

class MappingManager :
	public Singleton<MappingManager>,
	private Thread,
	private TimerManagerListener
#ifdef _DEBUG
	, virtual NonDerivable<MappingManager> // [+] IRainman fix.
#endif
{
	public:
		/** add an implementation derived from the base Mapper class, passed as template parameter.
		the first added mapper will be tried first, unless the "MAPPER" setting is not empty. */
		template<typename T> void addMapper()
		{
			mappers.push_back(make_pair(T::g_name, [] { return new T(); }));
		}
		StringList getMappers() const;
		
		bool open();
		void close();
		bool getOpened() const;
		string getStatus() const;
		string getDeviceString() const
		{
			if (working.get())
			{
				return deviceString(*working);
			}
			else
				return Util::emptyString;
		}
		static string getExternaIP()
		{
			return g_externalIP;
		}
		static string getDefaultGatewayIP()
		{
			return g_defaultGatewayIP;
		}
		static string setDefaultGatewayIP(const string& p_ip)
		{
			return g_defaultGatewayIP = p_ip;
		}
	private:
		friend class Singleton<MappingManager>;
		
		vector<pair<string, std::function<Mapper * ()>>> mappers;
		
		boost::atomic_flag m_busy;
		unique_ptr<Mapper> working; /// currently working implementation.
		uint64_t renewal; /// when the next renewal should happen, if requested by the mapper.
		int m_listeners_count;
		static string g_externalIP;
		static string g_defaultGatewayIP;
		
		MappingManager();
		/*virtual*/
		~MappingManager() // [!] IRainman fix: NonDerivable
		{
			join();
		}
		
		int run();
		
		void close(Mapper& mapper);
		void log(const string& message);
		string deviceString(const Mapper& p_mapper) const;
		
		void on(TimerManagerListener::Minute, uint64_t tick) noexcept;
};

#endif
