/*
 *
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

#if !defined(DCPLUSPLUS_CLIENT_PROFILE_DLG_H)
#define DCPLUSPLUS_CLIENT_PROFILE_DLG_H

#pragma once

#if !defined(AFX_ClientProfileDlg_H__A7EB85C3_1EEA_4FEC_8450_C090219B8619__INCLUDED_)
#define AFX_ClientProfileDlg_H__A7EB85C3_1EEA_4FEC_8450_C090219B8619__INCLUDED_

#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
#include "../client/Util.h"
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
#include "../client/ClientProfileManager.h"
#else
#include "../client/DetectionManager.h"
#endif


class ClientProfileDlg : public CDialogImpl<ClientProfileDlg>
{
		CEdit ctrlName, ctrlVersion, ctrlTag, ctrlExtendedTag, ctrlLock, ctrlPk, ctrlSupports,
		      /*ctrlTestSUR,*/ ctrlUserConCom, ctrlStatus, ctrlCheatingDescription, ctrlAddLine,
		      ctrlConnection, ctrlComment, ctrlRegExpText;
		      
		CComboBox ctrlRaw, ctrlRegExpCombo;
		
		CButton ctrlTagVersion, ctrlUseExtraVersion, ctrlCheckMismatch, ctrlRegExpButton, ctrlRecheck, ctrlSkipExtended;
		
	public:
	
		tstring name;
		tstring version;
		tstring tag;
		tstring extendedTag;
		tstring lock;
		tstring pk;
		tstring supports;
		//tstring testSUR;
		tstring userConCom;
		tstring status;
		tstring cheatingDescription;
		tstring addLine;
		tstring connection;
		tstring comment;
		int priority;
		int rawToSend;
//  bool tagVersion;
		bool useExtraVersion;
		bool checkMismatch;
		int currentProfileId;
		bool adding; // Member variable 'ClientProfileDlg::adding' is not initialized in the constructor.
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
		ClientProfile currentProfile;
#else
		DetectionEntry currentProfile;
#endif
		
		enum { IDD = IDD_CLIENT_PROFILE };
		
		BEGIN_MSG_MAP(ClientProfileDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_CLIENT_NAME, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_VERSION, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_TAG, onChangeTag)
		COMMAND_ID_HANDLER(IDC_CLIENT_EXTENDED_TAG, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_LOCK, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_PK, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_SUPPORTS, onChange)
		//COMMAND_ID_HANDLER(IDC_CLIENT_TESTSUR_RESPONSE, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_USER_CON_COM, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_STATUS, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_CHEATING_DESCRIPTION, onChange)
		COMMAND_ID_HANDLER(IDC_CLIENT_CONNECTION, onChange)
		COMMAND_ID_HANDLER(IDC_COMMENT, onChange)
		COMMAND_ID_HANDLER(IDC_NEXT, onNext)
		COMMAND_ID_HANDLER(IDC_REGEXP_TESTER_BUTTON, onMatch)
		END_MSG_MAP()
		
		ClientProfileDlg() : priority(0), rawToSend(0), /*tagVersion(0),*/ useExtraVersion(0), checkMismatch(0), currentProfileId(0), adding(false) { }
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlName.SetFocus();
			return FALSE;
		}
		
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		LRESULT onChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChangeTag(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMatch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (wID == IDOK)
			{
				updateVars();
				if (adding)
				{
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
					ClientProfile::List lst = ClientProfileManager::getInstance()->getClientProfiles();
#else
					const DetectionManager::DetectionItems& lst = DetectionManager::getInstance()->getProfiles();
#endif
					for (auto j = lst.cbegin(); j != lst.cend(); ++j)
					{
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
						if ((*j).getName().compare(Text::fromT(name)) == 0)
#else
						if ((*j).name.compare(Text::fromT(name)) == 0)
#endif
						{
							MessageBox("A client profile with this name already exists", CTSTRING(ERROR_STRING), MB_ICONSTOP);
							return 0;
						}
					}
				}
			}
			EndDialog(wID);
			return 0;
		}
	private:
		enum { BUF_LEN = 1024 };
		enum { MATCH, MISMATCH, INVALID };
		enum { VERSION, TAG, DESCRIPTION,
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
		       LOCK, PK,
#endif
		       SUPPORTS, /*TESTSUR,*/ COMMANDS, STATUS, CONNECTION
		     };
		void updateAddLine();
		void updateTag();
		void updateVars();
		void updateControls();
		void getProfile();
		int matchExp(const string& aExp, const string& aString);
#ifdef IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
		string getVersion(const string& aExp, const string& aTag);
		string splitVersion(const string& aExp, const string& aTag, const int part);
#endif // IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER
		StringMap params;
};

#endif // IRAINMAN_INCLUDE_OLD_CLIENT_PROFILE_MANAGER

#endif // DCPLUSPLUS_CLIENT_PROFILE_DLG_H
