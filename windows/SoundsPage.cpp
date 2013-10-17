/*
 * Copyright (C) 2001-2003 Jacek Sieka, j_s@telia.com
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
#include "SoundsPage.h"
#include "WinUtil.h"

PropPage::TextItem Sounds::texts[] =
{
	{ IDC_PRIVATE_MESSAGE_BEEP, ResourceManager::SETTINGS_PM_BEEP },
	{ IDC_PRIVATE_MESSAGE_BEEP_OPEN, ResourceManager::SETTINGS_PM_BEEP_OPEN },
	{ IDC_CZDC_SOUND, ResourceManager::SETTINGS_SOUNDS },
	//{ IDC_BROWSE, ResourceManager::BROWSE }, // [~] JhaoDa, not necessary any more
	{ IDC_PLAY, ResourceManager::PLAY },
	{ IDC_NONE, ResourceManager::NONE },
	{ IDC_DEFAULT, ResourceManager::DEFAULT },
	{ IDC_SOUNDS, ResourceManager::SOUND_THEME },   // [+] SCALOlaz: Sounds Combo
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item Sounds::items[] =
{
	{ IDC_PRIVATE_MESSAGE_BEEP, SettingsManager::PRIVATE_MESSAGE_BEEP, PropPage::T_BOOL },
	{ IDC_PRIVATE_MESSAGE_BEEP_OPEN, SettingsManager::PRIVATE_MESSAGE_BEEP_OPEN, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

Sounds::snds Sounds::g_sounds[] =
{
	{ ResourceManager::SOUND_DOWNLOAD_BEGINS,   SettingsManager::SOUND_BEGINFILE, ""},
	{ ResourceManager::SOUND_DOWNLOAD_FINISHED, SettingsManager::SOUND_FINISHFILE, ""},
	{ ResourceManager::SOUND_SOURCE_ADDED,  SettingsManager::SOUND_SOURCEFILE, ""},
	{ ResourceManager::SOUND_UPLOAD_FINISHED,   SettingsManager::SOUND_UPLOADFILE, ""},
	{ ResourceManager::SOUND_FAKER_FOUND,   SettingsManager::SOUND_FAKERFILE, ""},
	{ ResourceManager::SETCZDC_PRIVATE_SOUND,   SettingsManager::SOUND_BEEPFILE, ""},
	{ ResourceManager::MYNICK_IN_CHAT,  SettingsManager::SOUND_CHATNAMEFILE, ""},
	{ ResourceManager::SOUND_TTH_INVALID,   SettingsManager::SOUND_TTH, ""},
	{ ResourceManager::HUB_CONNECTED,   SettingsManager::SOUND_HUBCON, ""},
	{ ResourceManager::HUB_DISCONNECTED,    SettingsManager::SOUND_HUBDISCON, ""},
	{ ResourceManager::FAVUSER_ONLINE,  SettingsManager::SOUND_FAVUSER, ""},
	{ ResourceManager::FAVUSER_OFFLINE,      SettingsManager::SOUND_FAVUSER_OFFLINE, ""},
	{ ResourceManager::SOUND_TYPING_NOTIFY, SettingsManager::SOUND_TYPING_NOTIFY, ""},
	{ ResourceManager::SOUND_SEARCHSPY, SettingsManager::SOUND_SEARCHSPY, ""}
};

LRESULT Sounds::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	ctrlSNDTheme.Attach(GetDlgItem(IDC_SOUNDS_COMBO));
	
	GetSNDThemeList();
	for (auto i = m_SNDThemeList.cbegin(); i != m_SNDThemeList.cend(); ++i)
		ctrlSNDTheme.AddString(i->first.c_str());
		
	const int ind = WinUtil::getIndexFromMap(m_SNDThemeList, SETTING(THEME_MANAGER_SOUNDS_THEME_NAME));
	if (ind < 0)
	{
		ctrlSNDTheme.SetCurSel(0);  // Если когда-то был выбран пакет, но его удалили физически, ставим по умолчанию
		DefaultAllSounds();
	}
	else
		ctrlSNDTheme.SetCurSel(ind);
		
	ctrlSNDTheme.Detach();
	
	
	SetDlgItemText(IDC_SOUND_ENABLE, CTSTRING(ENABLE_SOUNDS));
	CheckDlgButton(IDC_SOUND_ENABLE, SETTING(SOUNDS_DISABLED) ? BST_UNCHECKED : BST_CHECKED);
	
	ctrlSounds.Attach(GetDlgItem(IDC_SOUNDLIST));
	CRect rc;
	ctrlSounds.GetClientRect(rc);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlSounds);
#ifdef USE_SET_LIST_COLOR_IN_SETTINGS
	SET_LIST_COLOR_IN_SETTING(ctrlSounds);
#endif
	ctrlSounds.InsertColumn(0, CTSTRING(SETTINGS_SOUNDS), LVCFMT_LEFT, (rc.Width() / 3) * 1, 0);
	ctrlSounds.InsertColumn(1, CTSTRING(FILENAME), LVCFMT_LEFT, (rc.Width() / 3) * 2, 1);
	
	// Do specialized reading here
	
	for (int i = 0; i < _countof(g_sounds); i++)
	{
		int j = ctrlSounds.insert(i, Text::toT(ResourceManager::getString(g_sounds[i].name)).c_str());
		g_sounds[i].value = SettingsManager::get((SettingsManager::StrSetting)g_sounds[i].setting, true);
		ctrlSounds.SetItemText(j, 1, Text::toT(g_sounds[i].value).c_str());
	}
	
	fixControls();
	return TRUE;
}


void Sounds::write()
{
	PropPage::write((HWND)*this, items);
	
	for (int i = 0; i < _countof(g_sounds); i++)
	{
		settings->set((SettingsManager::StrSetting)g_sounds[i].setting, ctrlSounds.ExGetItemText(i, 1));
	}
	
	SET_SETTING(SOUNDS_DISABLED, IsDlgButtonChecked(IDC_SOUND_ENABLE) == 1 ? false : true);
	
	ctrlSNDTheme.Attach(GetDlgItem(IDC_SOUNDS_COMBO));
	const string l_filetheme = WinUtil::getDataFromMap(ctrlSNDTheme.GetCurSel(), m_SNDThemeList);
	if (SETTING(THEME_MANAGER_SOUNDS_THEME_NAME) != l_filetheme)
		settings->set(SettingsManager::THEME_MANAGER_SOUNDS_THEME_NAME, l_filetheme);
		
	ctrlSNDTheme.Detach();
	
	// Do specialized writing here
	// settings->set(XX, YY);
}

LRESULT Sounds::onBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LocalArray<TCHAR, MAX_PATH> buf;
	LVITEM item = {0};
	item.mask = LVIF_TEXT;
	item.cchTextMax = 255;
	item.pszText = buf.data();
	if (ctrlSounds.GetSelectedItem(&item))
	{
		tstring x;
		if (WinUtil::browseFile(x, m_hWnd, false) == IDOK)
		{
			ctrlSounds.SetItemText(item.iItem, 1, x.c_str());
		}
	}
	return 0;
}

LRESULT Sounds::onClickedNone(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LocalArray<TCHAR, MAX_PATH> buf;
	LVITEM item = {0};
	item.mask = LVIF_TEXT;
	item.cchTextMax = 255;
	item.pszText = buf.data();
	
	if (ctrlSounds.GetSelectedItem(&item))
	{
		tstring x;
		ctrlSounds.SetItemText(item.iItem, 1, x.c_str());
	}
	
	fixControls();
	return 0;
}

LRESULT Sounds::onPlay(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LVITEM item = {0};
	item.mask = LVIF_TEXT;
	item.cchTextMax = 255;
	if (ctrlSounds.GetSelectedItem(&item))
	{
		PlaySound(ctrlSounds.ExGetItemTextT(item.iItem, 1).c_str(), NULL, SND_FILENAME | SND_ASYNC);
	}
	return 0;
}

static const TCHAR* g_SoundsTable[] =
{
	L"DownloadBegins.wav",
	L"DownloadFinished.wav",
	L"AltSourceAdded.wav",
	L"UploadFinished.wav",
	L"FakerFound.wav",
	L"PrivateMessage.wav",
	L"MyNickInMainChat.wav",
	L"FileCorrupted.wav",
	L"HubConnected.wav",
	L"HubDisconnected.wav",
	L"FavUser.wav",
	L"FavUserDisconnected.wav",
	L"TypingNotify.wav",
	L"SearchSpy.wav"
};

LRESULT Sounds::onDefault(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LocalArray<TCHAR, MAX_PATH> buf;
	LVITEM item = {0};
	item.mask = LVIF_TEXT;
	item.cchTextMax = 255;
	item.pszText = buf.data();
	if (ctrlSounds.GetSelectedItem(&item))
	{
		tstring l_themePath = Text::toT(Util::getSoundPath() + SETTING(THEME_MANAGER_SOUNDS_THEME_NAME));
		AppendPathSeparator(l_themePath);
		const tstring l_selectedSoundPath = l_themePath + g_SoundsTable[ctrlSounds.GetSelectionMark()];  // [~] SCALOlaz: remaked default setting with massive
		ctrlSounds.SetItemText(item.iItem, 1, l_selectedSoundPath.c_str());
	}
	return 0;
}

void Sounds::DefaultAllSounds() // [+] SCALOlaz: turn all sounds to default, on Sound Theme or default at choose Theme
{
	ctrlSNDTheme.Attach(GetDlgItem(IDC_SOUNDS_COMBO));
	const tstring l_themeName = Text::toT(WinUtil::getDataFromMap(ctrlSNDTheme.GetCurSel(), m_SNDThemeList));
	ctrlSNDTheme.Detach();
	
	tstring l_themePath = Text::toT(Util::getSoundPath()) + l_themeName;
	AppendPathSeparator(l_themePath);
	
	for (size_t i = 0; i < _countof(g_SoundsTable); ++i)
	{
		tstring l_currentSoundPath = l_themePath + g_SoundsTable[i];
		ctrlSounds.SetItemText(i, 1, l_currentSoundPath.c_str());
	}
}

void Sounds::fixControls()
{
	BOOL l_Enable = IsDlgButtonChecked(IDC_SOUND_ENABLE) == BST_CHECKED;
	
	::EnableWindow(GetDlgItem(IDC_CZDC_SOUND), l_Enable);// TODO: make these interface elements in gray when disabled
	::EnableWindow(GetDlgItem(IDC_SOUNDLIST), l_Enable);
	::EnableWindow(GetDlgItem(IDC_SOUNDS_COMBO), l_Enable); // [+] SCALOlaz: Sound Themes
	::EnableWindow(GetDlgItem(IDC_PLAY), l_Enable);
	::EnableWindow(GetDlgItem(IDC_DEFAULT), l_Enable);
	::EnableWindow(GetDlgItem(IDC_NONE), l_Enable);
	::EnableWindow(GetDlgItem(IDC_BROWSE), l_Enable);
	::EnableWindow(GetDlgItem(IDC_PRIVATE_MESSAGE_BEEP), l_Enable);
	::EnableWindow(GetDlgItem(IDC_PRIVATE_MESSAGE_BEEP_OPEN), l_Enable);
}

LRESULT Sounds::onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void Sounds::GetSNDThemeList()  // [+] SCALOlaz: get a list of dir's from SoundPatch.
{
	if (m_SNDThemeList.empty())
	{
		m_SNDThemeList.insert(SNDThemePair(TSTRING(SOUND_THEME_DEFAULT_NAME), Util::emptyString));
		const string fileFindPath = Util::getSoundPath() + '*';   // find only DIR
		for (FileFindIter i(fileFindPath); i != FileFindIter::end; ++i)
		{
			if (i->isDirectory())
			{
				const string name = i->getFileName();
				if (name != Util::m_dot && name != Util::m_dot_dot)        // not use parents...
				{
					const wstring wName = /*L"Theme '" + */Text::toT(name)/* + L"'"*/;
					m_SNDThemeList.insert(SNDThemePair(wName, name));
				}
			}
		}
	}
}


/**
 * @file
 * $Id: Sounds.cpp,v 1.11 2005/02/22 16:49:49 bigmuscle Exp $
 */

