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
#include "Resource.h"
#include "SpyFrame.h"
#include "SearchFrm.h"
#include "MainFrm.h"

#include "../client/ConnectionManager.h"

int SpyFrame::columnSizes[] = { 305, 70, 90, 120, 20 };
int SpyFrame::columnIndexes[] = { COLUMN_STRING, COLUMN_COUNT, COLUMN_USERS, COLUMN_TIME, COLUMN_SHARE_HIT }; // !SMT!-S
static ResourceManager::Strings columnNames[] = { ResourceManager::SEARCH_STRING, ResourceManager::COUNT, ResourceManager::USERS, ResourceManager::TIME, ResourceManager::SHARED }; // !SMT!-S

SpyFrame::SpyFrame() : CFlyTimerAdapter(m_hWnd), CFlyTaskAdapter(m_hWnd), m_total(0), m_current(0),
	m_ignoreTTH(BOOLSETTING(SPY_FRAME_IGNORE_TTH_SEARCHES)),
	m_showNick(BOOLSETTING(SHOW_SEEKERS_IN_SPY_FRAME)),
	m_LogFile(BOOLSETTING(LOG_SEEKERS_IN_SPY_FRAME)),
	m_ignoreTTHContainer(WC_BUTTON, this, SPYFRAME_IGNORETTH_MESSAGE_MAP),
	m_ShowNickContainer(WC_BUTTON, this, SPYFRAME_SHOW_NICK),
	m_SpyLogFileContainer(WC_BUTTON, this, SPYFRAME_LOG_FILE),
	m_log(nullptr), m_needsUpdateTime(true), m_needsResort(false) //[+]IRainman refactoring SpyFrame
{
	memzero(m_perSecond, sizeof(m_perSecond));
	ClientManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
}

LRESULT SpyFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlSearches.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, WS_EX_CLIENTEDGE, IDC_RESULTS);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlSearches);
	SET_LIST_COLOR(ctrlSearches);
	
	m_ctrlIgnoreTTH.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(IGNORE_TTH_SEARCHES), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_ctrlIgnoreTTH.SetButtonStyle(BS_AUTOCHECKBOX, false);
	m_ctrlIgnoreTTH.SetFont(Fonts::g_systemFont);
	m_ctrlIgnoreTTH.SetCheck(m_ignoreTTH);
	m_ignoreTTHContainer.SubclassWindow(m_ctrlIgnoreTTH.m_hWnd);
	
	m_ctrlShowNick.Create(ctrlStatus.m_hWnd, rcDefault, CTSTRING(SETTINGS_SHOW_SEEKERS_IN_SPY_FRAME), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_ctrlShowNick.SetButtonStyle(BS_AUTOCHECKBOX, false);
	m_ctrlShowNick.SetFont(Fonts::g_systemFont);
	m_ctrlShowNick.SetCheck(m_showNick);
	m_ShowNickContainer.SubclassWindow(m_ctrlShowNick.m_hWnd);
	
	const tstring l_log_path = Text::toT(Util::validateFileName(SETTING(LOG_DIRECTORY) + "spylog.log"));
	const tstring l_check_box_log_caption = TSTRING(SETTINGS_LOG_FILE_IN_SPY_FRAME) + _T(" (") + l_log_path + _T(" )");
	
	m_ctrlSpyLogFile.Create(ctrlStatus.m_hWnd, rcDefault, l_check_box_log_caption.c_str(), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	m_ctrlSpyLogFile.SetButtonStyle(BS_AUTOCHECKBOX, false);
	m_ctrlSpyLogFile.SetFont(Fonts::g_systemFont);
	m_ctrlSpyLogFile.SetCheck(m_LogFile);
	m_SpyLogFileContainer.SubclassWindow(m_ctrlSpyLogFile.m_hWnd);
	
	
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
	ShareManager::setHits(0);
	
	if (m_LogFile)
	{
		try
		{
			m_log = new File(l_log_path, File::WRITE, File::OPEN | File::CREATE);
			m_log->setEndPos(0);
			if (m_log->getPos() == 0)
				m_log->write("\xef\xbb\xbf");
		}
		catch (const FileException& e)
		{
			LogManager::message("Error create file " + Text::fromT(l_log_path) + " error = " + e.getError());
			safe_delete(m_log);
		}
	}
	//[~]IRainman refactoring SpyFrame
	create_timer(1000);
	ClientManager::g_isSpyFrame = true;
	bHandled = FALSE;
	return 1;
}

LRESULT SpyFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ClientManager::g_isSpyFrame = false;
	if (!m_closed)
	{
		m_closed = true;
		safe_destroy_timer();
		clear_and_destroy_task();
		if (m_log)
		{
			PostMessage(WM_SPEAKER, SAVE_LOG, (LPARAM)NULL);
		}
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
		SET_SETTING(SHOW_SEEKERS_IN_SPY_FRAME, m_showNick);
		SET_SETTING(LOG_SEEKERS_IN_SPY_FRAME, m_LogFile);
		safe_delete(m_log);
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
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[6];
		ctrlStatus.GetClientRect(sr);
		
		const int tmp = sr.Width() > 616 ? 516 : (sr.Width() > 116 ? sr.Width() - 100 : 16);
		
		w[0] = 170;
		w[1] = sr.right - tmp - 150;
		w[2] = w[1] + (tmp - 16) * 1 / 4;
		w[3] = w[1] + (tmp - 16) * 2 / 4;
		w[4] = w[1] + (tmp - 16) * 3 / 4;
		w[5] = w[1] + (tmp - 16) * 4 / 4;
		
		ctrlStatus.SetParts(6, w);
		
		ctrlStatus.GetRect(0, sr);
		m_ctrlIgnoreTTH.MoveWindow(sr);
		sr.MoveToX(170);
		sr.right += 50;
		m_ctrlShowNick.MoveWindow(sr);
		sr.MoveToX(sr.right + 10);
		sr.right += 200;
		m_ctrlSpyLogFile.MoveWindow(sr);
	}
	
	ctrlSearches.MoveWindow(&rect);
}

