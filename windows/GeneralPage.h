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

#if !defined(GENERAL_PAGE_H)
#define GENERAL_PAGE_H

#pragma once

#include <atlcrack.h>
#include "wtl_flylinkdc.h"
#include "PropPage.h"
#include "ExListViewCtrl.h"
#include "../XMLParser/XMLParser.h"

class GeneralPage : public CPropertyPage<IDD_GENERAL_PAGE>, public PropPage
{
	public:
		explicit GeneralPage(SettingsManager *s) : PropPage(s, TSTRING(SETTINGS_GENERAL))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~GeneralPage()
		{
#ifndef IRAINMAN_TEMPORARY_DISABLE_XXX_ICON
			m_GenderTypesImageList.Destroy();
#endif
		}
		
		BEGIN_MSG_MAP_EX(GeneralPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_NICK, EN_CHANGE, onTextChanged)
		COMMAND_HANDLER(IDC_EMAIL, EN_CHANGE, onTextChanged)
		COMMAND_HANDLER(IDC_DESCRIPTION, EN_CHANGE, onTextChanged)
		COMMAND_ID_HANDLER(IDC_LANG_LINK, onLinkClick)
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
		COMMAND_ID_HANDLER(IDC_CHECK_ADD_TO_DESCRIPTION, onClickedActive)
#endif
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onGetIP(WORD /* wNotifyCode */, WORD /* wID */, HWND /* hWndCtl */, BOOL& /* bHandled */);
		LRESULT onTextChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onLinkClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
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
		
		CComboBox ctrlLanguage;     // [+] SCALOlaz, Lang Select
		CFlyHyperLink m_LangTranslate;
		
		typedef boost::unordered_map<wstring, string> LanguageMap;
		void fixControls();
		LanguageMap m_languagesList;
		
		void GetLangList();
#ifndef IRAINMAN_USE_REAL_LOCALISATION_IN_SETTINGS
		bool GetLangByFile(const string& p_FileName, LanguageMap& p_LanguagesList);
#endif
		
		CComboBox ctrlConnection;
		CEdit nick;
		CEdit desc;
#ifndef IRAINMAN_TEMPORARY_DISABLE_XXX_ICON
		CComboBoxEx m_GenderTypeComboBox;
		CImageList m_GenderTypesImageList;
		void AddGenderItem(LPWSTR p_Text, int p_image_index, int p_index);
#endif
};

#endif // !defined(GENERAL_PAGE_H)