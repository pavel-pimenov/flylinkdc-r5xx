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

#ifndef PROXY_PAGE_H
#define PROXY_PAGE_H


#pragma once

#include "PropPage.h"

class ProxyPage : public CPropertyPage<IDD_PROXY_PAGE>, public PropPage
{
	public:
		explicit ProxyPage() : PropPage(TSTRING(SETTINGS_NETWORK) + _T('\\') + TSTRING(SETTINGS_PROXY))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~ProxyPage()
		{
		}
		
		BEGIN_MSG_MAP(ProxyPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_DIRECT_OUT, onClickedDirect)
		COMMAND_ID_HANDLER(IDC_SOCKS5, onClickedDirect)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedDirect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
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
		CEdit desc;
		
	protected:
		static Item items[];
		static TextItem texts[];
		
		
};

#endif // PROXY_PAGE_H

