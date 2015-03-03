/*
 * Copyright (C) 2015 FlylinkDC++ Team
 */

#if !defined(ABOUT_STAT_DLG_H)
#define ABOUT_STAT_DLG_H
#include "wtl_flylinkdc.h"
//#include "ExListViewCtrl.h"

class AboutStatDlg : public CDialogImpl<AboutStatDlg>
#ifdef _DEBUG
	, boost::noncopyable , virtual NonDerivable<AboutStatDlg> // [+] IRainman fix.
#endif
{
	public:
		enum { IDD = IDD_ABOUTSTAT };
		
		AboutStatDlg()  {}
		~AboutStatDlg() {}
		
		BEGIN_MSG_MAP(AboutStatDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			SetDlgItemText(IDC_STATISTICS_STATIC, (TSTRING(ABOUT_STATISTICS) + L':').c_str());
			SetDlgItemText(IDC_LATEST, CTSTRING(DOWNLOADING)); // what is it?
			
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
			CFlylinkDBManager::getInstance()->load_global_ratio();
			SetDlgItemText(IDC_TOTAL_UPLOAD, (TSTRING(UPLOADED) + _T(": ") +
			                                  Text::toT(Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_upload()))).c_str());
			SetDlgItemText(IDC_TOTAL_DOWNLOAD, (TSTRING(DOWNLOADED) + _T(": ") +
			                                    Text::toT(Util::formatBytes(CFlylinkDBManager::getInstance()->m_global_ratio.get_download()))).c_str());
			SetDlgItemText(IDC_RATIO, (TSTRING(RATING) + _T(": ") + (CFlylinkDBManager::getInstance()->get_ratioW())).c_str());
#else
			SetDlgItemText(IDC_TOTAL_UPLOAD, (TSTRING(UPLOADED) + _T(": ") + TSTRING(NONE));
			               SetDlgItemText(IDC_TOTAL_DOWNLOAD, (TSTRING(DOWNLOADED) + _T(": ") + TSTRING(NONE));
			                              SetDlgItemText(IDC_RATIO, (TSTRING(RATING) + _T(": ") + TSTRING(NONE));
#endif
			
			CEdit ctrlUDPStat(GetDlgItem(IDC_UDP_DHT_SSL_STAT));
			//ctrlUDPStat.SetFont(Fonts::g_halfFont);
			ctrlUDPStat.AppendText(Text::toT(CompatibilityManager::generateNetworkStats()).c_str(), FALSE);
			ctrlUDPStat.Detach();
			return TRUE;
		}
};

#endif // !defined(ABOUT_STAT_DLG_H)