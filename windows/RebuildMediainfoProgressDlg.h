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

#if !defined(REBUILD_MEDIAINFO_PROGRESS_DLG_H)
#define REBUILD_MEDIAINFO_PROGESS_DLG_H

#pragma once


class RebuildMediainfoProgressDlg : public CDialogImpl<RebuildMediainfoProgressDlg>, private CFlyTimerAdapter
{
	public:
		enum { IDD = IDD_REBUILD_MEDIAINFO_PROGRESS };
		
		RebuildMediainfoProgressDlg(): CFlyTimerAdapter(m_hWnd)
		{
		}
		~RebuildMediainfoProgressDlg() { }
		
		BEGIN_MSG_MAP(RebuildMediainfoProgressDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_BTN_ABORT, OnBnClickedBtnAbort)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			// Translate static strings
			SetWindowText(CTSTRING(HASH_PROGRESS));
			SetDlgItemText(IDOK, CTSTRING(HASH_PROGRESS_BACKGROUND));
			SetDlgItemText(IDC_STATISTICS, CTSTRING(HASH_PROGRESS_STATS));
			SetDlgItemText(IDC_HASH_INDEXING, CTSTRING(HASH_PROGRESS_TEXT));
			SetDlgItemText(IDC_BTN_ABORT, CTSTRING(HASH_ABORT_TEXT));
			SetDlgItemText(IDC_CHANGE_HASH_SPEED, CTSTRING(CHANGE_HASH_SPEED_TEXT));
			SetDlgItemText(IDC_CURRENT_HASH_SPEED, CTSTRING(CURRENT_HASH_SPEED_TEXT));
			
			m_progress.Attach(GetDlgItem(IDC_HASH_PROGRESS));
			m_progress.SetRange(0, 1000);
			updateStats();
			
			create_timer(1000);
			
			return TRUE;
		}
		
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			updateStats();
			return 0;
		}
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			safe_destroy_timer();
			m_progress.Detach();
			return 0;
		}
		
		void updateStats()
		{
			/*
			string file;
			int64_t bytes = 0;
			size_t files = 0;
			const uint64_t tick = GET_TICK();
			*/
			static int ll = 0;
			m_progress.SetPos(++ll);
			/*
			
			HashManager::getInstance()->getStats(file, bytes, files);
			
			const int64_t diff = tick - HashManager::getInstance()->GetStartTime();
			const bool paused = HashManager::getInstance()->isHashingPaused();
			if (diff < 1000 || files == 0 || bytes == 0 || paused)
			{
			    SetDlgItemText(IDC_FILES_PER_HOUR, (_T("-.-- ") + TSTRING(FILES_PER_HOUR) + _T(", ") + Util::toStringW(files) + _T(' ') + TSTRING(FILES_LEFT)).c_str());
			    SetDlgItemText(IDC_HASH_SPEED, (_T("-.-- ") + TSTRING(B) + _T('/') + TSTRING(S) + _T(", ") + Util::formatBytesW(bytes) + _T(' ') + TSTRING(LEFT)).c_str());
			    if (paused)
			    {
			        SetDlgItemText(IDC_TIME_LEFT, (_T("( ") + TSTRING(PAUSED) + _T(" )")).c_str());
			    }
			    else
			    {
			        SetDlgItemText(IDC_TIME_LEFT, (_T("-:--:-- ") + TSTRING(LEFT)).c_str());
			        progress.SetPos(0);
			    }
			}
			else
			{
			
			    int64_t processedBytes;
			    size_t processedFiles;
			    HashManager::getInstance()->GetProcessedFilesAndBytesCount(processedBytes, processedFiles);
			
			    const int64_t filestat = processedFiles * 60 * 60 * 1000 / diff;
			    const int64_t speedStat = processedBytes * 1000 / diff;
			
			    SetDlgItemText(IDC_FILES_PER_HOUR, (Util::toStringW(filestat) + _T(' ') + TSTRING(FILES_PER_HOUR) + _T(", ") + Util::toStringW((uint32_t)files) + _T(' ') + TSTRING(FILES_LEFT)).c_str());
			    SetDlgItemText(IDC_HASH_SPEED, (Util::formatBytesW(speedStat) + _T('/') + TSTRING(S) + _T(", ") + Util::formatBytesW(bytes) + _T(' ') + TSTRING(LEFT)).c_str());
			
			    if (EqualD(filestat, 0) || EqualD(speedStat, 0))
			    {
			        SetDlgItemText(IDC_TIME_LEFT, (_T("-:--:-- ") + TSTRING(LEFT)).c_str());
			    }
			    else
			    {
			        const int64_t fs = files * 60 * 60 / filestat;
			        const int64_t ss = bytes / speedStat;
			
			        SetDlgItemText(IDC_TIME_LEFT, (Util::formatSecondsW((int64_t)(fs + ss) / 2) + _T(' ') + TSTRING(LEFT)).c_str());
			    }
			}
			
			if (files == 0)
			{
			    SetDlgItemText(IDC_CURRENT_FILE, CTSTRING(DONE));
			}
			else
			{
			    SetDlgItemText(IDC_CURRENT_FILE, Text::toT(file).c_str());
			}
			
			progress.SetPos(HashManager::getInstance()->GetProgressValue());
			*/
		}
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			EndDialog(wID);
			return 0;
		}
		
		LRESULT OnBnClickedBtnAbort(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			// HashManager::getInstance()->stopHashing(Util::emptyString);
			return 0;
		}
		
	private:
		RebuildMediainfoProgressDlg(const RebuildMediainfoProgressDlg&);
	public:
		CProgressBarCtrl m_progress;
};

#endif // !defined(REBUILD_MEDIAINFO_PROGRESS_DLG_H)
