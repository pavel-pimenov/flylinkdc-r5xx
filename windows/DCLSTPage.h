/*
 * Copyright (C) 2012-2015 FlylinkDC++ Team http://flylinkdc.com/
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

#if !defined(_DCLSHT_PAGE_H_)
#define _DCLSHT_PAGE_H_

#include <atlcrack.h>
#include "PropPage.h"

class DCLSTPage : public CPropertyPage<IDD_DCLS_PAGE>, public PropPage
{
	public:
		explicit DCLSTPage(SettingsManager *s) : PropPage(s, TSTRING(SETTINGS_DOWNLOADS) + _T('\\') + TSTRING(SETTINGS_DCLS_PAGE))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~DCLSTPage()
		{
		}
		
		BEGIN_MSG_MAP_EX(DCLSTPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_DCLS_USE, OnClickedUseDCLST)
		COMMAND_ID_HANDLER(IDC_DCLS_CREATE_IN_FOLDER, OnClickedDCLSTFolder)
		COMMAND_ID_HANDLER(IDC_DCLS_ANOTHER_FOLDER, OnClickedDCLSTFolder)
		COMMAND_ID_HANDLER(IDC_PREVIEW_BROWSE, OnBrowseClick)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnClickedUseDCLST(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnClickedDCLSTFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnBrowseClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		
		void write();
		void cancel() {}
		void EnableDCLST(BOOL isEnabled);
		void CheckDCLSTPath(BOOL isEnabled);
		
	protected:
	
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
		
		CComboBox magnetClick;
};

#endif // _DCLSHT_PAGE_H_

/**
 * @file
 * $Id: DCLSTPage.h 8089 2011-09-03 21:57:11Z a.rainman $
 */
