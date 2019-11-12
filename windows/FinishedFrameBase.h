/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#if !defined(FINISHED_FRAME_BASE_H)
#define FINISHED_FRAME_BASE_H

#pragma once

#include "../client/DCPlusPlus.h"
#include "Resource.h"
#include "TypedListViewCtrl.h"
#include "ShellContextMenu.h"
#include "WinUtil.h"
#include "TextFrame.h"
#include "../client/ClientManager.h"
#include "../client/FinishedManager.h"
#include "../client/CFlylinkDBManager.h"

#define FINISHED_TREE_MESSAGE_MAP 11
#define FINISHED_LIST_MESSAGE_MAP 12

template<class T, int title, int id, int icon>
class FinishedFrameBase : public MDITabChildWindowImpl < T, RGB(0, 0, 0), icon >,
	public StaticFrame<T, title, id>,
	protected FinishedManagerListener,
	private SettingsManagerListener,
	public CSplitterImpl<FinishedFrameBase<T, title, id, icon> >
#ifdef _DEBUG
	, boost::noncopyable
#endif
{
		enum CFlyTreeNodeType
		{
			e_Root = -1,
			e_Current = -2,
			e_HistoryRoot = -3,
			e_HistoryDC = -4,
			e_HistoryTorrent = -5
		};
		
		eTypeTransfer m_transfer_type;
	protected:
		bool m_is_crrent_tree_node;
	public:
		typedef MDITabChildWindowImpl < T, RGB(0, 0, 0), icon > baseClass;
		
		FinishedFrameBase(eTypeTransfer p_transfer_type) :
			m_transfer_type(p_transfer_type),
			m_is_crrent_tree_node(false),
			m_totalBytes(0),
			m_totalActual(0),
			m_totalSpeed(0),
			m_totalCount(0),
			m_totalCountLast(0),
			m_type(FinishedManager::e_Download),
			m_treeContainer(WC_TREEVIEW, this, FINISHED_TREE_MESSAGE_MAP),
			m_listContainer(WC_LISTVIEW, this, FINISHED_LIST_MESSAGE_MAP)
		{
		}
		virtual ~FinishedFrameBase() { }
		
		BEGIN_MSG_MAP(T)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_REMOVE_TREE_ITEM, onRemoveTreeItem)
		COMMAND_ID_HANDLER(IDC_TOTAL, onRemove)
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
		COMMAND_ID_HANDLER(IDC_OPEN_FILE, onOpenFile)
		COMMAND_ID_HANDLER(IDC_REDOWNLOAD_FILE, onReDownload)
		COMMAND_ID_HANDLER(IDC_OPEN_FOLDER, onOpenFolder)
		COMMAND_ID_HANDLER(IDC_GETLIST, onGetList)
		COMMAND_ID_HANDLER(IDC_GRANTSLOT, onGrant)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TYPE, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_PATH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_SIZE, onCopy)
		COMMAND_ID_HANDLER(IDC_NETWORK_TRAFFIC, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_HUB_URL, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_SPEED, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_IP, onCopy)
		NOTIFY_HANDLER(id, LVN_GETDISPINFO, ctrlList.onGetDispInfo) // https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=47103 + http://www.flickr.com/photos/96019675@N02/11198858144/
		NOTIFY_HANDLER(id, LVN_COLUMNCLICK, ctrlList.onColumnClick)
		NOTIFY_HANDLER(id, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(id, NM_DBLCLK, onDoubleClick)
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		NOTIFY_HANDLER(id, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
#endif
		NOTIFY_HANDLER(IDC_TRANSFER_TREE, TVN_SELCHANGED, onSelChangedTree);
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(CSplitterImpl<FinishedFrameBase>)
		ALT_MSG_MAP(FINISHED_TREE_MESSAGE_MAP)
		ALT_MSG_MAP(FINISHED_LIST_MESSAGE_MAP)
		
		END_MSG_MAP()
		
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			string data;
			int i = -1;
			while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const auto l_item = ctrlList.getItemData(i);
				tstring sCopy;
				switch (wID)
				{
					case IDC_COPY_NICK:
						sCopy = l_item->getText(FinishedItem::COLUMN_NICK);
						break;
					case IDC_COPY_FILENAME:
						sCopy = l_item->getText(FinishedItem::COLUMN_FILE);
						break;
					case IDC_COPY_TYPE:
						sCopy = l_item->getText(FinishedItem::COLUMN_TYPE);
						break;
					case IDC_COPY_PATH:
						sCopy = l_item->getText(FinishedItem::COLUMN_PATH);
						break;
					case IDC_COPY_SIZE:
						sCopy = l_item->getText(FinishedItem::COLUMN_SIZE);
						break;
					case IDC_NETWORK_TRAFFIC:
						sCopy = l_item->getText(FinishedItem::COLUMN_NETWORK_TRAFFIC);
						break;
					case IDC_COPY_HUB_URL:
						sCopy = l_item->getText(FinishedItem::COLUMN_HUB);
						break;
					case IDC_COPY_TTH:
						sCopy = l_item->getText(FinishedItem::COLUMN_TTH);
						break;
					case IDC_COPY_SPEED:
						sCopy = l_item->getText(FinishedItem::COLUMN_SPEED);
						break;
					case IDC_COPY_IP:
						sCopy = l_item->getText(FinishedItem::COLUMN_IP);
						break;
					default:
						dcassert(0);
						dcdebug("Error copy\n");
						return 0;
				}
				if (!sCopy.empty())
				{
					if (data.empty())
						data = Text::fromT(sCopy);
					else
						data = data + "\r\n" + Text::fromT(sCopy);
				}
			}
			if (!data.empty())
			{
				WinUtil::setClipboard(data);
			}
			return 0;
		}
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
			m_ctrlStatus.Attach(m_hWndStatusBar);
			
			ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, id);
			SET_EXTENDENT_LIST_VIEW_STYLE(ctrlList);
			
			ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
			SET_LIST_COLOR(ctrlList);
			
			// Create listview columns
			WinUtil::splitTokens(columnIndexes, SettingsManager::get(columnOrder), FinishedItem::COLUMN_LAST);
			WinUtil::splitTokensWidth(columnSizes, SettingsManager::get(columnWidth), FinishedItem::COLUMN_LAST);
			static ResourceManager::Strings columnNames[] = { ResourceManager::FILENAME,
			                                                  ResourceManager::TYPE,
			                                                  ResourceManager::TIME,
			                                                  ResourceManager::PATH,
			                                                  ResourceManager::TTH,
			                                                  ResourceManager::NICK,
			                                                  ResourceManager::HUB,
			                                                  ResourceManager::SIZE,
			                                                  ResourceManager::SPEED,
			                                                  ResourceManager::IP_BARE,
			                                                  ResourceManager::NETWORK_TRAFFIC
			                                                };
			                                                
			BOOST_STATIC_ASSERT(_countof(columnSizes) == FinishedItem::COLUMN_LAST);
			BOOST_STATIC_ASSERT(_countof(columnNames) == FinishedItem::COLUMN_LAST);
			
			for (size_t j = 0; j < FinishedItem::COLUMN_LAST; j++) //-V104
			{
				const int fmt = (j == FinishedItem::COLUMN_SIZE || j == FinishedItem::COLUMN_SPEED || j == FinishedItem::COLUMN_NETWORK_TRAFFIC) ? LVCFMT_RIGHT : LVCFMT_LEFT; //-V104
				ctrlList.InsertColumn(j, TSTRING_I(columnNames[j]), fmt, columnSizes[j], j); //-V107
			}
			
			ctrlList.setColumnOrderArray(FinishedItem::COLUMN_LAST, columnIndexes); //-V106
			ctrlList.setVisible(SettingsManager::get(columnVisible));
			
			// ctrlList.setSortColumn(FinishedItem::COLUMN_DONE);
			ctrlList.setSortColumn(SETTING(FINISHED_DL_COLUMNS_SORT));
			ctrlList.setAscending(BOOLSETTING(FINISHED_DL_COLUMNS_SORT_ASC));
			
			UpdateLayout();
			
			ctxMenu.CreatePopupMenu();
			
			copyMenu.CreatePopupMenu();
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_TYPE, CTSTRING(TYPE));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_PATH, CTSTRING(PATH));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
			copyMenu.AppendMenu(MF_STRING, IDC_NETWORK_TRAFFIC, CTSTRING(NETWORK_TRAFFIC));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_HUB_URL, CTSTRING(HUB_ADDRESS));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_SPEED,      CTSTRING(SPEED));
			copyMenu.AppendMenu(MF_STRING, IDC_COPY_IP,      CTSTRING(IP));
			
			
			ctxMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY));
			ctxMenu.AppendMenu(MF_SEPARATOR);
