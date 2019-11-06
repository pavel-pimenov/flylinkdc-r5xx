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

#include "stdafx.h"

#pragma warning(disable: 4458)
#ifdef FLYLINKDC_USE_STATS_FRAME
#include "Resource.h"
#include "StatsFrame.h"

#include "../client/UploadManager.h"
#include "../client/DownloadManager.h"
#include "MainFrm.h"


int StatsFrame::g_width = 0;
int StatsFrame::g_height = 0;

#ifdef FLYLINKDC_USE_SHOW_UD_RATIO
int StatsFrame::columnIndexes[] =
{
	COLUMN_HUB,
	COLUMN_NICK,
	COLUMN_DOWNLOAD,
	COLUMN_UPLOAD
};
int StatsFrame::columnSizes[] = { 300, 100, 100, 100 };

static ResourceManager::Strings columnNames[] = { ResourceManager::FILE, ResourceManager::TYPE, ResourceManager::EXACT_SIZE,
                                                  ResourceManager::SIZE, ResourceManager::TTH_ROOT, ResourceManager::PATH, ResourceManager::DOWNLOADED,
                                                  ResourceManager::ADDED, ResourceManager::BITRATE
                                                }; //TODO // !PPA!

#endif
StatsFrame::StatsFrame() : CFlyTimerAdapter(m_hWnd), CFlyTaskAdapter(m_hWnd), twidth(0), lastTick(MainFrame::getLastUpdateTick()), scrollTick(0),
#ifdef FLYLINKDC_USE_SHOW_UD_RATIO
	ratioContainer(WC_LISTVIEW, this, 0),
#endif
	m_max(1)
{
	m_backgr.CreateSolidBrush(Colors::g_bgColor);
	m_UploadsPen.CreatePen(PS_SOLID, 0, SETTING(UPLOAD_BAR_COLOR));
	m_UploadSocketPen.CreatePen(PS_DOT, 0, SETTING(UPLOAD_BAR_COLOR));
	m_DownloadsPen.CreatePen(PS_SOLID, 0, SETTING(DOWNLOAD_BAR_COLOR));
	m_DownloadSocketPen.CreatePen(PS_DOT, 0, SETTING(DOWNLOAD_BAR_COLOR));
	m_foregr.CreatePen(PS_SOLID, 0, Colors::g_textColor);
}

LRESULT StatsFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// [+]IRainman
	//DownloadManager::getInstance()->addListener(this);
	//UploadManager::getInstance()->addListener(this);
	// [~]IRainman
	
	create_timer(1000);
	
	SetFont(Fonts::g_font);
	
	bHandled = FALSE;
#ifdef FLYLINKDC_USE_SHOW_UD_RATIO
	
	ctrlRatio.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_UD_RATIO);
	ratioContainer.SubclassWindow(ctrlRatio);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlRatio);
	SET_LIST_COLOR(ctrlRatio);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (uint8_t j = 0; j < COLUMN_LAST; j++)
	{
		ctrlRatio.InsertColumn(j, CTSTRING_I(columnNames[j]), LVCFMT_LEFT, columnSizes[j], j);
	}
#endif
	return 1;
}

LRESULT StatsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		safe_destroy_timer();
		clear_and_destroy_task();
		// [+]IRainman
		//DownloadManager::getInstance()->removeListener(this);
		//UploadManager::getInstance()->removeListener(this);
		// [~]IRainman
		
		WinUtil::setButtonPressed(IDC_NET_STATS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		bHandled = FALSE;
		return 0;
	}
}

void StatsFrame::drawLine(CDC& dc, const StatIter& begin, const StatIter& end, const CRect& rc, const CRect& crc)
{
	int x = crc.right;
	
	StatIter i;
	for (i = begin; i != end; ++i)
	{
		if ((x - (int)i->scroll) < rc.right)
			break;
		x -= i->scroll;
	}
	if (i != end)
	{
		int y = /* [-] IRainman fix (max == 0) ? 0 :*/ (int)((i->speed * g_height) / m_max);
		dc.MoveTo(x, g_height - y);
		x -= i->scroll;
		++i;
		
		for (; i != end && x > twidth; ++i)
		{
			y = /* [-] IRainman fix (max == 0) ? 0 :*/ (int)((i->speed * g_height) / m_max);
			dc.LineTo(x, g_height - y);
			if (x < rc.left)
				break;
			x -= i->scroll;
		}
	}
}

