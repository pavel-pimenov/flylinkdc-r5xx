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
#include "MappingManager.h"
#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "CompatibilityManager.h"
#include "CryptoManager.h"
#include "LogManager.h"
#include "Mapper_MiniUPnPc.h"
#include "Mapper_NATPMP.h"
#include "Mapper_WinUPnP.h"
#include "SearchManager.h"
#include "ScopedFunctor.h"
#include "../FlyFeatures/flyServer.h"

#ifdef STRONG_USE_DHT
#include "../dht/dht.h"
#endif


string MappingManager::g_externalIP;
string MappingManager::g_defaultGatewayIP;
string MappingManager::g_mapperName;
boost::logic::tribool MappingManager::g_is_wifi_router = boost::logic::indeterminate;

MappingManager::MappingManager() : m_renewal(0), m_listeners_count(0)
{
	//m_busy = false;
	g_defaultGatewayIP = Socket::getDefaultGateWay(g_is_wifi_router);
	addMapper<Mapper_MiniUPnPc>();
	addMapper<Mapper_NATPMP>();
#ifdef HAVE_NATUPNP_H
	addMapper<Mapper_WinUPnP>();
#endif
}
StringList MappingManager::getMappers() const
{
	StringList ret;
	ret.reserve(m_mappers.size());
	for (auto i = m_mappers.cbegin(), iend = m_mappers.cend(); i != iend; ++i)
	{
		ret.push_back(i->first);
	}
	return ret;
}

bool MappingManager::open()
{
	if (getOpened())
		return false;
		
	if (m_mappers.empty())
	{
		log_internal(STRING(UPNP_NO_IMPLEMENTATION));
		return false;
	}
	
	if (m_busy.test_and_set())
	{
		log_internal("Another UPnP port mapping attempt is in progress...");
		return false;
	}
	
	start(64);
	
	return true;
}

void MappingManager::close()
{
	if (m_listeners_count)
	{
		TimerManager::getInstance()->removeListener(this);
		--m_listeners_count;
	}
	
	if (m_working.get())
	{
		close(*m_working);
		m_working.reset();
	}
}

bool MappingManager::getOpened() const
{
	return m_working.get() != NULL;
}

string MappingManager::getStatus() const
{
	if (m_working.get())
	{
		const auto& mapper = *m_working;
		return str(F_("Successfully created port mappings on the %1% device with the %2% interface") %
		           deviceString(mapper) % mapper.getMapperName());
	}
	return "Failed to create port mappings";
}

