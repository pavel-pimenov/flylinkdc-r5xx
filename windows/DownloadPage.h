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

#if !defined(DOWNLOAD_PAGE_H)
#define DOWNLOAD_PAGE_H


#pragma once


#include <atlcrack.h>
#include "PropPage.h"

class DownloadPage : public CPropertyPage<IDD_DOWNLOAD_PAGE>, public PropPage
{
	public:
		explicit DownloadPage() : PropPage(TSTRING(SETTINGS_DOWNLOADS))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~DownloadPage() { }
		
		BEGIN_MSG_MAP_EX(DownloadPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_BROWSEDIR, onClickedBrowseDir)
		COMMAND_ID_HANDLER(IDC_BROWSETEMPDIR, onClickedBrowseTempDir)
		COMMAND_ID_HANDLER(IDC_SETTINGS_LIST_CONFIG, onClickedListConfigure)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedBrowseDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedBrowseTempDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedListConfigure(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
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
};

#endif //  !defined(DOWNLOAD_PAGE_H)

/**
 * @file
 * $Id: DownloadPage.h,v 1.12 2006/05/08 08:36:19 bigmuscle Exp $
 */
