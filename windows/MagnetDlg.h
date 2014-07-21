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

#if !defined(MAGNET_DLG_H)
#define MAGNET_DLG_H

#include "../client/QueueManager.h"
#include "ResourceLoader.h"

// (Modders) Enjoy my liberally commented out source code.  The plan is to enable the
// magnet link add an entry to the download queue, with just the hash (if that is the
// only information the magnet contains).  DC++ has to find sources for the file anyway,
// and can take filename, size, etc. values from there.
//                                                        - GargoyleMT

class MagnetDlg : public CDialogImpl<MagnetDlg >
{
	public:
		enum { IDD = IDD_MAGNET };
		
		MagnetDlg(const TTHValue& aHash, const tstring& aFileName, const int64_t aSize, const int64_t dSize = 0, bool isDCLST = false
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		          , bool isViewMedia = false
#endif
		         ) : mHash(aHash), mFileName(aFileName), mSize(aSize), mdSize(dSize), mIsDCLST(isDCLST)
#ifdef SSA_VIDEO_PREVIEW_FEATURE
			, mIsViewMedia(isViewMedia)
#endif
		{ }
		~MagnetDlg() { }
		
		BEGIN_MSG_MAP(MagnetDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDOK, onCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_MAGNET_OPEN, onRadioButton)
		COMMAND_ID_HANDLER(IDC_MAGNET_QUEUE, onRadioButton)
		COMMAND_ID_HANDLER(IDC_MAGNET_NOTHING, onRadioButton)
		COMMAND_ID_HANDLER(IDC_MAGNET_SEARCH, onRadioButton)
		COMMAND_ID_HANDLER(IDC_MAGNET_SAVEAS, onSaveAs) // !SMT!-UI
		END_MSG_MAP();
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onRadioButton(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSaveAs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // !SMT!-UI
		
		bool isDCLST() const
		{
			return mIsDCLST;
		}
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		bool isViewMedia() const
		{
			return mIsViewMedia;
		}
#endif
	private:
		TTHValue mHash;
		tstring mFileName;
		ExCImage mImg;
		int64_t mSize;
		int64_t mdSize;
		bool mIsDCLST;
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		bool mIsViewMedia;
#endif
};

#endif // !defined(MAGNET_DLG_H)

/**
* @file
* $Id: MagnetDlg.h,v 1.7 2006/05/08 08:36:19 bigmuscle Exp $
*/
