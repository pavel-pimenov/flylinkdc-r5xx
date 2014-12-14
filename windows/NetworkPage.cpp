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

#include "stdafx.h"

#include <comdef.h>
#include "Resource.h"

#include "NetworkPage.h"
#include "WinUtil.h"
#include "../FlyFeatures/flyServer.h"
#include "../client/CryptoManager.h"

//#define FLYLINKDC_USE_SSA_WINFIREWALL

#ifdef FLYLINKDC_USE_SSA_WINFIREWALL
#include "../FlyFeatures/WinFirewall.h"
#else
#include "../client/webrtc/talk/base/winfirewall.h"
#endif

PropPage::TextItem NetworkPage::texts[] =
{
	{ IDC_CONNECTION_DETECTION, ResourceManager::CONNECTION_DETECTION },
	{ IDC_DIRECT, ResourceManager::SETTINGS_DIRECT },
	{ IDC_FIREWALL_UPNP, ResourceManager::SETTINGS_FIREWALL_UPNP },
	{ IDC_FIREWALL_NAT, ResourceManager::SETTINGS_FIREWALL_NAT },
	{ IDC_FIREWALL_PASSIVE, ResourceManager::SETTINGS_FIREWALL_PASSIVE },
#ifdef RIP_USE_CONNECTION_AUTODETECT
	{ IDC_AUTODETECT, ResourceManager::SETTINGS_CONNECTION_AUTODETECT },
#endif
	{ IDC_OVERRIDE, ResourceManager::SETTINGS_OVERRIDE },
	{ IDC_SETTINGS_PORTS, ResourceManager::SETTINGS_PORTS },
	{ IDC_SETTINGS_IP, ResourceManager::SETTINGS_EXTERNAL_IP },
	{ IDC_SETTINGS_PORT_TCP, ResourceManager::SETTINGS_TCP_PORT },
	{ IDC_SETTINGS_PORT_UDP, ResourceManager::SETTINGS_UDP_PORT },
	{ IDC_SETTINGS_PORT_TLS, ResourceManager::SETTINGS_TLS_PORT },
	{ IDC_SETTINGS_PORT_DHT, ResourceManager::SETTINGS_DHT_PORT },
	{ IDC_SETTINGS_INCOMING, ResourceManager::SETTINGS_INCOMING },
	{ IDC_SETTINGS_BIND_ADDRESS, ResourceManager::SETTINGS_BIND_ADDRESS },
	{ IDC_SETTINGS_BIND_ADDRESS_HELP, ResourceManager::SETTINGS_BIND_ADDRESS_HELP },
	{ IDC_NATT, ResourceManager::ALLOW_NAT_TRAVERSAL },
	{ IDC_IPUPDATE, ResourceManager::UPDATE_IP },
	{ IDC_SETTINGS_UPDATE_IP_INTERVAL, ResourceManager::UPDATE_IP_INTERVAL },
#ifdef STRONG_USE_DHT
	{ IDC_UPDATE_IP_DHT, ResourceManager::SETTINGS_UPDATE_IP },
	{ IDC_SETTINGS_USE_DHT,  ResourceManager::USE_DHT },
	{ IDC_SETTINGS_USE_DHT_NOTANSWER,  ResourceManager::USE_DHT_NOTANSWER },
#endif
	{ IDC_GETIP, ResourceManager::GET_IP },
	{ IDC_ADD_FLYLINKDC_WINFIREWALL, ResourceManager::ADD_FLYLINKDC_WINFIREWALL },
	{ IDC_STATIC_GATEWAY, ResourceManager::SETTINGS_GATEWAY },
	
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item NetworkPage::items[] =
{
	{ IDC_CONNECTION_DETECTION, SettingsManager::AUTO_DETECT_CONNECTION, PropPage::T_BOOL },
	{ IDC_EXTERNAL_IP,      SettingsManager::EXTERNAL_IP,   PropPage::T_STR },
	{ IDC_PORT_TCP,         SettingsManager::TCP_PORT,      PropPage::T_INT },
	{ IDC_PORT_UDP,         SettingsManager::UDP_PORT,      PropPage::T_INT },
	{ IDC_PORT_TLS,         SettingsManager::TLS_PORT,      PropPage::T_INT },
	{ IDC_OVERRIDE,         SettingsManager::NO_IP_OVERRIDE, PropPage::T_BOOL },
	{ IDC_IP_GET_IP,        SettingsManager::URL_GET_IP,    PropPage::T_STR }, //[+]PPA
	{ IDC_IPUPDATE,         SettingsManager::IPUPDATE,      PropPage::T_BOOL },
	{ IDC_UPDATE_IP_INTERVAL, SettingsManager::IPUPDATE_INTERVAL, PropPage::T_INT },
	{ IDC_BIND_ADDRESS,     SettingsManager::BIND_ADDRESS, PropPage::T_STR },
	{ IDC_NATT,             SettingsManager::ALLOW_NAT_TRAVERSAL, PropPage::T_BOOL },
#ifdef STRONG_USE_DHT
	{ IDC_PORT_DHT,         SettingsManager::DHT_PORT,      PropPage::T_INT },
	{ IDC_UPDATE_IP_DHT,    SettingsManager::UPDATE_IP_DHT,     PropPage::T_BOOL },
	{ IDC_SETTINGS_USE_DHT, SettingsManager::USE_DHT, PropPage::T_BOOL },
	{ IDC_SETTINGS_USE_DHT_NOTANSWER, SettingsManager::USE_DHT_NOTANSWER, PropPage::T_BOOL },
#endif
	{ 0, 0, PropPage::T_END }
};

LRESULT NetworkPage::OnCtlColorDlg(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	/*
	if ( (HWND)lParam == GetDlgItem( IDC_DEFAULT_GATEWAY_IP ) )
	 {
	     HDC hdcStatic = (HDC) wParam;
	     ::SetTextColor( hdcStatic, RGB( 0, 0, 255) );
	     //::SetBkMode ( hdcStatic, TRANSPARENT );
	     //::SelectObject( hdcStatic, ::GetStockObject( NULL_BRUSH) );
	     return (HRESULT)::GetCurrentObject( hdcStatic, OBJ_BRUSH);
	}
	*/
	return 0;
}

LRESULT NetworkPage::OnEnKillfocusExternalIp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring l_externalIPW;
	GET_TEXT(IDC_EXTERNAL_IP, l_externalIPW);
	const auto l_externalIP = Text::fromT(l_externalIPW);
	if (!l_externalIP.empty())
	{
		boost::system::error_code ec;
		const auto l_ip = boost::asio::ip::address_v4::from_string(l_externalIP, ec);
		// TODO - убрать count и попробовать без буста - http://stackoverflow.com/questions/318236/how-do-you-validate-that-a-string-is-a-valid-ip-address-in-c?
		if (ec || std::count(l_externalIP.cbegin(), l_externalIP.cend(), '.') != 3)
		{
			const auto l_last_ip = SETTING(EXTERNAL_IP);
			::MessageBox(NULL, Text::toT("Error IP = " + l_externalIP + ", restore last valid IP = " + l_last_ip).c_str(), _T("IP Error!"), MB_OK | MB_ICONERROR);
			::SetWindowText(GetDlgItem(IDC_EXTERNAL_IP), Text::toT(SETTING(EXTERNAL_IP)).c_str());
		}
	}
	return 0;
}
void NetworkPage::write()
{
	PropPage::write((HWND)(*this), items);
	
	// Set connection active/passive
	int ct = SettingsManager::INCOMING_DIRECT;
	
	if (IsDlgButtonChecked(IDC_FIREWALL_UPNP))
		ct = SettingsManager::INCOMING_FIREWALL_UPNP;
	else if (IsDlgButtonChecked(IDC_FIREWALL_NAT))
		ct = SettingsManager::INCOMING_FIREWALL_NAT;
	else if (IsDlgButtonChecked(IDC_FIREWALL_PASSIVE))
		ct = SettingsManager::INCOMING_FIREWALL_PASSIVE;
		
	const auto l_current_set = SETTING(INCOMING_CONNECTIONS);
	if (l_current_set != ct)
	{
		settings->set(SettingsManager::INCOMING_CONNECTIONS, ct);
	}
	
#ifdef RIP_USE_CONNECTION_AUTODETECT
	settings->set(SettingsManager::INCOMING_AUTODETECT_FLAG, int(IsDlgButtonChecked(IDC_AUTODETECT)));
#endif
}

