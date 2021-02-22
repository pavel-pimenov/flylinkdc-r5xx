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

#if !defined(SPY_FRAME_H)
#define SPY_FRAME_H

#pragma once

#include "../client/ClientManager.h"
#include "../client/ShareManager.h"

#include "FlatTabCtrl.h"
#include "ExListViewCtrl.h"

#define SPYFRAME_IGNORETTH_MESSAGE_MAP 7
#define SPYFRAME_SHOW_NICK 8
#define SPYFRAME_LOG_FILE 9

class SpyFrame : public MDITabChildWindowImpl < SpyFrame, RGB(0, 0, 0), IDR_SPY >, public StaticFrame<SpyFrame, ResourceManager::SEARCH_SPY, IDC_SEARCH_SPY>,
	private ClientManagerListener,
	private SettingsManagerListener,
	virtual private CFlyTimerAdapter,
	virtual private CFlyTaskAdapter
{
	public:
		SpyFrame();
		~SpyFrame() { }
		
		enum
		{
			COLUMN_FIRST,
			COLUMN_STRING = COLUMN_FIRST,
			COLUMN_COUNT,
			COLUMN_USERS,
			COLUMN_TIME,
			COLUMN_SHARE_HIT,
			COLUMN_LAST
		};
		
		static int columnIndexes[COLUMN_LAST];
		static int columnSizes[COLUMN_LAST];
		
		DECLARE_FRAME_WND_CLASS_EX(_T("SpyFrame"), IDR_SPY, 0, COLOR_3DFACE)
		
		typedef MDITabChildWindowImpl < SpyFrame, RGB(0, 0, 0), IDR_SPY > baseClass;
		BEGIN_MSG_MAP(SpyFrame)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		COMMAND_ID_HANDLER(IDC_SEARCH, onSearch)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_COLUMNCLICK, onColumnClickResults)
		NOTIFY_HANDLER(IDC_RESULTS, NM_CUSTOMDRAW, onCustomDraw)
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(SPYFRAME_IGNORETTH_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onIgnoreTth)
		ALT_MSG_MAP(SPYFRAME_SHOW_NICK)
		MESSAGE_HANDLER(BM_SETCHECK, onShowNick)
		ALT_MSG_MAP(SPYFRAME_LOG_FILE)
		MESSAGE_HANDLER(BM_SETCHECK, onLogFile)
		END_MSG_MAP()
		
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onColumnClickResults(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
		LRESULT onShowNick(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			m_showNick = (wParam == BST_CHECKED);
			return 0;
		}
		
		LRESULT onLogFile(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			m_LogFile = (wParam == BST_CHECKED);
			return 0;
		}
		
		LRESULT onIgnoreTth(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			m_ignoreTTH = (wParam == BST_CHECKED);
			return 0;
		}
		
	private:
		enum
		{
			SEARCH,
			TICK_AVG,
			SAVE_LOG
		};
		
		ExListViewCtrl ctrlSearches;
		CStatusBarCtrl ctrlStatus;
		CContainedWindow m_ignoreTTHContainer;
		CContainedWindow m_ShowNickContainer;
		CContainedWindow m_SpyLogFileContainer;
		CButton m_ctrlIgnoreTTH;
		CButton m_ctrlShowNick;
		CButton m_ctrlSpyLogFile;
		uint64_t m_total;
		uint8_t m_current;
		static const uint8_t AVG_TIME = 60;
		std::array<uint16_t, AVG_TIME> m_perSecond;
		bool m_needsResort;
		
		tstring m_searchString;
		
		File* m_log;
		string m_log_txt;
		tstring m_CurrentTime;
		bool m_needsUpdateTime;
		
		struct Stats : public Task
		{
			uint16_t m_perS;
			uint16_t m_perM;
			Stats(): m_perS(0), m_perM(0)
			{
			}
		};
		
		bool m_ignoreTTH;
		bool m_showNick;
		bool m_LogFile;
		
		static const size_t NUM_SEEKERS = 8;
		struct SearchData
		{
				SearchData() : m_curpos(0), m_i(1) { }
				size_t m_i;
				string m_seekers[NUM_SEEKERS];
				
				void AddSeeker(const string& s)
				{
					m_seekers[m_curpos++] = s;
					m_curpos = m_curpos % NUM_SEEKERS;
				}
			private:
				size_t m_curpos;
		};
		
		typedef std::unordered_map<string, SearchData> SpySearchMap;
		
		SpySearchMap m_spy_searches;
		
		// ClientManagerListener
		struct SMTSearchInfo : public Task
		{
			explicit SMTSearchInfo(const string& p_user, const string& p_s, ClientManagerListener::SearchReply p_re) :
				seeker(p_user), s(p_s),  re(p_re) { }
			string seeker;
			string s;
			ClientManagerListener::SearchReply re;
		};
		
		// ClientManagerListener
		void on(ClientManagerListener::IncomingSearch, const string& user, const string& s, ClientManagerListener::SearchReply re) noexcept override;
		
		void on(SettingsManagerListener::Repaint) override;
};

#endif // !defined(SPY_FRAME_H)

/**
 * @file
 * $Id: SpyFrame.h,v 1.23 2006/10/13 20:04:32 bigmuscle Exp $
 */
