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

#if !defined(SPY_FRAME_H)
#define SPY_FRAME_H

#pragma once

#include "../client/DCPlusPlus.h"
#include "../client/ClientManager.h"
#include "../client/ShareManager.h"

#include "FlatTabCtrl.h"
#include "ExListViewCtrl.h"

#define SPYFRAME_IGNORETTH_MESSAGE_MAP 7
#define SPYFRAME_SHOW_NICK 8

class SpyFrame : public MDITabChildWindowImpl < SpyFrame, RGB(0, 0, 0), IDR_SPY > , public StaticFrame<SpyFrame, ResourceManager::SEARCH_SPY, IDC_SEARCH_SPY>,
	private ClientManagerListener,
	private CFlyTimerAdapter,
	private SettingsManagerListener
#ifdef _DEBUG
	, virtual NonDerivable<SpyFrame>, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		SpyFrame() : CFlyTimerAdapter(m_hWnd), m_total(0), m_current(0),
			m_ignoreTTH(BOOLSETTING(SPY_FRAME_IGNORE_TTH_SEARCHES)),
			m_showNick(BOOLSETTING(SHOW_SEEKERS_IN_SPY_FRAME)),
			m_ignoreTTHContainer(WC_BUTTON, this, SPYFRAME_IGNORETTH_MESSAGE_MAP),
			m_ShowNickContainer(WC_BUTTON, this, SPYFRAME_SHOW_NICK)
#ifdef _BIG_BROTHER_MODE
			, m_tick(0), m_log(NULL)
#endif
			, m_needsUpdateTime(true), m_needsResort(false) //[+]IRainman refactoring SpyFrame
		{
			memzero(m_perSecond, sizeof(m_perSecond));
			ClientManager::getInstance()->addListener(this);
			SettingsManager::getInstance()->addListener(this);
		}
		
		~SpyFrame() { }
		
		enum
		{
			COLUMN_FIRST,
			COLUMN_STRING = COLUMN_FIRST,
			COLUMN_COUNT,
			COLUMN_USERS,
			COLUMN_TIME,
			COLUMN_SHARE_HIT, // !SMT!-S
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
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		NOTIFY_HANDLER(IDC_RESULTS, LVN_COLUMNCLICK, onColumnClickResults)
		NOTIFY_HANDLER(IDC_RESULTS, NM_CUSTOMDRAW, onCustomDraw) // !SMT!-S
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(SPYFRAME_IGNORETTH_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onIgnoreTth)
		ALT_MSG_MAP(SPYFRAME_SHOW_NICK)
		MESSAGE_HANDLER(BM_SETCHECK, onShowNick)
		END_MSG_MAP()
		
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onColumnClickResults(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/); // !SMT!-S
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		
		LRESULT onShowNick(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			m_showNick = (wParam == BST_CHECKED);
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
#ifdef _BIG_BROTHER_MODE
			SAVE_LOG
#endif
		};
		
		ExListViewCtrl ctrlSearches;
		CStatusBarCtrl ctrlStatus;
		CContainedWindow m_ignoreTTHContainer;
		CContainedWindow m_ShowNickContainer;
		CButton m_ctrlIgnoreTTH;
		CButton m_ctrlShowNick;
		uint64_t m_total;
		uint8_t m_current;
		static const uint8_t AVG_TIME = 60;
		uint16_t m_perSecond[AVG_TIME];
		bool m_needsResort;
		TaskQueue m_tasks;
		
		tstring m_searchString;
		
		//[+]IRainman refactoring SpyFrame
#ifdef _BIG_BROTHER_MODE
		File* m_log;
		string m_txt;
		int m_tick;
#endif
		tstring m_CurrentTime;
		bool m_needsUpdateTime;
		
		struct Stats : public Task
		{
			uint16_t perS;
			uint16_t perM;
		};
		//[~]IRainman
		
		bool m_ignoreTTH;
		bool m_showNick;
		
		// [-] FastCriticalSection cs; // [-] IRainman fix: all data to needs to be lock usde in one thread.
		
		static const size_t NUM_SEEKERS = 8;
		struct SearchData
		{
				SearchData() : curpos(0), i(0) { }
				size_t i;
				string seekers[NUM_SEEKERS];
				
				void AddSeeker(const string& s)
				{
					seekers[curpos++] = s;
					curpos = curpos % NUM_SEEKERS;
				}
			private:
				size_t curpos;
		};
		
		typedef boost::unordered_map<string, SearchData> SearchMap;
		
		SearchMap m_searches;
		
		// ClientManagerListener
		struct SMTSearchInfo : public Task
		{
			explicit SMTSearchInfo(const string& _user, const string& _s, ClientManagerListener::SearchReply _re) :
				seeker(_user), s(_s),  re(_re) { } // !SMT!-S
			string seeker;
			string s;
			ClientManagerListener::SearchReply re; // !SMT!-S
		};
		
		// void onSearchResult(string aSearchString); // !SMT!-S
		
		// ClientManagerListener
		void on(ClientManagerListener::IncomingSearch, const string& user, const string& s, ClientManagerListener::SearchReply re) noexcept; // !SMT!-S
		
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept;
};

#endif // !defined(SPY_FRAME_H)

/**
 * @file
 * $Id: SpyFrame.h,v 1.23 2006/10/13 20:04:32 bigmuscle Exp $
 */
