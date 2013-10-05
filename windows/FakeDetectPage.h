/*
 * Copyright (C) 2001-2003 Jacek Sieka, j_s@telia.com
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

#ifndef FakeDetect_H
#define FakeDetect_H

#pragma once

#include "../client/DCPlusPlus.h"

#ifdef IRAINMAN_ENABLE_AUTO_BAN

#include <atlcrack.h>
#include "PropPage.h"
#include "Resource.h"
#include "ExListViewCtrl.h" // [+] IRainman

class FakeDetect : public CPropertyPage<IDD_FAKEDETECT_PAGE>, public PropPage
{
	public:
		FakeDetect(SettingsManager *s) : PropPage(s)
		{
			title = TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_FAKEDETECT);
			SetTitle(title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		};
		
		~FakeDetect()
		{
			ctrlList.Detach(); // [+] IRainman
		};
		BEGIN_MSG_MAP(FakeDetect)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_FAKE_BOOLEANS, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel() {}
	protected:
		static Item items[];
		static TextItem texts[];
	protected:
		wstring title;
		static ListItem listItems[];
		
		ExListViewCtrl ctrlList; // [+] IRainman
};
#else
class FakeDetect : public EmptyPage
{
	public:
		FakeDetect(SettingsManager *s) : EmptyPage(s, TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_FAKEDETECT))
		{
		}
};
#endif // IRAINMAN_ENABLE_AUTO_BAN

#endif //FakeDetect_H

/**
 * @file
 * $Id: FakeDetect.h,v 1.8 2006/02/02 20:28:13 bigmuscle Exp $
 */

