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
#include "Mapper_MiniUPnPc.h"
#include "SettingsManager.h"
#include "LogManager.h"

extern "C" {
#ifndef MINIUPNP_STATICLIB
#define MINIUPNP_STATICLIB
#endif
#include "../miniupnpc/miniupnpc.h"
#include "../miniupnpc/upnpcommands.h"
#include "../miniupnpc/upnperrors.h"
}

#pragma comment(lib, "iphlpapi.lib")

const string Mapper_MiniUPnPc::g_name = "MiniUPnP";

bool Mapper_MiniUPnPc::init()
{
	CFlyLog l_log("[MiniUPnP]");
	if (m_initialized)
	{
		l_log.step("m_initialized == true!");
		return true;
	}
	l_log.step("BIND_ADDRESS = " + SETTING(BIND_ADDRESS));
	UPNPDev* devices = upnpDiscover(2000,
	                                SettingsManager::getInstance()->isDefault(SettingsManager::BIND_ADDRESS) ? nullptr : SETTING(BIND_ADDRESS).c_str(),
	                                0, 0, 0, 0);
	if (!devices)
	{
		l_log.step("upnpDiscover == null");
		return false;
	}
	
	UPNPUrls urls = {0};
	IGDdatas data = {0};
	
	auto res = UPNP_GetValidIGD(devices, &urls, &data, 0, 0);
	
	m_initialized = res == 1;
	if (m_initialized)
	{
		l_log.step("UPNP_GetValidIGD OK!");
		m_url = urls.controlURL;
		m_service = data.first.servicetype;
		m_device = data.CIF.friendlyName;
		m_modelDescription = data.CIF.modelDescription;
		l_log.step("urls.controlURL = " + m_url);
		l_log.step("data.first.servicetype = " + m_service);
		l_log.step("data.CIF.friendlyName = " + m_device);
		l_log.step("data.CIF.modelDescription = " + m_modelDescription);
	}
	else
	{
		l_log.step("Error UPNP_GetValidIGD code = " + Util::toString(res));
	}
	
	if (res)
	{
		FreeUPNPUrls(&urls);
		freeUPNPDevlist(devices);
	}
	
	return m_initialized;
}

void Mapper_MiniUPnPc::uninit()
{
}

void Mapper_MiniUPnPc::log_error(const int p_error_code, const string& p_name)
{
	string l_error = strupnperror(p_error_code);
	l_error += " Code = " + Util::toString(p_error_code);
	LogManager::getInstance()->message("[miniupnpc] " + p_name + " error = " + l_error);
}

bool Mapper_MiniUPnPc::add(const unsigned short port, const Protocol protocol, const string& description)
{
	const string l_port = Util::toString(port);
	const string l_bind_addres = Util::getLocalOrBindIp(true);
	const auto l_upnp_res = UPNP_AddPortMapping(m_url.c_str(), m_service.c_str(), l_port.c_str(), l_port.c_str(),
	                                            l_bind_addres.c_str(), // http://code.google.com/p/flylinkdc/issues/detail?id=1359
	                                            description.c_str(), protocols[protocol], 0, 0);
	if (l_upnp_res == UPNPCOMMAND_SUCCESS)
	{
		LogManager::getInstance()->message("[MiniUPnPc::add] OK bind_addres = " + l_bind_addres);
		return true;
	}
	else
	{
		log_error(l_upnp_res, "UPNP_AddPortMapping, Error bind_addres = " + l_bind_addres);
	}
	return false;
}

bool Mapper_MiniUPnPc::remove(const unsigned short port, const Protocol protocol)
{
	const string l_port = Util::toString(port);
	const auto l_upnp_res = UPNP_DeletePortMapping(m_url.c_str(), m_service.c_str(), l_port.c_str(),
	                                               protocols[protocol], 0);
	if (l_upnp_res == UPNPCOMMAND_SUCCESS)
	{
		LogManager::getInstance()->message("[MiniUPnPc::remove] OK port = " + l_port);
		return true;
	}
	else
	{
		log_error(l_upnp_res, "UPNP_DeletePortMapping");
	}
	return false;
}


string Mapper_MiniUPnPc::getExternalIP()
{
	char buf[32] = {0};
	const auto l_upnp_res = UPNP_GetExternalIPAddress(m_url.c_str(), m_service.c_str(), buf);
	if (l_upnp_res == UPNPCOMMAND_SUCCESS)
	{
		LogManager::getInstance()->message("[MiniUPnPc::getExternalIP] IP= " + string(buf));
		return buf;
	}
	else
	{
		log_error(l_upnp_res, "UPNP_GetExternalIPAddress");
	}
	return Util::emptyString;
}
