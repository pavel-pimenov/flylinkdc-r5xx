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

#include "stdafx.h"

#include <comdef.h>

#include "NetworkPage.h"
#include "../FlyFeatures/flyServer.h"
#include "../client/CryptoManager.h"
#include "../client/MappingManager.h"
#include "../client/DownloadManager.h"

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
	{ IDC_WAN_IP_MANUAL, ResourceManager::SETTINGS_WAN_IP_MANUAL },
	{ IDC_NO_IP_OVERRIDE, ResourceManager::SETTINGS_NO_IP_OVERRIDE },
	{ IDC_SETTINGS_PORTS, ResourceManager::SETTINGS_PORTS },
	{ IDC_SETTINGS_IP, ResourceManager::SETTINGS_EXTERNAL_IP },
	{ IDC_SETTINGS_PORT_TCP, ResourceManager::SETTINGS_TCP_PORT },
	{ IDC_SETTINGS_PORT_UDP, ResourceManager::SETTINGS_UDP_PORT },
	{ IDC_SETTINGS_PORT_TLS, ResourceManager::SETTINGS_TLS_PORT },
	{ IDC_SETTINGS_PORT_TORRENT, ResourceManager::SETTINGS_TORRENT_PORT },
	{ IDC_SETTINGS_INCOMING, ResourceManager::SETTINGS_INCOMING },
	{ IDC_SETTINGS_BIND_ADDRESS, ResourceManager::SETTINGS_BIND_ADDRESS },
	{ IDC_SETTINGS_BIND_ADDRESS_HELP, ResourceManager::SETTINGS_BIND_ADDRESS_HELP },
	{ IDC_NATT, ResourceManager::ALLOW_NAT_TRAVERSAL },
	{ IDC_IPUPDATE, ResourceManager::UPDATE_IP },
	{ IDC_SETTINGS_UPDATE_IP_INTERVAL, ResourceManager::UPDATE_IP_INTERVAL },
	{ IDC_SETTINGS_USE_TORRENT,  ResourceManager::USE_TORRENT_SEARCH_TEXT },
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
	{ IDC_NO_IP_OVERRIDE, SettingsManager::NO_IP_OVERRIDE, PropPage::T_BOOL },
	{ IDC_IP_GET_IP,        SettingsManager::URL_GET_IP,    PropPage::T_STR },
	{ IDC_IPUPDATE,         SettingsManager::IPUPDATE,      PropPage::T_BOOL },
	{ IDC_WAN_IP_MANUAL, SettingsManager::WAN_IP_MANUAL, PropPage::T_BOOL },
	{ IDC_UPDATE_IP_INTERVAL, SettingsManager::IPUPDATE_INTERVAL, PropPage::T_INT },
	{ IDC_BIND_ADDRESS,     SettingsManager::BIND_ADDRESS, PropPage::T_STR },
	{ IDC_NATT,             SettingsManager::ALLOW_NAT_TRAVERSAL, PropPage::T_BOOL },
	{ IDC_PORT_TORRENT,         SettingsManager::DHT_PORT,      PropPage::T_INT },
	{ IDC_SETTINGS_USE_TORRENT, SettingsManager::USE_TORRENT_SEARCH, PropPage::T_BOOL },
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
		if (ec)
		{
			const auto l_last_ip = SETTING(EXTERNAL_IP);
			::MessageBox(NULL, Text::toT("Error IP = " + l_externalIP + ", restore last valid IP = " + l_last_ip).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
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
		g_settings->set(SettingsManager::INCOMING_CONNECTIONS, ct);
	}
	
#ifdef RIP_USE_CONNECTION_AUTODETECT
	g_settings->set(SettingsManager::INCOMING_AUTODETECT_FLAG, int(IsDlgButtonChecked(IDC_AUTODETECT)));
#endif
}

LRESULT NetworkPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	
#ifndef IRAINMAN_IP_AUTOUPDATE
	::EnableWindow(GetDlgItem(IDC_IPUPDATE), FALSE);
