/*
 * Copyright (C) 2001-2003 Jacek Sieka, j_s@telia.com
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

#include "OperaColorsPage.h"
#include "WinUtil.h"
#include "PropertiesDlg.h"

#include "BarShader.h"

PropPage::TextItem OperaColorsPage::texts[] =
{
	{ IDC_ODC_STYLE, ResourceManager::PROGRESSBAR_ODC_STYLE },
	{ IDC_STEALTHY_STYLE_ICO, ResourceManager::PROGRESSBAR_STEALTHY_STYLE_ICO },
	{ IDC_STEALTHY_STYLE_ICO_SPEEDIGNORE, ResourceManager::PROGRESSBAR_STEALTHY_STYLE_ICO_SPEEDIGNORE },
	{ IDC_PROGRESS_BUMPED, ResourceManager::BUMPED },
	{ IDC_PROGRESS_OVERRIDE, ResourceManager::SETTINGS_ZDC_PROGRESS_OVERRIDE },
	{ IDC_PROGRESS_OVERRIDE2, ResourceManager::SETTINGS_ZDC_PROGRESS_OVERRIDE2 },
	{ IDC_SETTINGS_DOWNLOAD_BAR_COLOR, ResourceManager::DOWNLOAD },
	{ IDC_PROGRESS_TEXT_COLOR_DOWN, ResourceManager::DOWNLOAD },
	{ IDC_SETTINGS_UPLOAD_BAR_COLOR, ResourceManager::UPLOAD },
	{ IDC_PROGRESS_TEXT_COLOR_UP, ResourceManager::UPLOAD },
	{ IDC_DEPTH_STR, ResourceManager::DEPTH_STR },
	{ IDC_SETTINGS_ODC_MENUBAR, ResourceManager::MENUBAR },
	{ IDC_SETTINGS_ODC_MENUBAR_LEFT, ResourceManager::LEFT_COLOR },
	{ IDC_SETTINGS_ODC_MENUBAR_RIGHT, ResourceManager::RIGHT_COLOR },
	{ IDC_SETTINGS_ODC_MENUBAR_USETWO, ResourceManager::TWO_COLORS },
	{ IDC_SETTINGS_ODC_MENUBAR_BUMPED, ResourceManager::BUMPED },
	{ IDC_CZDC_PROGRESS_COLOR, ResourceManager::SETCZDC_PROGRESSBAR_COLORS },
	{ IDC_CZDC_PROGRESS_TEXT, ResourceManager::SETCZDC_PROGRESSBAR_TEXT },
	{ IDC_SETTINGS_ODC_MENUBAR2, ResourceManager::SETCZDC_PROGRESSBAR_COLORS },
	{ IDC_KBPS, ResourceManager::KBPS },
	{ IDC_KBPS2, ResourceManager::KBPS },
	{ IDC_SPEED_STR, ResourceManager::TICKS_SPEED_DOWN },
	{ IDC_SPEED_UP_STR, ResourceManager::TICKS_SPEED_UP },
	{ IDC_MISC_GP, ResourceManager::SETTINGS_MISC },
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

#define CTLID_VALUE_RED 0x2C2
#define CTLID_VALUE_BLUE 0x2C3
#define CTLID_VALUE_GREEN 0x2C4

OperaColorsPage* current_page;
LPCCHOOKPROC color_proc;

PropPage::Item OperaColorsPage::items[] =
{
	{ IDC_ODC_STYLE, SettingsManager::PROGRESSBAR_ODC_STYLE, PropPage::T_BOOL },
	{ IDC_STEALTHY_STYLE_ICO, SettingsManager::STEALTHY_STYLE_ICO, PropPage::T_BOOL },
	{ IDC_STEALTHY_STYLE_ICO_SPEEDIGNORE, SettingsManager::STEALTHY_STYLE_ICO_SPEEDIGNORE, PropPage::T_BOOL },
	{ IDC_PROGRESS_BUMPED, SettingsManager::PROGRESSBAR_ODC_BUMPED, PropPage::T_BOOL },
	{ IDC_PROGRESS_OVERRIDE, SettingsManager::PROGRESS_OVERRIDE_COLORS, PropPage::T_BOOL },
	{ IDC_PROGRESS_OVERRIDE2, SettingsManager::PROGRESS_OVERRIDE_COLORS2, PropPage::T_BOOL },
	{ IDC_FLAT, SettingsManager::PROGRESS_3DDEPTH, PropPage::T_INT },
	{ IDC_TOP_SPEED, SettingsManager::TOP_SPEED, PropPage::T_INT },
	{ IDC_TOP_UP_SPEED, SettingsManager::TOP_UP_SPEED, PropPage::T_INT },
	//{ IDC_BACKGROUND_IMAGE, SettingsManager::BACKGROUND_IMAGE, PropPage::T_STR },
	{ 0, 0, PropPage::T_END }
};

PropPage::ListItem OperaColorsPage::listItems[] =
{
	{ SettingsManager::SHOW_PROGRESS_BARS, ResourceManager::SETTINGS_SHOW_PROGRESS_BARS },
	{ SettingsManager::UP_TRANSFER_COLORS, ResourceManager::UP_TRANSFER_COLORS }, // [+] Drakon.
	{ 0, ResourceManager::SETTINGS_AUTO_AWAY }
};

UINT_PTR CALLBACK MenuBarCommDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	if (uMsg == WM_COMMAND)
	{
		if (HIWORD(wParam) == EN_CHANGE &&
		        (LOWORD(wParam) == CTLID_VALUE_RED || LOWORD(wParam) == CTLID_VALUE_GREEN || LOWORD(wParam) == CTLID_VALUE_BLUE))
		{
			LocalArray<TCHAR, 16> buf;
			buf[0] = 0;
			::GetDlgItemText(hWnd, CTLID_VALUE_RED, buf.data(), 15);
			int color_r = Util::toInt(buf.data());
			buf[0] = 0;
			::GetDlgItemText(hWnd, CTLID_VALUE_BLUE, buf.data(), 15);
			int color_g = Util::toInt(buf.data());
			buf[0] = 0;
			::GetDlgItemText(hWnd, CTLID_VALUE_GREEN, buf.data(), 15);
			int color_b = Util::toInt(buf.data());
			
			color_r = (color_r < 0) ? 0 : ((color_r > 255) ? 255 : color_r);
			color_g = (color_g < 0) ? 0 : ((color_g > 255) ? 255 : color_g);
			color_b = (color_b < 0) ? 0 : ((color_b > 255) ? 255 : color_b);
			
			if (current_page->bDoProgress)
			{
				if (current_page->bDoLeft)
				{
					current_page->crProgressDown = RGB(color_r, color_g, color_b);
					current_page->ctrlProgressDownDrawer.Invalidate();
				}
				else if (current_page->bDoSegment)
				{
					current_page->crProgressSegment = RGB(color_r, color_g, color_b);
				}
				else
				{
					current_page->crProgressUp = RGB(color_r, color_g, color_b);
					current_page->ctrlProgressUpDrawer.Invalidate();
				}
			}
			else
			{
				if (current_page->bDoLeft)
					current_page->crMenubarLeft = RGB(color_r, color_g, color_b);
				else
					current_page->crMenubarRight = RGB(color_r, color_g, color_b);
				current_page->ctrlMenubarDrawer.Invalidate();
			}
		}
	}
	
	return (*color_proc)(hWnd, uMsg, wParam, lParam);
	
}
LRESULT OperaColorsPage::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	PropPage::translate((HWND)(*this), texts);
	PropPage::read((HWND)*this, items);
	PropPage::read((HWND)*this, items, listItems, GetDlgItem(IDC_OPERACOLORS_BOOLEANS));
	
	ctrlList.Attach(GetDlgItem(IDC_OPERACOLORS_BOOLEANS));
	
	crProgressDown = SETTING(DOWNLOAD_BAR_COLOR);
	crProgressUp = SETTING(UPLOAD_BAR_COLOR);
	crProgressTextDown = SETTING(PROGRESS_TEXT_COLOR_DOWN);
	crProgressTextUp = SETTING(PROGRESS_TEXT_COLOR_UP);
	
	ctrlProgressDownDrawer.Attach(GetDlgItem(IDC_PROGRESS_COLOR_DOWN_SHOW));
	ctrlProgressUpDrawer.Attach(GetDlgItem(IDC_PROGRESS_COLOR_UP_SHOW));
	
	CUpDownCtrl ctrlSpin;
	ctrlSpin.Attach(GetDlgItem(IDC_FLAT_SPIN));
	ctrlSpin.SetRange(1, 5);
	ctrlSpin.Detach();
	
	updateProgress();
	
	crMenubarLeft = SETTING(MENUBAR_LEFT_COLOR);
	crMenubarRight = SETTING(MENUBAR_RIGHT_COLOR);
	ctrlLeftColor.Attach(GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_LEFT));
	ctrlRightColor.Attach(GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_RIGHT));
	ctrlTwoColors.Attach(GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_USETWO));
	ctrlBumped.Attach(GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_BUMPED));
	ctrlMenubarDrawer.Attach(GetDlgItem(IDC_SETTINGS_ODC_MENUBAR_COLOR));
	
	checkBox(IDC_SETTINGS_ODC_MENUBAR_BUMPED, BOOLSETTING(MENUBAR_BUMPED));
	checkBox(IDC_SETTINGS_ODC_MENUBAR_USETWO, BOOLSETTING(MENUBAR_TWO_COLORS));
	BOOL b;
	onMenubarClicked(0, IDC_SETTINGS_ODC_MENUBAR_USETWO, 0, b);
	// Do specialized reading here
	
	return TRUE;
}

void OperaColorsPage::write()
{
	PropPage::write((HWND)*this, items, listItems, GetDlgItem(IDC_OPERACOLORS_BOOLEANS));
	PropPage::write((HWND)*this, items);
	
	// Do specialized writing here
	// settings->set(XX, YY);
	SET_SETTING(MENUBAR_LEFT_COLOR, (int)crMenubarLeft);
	SET_SETTING(MENUBAR_RIGHT_COLOR, (int)crMenubarRight);
	SET_SETTING(MENUBAR_BUMPED, getCheckbox(IDC_SETTINGS_ODC_MENUBAR_BUMPED));
	SET_SETTING(MENUBAR_TWO_COLORS, getCheckbox(IDC_SETTINGS_ODC_MENUBAR_USETWO));
	
	SET_SETTING(DOWNLOAD_BAR_COLOR, (int)crProgressDown);
	SET_SETTING(UPLOAD_BAR_COLOR, (int)crProgressUp);
	SET_SETTING(PROGRESS_TEXT_COLOR_DOWN, (int)crProgressTextDown);
	SET_SETTING(PROGRESS_TEXT_COLOR_UP, (int)crProgressTextUp);
	
	OperaColors::ClearCache();
}

LRESULT OperaColorsPage::onDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
{
	if (PropertiesDlg::g_needUpdate)
	{
		SendMessage(WM_DESTROY, 0, 0);
		SendMessage(WM_INITDIALOG, 0, 0);
		PropertiesDlg::g_needUpdate = false;
	}
	bHandled = FALSE;
	DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
	if (dis->CtlType == ODT_STATIC)
	{
		bHandled = TRUE;
		CDC dc;
		dc.Attach(dis->hDC);
		CRect rc(dis->rcItem);
		if (dis->CtlID == IDC_SETTINGS_ODC_MENUBAR_COLOR)
		{
			if (getCheckbox(IDC_SETTINGS_ODC_MENUBAR_USETWO))
				OperaColors::FloodFill(dc, rc.left, rc.top, rc.right, rc.bottom, crMenubarLeft, crMenubarRight, getCheckbox(IDC_SETTINGS_ODC_MENUBAR_BUMPED));
			else
				dc.FillSolidRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, crMenubarLeft);
			dc.SetTextColor(OperaColors::TextFromBackground(crMenubarLeft));
		}
		else if (dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW || dis->CtlID == IDC_PROGRESS_COLOR_UP_SHOW)
		{
			COLORREF clr = getCheckbox(IDC_PROGRESS_OVERRIDE) ? ((dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW) ? crProgressDown : crProgressUp) : GetSysColor(COLOR_HIGHLIGHT);
			int textcolor;
			if (odcStyle)
			{
				COLORREF a, b;
				OperaColors::EnlightenFlood(clr, a, b);
				OperaColors::FloodFill(dc, rc.left, rc.top, rc.right, rc.bottom, a, b, getCheckbox(IDC_PROGRESS_BUMPED));
			}
			else   // not stealthyStyle or odcStyle
			{
				CBarShader statusBar(rc.bottom - rc.top, rc.right - rc.left);
				statusBar.SetFileSize(16);
				statusBar.FillRange(0, 16, clr);
				statusBar.Draw(dc, rc.top, rc.left, hloubka);
			}
			textcolor = getCheckbox(IDC_PROGRESS_OVERRIDE2) ? ((dis->CtlID == IDC_PROGRESS_COLOR_DOWN_SHOW) ? crProgressTextDown : crProgressTextUp) : OperaColors::TextFromBackground(clr);
			dc.SetTextColor(textcolor);
		}
		const wstring l_text = TSTRING(SETTINGS_MENUHEADER_EXAMPLE);
		dc.DrawText(l_text.c_str(), l_text.length(), rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
		dc.Detach();
	} // if (dis->CtlType == ODT_STATIC) {
	return S_OK;
}

LRESULT OperaColorsPage::onMenubarClicked(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDC_SETTINGS_ODC_MENUBAR_LEFT)
	{
		CColorDialog d(crMenubarLeft, CC_FULLOPEN, *this);
		color_proc = d.m_cc.lpfnHook;
		d.m_cc.lpfnHook = MenuBarCommDlgProc;
		current_page = this;//d.m_cc.lCustData = (LPARAM)this;
		bDoProgress = false;
		bDoLeft = true;
		COLORREF backup = crMenubarLeft;
		if (d.DoModal() == IDOK)
			crMenubarLeft = d.GetColor();
		else
			crMenubarLeft = backup;
	}
	else if (wID == IDC_SETTINGS_ODC_MENUBAR_RIGHT)
	{
		CColorDialog d(crMenubarRight, CC_FULLOPEN, *this);
		color_proc = d.m_cc.lpfnHook;
		d.m_cc.lpfnHook = MenuBarCommDlgProc;
		current_page = this;//d.m_cc.lCustData = (LPARAM)this;
		bDoProgress = false;
		bDoLeft = false;
		COLORREF backup = crMenubarRight;
		if (d.DoModal() == IDOK)
			crMenubarRight = d.GetColor();
		else
			crMenubarRight = backup;
	}
	else if (wID == IDC_SETTINGS_ODC_MENUBAR_USETWO)
	{
		bool b = getCheckbox(IDC_SETTINGS_ODC_MENUBAR_USETWO);
		ctrlRightColor.EnableWindow(b);
		ctrlBumped.EnableWindow(b);
	}
	else if (wID == IDC_SETTINGS_ODC_MENUBAR_BUMPED)
	{
	}
	ctrlMenubarDrawer.Invalidate();
	return S_OK;
}
LRESULT OperaColorsPage::onClickedProgress(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	odcStyle = IsDlgButtonChecked(IDC_ODC_STYLE) == 1;
	stealthyStyleIco = IsDlgButtonChecked(IDC_STEALTHY_STYLE_ICO) == 1;
	stealthyStyleIcoSpeedIgnore = IsDlgButtonChecked(IDC_STEALTHY_STYLE_ICO_SPEEDIGNORE) == 1;
	updateProgress(false);
	if (wID == IDC_SETTINGS_DOWNLOAD_BAR_COLOR)
	{
		CColorDialog d(crProgressDown, CC_FULLOPEN, *this);
		color_proc = d.m_cc.lpfnHook;
		d.m_cc.lpfnHook = MenuBarCommDlgProc;
		current_page = this;//d.m_cc.lCustData = (LPARAM)this;
		bDoProgress = true;
		bDoLeft = true;
		bDoSegment = false;
		COLORREF backup = crProgressDown;
		if (d.DoModal() == IDOK)
			crProgressDown = d.GetColor();
		else
			crProgressDown = backup;
		ctrlProgressDownDrawer.Invalidate();
		//ctrlProgressSegmentDrawer.Invalidate();
	}
	else if (wID == IDC_SETTINGS_UPLOAD_BAR_COLOR)
	{
		CColorDialog d(crProgressUp, CC_FULLOPEN, *this);
		color_proc = d.m_cc.lpfnHook;
		d.m_cc.lpfnHook = MenuBarCommDlgProc;
		current_page = this;//d.m_cc.lCustData = (LPARAM)this;
		bDoProgress = true;
		bDoLeft = false;
		bDoSegment = false;
		COLORREF backup = crProgressUp;
		if (d.DoModal() == IDOK)
			crProgressUp = d.GetColor();
		else
			crProgressUp = backup;
		ctrlProgressUpDrawer.Invalidate();
	}
	else
	{
		ctrlProgressDownDrawer.Invalidate();
		ctrlProgressUpDrawer.Invalidate();
	}
	return TRUE;
}

LRESULT OperaColorsPage::onClickedProgressTextDown(WORD /* wNotifyCode */, WORD /* wID */, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	CColorDialog d(crProgressTextDown, 0, *this);
	if (d.DoModal() == IDOK)
	{
		crProgressTextDown = d.GetColor();
	}
	ctrlProgressDownDrawer.Invalidate();
	return TRUE;
}

