#include "stdafx.h"

#include "Resource.h"

#include "RemoteControlPage.h"
#include "../client/SettingsManager.h"
#include "WinUtil.h"

PropPage::TextItem RemoteControlPage::texts[] =
{
	{ IDC_ENABLE_WEBSERVER, ResourceManager::WEBSERVER },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item RemoteControlPage::items[] =
{
	{ IDC_ENABLE_WEBSERVER, SettingsManager::WEBSERVER, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

LRESULT RemoteControlPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	
	return TRUE;
}

void RemoteControlPage::write()
{
	PropPage::write((HWND)*this, items);
}