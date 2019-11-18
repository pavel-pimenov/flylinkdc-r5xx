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

#if !defined(PROP_PAGE_H)
#define PROP_PAGE_H

#pragma once


#include "DimEdit.h"
#ifdef SCALOLAZ_PROPPAGE_COLOR
#include "ResourceLoader.h"
#include "WinUtil.h"
#endif

class SettingsManager;
extern SettingsManager *g_settings;
class PropPage
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		PropPage(const wstring& p_title) : m_title(p_title)
#ifdef _DEBUG
			, m_check_read_write(0)
#endif
#ifdef SCALOLAZ_PROPPAGE_COLOR
			, m_hDialogBrush(0)
#endif
		{
		}
		virtual ~PropPage()
		{
#ifdef SCALOLAZ_PROPPAGE_COLOR
			if (m_hDialogBrush)
				DeleteObject(m_hDialogBrush);
#endif
			dcassert(m_check_read_write == 0);
		}
		
		virtual PROPSHEETPAGE *getPSP() = 0;
		virtual void write() = 0;
		virtual void cancel() = 0;
		enum Type { T_STR, T_INT, T_BOOL, T_CUSTOM, T_END };
		
		BEGIN_MSG_MAP_EX(PropPage)
#ifdef SCALOLAZ_PROPPAGE_COLOR
		MESSAGE_HANDLER(WM_CTLCOLORDLG, OnCtlColorDlg)
		MESSAGE_HANDLER(WM_CTLCOLORBTN, OnCtlColorDlg)
		MESSAGE_HANDLER(WM_CTLCOLORSCROLLBAR, OnCtlColorDlg)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
		MESSAGE_HANDLER(WM_CTLCOLORMSGBOX, OnCtlColorDlg)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnCtlColorDlg)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, OnCtlColorDlg)
		// MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
#endif
		END_MSG_MAP()
#ifdef SCALOLAZ_PROPPAGE_COLOR
		LRESULT OnCtlColorDlg(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT OnCtlColorStatic(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		/*
		LRESULT OnEraseBackground(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		*/
		HBRUSH m_hDialogBrush;
#endif
		
		struct Item
		{
			WORD itemID;
			int setting;
			Type type;
		};
		struct ListItem
		{
			int setting;
			ResourceManager::Strings desc;
		};
		struct TextItem
		{
			WORD itemID;
			ResourceManager::Strings translatedString;
		};
		
		
	protected:
		wstring m_title;
		void read(HWND page, Item const* items, ListItem* listItems = NULL, HWND list = NULL);
		void write(HWND page, Item const* items, ListItem* listItems = NULL, HWND list = NULL);
		void cancel(HWND page);
		void cancel_check()
		{
#ifdef _DEBUG
			dcassert(m_check_read_write > 0);
			m_check_read_write = 0;
#endif
		}
		void translate(HWND page, TextItem* textItems);
#ifdef _DEBUG
	protected:
		int m_check_read_write;
#endif
};

class EmptyPage : public CPropertyPage<IDD_EMPTY_PAGE>, public PropPage
{
	public:
		EmptyPage(const tstring& p_title) : PropPage(p_title)
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		BEGIN_MSG_MAP_EX(EmptyPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			PropPage::translate((HWND)(*this), texts);
			return TRUE;
		}
		
		virtual ~EmptyPage()
		{
		}
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write() {}
		void cancel()
		{
			cancel_check();
		}
	protected:
		static TextItem texts[];
};

#endif // !defined(PROP_PAGE_H)

/**
 * @file
 * $Id: PropPage.h,v 1.9 2006/05/08 08:36:19 bigmuscle Exp $
 */
