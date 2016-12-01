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
#include "WinUtil.h"
#include "AddMagnet.h"


LRESULT AddMagnet::onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_ctrlMagnet.SetFocus();
	return FALSE;
}

LRESULT AddMagnet::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_ctrlMagnet.Attach(GetDlgItem(IDC_MAGNET_LINK));
	m_ctrlMagnet.SetFocus();
	m_ctrlMagnet.SetWindowText(m_magnet.c_str());
	m_ctrlMagnet.SetSelAll(TRUE);
	
	m_ctrlDescription.Attach(GetDlgItem(IDC_MAGNET));
	m_ctrlDescription.SetWindowText(CTSTRING(MAGNET_SHELL_DESC));
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

/*
magnet:?xt=urn:btih:c9fe2ce1f7f70cb9056206e62b2859fd6f11c04e&dn=rutor.info_Lady+Gaga+-+Joanne+%282016%29+MP3&tr=udp://opentor.org:2710&tr=udp://opentor.org:2710&tr=http://retracker.local/announce
magnet:?xt=urn:btih:c1931558cad7d225aa8630743e3805b70bd839bd&dn=rutor.info_VA+-+20+%D0%9F%D0%B5%D1%81%D0%B5%D0%BD%2C+%D0%BA%D0%BE%D1%82%D0%BE%D1%80%D1%8B%D0%B5+%D0%B7%D0%B0%D1%81%D1%82%D0%B0%D0%B2%D0%BB%D1%8F%D1%8E%D1%82+%D1%81%D0%B5%D1%80%D0%B4%D1%86%D0%B5+%D0%B1%D0%B8%D1%82%D1%8C%D1%81%D1%8F+%D1%87%D0%B0%D1%89%D0%B5+%282016%29+MP3&tr=udp://opentor.org:2710&tr=udp://opentor.org:2710&tr=http://retracker.local/announce
*/

LRESULT AddMagnet::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		WinUtil::GetWindowText(m_magnet, m_ctrlMagnet);
		const StringTokenizer<tstring, TStringList> l_magnets(m_magnet, _T('\n'));
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