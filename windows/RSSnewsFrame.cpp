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

#include "stdafx.h"

#ifdef IRAINMAN_INCLUDE_RSS


#include "Resource.h"
#include "Mainfrm.h"

#include "RSSnewsFrame.h"
#include "../client/File.h"

int RSSNewsFrame::columnIndexes[] = { COLUMN_TITLE, COLUMN_URL, COLUMN_DESC, COLUMN_DATE, COLUMN_AUTHOR, COLUMN_CATEGORY, COLUMN_SOURCE };

int RSSNewsFrame::columnSizes[] = { 100, 110, 290, 125, 80, 80, 80 };


LRESULT RSSNewsFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | LVS_SINGLESEL, WS_EX_CLIENTEDGE, IDC_RSS);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlList);
	
	ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	SET_LIST_COLOR(ctrlList);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(RSS_COLUMNS_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(RSS_COLUMNS_WIDTHS), COLUMN_LAST);
	
	for (size_t j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_DATE) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlList.InsertColumn(j, TSTRING_I(rss_columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlList.setColumnOrderArray(COLUMN_LAST, columnIndexes);
	ctrlList.setVisible(SETTING(RSS_COLUMNS_VISIBLE));
	
	//  ctrlList.setSortColumn(COLUMN_DATE);
	//  ctrlList.setAscending(false);
	
	ctrlList.setSortColumn(SETTING(RSS_COLUMNS_SORT));
	ctrlList.setAscending(BOOLSETTING(RSS_COLUMNS_SORT_ASC));
	
	ctrlRemoveAll.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     BS_PUSHBUTTON, 0, IDC_REMOVE_ALL);
	ctrlRemoveAll.SetWindowText(CTSTRING(CDM_CLEAR));
	ctrlRemoveAll.SetFont(Fonts::g_systemFont);
	
	copyMenu.CreatePopupMenu();
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_URL, CTSTRING(RSS_URL));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_ACTUAL_LINE, (CTSTRING(RSS_TITLE) + Text::toT(" + ") + CTSTRING(RSS_URL)).c_str());
	
	UpdateLayout();
	
	SettingsManager::getInstance()->addListener(this);
	RSSManager::getInstance()->addListener(this);
	
	RSSManager::updateFeeds();
	
	updateList(RSSManager::lockNewsList());
	RSSManager::unlockNewsList();
	
	bHandled = FALSE;
	return TRUE;
}

LRESULT RSSNewsFrame::updateList(const RSSManager::NewsList& list)
{
	CLockRedraw<true> l_lock_draw(ctrlList);
	for (auto i = list.cbegin(); i != list.cend(); ++i)
	{
		addRSSEntry(*i);
	}
	return 1;
}
LRESULT RSSNewsFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{

	if (!m_closed)
	{
		m_closed = true;
		RSSManager::getInstance()->removeListener(this);
		SettingsManager::getInstance()->removeListener(this);
		WinUtil::setButtonPressed(IDC_RSS, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		ctrlList.saveHeaderOrder(SettingsManager::RSS_COLUMNS_ORDER, SettingsManager::RSS_COLUMNS_WIDTHS, SettingsManager::RSS_COLUMNS_VISIBLE);
		
		SET_SETTING(RSS_COLUMNS_SORT, ctrlList.getRealSortColumn());
		SET_SETTING(RSS_COLUMNS_SORT_ASC, ctrlList.isAscending());
		
		ctrlList.DeleteAndCleanAllItems();
		
		bHandled = FALSE;
		return 0;
	}
}

void RSSNewsFrame::UpdateLayout(BOOL bResizeBars /*= TRUE*/)
{
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	CRect rc = rect;
	rc.bottom -= 26;
	ctrlList.MoveWindow(rc);
	
	const long bwidth = 100;
	//const long bspace = 10;
	
	rc = rect;
	rc.bottom -= 2;
	rc.top = rc.bottom - 22;
	
	rc.left = 2;
	rc.right = rc.left + bwidth;
	ctrlRemoveAll.MoveWindow(rc);
	ctrlRemoveAll.ShowWindow(FALSE);
	
}

void RSSNewsFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlList.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}
void
RSSNewsFrame::on(RSSListener::NewRSS, const unsigned int newsCount) noexcept
{
	SHOW_POPUP(POPUP_NEW_RSSNEWS, Text::toT(m_Message), TSTRING(NEW_RSS_NEWS));
	m_Message.clear();
	
	if (BOOLSETTING(BOLD_NEWRSS))
	{
		setDirty(0);
	}
}

void RSSNewsFrame::on(RSSListener::Added, const RSSItem* p_item) noexcept
// 41 ������� � ���� ������� https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=23689
// http://i.imgur.com/Si4oqEI.png
{
	addRSSEntry(p_item);
	m_Message = p_item->getTitle() + " - " + p_item->getSource() + "\r\n";
}

LRESULT RSSNewsFrame::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE * const item = (NMITEMACTIVATE*) pnmh;
	
	if (item->iItem != -1)
	{
		RSSItemInfo *ii = ctrlList.getItemData(item->iItem);
		WinUtil::openLink(Text::toT(ii->m_entry->getUrl()));
	}
	return 0;
}
LRESULT RSSNewsFrame::onOpenClick(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		RSSItemInfo *ii = ctrlList.getItemData(i);
		WinUtil::openLink(Text::toT(ii->m_entry->getUrl()));
	}
	return 0;
}
LRESULT RSSNewsFrame::onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
//	CLockRedraw<true> l_lock_draw(ctrlList);
//	ctrlList.DeleteAllItems();
	return 0;
}
LRESULT RSSNewsFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL & bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlList && ctrlList.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlList, pt);
		}
		if (ctrlList.GetSelectedCount() == 1)
		{
			OMenu RSSMenu;
			RSSMenu.CreatePopupMenu();
			RSSMenu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN));
			RSSMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY));
			//  RSSMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
			RSSMenu.SetMenuDefaultItem(IDC_OPEN_FILE);
			RSSMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			return TRUE;
		}
	}
	bHandled = FALSE;
	return FALSE;
}
LRESULT RSSNewsFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string data;
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const RSSItemInfo* l_si = ctrlList.getItemData(i);
		string sCopy;
		switch (wID)
		{
			case IDC_COPY_URL:
				sCopy = l_si->m_entry->getUrl();
				break;
			case IDC_COPY_ACTUAL_LINE:
				sCopy = "'" + l_si->m_entry->getTitle() + "' " + l_si->m_entry->getUrl();
				break;
			default:
				dcdebug("RSSNEWSFRAME DON'T GO HERE\n");
				return 0;
		}
		if (!sCopy.empty())
		{
			if (data.empty())
				data = sCopy;
			else
				data = data + "\r\n" + sCopy;
		}
	}
	if (!data.empty())
	{
		WinUtil::setClipboard(data);
	}
	return 0;
}
#endif // IRAINMAN_INCLUDE_RSS