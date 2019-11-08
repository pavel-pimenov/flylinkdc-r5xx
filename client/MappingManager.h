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

#pragma once


#ifndef DCPLUSPLUS_DCPP_MAPPING_MANAGER_H
#define DCPLUSPLUS_DCPP_MAPPING_MANAGER_H

#include "forward.h"
#include "typedefs.h"
#include "Util.h"
#include "..\boost\boost\logic\tribool.hpp"

class MappingManager
{
	public:
		MappingManager();
		
		static string getPortmapInfo(bool p_show_public_ip);
		static string getExternaIP()
		{
			return g_externalIP;
		}
		static void setExternaIP(const string& p_ip)
		{
			g_externalIP = p_ip;
		}
		static string getDefaultGatewayIP()
		{
			return g_defaultGatewayIP;
		}
		static string setDefaultGatewayIP(const string& p_ip)
		{
			return g_defaultGatewayIP = p_ip;
		}
		static bool isRouter()
		{
			return g_is_wifi_router.value == boost::logic::tribool::true_value;
		}
		static void close()
		{
		}
		static void open()
		{
		}
	private:
		static string g_externalIP;
		static string g_defaultGatewayIP;
		static boost::logic::tribool g_is_wifi_router;
};

#endif
