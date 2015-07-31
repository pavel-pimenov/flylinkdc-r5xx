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

#ifndef PRIORITY_PAGE_H
#define PRIORITY_PAGE_H

#pragma once

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h"

class PriorityPage : public CPropertyPage<IDD_PRIORITY_PAGE>, public PropPage
{
	public:
		explicit PriorityPage( ) : PropPage( TSTRING(SETTINGS_DOWNLOADS) + _T('\\') + TSTRING(PRIORITY))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~PriorityPage()
		{
		}
		
		BEGIN_MSG_MAP(PriorityPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_USE_AUTOPRIORITY, onChangeCont) // [+] InfinitySky.
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onChangeCont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+] InfinitySky.
		
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
	protected:
		static Item items[];
		static TextItem texts[];
		
		void fixControls(); // [+] InfinitySky.
};

#endif // PRIORITY_PAGE_H