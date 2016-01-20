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

#include "stdafx.h"
#include "../client/Socket.h"
#include "Resource.h"
#include "GeneralPage.h"
#include "WinUtil.h"
#include "ResourceLoader.h" // [+] InfinitySky. PNG Support from Apex 1.3.8.

// Переводы элементов интерфейса.
PropPage::TextItem GeneralPage::texts[] =
{
	{ IDC_SETTINGS_PERSONAL_INFORMATION, ResourceManager::SETTINGS_PERSONAL_INFORMATION },
	{ IDC_SETTINGS_NICK, ResourceManager::NICK },
	{ IDC_SETTINGS_EMAIL, ResourceManager::EMAIL },
#ifdef FLYLINKDC_USE_XXX_ICON
	{ IDC_SETTINGS_GENDER, ResourceManager::FLY_GENDER },
#endif
	{ IDC_SETTINGS_DESCRIPTION, ResourceManager::DESCRIPTION },
	{ IDC_SETTINGS_UPLOAD_LINE_SPEED, ResourceManager::SETTINGS_UPLOAD_LINE_SPEED },
	{ IDC_SETTINGS_MEBIBITS, ResourceManager::MBPS },
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	{ IDC_CHECK_ADD_TO_DESCRIPTION, ResourceManager::ADD_TO_DESCRIPTION },
	{ IDC_CHECK_ADD_LIMIT, ResourceManager::ADD_LIMIT },
	{ IDC_CHECK_ADD_SLOTS, ResourceManager::ADD_SLOTS },
#endif
	{ IDC_SETTINGS_NOMINALBW, ResourceManager::SETTINGS_NOMINAL_BANDWIDTH },
	{ IDC_CD_GP, ResourceManager::CUST_DESC },
	{ IDC_TECHSUPPORT_BORDER, ResourceManager::TECHSUPPORT },
	{ IDC_CONNECT_TO_SUPPORT_HUB, ResourceManager::CONNECT_TO_SUPPORT_HUB },
	{ IDC_DISABLE_AUTOREMOVE_VIRUS_HUB, ResourceManager::DISABLE_AUTOREMOVE_VIRUS_HUB},
	{ IDC_SETTINGS_LANGUAGE, ResourceManager::SETTINGS_LANGUAGE },
	{ IDC_USE_FLY_SERVER_STATICTICS_SEND, ResourceManager::SETTINGS_STATISTICS_SEND },
	{ IDC_ENCODINGTEXT, ResourceManager::DEFAULT_NCDC_HUB_ENCODINGTEXT },
	{ IDC_ANTIVIRUS_AUTOBAN_FOR_IP, ResourceManager::SETTINGS_ANTIVIRUS_AUTOBAN_FOR_IP },
	{ IDC_ANTIVIRUS_AUTOBAN_FOR_NICK, ResourceManager::SETTINGS_ANTIVIRUS_AUTOBAN_FOR_NICK },
	{ IDC_ANTIVIRUS_COMMAND_IP, ResourceManager::SETTINGS_ANTIVIRUS_COMMAND_IP },
	
	
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item GeneralPage::items[] =
{
	{ IDC_NICK,         SettingsManager::NICK,          PropPage::T_STR },
	{ IDC_EMAIL,        SettingsManager::EMAIL,         PropPage::T_STR },
	{ IDC_DESCRIPTION,  SettingsManager::DESCRIPTION,   PropPage::T_STR },
	{ IDC_CONNECTION,   SettingsManager::UPLOAD_SPEED,  PropPage::T_STR },
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	{ IDC_CHECK_ADD_TO_DESCRIPTION,   SettingsManager::ADD_TO_DESCRIPTION,    PropPage::T_BOOL },
	{ IDC_CHECK_ADD_LIMIT,   SettingsManager::ADD_DESCRIPTION_LIMIT,    PropPage::T_BOOL },
	{ IDC_CHECK_ADD_SLOTS, SettingsManager::ADD_DESCRIPTION_SLOTS,    PropPage::T_BOOL },
#endif
	{ IDC_CONNECT_TO_SUPPORT_HUB, SettingsManager::CONNECT_TO_SUPPORT_HUB, PropPage::T_BOOL },
	{ IDC_DISABLE_AUTOREMOVE_VIRUS_HUB, SettingsManager::DISABLE_AUTOREMOVE_VIRUS_HUB, PropPage::T_BOOL },
	{ IDC_USE_FLY_SERVER_STATICTICS_SEND, SettingsManager::USE_FLY_SERVER_STATICTICS_SEND, PropPage::T_BOOL },
	{ 0, 0, PropPage::T_END }
};

void GeneralPage::write()
{
	PropPage::write((HWND)(*this), items);
	ctrlLanguage.Attach(GetDlgItem(IDC_LANGUAGE));
	const string l_filelang = WinUtil::getDataFromMap(ctrlLanguage.GetCurSel(), m_languagesList);
	dcassert(!l_filelang.empty());
	if (SETTING(LANGUAGE_FILE) != l_filelang)
	{
		g_settings->set(SettingsManager::LANGUAGE_FILE, l_filelang);
		SettingsManager::getInstance()->save();
		ResourceManager::loadLanguage(Util::getLocalisationPath() + l_filelang);
		if (m_languagesList.size() != 1)
			MessageBox(CTSTRING(CHANGE_LANGUAGE_INFO), CTSTRING(CHANGE_LANGUAGE), MB_OK | MB_ICONEXCLAMATION);
	}
	ctrlLanguage.Detach();
#ifdef FLYLINKDC_USE_XXX_ICON
	g_settings->set(SettingsManager::FLY_GENDER, m_GenderTypeComboBox.GetCurSel());
	ClientManager::resend_ext_json();
#endif
}

#ifdef FLYLINKDC_USE_XXX_ICON
void GeneralPage::AddGenderItem(LPCWSTR p_Text, int p_image_index, int p_index) // [+] FlylinkDC++
{
	COMBOBOXEXITEM cbitem = {CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE};
	cbitem.pszText = (LPWSTR)p_Text;
	cbitem.iItem = p_index;
	cbitem.iImage = p_image_index;
	cbitem.iSelectedImage = p_image_index;
	m_GenderTypeComboBox.InsertItem(&cbitem);
}
#endif

LRESULT GeneralPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	::EnableWindow(GetDlgItem(IDC_DISABLE_AUTOREMOVE_VIRUS_HUB), FALSE);
#ifndef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_TO_DESCRIPTION), FALSE);
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_SLOTS), FALSE);
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_LIMIT), FALSE);
	GetDlgItem(IDC_CHECK_ADD_TO_DESCRIPTION).ShowWindow(FALSE);
	GetDlgItem(IDC_CHECK_ADD_SLOTS).ShowWindow(FALSE);
	GetDlgItem(IDC_CHECK_ADD_LIMIT).ShowWindow(FALSE);
