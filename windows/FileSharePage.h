/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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


#if !defined(_FILE_SHARE_PAGE_H_)
#define _FILE_SHARE_PAGE_H_

#pragma once

#ifdef SSA_INCLUDE_FILE_SHARE_PAGE

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h"

class FileSharePage : public CPropertyPage<IDD_FILE_SHARE_PAGE>, public PropPage
{
	public:
		explicit FileSharePage() : PropPage(TSTRING(SETTINGS_UPLOADS) + _T('\\') + TSTRING(FILESHARE_TITLE))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~FileSharePage()
		{
			ctrlList.Detach();
		}
		
		BEGIN_MSG_MAP_EX(FileSharePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_FILESHARE_BOOLEANS, NM_CUSTOMDRAW, ctrlList.onCustomDraw)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		
		void write();
		
	protected:
	
	
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
		ExListViewCtrl ctrlList;
		
};
#else
class FileSharePage : public EmptyPage
{
	public:
		FileSharePage() : EmptyPage(TSTRING(SETTINGS_UPLOADS) + _T('\\') + TSTRING(FILESHARE_TITLE))
		{
		}
};
#endif // SSA_INCLUDE_FILE_SHARE_PAGE

#endif //_FILE_SHARE_PAGE_H_

/**
 * @file
 * $Id: FileSharePage.h 8089 2011-09-03 21:57:11Z a.rainman $
 */
