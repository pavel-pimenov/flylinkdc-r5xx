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
#include "libtorrent/announce_entry.hpp"
#include "libtorrent/hex.hpp"
#include "../windows/WinUtil.h"

#define WM_INIT_TORRENT_TREE WM_USER + 100

typedef std::vector< std::pair<tstring, tstring> > TStringPairArray;

#define FLYLINKDC_USE_RESIZE_DLG
class CFlyTorrentDialog :
	public CDialogImpl<CFlyTorrentDialog>
#ifdef FLYLINKDC_USE_RESIZE_DLG
    ,public CDialogResize<CFlyTorrentDialog>
#endif
{
	public:
		TStringPairArray m_FileInfo;
	private:
		CPropertyListCtrl m_torrentPropertyList;
        CTreeViewCtrl m_ctrlTree;
        CContainedWindow m_treeContainer;
        HTREEITEM m_htiRoot;
		std::shared_ptr<const libtorrent::torrent_info> m_ti;
	public:
		CFlyTorrentFileArray m_files;
		std::vector<libtorrent::download_priority_t> m_selected_files;
        enum { IDD = IDD_FLY_TORRENT_DIALOG };
        CFlyTorrentDialog(const CFlyTorrentFileArray& p_files, std::shared_ptr<const libtorrent::torrent_info> p_ti):
			 m_files(p_files),m_ti(p_ti), m_htiRoot(nullptr)
        {
        }
private:
		void addArray(const TCHAR* p_name, const TStringPairArray& p_array)
		{
			if (!p_array.empty())
			{
				m_torrentPropertyList.AddItem(PropCreateCategory(p_name));
				for (auto i = p_array.begin(); i != p_array.end(); ++i)
				{
					if (!i->second.empty())
						m_torrentPropertyList.AddItem(PropCreateSimple(i->first.c_str(), i->second.c_str()));
				}
			}
		}

		BEGIN_MSG_MAP(CFlyTorrentDialog)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_CLOSE, OnClose)
        MESSAGE_HANDLER(WM_INIT_TORRENT_TREE, onInitTorrentTree)
        NOTIFY_CODE_HANDLER(NM_CLICK, OnClick)

        COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_TORRENT_DOWNLOAD_DIR_CHANGE, onClickedBrowseDir)
#ifdef FLYLINKDC_USE_RESIZE_DLG
        REFLECT_NOTIFICATIONS()
        CHAIN_MSG_MAP(CDialogResize<CFlyTorrentDialog>)
#endif
		END_MSG_MAP()

#ifdef FLYLINKDC_USE_RESIZE_DLG
		BEGIN_DLGRESIZE_MAP(CFlyTorrentDialog)
		DLGRESIZE_CONTROL(IDC_FLY_TORRENT_INFO_LISTBOX, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_FLY_TORRENT_INFO_LISTBOX, DLSZ_MOVE_Y)
        DLGRESIZE_CONTROL(IDC_TORRENT_FILES, DLSZ_SIZE_X)
		DLGRESIZE_CONTROL(IDC_TORRENT_FILES, DLSZ_SIZE_Y)

		DLGRESIZE_CONTROL(IDCANCEL, DLSZ_MOVE_X | DLSZ_MOVE_Y)
		DLGRESIZE_CONTROL(IDOK, DLSZ_MOVE_X | DLSZ_MOVE_Y)		
		END_DLGRESIZE_MAP()
#endif
public:
		tstring m_dir;
