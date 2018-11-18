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

#ifndef _WZ_SHARE_H_
#define _WZ_SHARE_H_

#pragma once

#ifdef SSA_WIZARD_FEATURE

#include "resource.h"
#include "../client/SearchManager.h"
#include "../LineDlg.h"


class WzShare : public CPropertyPageImpl<WzShare>
{
public:
    enum { IDD = IDD_WIZARD_SHARE };

    // Construction
    WzShare() : CPropertyPageImpl<WzShare>(  CTSTRING(WIZARD_TITLE) )	
	{
		
	}

    // Maps
    BEGIN_MSG_MAP(WzShare)
        MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_ID_HANDLER(IDC_WIZARD_SHARE_ADD, OnAdd)
		COMMAND_ID_HANDLER(IDC_WIZARD_SHARE_REMOVE, OnRemove)
		COMMAND_ID_HANDLER(IDC_WIZARD_SHARE_EDIT, OnEdit)
		COMMAND_ID_HANDLER(IDC_WIZARD_SHARE_RENAME, OnRename)
		NOTIFY_HANDLER(IDC_WIZARD_SHARE_LIST, NM_CUSTOMDRAW, ctrlDirectories.onCustomDraw) // [+] IRainman
        CHAIN_MSG_MAP(CPropertyPageImpl<WzShare>)
    END_MSG_MAP()

    // Message handlers
    BOOL OnInitDialog ( HWND hwndFocus, LPARAM lParam )
	{
		// for Custom Themes Bitmap
		img_f.LoadFromResourcePNG(IDR_FLYLINK_PNG);
		GetDlgItem(IDC_WIZARD_STARTUP_PICT).SendMessage(STM_SETIMAGE, IMAGE_BITMAP, LPARAM((HBITMAP)img_f));

		SetDlgItemText(IDC_WIZARD_SHARE_STATIC, CTSTRING(WIZARD_SHARE_STATIC));
		SetDlgItemText(IDC_WIZARD_SHARE_ADD, CTSTRING(ADD2));
		SetDlgItemText(IDC_WIZARD_SHARE_REMOVE, CTSTRING(REMOVE2));
		SetDlgItemText(IDC_WIZARD_SHARE_EDIT, CTSTRING(EDIT));
		SetDlgItemText(IDC_WIZARD_SHARE_RENAME, CTSTRING(RENAME));

		ctrlDirectories.Attach(GetDlgItem(IDC_WIZARD_SHARE_LIST));
		SET_EXTENDENT_LIST_VIEW_STYLE_WITH_CHECK(ctrlDirectories);
#ifdef USE_SET_LIST_COLOR_IN_SETTINGS
		SET_LIST_COLOR_IN_SETTING(ctrlDirectories);
#endif

		// Prepare shared dir list
		// <Directory Virtual="Windows 7 Service Pack 1">\\FILESERVER\Volume_1\Distr\MS\Win7SP1\</Directory>
		CRect rc;
		ctrlDirectories.GetClientRect(rc);
		ctrlDirectories.InsertColumn(0, CTSTRING(NAME), LVCFMT_LEFT, rc.Width() / 4, 0);
		ctrlDirectories.InsertColumn(1, CTSTRING(DIRECTORY), LVCFMT_LEFT, rc.Width() * 3 / 4, 1);

		CFlyDirItemArray directories;
		ShareManager::getDirectories(directories);
		auto cnt = ctrlDirectories.GetItemCount();
		for (auto j = directories.cbegin(); j != directories.cend(); ++j)
		{
			const int i = ctrlDirectories.insert(cnt++, Text::toT(j->m_synonym));
			ctrlDirectories.SetItemText(i, 1, Text::toT(j->m_path).c_str());
			ctrlDirectories.SetCheckState(i, TRUE);
			// ctrlDirectories.SetItemText(i, 2, Text::toT(j->second).c_str());
		}
		ctrlDirectories.Detach();
		return TRUE;
	}

	bool IsShareDisabledForFolder(const tstring& target)
	{
		const string targetFolder = Text::fromT(target);
		const string tempFolder = SETTING(TEMP_DOWNLOAD_DIRECTORY);
		if (stricmp(tempFolder, targetFolder) == 0)
		{
			MessageBox(CTSTRING(DONT_SHARE_TEMP_DIRECTORY), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONERROR);
			return true;
		}

		return false;
	}

