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

#if !defined(RSS_NEWS_FRAME_H)
#define RSS_NEWS_FRAME_H

#pragma once

#ifdef IRAINMAN_INCLUDE_RSS

#include "FlatTabCtrl.h"
#include "WinUtil.h"
#include "TypedListViewCtrl.h"
#include "../FlyFeatures/RSSManager.h"

class RSSNewsFrame : public MDITabChildWindowImpl < RSSNewsFrame, RGB(0, 0, 0), IDR_RSS > , public StaticFrame<RSSNewsFrame, ResourceManager::RSS_NEWS, IDC_RSS>,
	private SettingsManagerListener, private RSSListener
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		DECLARE_FRAME_WND_CLASS_EX(_T("RSSNewsFrame"), IDR_RSS, 0, COLOR_3DFACE);
		
		RSSNewsFrame()
		{
		}
		~RSSNewsFrame() { }
		
		typedef MDITabChildWindowImpl < RSSNewsFrame, RGB(0, 0, 0), IDR_RSS > baseClass;
		
		BEGIN_MSG_MAP(RSSNewsFrame)
		MESSAGE_HANDLER(WM_SETFOCUS, OnFocus)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		COMMAND_ID_HANDLER(IDC_REMOVE_ALL, onRemoveAll)
		NOTIFY_HANDLER(IDC_RSS, LVN_GETDISPINFO, ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(IDC_RSS, LVN_COLUMNCLICK, ctrlList.onColumnClick)
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		NOTIFY_HANDLER(IDC_RSS, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
#endif
		CHAIN_MSG_MAP(baseClass)
		NOTIFY_HANDLER(IDC_RSS, NM_DBLCLK, onDoubleClick)
		COMMAND_ID_HANDLER(IDC_OPEN_FILE, onOpenClick)      // Go to the URL
		COMMAND_ID_HANDLER(IDC_COPY_URL, onCopy)            // Copy URL
		COMMAND_ID_HANDLER(IDC_COPY_ACTUAL_LINE, onCopy)    // Copy Name + URL
		END_MSG_MAP()
		
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		// LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		LRESULT OnFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlList.SetFocus();
			return 0;
		}
		
		LRESULT onRemoveAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onOpenClick(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	protected:
		enum
		{
			COLUMN_FIRST,
			COLUMN_TITLE = COLUMN_FIRST,
			COLUMN_URL,
			COLUMN_DESC,
			COLUMN_DATE,
			COLUMN_AUTHOR,
			COLUMN_CATEGORY,
			COLUMN_SOURCE,
			COLUMN_LAST
		};
		
		class RSSItemInfo
#ifdef _DEBUG
			: boost::noncopyable // [+] IRainman fix.
#endif
			
		{
				tstring m_columns[COLUMN_LAST];
			public:
				RSSItemInfo(const RSSItem* p_fi) : m_entry(p_fi)
				{
					m_columns[COLUMN_TITLE]  = Text::toT(m_entry->getTitle());
					m_columns[COLUMN_URL]  = Text::toT(m_entry->getUrl());
					m_columns[COLUMN_DESC]  = Util::eraseHtmlTags(Text::toT(m_entry->getDesc()));
					m_columns[COLUMN_DATE]  = Text::toT(Util::formatDigitalClock(m_entry->getPublishDate()));
					m_columns[COLUMN_AUTHOR]   = Text::toT(m_entry->getAuthor());
					m_columns[COLUMN_CATEGORY]  = Text::toT(m_entry->getCategory());
					m_columns[COLUMN_SOURCE] = Text::toT(m_entry->getSource());
				}
				
				const tstring& getText(int p_col) const
				{
					dcassert(p_col >= 0 && p_col < COLUMN_LAST);
					return m_columns[p_col];
				}
				
				static int compareItems(const RSSItemInfo* a, const RSSItemInfo* b, int col)
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					switch (col)
					{
						case COLUMN_DATE:
							return compare(b->m_entry->getPublishDate(), a->m_entry->getPublishDate());
						default:
							return Util::DefaultSort(a->m_columns[col].c_str(), b->m_columns[col].c_str());
					}
				}
				
				static int getImageIndex()
				{
					return 0;
				}
				static uint8_t getStateImageIndex()
				{
					return 0;
				}
				
				const RSSItem* m_entry;
		};
		
		void addRSSEntry(const RSSItem* p_entry)
		{
			const RSSItemInfo *l_ii = new RSSItemInfo(p_entry);
			const int l_loc = ctrlList.insertItem(l_ii, 0);
			ctrlList.EnsureVisible(l_loc, FALSE);
		}
		
	private:
	
		string m_Message;
		CButton ctrlRemoveAll;
		OMenu copyMenu;
		TypedListViewCtrl<RSSItemInfo, IDC_RSS> ctrlList;
		
		
		SettingsManager::StrSetting columnVisible;
		
		static int columnSizes[COLUMN_LAST];
		static int columnIndexes[COLUMN_LAST];
		
		void on(SettingsManagerListener::Repaint) override;
		void on(RSSListener::Added, const RSSItem*) noexcept override;
		void on(RSSListener::NewRSS, const unsigned int) noexcept override;
		LRESULT updateList(const RSSManager::NewsList& list);
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		
};
static ResourceManager::Strings rss_columnNames[] = {  ResourceManager::RSS_TITLE, ResourceManager::RSS_URL,  ResourceManager::DESCRIPTION, ResourceManager::TIME,
                                                       ResourceManager::RSS_AUTHOR, ResourceManager::RSS_CATEGORY, ResourceManager::RSS_SOURCE
                                                    };


#endif // !defined(RSS_NEWS_FRAME_H)

#endif // IRAINMAN_INCLUDE_RSS