/*
 * Copyright (C) 2012-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#ifdef SSA_VIDEO_PREVIEW_FEATURE

#include "Resource.h"
#include "PreviewLogDlg.h"
#include "WinUtil.h"
#include "HIconWrapper.h"



// Отключение диалога.
PreviewLogDlg::~PreviewLogDlg()
{
}

LRESULT PreviewLogDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	GetDlgItem(IDC_PRV_DLG_SERVER_GROUP).SetWindowText(CTSTRING(PRV_DLG_SERVER_GROUP));
	GetDlgItem(IDC_PRV_DLG_SERVER_STATE_STATIC).SetWindowText(CTSTRING(PRV_DLG_SERVER_STATE_STATIC));
	
	
	GetDlgItem(IDC_PRV_DLG_VIDEO_NAME_STATIC).SetWindowText(CTSTRING(PRV_DLG_VIDEO_NAME_STATIC));
	GetDlgItem(IDC_PRV_DLG_LOG_GROUP).SetWindowText(CTSTRING(PRV_DLG_LOG_GROUP));
	
	CenterWindow(GetParent());
	GetDlgItem(IDCANCEL).SetWindowText(CTSTRING(CLOSE));
	
	SetWindowText(CTSTRING(PREVIEW_LOG_DLG));
	
	UpdateItems();
	
	// Fill IDC_PRV_DLG_LOG_LST
	
	CListBox mBox;
	mBox.Attach(GetDlgItem(IDC_PRV_DLG_LOG_LST));
	string outString;
	while (VideoPreview::getInstance()->GetNextLogItem(outString))
		mBox.AddString(Text::toT(outString).c_str());
	mBox.Detach();
	
	VideoPreview::getInstance()->SetLogDlgWnd(*this);
	
	return FALSE;
}
LRESULT PreviewLogDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	EndDialog(wID);
	return 0;
}

LRESULT PreviewLogDlg::onReceiveLogItem(UINT /*wNotifyCode*/, WPARAM wID, LPARAM /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CListBox mBox;
	mBox.Attach(GetDlgItem(IDC_PRV_DLG_LOG_LST));
	string outString;
	if (VideoPreview::getInstance()->GetNextLogItem(outString))
		mBox.AddString(Text::toT(outString).c_str());
		
	mBox.SetTopIndex(mBox.GetCount() - 1);
	mBox.Detach();
	
	UpdateItems();
	
	// Make Scroll
	
	return 0;
}

void PreviewLogDlg::UpdateItems()
{
	if (VideoPreview::getInstance()->IsServerStarted())
	{
		GetDlgItem(IDC_PRV_DLG_SERVER_STATE).SetWindowText(CTSTRING(LOADED));
		GetDlgItem(IDC_PRV_DLG_SERVER_STATE_BTN).SetWindowText(CTSTRING(BTN_STOP));
	}
	else
	{
		GetDlgItem(IDC_PRV_DLG_SERVER_STATE).SetWindowText(CTSTRING(STOPPED));
		GetDlgItem(IDC_PRV_DLG_SERVER_STATE_BTN).SetWindowText(CTSTRING(BTN_START));
	}
	
	string videoInPreview = VideoPreview::getInstance()->GetFilePreviewName();
	GetDlgItem(IDC_PRV_DLG_VIDEO_NAME).SetWindowText(Text::toT(videoInPreview).c_str());
	
	
	if (VideoPreview::getInstance()->IsPreviewFileExists())
		GetDlgItem(IDC_PRV_DLG_VIDEO_STATUS).SetWindowText(CTSTRING(PRV_DLG_VIDEO_STATUS_EXIST));
	else
		GetDlgItem(IDC_PRV_DLG_VIDEO_STATUS).SetWindowText(CTSTRING(PRV_DLG_VIDEO_STATUS_DOWN));
		
}

LRESULT PreviewLogDlg::onStartStop(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (VideoPreview::getInstance()->IsServerStarted())
		VideoPreview::getInstance()->StopServer();
	else
		VideoPreview::getInstance()->StartServer();
		
	UpdateItems();
	
	return 0;
}

#endif // SSA_VIDEO_PREVIEW_FEATURE
