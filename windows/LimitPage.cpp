/**
* Страница в настройках "Ограничения скорости" / Page "Limits".
*/

#include "stdafx.h"

#include "Resource.h"

#include "LimitPage.h"
#include "WinUtil.h"

PropPage::TextItem LimitPage::texts[] =
{
	{ IDC_THROTTLE_ENABLE, ResourceManager::SETCZDC_ENABLE_LIMITING },
	{ IDC_CZDC_TRANSFER_LIMITING, ResourceManager::SETCZDC_TRANSFER_LIMITING },
	{ IDC_CZDC_UP_SPEEED, ResourceManager::UPLOAD },
	{ IDC_CZDC_UP_SPEEED1, ResourceManager::UPLOAD },
	{ IDC_SETTINGS_KBPS1, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS2, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS3, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS4, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS5, ResourceManager::KBPS_DISABLE },
	{ IDC_SETTINGS_KBPS6, ResourceManager::KBPS },
	{ IDC_SETTINGS_KBPS7, ResourceManager::KBPS },
	{ IDC_SETTINGS_MINUTES, ResourceManager::SECONDS },
	{ IDC_CZDC_DW_SPEEED, ResourceManager::DOWNLOAD },
	{ IDC_CZDC_DW_SPEEED1, ResourceManager::DOWNLOAD },
	{ IDC_TIME_LIMITING, ResourceManager::SETCZDC_ALTERNATE_LIMITING },
	{ IDC_CZDC_TO, ResourceManager::SETCZDC_TO },
	{ IDC_CZDC_SECONDARY_TRANSFER, ResourceManager::SETCZDC_SECONDARY_LIMITING },
	{ IDC_CZDC_SLOW_DISCONNECT, ResourceManager::SETCZDC_SLOW_DISCONNECT },
	{ IDC_SEGMENTED_ONLY, ResourceManager::SETTINGS_AUTO_DROP_SEGMENTED_SOURCE },
	{ IDC_CZDC_I_DOWN_SPEED, ResourceManager::SETCZDC_I_DOWN_SPEED },
	{ IDC_CZDC_TIME_DOWN, ResourceManager::SETCZDC_TIME_DOWN },
	{ IDC_CZDC_H_DOWN_SPEED, ResourceManager::SETCZDC_H_DOWN_SPEED },
	{ IDC_DISCONNECTING_ENABLE, ResourceManager::SETCZDC_DISCONNECTING_ENABLE },
	{ IDC_CZDC_MIN_FILE_SIZE, ResourceManager::SETCZDC_MIN_FILE_SIZE },
	{ IDC_SETTINGS_MB, ResourceManager::MB },
	{ IDC_REMOVE_IF, ResourceManager::NEW_DISCONNECT },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item LimitPage::items[] =
{
	// [!] IRainman SpeedLimiter: to work correctly, you must first set the upload speed, and only then download speed!
	{ IDC_MX_UP_SP_LMT_NORMAL, SettingsManager::MAX_UPLOAD_SPEED_LIMIT_NORMAL, PropPage::T_INT },
	{ IDC_MX_DW_SP_LMT_NORMAL, SettingsManager::MAX_DOWNLOAD_SPEED_LIMIT_NORMAL, PropPage::T_INT },
	{ IDC_TIME_LIMITING, SettingsManager::TIME_DEPENDENT_THROTTLE, PropPage::T_BOOL },
	{ IDC_MX_UP_SP_LMT_TIME, SettingsManager::MAX_UPLOAD_SPEED_LIMIT_TIME, PropPage::T_INT },
	{ IDC_MX_DW_SP_LMT_TIME, SettingsManager::MAX_DOWNLOAD_SPEED_LIMIT_TIME, PropPage::T_INT },
	{ IDC_BW_START_TIME, SettingsManager::BANDWIDTH_LIMIT_START, PropPage::T_INT },
	{ IDC_BW_END_TIME, SettingsManager::BANDWIDTH_LIMIT_END, PropPage::T_INT },
	{ IDC_THROTTLE_ENABLE, SettingsManager::THROTTLE_ENABLE, PropPage::T_BOOL },
	{ IDC_I_DOWN_SPEED, SettingsManager::DISCONNECT_SPEED, PropPage::T_INT },
	{ IDC_TIME_DOWN, SettingsManager::DISCONNECT_TIME, PropPage::T_INT },
	{ IDC_H_DOWN_SPEED, SettingsManager::DISCONNECT_FILE_SPEED, PropPage::T_INT },
	{ IDC_DISCONNECTING_ENABLE, SettingsManager::DISCONNECTING_ENABLE, PropPage::T_BOOL },
	{ IDC_SEGMENTED_ONLY, SettingsManager::DROP_MULTISOURCE_ONLY, PropPage::T_BOOL },
	{ IDC_MIN_FILE_SIZE, SettingsManager::DISCONNECT_FILESIZE, PropPage::T_INT },
	{ IDC_REMOVE_IF_BELOW, SettingsManager::REMOVE_SPEED, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

LRESULT LimitPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	
	CUpDownCtrl spin;
	spin.Attach(GetDlgItem(IDC_I_DOWN_SPEED_SPIN));
	spin.SetRange32(0, 99999);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_TIME_DOWN_SPIN));
	spin.SetRange32(1, 99999);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_H_DOWN_SPEED_SPIN));
	spin.SetRange32(0, 99999);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_UPLOADSPEEDSPIN));
	spin.SetRange32(0, 99999);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_DOWNLOADSPEEDSPIN));
	spin.SetRange32(0, 99999);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_UPLOADSPEEDSPIN_TIME));
	spin.SetRange32(0, 99999);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_DOWNLOADSPEEDSPIN_TIME));
	spin.SetRange32(0, 99999);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_MIN_FILE_SIZE_SPIN));
	spin.SetRange32(0, 99999);
	spin.Detach();
	spin.Attach(GetDlgItem(IDC_REMOVE_SPIN));
	spin.SetRange32(0, 99999);
	spin.Detach();
	
	// [+] InfinitySky. Выбор времени из выпадающего списка.
	timeCtrlBegin.Attach(GetDlgItem(IDC_BW_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_BW_END_TIME));
	
	WinUtil::GetTimeValues(timeCtrlBegin);
	WinUtil::GetTimeValues(timeCtrlEnd);
	
	timeCtrlBegin.SetCurSel(SETTING(BANDWIDTH_LIMIT_START));
	timeCtrlEnd.SetCurSel(SETTING(BANDWIDTH_LIMIT_END));
	
	timeCtrlBegin.Detach();
	timeCtrlEnd.Detach();
	// [+] InfinitySky. END.
	
	fixControls();
	
	// Do specialized reading here
	
	return TRUE;
}

