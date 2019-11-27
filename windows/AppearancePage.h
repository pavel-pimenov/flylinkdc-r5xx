/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#if !defined(APPEARANCE_PAGE_H)
#define APPEARANCE_PAGE_H


#pragma once


#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h"
#include "../client/File.h"

class AppearancePage : public CPropertyPage<IDD_APPEARANCE_PAGE>, public PropPage
{
	public:
		explicit AppearancePage() : PropPage(TSTRING(SETTINGS_APPEARANCE))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~AppearancePage();
		
		BEGIN_MSG_MAP_EX(AppearancePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_APPEARANCE_BOOLEANS, NM_CUSTOMDRAW, ctrlList.onCustomDraw)
		COMMAND_HANDLER(IDC_TIMESTAMP_HELP, BN_CLICKED, onClickedHelp)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedHelp(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		
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
		static ListItem listItems[];
		
		ExListViewCtrl ctrlList;
		
		typedef std::unordered_map<wstring, string> ThemeMap;
		typedef pair<wstring, string> ThemePair;
		
		CComboBox ctrlTheme;
		ThemeMap m_ThemeList;
		
		void GetThemeList();
		
};

#endif // !defined(APPEARANCE_PAGE_H)