	LRESULT OnAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		tstring target;
		if (WinUtil::browseDirectory(target, m_hWnd))
		{
			// Check If folder already in
			bool bFindF = false;			
			ctrlDirectories.Attach(GetDlgItem(IDC_WIZARD_SHARE_LIST));
			const auto cnt = ctrlDirectories.GetItemCount();
			for (int i = 0; i<cnt; i++)
			{
				AutoArray<TCHAR> path(FULL_MAX_PATH);
				if ( ctrlDirectories.GetItemText(i, 1, path, FULL_MAX_PATH-1) )
				{
					if (target.compare(path) == 0)
					{
						bFindF = true;
						break;
					}
				}
			}
			if (IsShareDisabledForFolder(target))
			{
				// MessageBox inside
				ctrlDirectories.Detach();
				return 0;
			}

			if (bFindF)
			{
				ctrlDirectories.Detach();
				MessageBox(CTSTRING(WIZARD_SHARE_ALREADYIN), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONERROR);
				return 0;
			}
			// [-] IRainman fix: in DC++ are allowed to merge directory with the same name.
			//tstring lastName = GetLastNameFormPath(target, ctrlDirectories);
			//
			//if (lastName.empty())
			//{
			//	ctrlDirectories.Detach();
			//	MessageBox(CTSTRING(WIZARD_SHARE_ALREADYINDESC), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONERROR);
			//	return 0;
			//}
			// Get last folder name
			const int i = ctrlDirectories.insert(ctrlDirectories.GetItemCount(), Util::getLastDir(target));
			ctrlDirectories.SetItemText(i, 1, target.c_str());
			ctrlDirectories.SetCheckState(i, TRUE);
			ctrlDirectories.Detach();

		}		