//			ctxMenu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT));

			if (m_transfer_type == e_TransferDownload)
			{
				ctxMenu.AppendMenu(MF_STRING, IDC_REDOWNLOAD_FILE, CTSTRING(DOWNLOAD));
			}
			ctxMenu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN));
			
			ctxMenu.AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
			ctxMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT, CTSTRING(GRANT_EXTRA_SLOT));
			ctxMenu.AppendMenu(MF_STRING, IDC_GETLIST, CTSTRING(GET_FILE_LIST));
			ctxMenu.AppendMenu(MF_SEPARATOR);
			ctxMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
			ctxMenu.AppendMenu(MF_STRING, IDC_TOTAL, CTSTRING(REMOVE_ALL));
			ctxMenu.SetMenuDefaultItem(IDC_OPEN_FILE);
			
			m_ctrlTree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, WS_EX_CLIENTEDGE, IDC_TRANSFER_TREE);
			m_ctrlTree.SetBkColor(Colors::g_bgColor);
			m_ctrlTree.SetTextColor(Colors::g_textColor);
			WinUtil::SetWindowThemeExplorer(m_ctrlTree.m_hWnd);
			m_treeContainer.SubclassWindow(m_ctrlTree);
			SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
			SetSplitterPanes(m_ctrlTree.m_hWnd, ctrlList.m_hWnd);
			m_nProportionalPos = 2000; //SETTING(FLYSERVER_HUBLIST_SPLIT);
			g_TransferTreeImage.init();
			m_ctrlTree.SetImageList(g_TransferTreeImage.getIconList(), TVSIL_NORMAL);
			
			m_directoryMenu.CreatePopupMenu();
			m_directoryMenu.AppendMenu(MF_STRING, IDC_REMOVE_TREE_ITEM, CTSTRING(REMOVE));
			
			m_RootItem = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
			                                   m_transfer_type == e_TransferDownload ? _T("Download") : _T("Upload"),
			                                   2, // g_ISPImage.m_flagImageCount + 14, // nImage
			                                   2, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
			                                   0, // nState
			                                   0, // nStateMask
			                                   e_Root, // lParam
			                                   0, // aParent,
			                                   0  // hInsertAfter
			                                  );
			m_CurrentItem.createChild(m_RootItem, m_ctrlTree, e_Current, false);
			m_HistoryItem.createChild(m_RootItem, m_ctrlTree, e_HistoryRoot, true);
			{
				m_transfer_histogram.clear();
				m_tree_dc.clear();
				CFlylinkDBManager::getInstance()->load_transfer_historgam(false, m_transfer_type, m_transfer_histogram);
				int l_index = 0;
				for (auto i = m_transfer_histogram.cbegin(); i != m_transfer_histogram.cend(); ++i, ++l_index)
				{
					string l_caption = i->m_date + " (" + Util::toString(i->m_count) + ")";
					if (i->m_actual)
					{
						l_caption += " (" + Util::formatBytes(i->m_actual) + ")";
					}
					
					const auto l_tree_node = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
					                                               Text::toT(l_caption).c_str(),
					                                               1, // g_ISPImage.m_flagImageCount + 14, // nImage
					                                               1, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
					                                               0, // nState
					                                               0, // nStateMask
					                                               l_index, // lParam
					                                               m_HistoryItem.m_dc, // aParent,
					                                               0  // hInsertAfter
					                                              );
					m_tree_dc[l_tree_node] = l_index;
				}
			}
			{
				m_transfer_histogram_torrent.clear();
				m_tree_torrent.clear();
				CFlylinkDBManager::getInstance()->load_transfer_historgam(true, m_transfer_type, m_transfer_histogram_torrent);
				int l_index = 0;
				for (auto i = m_transfer_histogram_torrent.cbegin(); i != m_transfer_histogram_torrent.cend(); ++i, ++l_index)
				{
					string l_caption = i->m_date + " (" + Util::toString(i->m_count) + ")";
					if (i->m_file_size)
					{
						l_caption += " (" + Util::formatBytes(i->m_file_size) + ")";
					}
					const auto l_tree_node = m_ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
					                                               Text::toT(l_caption).c_str(),
					                                               0, // g_ISPImage.m_flagImageCount + 14, // nImage
					                                               0, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
					                                               0, // nState
					                                               0, // nStateMask
					                                               l_index, // lParam
					                                               m_HistoryItem.m_torrent, // aParent,
					                                               0  // hInsertAfter
					                                              );
					m_tree_torrent[l_tree_node] = l_index;
				}
			}
			// TODO - развернуть историю по датам
			m_ctrlTree.Expand(m_RootItem);
			m_ctrlTree.Expand(m_HistoryItem.m_root);
			m_ctrlTree.Expand(m_HistoryItem.m_dc);
			m_ctrlTree.Expand(m_HistoryItem.m_torrent);
			m_ctrlTree.SelectItem(m_CurrentItem.m_dc);
			
			SettingsManager::getInstance()->addListener(this);
			FinishedManager::getInstance()->addListener(this);
			
			bHandled = FALSE;
			return TRUE;
		}
		
		LRESULT onSelChangedTree(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
		{
			NMTREEVIEW* p = (NMTREEVIEW*)pnmh;
			m_is_crrent_tree_node = false;
			m_totalBytes = 0;
			m_totalActual = 0;
			m_totalSpeed = 0;
			m_totalCount = 0;
			if (p->itemNew.state & TVIS_SELECTED)
			{
				CWaitCursor l_cursor_wait; //-V808
				ctrlList.DeleteAllItems();
				if (p->itemNew.lParam == e_Current)
				{
					m_is_crrent_tree_node = true;
					updateList(FinishedManager::lockList(m_type));
					FinishedManager::unlockList(m_type);
				}
				else
				{
					const auto l_tree_node_dc = m_tree_dc.find(p->itemNew.hItem);
					if (l_tree_node_dc != m_tree_dc.end())
					{
						CFlylinkDBManager::getInstance()->load_transfer_history(false, m_transfer_type, m_transfer_histogram[l_tree_node_dc->second].m_date_as_int);
					}
					else
					{
						const auto l_tree_node_torrent = m_tree_torrent.find(p->itemNew.hItem);
						if (l_tree_node_torrent != m_tree_torrent.end())
						{
							CFlylinkDBManager::getInstance()->load_transfer_history(true, m_transfer_type, m_transfer_histogram_torrent[l_tree_node_torrent->second].m_date_as_int);
						}
					}
					/*
					if (p->itemNew.lParam == e_HistoryDC)
					                    {
					                        if (size_t(p->itemNew.lParam) < m_transfer_histogram.size())
					                        {
					                            CFlylinkDBManager::getInstance()->load_transfer_history(false, m_transfer_type, m_transfer_histogram[p->itemNew.lParam].m_date_as_int);
					                        }
					                    }
					                    else
					                        if (p->itemNew.lParam == e_HistoryTorrent)
					                        {
					                            if (size_t(p->itemNew.lParam) < m_transfer_histogram_torrent.size())
					                            {
					                                CFlylinkDBManager::getInstance()->load_transfer_history(true, m_transfer_type, m_transfer_histogram_torrent[p->itemNew.lParam].m_date_as_int);
					                            }
					                        }
					                        else
					                        {
					                            dcassert(0);
					                        }
					*/
				}
				ctrlList.resort();
				updateStatus();
			}
			return 0;
		}
		
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (!m_closed)
			{
				m_closed = true;
				FinishedManager::getInstance()->removeListener(this);
				SettingsManager::getInstance()->removeListener(this);
				
				WinUtil::setButtonPressed(id, false);
				PostMessage(WM_CLOSE);
				return 0;
			}
			else
			{
				ctrlList.saveHeaderOrder(columnOrder, columnWidth, columnVisible);
				SET_SETTING(FINISHED_DL_COLUMNS_SORT, ctrlList.getSortColumn());
				SET_SETTING(FINISHED_DL_COLUMNS_SORT_ASC, ctrlList.isAscending());
				//frame = NULL; [-] IRainman see template StaticFrame in WinUtil.h
				
				ctrlList.DeleteAndCleanAllItems(); // [!] IRainman
				
				bHandled = FALSE;
				return 0;
			}
		}
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onDoubleClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMITEMACTIVATE * const item = (NMITEMACTIVATE*) pnmh;
			
			if (item->iItem != -1)
			{
				const auto ii = ctrlList.getItemData(item->iItem);
				WinUtil::openFile(Text::toT(ii->m_entry->getTarget()));
			}
			return 0;
		}
		
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			switch (wParam)
			{
				case SPEAK_ADD_LINE: //  https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=77059
				{
					const FinishedItemPtr* l_entry = reinterpret_cast<FinishedItemPtr*>(lParam);
					addFinishedEntry(*l_entry, m_is_crrent_tree_node); // https://drdump.com/UploadedReport.aspx?DumpID=3064705 https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=110193 + http://www.flickr.com/photos/96019675@N02/11199325634/
					if ((*l_entry)->getID() == 0)
					{
						if (SettingsManager::get(boldFinished))
						{
							setDirty(1);
						}
					}
					delete l_entry;
					updateStatus();
				}
				break;
				case SPEAK_REMOVE_LINE:
				{
					if (m_is_crrent_tree_node)
					{
						const FinishedItemPtr* l_entry = reinterpret_cast<FinishedItemPtr*>(lParam);
						const int l_cnt = ctrlList.GetItemCount();
						for (int i = 0; i < l_cnt; ++i)
						{
							auto l_item = ctrlList.getItemData(i);
							if (l_item && l_item->m_entry == *l_entry)
							{
								ctrlList.DeleteItem(i);
								break;
							}
						}
						delete l_entry;
						updateStatus();
					}
				}
				break;
				case SPEAK_UPDATE_STATUS:
				{
					updateStatus();
					break;
				}
			}
			return 0;
		}
		LRESULT onRemoveTreeItem(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			if (m_ctrlTree.GetSelectedItem())
			{
				const auto l_tree_node_torrent = m_tree_torrent.find(m_ctrlTree.GetSelectedItem());
				if (l_tree_node_torrent != m_tree_torrent.end())
				{
					SendMessage(WM_COMMAND, IDC_TOTAL);
					m_ctrlTree.DeleteItem(l_tree_node_torrent->first);
				}
				else
				{
					const auto l_tree_node_dc = m_tree_dc.find(m_ctrlTree.GetSelectedItem());
					if (l_tree_node_dc != m_tree_dc.end())
					{
						SendMessage(WM_COMMAND, IDC_TOTAL);
						m_ctrlTree.DeleteItem(l_tree_node_dc->first);
					}
				}
			}
			return 0;
		}
		
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			switch (wID)
			{
				case IDC_REMOVE:
				{
					int i = -1, p = -1;
					while ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
					{
						const auto ii = ctrlList.getItemData(i);
						if (!m_is_crrent_tree_node)
						{
							vector<__int64> l_id;
							l_id.push_back(ii->m_entry->getID());
							if (ii->m_entry->is_torrent())
							{
								CFlylinkDBManager::getInstance()->delete_transfer_history_torrent(l_id);
							}
							else
							{
								CFlylinkDBManager::getInstance()->delete_transfer_history(l_id);
							}
						}
						else
						{
							m_totalSpeed -= ii->m_entry->getAvgSpeed();
							FinishedManager::removeItem(ii->m_entry, m_type);
						}
						m_totalBytes -= ii->m_entry->getSize();
						m_totalActual -= ii->m_entry->getActual();
						m_totalCount--;
						ctrlList.DeleteItem(i);
						p = i;
					}
					ctrlList.SelectItem((p < ctrlList.GetItemCount() - 1) ? p : ctrlList.GetItemCount() - 1);
					updateStatus();
					break;
				}
				case IDC_TOTAL:
				{
					CWaitCursor l_cursor_wait; //-V808
					if (!m_is_crrent_tree_node)
					{
						const int l_cnt = ctrlList.GetItemCount();
						vector<__int64> l_id;
						vector<__int64> l_id_torrent;
						l_id.reserve(l_cnt);
						for (int i = 0; i < l_cnt; ++i)
						{
							const auto ii = ctrlList.getItemData(i);
							if (ii->m_entry->is_torrent())
								l_id_torrent.push_back(ii->m_entry->getID());
							else
								l_id.push_back(ii->m_entry->getID());
						}
						CFlylinkDBManager::getInstance()->delete_transfer_history_torrent(l_id_torrent);
						CFlylinkDBManager::getInstance()->delete_transfer_history(l_id);
					}
					else
					{
						FinishedManager::removeAll(m_type);
					}
					ctrlList.DeleteAndCleanAllItems(); // [!] IRainman
					m_totalBytes = 0;
					m_totalActual = 0;
					m_totalSpeed = 0;
					m_totalCount = 0;
					updateStatus();
					break;
				}
			}
			return 0;
		}
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i;
			if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				const auto ii = ctrlList.getItemData(i);
				if (ii != NULL)
					TextFrame::openWindow(Text::toT(ii->m_entry->getTarget()));
			}
			return 0;
		}
