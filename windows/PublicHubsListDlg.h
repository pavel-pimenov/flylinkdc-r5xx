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

#if !defined(PUBLIC_HUBS_LIST_DLG_H)
#define PUBLIC_HUBS_LIST_DLG_H

#pragma once

#include "../client/DCPlusPlus.h"
#include "../client/SettingsManager.h"
#include "../client/Text.h"
#include "../client/FavoriteManager.h"
#include "ExListViewCtrl.h"
#include "LineDlg.h"
#include "ResourceLoader.h"

class PublicHubListDlg : public CDialogImpl<PublicHubListDlg>
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		enum { IDD = IDD_HUB_LIST };
		
		PublicHubListDlg() { }
		~PublicHubListDlg()
		{
			ctrlList.Detach();
		}
		
		BEGIN_MSG_MAP(PublicHubListDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDC_LIST_ADD, onAdd);
		COMMAND_ID_HANDLER(IDC_LIST_UP, onMoveUp);
		COMMAND_ID_HANDLER(IDC_LIST_DOWN, onMoveDown);
		COMMAND_ID_HANDLER(IDC_LIST_EDIT, onEdit);
		COMMAND_ID_HANDLER(IDC_LIST_REMOVE, onRemove);
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		NOTIFY_HANDLER(IDC_HUBLIST, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
		NOTIFY_HANDLER(IDC_LIST_LIST, LVN_ITEMCHANGED, onItemchangedDirectories)
		END_MSG_MAP();
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			// set the title, center the window
			SetWindowText(CTSTRING(CONFIGURED_HUB_LISTS));
			CenterWindow(GetParent());
			
			// for Custom Themes Bitmap
			img_f.LoadFromResource(IDR_FLYLINK_PNG, _T("PNG"));
			GetDlgItem(IDC_STATIC).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)img_f));
			
			// fill in translatable strings
			SetDlgItemText(IDC_LIST_DESC, Util::emptyStringT.c_str());
			SetDlgItemText(IDC_LIST_ADD, CTSTRING(ADD));
			SetDlgItemText(IDC_LIST_UP, CTSTRING(MOVE_UP));
			SetDlgItemText(IDC_LIST_DOWN, CTSTRING(MOVE_DOWN));
			SetDlgItemText(IDC_LIST_EDIT, CTSTRING(EDIT_ACCEL));
			SetDlgItemText(IDC_LIST_REMOVE, CTSTRING(REMOVE));
			SetDlgItemText(IDCANCEL, CTSTRING(CANCEL));
			SetDlgItemText(IDOK, CTSTRING(OK));
			
			// set up the list of lists
			CRect rc;
			ctrlList.Attach(GetDlgItem(IDC_LIST_LIST));
			SET_EXTENDENT_LIST_VIEW_STYLE(ctrlList);
			SET_LIST_COLOR(ctrlList);
			ctrlList.GetClientRect(rc);
			ctrlList.InsertColumn(0, CTSTRING(SETTINGS_NAME), LVCFMT_LEFT, rc.Width() - 4, 0);
			const StringList lists = SPLIT_SETTING_AND_LOWER(HUBLIST_SERVERS);
			auto cnt = ctrlList.GetItemCount();
			for (auto idx = lists.cbegin(); idx != lists.cend(); ++idx)
			{
				ctrlList.insert(cnt++, Text::toT(*idx));
			}
			
			// set the initial focus
			CEdit focusThis;
			focusThis.Attach(GetDlgItem(IDC_LIST_EDIT_BOX));
			focusThis.SetFocus();
			
			return 0;
		}
		
		LRESULT onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
		{
			tstring buf;
			GET_TEXT(IDC_LIST_EDIT_BOX, buf);
			if (!buf.empty())
			{
				ctrlList.insert(0, buf);
				SetDlgItemText(IDC_LIST_EDIT_BOX, Util::emptyStringT.c_str());
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
		{
			const int j = ctrlList.GetItemCount();
			for (int i = 1; i < j; ++i)
			{
				if (ctrlList.GetItemState(i, LVIS_SELECTED))
				{
					ctrlList.moveItem(i, i - 1);
					ctrlList.SelectItem(i - 1);
					ctrlList.SetFocus();
				}
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
		{
			const int j = ctrlList.GetItemCount() - 2;
			for (int i = j; i >= 0; --i)
			{
				if (ctrlList.GetItemState(i, LVIS_SELECTED))
				{
					ctrlList.moveItem(i, i + 1);
					ctrlList.SelectItem(i + 1);
					ctrlList.SetFocus();
				}
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
		{
			int i = -1;
			while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				LineDlg hublist;
				hublist.title = TSTRING(HUB_LIST);
				hublist.description = TSTRING(HUB_LIST_EDIT);
				hublist.line = ctrlList.ExGetItemTextT(i, 0);
				if (hublist.DoModal(m_hWnd) == IDOK)
				{
					ctrlList.SetItemText(i, 0, hublist.line.c_str());
				}
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
		{
			int i = -1;
			while ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				ctrlList.DeleteItem(i);
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (wID == IDOK)
			{
				string tmp;
				const int j = ctrlList.GetItemCount();
				for (int i = 0; i < j; ++i)
				{
					tmp += ctrlList.ExGetItemText(i, 0) + ';';
				}
				if (j > 0)
				{
					tmp.erase(tmp.size() - 1);
				}
				SET_SETTING(HUBLIST_SERVERS, tmp);
			}
			EndDialog(wID);
			return 0;
		}
		
		LRESULT onItemchangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
			::EnableWindow(GetDlgItem(IDC_LIST_UP), (lv->uNewState & LVIS_FOCUSED));
			::EnableWindow(GetDlgItem(IDC_LIST_DOWN), (lv->uNewState & LVIS_FOCUSED));
			::EnableWindow(GetDlgItem(IDC_LIST_EDIT), (lv->uNewState & LVIS_FOCUSED));
			::EnableWindow(GetDlgItem(IDC_LIST_REMOVE), (lv->uNewState & LVIS_FOCUSED));
			return 0;
		}
		
	private:
		ExCImage img_f;
		ExListViewCtrl ctrlList;
};


#endif // !defined(PUBLIC_HUBS_LIST_DLG_H)

/**
* @file
* $Id: PublicHubsListDlg.h 477 2010-01-29 08:59:43Z bigmuscle $
*/
