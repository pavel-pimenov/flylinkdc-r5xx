/*
 * Copyright (C) 2011-2013 FlylinkDC++ Team http://flylinkdc.com/
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

#ifndef _FLY_UPDATE_DLG_H_
#define _FLY_UPDATE_DLG_H_
#pragma once

#include "../client/Util.h"
#include "Resource.h"
#include "ResourceLoader.h"
#include "../FlyFeatures/AutoUpdate.h"
#include "ExListViewCtrl.h"

class FlyUpdateDlg : public CDialogImpl<FlyUpdateDlg>
{
	public:
		enum { IDD = IDD_UPDATE_DLG };
		
		BEGIN_MSG_MAP(FlyUpdateDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		NOTIFY_CODE_HANDLER(EN_LINK, OnLink)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_IGNOREUPDATE, OnCloseCmd)
		END_MSG_MAP()
		
		FlyUpdateDlg(const string& data/* = Util::emptyString*/, const string& rtfData/* = Util::emptyString*/, const AutoUpdateFiles& fileList);
		~FlyUpdateDlg();
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnLink(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
	private:
		HICON m_UpdateIcon;
		ExCImage img_f;
		std::string m_data;
		std::string m_rtfData;
		AutoUpdateFiles m_fileList;
		HMODULE m_richEditLibrary;
		
		ExListViewCtrl ctrlCommands;
};

#endif // _FLY_UPDATE_DLG_H_

/**
* @file
* $Id: FlyUpdateDlg.h 7011 2011-05-16 20:17:37Z sa.stolper $
*/

