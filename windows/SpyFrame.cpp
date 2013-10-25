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

#include "stdafx.h"
#include "Resource.h"
#include "SpyFrame.h"
#include "SearchFrm.h"
#include "MainFrm.h"

#include "../client/ConnectionManager.h"

int SpyFrame::columnSizes[] = { 305, 70, 90, 120, 20 };
int SpyFrame::columnIndexes[] = { COLUMN_STRING, COLUMN_COUNT, COLUMN_USERS, COLUMN_TIME, COLUMN_SHARE_HIT }; // !SMT!-S
static ResourceManager::Strings columnNames[] = { ResourceManager::SEARCH_STRING, ResourceManager::COUNT, ResourceManager::USERS, ResourceManager::TIME, ResourceManager::SHARED }; // !SMT!-S

LRESULT SpyFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlSearches.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, WS_EX_CLIENTEDGE, IDC_RESULTS);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlSearches);
	SET_LIST_COLOR(ctrlSearches);
	
	ctrlIgnoreTth.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(IGNORE_TTH_SEARCHES), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlIgnoreTth.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlIgnoreTth.SetFont(Fonts::systemFont);
	ctrlIgnoreTth.SetCheck(m_ignoreTTH);
	ignoreTthContainer.SubclassWindow(ctrlIgnoreTth.m_hWnd);
	
	WinUtil::splitTokens(columnIndexes, SETTING(SPYFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(SPYFRAME_WIDTHS), COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (int j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_COUNT) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlSearches.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	//ctrlSearches.setSort(COLUMN_COUNT, ExListViewCtrl::SORT_INT, false);
	//ctrlSearches.setVisible(SETTING(SPYFRAME_VISIBLE)); // !SMT!-UI
	
	ctrlSearches.setSort(SETTING(SEARCH_SPY_COLUMNS_SORT), ExListViewCtrl::SORT_INT, BOOLSETTING(SEARCH_SPY_COLUMNS_SORT_ASC));
	ShareManager::getInstance()->setHits(0);
	
#ifdef _BIG_BROTHER_MODE
	//[!]IRainman refactoring SpyFrame
	if (BOOLSETTING(OPEN_SEARCH_SPY))
	{
		try
		{
			m_log = new File(Util::getConfigPath() + "SpyLog.txt", File::WRITE, File::OPEN);
			m_log->setEndPos(0);
			if (m_log->getPos() == 0)
				m_log->write("\xef\xbb\xbf");
		}
		catch (const FileException&)
		{
		}
	}
#endif
	//[~]IRainman refactoring SpyFrame
	create_timer(1000);
	bHandled = FALSE;
	return 1;
}

LRESULT SpyFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		safe_destroy_timer();
#ifdef _BIG_BROTHER_MODE
		if (m_log)
			PostMessage(WM_SPEAKER, SAVE_LOG, (LPARAM)NULL);
#endif
		ClientManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		bHandled = TRUE;
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlSearches, SettingsManager::SPYFRAME_ORDER, SettingsManager::SPYFRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		//ctrlSearches.saveHeaderOrder(SettingsManager::SPYFRAME_ORDER, SettingsManager::SPYFRAME_WIDTHS, SettingsManager::SPYFRAME_VISIBLE); // !SMT!-UI
		
		SET_SETTING(SEARCH_SPY_COLUMNS_SORT, ctrlSearches.getSortColumn());
		SET_SETTING(SEARCH_SPY_COLUMNS_SORT_ASC, ctrlSearches.isAscending());
		
		SET_SETTING(SPY_FRAME_IGNORE_TTH_SEARCHES, m_ignoreTTH);
#ifdef _BIG_BROTHER_MODE
		if (m_log)
			delete m_log;
#endif
		WinUtil::setButtonPressed(IDC_SEARCH_SPY, false);
		bHandled = FALSE;
		return 0;
	}
}

