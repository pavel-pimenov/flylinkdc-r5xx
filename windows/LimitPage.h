/**
* Страница в настройках "Ограничения скорости" / Page "Limits".
*/

#ifndef LimitPAGE_H
#define LimitPAGE_H

#include <atlcrack.h>
#include "PropPage.h"
#include "../client/ConnectionManager.h"

class LimitPage : public CPropertyPage<IDD_LIMIT_PAGE>, public PropPage
{
	public:
		LimitPage(SettingsManager *s) : PropPage(s, TSTRING(SETTINGS_ADVANCED) + _T('\\') + TSTRING(SETTINGS_LIMIT))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		};
		~LimitPage()
		{
		};
		
		BEGIN_MSG_MAP_EX(LimitPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_TIME_LIMITING, onChangeCont)
		COMMAND_ID_HANDLER(IDC_THROTTLE_ENABLE, onChangeCont)
		COMMAND_ID_HANDLER(IDC_DISCONNECTING_ENABLE, onChangeCont)
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onChangeCont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel() {}
	private:
		static Item items[];
		static TextItem texts[];
		CComboBox timeCtrlBegin, timeCtrlEnd; // [+] InfinitySky. Выбор времени из выпадающего списка.
		
		void fixControls();
};

#endif //LimitPAGE_H
