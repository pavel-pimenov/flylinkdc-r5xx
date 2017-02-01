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

#ifndef _UPNP_CHECK_DLG_H_
#define _UPNP_CHECK_DLG_H_

#include "../client/CFlyThread.h"
#include "../client/ConnectionManager.h"

#pragma once

#ifdef SSA_WIZARD_FEATURE

#ifdef SSA_WIZARD_FEATURE_UPNP

#define WM_SETSTAGE     WM_USER + 0x03
#define WM_SETOK        WM_USER + 0x04
#define WM_SETFAIL      WM_USER + 0x05

struct IUPnPNAT;
struct IStaticPortMappingCollection;

class UPNPCheckDlg : public CDialogImpl<UPNPCheckDlg>, public Thread
{

	public:
		enum { IDD = IDD_UPNPCHECK };
		
		BEGIN_MSG_MAP(UPNPCheckDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog) // [5] Wizard https://www.box.net/shared/4f943b7602a4cb4f6811
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
		MESSAGE_HANDLER(WM_SETSTAGE, OnSetStage)
		MESSAGE_HANDLER(WM_SETOK, OnSetOK)
		MESSAGE_HANDLER(WM_SETFAIL, OnSetFAIL)
		END_MSG_MAP()
		
		UPNPCheckDlg(uint16_t tcp, uint16_t udp, bool needPortCheck, bool needUPNP, bool useServer);
		~UPNPCheckDlg();
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT OnSetStage(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnSetOK(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnSetFAIL(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		bool IsSuccessUPNP()
		{
			return _result;
		}
		bool IsSuccessTCPPort()
		{
			return _isTCPOk;
		}
		bool IsSuccessUDPPort()
		{
			return _isUDPOk;
		}
		string getDeviceName() const
		{
			return m_device;
		}
		
	private:
		int run();
		void setStage(const string& newStage);
		void success(bool portSuccess);
		void failed();
		
		bool MiniUPnPc_init(string& url, string& service, string& device);
		bool MiniUPnPc_add(const unsigned short port, const string& protocol, const string& description, const string& service, const string& url);
		bool MiniUPnPc_remove(const unsigned short port, const string& protocol, const string& service, const string& url);
		string MiniUPnPc_getExternalIP(string& url, string& service);
		
		bool WinUPnP_init(IUPnPNAT*& pUN);
		void WinUPnP_uninit();
		bool WinUPnP_add(const unsigned short port, const string& protocol, const string& description, IUPnPNAT* pUN);
		bool WinUPnP_remove(const unsigned short port, const string& protocol, IUPnPNAT* pUN);
		IStaticPortMappingCollection* WinUPnP_getStaticPortMappingCollection(IUPnPNAT* pUN);
		
		bool CheckPorts(bool& isTCPOk, bool& isUDPOk);
		
		bool StartPortListener(uint16_t tcp, uint16_t udp);
		bool StopPortListener();
		
		const uint16_t _tcp;
		const uint16_t _udp;
		bool _isResultSetted;
		bool _result;
		const bool _needUPNP;
		string _stage;
		CProgressBarCtrl ctrlProgress;
		const bool _needPortCheck;
		bool _resultPort;
		bool _isTCPOk;
		bool _isUDPOk;
		const bool _useServer;
		ConnectionManager::Server* _serverTCP;
		string m_device;
};

#endif // SSA_WIZARD_FEATURE_UPNP

#endif // SSA_WIZARD_FEATURE

#endif // _UPNP_CHECK_DLG_H_