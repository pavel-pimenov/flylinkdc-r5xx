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
#include "format.h"
#include "ConnectionManager.h"
#include "ConnectivityManager.h"
#include "LogManager.h"
#include "Mapper_MiniUPnPc.h"
#include "Mapper_NATPMP.h"
#include "Mapper_WinUPnP.h"
#include "SearchManager.h"
#include "ScopedFunctor.h"

#ifdef STRONG_USE_DHT
#include "../dht/dht.h"
#endif

string MappingManager::g_externalIP;
string MappingManager::g_defaultGatewayIP;

MappingManager::MappingManager() : renewal(0), m_listeners_count(0)
{
	addMapper<Mapper_NATPMP>();
	addMapper<Mapper_MiniUPnPc>();
#ifdef HAVE_NATUPNP_H
	addMapper<Mapper_WinUPnP>();
#endif
}
StringList MappingManager::getMappers() const
{
	StringList ret;
	for (auto i = mappers.cbegin(), iend = mappers.cend(); i != iend; ++i)
		ret.push_back(i->first);
	return ret;
}

bool MappingManager::open()
{
	if (getOpened())
		return false;
		
	if (mappers.empty())
	{
		log(STRING(UPNP_NO_IMPLEMENTATION));
		return false;
	}
	
	if (m_busy.test_and_set())
	{
		log("Another UPnP port mapping attempt is in progress...");
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
	
	if (working.get())
	{
		close(*working);
		working.reset();
	}
}

bool MappingManager::getOpened() const
{
	return working.get() != NULL;
}

string MappingManager::getStatus() const
{
	if (working.get())
	{
		const auto& mapper = *working;
		return str(F_("Successfully created port mappings on the %1% device with the %2% interface") %
		           deviceString(mapper) % mapper.getName());
	}
	return "Failed to create port mappings";
}

int MappingManager::run()
{
	ScopedFunctor([this] { m_busy.clear(); });
	
	// cache these
	const uint16_t conn_port = ConnectionManager::getInstance()->getPort();
	const uint16_t secure_port = ConnectionManager::getInstance()->getSecurePort();
	const uint16_t search_port = SearchManager::getSearchPortUint();
#ifdef STRONG_USE_DHT
	const uint16_t dht_port = dht::DHT::getInstance()->getPort();
#endif
	if (renewal
	        && getOpened()) //[+]FlylinkDC++ Team
	{
		Mapper& mapper = *working;
		
		ScopedFunctor([&mapper] { mapper.uninit(); });
		if (!mapper.init())
		{
			// can't renew; try again later.
			renewal = GET_TICK() + std::max(mapper.renewal(), 10u) * 60 * 1000;
			return 0;
		}
		
		auto addRule = [this, &mapper](const unsigned short port, Mapper::Protocol protocol, const string & description)
		{
			// just launch renewal requests - don't bother with possible failures.
			if (port)
			{
				mapper.open(port, protocol, APPNAME + description + " port (" + Util::toString(port) + ' ' + Mapper::protocols[protocol] + ")");
			}
		};
		
		addRule(conn_port, Mapper::PROTOCOL_TCP, ("Transfer"));
		addRule(secure_port, Mapper::PROTOCOL_TCP, ("Encrypted transfer"));
		addRule(search_port, Mapper::PROTOCOL_UDP, ("Search"));
#ifdef STRONG_USE_DHT
		addRule(dht_port, Mapper::PROTOCOL_UDP, (dht::NetworkName));
#endif
		
		auto minutes = mapper.renewal();
		if (minutes)
		{
			renewal = GET_TICK() + std::max(minutes, 10u) * 60 * 1000;
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
	const auto& setting = SETTING(MAPPER);
	for (auto i = mappers.cbegin(); i != mappers.cend(); ++i)
	{
		if (i->first == setting)
		{
			if (i != mappers.cbegin())
			{
				auto mapper = *i;
				mappers.erase(i);
				mappers.insert(mappers.begin(), mapper);
			}
			break;
		}
	}
	
	for (auto i = mappers.cbegin(); i != mappers.cend(); ++i)
	{
		unique_ptr<Mapper> pMapper(i->second());
		Mapper& mapper = *pMapper;
		
		ScopedFunctor([&mapper] { mapper.uninit(); });
		if (!mapper.init())
		{
			log("Failed to initalize the " + mapper.getName() + " interface");
			continue;
		}
		auto addRule = [this, &mapper](const unsigned short port, Mapper::Protocol protocol, const string & description) -> bool
		{
			bool l_is_ok = true;
			if (port)
			{
				string l_info;
				string l_info_port = description + " port (" + Util::toString(port) + ' ' + Mapper::protocols[protocol] + ')';
				if (!mapper.open(port, protocol, APPNAME + l_info_port))
				{
					l_info = "Failed to map the ";
					mapper.close();
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
				this->log(l_info + l_info_port + " with the " + mapper.getName() + " interface");
			}
			return l_is_ok;
		};
		
		if (!(addRule(conn_port, Mapper::PROTOCOL_TCP, ("Transfer")) &&
		        addRule(secure_port, Mapper::PROTOCOL_TCP, ("Encrypted transfer")) &&
		        addRule(search_port, Mapper::PROTOCOL_UDP, ("Search"))
#ifdef STRONG_USE_DHT
		        && addRule(dht_port, Mapper::PROTOCOL_UDP, (dht::NetworkName))
#endif
		     ))
			continue;
			
		log(STRING(UPNP_SUCCESSFULLY_CREATED_MAPPINGS));
		
		working = move(pMapper); // [IntelC++ 2012 beta2] warning #734: "std::unique_ptr<_Ty, _Dx>::unique_ptr(const std::unique_ptr<_Ty, _Dx>::_Myt &) [with _Ty=Mapper, _Dx=std::default_delete<Mapper>]" (declared at line 2347 of "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include\memory"), required for copy that was eliminated, is inaccessible
		
		g_externalIP = mapper.getExternalIP();
		bool l_is_wifi_router;
		g_defaultGatewayIP = Socket::getDefaultGateWay(l_is_wifi_router);
		if (!BOOLSETTING(NO_IP_OVERRIDE))
		{
			if (!g_externalIP.empty())
			{
				SET_SETTING(EXTERNAL_IP, g_externalIP);
			}
			else
			{
				// no cleanup because the mappings work and hubs will likely provide the correct IP.
				log(STRING(UPNP_FAILED_TO_GET_EXTERNAL_IP));
			}
		}
		
		ConnectivityManager::getInstance()->mappingFinished(mapper.getName());
		
		auto minutes = mapper.renewal();
		if (minutes)
		{
			renewal = GET_TICK() + std::max(minutes, 10u) * 60 * 1000;
			if (m_listeners_count == 0)
			{
				TimerManager::getInstance()->addListener(this);
				++m_listeners_count;
			}
		}
		break;
	}
	
	if (!getOpened())
	{
		log(STRING(UPNP_FAILED_TO_CREATE_MAPPINGS));
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
			ret = mapper.close();
			mapper.uninit();
		}
		if (!ret)
			log(STRING(UPNP_FAILED_TO_REMOVE_MAPPINGS));
	}
}

void MappingManager::log(const string& message)
{
	LogManager::message("Port mapping: " + message);
}

string MappingManager::deviceString(const Mapper& p_mapper) const
{
	//TODO http://code.google.com/p/flylinkdc/issues/detail?id=1241
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
	if (tick >= renewal && !m_busy.test_and_set())
		start(64);
}
