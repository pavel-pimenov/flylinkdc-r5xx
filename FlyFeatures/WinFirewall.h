/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.blogspot.com/
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

#ifndef _WIN_FIREWALL_H_
#define _WIN_FIREWALL_H_

#pragma once

#include <Netfw.h>

class WinFirewall
{
	public:
	
		static BOOL IsWindowsFirewallAppEnabled(const TCHAR* in_pAuthorizedFilename);
		static void WindowFirewallSetAppAuthorization(const TCHAR* in_pAuthorizedDisplayname, const TCHAR* in_pAuthorizedFilename, bool bEnableService = true);
		static BOOL IsWindowsFirewallIsOn();
		static BOOL IsWindowsFirewallPortEnabled(long in_lPort, NET_FW_IP_PROTOCOL in_protocol);
#ifdef FLYLINKDC_USE_DEAD_CODE
		static void TurnWindowsFirewall(bool bTurnOn = true);
		static void WindowsFirewallPortAdd(TCHAR* in_sportName, long in_portNumber, bool bEnable = true, NET_FW_IP_PROTOCOL in_protocol = NET_FW_IP_PROTOCOL_TCP);
#endif
		
	private:
		static HRESULT GetFwProfile(INetFwProfile** out_fwProfile);
};




#endif // _WIN_FIREWALL_H_