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

#ifndef _WZ_NET_ACITVE_H_
#define _WZ_NET_ACITVE_H_

#pragma once

#ifdef SSA_WIZARD_FEATURE

#include "Resource.h"
#include "../FlyFeatures/WinFirewall.h"
//#include "UPNPCheckDlg.h"
#include "WinUtil.h"
#include "../client/ConnectionManager.h"

class WzNetActive : public CPropertyPageImpl<WzNetActive>
{
public:
    enum { IDD = IDD_WIZARD_NET_ACTIVE };
	enum StagesIcon
	{
		StageFail = 0,
		StageSuccess,
		StageWait,
		StageWarn,
		StageUnknown
	};
    // Construction
    WzNetActive() : CPropertyPageImpl<WzNetActive>( CTSTRING(WIZARD_TITLE)/*IDS_WIZARD_TITLE*/)
	{
			_isUPNPChecked = false;
			_isTCPOk = false;
			_isUDPOk = false;
	}

    // Maps
    BEGIN_MSG_MAP(WzNetActive)
        MSG_WM_INITDIALOG(OnInitDialog)
        CHAIN_MSG_MAP(CPropertyPageImpl<WzNetActive>)
		COMMAND_ID_HANDLER(IDC_WIZARD_NETA_BTN_RAND, OnBnClickedWizardNetaBtnRand)
		COMMAND_ID_HANDLER(IDC_WIZARD_NETA_USEDC, OnBnClickedWizardNetaUsedc)
		COMMAND_ID_HANDLER(IDC_WIZARD_NETA_BTN_CHECKUPNP, OnBnClickedWizardNetaCheckUPNP)
		COMMAND_ID_HANDLER(IDC_WIZARD_NETA_WINFIREWALL_BTN,OnBnClickedWizardNetaWinFirewalOpen)
		COMMAND_ID_HANDLER(IDC_WIZARD_NETA_BTN_TEST, OnBnClickedWizardNetaBtnTest)
	END_MSG_MAP()

    // Message handlers
    BOOL OnInitDialog ( HWND hwndFocus, LPARAM lParam )
	{		
		GetPropertySheet().SendMessage ( WM_CENTER_ALL );

		// for Custom Themes Bitmap
		img_f.LoadFromResourcePNG(IDR_FLYLINK_PNG);
		GetDlgItem(IDC_WIZARD_STARTUP_PICT).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)img_f));
		
		SetDlgItemText(IDC_WIZARD_NETA_TITLE, CTSTRING(WIZARD_NETA_TITLE));
		SetDlgItemText(IDC_SETTINGS_PORTS, CTSTRING(SETTINGS_PORTS));
		SetDlgItemText(IDC_WIZARD_NETA_BTN_RAND, CTSTRING(WIZARD_NICK_RND));
		SetDlgItemText(IDC_WIZARD_NETA_USEDC, CTSTRING(WIZARD_NETA_USEDC));
		SetDlgItemText(IDC_WIZARD_NETA_FIREWALL_STATIC, CTSTRING(WIZARD_NETA_FIREWALL_STATIC));
		SetDlgItemText(IDC_WIZARD_NETA_WINFIREWALL, CTSTRING(WIZARD_NETA_WINFIREWALL));
		SetDlgItemText(IDC_WIZARD_NETA_WINFIREWALL_STATIC2, CTSTRING(WIZARD_NETA_WINFIREWALL_STATIC2));
		SetDlgItemText(IDC_WIZARD_NETA_WINFIREWALL_BTN, CTSTRING(OPEN));
		SetDlgItemText(IDC_WIZARD_NETA_ROUTER_TITLE, CTSTRING(WIZARD_NETA_ROUTER_TITLE));
		SetDlgItemText(IDC_WIZARD_NETA_BTN_SETUPROUTER, CTSTRING(WIZARD_NETA_BTN_SETUPROUTER));
		SetDlgItemText(IDC_WIZARD_NETA_ROUTER_STATIC, CTSTRING(WIZARD_NETA_ROUTER_STATIC));
		SetDlgItemText(IDC_WIZARD_NETA_BTN_TEST, CTSTRING(WIZARD_NETA_BTN_TEST));
		SetDlgItemText(IDC_WIZARD_NETA_BTN_CHECKUPNP, CTSTRING(WIZARD_NETA_BTN_CHECKUPNP));
		
		uint16_t tcp = SETTING(TCP_PORT);
		SetDlgItemText(IDC_PORT_TCP, Util::toStringW(tcp).c_str());

		uint16_t udp = SETTING(UDP_PORT);
		SetDlgItemText(IDC_PORT_UDP, Util::toStringW(udp).c_str());
		if(!tcp || !udp )
		{
			MakeRandomPorts();
		}

		if (SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_DIRECT)
		{
			CheckDlgButton(IDC_WIZARD_NETA_USEDC, BST_CHECKED);
		}

		SetStage(IDC_WIZARD_NETA_TCP_ICO, StageWait);
		SetStage(IDC_WIZARD_NETA_UDP_ICO, StageWait);
		SetStage(IDC_WIZARD_NETA_WINFIREWALL_ICO, StageWait);
		SetStage(IDC_WIZARD_NETA_UPNP_ICO, StageWait);

		CheckPortEnable();
		CheckFirewall();
		const auto l_device_name = CheckUPNP(true, true); 
		if(!l_device_name.empty())
		{
			tstring l_router_name = Text::toT(l_device_name);
			SetDlgItemText(IDC_WIZARD_NETA_BTN_SETUPROUTER, l_router_name.c_str());
		}
		/*
		loop iterates over multiple ports to open. 
		if (test_ports() == ok)
				 return manual_mapping
		else  if (test_upnp() == ok && test_ports() == ok)
				 return upnp
		else
				 return passive
		*/
