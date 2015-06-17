
/*
 * ApexDC speedmod (c) SMT 2007
 */


#ifndef MESSAGES_PAGE_H
#define MESSAGES_PAGE_H

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h" // [+] IRainman

class MessagesPage : public CPropertyPage<IDD_MESSAGES_PAGE>, public PropPage
{
	public:
		explicit MessagesPage(SettingsManager *s) : PropPage(s, TSTRING(SETTINGS_MESSAGES))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~MessagesPage()
		{
			ctrlList.Detach(); // [+] IRainman
		}
		
		BEGIN_MSG_MAP(MessagesPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_TIME_AWAY, onFixControls)
		NOTIFY_HANDLER(IDC_MESSAGES_BOOLEANS, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel() {}
	protected:
		static Item items[];
		static TextItem texts[];
		static ListItem listItems[];
		void fixControls();
		
		CComboBox timeCtrlBegin, timeCtrlEnd; // [+] InfinitySky. ¬ыбор времени из выпадающего списка.
		
		ExListViewCtrl ctrlList; // [+] IRainman
};

#endif //MESSAGES_PAGE_H