#endif
	PropPage::translate((HWND)(*this), texts);
	ctrlConnection.Attach(GetDlgItem(IDC_CONNECTION));
	
	for (auto i = SettingsManager::g_connectionSpeeds.cbegin(); i != SettingsManager::g_connectionSpeeds.cend(); ++i)
	{
		ctrlConnection.AddString(Text::toT(*i).c_str());
	}
	
	PropPage::read((HWND)(*this), items);
	
	ctrlLanguage.Attach(GetDlgItem(IDC_LANGUAGE));
	GetLangList();
	for (auto i = m_languagesList.cbegin(); i != m_languagesList.cend(); ++i)
	{
		ctrlLanguage.AddString(i->first.c_str());
	}
	
	m_LangTranslate.init(GetDlgItem(IDC_LANG_LINK), _T(""));
	
	ctrlLanguage.SetCurSel(WinUtil::getIndexFromMap(m_languagesList, SETTING(LANGUAGE_FILE)));
	ctrlLanguage.Detach();
	
	ctrlConnection.SetCurSel(ctrlConnection.FindString(0, Text::toT(SETTING(UPLOAD_SPEED)).c_str()));
	
#ifdef FLYLINKDC_USE_XXX_ICON
	m_GenderTypeComboBox.Attach(GetDlgItem(IDC_GENDER));
	ResourceLoader::LoadImageList(IDR_GENDER_USERS, m_GenderTypesImageList, 16, 16);
	m_GenderTypeComboBox.SetImageList(m_GenderTypesImageList);
#else
	::EnableWindow(GetDlgItem(IDC_GENDER), FALSE);
#endif
	
	nick.Attach(GetDlgItem(IDC_NICK));
	nick.LimitText(49); // [~] InfinitySky. 35->49.
	nick.Detach();
	
	desc.Attach(GetDlgItem(IDC_DESCRIPTION));
	desc.LimitText(100); // [~] InfinitySky. 50->100.
	desc.Detach();
	
	desc.Attach(GetDlgItem(IDC_SETTINGS_EMAIL));
	desc.LimitText(64);
	desc.Detach();
	
#ifdef FLYLINKDC_USE_XXX_ICON
	int l_id = 0;
	AddGenderItem(CWSTRING(FLY_GENDER_NONE), l_id++, 0);
	AddGenderItem(CWSTRING(FLY_GENDER_MALE), l_id++, 1);
	AddGenderItem(CWSTRING(FLY_GENDER_FEMALE), l_id++, 2);
	AddGenderItem(CWSTRING(FLY_GENDER_ASEXUAL), l_id++, 3);
	m_GenderTypeComboBox.SetCurSel(SETTING(FLY_GENDER));
