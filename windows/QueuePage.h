/**
* Страница в настройках "Очередь" / Page "Queue".
*/

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

#if !defined(QUEUE_PAGE_H)
#define QUEUE_PAGE_H

#pragma once

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h"

class QueuePage : public CPropertyPage<IDD_QUEUE_PAGE>, public PropPage
{
	public:
		QueuePage(SettingsManager *s) : PropPage(s)
		{
			title = TSTRING(SETTINGS_DOWNLOADS) + _T('\\') + TSTRING(SETTINGS_QUEUE);
			SetTitle(title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~QueuePage()
		{
			ctrlList.Detach(); // [+] IRainman
		}
		
		BEGIN_MSG_MAP(QueuePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_OTHER_QUEUE_OPTIONS, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
		COMMAND_ID_HANDLER(IDC_MULTISOURCE, onClickedActive)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			fixControls();
			return 0;
		}
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel() {}
		
	private:
		static Item items[];
		static TextItem texts[];
		static ListItem optionItems[];
		wstring title;
		
		ExListViewCtrl ctrlList; // [+] IRainman
		
		CComboBox m_downlaskClick;
		void fixControls();
		
};

#endif // !defined(QUEUE_PAGE_H)