#endif
#ifdef FLYLINKDC_USE_TORRENT
	SET_SETTING(DHT_PORT, DownloadManager::getInstance()->listen_torrent_port());
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
	WinUtil::GetWindowText(m_original_test_port_caption, GetDlgItem(IDC_GETIP));
	
	boost::logic::tribool l_is_wifi_router;
	const string l_gateway_ip = Socket::getDefaultGateWay(l_is_wifi_router);
	MappingManager::setDefaultGatewayIP(l_gateway_ip);
	const auto l_ip_gatewayT = Text::toT(l_gateway_ip);
	::SetWindowText(GetDlgItem(IDC_DEFAULT_GATEWAY_IP), l_ip_gatewayT.c_str());
	const auto l_ip_upnp = Text::toT(MappingManager::getExternaIP());
	::SetWindowText(GetDlgItem(IDC_UPNP_EXTERNAL_IP), l_ip_upnp.c_str());
	if (l_is_wifi_router)
	{
		static HIconWrapper g_hWiFiRouterIco(IDC_WIFI_ROUTER_ICO, 48, 48);
		GetDlgItem(IDC_WIFI_ROUTER_ICO).SendMessage(STM_SETICON, (WPARAM)(HICON)g_hWiFiRouterIco, 0L);
	}
	m_is_init = true;
	return TRUE;
}

void NetworkPage::fixControls()
{
	const BOOL auto_detect = IsDlgButtonChecked(IDC_CONNECTION_DETECTION) == BST_CHECKED;
	//const BOOL direct = IsDlgButtonChecked(IDC_DIRECT) == BST_CHECKED;
	const BOOL upnp = IsDlgButtonChecked(IDC_FIREWALL_UPNP) == BST_CHECKED;
	const BOOL nat = IsDlgButtonChecked(IDC_FIREWALL_NAT) == BST_CHECKED;
	//const BOOL nat_traversal = IsDlgButtonChecked(IDC_NATT) == BST_CHECKED;
	const BOOL torrent = IsDlgButtonChecked(IDC_SETTINGS_USE_TORRENT) == BST_CHECKED;
	
	const BOOL passive = IsDlgButtonChecked(IDC_FIREWALL_PASSIVE) == BST_CHECKED;
	
	const BOOL wan_ip_manual = IsDlgButtonChecked(IDC_WAN_IP_MANUAL) == BST_CHECKED;
	
	::EnableWindow(GetDlgItem(IDC_EXTERNAL_IP), m_is_manual);
	::EnableWindow(GetDlgItem(IDC_DIRECT), !auto_detect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_UPNP), !auto_detect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_NAT), !auto_detect);
	::EnableWindow(GetDlgItem(IDC_FIREWALL_PASSIVE), !auto_detect);
	
	::EnableWindow(GetDlgItem(IDC_PORT_TORRENT), torrent);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_PORT_TORRENT), torrent);
	
	m_is_manual = wan_ip_manual;
	::EnableWindow(GetDlgItem(IDC_EXTERNAL_IP), m_is_manual);
	
	::EnableWindow(GetDlgItem(IDC_SETTINGS_IP), !auto_detect);
	
	// Вернул редакцию IP http://flylinkdc.com/forum/viewtopic.php?f=23&t=1294&p=5065#p5065
	::EnableWindow(GetDlgItem(IDC_IP_GET_IP), !auto_detect && (upnp || nat) && !m_is_manual);
	::EnableWindow(GetDlgItem(IDC_NO_IP_OVERRIDE), false); // !auto_detect && (direct || upnp || nat || nat_traversal));
#ifdef IRAINMAN_IP_AUTOUPDATE
	::EnableWindow(GetDlgItem(IDC_IPUPDATE), (upnp || nat));
#endif
	const BOOL ipupdate = (upnp || nat) && (IsDlgButtonChecked(IDC_IPUPDATE) == BST_CHECKED);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_UPDATE_IP_INTERVAL), ipupdate);
	::EnableWindow(GetDlgItem(IDC_UPDATE_IP_INTERVAL), ipupdate);
	const BOOL l_port_enabled = !auto_detect && (upnp || nat);
	::EnableWindow(GetDlgItem(IDC_PORT_TCP),  l_port_enabled);
	::EnableWindow(GetDlgItem(IDC_PORT_UDP),  l_port_enabled);
	::EnableWindow(GetDlgItem(IDC_PORT_TLS),  l_port_enabled && CryptoManager::TLSOk());
	::EnableWindow(GetDlgItem(IDC_BIND_ADDRESS), !auto_detect);
	
	