LRESULT NetworkPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	
#ifndef IRAINMAN_IP_AUTOUPDATE
	//::EnableWindow(GetDlgItem(IDC_GETIP), FALSE);
	::EnableWindow(GetDlgItem(IDC_IPUPDATE), FALSE);
#endif
	
	switch (SETTING(INCOMING_CONNECTIONS))
	{
		case SettingsManager::INCOMING_DIRECT:
			CheckDlgButton(IDC_DIRECT, BST_CHECKED);
			break;
		case SettingsManager::INCOMING_FIREWALL_UPNP:
			CheckDlgButton(IDC_FIREWALL_UPNP, BST_CHECKED);
			break;
		case SettingsManager::INCOMING_FIREWALL_NAT:
			CheckDlgButton(IDC_FIREWALL_NAT, BST_CHECKED);
			break;
		case SettingsManager::INCOMING_FIREWALL_PASSIVE:
			CheckDlgButton(IDC_FIREWALL_PASSIVE, BST_CHECKED);
			break;
		default:
			CheckDlgButton(IDC_DIRECT, BST_CHECKED);
			break;
	}
	
#ifdef RIP_USE_CONNECTION_AUTODETECT
	CheckDlgButton(IDC_AUTODETECT, SETTING(INCOMING_AUTODETECT_FLAG));
