#include "stdafx.h"

#include "Resource.h"
#include "UserListColours.h"
#include "WinUtil.h"
#include "HubFrame.h" // [+] InfinitySky.

PropPage::TextItem UserListColours::texts[] =
{
	{ IDC_STATIC_ULC, ResourceManager::SETTINGS_USER_LIST },
	{ IDC_CHANGE_COLOR, ResourceManager::SETTINGS_CHANGE },
	{ IDC_USERLIST, ResourceManager::USERLIST_ICONS },
	//{ IDC_IMAGEBROWSE, ResourceManager::BROWSE }, // [~] JhaoDa, not necessary any more
	{ IDC_HUB_POSITION_TEXT, ResourceManager::HUB_USERS_POSITION_TEXT },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

PropPage::Item UserListColours::items[] =
{
	{ IDC_USERLIST_IMAGE, SettingsManager::USERLIST_IMAGE, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

LRESULT UserListColours::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	
	normalColour = SETTING(NORMAL_COLOUR);
	favoriteColour = SETTING(FAVORITE_COLOR);
	favEnemyColour = SETTING(TEXT_ENEMY_FORE_COLOR);
	reservedSlotColour = SETTING(RESERVED_SLOT_COLOR);
	ignoredColour = SETTING(IGNORED_COLOR);
	fastColour = SETTING(FIREBALL_COLOR);
	serverColour = SETTING(SERVER_COLOR);
	pasiveColour = SETTING(PASIVE_COLOR);
	opColour = SETTING(OP_COLOR);
	fullCheckedColour = SETTING(FULL_CHECKED_COLOUR);
	badClientColour = SETTING(BAD_CLIENT_COLOUR);
	badFilelistColour = SETTING(BAD_FILELIST_COLOUR);
	
	n_lsbList.Attach(GetDlgItem(IDC_USERLIST_COLORS));
	n_Preview.Attach(GetDlgItem(IDC_PREVIEW));
	n_Preview.SetBackgroundColor(Colors::g_bgColor);
	n_lsbList.AddString(CTSTRING(SETTINGS_COLOR_NORMAL));
	n_lsbList.AddString(CTSTRING(SETTINGS_COLOR_FAVORITE));
	n_lsbList.AddString(CTSTRING(FAV_ENEMY_USER));
	n_lsbList.AddString(CTSTRING(SETTINGS_COLOR_RESERVED));
	n_lsbList.AddString(CTSTRING(SETTINGS_COLOR_IGNORED));
	n_lsbList.AddString(CTSTRING(COLOR_FAST));
	n_lsbList.AddString(CTSTRING(COLOR_SERVER));
	n_lsbList.AddString(CTSTRING(COLOR_OP));
	n_lsbList.AddString(CTSTRING(COLOR_PASIVE));
	n_lsbList.AddString(CTSTRING(SETTINGS_COLOR_FULL_CHECKED));
	n_lsbList.AddString(CTSTRING(SETTINGS_COLOR_BAD_CLIENT));
	n_lsbList.AddString(CTSTRING(SETTINGS_COLOR_BAD_FILELIST));
	n_lsbList.SetCurSel(0);
	
	refreshPreview();
	
	// Расположение списка пользователей.
	
	CComboBox HubPosition;
	HubPosition.Attach(GetDlgItem(IDC_HUB_POSITION_COMBO));
	HubPosition.AddString(CTSTRING(TABS_LEFT)); // 0
	HubPosition.AddString(CTSTRING(TABS_RIGHT)); // 1
	HubPosition.SetCurSel(SETTING(HUB_POSITION));
	HubPosition.Detach();
	
	// for Custom Themes
	m_png_users.LoadFromResource(IDR_USERS, _T("PNG"));
	GetDlgItem(IDC_STATIC_USERLIST).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)m_png_users));
	SetDlgItemText(IDC_USERS_LINK, CTSTRING(USERLIST_ICONS_LINK));
	m_hlink_users.init(GetDlgItem(IDC_USERS_LINK), CTSTRING(USERLIST_ICONS_LINK));
	
	return TRUE;
}
LRESULT UserListColours::onLinkClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	WinUtil::openLink(WinUtil::GetWikiLink() + _T("usersbar"));
	return 0;
}
LRESULT UserListColours::onChangeColour(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
	int index = n_lsbList.GetCurSel();
	if (index == -1) return 0;
	int colour = 0;
	switch (index)
	{
		case 0:
			colour = normalColour;
			break;
		case 1:
			colour = favoriteColour;
			break;
		case 2:
			colour = favEnemyColour;
			break;
		case 3:
			colour = reservedSlotColour;
			break;
		case 4:
			colour = ignoredColour;
			break;
		case 5:
			colour = fastColour;
			break;
		case 6:
			colour = serverColour;
			break;
		case 7:
			colour = opColour;
			break;
		case 8:
			colour = pasiveColour;
			break;
		case 9:
			colour = fullCheckedColour;
			break;
		case 10:
			colour = badClientColour;
			break;
		case 11:
			colour = badFilelistColour;
			break;
		default:
			break;
	}
	CColorDialog d(colour, 0, hWndCtl);
	if (d.DoModal() == IDOK)
	{
		switch (index)
		{
			case 0:
				normalColour = d.GetColor();
				break;
			case 1:
				favoriteColour = d.GetColor();
				break;
			case 2:
				favEnemyColour = d.GetColor();
				break;
			case 3:
				reservedSlotColour = d.GetColor();
				break;
			case 4:
				ignoredColour = d.GetColor();
				break;
			case 5:
				fastColour = d.GetColor();
				break;
			case 6:
				serverColour = d.GetColor();
				break;
			case 7:
				opColour = d.GetColor();
				break;
			case 8:
				pasiveColour = d.GetColor();
				break;
			case 9:
				fullCheckedColour = d.GetColor();
				break;
			case 10:
				badClientColour = d.GetColor();
				break;
			case 11:
				badFilelistColour = d.GetColor();
				break;
			default:
				break;
		}
		refreshPreview();
	}
	return TRUE;
}

