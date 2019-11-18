/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#if !defined(SHARE_PAGE_H)
#define SHARE_PAGE_H

#pragma once


#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h"
#include "FolderTree.h"
#include "../client/SettingsManager.h"

class SharePage : public CPropertyPage<IDD_SHARE_PAGE>, public PropPage
{
	public:
		explicit SharePage() : PropPage(TSTRING(SETTINGS_UPLOADS))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~SharePage()
		{
			ctrlDirectories.Detach();
			ctrlTotal.Detach();
		}
		
		BEGIN_MSG_MAP_EX(SharePage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		MESSAGE_HANDLER(WM_DROPFILES, onDropFiles)
		NOTIFY_HANDLER(IDC_DIRECTORIES, LVN_ITEMCHANGED, onItemchangedDirectories)
		NOTIFY_HANDLER(IDC_DIRECTORIES, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_DBLCLK, onDoubleClick)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_CUSTOMDRAW, ctrlDirectories.onCustomDraw)
		COMMAND_ID_HANDLER(IDC_ADD, onClickedAdd)
		COMMAND_ID_HANDLER(IDC_REMOVE, onClickedRemove)
		COMMAND_ID_HANDLER(IDC_RENAME, onClickedRename)
		COMMAND_ID_HANDLER(IDC_SHAREHIDDEN, onClickedShareHidden)
		COMMAND_ID_HANDLER(IDC_SHARESYSTEM, onClickedShareSystem)
		COMMAND_ID_HANDLER(IDC_SHAREVIRTUAL, onClickedShareVirtual)
		CHAIN_MSG_MAP(PropPage)
		REFLECT_NOTIFICATIONS()
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDropFiles(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onItemchangedDirectories(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onClickedAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedShareHidden(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedShareSystem(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedShareVirtual(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedShare(int item);
		
		// Common PropPage interface
		PROPSHEETPAGE *getPSP()
		{
			return (PROPSHEETPAGE *) * this;
		}
		void write();
		void cancel()
		{
			cancel_check();
		}
	protected:
		static Item items[];
		static TextItem texts[];
		ExListViewCtrl ctrlDirectories;
		CStatic ctrlTotal;
		
		void addDirectory(const tstring& aPath);
		FolderTree ft;
	private:
		void directoryListInit();
};

#endif // !defined(SHARE_PAGE_H)

/**
 * @file
 * $Id: UploadPage.h,v 1.18 2006/06/15 20:14:15 bigmuscle Exp $
 * $Id: SharePage.h, 2013/07/19 FlylinkDC++ Team $
 */
