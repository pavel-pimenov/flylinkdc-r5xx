/*
 * Copyright (C) 2011-2021 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef IPGUARD_H
#define IPGUARD_H

#include "Socket.h"
#include "iplist.h"

class IpGuard
{
	public:
		IpGuard()
		{
		}
		
		~IpGuard()
		{
		}
		
		static bool check_ip_str(const string& aIP, string& reason);
		static void check_ip_str(const string& aIP, Socket* socket = nullptr);
		static bool is_block_ip(const string& aIP, uint32_t& p_ip4);
		
		
		static void load();
		static void clear()
		{
			g_ipGuardList.clear();
		}
		static const string getIPGuardFileName()
		{
			return Util::getConfigPath() + "IPGuard.ini";
		}
		
	private:
		static IPList g_ipGuardList;
		static int g_ipGuardListLoad;
};

#endif // IPGUARD_H