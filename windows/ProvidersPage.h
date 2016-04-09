/*
 * Copyright (C) 2012-2016 FlylinkDC++ Team http://flylinkdc.com/
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
#if !defined(_PROVIDERS_PAGE_H_)
#define _PROVIDERS_PAGE_H_

#pragma once

#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU

#include <atlcrack.h>
#include "PropPage.h"
#include "WinUtil.h"
#include "wtl_flylinkdc.h"

class ProvidersPage : public CPropertyPage<IDD_PROVIDERS_PAGE>, public PropPage
{
	public:
	
		explicit ProvidersPage() : PropPage(TSTRING(SETTINGS_PROVIDERS))   /*, bInited(false)*/
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~ProvidersPage()
		{
		}
		
		BEGIN_MSG_MAP_EX(ProvidersPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_PROVIDER_USE_RESOURCES, onClickedUse)
		COMMAND_ID_HANDLER(IDC_ISP_MORE_INFO_LINK, onLink)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedUse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLink(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			WinUtil::openLink(_T("http://flylinkdc.com/forum/viewtopic.php?f=27&t=537"));
			return 0;
		}
		void write();
		void cancel()
		{
			cancel_check();
		}
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
	private:
		CFlyHyperLink m_url;
		
	protected:
		void fixControls();
		//IDC_ISP_MORE_INFO_LINK
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
		
};
#else
class ProvidersPage : public EmptyPage
{
	public:
		ProvidersPage() : EmptyPage(TSTRING(SETTINGS_PROVIDERS))
		{
		}
};
#endif // IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU

#endif // _PROVIDERS_PAGE_H_