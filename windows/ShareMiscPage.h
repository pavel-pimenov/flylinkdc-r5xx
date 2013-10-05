/*
 * FlylinkDC++ // Share Misc Settings Page
 */

#if !defined(SHARE_MISC_PAGE_H)
#define SHARE_MISC_PAGE_H

#pragma once

#include <atlcrack.h>
#include "PropPage.h"
#include "wtl_flylinkdc.h"

class ShareMiscPage : public CPropertyPage<IDD_SHARE_MISC_PAGE>, public PropPage
#ifdef _DEBUG
	, virtual NonDerivable<ShareMiscPage> // [+] IRainman fix.
#endif
{
	public:
		ShareMiscPage(SettingsManager *s) : PropPage(s)
		{
			title = TSTRING(SETTINGS_UPLOADS) + _T('\\') + TSTRING(SETTINGS_ADVANCED);
			SetTitle(title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		};
		~ShareMiscPage()
		{ };
		
		BEGIN_MSG_MAP(ShareMiscPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_TTH_IN_STREAM, onFixControls)
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
		wstring title;
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
};

#endif //SHARE_MISC_PAGE_H
