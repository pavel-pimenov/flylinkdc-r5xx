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
#include "ADLSProperties.h"
#include "../client/ADLSearch.h"
#include "WinUtil.h"

// Initialize dialog
LRESULT ADLSProperties::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	// Translate the texts
	SetWindowText(CTSTRING(ADLS_PROPERTIES));
	SetDlgItemText(IDC_ADLSP_SEARCH, CTSTRING(ADLS_SEARCH_STRING));
	SetDlgItemText(IDC_ADLSP_TYPE, CTSTRING(ADLS_TYPE));
	SetDlgItemText(IDC_ADLSP_SIZE_MIN, CTSTRING(ADLS_SIZE_MIN));
	SetDlgItemText(IDC_ADLSP_SIZE_MAX, CTSTRING(ADLS_SIZE_MAX));
	SetDlgItemText(IDC_ADLSP_UNITS, CTSTRING(ADLS_UNITS));
	SetDlgItemText(IDC_ADLSP_DESTINATION, CTSTRING(ADLS_DESTINATION));
	SetDlgItemText(IDC_IS_ACTIVE, CTSTRING(ADLS_ENABLED));
	SetDlgItemText(IDC_AUTOQUEUE, CTSTRING(ADLS_DOWNLOAD));
	SetDlgItemText(IDC_IS_FORBIDDEN, CTSTRING(FORBIDDEN));
	SetDlgItemText(IDC_ADLSEARCH_ACTION, CTSTRING(USER_CMD_RAW));
	
	cRaw.Attach(GetDlgItem(IDC_ADLSEARCH_RAW_ACTION));
	cRaw.AddString(CTSTRING(NO_ACTION));
	cRaw.AddString(Text::toT(SETTING(RAW1_TEXT)).c_str());
	cRaw.AddString(Text::toT(SETTING(RAW2_TEXT)).c_str());
	cRaw.AddString(Text::toT(SETTING(RAW3_TEXT)).c_str());
	cRaw.AddString(Text::toT(SETTING(RAW4_TEXT)).c_str());
	cRaw.AddString(Text::toT(SETTING(RAW5_TEXT)).c_str());
	cRaw.SetCurSel(search->raw);
	
	// Initialize dialog items
	ctrlSearch.Attach(GetDlgItem(IDC_SEARCH_STRING));
	ctrlDestDir.Attach(GetDlgItem(IDC_DEST_DIR));
	ctrlMinSize.Attach(GetDlgItem(IDC_MIN_FILE_SIZE));
	ctrlMaxSize.Attach(GetDlgItem(IDC_MAX_FILE_SIZE));
	ctrlActive.Attach(GetDlgItem(IDC_IS_ACTIVE));
	ctrlAutoQueue.Attach(GetDlgItem(IDC_AUTOQUEUE));
	
	ctrlSearchType.Attach(GetDlgItem(IDC_SOURCE_TYPE));
	ctrlSearchType.AddString(CTSTRING(FILENAME));
	ctrlSearchType.AddString(CTSTRING(DIRECTORY));
	ctrlSearchType.AddString(CTSTRING(ADLS_FULL_PATH));
	
	ctrlSizeType.Attach(GetDlgItem(IDC_SIZE_TYPE));
	ctrlSizeType.AddString(CTSTRING(B));
	ctrlSizeType.AddString(CTSTRING(KB));
	ctrlSizeType.AddString(CTSTRING(MB));
	ctrlSizeType.AddString(CTSTRING(GB));
	
	// Load search data
	ctrlSearch.SetWindowText(Text::toT(search->searchString).c_str());
	ctrlDestDir.SetWindowText(Text::toT(search->destDir).c_str());
	ctrlMinSize.SetWindowText(search->minFileSize > 0 ? Util::toStringW(search->minFileSize).c_str() : _T(""));
	ctrlMaxSize.SetWindowText(search->maxFileSize > 0 ? Util::toStringW(search->maxFileSize).c_str() : _T(""));
	ctrlActive.SetCheck(search->isActive ? 1 : 0);
	ctrlAutoQueue.SetCheck(search->isAutoQueue ? 1 : 0);
	ctrlSearchType.SetCurSel(search->sourceType);
	ctrlSizeType.SetCurSel(search->typeFileSize);
	::SendMessage(GetDlgItem(IDC_IS_FORBIDDEN), BM_SETCHECK, search->isForbidden ? 1 : 0, 0L);
	
	// Center dialog
	CenterWindow(GetParent());
	
	return FALSE;
}

// Exit dialog
LRESULT ADLSProperties::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		// Update search
		tstring buf;
		
		WinUtil::GetWindowText(buf, ctrlSearch);
		search->searchString = Text::fromT(buf);
		WinUtil::GetWindowText(buf, ctrlDestDir);
		search->destDir = Text::fromT(buf);
		
		WinUtil::GetWindowText(buf, ctrlMinSize);
		search->minFileSize = (buf.empty() ? -1 : Util::toInt64(buf));
		WinUtil::GetWindowText(buf, ctrlMaxSize);
		search->maxFileSize = (buf.empty() ? -1 : Util::toInt64(buf));
		
		search->isActive = (ctrlActive.GetCheck() == 1);
		search->isAutoQueue = (ctrlAutoQueue.GetCheck() == 1);
		
		search->sourceType = (ADLSearch::SourceType)ctrlSearchType.GetCurSel();
		search->typeFileSize = (ADLSearch::SizeType)ctrlSizeType.GetCurSel();
		search->isForbidden = (::SendMessage(GetDlgItem(IDC_IS_FORBIDDEN), BM_GETCHECK, 0, 0L) != 0);
		if (search->isForbidden)
		{
			search->destDir = "Forbidden Files";
		}
		search->raw = cRaw.GetCurSel();
	}
	
	EndDialog(wID);
	return 0;
}

/**
 * @file
 * $Id: ADLSProperties.cpp,v 1.12 2006/08/21 18:21:36 bigmuscle Exp $
 */
