/*
 * Copyright (C) 2010-2016 FlylinkDC++ Team http://flylinkdc.com/
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
#include "Resource.h"
#include "AddMagnet.h"
#include "WinUtil.h"
#include "HIconWrapper.h"


tstring MagnetLink;

LRESULT AddMagnet::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ctrlMagnet.Attach(GetDlgItem(IDC_MAGNET_LINK));
	ctrlMagnet.SetFocus();
	ctrlMagnet.SetWindowText(MagnetLink.c_str());
	ctrlMagnet.SetSelAll(TRUE);
	
	ctrlDescription.Attach(GetDlgItem(IDC_MAGNET));
	ctrlDescription.SetWindowText(CTSTRING(MAGNET_SHELL_DESC));
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	SetDlgItemText(IDC_MAGNET_START_VIEW, CTSTRING(MAGNET_START_VIEW));
	GetDlgItem(IDC_MAGNET_START_VIEW).EnableWindow(SETTING(MAGNET_ACTION) == SettingsManager::MAGNET_AUTO_DOWNLOAD);
#else
	::ShowWindow(GetDlgItem(IDC_MAGNET_START_VIEW), false);
#endif
	
	// Заголовок окна.
	SetWindowText(CTSTRING(ADDING_MAGNET_LINK));
	
	// Иконка.
	m_MagnetIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(IDR_MAGNET));// [!] SSA   (HICON)::LoadImage(_Module.get_m_hInst(), MAKEINTRESOURCE(IDR_MAGNET),IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR); // [~] Sergey Shushkanov
	SetIcon((HICON)*m_MagnetIcon, FALSE);
	SetIcon((HICON)*m_MagnetIcon, TRUE);
	
	CenterWindow(GetParent());
	return FALSE;
}
LRESULT AddMagnet::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::GetWindowText(MagnetLink, ctrlMagnet);
		const StringTokenizer<tstring, TStringList> l_magnets(MagnetLink, _T('\n'));
		for (auto j = l_magnets.getTokens().cbegin(); j != l_magnets.getTokens().cend() ; ++j)
		{
			WinUtil::parseMagnetUri(*j, WinUtil::MA_DEFAULT
#ifdef SSA_VIDEO_PREVIEW_FEATURE
			                        , IsDlgButtonChecked(IDC_MAGNET_START_VIEW) == TRUE
#endif
			                       );
		}
	}
	EndDialog(wID);
	return 0;
}