LRESULT StatsFrame::onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (GetUpdateRect(NULL))
	{
		CPaintDC dc(m_hWnd);
		CRect rc(dc.m_ps.rcPaint);
//[-]PPA        dcdebug("Update: %d, %d, %d, %d\n", rc.left, rc.top, rc.right, rc.bottom);

		dc.SelectBrush(m_backgr);
		dc.BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), NULL, 0, 0, PATCOPY);
		
		CRect clientRC;
		GetClientRect(clientRC);
		
		dc.SetTextColor(Colors::g_textColor);
		dc.SetBkColor(Colors::g_bgColor);
		
		{
			CSelectPen l_pen(dc, m_foregr); //-V808
			{
				CSelectFont l_font(dc, Fonts::g_font); //-V808
				const int lines = g_height / (Fonts::g_fontHeight * LINE_HEIGHT);
				const int lheight = g_height / (lines + 1);
				
				for (int i = 0; i < lines; ++i)
				{
					int ypos = lheight * (i + 1);
					if (ypos > Fonts::g_fontHeight + 2)
					{
						dc.MoveTo(rc.left, ypos);
						dc.LineTo(rc.right, ypos);
					}
					
					if (rc.left <= twidth)
					{
					
						ypos -= Fonts::g_fontHeight + 2;
						if (ypos < 0)
							ypos = 0;
						if (g_height == 0)
							g_height = 1;
						const tstring txt = Util::formatBytesW(m_max * (g_height - ypos) / g_height) + _T('/') + WSTRING(S);
						const int tw = WinUtil::getTextWidth(txt, dc);
						if (tw + 2 > twidth)
							twidth = tw + 2;
						dc.TextOut(1, ypos, txt.c_str());
					}
				}
			}
			if (rc.left < twidth)
			{
				const tstring txt = Util::formatBytesW(m_max) + _T('/') + WSTRING(S);
				int tw = WinUtil::getTextWidth(txt, dc);
				if (tw + 2 > twidth)
					twidth = tw + 2;
				dc.TextOut(1, 1, txt.c_str());
			}
		}
		{
			CSelectPen l_pen(dc, m_UploadSocketPen); //-V808
			drawLine(dc, m_UpSockets, rc, clientRC);
		}
		
		{
			CSelectPen l_pen(dc, m_DownloadSocketPen); //-V808
			drawLine(dc, m_DownSockets, rc, clientRC);
		}
		// [+]IRainman
		{
			CSelectPen l_pen(dc, m_UploadsPen); //-V808
			drawLine(dc, m_Uploads, rc, clientRC);
		}
		{
			CSelectPen l_pen(dc, m_DownloadsPen); //-V808
			drawLine(dc, m_Downloads, rc, clientRC);
		}
		// [~]IRainman
	}
	return 0;
}

inline int64_t calcSpeed(int64_t bdiff, uint64_t tdiff)
{
	return bdiff * 1024I64 / tdiff;
}
/*
void StatsFrame::addTick(int64_t bdiff, uint64_t tdiff, StatList& lst, AvgList& avg, int scroll)
{
    int64_t bspeed = calcSpeed(bdiff, tdiff);

    avg.push_front(bspeed);

    bspeed = 0;

    for (auto ai = avg.cbegin(); ai != avg.cend(); ++ai)
    {
        bspeed += *ai;
    }

    bspeed /= avg.size();

    updateStatList(bspeed, lst, scroll);

    while (avg.size() > SPEED_APPROXIMATION_INTERVAL_S)
    {
        avg.pop_back();
    }
}
*/
LRESULT StatsFrame::onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	const uint64_t tick = MainFrame::getLastUpdateTick();
	const uint64_t tdiff = tick - lastTick;
	if (tdiff == 0)
		return 0;
		
	const uint64_t scrollms = (tdiff + scrollTick) * PIX_PER_SEC;
	const uint64_t scroll = scrollms / 1000;
	
	if (scroll == 0)
		return 0;
		
	scrollTick = scrollms - (scroll * 1000);
	
	CRect rc;
	GetClientRect(rc);
	rc.left = twidth;
	ScrollWindow(-((int)scroll), 0, rc, rc);
	
	const int64_t d = MainFrame::getLastDownloadSpeed();
	//const int64_t ddiff = d - m_lastSocketsDown;
	
	const int64_t u = MainFrame::getLastUploadSpeed();
	//const int64_t udiff = u - m_lastSocketsUp;
	
	const int64_t dt = DownloadManager::getRunningAverage();
	
	const int64_t ut = UploadManager::getRunningAverage();
	// [~]IRainman
	
	//addTick(ddiff, tdiff, m_DownSockets, m_DownSocketsAvg, (int)scroll);
	//addTick(udiff, tdiff, m_UpSockets, m_UpSocketsAvg, (int)scroll);
	addAproximatedSpeedTick(d, m_DownSockets, (int)scroll);
	addAproximatedSpeedTick(u, m_UpSockets, (int)scroll);
	addAproximatedSpeedTick(dt, m_Downloads, (int)scroll);
	addAproximatedSpeedTick(ut, m_Uploads, (int)scroll);
	
	StatIter i;
	int64_t mspeed = 0;
	findMax(m_DownSockets, i, mspeed);
	findMax(m_UpSockets, i, mspeed);
	// [+]IRainman
	findMax(m_Downloads, i, mspeed);
	findMax(m_Uploads, i, mspeed);
	// [~]IRainman
	
	if (mspeed > m_max || ((m_max * 3 / 4) > mspeed))
	{
		m_max = mspeed + 1;// [!] IRainman fix: +1
		Invalidate();
	}
	
	lastTick = tick;
	//m_lastSocketsUp = u;
	//m_lastSocketsDown = d;
	doTimerTask();
	return 0;
}

LRESULT StatsFrame::onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CRect rc;
	GetClientRect(rc);
	g_width  = rc.Width();
	g_height = rc.Height() - 1;
	Invalidate();
	bHandled = FALSE;
	return 0;
}

#endif
/**
 * @file
 * $Id: StatsFrame.cpp 463 2009-10-01 16:30:22Z BigMuscle $
 */
