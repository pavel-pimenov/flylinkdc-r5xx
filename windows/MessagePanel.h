/*
 * Copyright (C) 2011-2017 FlylinkDC++ Team http://flylinkdc.com
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
#if !defined(MESSAGE_PANEL_H)
#define MESSAGE_PANEL_H

#pragma once

#include "WinUtil.h"
#include "wtl_flylinkdc.h"
#ifdef IRAINMAN_INCLUDE_SMILE
#include "EmoticonsDlg.h"
#endif

class MessagePanel
{
		BEGIN_MSG_MAP(MessagePanel)
#ifdef IRAINMAN_INCLUDE_SMILE
		COMMAND_ID_HANDLER(IDC_EMOT, onEmoticons)
		COMMAND_RANGE_HANDLER(IDC_EMOMENU, IDC_EMOMENU + GetEmotionMenuItemsCount(), onEmoPackChange)
#endif
		END_MSG_MAP()
		
	public:
		explicit MessagePanel(CEdit*& p_ctrlMessage);
		~MessagePanel();
		LRESULT  InitPanel(HWND& p_hWnd, RECT &p_rcDefault);
		void DestroyPanel(bool p_is_shutdown);
		LRESULT  UpdatePanel(CRect& rect);
		static int GetPanelWidth();
#ifdef IRAINMAN_INCLUDE_SMILE
		int GetEmotionMenuItemsCount()
		{
			return g_emoMenu.GetItemsCount();
		}
		LRESULT onEmoticons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& bHandled);
		LRESULT onEmoPackChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
#endif // IRAINMAN_INCLUDE_SMILE
		BOOL OnContextMenu(POINT& pt, WPARAM& wParam);
		
	private:
		CFlyToolTipCtrl m_tooltip;  // [+] SCALOlaz: add tooltips
		CEdit*& m_ctrlMessage;
		
		CButton ctrlShowUsers;
#ifdef IRAINMAN_INCLUDE_SMILE
		CButton ctrlEmoticons;
#endif
		CButton ctrlSendMessageBtn;
		CButton ctrlMultiChatBtn;
		CButton ctrlBoldBtn;
		CButton ctrlStrikeBtn;
		CButton ctrlItalicBtn;
		CButton ctrlUnderlineBtn;
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
		CComboBox ctrlSizeSel;
#endif
		CButton ctrlTransCodeBtn;
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		CButton ctrlColorBtn;
#endif
		CButton ctrlOSAGOBtn;
#ifdef IRAINMAN_INCLUDE_SMILE
		static CEmotionMenu g_emoMenu;
#endif
		HWND m_hWnd;
		bool m_isShutdown;
		
		static HIconWrapper g_hSendMessageIco;
		static HIconWrapper g_hMultiChatIco;
#ifdef IRAINMAN_INCLUDE_SMILE
		static HIconWrapper g_hEmoticonIco;
#endif
		static HIconWrapper g_hBoldIco;
		static HIconWrapper g_hUndelineIco;
		static HIconWrapper g_hStrikeIco;
		static HIconWrapper g_hItalicIco;
		static HIconWrapper g_hTransCodeIco;
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		static HIconWrapper g_hColorIco;
#endif
		// [~] Sergey Shuhskanov
};


#endif //MESSAGE_PANEL_H