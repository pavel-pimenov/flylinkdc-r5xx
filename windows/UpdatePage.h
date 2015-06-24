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

#if !defined(UPDATE_PAGE_H)
#define UPDATE_PAGE_H


#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h" // [+] IRainman


class UpdatePage : public CPropertyPage<IDD_UPDATE_PAGE>, public PropPage
{
	public:
		explicit UpdatePage(SettingsManager *s) : PropPage(s, TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_AUTOUPDATE_PROP))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~UpdatePage()
		{
			ctrlComponents.Detach(); // [+] IRainman
			ctrlAutoupdates.Detach(); // [+] IRainman
		}
		
		BEGIN_MSG_MAP_EX(UpdatePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_AUTOUPDATE_LIST, NM_CUSTOMDRAW, ctrlComponents.onCustomDraw) // [+] IRainman
		NOTIFY_HANDLER(IDC_AUTOUPDATE_COMPONENTS, NM_CUSTOMDRAW, ctrlAutoupdates.onCustomDraw) // [+] IRainman
#ifndef AUTOUPDATE_NOT_DISABLE
		COMMAND_ID_HANDLER(IDC_AUTOUPDATE_USE, onClickedUseAutoUpdate)
#endif
		COMMAND_ID_HANDLER(IDC_AUTOUPDATE_USE_CUSTOM_SERVER, onClickedUseCustomURL)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#ifndef AUTOUPDATE_NOT_DISABLE
		LRESULT onClickedUseAutoUpdate(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onClickedUseCustomURL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
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
		void EnableAutoUpdate(BOOL isEnabled);
		
		void CheckUseCustomURL();
		
	protected:
	
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
		static ListItem listComponents[];
		
		
		CComboBox ctrlTime;
		
		ExListViewCtrl ctrlComponents, ctrlAutoupdates; // [+] IRainman
};

#endif // !defined(UPDATE_PAGE_H)