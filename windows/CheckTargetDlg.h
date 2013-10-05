/*
 * Copyright (C) 2010-2013 FlylinkDC++ Team http://flylinkdc.com
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

#ifndef _CHECK_TARGET_DLG_
#define _CHECK_TARGET_DLG_


#include "../client/QueueManager.h"

class CheckTargetDlg : public CDialogImpl< CheckTargetDlg >
{
	public:
		enum { IDD = IDD_CHECKTARGETDLG };
		
		CheckTargetDlg(const string& aFileName, int64_t aSizeNew, int64_t aSizeExist, time_t aTimeExist, int option)
			: mFileName(Text::toT(aFileName)), mSizeNew(aSizeNew), mSizeExist(aSizeExist), mTimeExist(aTimeExist), mOption(option), mApply(false) { }
		~CheckTargetDlg() { }
		
		BEGIN_MSG_MAP(CheckTargetDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_REPLACE_REPLACE, onRadioButton)
		COMMAND_ID_HANDLER(IDC_REPLACE_RENAME, onRadioButton)
		COMMAND_ID_HANDLER(IDC_REPLACE_SKIP, onRadioButton)
		COMMAND_ID_HANDLER(IDC_REPLACE_CHANGE_NAME, onChangeName) // !SMT!-UI
		// COMMAND_ID_HANDLER(IDC_REPLACE_APPLY, onApply) // !SMT!-UI
		END_MSG_MAP();
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChangeName(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // !SMT!-UI
		// LRESULT onApply(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // !SMT!-UI
		/*
		const string& GetNewFilename() const
		{
		    return mFileName;
		}
		*/
		int     GetOption() const
		{
			return mOption;
		}
		bool    IsApplyForAll() const
		{
			return mApply;
		}
	private:
		tstring mFileName;
		int64_t mSizeNew;
		int64_t mSizeExist;
		time_t mTimeExist;
		int mOption;
		bool mApply;
		tstring mRenameName;
};




#endif // #ifndef _CHECK_TARGET_DLG_