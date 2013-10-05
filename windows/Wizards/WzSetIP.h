/*
 * Copyright (C) 2011-2013 FlylinkDC++ Team http://flylinkdc.com/
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

#ifndef _WZ_SET_IP_H_
#define _WZ_SET_IP_H_

#pragma once

#ifdef SSA_WIZARD_FEATURE

#include "resource.h"
#include "../client/SimpleXML.h"

class WzSetIP : public CPropertyPageImpl<WzSetIP>
{
public:
    enum { IDD = IDD_WIZARD_NET_IP };

    // Construction
    WzSetIP() : CPropertyPageImpl<WzSetIP>( CTSTRING(WIZARD_TITLE)/*IDS_WIZARD_TITLE*/)
	{
			_isInProcess = false;
	}
	~WzSetIP()
	{
	}
    // Maps
    BEGIN_MSG_MAP(FlyWizard)
        MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER(IDC_WIZARD_NET_IP_DETECT_IP, onGetIP)
        CHAIN_MSG_MAP(CPropertyPageImpl<WzSetIP>)
    END_MSG_MAP()

    // Message handlers
    BOOL OnInitDialog ( HWND hwndFocus, LPARAM lParam )
	{		
		GetPropertySheet().SendMessage ( WM_CENTER_ALL );

		// for Custom Themes Bitmap
		img_f.LoadFromResource(IDR_FLYLINK_PNG, _T("PNG"));
		GetDlgItem(IDC_WIZARD_STARTUP_PICT).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)img_f));		

		CString externalIP;
		externalIP.SetString(Text::toT( SETTING(EXTERNAL_IP) ).c_str());
		SetDlgItemText(IDC_EXTERNAL_IP, externalIP);

		CheckDlgButton(IDC_WIZARD_NET_IP_AUTO_IPUPDATE, BOOLSETTING(IPUPDATE) ? BST_CHECKED : BST_UNCHECKED);

		SetDlgItemText(IDC_WIZARD_NET_IP_TITLE, CTSTRING(WIZARD_NET_IP_TITLE));
		SetDlgItemText(IDC_WIZARD_NET_IP_STATIC, CTSTRING(WIZARD_NET_IP_STATIC));
		SetDlgItemText(IDC_WIZARD_NET_IP_AUTO_IPUPDATE, CTSTRING(WIZARD_NET_IP_AUTO_IPUPDATE));
		SetDlgItemText(IDC_WIZARD_NET_IP_DETECT_IP, CTSTRING(WIZARD_NET_IP_DETECT_IP));
		/*
		if (externalIP.IsEmpty())
		{
			BOOL bHandled = true;
			onGetIP(NULL, NULL, NULL, bHandled);
		}
		*/
		return TRUE;
	}

	LRESULT onGetIP(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
	{
		// write();
		const string& l_url = SETTING(URL_GET_IP);
		if (Util::isHttpLink(l_url))
		{
			 CWaitCursor l_cursor_wait;
			::EnableWindow(GetDlgItem(IDC_WIZARD_NET_IP_DETECT_IP), false);
			::EnableWindow(GetDlgItem(IDC_EXTERNAL_IP), false);
			_isInProcess = true;
		try
		{
		SetDlgItemText(IDC_EXTERNAL_IP, Text::toT(Util::getExternalIP(l_url)).c_str());
		}
		catch(Exception & e)
		{
				::MessageBox(NULL,Text::toT(e.getError()).c_str(), _T("GetIP Error!"), MB_OK | MB_ICONERROR);
		}
		::EnableWindow(GetDlgItem(IDC_GETIP), true);
		::EnableWindow(GetDlgItem(IDC_WIZARD_NET_IP_DETECT_IP), true);
		_isInProcess = false;
		}
		else
		::MessageBox(NULL,Text::toT(l_url).c_str(), _T("http:// URL Error !"), MB_OK | MB_ICONERROR);

		return 0;
	}

    // Notification handlers
    int OnSetActive()
	{		
		_isInProcess = false;
		SetWizardButtons ( PSWIZB_BACK | PSWIZB_NEXT );
		return 0;
	}


	int  OnWizardBack()
	{
		if (!_isInProcess) // (GetDlgItem(IDC_WIZARD_NET_IP_DETECT_IP).IsWindowEnabled()) 
			return IDD_WIZARD_NICK;
		else
			return 1;
	}

	int OnWizardNext()
	{
		try
		{
		if (!_isInProcess) // (GetDlgItem(IDC_WIZARD_NET_IP_DETECT_IP).IsWindowEnabled())
		{
			const BOOL isAutoUpdateIP = IsDlgButtonChecked(IDC_WIZARD_NET_IP_AUTO_IPUPDATE);
			// Save Info

				tstring externalIP;
				GET_TEXT(IDC_EXTERNAL_IP, externalIP);
				SET_SETTING(EXTERNAL_IP, Text::fromT(externalIP));

			SET_SETTING( IPUPDATE, isAutoUpdateIP );

			return IDD_WIZARD_NET_ACTIVE;
		}
		else
			return 1;
		}
		catch(Exception & e)
		{
			::MessageBox(NULL,Text::toT(e.getError()).c_str(), _T("Set IP Wizard Error!"), MB_OK | MB_ICONERROR);
			_isInProcess = false;
			return 0;       
		}

	}


private:
		ExCImage img_f;
		bool _isInProcess;
};

#endif // SSA_WIZARD_FEATURE

#endif // _WZ_SET_IP_H_