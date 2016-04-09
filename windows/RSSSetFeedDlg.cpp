/*
 * Copyright (C) 2010-2016 FlylinkDC++ Team http://flylinkdc.com
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


#include "../FlyFeatures/RSSManager.h"
#include "Resource.h"
#include "RSSSetFeedDlg.h"
#include "wtl_flylinkdc.h"

WinUtil::TextItem RSS_SetFeedDlg::texts[] =
{
	{ IDOK, ResourceManager::OK },
	{ IDCANCEL, ResourceManager::CANCEL },
	{ IDC_RSSFEED_URL_LBL, ResourceManager::RSSDLG_URL },
	{ IDC_RSSFEED_NAME_LBL, ResourceManager::RSSDLG_NAME },
	{ IDC_RSSFEED_CODEING_LBL, ResourceManager::RSSDLG_CODEING },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};


LRESULT RSS_SetFeedDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// Translate
	SetWindowText(CTSTRING(RSSDLG_TITLE));
	
	FillCodeingCombo();
	
	ATTACH(IDC_RSSFEED_URL, ctrlURL);
	ATTACH(IDC_RSSFEED_NAME, ctrlName);
	
	WinUtil::translate(*this, texts);
	
	ctrlURL.SetWindowText(m_strURL.c_str());
	ctrlName.SetWindowText(m_strName.c_str());
	
	CenterWindow(GetParent());
	return FALSE;
}

LRESULT RSS_SetFeedDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		GET_TEXT(IDC_RSSFEED_URL, m_strURL);
		GET_TEXT(IDC_RSSFEED_NAME, m_strName);
		
		ctrlCodeing.Attach(GetDlgItem(IDC_RSSFEED_CODEING));
		m_strCodeing = Text::toT(WinUtil::getDataFromMap(ctrlCodeing.GetCurSel(), m_CodeingList));
	}
	
	EndDialog(wID);
	return 0;
}

void RSS_SetFeedDlg::FillCodeingCombo()
{
	if (m_CodeingList.empty())
	{
		m_CodeingList.insert(CodeingMapPair(TSTRING(RSS_CODEING_AUTO), RSSManager::getCodeing(0)));
		m_CodeingList.insert(CodeingMapPair(TSTRING(RSS_CODEING_UTF8), RSSManager::getCodeing(1)));
		m_CodeingList.insert(CodeingMapPair(TSTRING(RSS_CODEING_CP1251), RSSManager::getCodeing(2)));
	}
	
	ctrlCodeing.Attach(GetDlgItem(IDC_RSSFEED_CODEING));
	for (auto i = m_CodeingList.cbegin(); i != m_CodeingList.cend(); ++i)
		ctrlCodeing.AddString(i->first.c_str());
		
	int iNum = WinUtil::getIndexFromMap(m_CodeingList, Text::fromT(m_strCodeing).c_str());
	ctrlCodeing.SetCurSel(iNum);
	
	ctrlCodeing.Detach();
}

#endif // IRAINMAN_INCLUDE_RSS