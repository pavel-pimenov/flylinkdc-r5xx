/*
 * Copyright (C) 2012-2017 FlylinkDC++ Team http://flylinkdc.com
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

#ifdef IRAINMAN_INCLUDE_RSS

#if !defined(RSSSETFEED_DLG_H)
#define RSSSETFEED_DLG_H

#pragma once


#include "WinUtil.h"


class RSS_SetFeedDlg : public CDialogImpl<RSS_SetFeedDlg>
{
		CEdit ctrlURL;
		CEdit ctrlName;
		CComboBox ctrlCodeing;
		CButton ctrlEnable;
		
		typedef std::unordered_map<wstring, string> CodeingMap;
		typedef pair<wstring, string> CodeingMapPair;
		
	public:
		tstring m_strURL;
		tstring m_strName;
		tstring m_strCodeing;
		bool m_enabled;
		
		enum { IDD = IDD_RSS_SET_FEED };
		
		BEGIN_MSG_MAP(RSS_SetFeedDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		END_MSG_MAP()
		
		RSS_SetFeedDlg() : m_strURL(BaseUtil::emptyStringT), m_strName(BaseUtil::emptyStringT), m_strCodeing(BaseUtil::emptyStringT), m_enabled(FALSE) { }
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlURL.SetFocus();
			return FALSE;
		}
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
	private:
		static WinUtil::TextItem texts[];
		CodeingMap m_CodeingList;
		
	protected:
		void FillCodeingCombo();
};

#endif //RSSSETFEED_DLG_H

#endif // IRAINMAN_INCLUDE_RSS