private:
		tstring getDir()
		{
			m_dir.clear();
			m_dir.resize(::GetWindowTextLength(GetDlgItem(IDC_TORRENT_DOWNLOAD_DIR)));
			if (m_dir.size())
			{
				GetDlgItemText(IDC_TORRENT_DOWNLOAD_DIR, &m_dir[0], m_dir.size() + 1);
				AppendPathSeparator(m_dir);
				SetDlgItemText(IDC_TORRENT_DOWNLOAD_DIR, m_dir.c_str());
			}
			return m_dir;
		}

		LRESULT onClickedBrowseDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			tstring dir = getDir();
			if (dir.size())
			{
				if (WinUtil::browseDirectory(dir, m_hWnd))
				{
					AppendPathSeparator(dir);
					SetDlgItemText(IDC_TORRENT_DOWNLOAD_DIR, dir.c_str());
					m_dir = dir;
				}
			}
			return 0;
		}

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
				HTREEITEM l_item = m_ctrlTree.InsertItem(Text::toT(i->m_file_path + " (" + Util::formatBytes(i->m_size) + ")").c_str(), m_htiRoot, TVI_LAST);
				m_ctrlTree.SetCheckState(l_item, TRUE);
				m_ctrlTree.SetItemData(l_item, j++);
			}
			m_ctrlTree.Expand(m_htiRoot);

			// m_torrentPropertyList.m_bReadOnly = TRUE;
			TStringPairArray l_info;

			if (m_ti->creation_date() > 0) {
				char buf[50];
				auto l_time = m_ti->creation_date();
				tm* l_loc = localtime(&l_time);
				if (l_loc && strftime(buf, 21, "%Y-%m-%d %H:%M:%S", l_loc))
					l_info.push_back({ _T("Creation date"), Text::toT(buf) });
			}

			if (!m_ti->creator().empty()) l_info.push_back({ _T("Creator"), Text::toT(m_ti->creator()) });
			if (!m_ti->comment().empty()) l_info.push_back({ _T("Comment"), Text::toT(m_ti->comment()) });
			l_info.push_back({ _T("Info hash"), Text::toT(lt::aux::to_hex(m_ti->info_hash())) });
			addArray(_T("Info"), l_info);
			l_info.clear();

			for (auto const& n : m_ti->nodes())
				l_info.push_back({ Text::toT(n.first), Util::toStringT(n.second) });
			addArray(_T("Nodes"), l_info);
			l_info.clear();

			for (auto const& t : m_ti->trackers())
				l_info.push_back({ Text::toT(t.trackerid), Text::toT(t.url) });
			l_info.clear();
			addArray(_T("Trackers"), l_info);

			TStringPairArray l_web_sed_info;
			TStringPairArray l_http_sed_info;
			for (auto const& s : m_ti->web_seeds())
			{
			if (s.type == lt::web_seed_entry::url_seed)
			l_web_sed_info.push_back({ _T(""), Text::toT(s.url) });
			else if (s.type == lt::web_seed_entry::http_seed)
			l_http_sed_info.push_back({ _T(""), Text::toT(s.url) });
			}
			addArray(_T("web seed"), l_web_sed_info);
			addArray(_T("http seed"), l_http_sed_info);

            return 0;
        }
            
		LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
		{
			CenterWindow();
			m_torrentPropertyList.SubclassWindow(GetDlgItem(IDC_FLY_TORRENT_INFO_LISTBOX));
			m_torrentPropertyList.SetExtendedListStyle(PLS_EX_CATEGORIZED);
			m_torrentPropertyList.SetColumnWidth(100);
            
            //::EnableWindow(GetDlgItem(IDC_TORRENT_DOWNLOAD_DIR_CHANGE), FALSE);
            //::EnableWindow(GetDlgItem(IDC_TORRENT_DOWNLOAD_DIR), FALSE);
                
             m_ctrlTree.Attach(GetDlgItem(IDC_TORRENT_FILES));
             ::SetWindowText(GetDlgItem(IDC_TORRENT_DOWNLOAD_DIR), Text::toT(SETTING(DOWNLOAD_DIRECTORY)).c_str()); // TODO -
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
				getDir();
                m_selected_files.resize(m_files.size());
                for (HTREEITEM child = m_ctrlTree.GetChildItem(m_htiRoot); child != NULL; child = m_ctrlTree.GetNextSiblingItem(child))
                {
                    const int l_index = (int)m_ctrlTree.GetItemData(child);
                    const bool l_state = m_ctrlTree.GetCheckState(child);
					dcassert(l_index < m_selected_files.size());
					if (l_index < m_selected_files.size())
					{
						if (l_state)
							m_selected_files[l_index] = libtorrent::default_priority;
						else
							m_selected_files[l_index] = libtorrent::dont_download;
					}
                }
			}
			EndDialog(wID);
			return 0;
		}
};

#endif // !defined(CFLY_TORRENT_DLG_H)
