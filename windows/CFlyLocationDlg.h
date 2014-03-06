//-----------------------------------------------------------------------------
//(c) 2014 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#if !defined(CFLYLOCATION_DLG_H)
#define CFLYLOCATION_DLG_H

#pragma once

#include "../client/Util.h"
#include "WinUtil.h"

class CFlyLocationDlg : public CDialogImpl<CFlyLocationDlg>
{
	public:
	
		enum { IDD = IDD_LOCATION_DIALOG };
		
		BEGIN_MSG_MAP(CFlyLocationDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		END_MSG_MAP()
		
		CFlyLocationDlg() {}
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			//ctrlLine.SetFocus();
			return FALSE;
		}
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		CComboBox m_ctrlCountry;
		CComboBox m_ctrlCity;
		CComboBox m_ctrlProvider;
		
		tstring m_Country;
		tstring m_City;
		tstring m_Provider;
		bool isEmpty() const
		{
			return m_Country.empty() || m_City.empty() || m_Provider.empty();
		}
		
};

#endif // !defined(CFLYLOCATION_DLG_H)

