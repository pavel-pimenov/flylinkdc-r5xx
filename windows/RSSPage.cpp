/*
 * Copyright (C) 2011-2021 FlylinkDC++ Team http://flylinkdc.com
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

#include "RSSPage.h"
#include "../FlyFeatures/RSSManager.h"
#include "RSSSetFeedDlg.h"

// Переводы элементов интерфейса.
PropPage::TextItem RSSPage::texts[] =
{
	{ IDC_ADD_RSS, ResourceManager::ADD },
	{ IDC_CHANGE_RSS, ResourceManager::SETTINGS_CHANGE },
	{ IDC_REMOVE_RSS, ResourceManager::REMOVE },
	{ IDC_RSSAUTOREFRESH_TIME_STR, ResourceManager::RSSAUTOREFRESH_TIME_STR },
	{ IDC_RSS_AUTOREFRESH_MIN, ResourceManager::DATETIME_MINUTES },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item RSSPage::items[] =
{
	{ IDC_RSS_AUTO_REFRESH_TIME, SettingsManager::RSS_AUTO_REFRESH_TIME, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

LRESULT RSSPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (m_CodeingList.empty())
	{
		m_CodeingList.insert(CodeingMapPair(TSTRING(RSS_CODEING_AUTO), RSSManager::getCodeing(0)));
		m_CodeingList.insert(CodeingMapPair(TSTRING(RSS_CODEING_UTF8), RSSManager::getCodeing(1)));
		m_CodeingList.insert(CodeingMapPair(TSTRING(RSS_CODEING_CP1251), RSSManager::getCodeing(2)));
	}
	
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items);
	
	CRect rc;
	
	ctrlCommands.Attach(GetDlgItem(IDC_RSS_ITEMS));
	ctrlCommands.GetClientRect(rc);
	
//	ctrlCommands.InsertColumn(0, CTSTRING(RSS_URL), LVCFMT_LEFT, (rc.Width() - 30) / 3, 0);
	ctrlCommands.InsertColumn(1, CTSTRING(RSS_TITLE), LVCFMT_LEFT, rc.Width() - 30, 1);
//	ctrlCommands.InsertColumn(2, CTSTRING(RSS_CODEING), LVCFMT_LEFT, (rc.Width() - 30) / 3, 1);
	ctrlCommands.InsertColumn(3, _T("+/-"), LVCFMT_CENTER, 30, 1);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlCommands);
#ifdef USE_SET_LIST_COLOR_IN_SETTINGS
	SET_LIST_COLOR_IN_SETTING(ctrlCommands);
#endif
	
	
	CUpDownCtrl spin;
	spin.Attach(GetDlgItem(IDC_RSS_AUTO_REFRESH_TIME_SPIN));
	spin.SetRange(0, 30);
	spin.Detach();
	
	// Do specialized reading here
	const RSSManager::FeedList& lst = RSSManager::lockFeedList();
	auto cnt = ctrlCommands.GetItemCount();
	for (auto i = lst.cbegin(); i != lst.cend(); ++i)
	{
		addRSSEntry(*i, cnt++);
	}
	RSSManager::unlockFeedList();
	
	return TRUE;
}

void RSSPage::write()
{
	PropPage::write(*this, items);
}

void RSSPage::addRSSEntry(const RSSFeed* rf, int pos)
{
	TStringList lst;
//	lst.push_back(Text::toT(rf->getFeedURL()));
	lst.push_back(Text::toT(rf->getSource()));
//	size_t codeingT = RSSManager::GetCodeingByString(rf->getCodeing());
//	lst.push_back(GetCodeingFromMapName(codeingT));
	lst.push_back(rf->getEnable() ? CTSTRING(YES) : _T(""));
	ctrlCommands.insert(pos, lst, 0, (LPARAM)pos);
}


LRESULT RSSPage::onAddFeed(WORD, WORD, HWND, BOOL&)
{
	RSS_SetFeedDlg dlg;
	
	if (dlg.DoModal() == IDOK)
	{
		RSSFeed* feed = RSSManager::getInstance()->addNewFeed(Text::fromT(dlg.m_strURL), Text::fromT(dlg.m_strName), Text::fromT(dlg.m_strCodeing), dlg.m_enabled);
		if (feed)
			addRSSEntry(feed,  ctrlCommands.GetItemCount());
	}
	return 0;
}

LRESULT RSSPage::onItemchangedFeeds(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_CHANGE_RSS), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_REMOVE_RSS), (lv->uNewState & LVIS_FOCUSED));
	return 0;
}

LRESULT RSSPage::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD_RSS, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE_RSS, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}


LRESULT RSSPage::onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	
	if (item->iItem >= 0)
	{
		PostMessage(WM_COMMAND, IDC_CHANGE_RSS, 0);
	}
	else if (item->iItem == -1)
	{
		PostMessage(WM_COMMAND, IDC_ADD_RSS, 0);
	}
	
	return 0;
}

LRESULT RSSPage::onChangeFeed(WORD, WORD, HWND, BOOL&)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int sel = ctrlCommands.GetSelectedIndex();
		RSSFeed* feed = RSSManager::lockFeedList().at(sel);
		if (feed)
		{
			RSS_SetFeedDlg dlg;
			dlg.m_strURL = Text::toT(feed->getFeedURL());
			dlg.m_strName = Text::toT(feed->getSource());
			dlg.m_strCodeing = Text::toT(feed->getCodeing());
			dlg.m_enabled = feed->getEnable();
			if (dlg.DoModal() == IDOK)
			{
				feed->setFeedURL(Text::fromT(dlg.m_strURL));
				feed->setSource(Text::fromT(dlg.m_strName));
				feed->setCodeing(Text::fromT(dlg.m_strCodeing));
				feed->setEnable(dlg.m_enabled);
				// Update ctrlCommands
				ctrlCommands.DeleteItem(sel);
				addRSSEntry(feed,  sel);
			}
		}
		RSSManager::unlockFeedList();
	}
	return 0;
}

// При удалении.
LRESULT RSSPage::onRemoveFeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlCommands.GetSelectedCount() == 1)
	{
		int i = ctrlCommands.GetNextItem(-1, LVNI_SELECTED);
		if (RSSManager::getInstance()->removeFeedAt(i))
		{
			ctrlCommands.DeleteItem(i);
		}
		
	}
	return 0;
}

#endif // IRAINMAN_INCLUDE_RSS