#ifdef RIP_USE_CONNECTION_AUTODETECT
	::EnableWindow(GetDlgItem(IDC_AUTODETECT), !passive);
#endif
	TestWinFirewall();
	auto calcUPnPIconsIndex = [&](const int p_icon, const boost::logic::tribool & p_status) -> bool
	{
		if (p_status)
		{
			SetStage(p_icon, StageSuccess);
			return true;
		}
		else if (!p_status)
		{
			SetStage(p_icon, StageFail);
		}
		else
		{
			SetStage(p_icon, StageUnknown);
		}
		return false;
	};
	
	calcUPnPIconsIndex(IDC_NETWORK_TEST_PORT_TCP_ICO_UPNP, SettingsManager::g_upnpTCPLevel);
	calcUPnPIconsIndex(IDC_NETWORK_TEST_PORT_UDP_ICO_UPNP, SettingsManager::g_upnpUDPSearchLevel);
	calcUPnPIconsIndex(IDC_NETWORK_TEST_PORT_TLS_TCP_ICO_UPNP, SettingsManager::g_upnpTLSLevel);
	calcUPnPIconsIndex(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO_UPNP, SettingsManager::g_upnpTorrentLevel);
	
}
LRESULT NetworkPage::onWANIPManualClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
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
		if (!p_is_wait && (m_is_manual == false || m_is_init == true))
		{
			++m_count_test_port_tick;
			if (m_test_port_flood)
				--m_test_port_flood;
			if (m_test_port_flood == 0)
			{
				::EnableWindow(GetDlgItem(IDC_GETIP), TRUE);
				if (!m_original_test_port_caption.empty())
					::SetWindowText(GetDlgItem(IDC_GETIP), m_original_test_port_caption.c_str());
			}
			else if (!m_original_test_port_caption.empty())
			{
				const tstring l_caption = m_original_test_port_caption + _T(" (") + Text::toT(Util::toString(m_test_port_flood)) + _T(")");
				::SetWindowText(GetDlgItem(IDC_GETIP), l_caption.c_str());
			}
		}
		auto calcIconsIndex = [&](const int p_icon, const boost::logic::tribool & p_status) -> bool
		{
			extern bool g_DisableTestPort;
			if (g_DisableTestPort)
			{
				SetStage(p_icon, StageDisableTest);
				return false;
			}
			if (p_is_wait && m_count_test_port_tick == 0)
			{
				SetStage(p_icon, StageWait);
			}
			else if (p_status)
			{
				SetStage(p_icon, StageSuccess);
				return true;
			}
			else if (!p_status) //  || (m_count_test_port_tick > 1 && boost::logic::indeterminate(p_status))
			{
				SetStage(p_icon, StageFail);
			}
			else
			{
				SetStage(p_icon, StageQuestion);
			}
			return false;
		};
		calcIconsIndex(IDC_NETWORK_TEST_PORT_TCP_ICO, SettingsManager::g_TestTCPLevel);
		calcIconsIndex(IDC_NETWORK_TEST_PORT_UDP_ICO, SettingsManager::g_TestUDPSearchLevel);
		if (CryptoManager::TLSOk())
		{
			calcIconsIndex(IDC_NETWORK_TEST_PORT_TLS_TCP_ICO, SettingsManager::g_TestTLSLevel);
		}
		else
		{
			SetStage(IDC_NETWORK_TEST_PORT_TLS_TCP_ICO, StageUnknown);
		}
		const BOOL torrent = IsDlgButtonChecked(IDC_SETTINGS_USE_TORRENT) == BST_CHECKED;
		if (torrent)
		{
			calcIconsIndex(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO, SettingsManager::g_TestTorrentLevel);
		}
		else
		{
			SetStage(IDC_NETWORK_TEST_PORT_DHT_UDP_ICO, StageUnknown);
		}
	}
}
LRESULT NetworkPage::onAddWinFirewallException(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	const tstring l_app_path = Util::getModuleFileName();
#ifdef FLYLINKDC_USE_SSA_WINFIREWALL
	try
	{
		WinFirewall::WindowFirewallSetAppAuthorization(getFlylinkDCAppCaptionT().c_str(), l_app_path.c_str());
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
	const auto l_res = fw.AddApplicationW(l_app_path.c_str(), getFlylinkDCAppCaptionT().c_str(), authorized, &hr_auth);
	// "C:\\vc10\\r5xx\\compiled\\FlylinkDC_Debug.exe"
	if (l_res)
	{
		::MessageBox(NULL, Text::toT("[Windows Firewall] FlylinkDC.exe - OK").c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		_com_error l_error_message(hr_auth);
		tstring l_message = _T("FlylinkDC++ module: ");
		l_message += l_app_path;
		l_message += Text::toT("\r\nError code = " + Util::toString(hr_auth) +
		                       "\r\nError text = ") + l_error_message.ErrorMessage();
		::MessageBox(NULL, l_message.c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
	}
#endif
	TestWinFirewall();
	return 0;
}
void NetworkPage::TestWinFirewall()
{
	const tstring l_app_path = Util::getModuleFileName();
	
#ifdef FLYLINKDC_USE_SSA_WINFIREWALL
	bool l_res = false;
	try
	{
		l_res = WinFirewall::IsWindowsFirewallAppEnabled(l_app_path.c_str()) != FALSE;
	}
	catch (...)
	{
	}
	bool authorized = l_res;
#else
	talk_base::WinFirewall fw;
	HRESULT hr = {0};
	bool authorized = false;
	fw.Initialize(&hr);
	const auto l_res = fw.QueryAuthorizedW(l_app_path.c_str(), &authorized);
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
bool NetworkPage::runTestPort()
{
	SettingsManager::testPortLevelInit();
	m_test_port_flood = 10;
	::EnableWindow(GetDlgItem(IDC_GETIP), FALSE);
	WinUtil::GetWindowText(m_original_test_port_caption, GetDlgItem(IDC_GETIP));
	string l_external_ip;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	std::vector<unsigned short> l_udp_port, l_tcp_port;
	l_udp_port.push_back(SETTING(UDP_PORT));
	l_tcp_port.push_back(SETTING(TCP_PORT));
	if (CryptoManager::TLSOk())
	{
		l_tcp_port.push_back(SETTING(TLS_PORT));
	}
	const bool l_is_udp_port_send = CFlyServerJSON::pushTestPort(l_udp_port, l_tcp_port, l_external_ip, 0, "Manual");
	if (l_is_udp_port_send && m_is_manual == false)
	{
		SetDlgItemText(IDC_EXTERNAL_IP, Text::toT(l_external_ip).c_str());
	}
	return l_is_udp_port_send;
#else
	return false;
#endif
}

LRESULT NetworkPage::onGetIP(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	TestWinFirewall();
	SettingsManager::testPortLevelInit();
	updateTestPortIcon(true);
	write();
	if (!runTestPort())
	{
		if (!m_is_manual)
		{
			const string l_url = SETTING(URL_GET_IP);
			if (Util::isHttpLink(l_url))
			{
				CWaitCursor l_cursor_wait; //-V808
				try
				{
					fixControls();
					auto l_ip = Util::getWANIP(l_url, 500);
					//if (!l_ip.empty())
					{
						SetDlgItemText(IDC_EXTERNAL_IP, Text::toT(l_ip).c_str());
					}
				}
				catch (Exception & e)
				{
					// TODO - сюда никогда не попадаем?
					::MessageBox(NULL, Text::toT(e.getError() + " (SetIP Error!)").c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
				}
			}
			else
			{
				::MessageBox(NULL, Text::toT(l_url + " (http:// URL Error !)").c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
			}
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
	static HIconWrapper g_hModeDisableTestIco(IDR_SKULL_RED_ICO);
	
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
		case StageDisableTest:
			l_icon = &g_hModeDisableTestIco;
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

