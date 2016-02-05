/*
 * Copyright (C) 2011-2015 FlylinkDC++ Team http://flylinkdc.com/
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
#include "WinFirewall.h"
#include <comutil.h>
#include <comdef.h>

// TODO - заменить на chromium\home\src_tarball\tarball\chromium\src\third_party\libjingle\source\talk\base
// winfirewall.cc
// winfirewall.h
BOOL WinFirewall::IsWindowsFirewallAppEnabled(const TCHAR* in_pAuthorizedFilename)
{
	BOOL bRet = FALSE;
	if (!in_pAuthorizedFilename) throw E_INVALIDARG;
	try
	{
		INetFwProfile* pFwProfilePtr;
		INetFwAuthorizedApplications* pFwAuthorizedApplicationsPtr;
		INetFwAuthorizedApplication* pFwAuthorizedApplicationPtr;
		HRESULT hr = GetFwProfile(&pFwProfilePtr);
		if (FAILED(hr)) throw hr;
		hr = pFwProfilePtr->get_AuthorizedApplications(&pFwAuthorizedApplicationsPtr);
		if (FAILED(hr)) throw E_UNEXPECTED;
		hr = pFwAuthorizedApplicationsPtr->Item(_bstr_t(in_pAuthorizedFilename), &pFwAuthorizedApplicationPtr);
		if (SUCCEEDED(hr))
		{
			VARIANT_BOOL fwEnabled;
			hr = pFwAuthorizedApplicationPtr->get_Enabled(&fwEnabled);
			if (FAILED(hr)) throw E_UNEXPECTED;
			bRet = (fwEnabled != VARIANT_FALSE) ? TRUE : FALSE;
		}
		else
		{
			// No such Application (Disabled)
		}
	}
	catch (const HRESULT& getHResult)
	{
		switch (getHResult)
		{
			case E_NOINTERFACE:  // No such Interface in system
				break;
			default:
				throw;
		}
	}
	return bRet;
}

void WinFirewall::WindowFirewallSetAppAuthorization(const TCHAR* in_pAuthorizedDisplayname, const TCHAR* in_pAuthorizedFilename, bool bEnableService /* = true */)
{
	if (!in_pAuthorizedDisplayname) throw E_INVALIDARG;
	if (!in_pAuthorizedFilename) throw E_INVALIDARG;
	try
	{
		BOOL bAppStatus = IsWindowsFirewallAppEnabled(in_pAuthorizedFilename);
		if (bAppStatus && bEnableService) // Already enabled
			return;
		if (!bAppStatus && !bEnableService) // Already disabled
			return;
			
		INetFwProfile* pFwProfilePtr;
		INetFwAuthorizedApplications* pFwAuthorizedApplicationsPtr;
		INetFwAuthorizedApplication* pFwAuthorizedApplicationPtr;
		HRESULT hr = GetFwProfile(&pFwProfilePtr);
		if (FAILED(hr)) throw hr;
		hr = pFwProfilePtr->get_AuthorizedApplications(&pFwAuthorizedApplicationsPtr);
		if (FAILED(hr)) throw E_UNEXPECTED;
		//CLSID clsID;
		//hr = CLSIDFromProgID(OLESTR("HNetCfg.FwAuthorizedApplication"),&clsID);
		//if(FAILED(hr)) throw E_UNEXPECTED;
		//hr = pFwAuthorizedApplicationPtr.CreateInstance(clsID);
		hr = CoCreateInstance(
		         __uuidof(NetFwAuthorizedApplication),
		         NULL,
		         CLSCTX_INPROC_SERVER,
		         __uuidof(INetFwAuthorizedApplication),
		         reinterpret_cast<void**>(static_cast<INetFwAuthorizedApplication**>(&pFwAuthorizedApplicationPtr)));
		if (FAILED(hr)) throw E_UNEXPECTED;
		pFwAuthorizedApplicationPtr->put_Name(_bstr_t(in_pAuthorizedDisplayname));
		pFwAuthorizedApplicationPtr->put_ProcessImageFileName(_bstr_t(in_pAuthorizedFilename));
		if (bEnableService)
			pFwAuthorizedApplicationPtr->put_Enabled(VARIANT_TRUE);
		else
			pFwAuthorizedApplicationPtr->put_Enabled(VARIANT_FALSE);
			
		hr = pFwAuthorizedApplicationsPtr->Add(pFwAuthorizedApplicationPtr);
		if (FAILED(hr)) throw E_UNEXPECTED;
	}
	catch (const HRESULT& getHResult)
	{
		switch (getHResult)
		{
			case E_NOINTERFACE:  // No such Interface in system
				break;
			default:
				throw;
		}
	}
}

BOOL WinFirewall::IsWindowsFirewallIsOn()
{
	HRESULT hr = S_OK;
	BOOL bRet = FALSE;
	VARIANT_BOOL fwEnabled;
	try
	{
		INetFwProfile* pFwProfilePtr;
		hr = GetFwProfile(&pFwProfilePtr);
		if (FAILED(hr)) throw hr;
		hr = pFwProfilePtr->get_FirewallEnabled(&fwEnabled);
		if (FAILED(hr)) throw E_UNEXPECTED;
		if (fwEnabled != VARIANT_FALSE)
			bRet = TRUE;
	}
	catch (const HRESULT& getHResult)
	{
		switch (getHResult)
		{
			case E_NOINTERFACE:  // No such Interface in system
				break;
			default:
				throw;
		}
	}
	return bRet;
}