void UserListColours::refreshPreview()
{
	CHARFORMAT2 cf;
	n_Preview.SetWindowText(_T(""));
	cf.dwMask = CFM_COLOR;
	cf.dwEffects = 0;
	
	cf.crTextColor = normalColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText(TSTRING(SETTINGS_COLOR_NORMAL).c_str());
	
	cf.crTextColor = favoriteColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(SETTINGS_COLOR_FAVORITE)).c_str());
	
	cf.crTextColor = favEnemyColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(FAV_ENEMY_USER)).c_str());
	
	cf.crTextColor = reservedSlotColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(SETTINGS_COLOR_RESERVED)).c_str());
	
	cf.crTextColor = ignoredColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(SETTINGS_COLOR_IGNORED)).c_str());
	
	cf.crTextColor = fastColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(COLOR_FAST)).c_str());
	
	cf.crTextColor = serverColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(COLOR_SERVER)).c_str());
	
	cf.crTextColor = opColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(COLOR_OP)).c_str());
	
	cf.crTextColor = pasiveColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(COLOR_PASIVE)).c_str());
	
	cf.crTextColor = fullCheckedColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(SETTINGS_COLOR_FULL_CHECKED)).c_str());
	
	cf.crTextColor = badClientColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(SETTINGS_COLOR_BAD_CLIENT)).c_str());
	
	cf.crTextColor = badFilelistColour;
	n_Preview.SetSelectionCharFormat(cf);
	n_Preview.AppendText((_T("\r\n") + TSTRING(SETTINGS_COLOR_BAD_FILELIST)).c_str());
	
	n_Preview.InvalidateRect(NULL);
}

void UserListColours::write()
{
	PropPage::write((HWND)*this, items);
	SET_SETTING(NORMAL_COLOUR, normalColour);
	SET_SETTING(FAVORITE_COLOR, favoriteColour);
	SET_SETTING(TEXT_ENEMY_FORE_COLOR, favEnemyColour);
	SET_SETTING(RESERVED_SLOT_COLOR, reservedSlotColour);
	SET_SETTING(IGNORED_COLOR, ignoredColour);
	SET_SETTING(FIREBALL_COLOR, fastColour);
	SET_SETTING(SERVER_COLOR, serverColour);
	SET_SETTING(PASIVE_COLOR, pasiveColour);
	SET_SETTING(OP_COLOR, opColour);
	SET_SETTING(FULL_CHECKED_COLOUR, fullCheckedColour);
	SET_SETTING(BAD_CLIENT_COLOUR, badClientColour);
	SET_SETTING(BAD_FILELIST_COLOUR, badFilelistColour);
	
	g_userImage.reinit(); // User Icon Begin / End
	
	CComboBox HubPosition;
	HubPosition.Attach(GetDlgItem(IDC_HUB_POSITION_COMBO));
	g_settings->set(SettingsManager::HUB_POSITION, HubPosition.GetCurSel()); // Меняем настройку.
	//HubFrame::UpdateLayout(); // Обновляем отображение. Нужно как-то вызвать эту функцию.
	HubPosition.Detach();
	
}

void UserListColours::BrowseForPic(int DLGITEM) // TODO: copy-past, move to WinUtil.
{
	tstring x;
	
	GET_TEXT(DLGITEM, x);
	
	if (WinUtil::browseFile(x, m_hWnd, false) == IDOK)
	{
		SetDlgItemText(DLGITEM, x.c_str());
	}
}

LRESULT UserListColours::onImageBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	BrowseForPic(IDC_USERLIST_IMAGE);
	return 0;
}
/**
 * @file
 * $Id: userlistcolours.cpp,v 1.6 2006/10/22 18:57:56 bigmuscle Exp $
 */