/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#include "PropertiesDlg.h"

#include "GeneralPage.h"
#include "DownloadPage.h"
#include "PriorityPage.h" // [+] InfinitySky.
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
#include "DefaultClickPage.h" // [+] NightOrion.
#include "UserListColours.h"
#include "NetworkPage.h"
#include "ProxyPage.h"
#include "WindowsPage.h"
#include "TabsPage.h" // [+] InfinitySky.
#include "QueuePage.h"
#include "MiscPage.h"
#include "MessagesPage.h" // !SMT!-PSW
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

bool PropertiesDlg::needUpdate = false;
bool PropertiesDlg::m_Create = false;
PropertiesDlg::PropertiesDlg(HWND parent, SettingsManager *s) : TreePropertySheet(CTSTRING(SETTINGS), 0, parent)
{
	m_Create = true;
	size_t n = 0;
	pages[n++] = new GeneralPage(s);
	pages[n++] = new ProvidersPage(s);
	pages[n++] = new NetworkPage(s);
	pages[n++] = new ProxyPage(s);
	pages[n++] = new DownloadPage(s);
	pages[n++] = new FavoriteDirsPage(s);
	pages[n++] = new AVIPreview(s);
	pages[n++] = new QueuePage(s);
	pages[n++] = new PriorityPage(s); // [+] InfinitySky.
	pages[n++] = new SharePage(s);
	pages[n++] = new SlotPage(s);
	pages[n++] = new MessagesPage(s); // !SMT!-PSW [~] InfinitySky
	pages[n++] = new AppearancePage(s);
	pages[n++] = new PropPageTextStyles(s);
	pages[n++] = new OperaColorsPage(s);
	pages[n++] = new UserListColours(s);
	pages[n++] = new Popups(s);
	pages[n++] = new Sounds(s);
	pages[n++] = new ToolbarPage(s);
	pages[n++] = new WindowsPage(s);
	pages[n++] = new TabsPage(s); // [+] InfinitySky.
	pages[n++] = new AdvancedPage(s);
	pages[n++] = new SDCPage(s);
	pages[n++] = new DefaultClickPage(s); // [+] NightOrion.
	pages[n++] = new LogPage(s);
	pages[n++] = new UCPage(s);
	pages[n++] = new LimitPage(s);
	pages[n++] = new FakeDetect(s);
	pages[n++] = new ClientsPage(s);
	pages[n++] = new RSSPage(s); // [+] SSA
	pages[n++] = new CertificatesPage(s);
	pages[n++] = new MiscPage(s);
	pages[n++] = new RangesPage(s);
	pages[n++] = new RemoteControlPage(s);
	pages[n++] = new WebServerPage(s);
	pages[n++] = new UpdatePage(s);
	pages[n++] = new DCLSTPage(s);
	pages[n++] = new IntegrationPage(s);
	pages[n++] = new IntPreviewPage(s);
	pages[n++] = new MessagesChatPage(s);
	pages[n++] = new ShareMiscPage(s);
	pages[n++] = new SearchPage(s);
#ifdef SSA_INCLUDE_FILE_SHARE_PAGE
	pages[n++] = new FileSharePage(s);
#endif
	// after add new page need add a new string in TreePropertySheet.cpp, l_HelpLinkTable.
	dcassert(n == m_numPages);
	
	for (size_t i = 0; i < m_numPages; i++)
	{
		AddPage(pages[i]->getPSP());
	}
	
	// Hide "Apply" button
	m_psh.dwFlags |= PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	m_psh.dwFlags &= ~PSH_HASHELP;
}

PropertiesDlg::~PropertiesDlg()
{
	for (size_t i = 0; i < m_numPages; i++)
	{
		delete pages[i];
	}
	m_Create = false;
}

void PropertiesDlg::write()
{
	for (size_t i = 0; i < m_numPages; i++)
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
	GetWindowRect(rcWindow);
	SET_SETTING(SETTINGS_WINDOW_POS_X, rcWindow.left);
	SET_SETTING(SETTINGS_WINDOW_POS_Y, rcWindow.top);
	SET_SETTING(SETTINGS_WINDOW_SIZE_X, rcWindow.right /*- rcWindow.left*/);
	SET_SETTING(SETTINGS_WINDOW_SIZE_YY, rcWindow.bottom /*- rcWindow.top*/);
	write();
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
	for (size_t i = 0; i < m_numPages; i++)
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
