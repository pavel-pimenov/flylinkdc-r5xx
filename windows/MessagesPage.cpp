
/*
 * ApexDC speedmod (c) SMT 2007
 */


#include "stdafx.h"

#include "Resource.h"
#include "MessagesPage.h"
#include "WinUtil.h"

PropPage::TextItem MessagesPage::texts[] =
{
	{ IDC_SETTINGS_DEFAULT_AWAY_MSG, ResourceManager::SETTINGS_DEFAULT_AWAY_MSG },
	
	{ IDC_TIME_AWAY, ResourceManager::SET_SECONDARY_AWAY },
	{ IDC_AWAY_TO, ResourceManager::SETCZDC_TO },
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item MessagesPage::items[] =
{
	{ IDC_DEFAULT_AWAY_MESSAGE, SettingsManager::DEFAULT_AWAY_MESSAGE, PropPage::T_STR },
	
	{ IDC_SECONDARY_AWAY_MESSAGE, SettingsManager::SECONDARY_AWAY_MESSAGE, PropPage::T_STR },
	{ IDC_TIME_AWAY, SettingsManager::AWAY_TIME_THROTTLE, PropPage::T_BOOL },
	{ IDC_AWAY_START_TIME, SettingsManager::AWAY_START, PropPage::T_INT },
	{ IDC_AWAY_END_TIME, SettingsManager::AWAY_END, PropPage::T_INT },
	
	{ 0, 0, PropPage::T_END }
};

MessagesPage::ListItem MessagesPage::listItems[] =
{
	{ SettingsManager::SEND_SLOTGRANT_MSG, ResourceManager::SEND_SLOTGRANT_MSG },
	// [-] { SettingsManager::NO_AWAYMSG_TO_BOTS, ResourceManager::SETTINGS_NO_AWAYMSG_TO_BOTS }, [-] IRainman fix.
	{ SettingsManager::USE_CTRL_FOR_LINE_HISTORY, ResourceManager::SETTINGS_USE_CTRL_FOR_LINE_HISTORY },
	{ SettingsManager::TIME_STAMPS, ResourceManager::SETTINGS_TIME_STAMPS },
	
	{ SettingsManager::IP_IN_CHAT, ResourceManager::IP_IN_CHAT },
	{ SettingsManager::COUNTRY_IN_CHAT, ResourceManager::COUNTRY_IN_CHAT },
	
	{ SettingsManager::DISPLAY_CHEATS_IN_MAIN_CHAT, ResourceManager::SETTINGS_DISPLAY_CHEATS_IN_MAIN_CHAT },
	{ SettingsManager::SHOW_SHARE_CHECKED_USERS, ResourceManager::SETTINGS_ADVANCED_SHOW_SHARE_CHECKED_USERS },
#ifdef IRAINMAN_INCLUDE_USER_CHECK
	{ SettingsManager::CHECK_NEW_USERS, ResourceManager::CHECK_ON_CONNECT },
#endif
	{ SettingsManager::STATUS_IN_CHAT, ResourceManager::SETTINGS_STATUS_IN_CHAT },
	{ SettingsManager::SHOW_JOINS, ResourceManager::SETTINGS_SHOW_JOINS },
	{ SettingsManager::FAV_SHOW_JOINS, ResourceManager::SETTINGS_FAV_SHOW_JOINS },
	{ SettingsManager::SUPPRESS_MAIN_CHAT, ResourceManager::SETTINGS_ADVANCED_SUPPRESS_MAIN_CHAT },
	{ SettingsManager::SUPPRESS_PMS, ResourceManager::SETTINGS_ADVANCED_SUPPRESS_PMS },//[+]IRainman
	{ SettingsManager::IGNORE_HUB_PMS, ResourceManager::SETTINGS_IGNORE_HUB_PMS },
	{ SettingsManager::IGNORE_BOT_PMS, ResourceManager::SETTINGS_IGNORE_BOT_PMS },
	{ SettingsManager::NSL_IGNORE_ME, ResourceManager::NSL_IGNORE_ME },
	{ SettingsManager::AUTO_AWAY, ResourceManager::SETTINGS_AUTO_AWAY },    //AdvancedPage
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT MessagesPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::read((HWND)*this, items, listItems, GetDlgItem(IDC_MESSAGES_BOOLEANS));
	
	ctrlList.Attach(GetDlgItem(IDC_MESSAGES_BOOLEANS)); // [+] IRainman
	
	PropPage::translate((HWND)(*this), texts);
	
	// [+] InfinitySky. ¬ыбор времени из выпадающего списка.
	timeCtrlBegin.Attach(GetDlgItem(IDC_AWAY_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_AWAY_END_TIME));
	
	WinUtil::GetTimeValues(timeCtrlBegin);
	WinUtil::GetTimeValues(timeCtrlEnd);
	
	timeCtrlBegin.SetCurSel(SETTING(AWAY_START));
	timeCtrlEnd.SetCurSel(SETTING(AWAY_END));
	
	timeCtrlBegin.Detach();
	timeCtrlEnd.Detach();
	// [+] InfinitySky. END.
	
	fixControls();
	
	// Do specialized reading here
	return TRUE;
}

LRESULT MessagesPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void MessagesPage::write()
{
	PropPage::write((HWND)*this, items, listItems, GetDlgItem(IDC_MESSAGES_BOOLEANS));
	
	// [+] InfinitySky. ¬ыбор времени из выпадающего списка.
	timeCtrlBegin.Attach(GetDlgItem(IDC_AWAY_START_TIME));
	timeCtrlEnd.Attach(GetDlgItem(IDC_AWAY_END_TIME));
	
	settings->set(SettingsManager::AWAY_START, timeCtrlBegin.GetCurSel());
	settings->set(SettingsManager::AWAY_END, timeCtrlEnd.GetCurSel());
	
	timeCtrlBegin.Detach();
	timeCtrlEnd.Detach();
	// [+] InfinitySky. END.
	
	// Do specialized writing here
}

void MessagesPage::fixControls()
{
	bool state = (IsDlgButtonChecked(IDC_TIME_AWAY) != 0);
	::EnableWindow(GetDlgItem(IDC_AWAY_START_TIME), state);
	::EnableWindow(GetDlgItem(IDC_AWAY_END_TIME), state);
	::EnableWindow(GetDlgItem(IDC_SECONDARY_AWAY_MESSAGE), state);
	::EnableWindow(GetDlgItem(IDC_SECONDARY_AWAY_MSG), state);
	::EnableWindow(GetDlgItem(IDC_AWAY_TO), state);
}

