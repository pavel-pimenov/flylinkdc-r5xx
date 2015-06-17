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

#if !defined(_INT_PREVIEW_PAGE_H_)
#define _INT_PREVIEW_PAGE_H_

#pragma once

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h"

#ifdef SSA_VIDEO_PREVIEW_FEATURE

class IntPreviewPage: public CPropertyPage<IDD_INT_PREVIEW_PAGE>, public PropPage
{
	public:
		explicit IntPreviewPage(SettingsManager *s) : PropPage(s, TSTRING(SETTINGS_DOWNLOADS) + _T('\\') + TSTRING(SETTINGS_AVIPREVIEW) + _T('\\') + TSTRING(SETTINGS_INT_PREVIEW_PAGE))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~IntPreviewPage()
		{
			ctrlPrevlist.Detach(); // [+] IRainman
		}
		
		BEGIN_MSG_MAP_EX(IntPreviewPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		NOTIFY_HANDLER(IDC_INT_PREVIEW_LIST, NM_CUSTOMDRAW, ctrlPrevlist.onCustomDraw) // [+] IRainman
		COMMAND_ID_HANDLER(IDC_BTN_SELECT, OnBrowseClick)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnBrowseClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		
		void write();
		void cancel() {}
	protected:
	
		ExListViewCtrl ctrlPrevlist; // [+] IRainman
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[]; // IDC_INT_PREVIEW_LIST
		
		
};
#else
class IntPreviewPage : public EmptyPage
{
	public:
		IntPreviewPage(SettingsManager *s) : EmptyPage(s, TSTRING(SETTINGS_DOWNLOADS) + _T('\\') + TSTRING(SETTINGS_AVIPREVIEW) + _T('\\') + TSTRING(SETTINGS_INT_PREVIEW_PAGE))
		{
		}
};

#endif // SSA_VIDEO_PREVIEW_FEATURE

#endif // _INT_PREVIEW_PAGE_H_