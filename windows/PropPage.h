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

#if !defined(PROP_PAGE_H)
#define PROP_PAGE_H

#include "DimEdit.h"
#include "resource.h"
#ifdef SCALOLAZ_PROPPAGE_COLOR
#include "ResourceLoader.h"
#include "WinUtil.h"
#endif
#define DIM_EDIT_EXPERIMENT 0

class SettingsManager;
#include "../client/ResourceManager.h"

class PropPage
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		PropPage(SettingsManager *src) : settings(src)
#ifdef SCALOLAZ_PROPPAGE_COLOR
			, m_hDialogBrush(0)
#endif
		{ }
		virtual ~PropPage()
		{
#ifdef SCALOLAZ_PROPPAGE_COLOR
			if (m_hDialogBrush)
				DeleteObject(m_hDialogBrush);
#endif
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
		LRESULT OnCtlColorDlg(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			if (BOOLSETTING(SETTINGS_WINDOW_COLORIZE))
			{
				m_hDialogBrush = CreateSolidBrush(Colors::bgColor /*GetSysColor(COLOR_BTNFACE)*/); // [!] IRainman fix.
				return (long) m_hDialogBrush;
			}
			else
			{
				return 0;
			}
		}
		LRESULT OnCtlColorStatic(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			if (BOOLSETTING(SETTINGS_WINDOW_COLORIZE))
			{
				HDC hdc = (HDC)wParam;
				SetBkMode(hdc, TRANSPARENT);
				return (long) m_hDialogBrush;
			}
			else
			{
				return 0;
			}
		}
		/*
		LRESULT OnEraseBackground(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
		    if (BOOLSETTING(SETTINGS_WINDOW_COLORIZE))
		    {
		        //  m_hDialogBrush = CreateSolidBrush( RGB(250, 250, 250) );    // Custom BG color
		        return 1;
		    }
		    else
		    {
		        return 0;
		    }
		}
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
	
#if DIM_EDIT_EXPERIMENT
		std::map<WORD, CDimEdit *> ctrlMap;
#endif
		SettingsManager *settings;
		void read(HWND page, Item const* items, ListItem* listItems = NULL, HWND list = NULL);
		void write(HWND page, Item const* items, ListItem* listItems = NULL, HWND list = NULL);
		void cancel(HWND page);
		void translate(HWND page, TextItem* textItems);
};

class EmptyPage : public CPropertyPage<IDD_EMPTY_PAGE>, public PropPage // [+] IRainman HE
{
	public:
		EmptyPage(SettingsManager *s, const tstring& p_title) : PropPage(s), title(p_title)
		{
			SetTitle(title.c_str());
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
		void cancel() {}
	protected:
		static TextItem texts[];
		wstring title;
};

#endif // !defined(PROP_PAGE_H)

/**
 * @file
 * $Id: PropPage.h,v 1.9 2006/05/08 08:36:19 bigmuscle Exp $
 */
