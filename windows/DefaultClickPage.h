/*
 * Copyright (C) 2010-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#ifndef DEFAULT_CLICK_H
#define DEFAULT_CLICK_H

#pragma once

#include "PropPage.h"

class DefaultClickPage : public CPropertyPage<IDD_DEFAULT_CLICK_PAGE>, public PropPage
{
	public:
		explicit DefaultClickPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_DEFAULT_CLICK))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~DefaultClickPage()
		{
		}
		
		BEGIN_MSG_MAP(DefaultClickPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
//		COMMAND_ID_HANDLER(IDC_TTH_IN_STREAM, onFixControls)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
//		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

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
		//  static Item items[];
		static TextItem texts[];
		
		CComboBox userlistaction, transferlistaction, chataction
		, favuserlistaction // !SMT!-UI
		, magneturllistaction;
		
		
};

#endif // DEFAULT_CLICK_H

/**
 * @file
 * $Id: DownloadPage.h,v 1.00 2010/07/12 12:07:24 NightOrion
 */
