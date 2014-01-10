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

#include "Resource.h"

#include "NetworkPage.h"
#include "WinUtil.h"

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
	{ IDC_CON_CHECK, ResourceManager::CHECK_SETTINGS },
	{ IDC_IPUPDATE, ResourceManager::UPDATE_IP },
	{ IDC_SETTINGS_UPDATE_IP_INTERVAL, ResourceManager::UPDATE_IP_INTERVAL },
#ifdef STRONG_USE_DHT
	{ IDC_UPDATE_IP_DHT, ResourceManager::SETTINGS_UPDATE_IP },
	{ IDC_SETTINGS_USE_DHT,  ResourceManager::USE_DHT },
	{ IDC_SETTINGS_USE_DHT_NOTANSWER,  ResourceManager::USE_DHT_NOTANSWER },
#endif
	{ IDC_GETIP, ResourceManager::GET_IP },
	
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
	{ IDC_URL_TEST_IP,      SettingsManager::URL_TEST_IP,   PropPage::T_STR }, //[+]PPA
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
		
	if (SETTING(INCOMING_CONNECTIONS) != ct)
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
	
#ifdef PPA_INCLUDE_UPNP
#ifdef FLYLINKDC_SUPPORT_WIN_2000
	if (!IsXPPlus())
	{
		::EnableWindow(GetDlgItem(IDC_FIREWALL_UPNP), FALSE); //[+]PPA
	}
#endif // FLYLINKDC_SUPPORT_WIN_2000
#else
	::EnableWindow(GetDlgItem(IDC_FIREWALL_UPNP), FALSE); //[+]PPA
#endif // PPA_INCLUDE_UPNP
	
#ifndef IRAINMAN_IP_AUTOUPDATE
	::EnableWindow(GetDlgItem(IDC_GETIP), FALSE);
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
	GetDlgItem(IDC_AUTODETECT).ShowWindow(FALSE);
#endif
	
	PropPage::read((HWND)(*this), items);
	
	fixControls();
	m_IPHint.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	m_IPHint.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(m_IPHint.IsWindow());
	
	desc.Attach(GetDlgItem(IDC_PORT_TCP));
	desc.LimitText(5);
	desc.Detach();
	desc.Attach(GetDlgItem(IDC_PORT_UDP));
	desc.LimitText(5);
	desc.Detach();
	desc.Attach(GetDlgItem(IDC_PORT_TLS));
	desc.LimitText(5);
	desc.Detach();
#ifdef STRONG_USE_DHT
	desc.Attach(GetDlgItem(IDC_PORT_DHT));
	desc.LimitText(5);
	desc.Detach();
#endif
	m_ConnCheckUrl.init(GetDlgItem(IDC_CON_CHECK), _T(""));
	BindCombo.Attach(GetDlgItem(IDC_BIND_ADDRESS));
	const auto l_tool_tip = WinUtil::getAddresses(BindCombo);
	m_IPHint.SetMaxTipWidth(1024);
	m_IPHint.AddTool(BindCombo, l_tool_tip.c_str());
	const auto l_bind = Text::toT(SETTING(BIND_ADDRESS));
	BindCombo.SetCurSel(BindCombo.FindString(0, l_bind.c_str()));
	
	if (BindCombo.GetCurSel() == -1)
	{
		BindCombo.AddString(l_bind.c_str());
		BindCombo.SetCurSel(BindCombo.FindString(0, l_bind.c_str()));
	}
	BindCombo.Detach();
	
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
	::EnableWindow(GetDlgItem(IDC_SETTINGS_USE_DHT_NOTANSWER), dht);
#endif
	::EnableWindow(GetDlgItem(IDC_SETTINGS_IP), !auto_detect);
	::EnableWindow(GetDlgItem(IDC_EXTERNAL_IP), !auto_detect && (direct || upnp || nat || nat_traversal));
	::EnableWindow(GetDlgItem(IDC_IP_GET_IP), !auto_detect && (upnp || nat)); //[+]PPA
	::EnableWindow(GetDlgItem(IDC_URL_TEST_IP), !passive);
	::EnableWindow(GetDlgItem(IDC_OVERRIDE), !auto_detect && (direct || upnp || nat || nat_traversal));
#ifdef IRAINMAN_IP_AUTOUPDATE
	::EnableWindow(GetDlgItem(IDC_GETIP), (upnp || nat));
	::EnableWindow(GetDlgItem(IDC_IPUPDATE), (upnp || nat));
#endif
	const BOOL ipupdate = (upnp || nat) && (IsDlgButtonChecked(IDC_IPUPDATE) == BST_CHECKED);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_UPDATE_IP_INTERVAL), ipupdate);
	::EnableWindow(GetDlgItem(IDC_UPDATE_IP_INTERVAL), ipupdate);
	::EnableWindow(GetDlgItem(IDC_PORT_TCP), !auto_detect && (upnp || nat));
	::EnableWindow(GetDlgItem(IDC_PORT_UDP), !auto_detect && (upnp || nat));
	::EnableWindow(GetDlgItem(IDC_PORT_TLS), !auto_detect && (upnp || nat));
	::EnableWindow(GetDlgItem(IDC_CON_CHECK), !passive);
	//::EnableWindow(GetDlgItem(IDC_NATT), passive); // for passive settings only,  [-] IRainman fix: why??
	
#ifdef RIP_USE_CONNECTION_AUTODETECT
	::EnableWindow(GetDlgItem(IDC_AUTODETECT), !passive);
#endif
}

LRESULT NetworkPage::onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

LRESULT NetworkPage::onGetIP(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	write();
	const string& l_url = SETTING(URL_GET_IP);
	if (Util::isHttpLink(l_url))
	{
		CWaitCursor l_cursor_wait;
		try
		{
			::EnableWindow(GetDlgItem(IDC_GETIP), false);
			fixControls();
			auto l_ip = Util::getExternalIP(l_url, 500);
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
		::EnableWindow(GetDlgItem(IDC_GETIP), true);
	}
	else
		::MessageBox(NULL, Text::toT(l_url).c_str(), _T("http:// URL Error !"), MB_OK | MB_ICONERROR);
	return 0;
}

LRESULT NetworkPage::onCheckConn(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	write();
	const string& aUrl = SETTING(URL_TEST_IP);
	if (Util::isHttpLink(aUrl))
	{
		WinUtil::openLink(Text::toT(aUrl) + _T("?port_IP=") + Util::toStringW(SETTING(TCP_PORT)) + _T("&port_PI=") + Util::toStringW(SETTING(UDP_PORT)) + _T("&ver=") + wstring(L_REVISION_NUM_STR));
	}
	return 0;
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