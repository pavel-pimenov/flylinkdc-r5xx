/**
* Диалог "Добавить / Изменить Избранный Путь" / Dialog "Add / Change Favorite Directory".
*/

#if !defined(FAV_DIR_DLG_H)
#define FAV_DIR_DLG_H

#pragma once

#include <atlcrack.h>
#include "wtl_flylinkdc.h"
#include "../client/Util.h"

class FavDirDlg : public CDialogImpl<FavDirDlg>
{
	public:
		FavDirDlg() { }
		~FavDirDlg() { }
		
		tstring name;
		tstring dir;
		tstring extensions;
		
		enum { IDD = IDD_FAVORITEDIR };
		
		BEGIN_MSG_MAP_EX(FavDirDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_FAVDIR_BROWSE, OnBrowse);
		END_MSG_MAP()
		
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			SetWindowText(CTSTRING(SETTINGS_ADD_FAVORITE_DIRS));
			SetDlgItemText(IDC_FAV_NAME2, CTSTRING(SETTINGS_NAME2));
			SetDlgItemText(IDC_GET_FAVORITE_DIR, CTSTRING(SETTINGS_GET_FAVORITE_DIR));
			SetDlgItemText(IDC_FAVORITE_DIR_EXT, CTSTRING(SETTINGS_FAVORITE_DIR_EXT));
			//SetDlgItemText(IDC_FAVDIR_BROWSE, CTSTRING(BROWSE));// [~] JhaoDa, not necessary any more
			SetDlgItemText(IDCANCEL, CTSTRING(CANCEL));
			SetDlgItemText(IDOK, CTSTRING(OK));
			
			ATTACH(IDC_FAVDIR_NAME, ctrlName);
			ATTACH(IDC_FAVDIR, ctrlDirectory);
			ATTACH(IDC_FAVDIR_EXTENSION, ctrlExtensions);
			
			ctrlName.SetWindowText(name.c_str());
			ctrlDirectory.SetWindowText(dir.c_str());
			ctrlExtensions.SetWindowText(extensions.c_str());
			ctrlName.SetFocus();
			
			CenterWindow(GetParent());
			return 0;
		}
		
		LRESULT OnBrowse(UINT /*uMsg*/, WPARAM /*wParam*/, HWND /*lParam*/, BOOL& /*bHandled*/)
		{
			tstring target;
			if (!dir.empty())
				target = dir;
			if (WinUtil::browseDirectory(target, m_hWnd))
			{
				ctrlDirectory.SetWindowText(target.c_str());
				if (ctrlName.GetWindowTextLength() == 0)
					ctrlName.SetWindowText(Util::getLastDir(target).c_str());
			}
			return 0;
		}
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (wID == IDOK)
			{
				if ((ctrlName.GetWindowTextLength() == 0) || (ctrlDirectory.GetWindowTextLength() == 0))
				{
					MessageBox(CTSTRING(NAME_OR_DIR_NOT_EMPTY));
					return 0;
				}
				
				GET_TEXT(IDC_FAVDIR_NAME, name);
				GET_TEXT(IDC_FAVDIR, dir);
				GET_TEXT(IDC_FAVDIR_EXTENSION, extensions);
			}
			
			ctrlName.Detach();
			ctrlDirectory.Detach();
			ctrlExtensions.Detach();
			
			EndDialog(wID);
			return 0;
		}
		
	private:
		FavDirDlg(const FavDirDlg&)
		{
			dcassert(0);
		}
		CEdit ctrlName;
		CEdit ctrlDirectory;
		CEdit ctrlExtensions;
};

#endif // !defined(FAV_DIR_DLG_H)