LRESULT SpyFrame::onColumnClickResults(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
	if (l->iSubItem == ctrlSearches.getSortColumn())
	{
		if (!ctrlSearches.isAscending())
			ctrlSearches.setSort(-1, ctrlSearches.getSortType());
		else
			ctrlSearches.setSortDirection(false);
	}
	else
	{
		if (l->iSubItem == COLUMN_COUNT)
		{
			ctrlSearches.setSort(l->iSubItem, ExListViewCtrl::SORT_INT);
		}
		else
		{
			ctrlSearches.setSort(l->iSubItem, ExListViewCtrl::SORT_STRING_NOCASE);
		}
	}
	return 0;
}

void SpyFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[6];
		ctrlStatus.GetClientRect(sr);
		
		int tmp = (sr.Width()) > 616 ? 516 : ((sr.Width() > 116) ? sr.Width() - 100 : 16);
		
		w[0] = 170;
		w[1] = sr.right - tmp - 150;
		w[2] = w[1] + (tmp - 16) * 1 / 4;
		w[3] = w[1] + (tmp - 16) * 2 / 4;
		w[4] = w[1] + (tmp - 16) * 3 / 4;
		w[5] = w[1] + (tmp - 16) * 4 / 4;
		
		ctrlStatus.SetParts(6, w);
		
		ctrlStatus.GetRect(0, sr);
		ctrlIgnoreTth.MoveWindow(sr);
	}
	
	ctrlSearches.MoveWindow(&rect);
}