LRESULT SpyFrame::onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	TaskQueue::List t;
	m_tasks.get(t);
	
	if (t.empty())
		return 0;
		
	CFlyBusyBool l_busy(m_spoken);
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
					auto& l_searh_item = m_spy_searches[si->s];
					if (m_showNick)// [+] IRainman
					{
						if (::strncmp(si->seeker.c_str(), "Hub:", 4))
						{
							const string::size_type l_twopt = si->seeker.find(':');
							if (l_twopt != string::npos)
							{
								const string l_ip = si->seeker.substr(0, l_twopt);
								const StringList l_users = ClientManager::getUsersByIp(l_ip);
								if (!l_users.empty())
								{
									si->seeker = "[IP:" + l_ip + "] Users:" + Util::toString(l_users);
								}
							}
						}
					}
					if (m_log && m_LogFile)
					{
						m_log_txt += Text::fromT(m_CurrentTime) + '\t' +
						             si->seeker + '\t' +
						             si->s + "\r\n";
					}
					if (m_showNick)// [+] IRainman
					{
						size_t k;
						for (k = 0; k < NUM_SEEKERS; ++k)
						{
							if (si->seeker == l_searh_item.m_seekers[k])
							{
								break;          //that user's searching for file already noted
							}
						}
						if (k == NUM_SEEKERS)           //loop terminated normally
						{
							l_searh_item.AddSeeker(si->seeker);
						}							
						for (k = 0; k < NUM_SEEKERS; ++k)
						{
							const string::size_type l_twopt = l_searh_item.m_seekers[k].find(':');
							if (l_twopt != string::npos)
							{
								const string l_ip = l_searh_item.m_seekers[k].substr(0, l_twopt);
								if (!l_ip.empty() && l_ip[0] != 'H')
								{
									const Util::CustomNetworkIndex& l_location = Util::getIpCountry(l_ip);
									l_SeekersNames += Text::toT(l_searh_item.m_seekers[k]) + _T(" [") + l_location.getCountry() + _T("] ");
								}
								else
								{
							l_SeekersNames += Text::toT(l_searh_item.m_seekers[k]) + _T("  ");
					}
							}
						}
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
					a.reserve(5);
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
					tmp[0] = 0;
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
				if (BOOLSETTING(BOLD_SEARCH))
				{
					setDirty(0);
				}
#ifdef FLYLINKDC_USE_SOUND_AND_POPUP_IN_SEARCH_SPY
				SHOW_POPUP(POPUP_SEARCH_SPY, m_CurrentTime + _T(" : ") + l_SeekersNames + _T("\r\n") + l_search, TSTRING(SEARCH_SPY)); // [+] SCALOlaz: Spy Popup. Thanks to tret2003 (NightOrion) with tstring
				PLAY_SOUND(SOUND_SEARCHSPY);
#endif
			}
			break;
			case TICK_AVG:
			{
				auto s = (Stats*)i->second;
				LocalArray<TCHAR, 50> buf;
				_snwprintf(buf.data(), buf.size(), CTSTRING(SEARCHES_PER), s->m_perS, s->m_perM);
				ctrlStatus.SetText(2, (TSTRING(TOTAL) + _T(' ') + Util::toStringW(m_total)).c_str());
				ctrlStatus.SetText(3, buf.data());
				ctrlStatus.SetText(4, (TSTRING(HITS) + _T(' ') + Util::toStringW((size_t)(ShareManager::getHits()))).c_str());
				const double ratio = m_total > 0 ? static_cast<double>(ShareManager::getHits()) / static_cast<double>(m_total) : 0.0;
				ctrlStatus.SetText(5, (TSTRING(HIT_RATIO) + _T(' ') + Util::toStringW(ratio)).c_str());
				if (m_needsResort)
				{
					m_needsResort = false;
					ctrlSearches.resort();
				}
			}
			break;
			case SAVE_LOG:
				if (m_log)
				{
					try
					{
						m_log->setEndPos(0);
						m_log->write(m_log_txt);
					}
					catch (const FileException& e)
					{
						LogManager::message("Error write file spylog.log error = " + e.getError());
					}
					m_log_txt.clear();
				}
				break;
		}
		delete i->second;
	}
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
		SearchFrame::openWindow(m_searchString.substr(4), 0, Search::SIZE_DONTCARE, Search::TYPE_TTH);
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
	if (!MainFrame::isAppMinimized(m_hWnd) && !isClosedOrShutdown())// [+] IRainman opt
	{
		auto s = new Stats;
		for (size_t i = 0; i < AVG_TIME; ++i)
			s->m_perM += m_perSecond[i];
			
		s->m_perS = s->m_perM / AVG_TIME;
		m_tasks.add(TICK_AVG, s);
	}
	{
		if (m_current++ >= AVG_TIME)
			m_current = 0;
			
		m_perSecond[m_current] = 0;
	}
	if (m_log)
	{
		m_tasks.add(SAVE_LOG, nullptr);
	}
	doTimerTask();
	return 0;
}

void SpyFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlSearches.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

// !SMT!-S
inline static COLORREF blendColors(COLORREF a, COLORREF b)
{
	return ((uint32_t)a & 0xFEFEFE) / 2 + ((uint32_t)b & 0xFEFEFE) / 2;
}

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
			plvcd->clrTextBk = blendColors(SETTING(DUPE_COLOR), SETTING(BACKGROUND_COLOR));
			
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		Colors::alternationBkColor(plvcd); // [+] IRainman
#endif
	}
	return CDRF_DODEFAULT;
}

/**
 * @file
 * $Id: SpyFrame.cpp,v 1.38 2006/10/13 20:04:32 bigmuscle Exp $
 */
