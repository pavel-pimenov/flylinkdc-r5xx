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

#include "stdinc.h"
#include "MappingManager.h"
#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "CryptoManager.h"
#include "LogManager.h"
#include "SearchManager.h"
#include "ScopedFunctor.h"
#include "../FlyFeatures/flyServer.h"

string MappingManager::g_externalIP;
string MappingManager::g_defaultGatewayIP;
boost::logic::tribool MappingManager::g_is_wifi_router = boost::logic::indeterminate;

MappingManager::MappingManager()
{
	g_defaultGatewayIP = Socket::getDefaultGateWay(g_is_wifi_router);
}

string MappingManager::getPortmapInfo(bool p_show_public_ip)
{
	string l_description;
	l_description = "Mode:";
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		l_description = "+Auto";
	}
	switch (SETTING(INCOMING_CONNECTIONS))
	{
		case SettingsManager::INCOMING_DIRECT:
			l_description += "+Direct";
			break;
		case SettingsManager::INCOMING_FIREWALL_UPNP:
			l_description += "+UPnP";
			break;
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
			l_description += "+Passive";
			break;
		case SettingsManager::INCOMING_FIREWALL_NAT:
			l_description += "+NAT+Manual";
			break;
		default:
			dcassert(0);
	}
	if (isRouter())
	{
		l_description += "+Router";
	}
	if (!getExternaIP().empty())
	{
		if (Util::isPrivateIp(getExternaIP()))
		{
			l_description += "+Private IP";
		}
		else
		{
			l_description += "+Public IP";
		}
		if (p_show_public_ip)
		{
			l_description += ": " + getExternaIP();
		}
	}
	auto calcTestPortInfo = [](const string & p_name_port, const boost::logic::tribool & p_status, const uint16_t p_port) ->string
	{
		string l_result = "," + p_name_port + ":" + Util::toString(p_port);
		if (p_status == true)
			l_result += "(+)";
		else if (p_status == false)
			l_result += "(-)";
		else
			l_result.clear();
		return l_result;
	};
	if (CFlyServerJSON::isTestPortOK(SETTING(UDP_PORT), "udp"))
	{
		l_description += calcTestPortInfo("UDP", SettingsManager::g_TestUDPSearchLevel, SETTING(UDP_PORT));
	}
	if (CFlyServerJSON::isTestPortOK(SETTING(TCP_PORT), "tcp"))
	{
		l_description += calcTestPortInfo("TCP", SettingsManager::g_TestTCPLevel, SETTING(TCP_PORT));
	}
	/*
	if (CFlyServerJSON::isTestPortOK(SETTING(DHT_PORT), "udp"))
	    {
	        l_description += calcTestPortInfo("Torrent", SettingsManager::g_TestTorrentLevel, SETTING(DHT_PORT));
	    }
	*/
	if (CryptoManager::TLSOk() && SETTING(TLS_PORT) > 1024)
	{
		l_description += calcTestPortInfo("TLS", SettingsManager::g_TestTLSLevel, SETTING(TLS_PORT));
	}
	return l_description;
}