#endif
		LRESULT onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i;
			if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				const auto ii = ctrlList.getItemData(i);
				if (ii)
				{
					WinUtil::openFile(Text::toT(ii->m_entry->getTarget()));
				}
			}
			return 0;
		}
		LRESULT onReDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i = -1;
			while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const auto ii = ctrlList.getItemData(i);
				if (ii)
				{
					if (ii->m_entry->getTTH() != TTHValue() && !File::isExist(ii->m_entry->getTarget()))
					{
						const UserPtr l_user = ClientManager::findLegacyUser(ii->m_entry->getNick(), ii->m_entry->getHub());
						try
						{
							QueueManager::getInstance()->add(0, ii->m_entry->getTarget(), ii->m_entry->getSize(), ii->m_entry->getTTH(), l_user, 0, false, true);
						}
						catch (const Exception& e)
						{
							//fix https://drdump.com/Problem.aspx?ProblemID=226879
							LogManager::message("QueueManager::getInstance()->add Error = " + e.getError());
						}
					}
				}
			}
			return 0;
		}
		LRESULT onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i;
			if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				const auto ii = ctrlList.getItemData(i);
				if (ii)
				{
					WinUtil::openFolder(Text::toT(ii->m_entry->getTarget()));
				}
			}
			return 0;
		}
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			if (reinterpret_cast<HWND>(wParam) == ctrlList && ctrlList.GetSelectedCount() > 0)
			{
				POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				
				if (pt.x == -1 && pt.y == -1)
				{
					WinUtil::getContextMenuPos(ctrlList, pt);
				}
				
				bool bShellMenuShown = false;
				if (BOOLSETTING(SHOW_SHELL_MENU) &&
				        ctrlList.GetSelectedCount() == 1)
				{
					const auto l_index = ctrlList.GetSelectedIndex();
					const auto* l_item_data = ctrlList.getItemData(l_index);
					if (l_item_data && l_item_data->m_entry)
					{
						const string path = l_item_data->m_entry->getTarget();
						if (File::isExist(path))
						{
							CShellContextMenu shellMenu;
							shellMenu.SetPath(Text::toT(path));
							
							CMenu* pShellMenu = shellMenu.GetMenu();
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
							pShellMenu->AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT));
#endif
							pShellMenu->AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
							pShellMenu->AppendMenu(MF_SEPARATOR);
							pShellMenu->AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
							pShellMenu->AppendMenu(MF_STRING, IDC_TOTAL, CTSTRING(REMOVE_ALL));
							pShellMenu->AppendMenu(MF_SEPARATOR);
							
							UINT idCommand = shellMenu.ShowContextMenu(m_hWnd, pt);
							if (idCommand != 0)
								PostMessage(WM_COMMAND, idCommand);
								
							bShellMenuShown = true;
						}
					}
				}
				
				if (!bShellMenuShown)
				{
					ctxMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
				}
				
				return TRUE;
			}
			else if (reinterpret_cast<HWND>(wParam) == m_ctrlTree && m_ctrlTree.GetSelectedItem() != NULL)
			{
				POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				
				if (pt.x == -1 && pt.y == -1)
				{
					WinUtil::getContextMenuPos(m_ctrlTree, pt);
				}
				else
				{
					m_ctrlTree.ScreenToClient(&pt);
					UINT a = 0;
					HTREEITEM ht = m_ctrlTree.HitTest(pt, &a);
					if (ht != NULL && ht != m_ctrlTree.GetSelectedItem())
						m_ctrlTree.SelectItem(ht);
					m_ctrlTree.ClientToScreen(&pt);
				}
				
				m_directoryMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
				return TRUE;
			}
			
			bHandled = FALSE;
			return FALSE;
		}
		
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLVKEYDOWN* kd = reinterpret_cast<NMLVKEYDOWN*>(pnmh);
			
			if (kd->wVKey == VK_DELETE)
			{
				PostMessage(WM_COMMAND, IDC_REMOVE);
			}
			return 0;
		}
		
		LRESULT onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i;
			if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				if (const auto ii = ctrlList.getItemData(i))
				{
					const UserPtr u = ClientManager::findUser(ii->m_entry->getCID());
					if (u && u->isOnline())
					{
						try // [+] IRainman fix done: [4] https://www.box.net/shared/0ac062dcc56424091537
						{
							QueueManager::getInstance()->addList(HintedUser(u, ii->m_entry->getHub()), QueueItem::FLAG_CLIENT_VIEW);
						}
						catch (const Exception& e)
						{
							addStatusLine(Text::toT(e.getError()));
						}
					}
					else
					{
						addStatusLine(TSTRING(USER_OFFLINE));
					}
				}
				else
				{
					addStatusLine(TSTRING(USER_OFFLINE));
				}
			}
			return 0;
		}
		
		LRESULT onGrant(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i;
			if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				if (const auto ii = ctrlList.getItemData(i))
				{
					const UserPtr u = ClientManager::findUser(ii->m_entry->getCID());
					if (u && u->isOnline())
					{
						UploadManager::getInstance()->reserveSlot(HintedUser(u, ii->m_entry->getHub()), 600);
					}
					else
					{
						addStatusLine(TSTRING(USER_OFFLINE));
					}
				}
				else
				{
					addStatusLine(TSTRING(USER_OFFLINE));
				}
			}
			return 0;
		}
		
		void UpdateLayout(BOOL bResizeBars = TRUE)
		{
			RECT rect;
			GetClientRect(&rect);
			
			// position bars and offset their dimensions
			UpdateBarsPosition(rect, bResizeBars);
			
			if (m_ctrlStatus.IsWindow())
			{
				CRect sr;
				int w[5];
				m_ctrlStatus.GetClientRect(sr);
				w[4] = sr.right - 16;
				w[3] = max(w[4] - 100, 0);
				w[2] = max(w[3] - 100, 0);
				w[1] = max(w[2] - 100, 0);
				w[0] = max(w[1] - 100, 0);
				
				m_ctrlStatus.SetParts(5, w);
			}
			
			CRect rc(rect);
			ctrlList.MoveWindow(rc);
			SetSplitterRect(&rect);
		}
		
		LRESULT onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /* bHandled */)
		{
			ctrlList.SetFocus();
			return 0;
		}
		
	protected:
		enum
		{
			SPEAK_ADD_LINE,
			SPEAK_REMOVE_LINE, // [+] IRainman
			SPEAK_UPDATE_STATUS
			/* TODO
			SPEAK_REMOVE,
			SPEAK_REMOVE_ALL,
			*/
		};
		
		class FinishedItemInfo
		{
			public:
				FinishedItemInfo(const FinishedItemPtr& fi) : m_entry(fi)
				{
					for (size_t i = FinishedItem::COLUMN_FIRST; i < FinishedItem::COLUMN_LAST; ++i) //-V104
					{
						m_columns[i] = fi->getText(i); //-V107
					}
				}
				const tstring& getText(int col) const
				{
					dcassert(col >= 0 && col < FinishedItem::COLUMN_LAST);
					return m_columns[col];
				}
				static int compareItems(const FinishedItemInfo* a, const FinishedItemInfo* b, int col)
				{
					return FinishedItem::compareItems(a->m_entry.get(), b->m_entry.get(), col);
				}
				
				int getImageIndex() const
				{
					return g_fileImage.getIconIndex(m_entry->getTarget());
				}
				static uint8_t getStateImageIndex()
				{
					return 0;
				}
			public:
				FinishedItemPtr m_entry;
			private:
				tstring m_columns[FinishedItem::COLUMN_LAST];
		};
		
		
		CStatusBarCtrl m_ctrlStatus;
		CMenu ctxMenu;
		CMenu copyMenu;
		
		OMenu m_directoryMenu;
		
		TypedListViewCtrl<FinishedItemInfo, id> ctrlList;
		CContainedWindow        m_listContainer;
		
		CFlyTransferHistogramArray m_transfer_histogram;
		CFlyTransferHistogramArray m_transfer_histogram_torrent;
		std::unordered_map<HTREEITEM, unsigned> m_tree_dc;
		std::unordered_map<HTREEITEM, unsigned> m_tree_torrent;
		CTreeViewCtrl           m_ctrlTree;
		CContainedWindow        m_treeContainer;
		HTREEITEM               m_RootItem;
		class CFlyTreeItem
		{
			public:
				HTREEITEM               m_root;
				HTREEITEM               m_dc;
				HTREEITEM               m_torrent;
				CFlyTreeItem() : m_root(nullptr), m_dc(nullptr), m_torrent(nullptr)
				{
				}
				void createChild(HTREEITEM p_root, CTreeViewCtrl& p_tree, CFlyTreeNodeType p_type_node, bool p_is_add_torrent)
				{
					m_root = p_tree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
					                           p_type_node == e_Current ? _T("Current session (RAM)") : _T("History (SQLite)"),
					                           3, // g_ISPImage.m_flagImageCount + 14, // nImage
					                           3, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
					                           0, // nState
					                           0, // nStateMask
					                           p_type_node, // lParam
					                           p_root, // aParent,
					                           0  // hInsertAfter
					                          );
					m_dc = p_tree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
					                         _T("DC++"),
					                         1, // g_ISPImage.m_flagImageCount + 14, // nImage
					                         1, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
					                         0, // nState
					                         0, // nStateMask
					                         p_type_node == e_HistoryRoot ? e_HistoryDC : p_type_node, // lParam
					                         m_root, // aParent,
					                         0  // hInsertAfter
					                        );
					if (p_is_add_torrent)
					{
						m_torrent = p_tree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
						                              _T("Torrent"),
						                              0, // g_ISPImage.m_flagImageCount + 14, // nImage
						                              0, // g_ISPImage.m_flagImageCount + 14, // nSelectedImage
						                              0, // nState
						                              0, // nStateMask
						                              p_type_node == e_HistoryRoot ? e_HistoryTorrent : p_type_node, // lParam
						                              m_root, // aParent,
						                              0  // hInsertAfter
						                             );
					}
				}
		};
		CFlyTreeItem            m_CurrentItem;
		CFlyTreeItem            m_HistoryItem;
		
		int64_t m_totalBytes;
		int64_t m_totalActual;
		int64_t m_totalSpeed;
		int64_t m_totalCount;
		int64_t m_totalCountLast;
		
		FinishedManager::eType m_type;
		SettingsManager::IntSetting boldFinished;
		SettingsManager::StrSetting columnWidth;
		SettingsManager::StrSetting columnOrder;
		SettingsManager::StrSetting columnVisible;
		
		
		static int columnSizes[FinishedItem::COLUMN_LAST];
		static int columnIndexes[FinishedItem::COLUMN_LAST];
		
		void addStatusLine(const tstring& aLine)
		{
			m_ctrlStatus.SetText(0, (Text::toT(Util::getShortTimeString()) + _T(' ') + aLine).c_str());
		}
		
		void updateStatus()
		{
			if (m_totalCountLast != m_totalCount)
			{
				m_totalCountLast = m_totalCount;
				m_ctrlStatus.SetText(1, (Util::toStringW(m_totalCount) + _T(' ') + TSTRING(ITEMS)).c_str());
				m_ctrlStatus.SetText(2, Util::formatBytesW(m_totalBytes).c_str());
				m_ctrlStatus.SetText(3, Util::formatBytesW(m_totalActual).c_str());
				m_ctrlStatus.SetText(4, (Util::formatBytesW(m_totalCount > 0 ? m_totalSpeed / m_totalCount : 0) + _T('/') + WSTRING(S)).c_str());
				setCountMessages(m_totalCount);
			}
		}
		
		void updateList(const FinishedItemList& fl)
		{
			CLockRedraw<true> l_lock_draw(ctrlList);
			for (auto i = fl.cbegin(); i != fl.cend(); ++i)
			{
				addFinishedEntry(*i, false);
			}
			updateStatus();
		}
		
		void addFinishedEntry(const FinishedItemPtr& p_entry, bool p_ensure_visible)
		{
			const auto ii = new FinishedItemInfo(p_entry);
			m_totalBytes += p_entry->getSize();
			m_totalActual += p_entry->getActual();
			m_totalSpeed += p_entry->getAvgSpeed();
			m_totalCount++;
			const int loc = ctrlList.insertItem(ii, I_IMAGECALLBACK); // ii->getImageIndex() // fix I_IMAGECALLBACK https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=47103
			if (p_ensure_visible)
			{
				ctrlList.EnsureVisible(loc, FALSE);
			}
		}
		void on(UpdateStatus) noexcept override
		{
			SendMessage(WM_SPEAKER, SPEAK_UPDATE_STATUS, (WPARAM)nullptr);
		}
		void on(SettingsManagerListener::Repaint) override
		{
			dcassert(!ClientManager::isBeforeShutdown());
			if (!ClientManager::isBeforeShutdown())
			{
				if (ctrlList.isRedraw())
				{
					RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
				}
			}
		}
		
};

template <class T, int title, int id, int icon>
int FinishedFrameBase<T, title, id, icon>::columnIndexes[] = { FinishedItem::COLUMN_DONE,
                                                               FinishedItem::COLUMN_FILE,
                                                               FinishedItem::COLUMN_TYPE,
                                                               FinishedItem::COLUMN_PATH,
                                                               FinishedItem::COLUMN_TTH,
                                                               FinishedItem::COLUMN_NICK,
                                                               FinishedItem::COLUMN_HUB,
                                                               FinishedItem::COLUMN_SIZE,
                                                               FinishedItem::COLUMN_NETWORK_TRAFFIC,
                                                               FinishedItem::COLUMN_SPEED,
                                                               FinishedItem::COLUMN_IP
                                                             };

template <class T, int title, int id, int icon>
int FinishedFrameBase<T, title, id, icon>::columnSizes[] = { 100, 110, 20, 290, 125, 80, 80, 80, 80, 80};


#endif // !defined(FINISHED_FRAME_BASE_H)

/**
* @file
* $Id: FinishedFrameBase.h 568 2011-07-24 18:28:43Z bigmuscle $
*/
