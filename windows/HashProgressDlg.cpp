#include "stdafx.h"
#include "Resource.h"
#include "WinUtil.h"
#include "HashProgressDlg.h"
// #include "../client/version.h"
// TODO не могу понять почему дефан из хедера не подтягиватся!
#define FLYLINKDC_REGISTRY_MEDIAINFO_FREEZE_KEY _T("MediaFreezeInfo")

LRESULT HashProgressDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	// Translate static strings
	SetWindowText(CTSTRING(HASH_PROGRESS));
	SetDlgItemText(IDOK, CTSTRING(HASH_PROGRESS_BACKGROUND));
	SetDlgItemText(IDC_STATISTICS, CTSTRING(HASH_PROGRESS_STATS));
	SetDlgItemText(IDC_HASH_INDEXING, CTSTRING(HASH_PROGRESS_TEXT));
	SetDlgItemText(IDC_BTN_ABORT, CTSTRING(HASH_ABORT_TEXT));
	SetDlgItemText(IDC_BTN_EXIT_ON_DONE, CTSTRING(EXIT_ON_HASHING_DONE_TEXT));
	SetDlgItemText(IDC_CHANGE_HASH_SPEED, CTSTRING(CHANGE_HASH_SPEED_TEXT));
	SetDlgItemText(IDC_CURRENT_HASH_SPEED, CTSTRING(CURRENT_HASH_SPEED_TEXT));
	SetDlgItemText(IDC_PAUSE, HashManager::getInstance()->isHashingPaused() ? CTSTRING(RESUME) : CTSTRING(PAUSED));
	// [+] SCALOlaz: add mediainfo size
#ifdef SCALOLAZ_HASH_HELPLINK
	m_HashHelp.init(GetDlgItem(IDC_MEDIAINFO_SIZE_TXT), _T(""));
	const tstring l_url = WinUtil::GetWikiLink() + _T("mediainfo");
	m_HashHelp.SetHyperLink(l_url.c_str());
	m_HashHelp.SetHyperLinkExtendedStyle(/*HLINK_LEFTIMAGE |*/ HLINK_UNDERLINEHOVER);
	m_HashHelp.SetLabel(CTSTRING(SETTINGS_MIN_MEDIAINFO_SIZE));
	m_HashHelp.SetFont(Fonts::g_systemFont, FALSE);
#else //not SCALOLAZ_HASH_HELPLINK
	SetDlgItemText(IDC_MEDIAINFO_SIZE_TXT, CTSTRING(SETTINGS_MIN_MEDIAINFO_SIZE));
#endif //SCALOLAZ_HASH_HELPLINK
	SetDlgItemInt(IDC_MEDIAINFO_SIZE, SETTING(MIN_MEDIAINFO_SIZE), FALSE);
	SetDlgItemText(IDC_MEDIAINFO_SIZE_MB, CTSTRING(MB));
	// SCALOlaz
	ExitOnDoneButton.Attach(GetDlgItem(IDC_BTN_EXIT_ON_DONE));
	ExitOnDoneButton.SetCheck(bExitOnDone);
	
	if (!bExitOnDone)
		ExitOnDoneButton.ShowWindow(SW_HIDE);
		
	SetDlgItemInt(IDC_EDIT_MAX_HASH_SPEED, HashManager::getInstance()->GetMaxHashSpeed(), FALSE);
	
	progress.Attach(GetDlgItem(IDC_HASH_PROGRESS));
	progress.SetRange(0, HashManager::GetMaxProgressValue());
	updateStats();
	
	HashManager::getInstance()->setPriority(Thread::NORMAL);
	
	create_timer(1000);
	return TRUE;
}

LRESULT HashProgressDlg::onPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (HashManager::getInstance()->isHashingPaused())
	{
		HashManager::getInstance()->resumeHashing();
		SetDlgItemText(IDC_PAUSE, CTSTRING(PAUSED));
	}
	else
	{
		HashManager::getInstance()->pauseHashing();
		SetDlgItemText(IDC_PAUSE, CTSTRING(RESUME));
	}
	return 0;
}

LRESULT HashProgressDlg::onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	safe_destroy_timer();
	HashManager::getInstance()->setPriority(Thread::IDLE);
	HashManager::getInstance()->DisableForceMinHashSpeed();
	progress.Detach();
	return 0;
}

void HashProgressDlg::updateStats()
{
	string file;
	int64_t bytes = 0;
	size_t files = 0;
	const uint64_t tick = GET_TICK();
	
	HashManager::getInstance()->getStats(file, bytes, files);
	
	if (files == 0)
	{
		if (ExitOnDoneButton.GetCheck())
		{
			EndDialog(IDC_BTN_EXIT_ON_DONE);
			return;
		}
		
		if (autoClose)
		{
			PostMessage(WM_CLOSE);
			return;
		}
	}
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
	extern string g_cur_mediainfo_file_tth;
	extern string g_cur_mediainfo_file;
	if (files == 0)
	{
		SetDlgItemText(IDC_CURRENT_FILE, CTSTRING(DONE));
	}
	else
	{
		SetDlgItemText(IDC_CURRENT_FILE, Text::toT(Text::toLabel(file)).c_str());
	}
	if (g_cur_mediainfo_file_tth.empty())
	{
		SetDlgItemText(IDC_CURRENT_TTH, _T(""));
		if (!m_cur_mediainfo_file_tth.empty()) // Раньше записывали в реестр значение?
		{
			m_cur_mediainfo_file_tth.clear();
			Util::deleteRegistryValue(FLYLINKDC_REGISTRY_MEDIAINFO_FREEZE_KEY);
		}
	}
	else
	{
		SetDlgItemText(IDC_CURRENT_TTH, Text::toT("TTH: " + g_cur_mediainfo_file_tth + " (mediainfo)").c_str());
		if (m_cur_mediainfo_file_tth != g_cur_mediainfo_file_tth) // Исключем постоянную запись в реестр
		{
			m_cur_mediainfo_file_tth = g_cur_mediainfo_file_tth;
			Util::setRegistryValueString(FLYLINKDC_REGISTRY_MEDIAINFO_FREEZE_KEY, Text::toT(g_cur_mediainfo_file));
		}
	}
	
	progress.SetPos(HashManager::getInstance()->GetProgressValue());
	SetDlgItemText(IDC_PAUSE, paused ? CTSTRING(RESUME) : CTSTRING(PAUSED)); // KUL - hash progress dialog patch
}
