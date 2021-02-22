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

#if !defined(STATS_FRAME_H)
#define STATS_FRAME_H

#pragma once

#ifdef FLYLINKDC_USE_STATS_FRAME

#include "FlatTabCtrl.h"
#include "WinUtil.h"

#ifdef FLYLINKDC_USE_SHOW_UD_RATIO
#include "TypedListViewCtrl.h"
struct RatioInfo
{
	RatioInfo(const tstring& p_hub, const tstring& p_nick) :
		m_hub(p_hub), m_nickT(p_nick) { }
		
	const tstring& getText(int col) const
	{
		return col == 0 ? m_hub : BaseUtil::emptyStringT;
	}
	static int compareItems(const RatioInfo* a, const RatioInfo* b, int col)
	{
		return col == 0 ? Util::DefaultSort(a->m_hub.c_str(), b->m_hub.c_str()) : 0;
	}
	int getImageIndex()
	{
		return 0;
	}
	
	tstring m_hub;
	tstring m_nickT;
};
#endif

class StatsFrame : public MDITabChildWindowImpl < StatsFrame, RGB(0, 0, 0), IDR_NETWORK_STATISTICS_ICON >, public StaticFrame<StatsFrame, ResourceManager::NETWORK_STATISTICS, IDC_NET_STATS>
	, virtual private CFlyTimerAdapter
	, virtual private CFlyTaskAdapter
{
#ifdef FLYLINKDC_USE_SHOW_UD_RATIO
		TypedListViewCtrl<RatioInfo, IDC_UD_RATIO> ctrlRatio;
#endif
	public:
		StatsFrame();
		~StatsFrame() { }
#ifdef FLYLINKDC_USE_SHOW_UD_RATIO
		enum
		{
			COLUMN_HUB,
			COLUMN_IP,
			COLUMN_NICK,
			COLUMN_DOWNLOAD,
			COLUMN_UPLOAD,
			COLUMN_LAST
		};
		CContainedWindow ratioContainer;
		
		static int columnIndexes[COLUMN_LAST];
		static int columnSizes[COLUMN_LAST];
#endif
		
		static CFrameWndClassInfo& GetWndClassInfo()
		{
			static CFrameWndClassInfo wc =
			{
				{
					sizeof(WNDCLASSEX), 0, StartWindowProc,
					0, 0, NULL, NULL, NULL, NULL, NULL, _T("StatsFrame"), NULL
				},
				NULL, NULL, IDC_ARROW, TRUE, 0, _T(""), IDR_NETWORK_STATISTICS_ICON
			};
			
			return wc;
		}
		
		typedef MDITabChildWindowImpl < StatsFrame, RGB(0, 0, 0), IDR_NETWORK_STATISTICS_ICON > baseClass;
		BEGIN_MSG_MAP(StatsFrame)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SIZE, onSize)
#ifdef FLYLINKDC_USE_SHOW_UD_RATIO
		NOTIFY_HANDLER(IDC_UD_RATIO, NM_CUSTOMDRAW, ctrlRatio.onCustomDraw)
#endif
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
	private:
		// Pixels per second
		enum { PIX_PER_SEC = 2 };
		enum { LINE_HEIGHT = 10 };
		
		CBrush m_backgr;
		CPen m_UploadSocketPen;
		CPen m_DownloadSocketPen;
		CPen m_UploadsPen;
		CPen m_DownloadsPen;
		CPen m_foregr;
		
		struct Stat
		{
			Stat(unsigned int aScroll, int64_t aSpeed) : scroll(aScroll), speed(aSpeed) { }
			const unsigned int scroll;
			const int64_t speed;
		};
		typedef deque<Stat> StatList;
		typedef StatList::const_iterator StatIter;
		
		StatList m_UpSockets;
		StatList m_DownSockets;
		StatList m_Uploads;
		StatList m_Downloads;
		
		static int g_width;
		static int g_height;
		
		int twidth;
		
		uint64_t lastTick;
		uint64_t scrollTick;
		
		int64_t m_max;
		
		void drawLine(CDC& dc, const StatIter& begin, const StatIter& end, const CRect& rc, const CRect& crc);
		
		void drawLine(CDC& dc, StatList& lst, const CRect& rc, const CRect& crc)
		{
			drawLine(dc, lst.begin(), lst.end(), rc, crc);
		}
		static void findMax(const StatList& lst, StatIter& i, int64_t& mspeed)
		{
			for (i = lst.cbegin(); i != lst.cend(); ++i)
			{
				if (mspeed < i->speed)
					mspeed = i->speed;
			}
		}
		
		static void updateStatList(int64_t bspeed, StatList& lst, int scroll)
		{
			lst.push_front(Stat(scroll, bspeed));
			
			while ((int)lst.size() > ((g_width / PIX_PER_SEC) + 1))
			{
				lst.pop_back();
			}
		}
		
		//static void addTick(int64_t bdiff, uint64_t tdiff, StatList& lst, AvgList& avg, int scroll);
		static void addAproximatedSpeedTick(int64_t bspeed, StatList& lst, int scroll)
		{
			updateStatList(bspeed, lst, scroll);
		}
};

#endif
#endif // !defined(STATS_FRAME_H)

/**
 * @file
 * $Id: StatsFrame.h 308 2007-07-13 18:57:02Z bigmuscle $
 */
