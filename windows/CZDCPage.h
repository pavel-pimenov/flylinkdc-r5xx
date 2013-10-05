/*
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

#ifndef CZDCPage_H
#define CZDCPage_H

#include <atlcrack.h>
#include "PropPage.h"
#include "../client/ConnectionManager.h"

class CZDCPage : public CPropertyPage<IDD_CZDCPAGE>, public PropPage
{
	public:
		CZDCPage(SettingsManager *s) : PropPage(s)
		{
			SetTitle(_T(APPNAME));
			m_psp.dwFlags |= PSP_RTLREADING;
		};
		virtual ~CZDCPage() { }
		
		BEGIN_MSG_MAP_EX(CZDCPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_WINAMP_HELP, BN_CLICKED, onClickedWinampHelp)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onClickedWinampHelp(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		virtual void write();
		
	private:
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
		
};

#endif //CZDCPage_H