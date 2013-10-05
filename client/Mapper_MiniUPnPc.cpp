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

extern "C" {
#ifndef STATICLIB
#define STATICLIB
#endif
#include "../miniupnpc/miniupnpc.h"
#include "../miniupnpc/upnpcommands.h"
#pragma comment(lib, "iphlpapi.lib")
}

const string Mapper_MiniUPnPc::g_name = "MiniUPnP";

bool Mapper_MiniUPnPc::init()
{
	if (m_initialized)
		return true;
		
	UPNPDev* devices = upnpDiscover(2000,
	                                SettingsManager::getInstance()->isDefault(SettingsManager::BIND_ADDRESS) ? nullptr : SETTING(BIND_ADDRESS).c_str(),
	                                0, 0, 0, 0);
	if (!devices)
		return false;
		
	UPNPUrls urls = {0};
	IGDdatas data = {0};
	
	auto res = UPNP_GetValidIGD(devices, &urls, &data, 0, 0);
	
	m_initialized = res == 1;
	if (m_initialized)
	{
		m_url = urls.controlURL;
		m_service = data.first.servicetype;
		m_device = data.CIF.friendlyName;
		m_modelDescription = data.CIF.modelDescription;
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

bool Mapper_MiniUPnPc::add(const unsigned short port, const Protocol protocol, const string& description)
{
	const string port_ = Util::toString(port);
	return UPNP_AddPortMapping(m_url.c_str(), m_service.c_str(), port_.c_str(), port_.c_str(),
	                           Util::getLocalIp().c_str(), description.c_str(), protocols[protocol], 0, 0) == UPNPCOMMAND_SUCCESS;
}

bool Mapper_MiniUPnPc::remove(const unsigned short port, const Protocol protocol)
{
	return UPNP_DeletePortMapping(m_url.c_str(), m_service.c_str(), Util::toString(port).c_str(),
	                              protocols[protocol], 0) == UPNPCOMMAND_SUCCESS;
}


string Mapper_MiniUPnPc::getExternalIP()
{
	char buf[32] = {0};
	if (UPNP_GetExternalIPAddress(m_url.c_str(), m_service.c_str(), buf) == UPNPCOMMAND_SUCCESS)
		return buf;
	return Util::emptyString;
}