#else
	::EnableWindow(GetDlgItem(IDC_AUTODETECT), FALSE);
#endif
	
	PropPage::read((HWND)(*this), items);
	
	fixControls();
	m_IPHint.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	m_IPHint.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(m_IPHint.IsWindow());
	
	m_desc.Attach(GetDlgItem(IDC_PORT_TCP));
	m_desc.LimitText(5);
	m_desc.Detach();
	m_desc.Attach(GetDlgItem(IDC_PORT_UDP));
	m_desc.LimitText(5);
	m_desc.Detach();
	m_desc.Attach(GetDlgItem(IDC_PORT_TLS));
	m_desc.LimitText(5);
	m_desc.Detach();
#ifdef STRONG_USE_DHT
	m_desc.Attach(GetDlgItem(IDC_PORT_DHT));
	m_desc.LimitText(5);
	m_desc.Detach();
#endif
	m_BindCombo.Attach(GetDlgItem(IDC_BIND_ADDRESS));
	const auto l_tool_tip = WinUtil::getAddresses(m_BindCombo);
	m_IPHint.SetMaxTipWidth(1024);
	m_IPHint.AddTool(m_BindCombo, l_tool_tip.c_str());
	const auto l_bind = Text::toT(SETTING(BIND_ADDRESS));
	m_BindCombo.SetCurSel(m_BindCombo.FindString(0, l_bind.c_str()));
	
	if (m_BindCombo.GetCurSel() == -1)
	{
		m_BindCombo.AddString(l_bind.c_str());
		m_BindCombo.SetCurSel(m_BindCombo.FindString(0, l_bind.c_str()));
	}
	m_BindCombo.Detach();
	updateTestPortIcon(false);
	//::SendMessage(m_hWnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, IDC_ADD_FLYLINKDC_WINFIREWALL, true);
	//SetButtonElevationRequiredState(IDC_ADD_FLYLINKDC_WINFIREWALL,);
	
	bool l_is_wifi_router;
	const auto l_ip_gateway = Text::toT(Socket::getDefaultGateWay(l_is_wifi_router));
	::SetWindowText(GetDlgItem(IDC_DEFAULT_GATEWAY_IP), l_ip_gateway.c_str());
	{
		if (l_is_wifi_router)
		{
			static HIconWrapper g_hWiFiRouterIco(IDC_WIFI_ROUTER_ICO, 48, 48);
			GetDlgItem(IDC_WIFI_ROUTER_ICO).SendMessage(STM_SETICON, (WPARAM)(HICON)g_hWiFiRouterIco, 0L);
		}
	}
	
	return TRUE;
}

