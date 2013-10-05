/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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

#if !defined(FAV_HUB_PROPERTIES_H)
#define FAV_HUB_PROPERTIES_H


#include <atlcrack.h>

class FavoriteHubEntry;

class FavHubProperties : public CDialogImpl<FavHubProperties>
{
	public:
		FavHubProperties(FavoriteHubEntry *_entry) : entry(_entry) { }
		~FavHubProperties() { }
		
		enum { IDD = IDD_FAVORITEHUB };
		
		BEGIN_MSG_MAP_EX(FavHubProperties)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_HUBNICK, EN_CHANGE, OnTextChanged)
		COMMAND_HANDLER(IDC_HUBPASS, EN_CHANGE, OnTextChanged)
		COMMAND_HANDLER(IDC_HUBUSERDESCR, EN_CHANGE, OnTextChanged)
		COMMAND_HANDLER(IDC_HUBEMAIL, EN_CHANGE, OnTextChanged)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_CLIENT_ID, OnChangeId)
		COMMAND_ID_HANDLER(IDC_WIZARD_NICK_RND, onRandomNick)   // [+] SSA - random Nick
		COMMAND_ID_HANDLER(IDC_WIZARD_NICK_RND2, onDefaultNick) // [+] SCALOlaz - set Nick from Global Settings
		END_MSG_MAP()
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/);
		LRESULT OnChangeId(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onRandomNick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDefaultNick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		CComboBox IdCombo; // !SMT!-S
		
	protected:
		FavoriteHubEntry *entry;
};

#endif // !defined(FAV_HUB_PROPERTIES_H)

/**
 * @file
 * $Id: FavHubProperties.h,v 1.9 2006/05/08 08:36:19 bigmuscle Exp $
 */
