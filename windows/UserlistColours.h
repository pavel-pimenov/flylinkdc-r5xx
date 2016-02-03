#ifndef UserListColours_H
#define UserListColours_H

#pragma once


#include "PropPage.h"

class UserListColours : public CPropertyPage<IDD_USERLIST_COLOURS_PAGE>, public PropPage
{
	public:
	
		explicit UserListColours() : PropPage(TSTRING(SETTINGS_APPEARANCE) + _T('\\') + TSTRING(SETTINGS_TEXT_STYLES) + _T('\\') + TSTRING(SETTINGS_USER_LIST))
		{
			SetTitle(m_title.c_str());
			m_psp.dwFlags |= PSP_RTLREADING;
		}
		
		~UserListColours()
		{
		}
		
		BEGIN_MSG_MAP(UserListColours)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_HANDLER(IDC_CHANGE_COLOR, BN_CLICKED, onChangeColour)
		COMMAND_HANDLER(IDC_IMAGEBROWSE, BN_CLICKED, onImageBrowse)
		COMMAND_ID_HANDLER(IDC_USERS_LINK, onLinkClick)
		CHAIN_MSG_MAP(PropPage)
		REFLECT_NOTIFICATIONS()
		END_MSG_MAP()
		
		LRESULT onInitDialog(UINT, WPARAM, LPARAM, BOOL&);
		LRESULT onChangeColour(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onImageBrowse(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
		LRESULT onLinkClick(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
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
		CRichEditCtrl n_Preview;
	private:
		void BrowseForPic(int DLGITEM);
		
		void refreshPreview();
		
		CListBox n_lsbList;
		int normalColour;
		int favoriteColour;
		int favEnemyColour;
		int reservedSlotColour;
		int ignoredColour;
		int fastColour;
		int serverColour;
		int pasiveColour;
		int opColour;
		int fullCheckedColour;
		int badClientColour;
		int badFilelistColour;
		
		ExCImage m_png_users;
		CFlyHyperLink m_hlink_users;
	protected:
		static Item items[];
		static TextItem texts[];
		
};

#endif //UserListColours_H

/**
 * @file
 * $Id: UserlistColours.h 499 2010-05-16 11:01:37Z bigmuscle $
 */
