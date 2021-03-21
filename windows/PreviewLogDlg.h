/*
 * Copyright (C) 2012-2021 FlylinkDC++ Team http://flylinkdc.com
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

#if !defined(_PREVIEW_LOG_DLG_H_)
#define _PREVIEW_LOG_DLG_H_

#pragma once

#include "../client/Util.h"

#ifdef SSA_VIDEO_PREVIEW_FEATURE

#include "../FlyFeatures/VideoPreview.h"

class PreviewLogDlg : public CDialogImpl<PreviewLogDlg>
{

	public:
	
		enum { IDD = IDD_PREVIEW_LOG_DLG };
		
		BEGIN_MSG_MAP(PreviewLogDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_PREVIEW_SERVER_LOG, onReceiveLogItem)
		COMMAND_ID_HANDLER(IDC_PRV_DLG_SERVER_STATE_BTN, onStartStop)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		END_MSG_MAP()
		
		PreviewLogDlg() { }
		~PreviewLogDlg();
		
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onReceiveLogItem(UINT /*wNotifyCode*/, WPARAM wID, LPARAM /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onStartStop(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	private:
		void UpdateItems();
};

#endif // SSA_VIDEO_PREVIEW_FEATURE

#endif // _PREVIEW_LOG_DLG_H_