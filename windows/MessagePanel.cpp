/*
 * Copyright (C) 2011-2021 FlylinkDC++ Team http://flylinkdc.com
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
#include "atlwin.h"
#include "ResourceLoader.h"
#include "MessagePanel.h"
#ifdef IRAINMAN_INCLUDE_SMILE
#include "AGEmotionSetup.h"
#endif

HIconWrapper MessagePanel::g_hSendMessageIco(IDR_SENDMESSAGES_ICON);
HIconWrapper MessagePanel::g_hMultiChatIco(IDR_MULTI_CHAT_ICON);
#ifdef IRAINMAN_INCLUDE_SMILE
HIconWrapper MessagePanel::g_hEmoticonIco(IDR_SMILE_ICON);
#endif
HIconWrapper MessagePanel::g_hBoldIco(IDR_BOLD_ICON);
HIconWrapper MessagePanel::g_hUndelineIco(IDR_UNDERLINE_ICON);
HIconWrapper MessagePanel::g_hStrikeIco(IDR_STRIKE_ICON);
HIconWrapper MessagePanel::g_hItalicIco(IDR_ITALIC_ICON);
HIconWrapper MessagePanel::g_hTransCodeIco(IDR_TRANSCODE_ICON);
#ifdef SCALOLAZ_BB_COLOR_BUTTON
HIconWrapper MessagePanel::g_hColorIco(IDR_COLOR_ICON);
#endif
#ifdef IRAINMAN_INCLUDE_SMILE
CEmotionMenu MessagePanel::g_emoMenu;
#endif

MessagePanel::MessagePanel(CEdit*& p_ctrlMessage)
	: m_hWnd(nullptr), m_ctrlMessage(p_ctrlMessage), m_isShutdown(false)
{
}

MessagePanel::~MessagePanel()
{
}

void MessagePanel::DestroyPanel(bool p_is_shutdown)
{
	m_isShutdown = p_is_shutdown; // TODO - убрать?
#ifdef IRAINMAN_INCLUDE_SMILE
	ctrlEmoticons.DestroyWindow();
#endif // IRAINMAN_INCLUDE_SMILE
	ctrlSendMessageBtn.DestroyWindow();
	ctrlMultiChatBtn.DestroyWindow();
	ctrlBoldBtn.DestroyWindow();
	ctrlUnderlineBtn.DestroyWindow();
	ctrlStrikeBtn.DestroyWindow();
	ctrlItalicBtn.DestroyWindow();
	ctrlTransCodeBtn.DestroyWindow();
#ifdef SCALOLAZ_BB_COLOR_BUTTON
	ctrlColorBtn.DestroyWindow();
#endif
	ctrlOSAGOBtn.DestroyWindow();
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
	ctrlSizeSel.DestroyWindow();
#endif
	m_tooltip.DestroyWindow();
}
LRESULT MessagePanel::InitPanel(HWND& p_hWnd, RECT &p_rcDefault)
{
	m_hWnd = p_hWnd;
	m_tooltip.Create(m_hWnd, p_rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP | TTS_BALLOON, WS_EX_TOPMOST);
	m_tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(m_tooltip.IsWindow());
#ifdef IRAINMAN_INCLUDE_SMILE
	ctrlEmoticons.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_EMOT);
	ctrlEmoticons.SetIcon(g_hEmoticonIco);
	m_tooltip.AddTool(ctrlEmoticons, ResourceManager::BBCODE_PANEL_EMOTICONS);
#endif // IRAINMAN_INCLUDE_SMILE
	
	ctrlSendMessageBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_SEND_MESSAGE);
	ctrlSendMessageBtn.SetIcon(g_hSendMessageIco);
	m_tooltip.AddTool(ctrlSendMessageBtn, ResourceManager::BBCODE_PANEL_SENDMESSAGE);
	
	ctrlMultiChatBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_MESSAGEPANEL);
	ctrlMultiChatBtn.SetIcon(g_hMultiChatIco);
	m_tooltip.AddTool(ctrlMultiChatBtn, ResourceManager::BBCODE_PANEL_MESSAGEPANELSIZE);
	
	ctrlBoldBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_BOLD);
	ctrlBoldBtn.SetIcon(g_hBoldIco);
	m_tooltip.AddTool(ctrlBoldBtn, ResourceManager::BBCODE_PANEL_BOLD);
	
	ctrlUnderlineBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_UNDERLINE);
	ctrlUnderlineBtn.SetIcon(g_hUndelineIco);
	m_tooltip.AddTool(ctrlUnderlineBtn, ResourceManager::BBCODE_PANEL_UNDERLINE);
	
	ctrlStrikeBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_STRIKE);
	ctrlStrikeBtn.SetIcon(g_hStrikeIco);
	m_tooltip.AddTool(ctrlStrikeBtn, ResourceManager::BBCODE_PANEL_STRIKE);
	
	ctrlItalicBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_ITALIC);
	ctrlItalicBtn.SetIcon(g_hItalicIco);
	m_tooltip.AddTool(ctrlItalicBtn, ResourceManager::BBCODE_PANEL_ITALIC);
	
	ctrlTransCodeBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, ID_TEXT_TRANSCODE);
	ctrlTransCodeBtn.SetIcon(g_hTransCodeIco);
	m_tooltip.AddTool(ctrlTransCodeBtn, ResourceManager::BBCODE_PANEL_TRANSLATE);
	
#ifdef SCALOLAZ_BB_COLOR_BUTTON
	ctrlColorBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_COLOR);
	ctrlColorBtn.SetIcon(g_hColorIco);
	m_tooltip.AddTool(ctrlColorBtn, ResourceManager::BBCODE_PANEL_COLOR);
#endif
	ctrlOSAGOBtn.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER, 0, IDC_OSAGO);
	static ExCImage g_OSAGOHandle;
	if (!(HBITMAP)g_OSAGOHandle)
	{
		g_OSAGOHandle.LoadFromResourcePNG(IDR_OSAGO_PNG);
	}
	if (ctrlOSAGOBtn.GetBitmap() == NULL)
	{
		ctrlOSAGOBtn.SetBitmap(g_OSAGOHandle);
	}
	//  m_tooltip.AddTool(ctrlOSAGOBtn, ResourceManager::BBCODE_PANEL_COLOR);
	
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
	ctrlSizeSel.Create(m_hWnd, p_rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL |
	                   WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
	ctrlSizeSel.SetFont(Fonts::g_font);
	
	ctrlSizeSel.AddString(L"-2");
	ctrlSizeSel.AddString(L"-1");
	ctrlSizeSel.AddString(L"+1");
	ctrlSizeSel.AddString(L"+2");
	ctrlSizeSel.SetCurSel(2);
#endif
	
	m_tooltip.SetMaxTipWidth(200);
	if (!BOOLSETTING(POPUPS_DISABLED) && BOOLSETTING(POPUPS_MESSAGEPANEL_ENABLED))
	{
		m_tooltip.Activate(TRUE);
	}
	
	return 0;
}

LRESULT MessagePanel::UpdatePanel(CRect& rect)
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!m_isShutdown);
	m_tooltip.Activate(FALSE);
	if (m_hWnd == NULL)
		return 0;
		
	CRect rc = rect;
	CRect rc_osago = rc;
	
	rc.left += 2;
	rc.right += 23;
	rc.top = rc.bottom - 19;
	
	rc_osago.top = rc_osago.bottom - 21;
	rc_osago.left -= 116;
	rc_osago.bottom += 2;
	
	ctrlOSAGOBtn.ShowWindow(SW_SHOW);
	
	ctrlOSAGOBtn.MoveWindow(rc_osago);
	
	if (BOOLSETTING(SHOW_SEND_MESSAGE_BUTTON))
	{
		ctrlSendMessageBtn.ShowWindow(SW_SHOW);
		ctrlSendMessageBtn.MoveWindow(rc);
		rc.left = rc.right + 1;
		rc.right += 22;
	}
	else
	{
		ctrlSendMessageBtn.ShowWindow(SW_HIDE);
	}
	
	if (BOOLSETTING(SHOW_MULTI_CHAT_BTN))
	{
		ctrlMultiChatBtn.ShowWindow(SW_SHOW);
		ctrlMultiChatBtn.MoveWindow(rc);
		rc.left = rc.right + 1;
		rc.right += 22;
	}
	else
	{
		ctrlMultiChatBtn.ShowWindow(SW_HIDE);
	}
#ifdef IRAINMAN_INCLUDE_SMILE
	if (BOOLSETTING(SHOW_EMOTIONS_BTN))
	{
		ctrlEmoticons.ShowWindow(SW_SHOW);
		ctrlEmoticons.MoveWindow(rc);
		rc.left = rc.right + 1;
		rc.right += 22;
	}
	else
	{
		ctrlEmoticons.ShowWindow(SW_HIDE);
	}
#endif // IRAINMAN_INCLUDE_SMILE
	if (BOOLSETTING(SHOW_BBCODE_PANEL))
	{
		// Transcode
		//rc.left = rc.right + 1;
		//rc.right += 23;
		ctrlTransCodeBtn.ShowWindow(SW_SHOW);
		ctrlTransCodeBtn.MoveWindow(rc);
		// Bold
		rc.left = rc.right + 1;
		rc.right += 22;
		ctrlBoldBtn.ShowWindow(SW_SHOW);
		ctrlBoldBtn.MoveWindow(rc);
		// Italic
		rc.left = rc.right + 1;
		rc.right += 22;
		ctrlItalicBtn.ShowWindow(SW_SHOW);
		ctrlItalicBtn.MoveWindow(rc);
		// Underline
		rc.left = rc.right + 1;
		rc.right += 22;
		ctrlUnderlineBtn.ShowWindow(SW_SHOW);
		ctrlUnderlineBtn.MoveWindow(rc);
		// Strike
		rc.left = rc.right + 1;
		rc.right += 22;
		ctrlStrikeBtn.ShowWindow(SW_SHOW);
		ctrlStrikeBtn.MoveWindow(rc);
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		if (BOOLSETTING(FORMAT_BB_CODES_COLORS))
		{
			// Color
			rc.left = rc.right + 1;
			rc.right += 22;
			ctrlColorBtn.ShowWindow(SW_SHOW);
			ctrlColorBtn.MoveWindow(rc);
		}
		else
		{
			ctrlColorBtn.ShowWindow(SW_HIDE);
		}
#endif // SCALOLAZ_BB_COLOR_BUTTON
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
		ctrlSizeSel.ShowWindow(SW_HIDE);
#endif // FLYLINKDC_USE_BB_SIZE_CODE
	}
	else
	{
		ctrlBoldBtn.ShowWindow(SW_HIDE);
		ctrlStrikeBtn.ShowWindow(SW_HIDE);
		ctrlUnderlineBtn.ShowWindow(SW_HIDE);
		ctrlItalicBtn.ShowWindow(SW_HIDE);
#ifdef FLYLINKDC_USE_BB_SIZE_CODE
		ctrlSizeSel.ShowWindow(SW_HIDE);
#endif
		ctrlTransCodeBtn.ShowWindow(SW_HIDE);
#ifdef SCALOLAZ_BB_COLOR_BUTTON
		ctrlColorBtn.ShowWindow(SW_HIDE);
#endif
	}
	if (!BOOLSETTING(POPUPS_DISABLED) && BOOLSETTING(POPUPS_MESSAGEPANEL_ENABLED))
	{
		m_tooltip.Activate(TRUE);
	}
	return 0;
}

int MessagePanel::GetPanelWidth()
{
	int iButtonPanelLength = 0;
	iButtonPanelLength += BOOLSETTING(SHOW_EMOTIONS_BTN) ? 22 : 0;
#ifdef IRAINMAN_INCLUDE_SMILE
	iButtonPanelLength += BOOLSETTING(SHOW_EMOTIONS_BTN) ? 22 : 0;
#endif
	iButtonPanelLength += BOOLSETTING(SHOW_SEND_MESSAGE_BUTTON) ? 22 : 0;
	iButtonPanelLength += BOOLSETTING(SHOW_BBCODE_PANEL) ? 22 *
#ifdef SCALOLAZ_BB_COLOR_BUTTON
	                      6
#else   //SCALOLAZ_BB_COLOR_BUTTON
	                      5
#endif  //SCALOLAZ_BB_COLOR_BUTTON
	                      : 0;
	iButtonPanelLength += 1;
	
	return iButtonPanelLength;
}
#ifdef IRAINMAN_INCLUDE_SMILE
LRESULT MessagePanel::onEmoticons(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& bHandled)
{
	dcassert(!m_isShutdown);
	if (!m_isShutdown)
	{
		m_tooltip.Activate(FALSE);
		// fix COPY-PASTE!
		if (hWndCtl != ctrlEmoticons.m_hWnd)
		{
			bHandled = false;
			return 0;
		}
		EmoticonsDlg dlg;
		ctrlEmoticons.GetWindowRect(dlg.pos);
		dlg.DoModal(m_hWnd);
		if (!dlg.result.empty())
		{
			// !BUGMASTER!
			int start, end;
			int len = m_ctrlMessage->GetWindowTextLength();
			m_ctrlMessage->GetSel(start, end);
			std::vector<TCHAR> l_message(len + 1);
			m_ctrlMessage->GetWindowText(&l_message[0], len + 1);
			tstring s1(&l_message[0], start);
			tstring s2(&l_message[end], len - end);
			m_ctrlMessage->SetWindowText((s1 + dlg.result + s2).c_str());
			m_ctrlMessage->SetFocus();
			start += dlg.result.length();
			m_ctrlMessage->SetSel(start, start);
			// end !BUGMASTER!
		}
		if (!BOOLSETTING(POPUPS_DISABLED) && BOOLSETTING(POPUPS_MESSAGEPANEL_ENABLED))
		{
			if (m_tooltip.IsWindow())
			{
				m_tooltip.Activate(TRUE);
			}
		}
	}
	return 0;
}

LRESULT MessagePanel::onEmoPackChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	dcassert(!m_isShutdown);
	if (m_isShutdown)
		return 0;
	m_tooltip.Activate(FALSE);
	LocalArray<TCHAR, 256> buf;
	g_emoMenu.GetMenuString(wID, buf.data(), buf.size(), MF_BYCOMMAND);
	if (buf.data() != Text::toT(SETTING(EMOTICONS_FILE)))
	{
		SET_SETTING(EMOTICONS_FILE, Text::fromT(buf.data()));
		if (!CAGEmotionSetup::reCreateEmotionSetup())
		{
			return -1;
		}
	}
	if (!BOOLSETTING(POPUPS_DISABLED) && BOOLSETTING(POPUPS_MESSAGEPANEL_ENABLED))
	{
		m_tooltip.Activate(TRUE);
	}
	return 0;
}
#endif // IRAINMAN_INCLUDE_SMILE
BOOL MessagePanel::OnContextMenu(POINT& pt, WPARAM& wParam)
{
	dcassert(!m_isShutdown);
#ifdef IRAINMAN_INCLUDE_SMILE
	if (reinterpret_cast<HWND>(wParam) == ctrlEmoticons)
	{
		g_emoMenu.CreateEmotionMenu(pt, m_hWnd, IDC_EMOMENU);
		return TRUE;
	}
#endif
	return FALSE;
}
