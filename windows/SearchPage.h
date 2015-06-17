/*
 * FlylinkDC++ // Search Page
 */

#if !defined(SEARCH_PAGE_H)
#define SEARCH_PAGE_H

#pragma once

#include <atlcrack.h>
#include "ExListViewCtrl.h" // [+] IRainman
#include "PropPage.h"
#include "wtl_flylinkdc.h"

class SearchPage : public CPropertyPage<IDD_SEARCH_PAGE>, public PropPage
{
	public:
		explicit SearchPage(SettingsManager *s) : PropPage(s, TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_ADVANCED3) + _T('\\') + TSTRING(SEARCH))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~SearchPage()
		{
			ctrlList.Detach(); // [+] IRainman
		}
		
		BEGIN_MSG_MAP(SearchPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_SEARCH_FORGET, onFixControls)
		NOTIFY_HANDLER(IDC_ADVANCED_BOOLEANS, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+]NightOrion
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel() {}
	private:
		void fixControls(); // [+]NightOrion
	protected:
	
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
		
		ExListViewCtrl ctrlList; // [+] IRainman
};

#endif //SEARCH_PAGE_H