void NetworkPage::fixControls()
{
	const BOOL auto_detect = IsDlgButtonChecked(IDC_CONNECTION_DETECTION) == BST_CHECKED;
	const BOOL direct = IsDlgButtonChecked(IDC_DIRECT) == BST_CHECKED;
	const BOOL upnp = IsDlgButtonChecked(IDC_FIREWALL_UPNP) == BST_CHECKED;
	const BOOL nat = IsDlgButtonChecked(IDC_FIREWALL_NAT) == BST_CHECKED;
	const BOOL nat_traversal = IsDlgButtonChecked(IDC_NATT) == BST_CHECKED;
#ifdef STRONG_USE_DHT
	const BOOL dht = IsDlgButtonChecked(IDC_SETTINGS_USE_DHT) == BST_CHECKED;
#endif
	const BOOL passive = IsDlgButtonChecked(IDC_FIREWALL_PASSIVE) == BST_CHECKED;
	
	::EnableWindow(GetDlgItem(IDC_DIRECT), !auto_detect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_UPNP), !auto_detect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_NAT), !auto_detect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_PASSIVE), !auto_detect);
#ifdef STRONG_USE_DHT
	::EnableWindow(GetDlgItem(IDC_UPDATE_IP_DHT), dht); // [!] IRainman  if DHT is enable allow update IP from packets taked place firewall.
	::EnableWindow(GetDlgItem(IDC_PORT_DHT), dht);
	//SetStage(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO, dht ? StageWait : StageUnknown); // StageUnknown - не показывается
	//::EnableWindow(GetDlgItem(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO),dht);
	
	::EnableWindow(GetDlgItem(IDC_SETTINGS_USE_DHT_NOTANSWER), dht);
#endif
	::EnableWindow(GetDlgItem(IDC_SETTINGS_IP), !auto_detect);
	::EnableWindow(GetDlgItem(IDC_EXTERNAL_IP), !auto_detect && (direct || upnp || nat || nat_traversal));
	::EnableWindow(GetDlgItem(IDC_IP_GET_IP), !auto_detect && (upnp || nat)); //[+]PPA
	::EnableWindow(GetDlgItem(IDC_OVERRIDE), !auto_detect && (direct || upnp || nat || nat_traversal));
#ifdef IRAINMAN_IP_AUTOUPDATE
	//::EnableWindow(GetDlgItem(IDC_GETIP), (upnp || nat));
	::EnableWindow(GetDlgItem(IDC_IPUPDATE), (upnp || nat));
#endif
	const BOOL ipupdate = (upnp || nat) && (IsDlgButtonChecked(IDC_IPUPDATE) == BST_CHECKED);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_UPDATE_IP_INTERVAL), ipupdate);
	::EnableWindow(GetDlgItem(IDC_UPDATE_IP_INTERVAL), ipupdate);
	const BOOL l_port_enabled = !auto_detect && (upnp || nat);
	::EnableWindow(GetDlgItem(IDC_PORT_TCP),  l_port_enabled);
	::EnableWindow(GetDlgItem(IDC_PORT_UDP),  l_port_enabled);
	::EnableWindow(GetDlgItem(IDC_PORT_TLS),  l_port_enabled && CryptoManager::getInstance()->TLSOk());
	//::EnableWindow(GetDlgItem(IDC_NATT), passive); // for passive settings only,  [-] IRainman fix: why??
	
#ifdef RIP_USE_CONNECTION_AUTODETECT
	::EnableWindow(GetDlgItem(IDC_AUTODETECT), !passive);
#endif
	TestWinFirewall();
}