#ifdef FLYLINKDC_SUPPORT_WIN_XP
		if (CompatibilityManager::isOsVistaPlus())
#endif
			GetDlgItem(IDC_WIZARD_NETA_WINFIREWALL_BTN).SendMessage(BCM_FIRST + 0x000C, 0, 0xFFFFFFFF);

		return TRUE;
	}

	bool _isUPNPChecked;
	bool _isTCPOk;
	bool _isUDPOk;

	string CheckUPNP(bool needPortCheck, bool needUPNP)
	{
		uint16_t tcp, udp;
		GetPortFromPage(tcp, udp);		
		
		bool useServer = true;
		if (ConnectionManager::getInstance()->isValidInstance() && !ConnectionManager::isShuttingDown() && ConnectionManager::getInstance()->getPort() == tcp)
		{
			useServer = false;
		}

		UPNPCheckDlg dlg(tcp, udp, needPortCheck, needUPNP, useServer);
		INT res = dlg.DoModal(*this);
		if ( needUPNP )
		{
			if (res == IDOK)
			{
				SetStage(IDC_WIZARD_NETA_UPNP_ICO, StageSuccess);
				_isUPNPChecked = true;
			}else
			{
				SetStage(IDC_WIZARD_NETA_UPNP_ICO, StageFail);
				_isUPNPChecked = false;
			}
		}
		if ( needPortCheck )
		{
			if (dlg.IsSuccessTCPPort())
			{
				SetStage(IDC_WIZARD_NETA_TCP_ICO, StageSuccess);
				_isTCPOk = true;
			}else
			{
				SetStage(IDC_WIZARD_NETA_TCP_ICO, StageFail);
				_isTCPOk = false;
			}
			if (dlg.IsSuccessUDPPort())
			{
				SetStage(IDC_WIZARD_NETA_UDP_ICO, StageSuccess);
				_isUDPOk = true;
			}else
			{
				SetStage(IDC_WIZARD_NETA_UDP_ICO, StageWait);
				_isUDPOk = false;
			}
		}
		return dlg.getDeviceName(); 
	}
	bool WrongPorts(uint16_t tcp, uint16_t udp)
	{
		if (tcp == udp) // TODO please fix me: script on the website does not understand the combination of ports (they are), although it is permissible.
			return true;

		if (SET_SETTING(TCP_PORT, tcp) || SET_SETTING(UDP_PORT, udp)) 
			return true;
	
		return false;
	}

	void CheckFirewall()
	{
		try{
			if (!WinFirewall::IsWindowsFirewallIsOn())
			{
				GetDlgItem(IDC_WIZARD_NETA_WINFIREWALL_BTN).EnableWindow(FALSE);
				SetStage(IDC_WIZARD_NETA_WINFIREWALL_ICO, StageSuccess);
				return;
			}
		}
		catch(...)
		{
		}
		const tstring in_pAuthorizedFilename = Util::getModuleFileName();
		try{
			if (WinFirewall::IsWindowsFirewallAppEnabled(in_pAuthorizedFilename.c_str()))
			{
				GetDlgItem(IDC_WIZARD_NETA_WINFIREWALL_BTN).EnableWindow(FALSE);
				SetStage(IDC_WIZARD_NETA_WINFIREWALL_ICO, StageSuccess);
				return;
			}
		}
		catch(...)
		{
		}
		uint16_t tcp, udp;
		GetPortFromPage(tcp, udp);		
		try{
			if (WinFirewall::IsWindowsFirewallPortEnabled(tcp, NET_FW_IP_PROTOCOL_TCP) && WinFirewall::IsWindowsFirewallPortEnabled(udp, NET_FW_IP_PROTOCOL_UDP))
			{
				GetDlgItem(IDC_WIZARD_NETA_WINFIREWALL_BTN).EnableWindow(FALSE);
				SetStage(IDC_WIZARD_NETA_WINFIREWALL_ICO, StageSuccess);
				return;
			}
		}
		catch(...)
		{
		}
		SetStage(IDC_WIZARD_NETA_WINFIREWALL_ICO, StageFail);
		GetDlgItem(IDC_WIZARD_NETA_WINFIREWALL_BTN).EnableWindow(TRUE);
	}

	void GetPortFromPage(uint16_t& tcp, uint16_t& udp)
	{
		tstring tmp;
		GET_TEXT(IDC_PORT_TCP, tmp);
		tcp = Util::toInt(Text::fromT(tmp));
		GET_TEXT(IDC_PORT_UDP, tmp);
		udp = Util::toInt(Text::fromT(tmp));
	}

	void CheckPortEnable()
	{
		GetDlgItem(IDC_PORT_TCP).EnableWindow( IsDlgButtonChecked(IDC_WIZARD_NETA_USEDC) != BST_CHECKED);
		GetDlgItem(IDC_PORT_UDP).EnableWindow( IsDlgButtonChecked(IDC_WIZARD_NETA_USEDC) != BST_CHECKED);
		GetDlgItem(IDC_WIZARD_NETA_BTN_RAND).EnableWindow( IsDlgButtonChecked(IDC_WIZARD_NETA_USEDC) != BST_CHECKED);
	}

    // Notification handlers
    int OnSetActive()
	{		
		SetWizardButtons ( PSWIZB_BACK | PSWIZB_NEXT );
		return 0;
	}

	LRESULT OnBnClickedWizardNetaCheckUPNP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CheckUPNP(false, true);
		return 0;
	}

	LRESULT OnBnClickedWizardNetaBtnRand(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		MakeRandomPorts();
		return 0;
	}

	LRESULT OnBnClickedWizardNetaUsedc(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{	
		if ( IsDlgButtonChecked(IDC_WIZARD_NETA_USEDC) == BST_CHECKED) 
			MakeRandomPorts();

		CheckPortEnable();
		return 0;
	}
	LRESULT OnBnClickedWizardNetaWinFirewalOpen(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		const tstring in_pAuthorizedFilename = Util::getModuleFileName();
		try{
			WinFirewall::WindowFirewallSetAppAuthorization(getFlylinkDCAppCaptionT().c_str(), in_pAuthorizedFilename.c_str());
		}
		catch (...) {}
		CheckFirewall();
		return 0;
	}

	LRESULT OnBnClickedWizardNetaBtnTest(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		CheckUPNP(true, _isUPNPChecked);
		if (_isTCPOk)
		{
			MessageBox(CTSTRING(WIZARD_NETA_SETUPSUCCESS), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONASTERISK); //  L"FlylinkDC++ active mode was succesfully setuped"
		}else
		{
			MessageBox(CTSTRING(WIZARD_NETA_SETUPFAILED), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONHAND); // L"FlylinkDC++ active mode was not succesfully setuped"
		}
		return 0;
	}

	void MakeRandomPorts()
	{

		uint16_t tcpval = SettingsManager::getNewPortValue();
		uint16_t udpval = SettingsManager::getNewPortValue();
		CString tcp;
		tcp.SetString(Text::toT( Util::toString( tcpval )).c_str());
		SetDlgItemText(IDC_PORT_TCP, tcp);

		CString udp;
		udp.SetString(Text::toT( Util::toString( udpval )).c_str());
		SetDlgItemText(IDC_PORT_UDP, udp);
	}

	void SetStage(int ID, StagesIcon stage)
	{
	static HIconWrapper g_hModeActiveIco(IDR_ICON_SUCCESS_ICON);
	static HIconWrapper g_hModePassiveIco(IDR_ICON_WARN_ICON);
	static HIconWrapper g_hModeFailIco(IDR_ICON_FAIL_ICON);
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
		case StageWait:
		default:
			l_icon = &g_hModeProcessIco;
			break;
	}
	dcassert(l_icon);
	if(l_icon)
		{
		 GetDlgItem(ID).SendMessage(STM_SETICON, (WPARAM)(HICON)*l_icon, 0L);
		}
	}

	int OnWizardNext()//OnKillActive()
	{
		try
		{
		if (!_isTCPOk)
		{
			// �� ���������� ��� ��������� �����. 
			// isNeedPassiveMode = (MessageBox(CTSTRING(WIZARD_NETA_NEEDPASSIVEMODE), CTSTRING(WIZARD_TITLE), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1) == IDYES);
			// TODO - ���������� ������� WIZARD_NETA_NEEDPASSIVEMODE
		}
		tstring tmp;
		GET_TEXT(IDC_PORT_TCP, tmp);
//		bool test; // TODO
/*		test = */SET_SETTING( TCP_PORT , Text::fromT( tmp ));
//if ( test) ???
		GET_TEXT(IDC_PORT_UDP, tmp);
/*		test = */SET_SETTING( UDP_PORT , Text::fromT( tmp ));
//if ( test) ???
		SET_SETTING( INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_UPNP );
		return FALSE;       // allow deactivation
		}
		catch(Exception & e)
		{
			SET_SETTING( INCOMING_CONNECTIONS, SettingsManager::INCOMING_FIREWALL_UPNP );
			::MessageBox(NULL,Text::toT(e.getError()).c_str(), _T("Net Wizard Error!"), MB_OK | MB_ICONERROR);
			return FALSE;       
		}

	}

	private:
		ExCImage img_f;

};

#endif // SSA_WIZARD_FEATURE

#endif // _WZ_NET_ACITVE_H_