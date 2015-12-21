/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#include "stdafx.h"
#include "Resource.h"
#include "ShareMiscPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"
#include "../client/GPGPUManager.h"

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
	
	{ IDC_TTH_USE_GPU, SettingsManager::USE_GPU_IN_TTH_COMPUTING, PropPage::T_BOOL },
	{ IDC_TTH_GPU_DEVICES, SettingsManager::GPU_DEV_NAME_FOR_TTH_COMP, PropPage::T_STR },
	
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
	
#ifdef FLYLINKDC_USE_GPU_TTH
	const auto l_count_gpu = GPGPUTTHManager::getInstance()->get()->get_dev_cnt();
	const auto l_set_dnum = SETTING(TTH_GPU_DEV_NUM);
	const auto s_set_dname = SETTING(GPU_DEV_NAME_FOR_TTH_COMP);
	const CString cs_set_dname(s_set_dname.c_str());
	
	ctrlTTHGPUDevices.Attach(GetDlgItem(IDC_TTH_GPU_DEVICES));
	
	for (int i = 0; i < l_count_gpu; ++i)
	{
		const string& s_dname = GPGPUTTHManager::getInstance()->get()->get_dev_name(i);
		const CString cs_dname(s_dname.c_str());
		
		ctrlTTHGPUDevices.AddString((LPCTSTR)cs_dname);
	}
	
	if (l_set_dnum >= 0 && l_set_dnum < l_count_gpu)
	{
		const auto s_dname = GPGPUTTHManager::getInstance()->get()->get_dev_name(l_set_dnum);
		
		if (s_dname == s_set_dname)
		{
			ctrlTTHGPUDevices.SetCurSel(l_set_dnum);
		}
		else
		{
			ctrlTTHGPUDevices.SetWindowTextW((LPCTSTR)cs_set_dname);
		}
	}
	else
	{
		if (l_set_dnum == -1)
		{
			ctrlTTHGPUDevices.SetWindowTextW(TEXT(""));
		}
		else
		{
			ctrlTTHGPUDevices.SetWindowTextW((LPCTSTR)cs_set_dname);
		}
	}
	
	ctrlTTHGPUDevices.Detach();
#endif
	fixGPUTTHControls();
	fixControls();
	// Do specialized reading here
	return TRUE;
}

void ShareMiscPage::write()
{
	PropPage::write((HWND)*this, items);
#ifdef FLYLINKDC_USE_GPU_TTH
	bool use_gpu = BOOLSETTING(USE_GPU_IN_TTH_COMPUTING);
	
	ctrlTTHGPUDevices.Attach(GetDlgItem(IDC_TTH_GPU_DEVICES));
	
	int sel_dev_num = ctrlTTHGPUDevices.GetCurSel();
	
	if (sel_dev_num != CB_ERR && sel_dev_num != SETTING(TTH_GPU_DEV_NUM))
	{
		if (use_gpu)
		{
			GPGPUTTHManager::getInstance()->get()->select_device(sel_dev_num);
		}
		
		SettingsManager::set(SettingsManager::TTH_GPU_DEV_NUM, sel_dev_num);
		SettingsManager::getInstance()->save();
	}
	else if (sel_dev_num != CB_ERR && sel_dev_num == SETTING(TTH_GPU_DEV_NUM)
	         && use_gpu)
	{
		int cur_dev_num = GPGPUTTHManager::getInstance()->get()->get_cur_dev();
		
		if (sel_dev_num != cur_dev_num)
		{
			GPGPUTTHManager::getInstance()->get()->select_device(sel_dev_num);
		}
	}
	
	ctrlTTHGPUDevices.Detach();
#endif  //FLYLINKDC_USE_GPU_TTH
}

LRESULT ShareMiscPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // [+]NightOrion
{
	fixControls();
	return 0;
}

LRESULT ShareMiscPage::onTTHUseGPUToggle(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // [+]NightOrion
{
	fixGPUTTHControls();
	return 0;
}

void ShareMiscPage::fixControls()
{
	const BOOL state = (IsDlgButtonChecked(IDC_TTH_IN_STREAM) != 0);
	::EnableWindow(GetDlgItem(IDC_SET_MIN_LENGHT_TTH_STREAM), state);
	::EnableWindow(GetDlgItem(IDC_LENGHT_FILE_TTH_IN_STREAM), state);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_MB), state);
}

void ShareMiscPage::fixGPUTTHControls()
{
	BOOL state = (IsDlgButtonChecked(IDC_TTH_USE_GPU) != 0);
#ifndef FLYLINKDC_USE_GPU_TTH
	state = false;
	::EnableWindow(GetDlgItem(IDC_TTH_USE_GPU), state);
#endif
	::EnableWindow(GetDlgItem(IDC_TTH_GPU_DEVICES), state);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_TTH_GPU_DEVICE), state);
}
