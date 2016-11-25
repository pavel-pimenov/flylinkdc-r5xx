/*
 * FlylinkDC++ // Search Page
 */

#include "stdafx.h"
#include "Resource.h"
#include "SearchPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"

PropPage::TextItem SearchPage::texts[] =
{
	{ IDC_S, ResourceManager::S },
	{ IDC_SETTINGS_SEARCH_HISTORY, ResourceManager::SETTINGS_SEARCH_HISTORY },
	{ IDC_SETTINGS_AUTO_SEARCH_LIMIT, ResourceManager::SETTINGS_AUTO_SEARCH_LIMIT },
	{ IDC_INTERVAL_TEXT, ResourceManager::MINIMUM_SEARCH_INTERVAL },
	{ IDC_MATCH_QUEUE_TEXT, ResourceManager::SETTINGS_SB_MAX_SOURCES },
	{ IDC_SEARCH_FORGET, ResourceManager::FORGET_SEARCH_REQUEST },  // [+] SCALOlaz: don't remember search string
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item SearchPage::items[] =
{
	{ IDC_SEARCH_HISTORY, SettingsManager::SEARCH_HISTORY, PropPage::T_INT },
	{ IDC_INTERVAL, SettingsManager::MINIMUM_SEARCH_INTERVAL, PropPage::T_INT },
	{ IDC_MATCH, SettingsManager::MAX_AUTO_MATCH_SOURCES, PropPage::T_INT },
	{ IDC_AUTO_SEARCH_LIMIT, SettingsManager::AUTO_SEARCH_LIMIT, PropPage::T_INT },
	{ IDC_SEARCH_FORGET, SettingsManager::FORGET_SEARCH_REQUEST, PropPage::T_BOOL },    // [+] SCALOlaz: don't remember search string
	
	{ 0, 0, PropPage::T_END }
};

SearchPage::ListItem SearchPage::listItems[] =
{
	{ SettingsManager::CLEAR_SEARCH, ResourceManager::SETTINGS_CLEAR_SEARCH },
	{ SettingsManager::USE_EXTENSION_DOWNTO, ResourceManager::SETTINGS_USE_EXTENSION_DOWNTO },
	{ SettingsManager::ADLS_BREAK_ON_FIRST, ResourceManager::SETTINGS_ADLS_BREAK_ON_FIRST },
	{ SettingsManager::SEARCH_PASSIVE, ResourceManager::SETCZDC_PASSIVE_SEARCH },
	{ SettingsManager::FILTER_ENTER, ResourceManager::SETTINGS_FILTER_ENTER },
	{ SettingsManager::ENABLE_FLY_SERVER, ResourceManager::ENABLE_FLY_SERVER },
	{ SettingsManager::BLEND_OFFLINE_SEARCH, ResourceManager::BLEND_OFFLINE_SEARCH },
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

#define setMinMax(x, y, z) \
	updown.Attach(GetDlgItem(x)); \
	updown.SetRange32(y, z); \
	updown.Detach();

LRESULT SearchPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
	
	CUpDownCtrl updown;
	setMinMax(IDC_SEARCH_HISTORY_SPIN, 1, 100);
	setMinMax(IDC_INTERVAL_SPIN, 2, 100);
	setMinMax(IDC_MATCH_SPIN, 1, 999);
	setMinMax(IDC_AUTO_SEARCH_LIMIT_SPIN, 1, 999);
	
	ctrlList.Attach(GetDlgItem(IDC_ADVANCED_BOOLEANS)); // [+] IRainman
	
	
	fixControls();
	// Do specialized reading here
	return TRUE;
}

void SearchPage::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_ADVANCED_BOOLEANS));
	// Do specialized writing here
}

LRESULT SearchPage::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) // [+]NightOrion
{
	fixControls();
	return 0;
}
void SearchPage::fixControls()
{
	BOOL state = (IsDlgButtonChecked(IDC_SEARCH_FORGET) != 1);
	::EnableWindow(GetDlgItem(IDC_SETTINGS_SEARCH_HISTORY), state);
	::EnableWindow(GetDlgItem(IDC_SEARCH_HISTORY), state);
	::EnableWindow(GetDlgItem(IDC_SEARCH_HISTORY_SPIN), state);
}