int MappingManager::run()
{
	g_mapperName.clear();
	ScopedFunctor([this] { m_busy.clear(); });
	
	// cache these
	const uint16_t conn_port = ConnectionManager::getInstance()->getPort();
	const uint16_t secure_port = ConnectionManager::getInstance()->getSecurePort();
	const uint16_t search_port = SearchManager::getSearchPortUint();
#ifdef STRONG_USE_DHT
	const uint16_t dht_port = dht::DHT::getInstance()->getPort();
#endif
	if (m_renewal
	        && getOpened()) //[+]FlylinkDC++ Team
	{
		Mapper& mapper = *m_working;
		
		ScopedFunctor([&mapper] { mapper.uninit(); });
		if (!mapper.init())
		{
			// can't renew; try again later.
			m_renewal = GET_TICK() + std::max(mapper.renewal(), 10u) * 60 * 1000;
			return 0;
		}
		
		auto addRule = [this, &mapper](const unsigned short port, Mapper::Protocol protocol, const string & description) -> bool
		{
			// just launch renewal requests - don't bother with possible failures.
			if (port)
			{
				return mapper.open(port, protocol, getFlylinkDCAppCaption() + description + " port (" + Util::toString(port) + ' ' + Mapper::protocols[protocol] + ")");
			}
			return false;
		};
		SettingsManager::upnpPortLevelInit();
		g_mapperName = mapper.getMapperName();
		SettingsManager::g_upnpTCPLevel = addRule(conn_port, Mapper::PROTOCOL_TCP, ("Transfer"));
		if (CryptoManager::TLSOk())
		{
			SettingsManager::g_upnpTLSLevel = addRule(secure_port, Mapper::PROTOCOL_TCP, ("Encrypted transfer"));
		}
		SettingsManager::g_upnpUDPSearchLevel = addRule(search_port, Mapper::PROTOCOL_UDP, ("Search"));
#ifdef STRONG_USE_DHT
		if (BOOLSETTING(USE_DHT))
		{
			SettingsManager::g_upnpUDPDHTLevel = addRule(dht_port, Mapper::PROTOCOL_UDP, dht::NetworkName);
		}
#endif
		
		auto minutes = mapper.renewal();
		if (minutes)
		{
			m_renewal = GET_TICK() + std::max(minutes, 10u) * 60 * 1000;
		}
		else
		{
			if (m_listeners_count)
			{
				TimerManager::getInstance()->removeListener(this);
				--m_listeners_count;
			}
		}
		
		return 0;
	}
	
	// move the preferred mapper to the top of the stack.
	const auto setting = SETTING(MAPPER);
	for (auto i = m_mappers.cbegin(); i != m_mappers.cend(); ++i)
	{
		if (i->first == setting)
		{
			if (i != m_mappers.cbegin())
			{
				const auto mapper = *i;
				m_mappers.erase(i);
				m_mappers.insert(m_mappers.begin(), mapper);
			}
			break;
		}
	}
	
	for (auto i = m_mappers.cbegin(); i != m_mappers.cend(); ++i)
	{
		unique_ptr<Mapper> pMapper(i->second());
		Mapper& mapper = *pMapper;
		
		ScopedFunctor([&mapper] { mapper.uninit(); });
		if (!mapper.init())
		{
			log_internal("Failed to initalize the " + mapper.getMapperName() + " interface");
			continue;
		}
		auto addRule = [this, &mapper](const unsigned short port, Mapper::Protocol protocol, const string & description) -> bool
		{
			bool l_is_ok = true;
			if (port)
			{
				string l_info;
				string l_info_port = description + " port (" + Util::toString(port) + ' ' + Mapper::protocols[protocol] + ')';
				if (!mapper.open(port, protocol, getFlylinkDCAppCaption() + l_info_port))
				{
					l_info = "Failed to map the ";
					// не скидываем все пробросы! mapper.close();
					l_is_ok = false;
				}
#ifdef STRONG_USE_DHT
				else
				{
					if (protocol == Mapper::PROTOCOL_UDP && port == SETTING(DHT_PORT) /* && description == dht::NetworkName */)
					{
						dht::DHT::test_dht_port();
					}
					l_info = "Successfully Port Forwarding ";
					l_is_ok = true;
				}
#endif // STRONG_USE_DHT
				log_internal(l_info + l_info_port + " with the " + mapper.getMapperName() + " interface");
			}
			return l_is_ok;
		};
		
		g_mapperName.clear();
		SettingsManager::upnpPortLevelInit();
		const bool l_is_map_tcp = addRule(conn_port, Mapper::PROTOCOL_TCP, "Transfer");
		bool l_is_map_tls = false;
		if (CryptoManager::TLSOk())
		{
			l_is_map_tls = addRule(secure_port, Mapper::PROTOCOL_TCP, "Encrypted transfer");
		}
		const bool l_is_map_udp = addRule(search_port, Mapper::PROTOCOL_UDP, "Search");
#ifdef STRONG_USE_DHT
		bool l_is_map_dht = false;
		if (BOOLSETTING(USE_DHT))
		{
			l_is_map_dht = addRule(dht_port, Mapper::PROTOCOL_UDP, dht::NetworkName);
			
			SettingsManager::g_upnpUDPDHTLevel = l_is_map_dht;
		}
#endif
		SettingsManager::g_upnpUDPSearchLevel = l_is_map_udp;
		SettingsManager::g_upnpTCPLevel = l_is_map_tcp;
		if (CryptoManager::TLSOk())
		{
			SettingsManager::g_upnpTLSLevel = l_is_map_tls;
		}
		
		if (!(l_is_map_tcp
		        && l_is_map_udp
		        && (l_is_map_tls || !CryptoManager::TLSOk())
#ifdef STRONG_USE_DHT
		        && (l_is_map_dht || !BOOLSETTING(USE_DHT))
#endif
		     ))
		{
			dcassert(0);
			continue;
		}
		
		g_mapperName = mapper.getMapperName();
		log_internal(STRING(UPNP_SUCCESSFULLY_CREATED_MAPPINGS));
		
		m_working = move(pMapper); // [IntelC++ 2012 beta2] warning #734: "std::unique_ptr<_Ty, _Dx>::unique_ptr(const std::unique_ptr<_Ty, _Dx>::_Myt &) [with _Ty=Mapper, _Dx=std::default_delete<Mapper>]" (declared at line 2347 of "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include\memory"), required for copy that was eliminated, is inaccessible
		
		g_externalIP = mapper.getExternalIP();
		if (g_externalIP.empty())
		{
			// no cleanup because the mappings work and hubs will likely provide the correct IP.
			log_internal(STRING(UPNP_FAILED_TO_GET_EXTERNAL_IP));
		}
		
		ConnectivityManager::getInstance()->mappingFinished(mapper.getMapperName());
		
		auto minutes = mapper.renewal();
		if (minutes)
		{
			m_renewal = GET_TICK() + std::max(minutes, 10u) * 60 * 1000;
			if (m_listeners_count == 0)
			{
				TimerManager::getInstance()->addListener(this);
				++m_listeners_count;
			}
		}
		ClientManager::infoUpdated(true);
		break;
	}
	
	if (!getOpened())
	{
		log_internal(STRING(UPNP_FAILED_TO_CREATE_MAPPINGS));
		ConnectivityManager::getInstance()->mappingFinished(Util::emptyString);
		ClientManager::upnp_error_force_passive();
	}
	
	return 0;
}