		return 0;
	}



	// [-] IRainman fix: in DC++ are allowed to merge directory with the same name.
	//tstring GetLastNameFormPath(tstring target, const ExListViewCtrl& ctrlDirectories)
	//{
	//		tstring lastName = Util::getLastDir(target);
	//		tstring lastNameSample = lastName; 
	//		bool bFindD;
	//		int iCount = 0;
	//		do
	//		{
	//			bFindD = false;			
	//			for (int i = 0; i<ctrlDirectories.GetItemCount(); i++)
	//			{
	//				AutoArray<TCHAR> path(FULL_MAX_PATH);
	//				if ( ctrlDirectories.GetItemText(i, 0, path, FULL_MAX_PATH-1) )
	//				{
	//					if (lastName.compare(path) == 0)
	//					{
	//						bFindD = true;
	//						break;
	//					}
	//				}				
	//			}
	//			if (bFindD)
	//			{
	//				lastName = lastNameSample;
	//				lastName += L'_';
	//				lastName += Text::toT( Util::toString( ++iCount ));
	//			}
	//		}while (bFindD);
	//
	//		return lastName;
	//}

	LRESULT OnRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		ctrlDirectories.Attach(GetDlgItem(IDC_WIZARD_SHARE_LIST));
		int iSelected = ctrlDirectories.GetSelectedIndex();
		if (iSelected > -1 && iSelected < ctrlDirectories.GetItemCount())
		{
			AutoArray<TCHAR> path(FULL_MAX_PATH);
			if ( ctrlDirectories.GetItemText(iSelected, 1, path, FULL_MAX_PATH-1) )
			{
				tstring question = path;
				question += TSTRING(WIZARD_SHARE_DELETEFOLDERQUESTION);
				if (MessageBox(question.c_str(), CTSTRING(WIZARD_TITLE), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1 ) == IDYES )
				{
					ctrlDirectories.DeleteItem(iSelected);
				}
			}
		}
		ctrlDirectories.Detach();
		return 0;
	}

	LRESULT OnEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		ctrlDirectories.Attach(GetDlgItem(IDC_WIZARD_SHARE_LIST));
		int iSelected = ctrlDirectories.GetSelectedIndex();
		if (iSelected > -1 && iSelected < ctrlDirectories.GetItemCount())
		{
			AutoArray<TCHAR> path(FULL_MAX_PATH);
			if ( ctrlDirectories.GetItemText(iSelected, 1, path, FULL_MAX_PATH-1) )
			{
				tstring target = path;
				if (WinUtil::browseDirectory(target, m_hWnd))
				{
					bool bFindF = false;
					for (int i = 0; i<ctrlDirectories.GetItemCount(); i++)
					{
						if ( i != iSelected)
						{
							AutoArray<TCHAR> path(FULL_MAX_PATH);
							if ( ctrlDirectories.GetItemText(i, 1, path, FULL_MAX_PATH-1) )
							{
								if (target.compare(path) == 0)
								{
									bFindF = true;
									break;
								}
							}
						}
					}
					if (IsShareDisabledForFolder(target))
					{
						// MessageBox inside
						ctrlDirectories.Detach();
						return 0;
					}
					if (bFindF)
					{
						ctrlDirectories.Detach();
						MessageBox(CTSTRING(WIZARD_SHARE_ALREADYIN), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONERROR);
						return 0;
					}
					ctrlDirectories.SetItemText(iSelected, 1, target.c_str());
				}
			}
		}
		ctrlDirectories.Detach();
		return 0;
	}

	LRESULT OnRename(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	{
		ctrlDirectories.Attach(GetDlgItem(IDC_WIZARD_SHARE_LIST));
		int iSelected = ctrlDirectories.GetSelectedIndex();
		if (iSelected > -1 && iSelected < ctrlDirectories.GetItemCount())
		{
			//[-] PVS-Studio V808 tstring pathSel;
			tstring pathName;
			AutoArray<TCHAR> path(FULL_MAX_PATH);
			if ( ctrlDirectories.GetItemText(iSelected, 0, path, FULL_MAX_PATH-1) )
			{
				pathName = path;
			}
			LineDlg virt;
			virt.title = TSTRING(VIRTUAL_NAME);
			virt.description = TSTRING(VIRTUAL_NAME_LONG);
			virt.line = pathName;
			if (virt.DoModal(m_hWnd) == IDOK)
			{
				// [!] IRainman fix: in DC++ are allowed to merge directory with the same name.
				//bool bFindD = false;
				//for (int i = 0; i<ctrlDirectories.GetItemCount(); i++)
				//{
				//	if ( i != iSelected)
				//	{
				//		AutoArray<TCHAR> path(FULL_MAX_PATH);
				//		if ( ctrlDirectories.GetItemText(i, 0, path, FULL_MAX_PATH-1) )
				//		{
				//			if (virt.line.compare(path) == 0)
				//			{
				//				bFindD = true;
				//				break;
				//			}
				//		}				
				//	}
				//}
				//
				//if (bFindD)
				//{
				//	ctrlDirectories.Detach();
				//	MessageBox(CTSTRING(WIZARD_SHARE_ALREADYINDESC), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONERROR);
				//	return 0;
				//}
				
				ctrlDirectories.SetItemText(iSelected, 0, virt.line.c_str());
			}
		}
		ctrlDirectories.Detach();
		return 0;
	}

	int OnSetActive()
	{
		SetWizardButtons ( PSWIZB_BACK | PSWIZB_FINISH );
		return 0;
	}

	int OnWizardFinish()
	{
		ctrlDirectories.Attach(GetDlgItem(IDC_WIZARD_SHARE_LIST));
		try
		{
		// Save 
		CFlyDirItemArray directories;
		ShareManager::getDirectories(directories);

		CFlyDirItemArray newList;
		CFlyDirItemArray renameList;

		CFlyDirItemArray directoriesNew;
		const auto cnt = ctrlDirectories.GetItemCount();
		for (int i = 0; i<cnt; i++)
		{
			AutoArray<TCHAR> path(FULL_MAX_PATH);
			tstring desc;
			tstring target;
			if ( ctrlDirectories.GetItemText(i, 1, path, FULL_MAX_PATH-1) )
			{
				target = path;
			}
			if ( ctrlDirectories.GetItemText(i, 0, path, FULL_MAX_PATH-1) )
			{
				desc = path;
			}				
			CFlyDirItem pair(Text::fromT(desc), Text::fromT(target),0);
			directoriesNew.push_back(pair);
			bool bFound = false;
			for (auto j = directories.cbegin(); j != directories.cend(); ++j)
			{
				// found
				if (j->m_path.compare(pair.m_path) == 0 )
				{
					bFound = true;
					if (j->m_synonym.compare(pair.m_synonym) != 0)
						renameList.push_back( pair );
					break;
				}
			}
			if (!bFound)
				newList.push_back( pair );
		}

		for (auto i = directories.cbegin(); i != directories.cend(); ++i)
		{
			bool bFind = false;
			for (auto j = directoriesNew.cbegin(); j != directoriesNew.cend(); ++j)
			{
				if ( i->m_path.compare(j->m_path) == 0){
					bFind = true;
					break;
				}
			}	
			if (!bFind)
			{
				ShareManager::getInstance()->removeDirectory(i->m_path);
			}
		}

		//for (StringPairIter i = removeList.cbegin(); i != removeList.cend(); ++i)
		//{
		//	ShareManager::getInstance()->removeDirectory(i->second);
		//}
		// 2. Rename
		for (auto i = renameList.cbegin(); i != renameList.cend(); ++i)
		{
			ShareManager::getInstance()->renameDirectory(i->m_path, i->m_synonym);
		}
		// 3. Add new
		for (auto i = newList.cbegin(); i != newList.cend(); ++i)
		{
			CWaitCursor l_cursor_wait; //-V808
			ShareManager::getInstance()->addDirectory(i->m_path, i->m_synonym, true); 
		}

		ctrlDirectories.Detach();
#ifdef VIP
		static const int64_t l_minlim = 2I64 * 1024I64 * 1024I64 * 1024I64;
		if( ShareManager::getShareSize() < l_minlim )
		{
			string l_messages = STRING(WIZARD_SHARE_TO_SMALL);
			l_messages += " ";
			l_messages += STRING(MIN_SHARE);
			l_messages += " ";
			l_messages += Util::formatBytes(l_minlim).c_str();
			MessageBox(Text::toT(l_messages).c_str(), CTSTRING(WIZARD_TITLE), MB_OK | MB_ICONERROR );
			return TRUE;
		}
#endif
		return FALSE;       // allow deactivation
		}
		catch(Exception & e)
		{
			ctrlDirectories.Detach();
			::MessageBox(NULL,Text::toT(e.getError()).c_str(), _T("Share Wizard Error!"), MB_OK | MB_ICONERROR);
			return FALSE;       
		}
	}
	
	int OnKillActive()
	{
		return FALSE;
	}

	ExListViewCtrl ctrlDirectories; // [~] IRainman
	private:
		ExCImage img_f;	
};

#endif // SSA_WIZARD_FEATURE

#endif // _WZ_SHARE_H_