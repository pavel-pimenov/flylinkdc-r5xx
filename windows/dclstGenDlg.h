/*
 * Copyright (C) 2012-2021 FlylinkDC++ Team http://flylinkdc.com
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

#if !defined(DCLST_GEN_DLG_H)
#define DCLST_GEN_DLG_H


#pragma once


#include "../client/DirectoryListing.h"
#include "../client/Util.h"
#include "ResourceLoader.h"

#define WM_UPDATE_WINDOW WM_USER + 101
#define WM_FINISHED  WM_USER+102

class DCLSTGenDlg : public CDialogImpl< DCLSTGenDlg >, public Thread
{
	public:
		enum { IDD = IDD_DCLS_GENERATOR };
		
		DCLSTGenDlg(DirectoryListing::Directory* aDir, const UserPtr& usr) : _Dir(aDir), _usr(usr),  _breakThread(false), _progresStatus(0), _totalFiles(0), _totalFolders(0), _totalSize(0), _filesCount(0), _isCanceled(false), _isInProcess(false)  { }
		~DCLSTGenDlg() { }
		
		BEGIN_MSG_MAP(DCLSTGenDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, onInitDialog)
		COMMAND_ID_HANDLER(IDCANCEL, onCloseCmd)
		COMMAND_ID_HANDLER(IDC_DCLSTGEN_COPYMAGNET, onCopyMagnet)
		COMMAND_ID_HANDLER(IDC_DCLSTGEN_SHARE, onShareThis)
		COMMAND_ID_HANDLER(IDC_DCLSTGEN_SAVEAS, onSaveAS)
		MESSAGE_HANDLER(WM_UPDATE_WINDOW, OnUpdateDCLSTWindow)
		MESSAGE_HANDLER(WM_FINISHED, OnFinishedDCLSTWindow)
		END_MSG_MAP();
		
		LRESULT onInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT OnUpdateDCLSTWindow(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT OnFinishedDCLSTWindow(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onShareThis(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSaveAS(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		int run() override;
		string getDCLSTName(const string& folderName);
		
	private:
		void UpdateDialogItems();
		void MakeXMLCaption();
		void MakeXMLEnd();
		void WriteFolder(DirectoryListing::Directory* dir);
		void WriteFile(DirectoryListing::File* file);
		size_t PackAndSave();
		void CalculateTTH();
		
	private:
		DirectoryListing::Directory* _Dir;
		ExCImage _img;
		ExCImage _img_f;
		bool _breakThread;
		int _progresStatus;
		size_t _totalFiles;
		int _totalFolders;
		int64_t _totalSize;
		size_t _filesCount;
		CProgressBarCtrl _pBar;
		UserPtr         _usr;
		string      _xml;
		string _mNameDCLST;
		string _strMagnet;
		volatile bool _isCanceled;
		volatile bool _isInProcess;
		unique_ptr<TigerTree>  _tth;
		
};

#endif // !defined(DCLST_GEN_DLG_H)
