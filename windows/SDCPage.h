/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
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

#ifndef SDCPage_H
#define SDCPage_H

#pragma once


#include "PropPage.h"


class SDCPage : public CPropertyPage<IDD_SDC_PAGE>, public PropPage
{
	public:
		explicit SDCPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_ADVANCED3))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~SDCPage()
		{
			ctrlShutdownAction.Detach();
		}
		
		BEGIN_MSG_MAP(SDCPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		//COMMAND_ID_HANDLER(IDC_SEARCH_FORGET, onFixControls)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
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
	private:
		void fixControls();
	protected:
		static Item items[];
		static TextItem texts[];
		
		CComboBox ctrlShutdownAction;
		
		
};

#endif //SDCPage_H

/**
 * @file
 * $Id: SDCPage.h,v 1.5 2006/02/02 20:28:14 bigmuscle Exp $
 */

