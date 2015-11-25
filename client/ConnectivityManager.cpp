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
#include "format.h"
#include "ConnectivityManager.h"
#include "ClientManager.h"
#include "ConnectionManager.h"
#include "LogManager.h"
#include "MappingManager.h"
#include "SearchManager.h"
#include "SettingsManager.h"
#include "../FlyFeatures/flyServer.h"

#ifdef STRONG_USE_DHT
#include "../dht/dht.h"
#endif

ConnectivityManager::ConnectivityManager() :
	autoDetected(false),
	running(false)
{
}

void ConnectivityManager::startSocket()
{
	autoDetected = false;
	
	disconnect();
	
	// active check mustn't be there to hub-dependent setting works correctly
	//if(ClientManager::isActive()) {
	listen();
	// must be done after listen calls; otherwise ports won't be set
	if (SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_UPNP)
	{
		MappingManager::getInstance()->open();
	}
	//}
}

void ConnectivityManager::detectConnection()
{
	if (running)
		return;
	running = true;
	
	m_status.clear();
	// PPA_INCLUDE_DEAD_CODE fly_fire1(ConnectivityManagerListener::Started());
	
	const string l_old_bind = SETTING(BIND_ADDRESS);
	// restore connectivity settings to their default value.
	SettingsManager::getInstance()->unset(SettingsManager::EXTERNAL_IP);
	SettingsManager::getInstance()->unset(SettingsManager::BIND_ADDRESS);
	if (MappingManager::getInstance()->getOpened())
	{
		MappingManager::getInstance()->close();
	}
	
	disconnect();
	
	// FlylinkDC Team TODO: Need to check whether crowned with success port mapping via UPnP.
	// For direct connection, you must run the same test that the outside world can not connect to port!
	// ps: need to migrate this functionality from the wizard here!
	
	log(STRING(DETERMIN_CON_TYPE));
	SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_PASSIVE);
	try
	{
		listen();
	}
	catch (const Exception& e)
	{
		SET_SETTING(ALLOW_NAT_TRAVERSAL, true);
		SET_SETTING(BIND_ADDRESS, l_old_bind);
		
		AutoArray<char> buf(512);
		_snprintf(buf.data(), 512, CSTRING(UNABLE_TO_OPEN_PORT), e.getError().c_str());
		log(buf.data());
		// PPA_INCLUDE_DEAD_CODE fly_fire1(ConnectivityManagerListener::Finished());
		running = false;
		return;
	}
	
	autoDetected = true;
	boost::logic::tribool l_is_wifi_router;
	const auto l_ip_gateway = Socket::getDefaultGateWay(l_is_wifi_router);
	if (l_is_wifi_router)
	{
		log("WiFi router detected IP = " + l_ip_gateway);
		SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_UPNP);
		// PPA_INCLUDE_DEAD_CODE fly_fire1(ConnectivityManagerListener::Finished());
	}
	else
	{
		const auto l_ip = Util::getLocalOrBindIp(false);
		if (!Util::isPrivateIp(l_ip)) // false - дл€ детекта внешнего IP
		{
			SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_DIRECT); // ¬от тут сомнительно
			log(STRING(PUBLIC_IP_DETECTED) + " IP = " + l_ip);
			// PPA_INCLUDE_DEAD_CODE fly_fire1(ConnectivityManagerListener::Finished());
			running = false;
			return;
		}
	}
	log(STRING(LOCAL_NET_NAT_DETECT));
	
	if (!MappingManager::getInstance()->open())
	{
		running = false;
	}
}

void ConnectivityManager::setup_connections(bool settingsChanged)
{
	try
	{
		if (BOOLSETTING(AUTO_DETECT_CONNECTION))
		{
			if (!autoDetected)
				detectConnection();
		}
		else
		{
			if (autoDetected || settingsChanged)
			{
				if (settingsChanged || SETTING(INCOMING_CONNECTIONS) != SettingsManager::INCOMING_FIREWALL_UPNP)
				{
					MappingManager::getInstance()->close();
				}
				startSocket();
			}
			else if (SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_UPNP && !MappingManager::getInstance()->getOpened())
			{
				// previous mappings had failed; try again
				MappingManager::getInstance()->open();
			}
		}
	}
	catch (const Exception& e)
	{
		dcassert(0);
		const string l_error = "ConnectivityManager::setup error = " + e.getError();
		CFlyServerJSON::pushError(56, l_error);
	}
	if (settingsChanged && SETTING(INCOMING_CONNECTIONS) != SettingsManager::INCOMING_FIREWALL_PASSIVE)
	{
		// Test Port
		string l_external_ip;
		std::vector<unsigned short> l_udp_port, l_tcp_port;
		l_udp_port.push_back(SETTING(UDP_PORT));
		l_udp_port.push_back(SETTING(DHT_PORT));
		l_tcp_port.push_back(SETTING(TCP_PORT));
		l_tcp_port.push_back(SETTING(TLS_PORT));
		if (CFlyServerJSON::pushTestPort(l_udp_port, l_tcp_port, l_external_ip, 0))
		{
			if (!l_external_ip.empty())
			{
				SET_SETTING(EXTERNAL_IP, l_external_ip);
			}
		}
	}
}

