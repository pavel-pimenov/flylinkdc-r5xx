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

#if !defined(ADLS_PROPERTIES_H)
#define ADLS_PROPERTIES_H

#pragma once


class ADLSearch;

///////////////////////////////////////////////////////////////////////////////
//
//  Dialog for new/edit ADL searches
//
///////////////////////////////////////////////////////////////////////////////
class ADLSProperties : public CDialogImpl<ADLSProperties>
{
	public:
	
		// Constructor/destructor
		explicit ADLSProperties(ADLSearch *_search) : search(_search) { }
		~ADLSProperties() { }
		
		// Dilaog unique id
		enum { IDD = IDD_ADLS_PROPERTIES };
		
		// Inline message map
		BEGIN_MSG_MAP(ADLSProperties)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		END_MSG_MAP()
		
		// Message handlers
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
	private:
	
		// Current search
		ADLSearch* search;
		
		CEdit ctrlSearch;
		CEdit ctrlDestDir;
		CEdit ctrlMinSize;
		CEdit ctrlMaxSize;
		CButton ctrlActive;
		CButton ctrlAutoQueue;
		CComboBox ctrlSearchType;
		CComboBox ctrlSizeType;
		CComboBox cRaw;
};

#endif // !defined(ADLS_PROPERTIES_H)

/**
 * @file
 * $Id: ADLSProperties.h,v 1.11 2006/07/04 11:05:18 bigmuscle Exp $
 */
