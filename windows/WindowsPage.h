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

#if !defined(WINDOWS_PAGE_H)
#define WINDOWS_PAGE_H

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h" // [+] IRainman

class WindowsPage : public CPropertyPage<IDD_WINDOWS_PAGE>, public PropPage
{
	public:
		explicit WindowsPage() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_WINDOWS))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~WindowsPage()
		{
			ctrlStartup.Detach(); // [+] IRainman
			ctrlOptions.Detach(); // [+] IRainman
			ctrlConfirms.Detach(); // [+] IRainman
		}
		
		BEGIN_MSG_MAP(WindowsPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_WINDOWS_STARTUP, NM_CUSTOMDRAW, ctrlStartup.onCustomDraw) // [+] IRainman
		NOTIFY_HANDLER(IDC_WINDOWS_OPTIONS, NM_CUSTOMDRAW, ctrlOptions.onCustomDraw) // [+] IRainman
		NOTIFY_HANDLER(IDC_CONFIRM_OPTIONS, NM_CUSTOMDRAW, ctrlConfirms.onCustomDraw) // [+] IRainman
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
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
	
		static TextItem textItem[];
		static Item items[];
		static ListItem listItems[];
		static ListItem optionItems[];
		static ListItem confirmItems[]; // [+] InfinitySky.
		
		
		
		ExListViewCtrl ctrlStartup, ctrlOptions, ctrlConfirms; // [+] IRainman
};

#endif // !defined(WINDOWS_PAGE_H)