string ConnectivityManager::getInformation() const
{
	if (running)
	{
		return "Connectivity settings are being configured; try again later";
	}
	
//	string autoStatus = ok() ? str(F_("enabled - %1%") % getStatus()) : "disabled";

	string mode;
	switch (SETTING(INCOMING_CONNECTIONS))
	{
		case SettingsManager::INCOMING_DIRECT:
		{
			mode = "Active mode";
			break;
		}
		case SettingsManager::INCOMING_FIREWALL_UPNP:
		{
			const auto l_status = MappingManager::getInstance()->getStatus();
			mode = str(F_("Active mode behind a router that %1% can configure; port mapping status: %2%") %
			           APPNAME % l_status);
#ifdef FLYLINKDC_USE_GATHER_STATISTICS
			g_fly_server_stat.m_upnp_router_name = MappingManager::getInstance()->getDeviceString();
			g_fly_server_stat.m_upnp_status = l_status;
#endif
			break;
		}
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
		{
			mode = "Passive mode";
			break;
		}
	}
	
	auto field = [](const string & s)
	{
		return s.empty() ? "undefined" : s;
	};
	
	return str(F_(
	               "Connectivity information:\n"
	               "\tExternal IP (v4): %1%\n"
	               "\tBound interface (v4): %2%\n"
	               "\tTransfer port: %3%\n"
	               "\tEncrypted transfer port: %4%\n"
	               "\tSearch port: %5%\n"
	               "\tDHT port: %6%\n"
	               "\tStatus: %7%"
	           ) %
	           field(SETTING(EXTERNAL_IP)) %
	           field(SETTING(BIND_ADDRESS)) %
	           field(Util::toString(ConnectionManager::getInstance()->getPort())) %
	           field(Util::toString(ConnectionManager::getInstance()->getSecurePort())) %
	           field(SearchManager::getSearchPort()) %
#ifdef STRONG_USE_DHT
	           field(Util::toString(dht::DHT::getInstance()->getPort())) %
#else
	           field(" DHT Disable!") %
#endif
	           field(getStatus())
	          );
}

void ConnectivityManager::mappingFinished(const string& p_mapper)
{
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		if (p_mapper.empty())
		{
			//StrongDC++: don't disconnect when mapping fails else DHT and active mode in favorite hubs won't work
			//disconnect();
			SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_PASSIVE);
			SET_SETTING(ALLOW_NAT_TRAVERSAL, true);
			log(STRING(AUTOMATIC_SETUP_ACTIV_MODE_FAILED));
		}
#ifdef FLYLINKDC_BETA
		else
		{
			if (!MappingManager::getExternaIP().empty() && Util::isPrivateIp(MappingManager::getExternaIP()))
			{
				SET_SETTING(INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_PASSIVE);
				SET_SETTING(ALLOW_NAT_TRAVERSAL, true);
				const string l_error = "Auto passive mode: Private IP = " + MappingManager::getExternaIP();
				log(l_error);
				CFlyServerJSON::pushError(29, l_error);
			}
		}
#endif
		// PPA_INCLUDE_DEAD_CODE fly_fire1(ConnectivityManagerListener::Finished());
	}
	if (!ClientManager::isShutdown())
	{
		log(getInformation());
		SET_SETTING(MAPPER, p_mapper);
	}
	running = false;
}

void ConnectivityManager::listen() // TODO - fix copy-paste
{
	string l_exceptions;
	for (int i = 0; i < 5; ++i)
	{
		try
		{
			ConnectionManager::getInstance()->listen();
		}
		catch (const SocketException& e)
		{
			if (e.getErrorCode() == 10048)
			{
				if (i == 0)
				{
					SET_SETTING(TCP_PORT, 0); // ѕервую попытку делаем подключа€сь к порту = 0
					continue;
				}
				SettingsManager::generateNewTCPPort();
				LogManager::message("Try bind random TCP Port = " + Util::toString(SETTING(TCP_PORT)));
				continue;
			}
			else
			{
				l_exceptions += " * TCP/TLS listen error = " + e.getError() + "\r\n";
			}
		}
		break;
	}
	try
	{
		SearchManager::getInstance()->listen();
	}
	catch (const Exception& e)
	{
		l_exceptions += " * UDP listen error = " + e.getError() + "\r\n";
	}
	
#ifdef STRONG_USE_DHT
	try
	{
		if (dht::DHT::getInstance()->getPort() &&  dht::DHT::getInstance()->getPort() != SETTING(DHT_PORT))
		{
			// TODO dht::DHT::getInstance()->stop(); это не пашет - нужно разрушение
		}
		dht::DHT::getInstance()->start();
	}
	catch (const Exception& e)
	{
		l_exceptions += " * DHT listen error = " + e.getError() + "\r\n";
	}
#endif
	if (!l_exceptions.empty())
	{
		throw Exception("ConnectivityManager::listen() error:\r\n" + l_exceptions);
	}
}

void ConnectivityManager::disconnect()
{
	SearchManager::getInstance()->disconnect();
	ConnectionManager::getInstance()->disconnect();
#ifdef STRONG_USE_DHT
	dht::DHT::getInstance()->stop();
#endif
}

void ConnectivityManager::log(const string& message)
{
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		m_status = message;
		LogManager::message(STRING(CONNECTIVITY) + ' ' + m_status);
		// PPA_INCLUDE_DEAD_CODE fly_fire1(ConnectivityManagerListener::Message(), m_status);
	}
	else
	{
		LogManager::message(message);
	}
}