LRESULT OperaColorsPage::onClickedProgressTextUp(WORD /* wNotifyCode */, WORD /* wID */, HWND /* hWndCtl */, BOOL& /* bHandled */)
{
	CColorDialog d(crProgressTextUp, 0, *this);
	if (d.DoModal() == IDOK)
	{
		crProgressTextUp = d.GetColor();
	}
	ctrlProgressUpDrawer.Invalidate();
	return TRUE;
}

LRESULT OperaColorsPage::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (ctrlProgressDownDrawer.m_hWnd != NULL)
		ctrlProgressDownDrawer.Detach();
	if (ctrlProgressUpDrawer.m_hWnd != NULL)
		ctrlProgressUpDrawer.Detach();
	if (ctrlLeftColor.m_hWnd != NULL)
		ctrlLeftColor.Detach();
	if (ctrlRightColor.m_hWnd != NULL)
		ctrlRightColor.Detach();
	if (ctrlTwoColors.m_hWnd != NULL)
		ctrlTwoColors.Detach();
	if (ctrlBumped.m_hWnd != NULL)
		ctrlBumped.Detach();
	if (ctrlMenubarDrawer.m_hWnd != NULL)
		ctrlMenubarDrawer.Detach();
	return 1;
}
/**
 * @file
 * $Id: OperaColorsPage.cpp,v 1.23 2006/10/22 18:57:56 bigmuscle Exp $
 */

