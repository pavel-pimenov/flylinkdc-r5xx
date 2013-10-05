/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#include "stdafx.h"
#include "Resource.h"
#include "ShareMiscPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"

PropPage::TextItem ShareMiscPage::texts[] =
{
	{ IDC_TTH_IN_STREAM, ResourceManager::SETTINGS_SAVE_TTH_IN_NTFS_FILESTREAM },
	{ IDC_LENGHT_FILE_TTH_IN_STREAM, ResourceManager::SETTINGS_SET_MIN_LENGHT_TTH_IN_NTFS_FILESTREAM },
	{ IDC_SETTINGS_MB, ResourceManager::MB },
	{ IDC_SETTINGS_AUTO_REFRESH_TIME, ResourceManager::SETTINGS_AUTO_REFRESH_TIME },
	{ IDC_M, ResourceManager::M },
	{ IDC_SETTINGS_MAX_HASH_SPEED, ResourceManager::SETTINGS_MAX_HASH_SPEED },
	{ IDC_SETTINGS_MBS, ResourceManager::MBPS },
	
	{ IDC_MIN_LENGHT_FILE_TO_MEDIAINFO, ResourceManager::SETTINGS_MIN_MEDIAINFO_SIZE },
	{ IDC_SETTINGS_MB3, ResourceManager::MB },
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item ShareMiscPage::items[] =
{
	{ IDC_TTH_IN_STREAM, SettingsManager::SAVE_TTH_IN_NTFS_FILESTREAM, PropPage::T_BOOL },
	{ IDC_SET_MIN_LENGHT_TTH_STREAM, SettingsManager::SET_MIN_LENGHT_TTH_IN_NTFS_FILESTREAM, PropPage::T_INT},
	{ IDC_AUTO_REFRESH_TIME, SettingsManager::AUTO_REFRESH_TIME, PropPage::T_INT },
	{ IDC_MAX_HASH_SPEED, SettingsManager::MAX_HASH_SPEED, PropPage::T_INT },
	
	{ IDC_SET_MIN_LENGHT_FOR_MEDIAINFO, SettingsManager::MIN_MEDIAINFO_SIZE, PropPage::T_INT}, // [+] PPA
	
	{ 0, 0, PropPage::T_END }
};

ShareMiscPage::ListItem ShareMiscPage::listItems[] =
{

	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT ShareMiscPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	
	CUpDownCtrl updown;
	SET_MIN_MAX(IDC_SET_MIN_LENGHT_TTH_STREAM_SPIN, 1, 1000);
	SET_MIN_MAX(IDC_REFRESH_SPIN, 0, 3000);
	SET_MIN_MAX(IDC_HASH_SPIN, 0, 999);
	SET_MIN_MAX(IDC_SET_MIN_LENGHT_FILE_FOR_MEDIAINFO_SPIN, 0, 1000);
	
	fixControls();
	// Do specialized reading here
	return TRUE;
}

void ShareMiscPage::write()
{
	PropPage::write((HWND)*this, items);
	// Do specialized writing here
}

LRESULT ShareMiscPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // [+]NightOrion
{
	fixControls();
	return 0;
}
void ShareMiscPage::fixControls()
{
	BOOL state = (IsDlgButtonChecked(IDC_TTH_IN_STREAM) != 0);
	::EnableWindow(GetDlgItem(IDC_SET_MIN_LENGHT_TTH_STREAM), state);
	::EnableWindow(GetDlgItem(IDC_LENGHT_FILE_TTH_IN_STREAM), state);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_MB), state);
}
