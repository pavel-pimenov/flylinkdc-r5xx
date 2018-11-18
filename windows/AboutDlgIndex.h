/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#if !defined(ABOUT_DLG_INDEX_H)
#define ABOUT_DLG_INDEX_H


#pragma once


#include "AboutDlg.h"
#include "AboutCmdsDlg.h"
#include "AboutLogDlg.h"
#include "AboutStatDlg.h"
//#include other 3,4... pages
#include "HIconWrapper.h"
#include "wtl_flylinkdc.h"

class AboutDlgIndex : public CDialogImpl<AboutDlgIndex>
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		static const int m_log_page = 0;        // Temp : deactivate Log Page (VERSION_HISTORY), because code not present
		
		enum { IDD = IDD_ABOUTTABS };
		enum
		{
			ABOUT,
			CHAT_COMMANDS,
			STATISTICS,
			VERSION_HISTORY,
		};
		AboutDlgIndex(): m_pTabDialog(0)
		{
		}
		~AboutDlgIndex()
		{
			m_ctrTab.Detach();
		}
		
		BEGIN_MSG_MAP(AboutDlgIndex)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		NOTIFY_HANDLER(IDC_ABOUTTAB, TCN_SELCHANGE, OnSelchangeTabs)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			int ab = 0;
			if (_MSC_VER >= 1600)
				ab = 2010;
			if (_MSC_VER >= 1700)
				ab = 2012;
			if (_MSC_VER >= 1800) // 1800: MSVC 2013 (yearly release cycle)
				ab = 2013;
			if (_MSC_VER >= 1900)
				ab = 2015;
			if (_MSC_VER >= 1910)
				ab = 2017;
			char l_full_version[64];
			_snprintf(l_full_version, _countof(l_full_version), "%d (%d)", ab, _MSC_FULL_VER);
			
			SetDlgItemText(IDC_COMPT, (TSTRING(COMPILED_ON) + _T(' ') + Util::getCompileDate() + _T(' ') + Util::getCompileTime(_T("%H:%M:%S"))
			                           + _T(", Visual C++ ") + Text::toT(l_full_version)).c_str());
			SetWindowText(CTSTRING(MENU_ABOUT));
			m_ctrTab.Attach(GetDlgItem(IDC_ABOUTTAB));
			TCITEM tcItem;
			tcItem.mask = TCIF_TEXT | TCIF_PARAM;
			tcItem.iImage = -1;
			
			// Page List. May be optimized to massive
			tcItem.pszText = (LPWSTR) CTSTRING(MENU_ABOUT);
			m_Page1 = std::unique_ptr<AboutDlg>(new AboutDlg());
			tcItem.lParam = (LPARAM)&m_Page1;
			m_ctrTab.InsertItem(0, &tcItem);
			m_Page1->Create(m_ctrTab.m_hWnd, AboutDlg::IDD);
			
			tcItem.pszText = (LPWSTR) CTSTRING(CHAT_COMMANDS);
			m_Page2 = std::unique_ptr<AboutCmdsDlg>(new AboutCmdsDlg());
			tcItem.lParam = (LPARAM)&m_Page2;
			m_ctrTab.InsertItem(1, &tcItem);
			m_Page2->Create(m_ctrTab.m_hWnd, AboutCmdsDlg::IDD);
			
			tcItem.pszText = (LPWSTR) CTSTRING(ABOUT_STATISTICS);
			m_Page3 = std::unique_ptr<AboutStatDlg>(new AboutStatDlg());
			tcItem.lParam = (LPARAM)&m_Page3;
			m_ctrTab.InsertItem(2, &tcItem);
			m_Page3->Create(m_ctrTab.m_hWnd, AboutStatDlg::IDD);
			
			if (BOOLSETTING(AUTOUPDATE_ENABLE))
			{
				tcItem.pszText = (LPWSTR) _T("Update Log");
				m_Page4 = std::unique_ptr<AboutLogDlg>(new AboutLogDlg());
				tcItem.lParam = (LPARAM)&m_Page4;
				if (m_log_page > 0)
					m_ctrTab.InsertItem(3, &tcItem);
				m_Page4->Create(m_ctrTab.m_hWnd, AboutLogDlg::IDD);
			}
			
			m_ctrTab.SetCurSel(m_pTabDialog);
			
			// for Custom Themes
			m_png_logo.LoadFromResourcePNG(IDR_FLYLINK_PNG);
			GetDlgItem(IDC_STATIC).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)m_png_logo));
			//m_hIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_MAINFRAME));   // [!] SSA - because of theme use
			//SetIcon((HICON)*m_hIcon, FALSE);
			//SetIcon((HICON)*m_hIcon, TRUE);
			
			CenterWindow(GetParent());
			
			NMHDR hdr;  // Give PENDEL to View First Tab (from param m_pTabDialog, for start with 2 or other open Tab) :)
			hdr.code = TCN_SELCHANGE;
			hdr.hwndFrom = m_ctrTab.m_hWnd;
			hdr.idFrom = IDC_ABOUTTAB;
			::SendMessage(m_ctrTab.m_hWnd, WM_NOTIFY, hdr.idFrom, (LPARAM)&hdr);
			
			return TRUE;
		}
		
		LRESULT OnSelchangeTabs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
		{
			int pos = m_ctrTab.GetCurSel(); // Selected Tab #
			// Hide All Dialogs
			m_Page1->ShowWindow(SW_HIDE);
			m_Page2->ShowWindow(SW_HIDE);
			m_Page3->ShowWindow(SW_HIDE);
			if (m_log_page > 0)
				m_Page4->ShowWindow(SW_HIDE);
			m_pTabDialog = pos;
			
			CRect rc;
			m_ctrTab.GetClientRect(&rc); // Get work Area Rect
			m_ctrTab.AdjustRect(FALSE, &rc);    // Clear Tab line from Rect. Do not "TRUE"
			rc.left -= 3;
			
			switch (pos)
			{
				case ABOUT: // About
				{
					m_Page1->MoveWindow(&rc);
					m_Page1->ShowWindow(SW_SHOW);
					break;
				}
				case CHAT_COMMANDS: // Chat Commands
				{
					m_Page2->MoveWindow(&rc);
					m_Page2->ShowWindow(SW_SHOW);
					break;
				}
				case STATISTICS: // UDP, TCP, TLS etc statistics
				{
					m_Page3->MoveWindow(&rc);
					m_Page3->ShowWindow(SW_SHOW);
					break;
				}
				case VERSION_HISTORY: // Version History, Log
				{
					if (m_log_page > 0)
					{
						m_Page4->MoveWindow(&rc);
						m_Page4->ShowWindow(SW_SHOW);
					}
					break;
				}
			}
			return 1;
		}
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			EndDialog(wID);
			return 0;
		}
	private:
		ExCImage m_png_logo;
		//std::unique_ptr<HIconWrapper> m_hIcon;
		
		CTabCtrl m_ctrTab;
		int m_pTabDialog;
		
		std::unique_ptr<AboutDlg> m_Page1;
		std::unique_ptr<AboutCmdsDlg> m_Page2;
		std::unique_ptr<AboutStatDlg> m_Page3;
		std::unique_ptr<AboutLogDlg> m_Page4;
};

#endif // !defined(ABOUT_DLG_INDEX_H)