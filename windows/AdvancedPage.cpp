﻿/*
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
#include "AdvancedPage.h"
#include "CommandDlg.h"

PropPage::TextItem AdvancedPage::texts[] =
{
	{ IDC_MAGNET_URL_TEMPLATE, ResourceManager::SETCZDC_MAGNET_URL_TEMPLATE },
	{ IDC_CZDC_WINAMP, ResourceManager::SETCZDC_WINAMP },
	{ IDC_CZDC_RATIOMSG, ResourceManager::CZDC_RATIOMSG},
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item AdvancedPage::items[] =
{
	{ IDC_EWMAGNET_TEMPL, SettingsManager::COPY_WMLINK, PropPage::T_STR},
	{ IDC_RATIOMSG, SettingsManager::RATIO_TEMPLATE, PropPage::T_STR},
	{ 0, 0, PropPage::T_END }
};

AdvancedPage::ListItem AdvancedPage::listItems[] =
{
#ifdef FLYLINKDC_USE_AUTO_FOLLOW
	{ SettingsManager::AUTO_FOLLOW, ResourceManager::SETTINGS_AUTO_FOLLOW },
#endif
	{ SettingsManager::STARTUP_BACKUP, ResourceManager::STARTUP_BACKUP },
	{ SettingsManager::KEEP_LISTS, ResourceManager::SETTINGS_KEEP_LISTS },
	{ SettingsManager::AUTO_KICK, ResourceManager::SETTINGS_AUTO_KICK },
	{ SettingsManager::AUTO_KICK_NO_FAVS, ResourceManager::SETTINGS_AUTO_KICK_NO_FAVS },
	{ SettingsManager::COMPRESS_TRANSFERS, ResourceManager::SETTINGS_COMPRESS_TRANSFERS },
	{ SettingsManager::HUB_USER_COMMANDS, ResourceManager::SETTINGS_HUB_USER_COMMANDS },
	{ SettingsManager::SEND_UNKNOWN_COMMANDS, ResourceManager::SETTINGS_SEND_UNKNOWN_COMMANDS },
	{ SettingsManager::SEND_BLOOM, ResourceManager::SETTINGS_SEND_BLOOM },
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
	{ SettingsManager::ADC_DEBUG, ResourceManager::SETTINGS_ADC_DEBUG },
	{ SettingsManager::NMDC_DEBUG, ResourceManager::SETTINGS_NMDC_DEBUG },
#endif
	{ SettingsManager::SHOW_SHELL_MENU, ResourceManager::SETTINGS_SHOW_SHELL_MENU },
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	{ SettingsManager::ENABLE_LAST_IP_AND_MESSAGE_COUNTER, ResourceManager::ENABLE_LAST_IP_AND_MESSAGE_COUNTER },
#endif
	{ SettingsManager::ENABLE_HIT_FILE_LIST, ResourceManager::ENABLE_HIT_FILE_LIST },
	{ SettingsManager::ENABLE_RATIO_USER_LIST, ResourceManager::ENABLE_RATIO_USER_LIST },
	{ SettingsManager::REDUCE_PRIORITY_IF_MINIMIZED_TO_TRAY, ResourceManager::REDUCE_PRIORITY_IF_MINIMIZED },
	{ SettingsManager::SQLITE_USE_JOURNAL_MEMORY, ResourceManager::SQLITE_USE_JOURNAL_MEMORY },
	{ SettingsManager::USE_MAGNETS_IN_PLAYERS_SPAM, ResourceManager::USE_MAGNETS_IN_PLAYERS_SPAM },
	{ SettingsManager::USE_BITRATE_FIX_FOR_SPAM, ResourceManager::USE_BITRATE_FIX_FOR_SPAM},
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT AdvancedPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
	
	CurSel = SETTING(MEDIA_PLAYER);
	WMPlayerStr = Text::toT(SETTING(WMP_FORMAT));
	WinampStr = Text::toT(SETTING(WINAMP_FORMAT));
	iTunesStr = Text::toT(SETTING(ITUNES_FORMAT));
	MPCStr = Text::toT(SETTING(MPLAYERC_FORMAT));
	JAStr = Text::toT(SETTING(JETAUDIO_FORMAT));
	QCDQMPStr = Text::toT(SETTING(QCDQMP_FORMAT));
	
	ctrlList.Attach(GetDlgItem(IDC_ADVANCED_BOOLEANS));
	
	ctrlPlayer.Attach(GetDlgItem(IDC_PLAYER_COMBO));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_WINAMP));//  _T("Winamp (AIMP)"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_WMP)); //_T("Windows Media Player"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_ITUNES)); //_T("iTunes"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_WPC)); //_T("Media Player Classic"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_JA)); // _T("jetAudio Player"));
	ctrlPlayer.AddString(CTSTRING(MEDIA_MENU_QCDQMP));
	ctrlPlayer.SetCurSel(CurSel);
	
	switch (CurSel)
	{
		case SettingsManager::WinAmp:
			SetDlgItemText(IDC_WINAMP, WinampStr.c_str());
			break;
		case SettingsManager::WinMediaPlayer:
			SetDlgItemText(IDC_WINAMP, WMPlayerStr.c_str());
			break;
		case SettingsManager::iTunes:
			iTunesStr = SetDlgItemText(IDC_WINAMP, iTunesStr.c_str());
			break;
		case SettingsManager::WinMediaPlayerClassic:
			SetDlgItemText(IDC_WINAMP, MPCStr.c_str());
			break;
		case SettingsManager::JetAudio:
			SetDlgItemText(IDC_WINAMP, JAStr.c_str());
			break;
		case SettingsManager::QCDQMP:
			SetDlgItemText(IDC_WINAMP, QCDQMPStr.c_str());
			break;
		default:
		{
			SetDlgItemText(IDC_WINAMP, CTSTRING(NO_MEDIA_SPAM));
			::EnableWindow(GetDlgItem(IDC_WINAMP), false);
			::EnableWindow(GetDlgItem(IDC_WINAMP_HELP), false);
		}
		break;
	}
	
	//bInited = true;
	
	// Do specialized reading here
	return TRUE;
}

void AdvancedPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
	
	ctrlFormat.Attach(GetDlgItem(IDC_WINAMP));
	
	tstring buf;
	WinUtil::GetWindowText(buf, ctrlFormat);
	
	ctrlFormat.Detach();
	
	switch (CurSel)
	{
		case SettingsManager::WinAmp:
			WinampStr = buf;
			break;
		case SettingsManager::WinMediaPlayer:
			WMPlayerStr = buf;
			break;
		case SettingsManager::iTunes:
			iTunesStr = buf;
			break;
		case SettingsManager::WinMediaPlayerClassic:
			MPCStr = buf;
			break;
		case SettingsManager::JetAudio:
			JAStr = buf;
			break;
		case SettingsManager::QCDQMP:
			QCDQMPStr = buf;
			break;
	}
	
	SET_SETTING(MEDIA_PLAYER, ctrlPlayer.GetCurSel());
	SET_SETTING(WINAMP_FORMAT, Text::fromT(WinampStr).c_str());
	SET_SETTING(WMP_FORMAT, Text::fromT(WMPlayerStr).c_str());
	SET_SETTING(ITUNES_FORMAT, Text::fromT(iTunesStr).c_str());
	SET_SETTING(MPLAYERC_FORMAT, Text::fromT(MPCStr).c_str());
	SET_SETTING(JETAUDIO_FORMAT, Text::fromT(JAStr).c_str());
	SET_SETTING(QCDQMP_FORMAT, Text::fromT(QCDQMPStr).c_str());
}

LRESULT AdvancedPage::onClickedWinampHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	switch (CurSel)
	{
		case SettingsManager::WinAmp:
			MessageBox(CTSTRING(WINAMP_HELP), CTSTRING(WINAMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::WinMediaPlayer:
			MessageBox(CTSTRING(WMP_HELP_STR), CTSTRING(WMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::iTunes:
			MessageBox(CTSTRING(ITUNES_HELP), CTSTRING(ITUNES_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::WinMediaPlayerClassic:
			MessageBox(CTSTRING(MPC_HELP), CTSTRING(MPC_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::JetAudio:
			MessageBox(CTSTRING(JA_HELP), CTSTRING(JA_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
		case SettingsManager::QCDQMP:
			MessageBox(CTSTRING(QCDQMP_HELP), CTSTRING(QCDQMP_HELP_DESC), MB_OK | MB_ICONINFORMATION);
			break;
	}
	
	return S_OK;
}

LRESULT AdvancedPage::onClickedRatioMsgHelp(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	MessageBox(CTSTRING(RATIO_MSG_HELP), CTSTRING(RATIO_MSG_HELP_DESC), MB_OK | MB_ICONINFORMATION);
	return S_OK;
}

LRESULT AdvancedPage::onSelChange(WORD /* wNotifyCode */, WORD /*wID*/, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	ctrlFormat.Attach(GetDlgItem(IDC_WINAMP));
	
	tstring buf;
	WinUtil::GetWindowText(buf, ctrlFormat);
	
	switch (CurSel)
	{
		case SettingsManager::WinAmp:
			WinampStr = buf;
			break;
		case SettingsManager::WinMediaPlayer:
			WMPlayerStr = buf;
			break;
		case SettingsManager::iTunes:
			iTunesStr = buf;
			break;
		case SettingsManager::WinMediaPlayerClassic:
			MPCStr = buf;
			break;
		case SettingsManager::JetAudio:
			JAStr = buf;
			break;
		case SettingsManager::QCDQMP:
			QCDQMPStr = buf;
			break;
	}
	
	ctrlFormat.Detach();
	
	CurSel = ctrlPlayer.GetCurSel();
	switch (CurSel)
	{
		case SettingsManager::WinAmp:
			SetDlgItemText(IDC_WINAMP, WinampStr.c_str());
			break;
		case SettingsManager::WinMediaPlayer:
			SetDlgItemText(IDC_WINAMP, WMPlayerStr.c_str());
			break;
		case SettingsManager::iTunes:
			SetDlgItemText(IDC_WINAMP, iTunesStr.c_str());
			break;
		case SettingsManager::WinMediaPlayerClassic:
			SetDlgItemText(IDC_WINAMP, MPCStr.c_str());
			break;
		case SettingsManager::JetAudio:
			SetDlgItemText(IDC_WINAMP, JAStr.c_str());
			break;
		case SettingsManager::QCDQMP:
			SetDlgItemText(IDC_WINAMP, QCDQMPStr.c_str());
			break;
		default:
		{
			SetDlgItemText(IDC_WINAMP, CTSTRING(NO_MEDIA_SPAM));
		}
		break;
	}
	
	bool isPlayerSelected = (CurSel >= SettingsManager::WinAmp && CurSel < SettingsManager::PlayersCount);
	
	::EnableWindow(GetDlgItem(IDC_WINAMP), isPlayerSelected);
	::EnableWindow(GetDlgItem(IDC_WINAMP_HELP), isPlayerSelected);
	
	return 0;
}

