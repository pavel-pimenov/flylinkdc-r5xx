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
#ifndef IPGUARD_H
#define IPGUARD_H

#ifdef PPA_INCLUDE_IPGUARD

#include "Socket.h"
#include "Singleton.h"
#include "SimpleXML.h"
#include "iplist.h"

class IpGuard : public Singleton<IpGuard>, private SettingsManagerListener
{
	public:
		IpGuard()
		{
			SettingsManager::getInstance()->addListener(this);
		}
		
		~IpGuard()
		{
			SettingsManager::getInstance()->removeListener(this);
		}
		
		bool check(const string& aIP, string& reason);
		void check(uint32_t aIP, Socket* socket = nullptr);
		
		void load();
		void clear()
		{
			m_ipGuardList.clear();
		}
		static const string getIPGuardFileName()
		{
			return Util::getConfigPath() + "IPGuard.ini";
		}
		
	private:
		IPList m_ipGuardList;
		
		// SettingsManagerListener
		void on(SettingsManagerListener::Load, SimpleXML& /*xml*/) noexcept
		{
			load();
		}
};
#endif // PPA_INCLUDE_IPGUARD

#endif // IPGUARD_H