void LimitPage::write()
{
	PropPage::write((HWND)*this, items);
	
	// Do specialized writing here
	// settings->set(XX, YY);
	
	timeCtrlBegin.Attach(GetDlgItem(IDC_BW_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_BW_END_TIME));
	settings->set(SettingsManager::BANDWIDTH_LIMIT_START, timeCtrlBegin.GetCurSel());
	settings->set(SettingsManager::BANDWIDTH_LIMIT_END, timeCtrlEnd.GetCurSel());
	timeCtrlBegin.Detach();
	timeCtrlEnd.Detach();
}

void LimitPage::fixControls()
{
#ifndef PPA_INCLUDE_DROP_SLOW
	::EnableWindow(GetDlgItem(IDC_DISCONNECTING_ENABLE), false);
	::EnableWindow(GetDlgItem(IDC_SEGMENTED_ONLY), false);
#endif
	bool state = (IsDlgButtonChecked(IDC_THROTTLE_ENABLE) != 0);
	//::EnableWindow(GetDlgItem(IDC_MX_UP_SP_LMT_NORMAL), state);
	//::EnableWindow(GetDlgItem(IDC_UPLOADSPEEDSPIN), state);
	//::EnableWindow(GetDlgItem(IDC_MX_DW_SP_LMT_NORMAL), state);
	//::EnableWindow(GetDlgItem(IDC_DOWNLOADSPEEDSPIN), state);
	
	// Allow editing either with disabled limits checkboxes.
	::EnableWindow(GetDlgItem(IDC_MX_UP_SP_LMT_NORMAL), true);
	::EnableWindow(GetDlgItem(IDC_UPLOADSPEEDSPIN), true);
	::EnableWindow(GetDlgItem(IDC_MX_DW_SP_LMT_NORMAL), true);
	::EnableWindow(GetDlgItem(IDC_DOWNLOADSPEEDSPIN), true);
	::EnableWindow(GetDlgItem(IDC_TIME_LIMITING), state);
	
	state = ((IsDlgButtonChecked(IDC_THROTTLE_ENABLE) != 0) && (IsDlgButtonChecked(IDC_TIME_LIMITING) != 0));
	::EnableWindow(GetDlgItem(IDC_BW_START_TIME), state);
	::EnableWindow(GetDlgItem(IDC_BW_END_TIME), state);
	//::EnableWindow(GetDlgItem(IDC_MX_UP_SP_LMT_TIME), state);
	//::EnableWindow(GetDlgItem(IDC_UPLOADSPEEDSPIN_TIME), state);
	//::EnableWindow(GetDlgItem(IDC_MX_DW_SP_LMT_TIME), state);
	//::EnableWindow(GetDlgItem(IDC_DOWNLOADSPEEDSPIN_TIME), state);
	::EnableWindow(GetDlgItem(IDC_MX_UP_SP_LMT_TIME), true);
	::EnableWindow(GetDlgItem(IDC_UPLOADSPEEDSPIN_TIME), true);
	::EnableWindow(GetDlgItem(IDC_MX_DW_SP_LMT_TIME), true);
	::EnableWindow(GetDlgItem(IDC_DOWNLOADSPEEDSPIN_TIME), true);
	
	state = (IsDlgButtonChecked(IDC_DISCONNECTING_ENABLE) != 0);
	::EnableWindow(GetDlgItem(IDC_I_DOWN_SPEED), state);
	::EnableWindow(GetDlgItem(IDC_I_DOWN_SPEED_SPIN), state);
	::EnableWindow(GetDlgItem(IDC_TIME_DOWN), state);
	::EnableWindow(GetDlgItem(IDC_TIME_DOWN_SPIN), state);
	::EnableWindow(GetDlgItem(IDC_H_DOWN_SPEED), state);
	::EnableWindow(GetDlgItem(IDC_H_DOWN_SPEED_SPIN), state);
	::EnableWindow(GetDlgItem(IDC_MIN_FILE_SIZE), state);
	::EnableWindow(GetDlgItem(IDC_MIN_FILE_SIZE_SPIN), state);
	::EnableWindow(GetDlgItem(IDC_REMOVE_IF_BELOW), state);
	::EnableWindow(GetDlgItem(IDC_REMOVE_SPIN), state);
}

LRESULT LimitPage::onChangeCont(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_TIME_LIMITING:
		case IDC_THROTTLE_ENABLE:
		case IDC_DISCONNECTING_ENABLE:
			fixControls();
			break;
	}
	return true;
}