LRESULT SpyFrame::onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	TaskQueue::List t;
	m_tasks.get(t);
	
	if (t.empty())
		return 0;
		
	CLockRedraw<> lockCtrlList(ctrlSearches);
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		switch (i->first)
		{
			case SEARCH:
			{
				SMTSearchInfo* si = (SMTSearchInfo*)i->second;
				//[+]IRainman refactoring SpyFrame
				if (m_needsUpdateTime)
				{
					m_CurrentTime = Text::toT(Util::formatDigitalClock(GET_TIME()));
					m_needsUpdateTime = false;
				}
				//[~]IRainman refactoring SpyFrame
				tstring l_SeekersNames;
				{
#ifdef _BIG_BROTHER_MODE
					if (m_log)
					{
						m_txt += Text::fromT(m_CurrentTime) + "\t" +
						         si->seeker + "\t" +
						         si->s + "\r\n";
					}
#endif
					const auto& it2 = m_searches.find(si->s);
					if (it2 == m_searches.end())
					{
						m_searches[si->s].i = 1;
					}
					
					if (BOOLSETTING(SHOW_SEEKERS_IN_SPY_FRAME))// [+] IRainman
					{
						if (::strncmp(si->seeker.c_str(), "Hub:", 4))
						{
							const string::size_type l_twopt = si->seeker.find(':');
							if (l_twopt != string::npos)
							{
								si->seeker = si->seeker.substr(0, l_twopt);
								const UserPtr u = ClientManager::getInstance()->getUserByIp(si->seeker);
								if (u)
								{
									si->seeker = u->getLastNick();
								}
							}
						}
						
						size_t k;
						for (k = 0; k < NUM_SEEKERS; ++k)
							if (si->seeker == (m_searches[si->s].seekers)[k])
								break;          //that user's searching for file already noted
								
						if (k == NUM_SEEKERS)           //loop terminated normally
							m_searches[si->s].AddSeeker(si->seeker);
							
						for (k = 0; k < NUM_SEEKERS; ++k)
							l_SeekersNames += Text::toT((m_searches[si->s].seekers)[k]) + _T("  ");
					}
					
					++m_total;
					
					++m_perSecond[m_current];
				}
				// !SMT!-S
				tstring hit;
				if (si->re == ClientManagerListener::SEARCH_PARTIAL_HIT)
					hit = _T('*');
				else if (si->re == ClientManagerListener::SEARCH_HIT)
					hit = _T('+');
					
				tstring l_search;
				l_search = Text::toT(si->s, l_search);
				const int j = ctrlSearches.find(l_search);
				if (j == -1)
				{
					TStringList a;
					a.push_back(l_search);
					a.push_back(_T("1"));
					a.push_back(l_SeekersNames);
					a.push_back(m_CurrentTime);
					a.push_back(hit);
					ctrlSearches.insert(a, 0, si->re);// !SMT!-S
					int l_Count = ctrlSearches.GetItemCount();
					if (l_Count > 500)
					{
						ctrlSearches.DeleteItem(--l_Count);
					}
				}
				else
				{
					TCHAR tmp[32];
					ctrlSearches.GetItemText(j, COLUMN_COUNT, tmp, 32);
					ctrlSearches.SetItemText(j, COLUMN_COUNT, Util::toStringW(Util::toInt(tmp) + 1).c_str());
					ctrlSearches.SetItemText(j, COLUMN_USERS, l_SeekersNames.c_str());
					ctrlSearches.SetItemText(j, COLUMN_TIME, m_CurrentTime.c_str());
					ctrlSearches.SetItemText(j, COLUMN_SHARE_HIT, hit.c_str()); // !SMT!-S
					ctrlSearches.SetItem(j, COLUMN_SHARE_HIT, LVIF_PARAM, NULL, 0, 0, 0, si->re); // !SMT!-S
					if (ctrlSearches.getSortColumn() == COLUMN_COUNT ||
					        ctrlSearches.getSortColumn() == COLUMN_TIME
					   )
						m_needsResort = true;
				}
				
				//Bolded activity in SpyFrame
				if (BOOLSETTING(BOLD_SEARCH))
				{
					setDirty();
				}
				
				SHOW_POPUP(POPUP_SEARCH_SPY, m_CurrentTime + _T(" : ") + l_SeekersNames + _T("\r\n") + l_search, TSTRING(SEARCH_SPY)); // [+] SCALOlaz: Spy Popup. Thanks to tret2003 (NightOrion) with tstring
				PLAY_SOUND(SOUND_SEARCHSPY);
			}
			break;
			case TICK_AVG:
			{
				auto s = (Stats*)i->second;
				LocalArray<TCHAR, 50> buf;
				snwprintf(buf.data(), buf.size(), CTSTRING(SEARCHES_PER), s->perS, s->perM);
				ctrlStatus.SetText(2, (TSTRING(TOTAL) + _T(' ') + Util::toStringW(m_total)).c_str());
				ctrlStatus.SetText(3, buf.data());
				ctrlStatus.SetText(4, (TSTRING(HITS) + _T(' ') + Util::toStringW((size_t)(ShareManager::getInstance()->getHits()))).c_str());
				const double ratio = m_total > 0 ? static_cast<double>(ShareManager::getInstance()->getHits()) / static_cast<double>(m_total) : 0.0;
				ctrlStatus.SetText(5, (TSTRING(HIT_RATIO) + _T(' ') + Util::toStringW(ratio)).c_str());
				if (m_needsResort)
				{
					m_needsResort = false;
					ctrlSearches.resort();
				}
			}
			break;
#ifdef _BIG_BROTHER_MODE
			case SAVE_LOG:
			{
				try
				{
					m_log->setEndPos(0);
					m_log->write(m_txt);
				}
				catch (const FileException&)
				{
				}
				m_txt.clear();
			}
			break;
#endif
		}
		delete i->second;
	}
	
	m_spoken = false;
	
	return 0;
}

