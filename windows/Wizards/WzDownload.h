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

#ifndef _WZ_DOWNLOAD_H_
#define _WZ_DOWNLOAD_H_

#pragma once

#ifdef SSA_WIZARD_FEATURE

#include "Resource.h"

class WzDownload : public CPropertyPageImpl<WzDownload>
{
public:
    enum { IDD = IDD_WIZARD_DOWNLOAD };

    // Construction
	WzDownload() : CPropertyPageImpl<WzDownload>( CTSTRING(WIZARD_TITLE) ) 	
	{
	}

    // Maps
    BEGIN_MSG_MAP(WzDownload)
        MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER(IDC_WIZARD_BROWSEDIR, onClickedBrowseDir)
		COMMAND_ID_HANDLER(IDC_WIZARD_BROWSETEMPDIR, onClickedBrowseTempDir)
        CHAIN_MSG_MAP(CPropertyPageImpl<WzDownload>)
    END_MSG_MAP()

    // Message handlers
    BOOL OnInitDialog ( HWND hwndFocus, LPARAM lParam )
	{
		// for Custom Themes Bitmap
		img_f.LoadFromResourcePNG(IDR_FLYLINK_PNG);
		GetDlgItem(IDC_WIZARD_STARTUP_PICT).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)img_f));

		SetDlgItemText(IDC_WIZARD_DOWNLOAD_STATIC,CTSTRING(SETTINGS_DOWNLOAD_DIRECTORY));
		SetDlgItemText(IDC_WIZARD_DOWNLOADTEMP_STATIC,CTSTRING(SETTINGS_UNFINISHED_DOWNLOAD_DIRECTORY));
		SetDlgItemText(IDC_WIZARD_BROWSEDIR, CTSTRING(BROWSE));
		SetDlgItemText(IDC_WIZARD_BROWSETEMPDIR, CTSTRING(BROWSE));
		SetDlgItemText(IDC_WIZARD_DOWNDIR_HELP, CTSTRING(WIZARD_DOWNLOADDIR_STATIC));	// [+] Help frame, about freespace and etc...
		
		CString dDir;
		dDir.SetString(Text::toT( SETTING( DOWNLOAD_DIRECTORY ) ).c_str());
		SetDlgItemText(IDC_WIZARD_DOWNLOADDIR, dDir);

		CString dDirTemp;
		dDirTemp.SetString(Text::toT( SETTING( TEMP_DOWNLOAD_DIRECTORY ) ).c_str());
		SetDlgItemText(IDC_WIZARD_TEMP_DOWNLOAD_DIRECTORY, dDirTemp);
	
		return TRUE;
	}

	LRESULT onClickedBrowseDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		tstring dir = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
		if(WinUtil::browseDirectory(dir, m_hWnd))
		{
			AppendPathSeparator(dir);
			SetDlgItemText(IDC_WIZARD_DOWNLOADDIR, dir.c_str());
		}
		return 0;
	}

	LRESULT onClickedBrowseTempDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		tstring dir = Text::toT(SETTING(TEMP_DOWNLOAD_DIRECTORY));
		if(WinUtil::browseDirectory(dir, m_hWnd))
		{
			AppendPathSeparator(dir);
			SetDlgItemText(IDC_WIZARD_TEMP_DOWNLOAD_DIRECTORY, dir.c_str());
		}
		return 0;
	}

	int OnSetActive()
	{
		SetWizardButtons ( PSWIZB_BACK | PSWIZB_NEXT );
		return 0;
	}

	int OnWizardNext()
	{
		try
		{
		tstring ddir;
		ddir.resize(1024);
		ddir.resize(::GetDlgItemText(m_hWnd, IDC_WIZARD_DOWNLOADDIR, &ddir[0], 1024));
		if(ddir.size())
		{
			SET_SETTING(DOWNLOAD_DIRECTORY, Text::fromT(ddir));
		}

		tstring tdir;
		tdir.resize(1024);
		tdir.resize(::GetDlgItemText(m_hWnd, IDC_WIZARD_TEMP_DOWNLOAD_DIRECTORY, &tdir[0], 1024));
		if(tdir.size())
		{
		  SET_SETTING( TEMP_DOWNLOAD_DIRECTORY, Text::fromT(tdir) );
		}
		return FALSE;       // allow deactivation
		}
		catch(Exception & e)
		{
			::MessageBox(NULL,Text::toT(e.getError()).c_str(), _T("Download Wizard Error!"), MB_OK | MB_ICONERROR);
			return FALSE;       
		}
	}

	private:
		ExCImage img_f;
};

#endif // SSA_WIZARD_FEATURE

#endif // _WZ_DOWNLOAD_H_