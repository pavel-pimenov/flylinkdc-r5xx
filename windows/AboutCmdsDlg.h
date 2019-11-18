/*
 * Copyright (C) 2016 FlylinkDC++ Team
 */

#if !defined(ABOUT_CMDS_DLG_H)
#define ABOUT_CMDS_DLG_H

#pragma once

#include "wtl_flylinkdc.h"

class AboutCmdsDlg : public CDialogImpl<AboutCmdsDlg>
#ifdef _DEBUG
	, boost::noncopyable
#endif
{
	public:
		enum { IDD = IDD_ABOUTCMDS };
		
		AboutCmdsDlg() {}
		~AboutCmdsDlg() {}
		
		BEGIN_MSG_MAP(AboutCmdsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			tstring l_thanks = WinUtil::getCommandsList();
			
			//  static const wregex g_tagsRegexA(L"/\n");
			//  l_thanks = regex_replace(l_thanks, g_tagsRegexA, (tstring) L"/\r/\n");
			
			// Clear string from \n, \t and '---' flood, because we use EditControl
			l_thanks = Text::toT(Util::ConvertFromHTMLSymbol(Text::fromT(l_thanks), "--", ""));
			l_thanks = Text::toT(Util::ConvertFromHTMLSymbol(Text::fromT(l_thanks), "-", ""));
			l_thanks = Text::toT(Util::ConvertFromHTMLSymbol(Text::fromT(l_thanks), "\n", "\r\n"));
			l_thanks = Text::toT(Util::ConvertFromHTMLSymbol(Text::fromT(l_thanks), "\t", "."));
			
			CEdit ctrlThanks(GetDlgItem(IDC_THANKS));
			ctrlThanks.FmtLines(TRUE);
			ctrlThanks.AppendText((l_thanks).c_str(), TRUE);
			ctrlThanks.Detach();
			
			//CenterWindow(GetParent());
			return TRUE;
		}
};

#endif // !defined(ABOUT_CMDS_DLG_H)