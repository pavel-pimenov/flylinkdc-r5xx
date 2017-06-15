/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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


#include "stdafx.h"
#include "Resource.h"

#ifdef SSA_WIZARD_FEATURE_UPNP

#include "UPNPCheckDlg.h"
#include "../client/Util.h"
#include "../client/SettingsManager.h"
#include "../XMLParser/xmlParser.h"

#ifdef SSA_WIZARD_FEATURE

///"Mapper_MiniUPnPc.h"
extern "C" {
#ifndef MINIUPNP_STATICLIB
#define MINIUPNP_STATICLIB
#endif
#include "../miniupnpc/miniupnpc.h"
#include "../miniupnpc/upnpcommands.h"
}

#include <natupnp.h>


UPNPCheckDlg::UPNPCheckDlg(uint16_t tcp, uint16_t udp, bool needPortCheck, bool needUPNP, bool useServer)
	: _tcp(tcp)
	, _udp(udp)
	, _result(false)
	, _isResultSetted(false)
	, _stage(Util::emptyString)
	, _needPortCheck(needPortCheck)
	, _resultPort(false)
	, _serverTCP(nullptr)
	, _isTCPOk(false)
	, _isUDPOk(false)
	, _needUPNP(needUPNP)
	, _useServer(useServer)
{
}

UPNPCheckDlg::~UPNPCheckDlg()
{
	if (_needPortCheck)
		StopPortListener();
}

LRESULT UPNPCheckDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (!_needUPNP && !_needPortCheck)
		return 1;
		
	ctrlProgress.Attach(GetDlgItem(IDC_UPNPCHECK_PROGRESS));
	if (!_needUPNP && _needPortCheck)
	{
		ctrlProgress.SetRange(0, 1);
	}
	else if (_needUPNP)
	{
		if (_needPortCheck)
			ctrlProgress.SetRange(0, 5);
		else
			ctrlProgress.SetRange(0, 4);
	}
	ctrlProgress.SetStep(1);
	ctrlProgress.Detach();
	
	if (_needPortCheck)
		StartPortListener(_tcp, _udp);
		
	start(64); // Wizard https://www.box.net/shared/287cdbad1ef13f2393ca
	CenterWindow(GetParent());
	return 0;
}
LRESULT UPNPCheckDlg::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (_isResultSetted)
	{
		bHandled = TRUE;
	}
	return 0;
}


int
UPNPCheckDlg::run()
{

	static const string description = getFlylinkDCAppCaption();
	// description += "Transfer port ";
	
	if (!_needUPNP)
	{
		setStage(STRING(UPNPCHECKDLG_PORT_CHECK));//  "Checkning Ports..."
		success(CheckPorts(_isTCPOk, _isUDPOk));
		return 1;
	}
	try
	{
		setStage(STRING(UPNPCHECKDLG_MINI_INIT));//  "Checkning MiniUPNP..."
		string url;
		string service;
		if (MiniUPnPc_init(url, service, m_device))
		{
			setStage(STRING(UPNPCHECKDLG_MINI_OPEN));// "Open ports in MiniUPNP..."
			// Open Ports
			bool isSuccess = false;
			bool portTestResult = false;
			string tcpDesc = description + "Transfer port (" + Util::toString(_tcp) + " TCP)";
			if (MiniUPnPc_add(_tcp, "TCP", tcpDesc, service, url))
			{
				string udpDesc = description + "Search port (" + Util::toString(_udp) + " UDP)";
				if (MiniUPnPc_add(_udp, "UDP", udpDesc, service, url))
				{
					if (_needPortCheck)
					{
						_isTCPOk = false;
						_isUDPOk = false;
						portTestResult = CheckPorts(_isTCPOk, _isUDPOk);
					}
					isSuccess = true;
					MiniUPnPc_remove(_udp, "UDP", service, url);
				}
				MiniUPnPc_remove(_tcp, "TCP", service, url);
				if (isSuccess)
				{
					success(portTestResult);
					return 1;
				}
			}
		}
	}
	catch (const Exception& /*e*/) {}
	
	try
	{
		setStage(STRING(UPNPCHECKDLG_WIN_INIT)); //"Checkning WinUPNP..."
		IUPnPNAT* pUN = NULL;
		bool isSuccess = false;
		bool portTestResult = false;
		if (WinUPnP_init(pUN))
		{
			setStage(STRING(UPNPCHECKDLG_WIN_OPEN));// "Open ports in WinUPNP..."
			if (WinUPnP_add(_tcp, "TCP", description, pUN))
			{
				if (WinUPnP_add(_udp, "UDP", description, pUN))
				{
					if (_needPortCheck)
					{
						_isTCPOk = false;
						_isUDPOk = false;
						portTestResult = CheckPorts(_isTCPOk, _isUDPOk);
					}
					isSuccess = true;
					WinUPnP_remove(_udp, "UDP", pUN);
				}
				WinUPnP_remove(_tcp, "TCP", pUN);
			}
			WinUPnP_uninit();
			if (isSuccess)
			{
				success(false);
				return 1;
			}
		}
	}
	catch (const Exception& /*e*/) {}
	
	if (_needPortCheck)
	{
		setStage(STRING(UPNPCHECKDLG_PORT_CHECK));//  "Checkning Ports..."
		CheckPorts(_isTCPOk, _isUDPOk);
	}
	
	failed();
	return 0;
}

