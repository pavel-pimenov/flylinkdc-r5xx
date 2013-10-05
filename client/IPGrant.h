/*
 * Copyright (C) 2011-2013 FlylinkDC++ Team http://flylinkdc.com/
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


#ifndef IPGRANT_H
#define IPGRANT_H

#include "Singleton.h"
#include "SettingsManager.h"

#ifdef SSA_IPGRANT_FEATURE

#include "iplist.h"

class IpGrant : public Singleton<IpGrant>, private SettingsManagerListener
{
	public:
		IpGrant()
		{
			SettingsManager::getInstance()->addListener(this);
		}
		
		~IpGrant()
		{
			SettingsManager::getInstance()->removeListener(this);
		}
		
		bool check(const std::string& aIP);
		void load();
		void clear()
		{
			FastLock l(m_cs);
			m_ipList.clear();
		}
		
		static string getConfigFileName()
		{
			return Util::getConfigPath() + "IPGrant.ini";
		}
	private:
	
		FastCriticalSection m_cs; // [!] IRainman opt: use spin lock here.
		IPList m_ipList;
		
		// SettingsManagerListener
		void on(SettingsManagerListener::Load, SimpleXML& /*xml*/) noexcept
		{
			load();
		}
};
#endif // SSA_IPGRANT_FEATURE

#endif // IPGRANT_H