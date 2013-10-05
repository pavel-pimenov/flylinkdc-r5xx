/*
 * Copyright (C) 2006-2009 Crise, crise@mail.berlios.de
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

#ifndef DETECTION_ENTRY_DLG
#define DETECTION_ENTRY_DLG

#pragma once

#include "../client/DCPlusPlus.h"
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER

#include "../client/Util.h"
#include "../client/DetectionManager.h"
#include "../client/version.h"

#include "ExListViewCtrl.h"

class DetectionEntryDlg : public CDialogImpl<DetectionEntryDlg>
{
	public:
		DetectionEntry& curEntry;
		
		enum { IDD = IDD_DETECTION_ENTRY };
		
		BEGIN_MSG_MAP(DetectionEntryDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		COMMAND_ID_HANDLER(IDC_ADD, onAdd)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_CHANGE, onChange)
		COMMAND_ID_HANDLER(IDC_ID_EDIT, onIdEdit)
		COMMAND_ID_HANDLER(IDC_NEXT, onNext)
		COMMAND_ID_HANDLER(IDC_BACK, onNext)
		COMMAND_ID_HANDLER(IDC_MATCH, onMatch)
		COMMAND_ID_HANDLER(IDC_ENABLE, onEnable)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_HANDLER(IDC_INFMAP_TYPE, LBN_SELCHANGE, onProtocolChange)
		NOTIFY_HANDLER(IDC_PARAMS, LVN_ITEMCHANGED, onItemchangedDirectories)
		END_MSG_MAP()
		
		DetectionEntryDlg(DetectionEntry& de) : curEntry(de), idChanged(false), origId(de.Id) { }
		
		~DetectionEntryDlg()
		{
			ctrlName.Detach();
			ctrlComment.Detach();
			ctrlLevel.Detach();
			ctrlCheat.Detach();
			ctrlRaw.Detach();
			ctrlParams.Detach();
			ctrlExpTest.Detach();
			ctrlProtocol.Detach();
			sharedMap.clear();
			nmdcMap.clear();
			adcMap.clear();
		}
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlName.SetFocus();
			return FALSE;
		}
		
		LRESULT onIdEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			idChanged = true;
			::EnableWindow(GetDlgItem(IDC_DETECT_ID), true);
			::EnableWindow(GetDlgItem(IDC_ID_EDIT), false);
			return 0;
		}
		
		LRESULT onProtocolChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			updateVars();
			ctrlParams.DeleteAllItems();
			updateControls();
			return S_OK;
		}
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onNext(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMatch(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEnable(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onItemchangedDirectories(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		uint32_t origId;
		
	private:
		CEdit ctrlName, ctrlComment, ctrlCheat, ctrlExpTest;
		CComboBox ctrlRaw;
		CComboBox ctrlLevel, ctrlProtocol;
		ExListViewCtrl ctrlParams;
		bool idChanged;
		
		DetectionEntry::INFMap sharedMap, nmdcMap, adcMap;
		
		void updateVars();
		void updateControls();
		
		tstring matchExp(const string& expression, const string& strToMatch)
		{
			try
			{
				const std::regex reg(expression);
				return std::regex_search(strToMatch.begin(), strToMatch.end(), reg) ? TSTRING(REGEXP_MATCH) : TSTRING(REGEXP_MISMATCH);
			}
			catch (const std::regex_error& e)
			{
				return TSTRING(REGEXP_INVALID) + _T(" ") + CTSTRING(ERROR_STRING) + _T(": ") + Text::toT(e.what());
			}
			return TSTRING(REGEXP_INVALID);
		}
};

#endif // IRAINMAN_INCLUDE_DETECTION_MANAGER

#endif // DETECTION_ENTRY_DLG
