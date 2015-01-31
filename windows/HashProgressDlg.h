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

#if !defined(HASH_PROGRESS_DLG_H)
#define HASH_PROGESS_DLG_H

#include "../client/HashManager.h"
#ifdef SCALOLAZ_HASH_HELPLINK
#include "wtl_flylinkdc.h"
#endif
class HashProgressDlg : public CDialogImpl<HashProgressDlg>, private CFlyTimerAdapter
{
	public:
		enum { IDD = IDD_HASH_PROGRESS };
		
		HashProgressDlg(bool p_AutoClose, bool p_bExitOnDone = false) : CFlyTimerAdapter(m_hWnd), autoClose(p_AutoClose), bExitOnDone(p_bExitOnDone)
		{
			dcassert(g_is_execute == 0);
			++g_is_execute;
		}
		~HashProgressDlg()
		{
			--g_is_execute;
		}
		
		BEGIN_MSG_MAP(HashProgressDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_BTN_ABORT, OnBnClickedBtnAbort)
		COMMAND_HANDLER(IDC_EDIT_MAX_HASH_SPEED, EN_CHANGE, OnEnChangeEditMaxHashSpeed)
		COMMAND_ID_HANDLER(IDC_PAUSE, onPause)
		COMMAND_ID_HANDLER(IDC_BTN_REFRESH_FILELIST, onRefresh)
		MESSAGE_HANDLER(WM_HSCROLL, onSlideChangeMaxHashSpeed)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		LRESULT onPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onSlideChangeMaxHashSpeed(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		LRESULT onRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		void updateStats();
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT OnBnClickedBtnAbort(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT OnEnChangeEditMaxHashSpeed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
	private:
		HashProgressDlg(const HashProgressDlg&);
		
		bool autoClose;
		bool bExitOnDone;
		CProgressBarCtrl progress;
		CButton ExitOnDoneButton;
		string m_cur_mediainfo_file_tth;
		CTrackBarCtrl m_Slider;
#ifdef SCALOLAZ_HASH_HELPLINK
		CFlyHyperLink m_HashHelp;
#endif
	public:
		static int g_is_execute;
};

#endif // !defined(HASH_PROGRESS_DLG_H)

/**
 * @file
 * $Id: HashProgressDlg.h,v 1.16 2006/10/13 20:04:32 bigmuscle Exp $
 */
