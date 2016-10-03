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

#if !defined(NETWORK_PAGE_H)
#define NETWORK_PAGE_H


#pragma once

#include <atlcrack.h>
#include "PropPage.h"
#include "wtl_flylinkdc.h"
#include "../client/SimpleXML.h"

class NetworkPage : public CPropertyPage<IDD_NETWORK_PAGE>, public PropPage
{
		enum StagesIcon
		{
			StageFail = 0,
			StageSuccess,
			StageWait,
			StageWarn,
			StageUnknown,
			StageQuestion
		};
		void SetStage(int ID, StagesIcon stage);
		void TestWinFirewall();
	public:
		explicit NetworkPage() : PropPage(TSTRING(SETTINGS_NETWORK)),
			m_count_test_port_tick(0), m_test_port_flood(0), m_is_manual(false), m_is_init(false)
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP(NetworkPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorDlg)
		
		COMMAND_HANDLER(IDC_EXTERNAL_IP, EN_KILLFOCUS, OnEnKillfocusExternalIp)
		COMMAND_ID_HANDLER(IDC_CONNECTION_DETECTION, onClickedActive)
		COMMAND_ID_HANDLER(IDC_DIRECT, onClickedActive)
		COMMAND_ID_HANDLER(IDC_FIREWALL_PASSIVE, onClickedActive)
		COMMAND_ID_HANDLER(IDC_FIREWALL_UPNP, onClickedActive)
		COMMAND_ID_HANDLER(IDC_FIREWALL_NAT, onClickedActive)
		COMMAND_ID_HANDLER(IDC_IPUPDATE, onClickedActive)
		COMMAND_ID_HANDLER(IDC_NATT, onClickedActive)
#ifdef RIP_USE_CONNECTION_AUTODETECT
		COMMAND_ID_HANDLER(IDC_AUTODETECT, onClickedActive)
#endif
		COMMAND_ID_HANDLER(IDC_WAN_IP_MANUAL, onWANIPManualClickedActive)
		COMMAND_ID_HANDLER(IDC_GETIP, onGetIP)
		COMMAND_ID_HANDLER(IDC_ADD_FLYLINKDC_WINFIREWALL, onAddWinFirewallException)
		
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onWANIPManualClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetIP(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onAddWinFirewallException(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT OnEnKillfocusExternalIp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnCtlColorDlg(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel()
		{
			cancel_check();
		}
		void updateTestPortIcon(bool p_is_wait);
	private:
		int m_count_test_port_tick;
		int m_test_port_flood;
		static Item items[];
		static TextItem texts[];
		CEdit m_desc;
		CFlyToolTipCtrl m_IPHint;
		CComboBox m_BindCombo;
		void fixControls();
		bool runTestPort();
		tstring m_original_test_port_caption;
		bool m_is_manual;
		bool m_is_init;
};

#endif // !defined(NETWORK_PAGE_H)

/**
 * @file
 * $Id: NetworkPage.h,v 1.6 2006/05/08 08:36:19 bigmuscle Exp $
 */