#endif
	
	CComboBox combo;
	combo.Attach(GetDlgItem(IDC_ENCODING));
	combo.AddString(Text::toT(Text::g_code1251).c_str());
	combo.AddString(_T("System default"));
	if (Text::g_systemCharset == Text::g_code1251)
	{
		combo.SetCurSel(0);
	}
	else
	{
		combo.SetCurSel(1);
	}
	combo.Detach();
	::EnableWindow(GetDlgItem(IDC_ENCODING), FALSE);
	
	fixControls();
	return TRUE;
}

LRESULT GeneralPage::onTextChanged(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
	tstring buf;
	GET_TEXT(wID, buf);
	tstring old = buf;
	
	// TODO: move to Text and cleanup.
	if (!buf.empty())
	{
		// Strip '$', '|', '<', '>' and ' ' from text
		TCHAR *b = &buf[0], *f = &buf[0], c;
		while ((c = *b++) != '\0')
		{
			if (c != '$' && c != '|' && (wID == IDC_DESCRIPTION || c != ' ') && ((wID != IDC_NICK && wID != IDC_DESCRIPTION && wID != IDC_SETTINGS_EMAIL) || (c != '<' && c != '>')))
				*f++ = c;
		}
		
		*f = '\0';
	}
	if (old != buf)
	{
		// Something changed; update window text without changing cursor pos
		CEdit tmp;
		tmp.Attach(hWndCtl);
		int start, end;
		tmp.GetSel(start, end);
		tmp.SetWindowText(buf.data());
		if (start > 0) start--;
		if (end > 0) end--;
		tmp.SetSel(start, end);
		tmp.Detach();
	}
	
	return TRUE;
}

LRESULT GeneralPage::onClickedActive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

LRESULT GeneralPage::onLinkClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	WinUtil::openLink(_T("https://www.transifex.com/projects/p/flylinkdc/"));
	return 0;
}
void GeneralPage::GetLangList()
{
	if (m_languagesList.empty())
	{
		m_languagesList.insert(make_pair(L"English", "en-US.xml"));
		const StringList& l_files = File::findFiles(Util::getLocalisationPath(), "*-*.xml");
		for (auto i = l_files.cbegin(); i != l_files.cend(); ++i)
		{
			string l_langFileName = Util::getFileName(*i);
#ifndef IRAINMAN_USE_REAL_LOCALISATION_IN_SETTINGS
			if (!GetLangByFile(l_langFileName, m_languagesList))
#endif
			{
				XMLParser::XMLResults xRes;
				const XMLParser::XMLNode xRootNode = XMLParser::XMLNode::parseFile(Text::toT(*i).c_str(), 0, &xRes);
				if (xRes.error == XMLParser::eXMLErrorNone)
				{
					const XMLParser::XMLNode ResNode = xRootNode.getChildNode("Language");
					if (!ResNode.isEmpty())
					{
						m_languagesList.insert(make_pair(Text::toT(ResNode.getAttributeOrDefault("Name")), l_langFileName));
					}
				}
			}
		}
	}
}
#ifndef IRAINMAN_USE_REAL_LOCALISATION_IN_SETTINGS
struct LangInfo
{
	WCHAR* Lang;
	char* FileName;
};


bool GeneralPage::GetLangByFile(const string& p_FileName, LanguageMap& p_LanguagesList)
{
	static const LangInfo g_DefaultLang[] =
	{
		L"Беларуская (Беларусь)", "be-BY.xml",
		L"English"              , "en-US.xml",
		L"Español (España)"     , "es-ES.xml",
		L"Francés (Francia)"    , "fr-FR.xml",
		L"Português (Brasil)"   , "pt-BR.xml",
		L"Русский (Россия)"     , "ru-RU.xml",
		L"Український (Україна)", "uk-UA.xml"
	};
	for (int i = 0; i < _countof(g_DefaultLang); ++i)
	{
		if (p_FileName == g_DefaultLang[i].FileName)
		{
			m_languagesList.insert(make_pair(g_DefaultLang[i].Lang, g_DefaultLang[i].FileName));
			return true;
		}
	}
	return false;
}
#endif // IRAINMAN_USE_REAL_LOCALISATION_IN_SETTINGS
void GeneralPage::fixControls()
{
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	BOOL use_description = IsDlgButtonChecked(IDC_CHECK_ADD_TO_DESCRIPTION) == BST_CHECKED;
	
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_SLOTS), use_description);
	::EnableWindow(GetDlgItem(IDC_CHECK_ADD_LIMIT), use_description);
#endif
}