void MappingManager::close(Mapper& mapper)
{
	if (mapper.hasRules())
	{
		bool ret = mapper.init();
		if (ret)
		{
			ret = mapper.close_all_rules();
			mapper.uninit();
			SettingsManager::upnpPortLevelInit();
		}
		if (!ret)
		{
			log_internal(STRING(UPNP_FAILED_TO_REMOVE_MAPPINGS));
		}
	}
}

void MappingManager::log_internal(const string& message)
{
	LogManager::message("Port mapping: " + message);
}

string MappingManager::deviceString(const Mapper& p_mapper) const
{
	const auto l_dev_name = p_mapper.getDeviceName();
	const auto l_model_desc = p_mapper.getModelDescription();
	string l_name = l_dev_name;
	if (!l_name.empty() || !l_model_desc.empty())
	{
		if (l_model_desc != l_dev_name)
		{
			return l_name + '(' + l_model_desc + ')';
		}
		else
		{
			return l_model_desc;
		}
	}
	else
	{
		return Util::emptyString;
	}
}

void MappingManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	if (tick >= m_renewal && !m_busy.test_and_set())
	{
		start(64);
	}
}
string MappingManager::getPortmapInfo(bool p_add_router_name, bool p_show_public_ip)
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
			if (g_mapperName.empty())
			{
				//l_description += "(error)";
			}
			else
			{
				l_description += "(" + g_mapperName + ")";
			}
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
	if (MappingManager::isRouter())
	{
		l_description += "+Router";
		if (p_add_router_name)
		{
			l_description += ": " + (CompatibilityManager::g_upnp_router_model.empty() ? "undefined" : CompatibilityManager::g_upnp_router_model);
		}
	}
	if (!MappingManager::getExternaIP().empty())
	{
		if (Util::isPrivateIp(MappingManager::getExternaIP()))
		{
			l_description += "+Private IP";
		}
		else
		{
			l_description += "+Public IP";
		}
		if (p_show_public_ip)
		{
			l_description += ": " + MappingManager::getExternaIP();
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
#ifdef STRONG_USE_DHT
	if (dht::DHT::isValidInstance() && dht::DHT::getInstance()->getPort())
	{
		if (CFlyServerJSON::isTestPortOK(SETTING(DHT_PORT), "udp"))
		{
			l_description += calcTestPortInfo("DHT", SettingsManager::g_TestUDPDHTLevel, SETTING(DHT_PORT));
		}
	}
#endif
	if (CryptoManager::TLSOk() && SETTING(TLS_PORT) > 1024)
	{
		l_description += calcTestPortInfo("TLS", SettingsManager::g_TestTLSLevel, SETTING(TLS_PORT));
	}
	return l_description;
}