LRESULT SpyFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlSearches && ctrlSearches.GetSelectedCount() == 1)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		CRect rc;
		ctrlSearches.GetHeader().GetWindowRect(&rc);
		if (PtInRect(&rc, pt))
		{
			return 0;
		}
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlSearches, pt);
		}
		
		int i = ctrlSearches.GetNextItem(-1, LVNI_SELECTED);
		
		CMenu mnu;
		mnu.CreatePopupMenu();
		mnu.AppendMenu(MF_STRING, IDC_SEARCH, CTSTRING(SEARCH));
		
		m_searchString.resize(256);
		ctrlSearches.GetItemText(i, COLUMN_STRING, &m_searchString[0], m_searchString.size());
		
		mnu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT SpyFrame::onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (strnicmp(m_searchString.c_str(), _T("TTH:"), 4) == 0)
		SearchFrame::openWindow(m_searchString.substr(4), 0, Search::SIZE_DONTCARE, SearchManager::TYPE_TTH);
	else
		SearchFrame::openWindow(m_searchString);
	return 0;
}

void SpyFrame::on(ClientManagerListener::IncomingSearch, const string& user, const string& s, ClientManagerListener::SearchReply re) noexcept
{
	if (m_ignoreTTH && isTTHBase64(s))
		return;
		
	SMTSearchInfo *x = new SMTSearchInfo(user, s, re);
	string::size_type i = 0;
	while ((i = (x->s).find('$')) != string::npos)
	{
		(x->s)[i] = ' ';
	}
	m_tasks.add(SEARCH, x);
}

LRESULT SpyFrame::onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	m_needsUpdateTime = true;//[+]IRainman refactoring SpyFrame
	if (!MainFrame::isAppMinimized() && WinUtil::g_tabCtrl->isActive(m_hWnd))// [+] IRainman opt
	{
		auto s = new Stats;
		s->perM = 0;
		for (size_t i = 0; i < AVG_TIME; ++i)
			s->perM += m_perSecond[i];
			
		s->perS = s->perM / AVG_TIME;
		m_tasks.add(TICK_AVG, s);
	}
	{
		if (m_current++ >= AVG_TIME)
			m_current = 0;
			
		m_perSecond[m_current] = 0;
	}
#ifdef _BIG_BROTHER_MODE
	if (m_log && ++m_tick > 10)
	{
		m_tasks.add(SAVE_LOG, nullptr);
		m_tick = 0;
	}
#endif
	if (!m_tasks.empty())
	{
		speak();
	}
	return 0;
}

void SpyFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
{
	bool refresh = false;
	if (ctrlSearches.GetBkColor() != Colors::bgColor)
	{
		ctrlSearches.SetBkColor(Colors::bgColor);
		ctrlSearches.SetTextBkColor(Colors::bgColor);
		refresh = true;
	}
	if (ctrlSearches.GetTextColor() != Colors::textColor)
	{
		ctrlSearches.SetTextColor(Colors::textColor);
		refresh = true;
	}
	if (refresh == true)
	{
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

// !SMT!-S
LRESULT SpyFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW plvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	if (CDDS_PREPAINT == plvcd->nmcd.dwDrawStage)
		return CDRF_NOTIFYITEMDRAW;
		
	if (CDDS_ITEMPREPAINT == plvcd->nmcd.dwDrawStage)
	{
		ClientManagerListener::SearchReply re = (ClientManagerListener::SearchReply)(plvcd->nmcd.lItemlParam);
		
		//check if the file or dir is a dupe, then use the dupesetting color
		if (re == ClientManagerListener::SEARCH_HIT)
			plvcd->clrTextBk = SETTING(DUPE_COLOR);
		else if (re == ClientManagerListener::SEARCH_PARTIAL_HIT)
			//if it's a partial hit, try to use some simple blending
			plvcd->clrTextBk = WinUtil::blendColors(SETTING(DUPE_COLOR), SETTING(BACKGROUND_COLOR));
			
		Colors::alternationBkColor(plvcd); // [+] IRainman
	}
	return CDRF_DODEFAULT;
}

/**
 * @file
 * $Id: SpyFrame.cpp,v 1.38 2006/10/13 20:04:32 bigmuscle Exp $
 */
