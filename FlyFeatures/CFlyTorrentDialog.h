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

#if !defined(CFLY_TORRENT_DLG_H)
#define CFLY_TORRENT_DLG_H


#include "PropertyList.h"

#define WM_INIT_TORRENT_TREE WM_USER + 100

//typedef std::vector< std::pair<tstring, tstring> > TStringPairArray;

#define FLYLINKDC_USE_RESIZE_DLG
class CFlyTorrentDialog :
	public CDialogImpl<CFlyTorrentDialog>
#ifdef FLYLINKDC_USE_RESIZE_DLG
    ,public CDialogResize<CFlyTorrentDialog>
#endif
{
	public:
//		TStringPairArray m_MediaInfo;
//		TStringPairArray m_FileInfo;
//		TStringPairArray m_MIInform;
	private:
		CPropertyListCtrl m_ctlList;
        CTreeViewCtrl m_ctrlTree;
        CContainedWindow m_treeContainer;
        HTREEITEM m_htiRoot;
		CFlyTorrentFileArray m_files;
	public:
        std::vector<int> m_selected_files;
        enum { IDD = IDD_FLY_TORRENT_DIALOG };
        CFlyTorrentDialog(const CFlyTorrentFileArray& p_files):m_files(p_files)
        {
        }
		BEGIN_MSG_MAP(CFlyTorrentDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_INIT_TORRENT_TREE, onInitTorrentTree)
        NOTIFY_CODE_HANDLER(NM_CLICK, OnClick)

        COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
#ifdef FLYLINKDC_USE_RESIZE_DLG
        REFLECT_NOTIFICATIONS()
        CHAIN_MSG_MAP(CDialogResize<CFlyTorrentDialog>)
#endif
		END_MSG_MAP()

#ifdef FLYLINKDC_USE_RESIZE_DLG
		BEGIN_DLGRESIZE_MAP(CFlyTorrentDialog)
		DLGRESIZE_CONTROL(IDC_FLY_TORRENT_INFO_LISTBOX, DLSZ_SIZE_X)
        DLGRESIZE_CONTROL(IDC_TORRENT_FILES, DLSZ_SIZE_X)
            
		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)		
		END_DLGRESIZE_MAP()
#endif

	private:	
		/*
        void addArray(const TCHAR* p_name, const TStringPairArray& p_array)
		{
			if (!p_array.empty())
			{
				m_ctlList.AddItem(PropCreateCategory(p_name));
				for (auto i = p_array.begin(); i != p_array.end(); ++i)
				{
					if (!i->second.empty())
						m_ctlList.AddItem(PropCreateSimple(i->first.c_str(), i->second.c_str()));
				}
			}
		}
        */
        LRESULT OnClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled)
        {
            const DWORD dwPos = GetMessagePos();
            POINT ptPos;
            ptPos.x = GET_X_LPARAM(dwPos);
            ptPos.y = GET_Y_LPARAM(dwPos);

            m_ctrlTree.ScreenToClient(&ptPos);

            UINT uFlags;
            HTREEITEM htItemClicked = m_ctrlTree.HitTest(ptPos, &uFlags);

            // if the item's checkbox was clicked ...
            if (uFlags & TVHT_ONITEMSTATEICON)
            {
                // TODO http://forums.codeguru.com/showthread.php?170637-How-to-run-a-very-long-SQL-statement
                if (htItemClicked == m_htiRoot)
                {
                    const bool l_state_root = m_ctrlTree.GetCheckState(m_htiRoot);
                    for (HTREEITEM child = m_ctrlTree.GetChildItem(m_htiRoot); child != NULL; child = m_ctrlTree.GetNextSiblingItem(child))
                    {
                        //const bool l_state = m_ctrlTree.GetCheckState(child);
                        m_ctrlTree.SetCheckState(child, !l_state_root);
                    }
                }
            }

            bHandled = FALSE;
            return 0;
        }
        LRESULT onInitTorrentTree(UINT, WPARAM, LPARAM, BOOL&)
        {
            m_ctrlTree.SetWindowLong(GWL_STYLE, m_ctrlTree.GetWindowLong(GWL_STYLE) | TVS_CHECKBOXES);
            m_htiRoot = m_ctrlTree.InsertItem(_T("Torrent files:"), TVI_ROOT, TVI_LAST);
            //m_ctrlTree.ModifyStyle(0, TVS_CHECKBOXES);
            m_ctrlTree.SetCheckState(m_htiRoot, TRUE);
            //SetChecked(m_htiRoot, true, 0);
            int j = 0;
            for (auto i = m_files.cbegin(); i != m_files.cend(); ++i)
            {
                HTREEITEM l_item = m_ctrlTree.InsertItem(Text::toT(i->first + " (" + Util::formatBytes(i->second) +")").c_str(), m_htiRoot, TVI_LAST);
                m_ctrlTree.SetCheckState(l_item, TRUE);
                m_ctrlTree.SetItemData(l_item, j++);
            }
            m_ctrlTree.Expand(m_htiRoot);
            //m_ctrlTree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, WS_EX_CLIENTEDGE, IDC_TORRENT_FILES);
            //WinUtil::SetWindowThemeExplorer(m_ctrlTree.m_hWnd);
            //m_ctrlTree.SetBkColor(Colors::g_bgColor);
            //m_ctrlTree.SetTextColor(Colors::g_textColor);

            //m_ctlList.m_bReadOnly = TRUE;
            //addArray(_T("FileInfo"), m_FileInfo);
            //addArray(_T("MediaInfo"), m_MediaInfo);
            return 0;
        }
            
		LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
		{
			CenterWindow();
			m_ctlList.SubclassWindow(GetDlgItem(IDC_FLY_TORRENT_INFO_LISTBOX));
			m_ctlList.SetExtendedListStyle(PLS_EX_CATEGORIZED);
			m_ctlList.SetColumnWidth(100);
            
            ::EnableWindow(GetDlgItem(IDC_TORRENT_DOWNLOAD_DIR_CHANGE), FALSE);
            ::EnableWindow(GetDlgItem(IDC_TORRENT_DOWNLOAD_DIR), FALSE);
                
             m_ctrlTree.Attach(GetDlgItem(IDC_TORRENT_FILES));
             ::SetWindowText(GetDlgItem(IDC_TORRENT_DOWNLOAD_DIR), Text::toT(SETTING(DOWNLOAD_DIRECTORY)).c_str());
#ifdef FLYLINKDC_USE_RESIZE_DLG
             DlgResize_Init();
#endif
             PostMessage(WM_INIT_TORRENT_TREE, 0, 0);
			return TRUE;
		}
		LRESULT OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			EndDialog(IDOK);
			return 0;
		}		
		LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& bHandled)
		{
			if (wID == IDOK)
			{
                m_selected_files.resize(m_files.size());
                for (HTREEITEM child = m_ctrlTree.GetChildItem(m_htiRoot); child != NULL; child = m_ctrlTree.GetNextSiblingItem(child))
                {
                    const int l_index = (int)m_ctrlTree.GetItemData(child);
                    const bool l_state = m_ctrlTree.GetCheckState(child);
                    if(l_state)
                        m_selected_files[l_index] = 4;
                    else
                        m_selected_files[l_index] = 0;
                }
			}
			EndDialog(wID);
			return 0;
		}
};

#endif // !defined(CFLY_TORRENT_DLG_H)
