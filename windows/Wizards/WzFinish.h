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
#ifndef _WZ_FINISH_H_
#define _WZ_FINISH_H_

#pragma once

#ifdef SSA_WIZARD_FEATURE

#include "resource.h"

class WzFinish : public CPropertyPageImpl<WzFinish>
{
public:
    enum { IDD = IDD_WIZARD_FINISH };

    WzFinish() : CPropertyPageImpl<WzFinish>( CTSTRING(WIZARD_TITLE)  )
	{
	}

    // Maps
    BEGIN_MSG_MAP(WzFinish)
		MSG_WM_INITDIALOG(OnInitDialog)
        CHAIN_MSG_MAP(CPropertyPageImpl<WzFinish>)
    END_MSG_MAP()

	BOOL OnInitDialog ( HWND hwndFocus, LPARAM lParam )
	{
		// for Custom Themes Bitmap
		img_f.LoadFromResource(IDR_FLYLINK_PNG, _T("PNG"));
		GetDlgItem(IDC_WIZARD_STARTUP_PICT).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)img_f));

		SetDlgItemText(IDC_WIZARD_FINISH_LABEL1, CTSTRING(WIZARD_FINISH_LABEL1));
		SetDlgItemText(IDC_WIZARD_FINISH_LABEL2, CTSTRING(WIZARD_FINISH_LABEL2));

		HFONT hFnt = ::CreateFont(14, 0, 0, 0, FW_BOLD, 0, 0, 0, 1, 0, 0, 4, FF_DONTCARE, L"Times New Roman");
		
		CStatic label;
		label.Attach(GetDlgItem(IDC_WIZARD_FINISH_LABEL1));
		label.SetFont(hFnt);
		label.Detach();
		label.Attach(GetDlgItem(IDC_WIZARD_FINISH_LABEL2));
		label.SetFont(hFnt);
		label.Detach();

		::DeleteObject(hFnt);

		return TRUE;
	}

    // Notification handlers
    int OnSetActive()
	{
		SetWizardButtons ( PSWIZB_BACK | PSWIZB_FINISH );
		return 0;
	}

	private:
		ExCImage img_f;	
};

#endif // SSA_WIZARD_FEATURE

#endif // _WZ_FINISH_H_