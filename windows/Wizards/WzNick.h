/*
 * Copyright (C) 2011-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#ifndef _WZ_NICK_H_
#define _WZ_NICK_H_

#pragma once

#ifdef SSA_WIZARD_FEATURE

#include "resource.h"

class WzNick : public CPropertyPageImpl<WzNick>
{
public:
    enum { IDD = IDD_WIZARD_NICK };

    // Construction
    WzNick() : CPropertyPageImpl<WzNick>(  CTSTRING(WIZARD_TITLE) )	
	{
		/*[-] IRainman fix srand( (unsigned)time( NULL ) );*/
	}

    // Maps
    BEGIN_MSG_MAP(WzNick)
        MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER(IDC_WIZARD_NICK_RND, onRandomNick)
        CHAIN_MSG_MAP(CPropertyPageImpl<WzNick>)
    END_MSG_MAP()

	LRESULT onRandomNick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		SetDlgItemText(IDC_WIZARD_LOGIN_EDIT, Text::toT(Util::getRandomNick()).c_str());

		return 0;
	}

    // Message handlers
    BOOL OnInitDialog ( HWND hwndFocus, LPARAM lParam )
	{
		// for Custom Themes Bitmap
		img_f.LoadFromResource(IDR_FLYLINK_PNG, _T("PNG"));
		GetDlgItem(IDC_WIZARD_STARTUP_PICT).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)img_f));

		//CRect rc;
		//GetClientRect(rc);
		//if( rc.bottom > 271 || rc.right > 428)
		//{
		//	CStatic st;
		//	st.Attach(GetDlgItem(IDC_WIZARD_STARTUP_PICT));
		//	st.CenterWindow(m_hWnd);
		//	st.Detach();
		//}

		SetDlgItemText(IDC_WIZARD_NICK_STATIC, CTSTRING(WIZARD_NICK_STATIC));
		SetDlgItemText(IDC_WIZARD_NICK_STATIC1, CTSTRING(WIZARD_NICK_STATIC1));
		SetDlgItemText(IDC_WIZARD_NICK_STATIC2, CTSTRING(WIZARD_NICK_STATIC2));
		SetDlgItemText(IDC_WIZARD_NICK_EMAIL_LABEL, CTSTRING(WIZARD_NICK_EMAIL_LABEL));
		SetDlgItemText(IDC_WIZARD_NICK_DESC_LABEL, CTSTRING(WIZARD_NICK_DESC_LABEL));
		SetDlgItemText(IDC_WIZARD_NICK_RND, CTSTRING(WIZARD_NICK_RND));
		// Set already entered values:

		CString login;
		login.SetString(Text::toT( SETTING(NICK) ).c_str());
		SetDlgItemText(IDC_WIZARD_LOGIN_EDIT, login);

		CString email;
		email.SetString(Text::toT( SETTING(EMAIL) ).c_str());
		SetDlgItemText(IDC_WIZARD_NICK_EMAIL, email); 

		CString descr;
		descr.SetString(Text::toT( SETTING(DESCRIPTION) ).c_str());
		SetDlgItemText(IDC_WIZARD_NICK_DESCR, descr);

		return TRUE;
	}

	int OnSetActive()
	{
		SetWizardButtons ( PSWIZB_BACK | PSWIZB_NEXT );
		return 0;
	}

	int  OnWizardBack()
	{
		return IDD_WIZARD_STARTUP;
	}

	int OnWizardNext()//OnKillActive()
	{
		try
		{

		tstring tmp;
		GET_TEXT(IDC_WIZARD_LOGIN_EDIT, tmp);
		if(tmp.size() < 3 || tmp.size() > 64)
		{
			MessageBox( CTSTRING(WIZARD_NICK_ERROR_LENGTH), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONERROR );
			return TRUE;
		}
		SET_SETTING(NICK, Text::fromT(tmp));

		GET_TEXT(IDC_WIZARD_NICK_EMAIL, tmp);
		SET_SETTING(EMAIL, Text::fromT(tmp));
		
		GET_TEXT(IDC_WIZARD_NICK_DESCR, tmp);
		SET_SETTING(DESCRIPTION, Text::fromT(tmp));


		return FALSE;       // allow deactivation
		}
		catch(Exception & e)
		{
			::MessageBox(NULL,Text::toT(e.getError()).c_str(), _T("Nick Wizard Error!"), MB_OK | MB_ICONERROR);
			return FALSE;       
		}

	}
	
	CEdit m_login;
	private:
		ExCImage img_f;

};

#endif // SSA_WIZARD_FEATURE

#endif // _WZ_NICK_H_