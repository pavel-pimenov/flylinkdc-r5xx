/*
 * FlylinkDC++ // Chat Settings Page
 */

#if !defined(MESSAGES_CHAT_PAGE_H)
#define MESSAGES_CHAT_PAGE_H

#pragma once

#include <atlcrack.h>
#include "PropPage.h"
#include "ExListViewCtrl.h" // [+] IRainman
#include "wtl_flylinkdc.h"

class MessagesChatPage : public CPropertyPage<IDD_MESSAGES_CHAT_PAGE>, public PropPage
{
	public:
		explicit MessagesChatPage() : PropPage(TSTRING(SETTINGS_MESSAGES) + _T('\\') + TSTRING(SETTINGS_ADVANCED))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		~MessagesChatPage()
		{
			ctrlList_chat.Detach(); // [+] IRainman
			ctrlSee.Detach();
			ctrlProtect.Detach();
			ctrlRnd.Detach();
		}
		
		BEGIN_MSG_MAP(MessagesChatPage)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog_chat)
		COMMAND_ID_HANDLER(IDC_PROTECT_PRIVATE_RND, onClickedUse)
		COMMAND_ID_HANDLER(IDC_PM_PASSWORD_GENERATE, onRandomPassword)
		COMMAND_HANDLER(IDC_PM_PASSWORD_HELP, BN_CLICKED, onClickedHelp)
		NOTIFY_HANDLER(IDC_MESSAGES_CHAT_BOOLEANS, NM_CUSTOMDRAW, ctrlList_chat.onCustomDraw) // [+] IRainman
		CHAIN_MSG_MAP(PropPage)
		END_MSG_MAP()
		
		LRESULT onInitDialog_chat(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onClickedUse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRandomPassword(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClickedHelp(WORD /* wNotifyCode */, WORD wID, HWND /* hWndCtl */, BOOL& /* bHandled */);
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
	private:
		CFlyToolTipCtrl tooltip_messageschat;  // [+] SCALOlaz: add tooltips
		CButton ctrlSee;
		CButton ctrlProtect;
		CButton ctrlRnd;
		
	protected:
	
		static Item g_items_chat[];
		static TextItem g_texts_chat[];
		static ListItem g_listItems_chat[];
		void fixControls();
		ExListViewCtrl ctrlList_chat; // [+] IRainman
};

#endif //MESSAGES_CHAT_PAGE_H
