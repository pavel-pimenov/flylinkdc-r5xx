/*
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

#include "PropertiesDlg.h"

#include "GeneralPage.h"
#include "DownloadPage.h"
#include "PriorityPage.h"
#include "SharePage.h"
#include "SlotPage.h"
#include "AppearancePage.h"
#include "AdvancedPage.h"
#include "LogPage.h"
#include "SoundsPage.h"
#include "UCPage.h"
#include "LimitPage.h"
#include "FakeDetectPage.h"
#include "AVIPreviewPage.h"
#include "OperaColorsPage.h"
#include "ToolbarPage.h"
#include "ClientsPage.h"
#include "FavoriteDirsPage.h"
#include "PopupsPage.h"
#include "SDCPage.h"
#include "DefaultClickPage.h"
#include "UserListColours.h"
#include "NetworkPage.h"
#include "ProxyPage.h"
#include "WindowsPage.h"
#include "TabsPage.h"
#include "QueuePage.h"
#include "MiscPage.h"
#include "MessagesPage.h"
#include "RangesPage.h"
#include "RemoteControlPage.h"
#include "WebServerPage.h"
#include "RSSPage.h"
#include "UpdatePage.h"
#include "DCLSTPage.h"
#include "FileSharePage.h"
#include "IntegrationPage.h"
#include "IntPreviewPage.h"
#include "ProvidersPage.h"
#include "CertificatesPage.h"
#include "MessagesChatPage.h"
#include "ShareMiscPage.h"
#include "SearchPage.h"

bool PropertiesDlg::g_needUpdate = false;
bool PropertiesDlg::g_is_create = false;
PropertiesDlg::PropertiesDlg(HWND parent) : TreePropertySheet(CTSTRING(SETTINGS), 0, parent), m_network_page(nullptr)
{
	::g_settings = SettingsManager::getInstance();
	g_is_create = true;
	size_t n = 0;
	pages[n++] = new GeneralPage();
	pages[n++] = new ProvidersPage();
	m_network_page = new NetworkPage();
	pages[n++] = m_network_page;
	pages[n++] = new ProxyPage();
	pages[n++] = new DownloadPage();
	pages[n++] = new FavoriteDirsPage();
	pages[n++] = new AVIPreview();
	pages[n++] = new QueuePage();
	pages[n++] = new PriorityPage();
	pages[n++] = new SharePage();
	pages[n++] = new SlotPage();
	pages[n++] = new MessagesPage();
	pages[n++] = new AppearancePage();
	pages[n++] = new PropPageTextStyles();
	pages[n++] = new OperaColorsPage();
	pages[n++] = new UserListColours();
	pages[n++] = new Popups();
	pages[n++] = new Sounds();
	pages[n++] = new ToolbarPage();
	pages[n++] = new WindowsPage();
	pages[n++] = new TabsPage();
	pages[n++] = new AdvancedPage();
	pages[n++] = new SDCPage();
	pages[n++] = new DefaultClickPage();
	pages[n++] = new LogPage();
	pages[n++] = new UCPage();
	pages[n++] = new LimitPage();
	pages[n++] = new FakeDetect();
	pages[n++] = new ClientsPage();
	pages[n++] = new RSSPage();
	pages[n++] = new CertificatesPage();
	pages[n++] = new MiscPage();
	pages[n++] = new RangesPage();
	pages[n++] = new RemoteControlPage();
	pages[n++] = new WebServerPage();
	pages[n++] = new UpdatePage();
	pages[n++] = new DCLSTPage();
	pages[n++] = new IntegrationPage();
	pages[n++] = new IntPreviewPage();
	pages[n++] = new MessagesChatPage();
	pages[n++] = new ShareMiscPage();
	pages[n++] = new SearchPage();
#ifdef SSA_INCLUDE_FILE_SHARE_PAGE
	pages[n++] = new FileSharePage();
#endif
	// after add new page need add a new string in TreePropertySheet.cpp, l_HelpLinkTable.
	dcassert(n == g_numPages);
	
	for (size_t i = 0; i < g_numPages; i++)
	{
		AddPage(pages[i]->getPSP());
	}
	
	// Hide "Apply" button
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	m_psh.dwFlags &= ~PSH_HASHELP;
}

PropertiesDlg::~PropertiesDlg()
{
	for (size_t i = 0; i < g_numPages; i++)
	{
		if (m_network_page == pages[i])
			m_network_page = nullptr;
		safe_delete(pages[i]);
	}
	g_is_create = false;
}
void PropertiesDlg::onTimerSec()
{
	if (m_network_page)
	{
		const auto l_page = GetActivePage();
		if (l_page == *m_network_page)
		{
			m_network_page->updateTestPortIcon(false);
		}
	}
}
void PropertiesDlg::write()
{
	for (size_t i = 0; i < g_numPages; i++)
	{
		// Check HWND of page to see if it has been created
		const HWND page = PropSheet_IndexToHwnd((HWND) * this, i);
		
		if (page != nullptr)
			pages[i]->write();
	}
}

LRESULT PropertiesDlg::onOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	CRect rcWindow;
	if (GetWindowRect(rcWindow))
	{
		write();
	}
	bHandled = FALSE;
	return TRUE;
}

LRESULT PropertiesDlg::onCANCEL(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	cancel();
	bHandled = FALSE;
	return TRUE;        //Close Settings window if push 'Cancel'
}

void PropertiesDlg::cancel()
{
	for (size_t i = 0; i < g_numPages; i++)
	{
		// Check HWND of page to see if it has been created
		const HWND page = PropSheet_IndexToHwnd((HWND) * this, i);
		
		if (page != nullptr)
			pages[i]->cancel();
	}
}
/**
 * @file
 * $Id: PropertiesDlg.cpp,v 1.20 2006/07/08 23:19:54 bigmuscle Exp $
 */
