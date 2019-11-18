/*
 * Copyright (C) 2001-2017 Jacek Sieka, j_s@telia.com
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

#include "PopupsPage.h"
#include "MainFrm.h"
#include "PopupManager.h"

PropPage::TextItem Popups::texts[] =
{
	{ IDC_POPUPGROUP, ResourceManager::BALLOON_POPUPS },
	{ IDC_PREVIEW, ResourceManager::PREVIEW_MENU },
	{ IDC_POPUPTYPE, ResourceManager::POPUP_TYPE },
	{ IDC_POPUP_TIME_STR, ResourceManager::POPUP_TIME },
	{ IDC_S, ResourceManager::S },
	{ IDC_POPUP_BACKCOLOR, ResourceManager::POPUP_BACK_COLOR },
	{ IDC_POPUP_FONT, ResourceManager::POPUP_FONT },
	{ IDC_POPUP_TITLE_FONT, ResourceManager::POPUP_TITLE_FONT },
	{ IDC_MAX_MSG_LENGTH_STR, ResourceManager::MAX_MSG_LENGTH },
	{ IDC_POPUP_W_STR, ResourceManager::WIDTH },
	{ IDC_POPUP_H_STR, ResourceManager::HEIGHT },
	{ IDC_POPUP_TRANSP_STR, ResourceManager::TRANSPARENCY },
	{ IDC_POPUP_IMAGE_GP, ResourceManager::POPUP_IMAGE },
	{ IDC_POPUP_COLORS, ResourceManager::POPUP_COLORS },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item Popups::items[] =
{
	{ IDC_POPUP_TIME, SettingsManager::POPUP_TIME, PropPage::T_INT },
	{ IDC_POPUPFILE, SettingsManager::POPUPFILE, PropPage::T_STR },
	{ IDC_MAX_MSG_LENGTH, SettingsManager::MAX_MSG_LENGTH, PropPage::T_INT },
	{ IDC_POPUP_W, SettingsManager::POPUP_W, PropPage::T_INT },
	{ IDC_POPUP_H, SettingsManager::POPUP_H, PropPage::T_INT },
	{ 0, 0, PropPage::T_END }
};

Popups::ListItem Popups::listItems[] =
{
	{ SettingsManager::POPUP_HUB_CONNECTED, ResourceManager::POPUP_HUB_CONNECTED },
	{ SettingsManager::POPUP_HUB_DISCONNECTED, ResourceManager::POPUP_HUB_DISCONNECTED },
	{ SettingsManager::POPUP_FAVORITE_CONNECTED, ResourceManager::POPUP_FAVORITE_CONNECTED },
	{ SettingsManager::POPUP_FAVORITE_DISCONNECTED, ResourceManager::POPUP_FAVORITE_DISCONNECTED },
	{ SettingsManager::POPUP_CHEATING_USER, ResourceManager::POPUP_CHEATING_USER },
	{ SettingsManager::POPUP_CHAT_LINE, ResourceManager::POPUP_CHAT_LINE },
	{ SettingsManager::POPUP_DOWNLOAD_START, ResourceManager::POPUP_DOWNLOAD_START },
	{ SettingsManager::POPUP_DOWNLOAD_FAILED, ResourceManager::POPUP_DOWNLOAD_FAILED },
	{ SettingsManager::POPUP_DOWNLOAD_FINISHED, ResourceManager::POPUP_DOWNLOAD_FINISHED },
	{ SettingsManager::POPUP_UPLOAD_FINISHED, ResourceManager::POPUP_UPLOAD_FINISHED },
	{ SettingsManager::POPUP_PM, ResourceManager::POPUP_PM },
	{ SettingsManager::POPUP_NEW_PM, ResourceManager::POPUP_NEW_PM },
	{ SettingsManager::PM_PREVIEW, ResourceManager::PM_PREVIEW },
#ifdef IRAINMAN_INCLUDE_RSS
	{ SettingsManager::POPUP_NEW_RSSNEWS, ResourceManager::POPUP_NEW_RSSNEWS },
#endif
	{ SettingsManager::POPUP_SEARCH_SPY, ResourceManager::POPUP_SEARCH_SPY },
//	{ SettingsManager::POPUP_NEW_FOLDERSHARE, ResourceManager::POPUP_NEW_FOLDERSHARE },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

LRESULT Popups::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read(*this, items, listItems, GetDlgItem(IDC_POPUPLIST));
	
	ctrlPopups.Attach(GetDlgItem(IDC_POPUPLIST));
	
	ctrlPopupType.Attach(GetDlgItem(IDC_POPUP_TYPE));
	ctrlPopupType.AddString(CTSTRING(POPUP_BALOON));
	ctrlPopupType.AddString(CTSTRING(POPUP_CUSTOM));
	ctrlPopupType.AddString(CTSTRING(POPUP_SPLASH));
	ctrlPopupType.AddString(CTSTRING(POPUP_WINDOW));
	ctrlPopupType.SetCurSel(SETTING(POPUP_TYPE));
	
	CUpDownCtrl spin;
	spin.Attach(GetDlgItem(IDC_POPUP_TIME_SPIN));
	spin.SetBuddy(GetDlgItem(IDC_POPUP_TIME));
	spin.SetRange32(1, 15);
	spin.Detach();
	
	spin.Attach(GetDlgItem(IDC_MAX_MSG_LENGTH_SPIN));
	spin.SetBuddy(GetDlgItem(IDC_MAX_MSG_LENGTH));
	spin.SetRange32(1, 512);
	spin.Detach();
	
	spin.Attach(GetDlgItem(IDC_POPUP_W_SPIN));
	spin.SetBuddy(GetDlgItem(IDC_POPUP_W));
	spin.SetRange32(80, 599);
	spin.Detach();
	
	spin.Attach(GetDlgItem(IDC_POPUP_H_SPIN));
	spin.SetBuddy(GetDlgItem(IDC_POPUP_H));
	spin.SetRange32(50, 299);
	spin.Detach();
	
	slider = GetDlgItem(IDC_POPUP_TRANSP_SLIDER);
	slider.SetRangeMin(50, true);
	slider.SetRangeMax(255, true);
	slider.SetPos(SETTING(POPUP_TRANSP));
	
	SetDlgItemText(IDC_POPUPFILE, Text::toT(SETTING(POPUPFILE)).c_str());
	
	if (SETTING(POPUP_TYPE) == BALLOON)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_FONT), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_TITLE_FONT), false);
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), false);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_W), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_W_SPIN), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_H), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_H_SPIN), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_TRANSP_SLIDER), false);
	}
	else if (SETTING(POPUP_TYPE) == CUSTOM)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), false);
	}
	else
	{
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), false);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), false);
	}
	
	SetDlgItemText(IDC_POPUP_INPROGRAM, CTSTRING(POPUPS_INPROGRAM));
	SetDlgItemText(IDC_POPUP_ENABLE, CTSTRING(ENABLE_POPUPS));
	CheckDlgButton(IDC_POPUP_ENABLE, SETTING(POPUPS_DISABLED) ? BST_UNCHECKED : BST_CHECKED);
	SetDlgItemText(IDC_POPUP_TABS_ENABLE, CTSTRING(POPUPS_IN_TABS));
	CheckDlgButton(IDC_POPUP_TABS_ENABLE, SETTING(POPUPS_TABS_ENABLED) ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(IDC_POPUP_MESSAGEPANEL_ENABLE, CTSTRING(POPUPS_IN_BBCODEPANEL));
	CheckDlgButton(IDC_POPUP_MESSAGEPANEL_ENABLE, SETTING(POPUPS_MESSAGEPANEL_ENABLED) ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(IDC_POPUP_SHOW_INFO_TIPS_ENABLE, CTSTRING(SETTINGS_SHOW_INFO_TIPS));
	CheckDlgButton(IDC_POPUP_SHOW_INFO_TIPS_ENABLE, SETTING(SHOW_INFOTIPS) ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(IDC_POPUP_AWAY, CTSTRING(SHOW_POPUP_AWAY));
	CheckDlgButton(IDC_POPUP_AWAY, SETTING(POPUP_AWAY) ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(IDC_POPUP_MINIMIZED, CTSTRING(SHOW_POPUP_MINIMIZED));
	CheckDlgButton(IDC_POPUP_MINIMIZED, SETTING(POPUP_MINIMIZED) ? BST_CHECKED : BST_UNCHECKED);
	fixControls();
	
	return TRUE;
}

LRESULT Popups::onBackColor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CColorDialog dlg(SETTING(POPUP_BACKCOLOR), CC_FULLOPEN);
	if (dlg.DoModal() == IDOK)
		SET_SETTING(POPUP_BACKCOLOR, (int)dlg.GetColor());
	return 0;
}

LRESULT Popups::onFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LOGFONT font  = {0};
	Fonts::decodeFont(Text::toT(SETTING(POPUP_FONT)), font);
	CFontDialog dlg(&font, CF_EFFECTS | CF_SCREENFONTS | CF_FORCEFONTEXIST);
	dlg.m_cf.rgbColors = SETTING(POPUP_TEXTCOLOR);
	if (dlg.DoModal() == IDOK)
	{
		SET_SETTING(POPUP_TEXTCOLOR, (int)dlg.GetColor());
		SET_SETTING(POPUP_FONT, Text::fromT(WinUtil::encodeFont(font)));
	}
	return 0;
}

LRESULT Popups::onTitleFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	LOGFONT tmp = myFont;
	Fonts::decodeFont(Text::toT(SETTING(POPUP_TITLE_FONT)), tmp);
	CFontDialog dlg(&tmp, CF_EFFECTS | CF_SCREENFONTS | CF_FORCEFONTEXIST);
	dlg.m_cf.rgbColors = SETTING(POPUP_TITLE_TEXTCOLOR);
	if (dlg.DoModal() == IDOK)
	{
		myFont = tmp;
		SET_SETTING(POPUP_TITLE_TEXTCOLOR, (int)dlg.GetColor());
		SET_SETTING(POPUP_TITLE_FONT, Text::fromT(WinUtil::encodeFont(myFont)));
	}
	return 0;
}

LRESULT Popups::onPopupBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring x;
	
	GET_TEXT(IDC_POPUPFILE, x);
	
	if (WinUtil::browseFile(x, m_hWnd, false) == IDOK)
	{
		SetDlgItemText(IDC_POPUPFILE, x.c_str());
		SET_SETTING(POPUPFILE, Text::fromT(x));
	}
	return 0;
}

LRESULT Popups::onTypeChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	SET_SETTING(POPUP_TYPE, ctrlPopupType.GetCurSel());
	if (ctrlPopupType.GetCurSel() == BALLOON)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_FONT), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_TITLE_FONT), false);
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), false);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_W), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_W_SPIN), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_H), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_H_SPIN), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_TRANSP_SLIDER), false);
	}
	else if (ctrlPopupType.GetCurSel() == CUSTOM)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), false);
		::EnableWindow(GetDlgItem(IDC_POPUP_FONT), true);
		::EnableWindow(GetDlgItem(IDC_POPUP_TITLE_FONT), true);
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), true);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), true);
	}
	else
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_BACKCOLOR), true);
		::EnableWindow(GetDlgItem(IDC_POPUP_FONT), true);
		::EnableWindow(GetDlgItem(IDC_POPUP_TITLE_FONT), true);
		::EnableWindow(GetDlgItem(IDC_POPUPFILE), false);
		::EnableWindow(GetDlgItem(IDC_POPUPBROWSE), false);
	}
	
	if (ctrlPopupType.GetCurSel() != BALLOON)
	{
		::EnableWindow(GetDlgItem(IDC_POPUP_W), true);
		::EnableWindow(GetDlgItem(IDC_POPUP_W_SPIN), true);
		::EnableWindow(GetDlgItem(IDC_POPUP_H), true);
		::EnableWindow(GetDlgItem(IDC_POPUP_H_SPIN), true);
		::EnableWindow(GetDlgItem(IDC_POPUP_TRANSP_SLIDER), true);
	}
	return 0;
}

LRESULT Popups::onFixControls(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	fixControls();
	return 0;
}

void Popups::write()
{
	PropPage::write(*this, items, listItems, GetDlgItem(IDC_POPUPLIST));
	
	SET_SETTING(POPUP_TYPE, ctrlPopupType.GetCurSel());
	SET_SETTING(POPUPS_DISABLED, IsDlgButtonChecked(IDC_POPUP_ENABLE) == 1 ? false : true);
	SET_SETTING(POPUPS_TABS_ENABLED, IsDlgButtonChecked(IDC_POPUP_TABS_ENABLE) == 1 ? true : false);
	SET_SETTING(POPUPS_MESSAGEPANEL_ENABLED, IsDlgButtonChecked(IDC_POPUP_MESSAGEPANEL_ENABLE) == 1 ? true : false);
	SET_SETTING(SHOW_INFOTIPS, IsDlgButtonChecked(IDC_POPUP_SHOW_INFO_TIPS_ENABLE) == 1 ? true : false);
	SET_SETTING(POPUP_AWAY, IsDlgButtonChecked(IDC_POPUP_AWAY) == 1 ? true : false);
	SET_SETTING(POPUP_MINIMIZED, IsDlgButtonChecked(IDC_POPUP_MINIMIZED) == 1 ? true : false);
	SET_SETTING(POPUP_TRANSP, slider.GetPos());
	// Do specialized writing here
	// settings->set(XX, YY);
}

void Popups::fixControls()
{
	bool state = (IsDlgButtonChecked(IDC_POPUP_ENABLE) != 0);
	::EnableWindow(GetDlgItem(IDC_POPUPLIST), state);
	::EnableWindow(GetDlgItem(IDC_POPUP_TABS_ENABLE), state);
	::EnableWindow(GetDlgItem(IDC_POPUP_MESSAGEPANEL_ENABLE), state);
	::EnableWindow(GetDlgItem(IDC_POPUP_SHOW_INFO_TIPS_ENABLE), state);
	::EnableWindow(GetDlgItem(IDC_POPUP_MINIMIZED), state);
	::EnableWindow(GetDlgItem(IDC_POPUP_AWAY), state);
}

LRESULT Popups::onPreview(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int PopupW = SETTING(POPUP_W);
	int PopupH = SETTING(POPUP_H);
	int PopupTransp = SETTING(POPUP_TRANSP);
	tstring buf;
	GET_TEXT(IDC_POPUP_W, buf);
	SET_SETTING(POPUP_W, Text::fromT(buf));
	GET_TEXT(IDC_POPUP_H, buf);
	SET_SETTING(POPUP_H, Text::fromT(buf));
	SET_SETTING(POPUP_TRANSP, slider.GetPos());      //set from TrackBar position
	SET_SETTING(POPUP_TYPE, ctrlPopupType.GetCurSel());
	
	PopupManager::getInstance()->Show(TSTRING(FILE) + _T(": FlylinkDC++.7z\n") +
	                                  TSTRING(USER) + _T(": ") + Text::toT(SETTING(NICK)), TSTRING(DOWNLOAD_FINISHED_IDLE), NIIF_INFO, true);
	                                  
	SET_SETTING(POPUP_W, PopupW);
	SET_SETTING(POPUP_H, PopupH);
	SET_SETTING(POPUP_TRANSP, PopupTransp);
	return 0;
}
/**
 * @file
 * $Id: Popups.cpp,v 1.7 2006/07/29 22:51:53 bigmuscle Exp $
 */