BOOL WinFirewall::IsWindowsFirewallPortEnabled(long in_lPort, NET_FW_IP_PROTOCOL in_protocol)
{
	BOOL bRet = FALSE;
	try
	{
		INetFwProfile* pFwProfilePtr;
		HRESULT hr = GetFwProfile(&pFwProfilePtr);
		if (FAILED(hr)) throw hr;
		INetFwOpenPort* fwOpenPort;
		INetFwOpenPorts* fwOpenPorts;
		pFwProfilePtr->get_GloballyOpenPorts(&fwOpenPorts);
		if (FAILED(hr)) throw hr;
		hr = fwOpenPorts->Item(in_lPort, in_protocol, &fwOpenPort);
		if (SUCCEEDED(hr))
		{
			VARIANT_BOOL fwEnabled;
			hr = fwOpenPort->get_Enabled(&fwEnabled);
			if (FAILED(hr)) throw hr;
			bRet = (fwEnabled != VARIANT_FALSE) ? TRUE : FALSE;
		}
		else
		{
			// No port in Firewall, so it disabled.
		}
	}
	catch (const HRESULT& getHResult)
	{
		switch (getHResult)
		{
			case E_NOINTERFACE:  // No such Interface in system
				break;
			default:
				throw;
		}
	}
	return bRet;
}

#ifdef PPA_INCLUDE_DEAD_CODE
void WinFirewall::TurnWindowsFirewall(bool bTurnOn /*= true*/)
{
	try
	{
		BOOL bFireWallStatus = IsWindowsFirewallIsOn();
		if (bTurnOn && bFireWallStatus) // Already Turn on
			return;
		if (!bTurnOn && !bFireWallStatus) // Alreadt turn off
			return;
		INetFwProfile* pFwProfilePtr;
		HRESULT hr = GetFwProfile(&pFwProfilePtr);
		if (FAILED(hr)) throw hr;
		pFwProfilePtr->put_FirewallEnabled(bTurnOn ? VARIANT_TRUE : VARIANT_FALSE);
	}
	catch (const HRESULT& getHResult)
	{
		switch (getHResult)
		{
			case E_NOINTERFACE:  // No such Interface in system
				break;
			default:
				throw;
		}
	}
}

void WinFirewall::WindowsFirewallPortAdd(TCHAR* in_sportName, long in_portNumber, bool bEnable /*= true*/, NET_FW_IP_PROTOCOL in_protocol /*= NetFwTypeLib::NET_FW_IP_PROTOCOL_TCP*/)
{
	if (!in_sportName) throw E_INVALIDARG;
	if (!in_portNumber) throw E_INVALIDARG;
	try
	{
		BOOL bIsPortEnable = IsWindowsFirewallPortEnabled(in_portNumber, in_protocol);
		if (bEnable && bIsPortEnable)
			return;
		if (!bEnable && !bIsPortEnable)
			return;
		INetFwProfile* pFwProfilePtr;
		HRESULT hr = GetFwProfile(&pFwProfilePtr);
		if (FAILED(hr)) throw hr;
		INetFwOpenPort* fwOpenPort;
		INetFwOpenPorts* fwOpenPorts;
		hr = pFwProfilePtr->get_GloballyOpenPorts(&fwOpenPorts);
		if (FAILED(hr)) throw E_UNEXPECTED;
		//CLSID clsID;
		//hr = CLSIDFromProgID(OLESTR("HNetCfg.FwOpenPort"),&clsID);
		//if(FAILED(hr)) throw E_UNEXPECTED;
		//hr = fwOpenPort.CreateInstance(clsID);
		hr = CoCreateInstance(
		         __uuidof(NetFwOpenPort),
		         NULL,
		         CLSCTX_INPROC_SERVER,
		         __uuidof(INetFwOpenPort),
		         reinterpret_cast<void**>(static_cast<INetFwOpenPort**>(&fwOpenPort)));
		if (FAILED(hr)) throw E_UNEXPECTED;
		fwOpenPort->put_Port(in_portNumber);
		fwOpenPort->put_Protocol(in_protocol);
		fwOpenPort->put_Name(_bstr_t(in_sportName));
		fwOpenPort->put_Enabled(bEnable);
		hr = fwOpenPorts->Add(fwOpenPort);
		if (FAILED(hr)) throw E_UNEXPECTED;
	}
	catch (const HRESULT& getHResult)
	{
		switch (getHResult)
		{
			case E_NOINTERFACE:  // No such Interface in system
				break;
			default:
				throw;
		}
	}
}
#endif

HRESULT WinFirewall::GetFwProfile(INetFwProfile** out_fwProfile)
{
	HRESULT hRet = S_OK;
	try
	{
		INetFwMgr* pFwMgrPtr;
		INetFwPolicy* pFwPolicyPtr;
		// CLSID clsID;
		//HRESULT hr= CLSIDFromProgID(OLESTR("HNetCfg.FwMgr"), &clsID);
		//if(FAILED(hr)) throw E_NOINTERFACE;
		// hr = pFwMgrPtr.CreateInstance(clsID);
		HRESULT hr = CoCreateInstance(
		                 __uuidof(NetFwMgr),
		                 NULL,
		                 CLSCTX_INPROC_SERVER,
		                 __uuidof(INetFwMgr),
		                 reinterpret_cast<void**>(static_cast<INetFwMgr**>(&pFwMgrPtr))
		             );
		if (FAILED(hr)) throw hr;
		hr = pFwMgrPtr->get_LocalPolicy(&pFwPolicyPtr);
		if (FAILED(hr)) throw E_UNEXPECTED;
		hr = pFwPolicyPtr->get_CurrentProfile(out_fwProfile);
		if (FAILED(hr)) throw E_UNEXPECTED;
	}
	catch (const HRESULT& getHResult)
	{
		hRet = getHResult;
	}
	catch (...)
	{
		hRet = E_UNEXPECTED;
	}
	return hRet;
}
