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

#ifndef _FLY_WIZARD_H_
#define _FLY_WIZARD_H_

#pragma once

#ifdef SSA_WIZARD_FEATURE

#include "Wizards/WzStartup.h"
#include "Wizards/WzNick.h"
#include "Wizards/WzDownload.h"
//#include "Wizards/WzFinish.h"
#include "Wizards/WzShare.h"
//#include "Wizards/WzSetIP.h"
//#include "Wizards/WzNetActive.h"

class FlyWizard : public CPropertySheetImpl<FlyWizard>
{
public:
    // Construction
	FlyWizard ( HWND hWndParent = NULL ) : CPropertySheetImpl<FlyWizard> ( (LPCTSTR)NULL, 0, hWndParent ),
		m_bCentered(false)
	{
#if defined(USE_AERO_WIZARD)
#ifdef FLYLINKDC_SUPPORT_WIN_XP
		if (CompatibilityManager::isOsVistaPlus())
#endif
		{
			m_psh.pszCaption = CTSTRING(WIZARD_TITLE);
			m_psh.dwFlags |= 0x00004000;
		}
		else
#endif
			SetWizardMode();


		AddPage ( m_pgStartup );
		AddPage ( m_pgNick );
//		AddPage ( m_pgSetIP );
//		AddPage ( m_pgNetActive );
		AddPage ( m_pgDownload);
		AddPage ( m_pgShare);
//		AddPage ( m_pgFinish );

	}

    // Maps
    BEGIN_MSG_MAP(FlyWizard)
        MESSAGE_HANDLER_EX(WM_CENTER_ALL, OnPageInit)
        CHAIN_MSG_MAP(CPropertySheetImpl<FlyWizard>)
    END_MSG_MAP()

    // Message handlers
    LRESULT OnPageInit ( UINT uMsg, WPARAM wParam ,LPARAM lParam )
	{
		if ( !m_bCentered )
		{
			m_bCentered = true;
			CenterWindow ( m_psh.hwndParent );
		}

		return 0;
	}
    // Property pages
	WzStartup m_pgStartup;
	WzDownload m_pgDownload;
//	WzFinish m_pgFinish;
	WzNick m_pgNick;
	WzShare m_pgShare;
//	WzSetIP m_pgSetIP;
//	WzNetActive m_pgNetActive;
    // Implementation
protected:
    bool m_bCentered;
};

#endif // SSA_WIZARD_FEATURE

#endif // _FLY_WIZARD_H_