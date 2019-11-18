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

/*
 * Automatic Directory Listing Search
 * Henrik Engstr�m, henrikengstrom on home point se
 */


#if !defined(ADL_SEARCH_FRAME_H)
#define ADL_SEARCH_FRAME_H


#pragma once


#include "FlatTabCtrl.h"
#include "ExListViewCtrl.h"

#include "../client/ADLSearch.h"

///////////////////////////////////////////////////////////////////////////////
//
//  Class that represent an ADL search manager interface
//
///////////////////////////////////////////////////////////////////////////////
class ADLSearchFrame : public MDITabChildWindowImpl < ADLSearchFrame, RGB(0, 0, 0), IDR_ADLSEARCH >
	, public StaticFrame<ADLSearchFrame, ResourceManager::ADL_SEARCH, IDC_FILE_ADL_SEARCH>
	, private SettingsManagerListener
#ifdef _DEBUG
	, boost::noncopyable
#endif
{
	public:
	
		// Base class typedef
		typedef MDITabChildWindowImpl < ADLSearchFrame, RGB(0, 0, 0), IDR_ADLSEARCH > baseClass;
		
		// Constructor/destructor
		ADLSearchFrame()  {}
		~ADLSearchFrame() { }
		
		// Frame window declaration
		DECLARE_FRAME_WND_CLASS_EX(_T("ADLSearchFrame"), IDR_ADLSEARCH, 0, COLOR_3DFACE);
		
		// Inline message map
		BEGIN_MSG_MAP(ADLSearchFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_ADD, onAdd)
		COMMAND_ID_HANDLER(IDC_EDIT, onEdit)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_HELP_FAQ, onHelp)
		COMMAND_ID_HANDLER(IDC_MOVE_UP, onMoveUp)
		COMMAND_ID_HANDLER(IDC_MOVE_DOWN, onMoveDown)
		NOTIFY_HANDLER(IDC_ADLLIST, NM_CUSTOMDRAW, ctrlList.onCustomDraw)
		NOTIFY_HANDLER(IDC_ADLLIST, NM_DBLCLK, onDoubleClickList)
		NOTIFY_HANDLER(IDC_ADLLIST, LVN_ITEMCHANGED, onItemChanged)
		NOTIFY_HANDLER(IDC_ADLLIST, LVN_KEYDOWN, onKeyDown)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		// Message handlers
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onAdd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEdit(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onRemove(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onHelp(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onMoveUp(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onMoveDown(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onDoubleClickList(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onChar(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		
		// Update colors
		LRESULT onCtlColor(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			HWND hWnd = (HWND)lParam;
			HDC hDC   = (HDC)wParam;
			if (hWnd == ctrlList.m_hWnd)
			{
				return Colors::setColor(hDC);
			}
			bHandled = FALSE;
			return FALSE;
		}
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		// Update control layouts
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
	private:
	
		// Communication with manager
		void LoadAll();
		void UpdateSearch(size_t index, BOOL doDelete = TRUE);
		
		// Contained controls
		ExListViewCtrl ctrlList;
		CButton ctrlAdd;
		CButton ctrlEdit;
		CButton ctrlRemove;
		CButton ctrlMoveUp;
		CButton ctrlMoveDown;
		CButton ctrlHelp;
		CMenu contextMenu;
		
		// Column order
		enum
		{
			COLUMN_FIRST = 0,
			COLUMN_ACTIVE_SEARCH_STRING = COLUMN_FIRST,
			COLUMN_SOURCE_TYPE,
			COLUMN_DEST_DIR,
			COLUMN_MIN_FILE_SIZE,
			COLUMN_MAX_FILE_SIZE,
			COLUMN_LAST
		};
		
		// Column parameters
		static int columnIndexes[];
		static int columnSizes[];
		void on(SettingsManagerListener::Repaint) override;
};

#endif // !defined(ADL_SEARCH_FRAME_H)

/**
 * @file
 * $Id: ADLSearchFrame.h,v 1.12 2006/06/15 20:14:14 bigmuscle Exp $
 */