void UPNPCheckDlg::setStage(const string& newStage)
{
	_stage = newStage; // [+] IRainman fix
	SendMessage(WM_SETSTAGE, (WPARAM)NULL, NULL);
}
void UPNPCheckDlg::success(bool portSuccess)
{
	PostMessage(WM_SETOK, (WPARAM)portSuccess, NULL);
}
void UPNPCheckDlg::failed()
{
	PostMessage(WM_SETFAIL, (WPARAM)NULL, NULL);
}



LRESULT UPNPCheckDlg::OnSetStage(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetDlgItemText(IDC_UPNPCHECK_STATIC, Text::toT(_stage).c_str());
	
	ctrlProgress.Attach(GetDlgItem(IDC_UPNPCHECK_PROGRESS));
	ctrlProgress.StepIt();
	ctrlProgress.Detach();
	UpdateWindow();
	
	return NULL;
}
LRESULT UPNPCheckDlg::OnSetOK(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	_isResultSetted = true;
	_result = true;
	_resultPort = (wParam == 1) ? true : false;
	EndDialog(IDOK);
	return NULL;
}
LRESULT UPNPCheckDlg::OnSetFAIL(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	_isResultSetted = true;
	_result = false;
	_resultPort = false;
	EndDialog(IDCANCEL);
	return NULL;
}


// MiniUPNP
bool UPNPCheckDlg::MiniUPnPc_init(string& url, string& service, string& device)
{
	bool initialized  = false;
	int l_error = UPNPDISCOVER_SUCCESS;
	UPNPDev* devices = upnpDiscover(2000,
	                                SettingsManager::getInstance()->isDefault(SettingsManager::BIND_ADDRESS) ? nullptr : SETTING(BIND_ADDRESS).c_str(),
	                                nullptr, UPNP_LOCAL_PORT_ANY, false, 2, &l_error);
	if (!devices)
		return false;
		
	UPNPUrls urls = {0};
	IGDdatas data = {0};
	
	auto res = UPNP_GetValidIGD(devices, &urls, &data, 0, 0);
	
	initialized = res == 1;
	if (initialized)
	{
		url = urls.controlURL;
		service = data.first.servicetype;
		device = data.CIF.friendlyName;
		device += '(' + data.CIF.modelDescription + ')';
	}
	
	if (res)
	{
		FreeUPNPUrls(&urls);
		freeUPNPDevlist(devices);
	}
	return initialized;
}

bool UPNPCheckDlg::MiniUPnPc_add(const unsigned short port, const string& protocol, const string& description, const string& service, const string& url)
{
	const string port_ = Util::toString(port);
	return UPNP_AddPortMapping(url.c_str(), service.c_str(), port_.c_str(), port_.c_str(),
	                           Util::getLocalOrBindIp(false).c_str(), description.c_str(), protocol.c_str(), "", 0) == UPNPCOMMAND_SUCCESS;
}

bool UPNPCheckDlg::MiniUPnPc_remove(const unsigned short port, const string& protocol, const string& service, const string& url)
{
	return UPNP_DeletePortMapping(url.c_str(), service.c_str(), Util::toString(port).c_str(),
	                              protocol.c_str(), 0) == UPNPCOMMAND_SUCCESS;
}


