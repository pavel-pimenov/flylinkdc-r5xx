/*
 * Copyright (C) 2010-2017 FlylinkDC++ Team http://flylinkdc.com
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
#include "../client/Text.h"

#include "CheckTargetDlg.h"

LRESULT CheckTargetDlg::onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{

	mRenameName = Text::toT(Util::getFileName(Util::getFilenameForRenaming(Text::fromT(mFileName))));
	
	SetWindowText(CTSTRING(REPLACE_DLG_TITLE));
	AutoArray<TCHAR> buf(512);
	_snwprintf(buf.data(), 512, CTSTRING(REPLACE_DESCR), mFileName.c_str());
	mFileName = Util::getFileName(mFileName);
	SetDlgItemText(IDC_REPLACE_DESCR, buf.data());
	SetDlgItemText(IDC_REPLACE_BORDER_EXISTS, CTSTRING(REPLACE_BORDER_EXISTS));
	SetDlgItemText(IDC_REPLACE_NAME_EXISTS, CTSTRING(MAGNET_DLG_FILE));
	SetDlgItemText(IDC_REPLACE_SIZE_EXISTS, CTSTRING(MAGNET_DLG_SIZE));
	SetDlgItemText(IDC_REPLACE_DATE_EXISTS, CTSTRING(REPLACE_DATE_EXISTS));
	SetDlgItemText(IDC_REPLACE_BORDER_NEW, CTSTRING(REPLACE_BORDER_NEW));
	SetDlgItemText(IDC_REPLACE_NAME_NEW, CTSTRING(MAGNET_DLG_FILE));
	SetDlgItemText(IDC_REPLACE_SIZE_NEW, CTSTRING(MAGNET_DLG_SIZE));
	SetDlgItemText(IDC_REPLACE_REPLACE, CTSTRING(REPLACE_REPLACE));
	_snwprintf(buf.data(), 512, CTSTRING(REPLACE_RENAME), mRenameName.c_str());
	SetDlgItemText(IDC_REPLACE_RENAME, buf.data());
	SetDlgItemText(IDC_REPLACE_SKIP, CTSTRING(REPLACE_SKIP));
	SetDlgItemText(IDC_REPLACE_APPLY, CTSTRING(REPLACE_APPLY));
	SetDlgItemText(IDC_REPLACE_CHANGE_NAME, CTSTRING(MAGNET_DLG_SAVEAS));
	
	CenterWindow(GetParent());
	
	SetDlgItemText(IDC_REPLACE_DISP_NAME_EXISTS, mFileName.c_str());
	
	wstring strSize = Util::toStringW(mSizeExist) + L" (";
	strSize += Util::formatBytesW(mSizeExist);
	strSize += L')';
	
	SetDlgItemText(IDC_REPLACE_DISP_SIZE_EXISTS, strSize.c_str());
	SetDlgItemText(IDC_REPLACE_DISP_DATE_EXISTS, Text::toT(Util::formatDigitalClock(mTimeExist)).c_str());
	
	SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, mFileName.c_str());
	
	wstring strSizeNew = Util::toStringW(mSizeNew) + L" (";
	strSizeNew += Util::formatBytesW(mSizeNew);
	strSizeNew += L')';
	SetDlgItemText(IDC_REPLACE_DISP_SIZE_NEW, strSizeNew.c_str());
	
	if (BOOLSETTING(KEEP_FINISHED_FILES_OPTION))
	{
		GetDlgItem(IDC_REPLACE_REPLACE).EnableWindow(FALSE);
		if (mOption == SettingsManager::ON_DOWNLOAD_REPLACE)
			mOption = SettingsManager::ON_DOWNLOAD_RENAME;
	}
	
	switch (mOption)
	{
		case SettingsManager::ON_DOWNLOAD_REPLACE:
		{
			CheckDlgButton(IDC_REPLACE_REPLACE, BST_CHECKED);
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, mFileName.c_str());
		}
		break;
		case SettingsManager::ON_DOWNLOAD_SKIP:
		{
			CheckDlgButton(IDC_REPLACE_SKIP, BST_CHECKED);
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, L"");
		}
		break;
		case SettingsManager::ON_DOWNLOAD_RENAME:
		default:
		{
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, mRenameName.c_str());
			CheckDlgButton(IDC_REPLACE_RENAME, BST_CHECKED);
		}
		break;
	}
	
	return 0;
}


LRESULT CheckTargetDlg::onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (wID == IDOK)
	{
		// Save Filename if REPLACE, Option, Apply
		if (IsDlgButtonChecked(IDC_REPLACE_REPLACE))
		{
			mOption = SettingsManager::ON_DOWNLOAD_REPLACE;
		}
		else if (IsDlgButtonChecked(IDC_REPLACE_RENAME))
		{
			mOption = SettingsManager::ON_DOWNLOAD_RENAME;
			//mFileName = mRenameName; // for safest: call Util::getFilenameForRenaming twice from gui and core.
		}
		else if (IsDlgButtonChecked(IDC_REPLACE_SKIP))
		{
			mOption = SettingsManager::ON_DOWNLOAD_SKIP;
		}
		else
			mOption = SettingsManager::ON_DOWNLOAD_ASK;
			
		mApply = IsDlgButtonChecked(IDC_REPLACE_APPLY) == TRUE;
		
	}
	else if (wID == IDCANCEL)
	{
		mOption = SettingsManager::ON_DOWNLOAD_SKIP;
	}
	EndDialog(wID);
	return 0;
}

LRESULT CheckTargetDlg::onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_REPLACE_REPLACE:
		{
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, mFileName.c_str());
		}
		break;
		case IDC_REPLACE_RENAME:
		{
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, mRenameName.c_str());
		}
		break;
		case IDC_REPLACE_SKIP:
		{
			SetDlgItemText(IDC_REPLACE_DISP_NAME_NEW, _T(""));
		}
		break;
	};
	
	/*
	::EnableWindow(GetDlgItem(IDC_REPLACE_CHANGE_NAME), IsDlgButtonChecked(IDC_REPLACE_RENAME) == BST_CHECKED);
	::EnableWindow(GetDlgItem(IDC_REPLACE_APPLY), IsDlgButtonChecked(IDC_REPLACE_RENAME) != BST_CHECKED);
	*/
	return 0;
}


LRESULT CheckTargetDlg::onChangeName(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	/*
	tstring dst = mFileName;
	if (!WinUtil::browseFile(dst, m_hWnd, true)) return 0;
	mFileName = dst;
	SetDlgItemText(IDC_MAGNET_DISP_NAME, dst.c_str());
	*/
	return 0;
}

