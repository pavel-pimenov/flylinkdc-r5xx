/*
 * Copyright (C) 2010-2017 FlylinkDC++ Team http://flylinkdc.com
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

#if !defined(RSS_PAGE_H)
#define RSS_PAGE_H

#pragma once


#ifdef IRAINMAN_INCLUDE_RSS

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h"

class RSSFeed;

class RSSPage : public CPropertyPage<IDD_RSS_PAGE>, public PropPage
{
	public:
		explicit RSSPage() : PropPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_RSS_PROP))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~RSSPage()
		{
			ctrlCommands.Detach();
		}
		BEGIN_MSG_MAP_EX(RSSPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_ADD_RSS, onAddFeed)
		COMMAND_ID_HANDLER(IDC_REMOVE_RSS, onRemoveFeed)
		COMMAND_ID_HANDLER(IDC_CHANGE_RSS, onChangeFeed)
		NOTIFY_HANDLER(IDC_RSS_ITEMS, LVN_ITEMCHANGED, onItemchangedFeeds)
		NOTIFY_HANDLER(IDC_RSS_ITEMS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_RSS_ITEMS, NM_DBLCLK, onDoubleClick)
		NOTIFY_HANDLER(IDC_RSS_ITEMS, NM_CUSTOMDRAW, ctrlCommands.onCustomDraw)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onAddFeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onItemchangedFeeds(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onChangeFeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemoveFeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
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
		typedef std::unordered_map<wstring, string> CodeingMap;
		typedef pair<wstring, string> CodeingMapPair;
		
		
	protected:
		ExListViewCtrl ctrlCommands;
		
		static Item items[];
		static TextItem texts[];
		
		
		void addRSSEntry(const RSSFeed* rf, int pos);
		
	private:
	
		CodeingMap m_CodeingList;
		const wstring& GetCodeingFromMapName(const int p_LangNum)
		{
			int i = 0;
			for (auto l_lang = m_CodeingList.cbegin(); l_lang != m_CodeingList.cend(); ++l_lang, ++i)
				if (p_LangNum == i)
					return l_lang->first;
			return Util::emptyStringT;
		}
		
};
#else
class RSSPage : public EmptyPage
{
	public:
		RSSPage() : EmptyPage(TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_RSS_PROP))
		{
		}
};
#endif // IRAINMAN_INCLUDE_RSS

#endif //RSS_PAGE_H