bool UPNPCheckDlg::WinUPnP_init(IUPnPNAT*& pUN)
{
#ifdef HAVE_NATUPNP_H
	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	ATLASSERT(SUCCEEDED(hRes)); // [+] IRainman
	
	// Lacking the __uuidof in mingw...
	CLSID upnp;
	OLECHAR upnps[] = L"{AE1E00AA-3FD5-403C-8A27-2BBDC30CD0E1}";
	CLSIDFromString(upnps, &upnp);
	IID iupnp;
	OLECHAR iupnps[] = L"{B171C812-CC76-485A-94D8-B6B3A2794E99}";
	CLSIDFromString(iupnps, &iupnp);
	pUN = 0;
	HRESULT hr = CoCreateInstance(upnp, 0, CLSCTX_INPROC_SERVER, iupnp, reinterpret_cast<LPVOID*>(&pUN));
	if (FAILED(hr))
		pUN = 0;
#endif
	return pUN != 0;
}

void UPNPCheckDlg::WinUPnP_uninit()
{
#ifdef HAVE_NATUPNP_H
	CoUninitialize();
#endif
}


bool UPNPCheckDlg::WinUPnP_add(const unsigned short port, const string& protocol, const string& description, IUPnPNAT* pUN)
{
#ifdef HAVE_NATUPNP_H
	IStaticPortMappingCollection* pSPMC = WinUPnP_getStaticPortMappingCollection(pUN);
	if (!pSPMC)
		return false;
		
	/// @todo use a BSTR wrapper
	BSTR protocol_ = SysAllocString(Text::toT(protocol).c_str());
	BSTR description_ = SysAllocString(Text::toT(description).c_str());
	BSTR localIP = SysAllocString(Text::toT(Util::getLocalOrBindIp(false)).c_str());
	
	IStaticPortMapping* pSPM = 0;
	HRESULT hr = pSPMC->Add(port, protocol_, port, localIP, VARIANT_TRUE, description_, &pSPM);
	
	SysFreeString(protocol_);
	SysFreeString(description_);
	SysFreeString(localIP);
	
	bool ret = SUCCEEDED(hr);
	if (ret)
	{
		safe_release(pSPM);
	}
	safe_release(pSPMC);
	return ret;
#else
	return false;
#endif
}

bool UPNPCheckDlg::WinUPnP_remove(const unsigned short port, const string& protocol, IUPnPNAT* pUN)
{
#ifdef HAVE_NATUPNP_H
	IStaticPortMappingCollection* pSPMC = WinUPnP_getStaticPortMappingCollection(pUN);
	if (!pSPMC)
		return false;
		
	/// @todo use a BSTR wrapper
	BSTR protocol_ = SysAllocString(Text::toT(protocol).c_str());
	
	HRESULT hr = pSPMC->Remove(port, protocol_);
	safe_release(pSPMC);
	SysFreeString(protocol_);
	
	return SUCCEEDED(hr);
#else
	return false;
#endif
}

IStaticPortMappingCollection*
UPNPCheckDlg::WinUPnP_getStaticPortMappingCollection(IUPnPNAT* pUN)
{
#ifdef HAVE_NATUPNP_H
	if (!pUN)
		return 0;
	IStaticPortMappingCollection* ret = 0;
	HRESULT hr = 0;
	
	// some routers lag here
	for (int i = 0; i < 3; i++)
	{
		hr = pUN->get_StaticPortMappingCollection(&ret);
		
		if (SUCCEEDED(hr) && ret) break;
		
		Sleep(1500);
	}
	
	if (FAILED(hr))
		return 0;
	return ret;
#else
	return NULL;
#endif
}

bool UPNPCheckDlg::CheckPorts(bool& isTCPOk, bool& isUDPOk)
{
	return true;
}

string UPNPCheckDlg::MiniUPnPc_getExternalIP(string& url, string&  service)
{
	char buf[16] = { 0 };
	if (UPNP_GetExternalIPAddress(url.c_str(), service.c_str(), buf) == UPNPCOMMAND_SUCCESS)
		return buf;
	return Util::emptyString;
}
bool UPNPCheckDlg::StartPortListener(uint16_t tcp, uint16_t udp)
{
	if (!_useServer)
		return false;
	try
	{
		_serverTCP = new ConnectionManager::Server(false, tcp, SETTING(BIND_ADDRESS)); // [2] https://www.box.net/shared/b1e438ac1b85276d8603
	}
	catch (Exception & e)
	{
		::MessageBox(NULL, Text::toT(e.getError()).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
		return false;
	}
	return true;
}

bool UPNPCheckDlg::StopPortListener()
{
	safe_delete(_serverTCP);
	return true;
}

#endif // SSA_WIZARD_FEATURE

#endif // SSA_WIZARD_FEATURE_UPNP
