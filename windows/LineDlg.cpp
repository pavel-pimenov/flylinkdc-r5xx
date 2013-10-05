/*
 *
 *
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

#include "stdafx.h"

#include "../client/SettingsManager.h"
#include "Resource.h"
#include "LineDlg.h"
#include "atlstr.h"


tstring KickDlg::m_sLastMsg;

LRESULT KickDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	Msgs[0] = Text::toT(SETTING(KICK_MSG_RECENT_01));
	Msgs[1] = Text::toT(SETTING(KICK_MSG_RECENT_02));
	Msgs[2] = Text::toT(SETTING(KICK_MSG_RECENT_03));
	Msgs[3] = Text::toT(SETTING(KICK_MSG_RECENT_04));
	Msgs[4] = Text::toT(SETTING(KICK_MSG_RECENT_05));
	Msgs[5] = Text::toT(SETTING(KICK_MSG_RECENT_06));
	Msgs[6] = Text::toT(SETTING(KICK_MSG_RECENT_07));
	Msgs[7] = Text::toT(SETTING(KICK_MSG_RECENT_08));
	Msgs[8] = Text::toT(SETTING(KICK_MSG_RECENT_09));
	Msgs[9] = Text::toT(SETTING(KICK_MSG_RECENT_10));
	Msgs[10] = Text::toT(SETTING(KICK_MSG_RECENT_11));
	Msgs[11] = Text::toT(SETTING(KICK_MSG_RECENT_12));
	Msgs[12] = Text::toT(SETTING(KICK_MSG_RECENT_13));
	Msgs[13] = Text::toT(SETTING(KICK_MSG_RECENT_14));
	Msgs[14] = Text::toT(SETTING(KICK_MSG_RECENT_15));
	Msgs[15] = Text::toT(SETTING(KICK_MSG_RECENT_16));
	Msgs[16] = Text::toT(SETTING(KICK_MSG_RECENT_17));
	Msgs[17] = Text::toT(SETTING(KICK_MSG_RECENT_18));
	Msgs[18] = Text::toT(SETTING(KICK_MSG_RECENT_19));
	Msgs[19] = Text::toT(SETTING(KICK_MSG_RECENT_20));
	
	ctrlLine.Attach(GetDlgItem(IDC_LINE));
	ctrlLine.SetFocus();
	
	line.clear();
	for (int i = 0; i < 20; i++)
	{
		if (!Msgs[i].empty())
			ctrlLine.AddString(Msgs[i].c_str());
	}
	ctrlLine.SetWindowText(m_sLastMsg.c_str());
	
	ctrlDescription.Attach(GetDlgItem(IDC_DESCRIPTION));
	ctrlDescription.SetWindowText(description.c_str());
	
	SetWindowText(title.c_str());
	
	CenterWindow(GetParent());
	return FALSE;
}

LRESULT KickDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::GetWindowText(line, ctrlLine);
		m_sLastMsg = line;
		int i, j;
		
		for (i = 0; i < 20; i++)
		{
			if (line == Msgs[i])
			{
				for (j = i; j > 0; j--)
				{
					Msgs[j] = Msgs[j - 1];
				}
				Msgs[0] = line;
				break;
			}
		}
		
		if (i >= 20)
		{
			for (j = 19; j > 0; j--)
			{
				Msgs[j] = Msgs[j - 1];
			}
			Msgs[0] = line;
		}
		// TODO use code generator
		SET_SETTING(KICK_MSG_RECENT_01, Text::fromT(Msgs[0]));
		SET_SETTING(KICK_MSG_RECENT_02, Text::fromT(Msgs[1]));
		SET_SETTING(KICK_MSG_RECENT_03, Text::fromT(Msgs[2]));
		SET_SETTING(KICK_MSG_RECENT_04, Text::fromT(Msgs[3]));
		SET_SETTING(KICK_MSG_RECENT_05, Text::fromT(Msgs[4]));
		SET_SETTING(KICK_MSG_RECENT_06, Text::fromT(Msgs[5]));
		SET_SETTING(KICK_MSG_RECENT_07, Text::fromT(Msgs[6]));
		SET_SETTING(KICK_MSG_RECENT_08, Text::fromT(Msgs[7]));
		SET_SETTING(KICK_MSG_RECENT_09, Text::fromT(Msgs[8]));
		SET_SETTING(KICK_MSG_RECENT_10, Text::fromT(Msgs[9]));
		SET_SETTING(KICK_MSG_RECENT_11, Text::fromT(Msgs[10]));
		SET_SETTING(KICK_MSG_RECENT_12, Text::fromT(Msgs[11]));
		SET_SETTING(KICK_MSG_RECENT_13, Text::fromT(Msgs[12]));
		SET_SETTING(KICK_MSG_RECENT_14, Text::fromT(Msgs[13]));
		SET_SETTING(KICK_MSG_RECENT_15, Text::fromT(Msgs[14]));
		SET_SETTING(KICK_MSG_RECENT_16, Text::fromT(Msgs[15]));
		SET_SETTING(KICK_MSG_RECENT_17, Text::fromT(Msgs[16]));
		SET_SETTING(KICK_MSG_RECENT_18, Text::fromT(Msgs[17]));
		SET_SETTING(KICK_MSG_RECENT_19, Text::fromT(Msgs[18]));
		SET_SETTING(KICK_MSG_RECENT_20, Text::fromT(Msgs[19]));
	}
	
	EndDialog(wID);
	return 0;
}