LRESULT NetworkPage::onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}
void NetworkPage::updateTestPortIcon(bool p_is_wait)
{
	//if (::IsWindow(m_BindCombo))
	{
		if (!p_is_wait)
		{
			++m_count_test_port_tick;
		}
		auto calcIconsIndex = [&](const int p_icon, const boost::logic::tribool & p_status)
		{
			if (p_is_wait && m_count_test_port_tick == 0)
			{
				SetStage(p_icon, StageWait);
			}
			else if (p_status)
			{
				SetStage(p_icon, StageSuccess);
			}
			else if (!p_status) //  || (m_count_test_port_tick > 1 && boost::logic::indeterminate(p_status))
			{
				SetStage(p_icon, StageFail);
			}
			else
			{
				SetStage(p_icon, StageQuestion);
			}
		};
		calcIconsIndex(IDC_NETWORK_TEST_PORT_TCP_ICO, SettingsManager::g_TestTCPLevel);
		calcIconsIndex(IDC_NETWORK_TEST_PORT_UDP_ICO, SettingsManager::g_TestUDPSearchLevel);
		if (CryptoManager::getInstance()->TLSOk())
		{
			calcIconsIndex(IDC_NETWORK_TEST_PORT_TLS_TCP_ICO, SettingsManager::g_TestTSLLevel);
		}
		else
		{
			SetStage(IDC_NETWORK_TEST_PORT_TLS_TCP_ICO, StageUnknown);
		}
		const BOOL dht = IsDlgButtonChecked(IDC_SETTINGS_USE_DHT) == BST_CHECKED;
		if (dht)
		{
			calcIconsIndex(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO, SettingsManager::g_TestUDPDHTLevel);
		}
		else
		{
			SetStage(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO, StageUnknown);
		}
	}
}
LRESULT NetworkPage::onAddWinFirewallException(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	TCHAR l_app_path[MAX_PATH];
	l_app_path[0] = 0;
	::GetModuleFileName(NULL, l_app_path, MAX_PATH);
#ifdef FLYLINKDC_USE_SSA_WINFIREWALL
	try
	{
		WinFirewall::WindowFirewallSetAppAuthorization(Text::toT(APPNAME).c_str(), l_app_path);
	}
	catch (...)
	{
	}
#else
//	http://www.dslreports.com/faq/dc/3.1_Software_Firewalls
	talk_base::WinFirewall fw;
	HRESULT hr = {0};
	HRESULT hr_auth = {0};
	bool authorized = true;
	fw.Initialize(&hr);
	// TODO - try
	// https://github.com/zhaozongzhe/gmDev/blob/70a1a871bb350860bdfff46c91913815184badd6/gmSetup/fwCtl.cpp
	const auto l_res = fw.AddApplicationW(l_app_path, Text::toT(APPNAME).c_str(), authorized, &hr_auth);
	// "C:\\vc10\\r5xx\\compiled\\FlylinkDC_Debug.exe"
	if (l_res)
	{
		::MessageBox(NULL, Text::toT("FlylinkDC.exe - OK").c_str(), _T("Windows Firewall"), MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		_com_error l_error_message(hr_auth);
		tstring l_message = _T("FlylinkDC++ module: ");
		l_message += l_app_path;
		l_message += Text::toT("\r\nError code = " + Util::toString(hr_auth) +
		                       "\r\nError text = ") + l_error_message.ErrorMessage();
		::MessageBox(NULL, l_message.c_str(), _T("Windows Firewall"), MB_OK | MB_ICONERROR);
	}
#endif
	TestWinFirewall();
	return 0;
}
void NetworkPage::TestWinFirewall()
{
	TCHAR l_app_path[MAX_PATH];
	l_app_path[0] = 0;
	::GetModuleFileName(NULL, l_app_path, MAX_PATH);
#ifdef FLYLINKDC_USE_SSA_WINFIREWALL
	bool l_res = false;
	try
	{
		l_res = WinFirewall::IsWindowsFirewallAppEnabled(l_app_path) != FALSE;
	}
	catch (...)
	{
	}
	bool authorized = l_res;
#else
	talk_base::WinFirewall fw;
	HRESULT hr = {0};
	bool authorized;
	fw.Initialize(&hr);
	const auto l_res = fw.QueryAuthorizedW(l_app_path, &authorized);
#endif
	if (l_res)
	{
		if (authorized)
		{
			SetStage(IDC_NETWORK_WINFIREWALL_ICO, StageSuccess);
		}
		else
		{
			SetStage(IDC_NETWORK_WINFIREWALL_ICO, StageFail);
		}
	}
	else
	{
		SetStage(IDC_NETWORK_WINFIREWALL_ICO, StageQuestion);
	}
}
LRESULT NetworkPage::onGetIP(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	TestWinFirewall();
	SettingsManager::testPortLevelInit();
	updateTestPortIcon(true);
	write();
	string l_external_ip;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	std::vector<unsigned short> l_udp_port, l_tcp_port;
	l_udp_port.push_back(SETTING(UDP_PORT));
#ifdef STRONG_USE_DHT
	l_udp_port.push_back(SETTING(DHT_PORT));
#endif
	l_tcp_port.push_back(SETTING(TCP_PORT));
	if (CryptoManager::getInstance()->TLSOk())
	{
		l_tcp_port.push_back(SETTING(TLS_PORT));
	}
	bool l_is_udp_port_send = CFlyServerAdapter::CFlyServerJSON::pushTestPort(ClientManager::getMyCID().toBase32(), l_udp_port, l_tcp_port, l_external_ip, 0);
	if (l_is_udp_port_send)
	{
		SetDlgItemText(IDC_EXTERNAL_IP, Text::toT(l_external_ip).c_str());
	}
	else
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
	{
		const string& l_url = SETTING(URL_GET_IP);
		if (Util::isHttpLink(l_url))
		{
			CWaitCursor l_cursor_wait; //-V808
			try
			{
				::EnableWindow(GetDlgItem(IDC_GETIP), FALSE);
				fixControls();
				auto l_ip = Util::getWANIP(l_url, 500);
				if (!l_ip.empty())
				{
					SetDlgItemText(IDC_EXTERNAL_IP, Text::toT(l_ip).c_str());
				}
			}
			catch (Exception & e)
			{
				// TODO - сюда никогда не попадаем?
				::MessageBox(NULL, Text::toT(e.getError()).c_str(), _T("SetIP Error!"), MB_OK | MB_ICONERROR);
			}
			::EnableWindow(GetDlgItem(IDC_GETIP), TRUE);
		}
		else
		{
			::MessageBox(NULL, Text::toT(l_url).c_str(), _T("http:// URL Error !"), MB_OK | MB_ICONERROR);
		}
	}
	return 0;
}


void NetworkPage::SetStage(int ID, StagesIcon stage)
{
	static HIconWrapper g_hModeActiveIco(IDR_ICON_SUCCESS_ICON);
	static HIconWrapper g_hModePassiveIco(IDR_ICON_WARN_ICON);
	static HIconWrapper g_hModeQuestionIco(IDR_ICON_QUESTION_ICON);
	static HIconWrapper g_hModeFailIco(IDR_ICON_FAIL_ICON);
	static HIconWrapper g_hModePauseIco(IDR_ICON_PAUSE_ICON);
	static HIconWrapper g_hModeProcessIco(IDR_NETWORK_STATISTICS_ICON);
	
	HIconWrapper* l_icon = nullptr;
	switch (stage)
	{
		case StageFail:
			l_icon = &g_hModeFailIco;
			break;
		case StageSuccess:
			l_icon = &g_hModeActiveIco;
			break;
		case StageWarn:
			l_icon = &g_hModePassiveIco;
			break;
		case StageUnknown:
			l_icon = &g_hModePauseIco;
			break;
		case StageQuestion:
			l_icon = &g_hModeQuestionIco;
			break;
		case StageWait:
		default:
			l_icon = &g_hModeProcessIco;
			break;
	}
	dcassert(l_icon);
	if (l_icon)
	{
		GetDlgItem(ID).SendMessage(STM_SETICON, (WPARAM)(HICON)*l_icon, 0L);
	}
}

// [+] brain-ripper (rewriten by IRainman)
// Display warning if DHT turned on
#ifdef STRONG_USE_DHT
LRESULT NetworkPage::onCheckDHTStats(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	if (IsDlgButtonChecked(IDC_SETTINGS_USE_DHT) == BST_CHECKED && !BOOLSETTING(USE_DHT_NOTANSWER))
	{
		if (MessageBox(CTSTRING(DHT_WARNING), CTSTRING(WARNING), MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON1) != IDYES)
		{
			CheckDlgButton(IDC_SETTINGS_USE_DHT, BST_UNCHECKED);
		}
	}
	fixControls();
	return 0;
}
#endif