//-----------------------------------------------------------------------------
//(c) 20014-2019 pavel.pimenov@gmail.com
//-----------------------------------------------------------------------------

#include "stdafx.h"

#include "Resource.h"
#include "CFlyLocationDlg.h"
#include "atlstr.h"

#ifdef FLYLINKDC_USE_LOCATION_DIALOG

LRESULT CFlyLocationDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_ctrlCountry.Attach(GetDlgItem(IDC_LOCATION_COUNTRY));
	m_ctrlCity.Attach(GetDlgItem(IDC_LOCATION_CITY));
	m_ctrlProvider.Attach(GetDlgItem(IDC_LOCATION_PROVIDER));
	
	auto insertAndLocate = [&](CComboBox & m_box, const string & p_text) -> void
	{
		if (!p_text.empty())
		{
			const auto l_text = Text::toT(p_text);
			if (m_box.FindString(0, l_text.c_str()) < 0)
			{
				m_box.AddString(l_text.c_str());
				m_box.SelectString(0, l_text.c_str());
			}
		}
	};
	
	m_ctrlCountry.AddString(_T("Russia"));
	m_ctrlCountry.SelectString(0, _T("Russia"));
	m_ctrlCountry.AddString(_T("Belarus"));
	m_ctrlCountry.AddString(_T("Kazakhstan"));
	insertAndLocate(m_ctrlCountry, SETTING(FLY_LOCATOR_COUNTRY));
	insertAndLocate(m_ctrlCity, SETTING(FLY_LOCATOR_CITY));
	insertAndLocate(m_ctrlProvider, SETTING(FLY_LOCATOR_ISP));
	
	m_ctrlCity.SetFocus();
	CenterWindow(GetParent());
	return FALSE;
}
LRESULT CFlyLocationDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& bHandled)
{
	if (wID == IDOK)
	{
		WinUtil::GetWindowText(m_Country, m_ctrlCountry);
		WinUtil::GetWindowText(m_City, m_ctrlCity);
		WinUtil::GetWindowText(m_Provider, m_ctrlProvider);
		if (isEmpty())
		{
			::MessageBox(m_hWnd, _T("Fill all fields"), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
			bHandled = FALSE;
			return 0;
		}
	}
	EndDialog(wID);
	return 0;
}

#endif