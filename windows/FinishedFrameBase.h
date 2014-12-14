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

#if !defined(FINISHED_FRAME_BASE_H)
#define FINISHED_FRAME_BASE_H

#pragma once

#include "../client/DCPlusPlus.h"
#include "Resource.h"
#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "ShellContextMenu.h"
#include "WinUtil.h"
#include "TextFrame.h"
#include "../client/ClientManager.h"
#include "../client/FinishedManager.h"

template<class T, int title, int id, int icon>
class FinishedFrameBase : public MDITabChildWindowImpl < T, RGB(0, 0, 0), icon > , public StaticFrame<T, title, id>,
	protected FinishedManagerListener, private SettingsManagerListener
#ifdef _DEBUG
	, boost::noncopyable
#endif
{
	public:
		typedef MDITabChildWindowImpl < T, RGB(0, 0, 0), icon > baseClass;
		
		FinishedFrameBase() : totalBytes(0), totalSpeed(0), m_type(FinishedManager::e_Download) { }
		virtual ~FinishedFrameBase() { }
		
		BEGIN_MSG_MAP(T)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_TOTAL, onRemove)
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
		COMMAND_ID_HANDLER(IDC_OPEN_FILE, onOpenFile)
		COMMAND_ID_HANDLER(IDC_OPEN_FOLDER, onOpenFolder)
		COMMAND_ID_HANDLER(IDC_GETLIST, onGetList)
		COMMAND_ID_HANDLER(IDC_GRANTSLOT, onGrant)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		NOTIFY_HANDLER(id, LVN_GETDISPINFO, ctrlList.onGetDispInfo) // https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=47103 + http://www.flickr.com/photos/96019675@N02/11198858144/
		NOTIFY_HANDLER(id, LVN_COLUMNCLICK, ctrlList.onColumnClick)
		NOTIFY_HANDLER(id, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(id, NM_DBLCLK, onDoubleClick)
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		NOTIFY_HANDLER(id, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
#endif
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
			ctrlStatus.Attach(m_hWndStatusBar);
			
			ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | LVS_SINGLESEL, WS_EX_CLIENTEDGE, id);
			SET_EXTENDENT_LIST_VIEW_STYLE(ctrlList);
			
			ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
			SET_LIST_COLOR(ctrlList);
			
			// Create listview columns
			WinUtil::splitTokens(columnIndexes, SettingsManager::get(columnOrder), FinishedItem::COLUMN_LAST);
			WinUtil::splitTokensWidth(columnSizes, SettingsManager::get(columnWidth), FinishedItem::COLUMN_LAST);
			
			BOOST_STATIC_ASSERT(_countof(columnSizes) == FinishedItem::COLUMN_LAST);
			BOOST_STATIC_ASSERT(_countof(columnNames) == FinishedItem::COLUMN_LAST);
			
			for (size_t j = 0; j < FinishedItem::COLUMN_LAST; j++) //-V104
			{
				const int fmt = (j == FinishedItem::COLUMN_SIZE || j == FinishedItem::COLUMN_SPEED) ? LVCFMT_RIGHT : LVCFMT_LEFT; //-V104
				ctrlList.InsertColumn(j, TSTRING_I(columnNames[j]), fmt, columnSizes[j], j); //-V107
			}
			
			ctrlList.setColumnOrderArray(FinishedItem::COLUMN_LAST, columnIndexes); //-V106
			ctrlList.setVisible(SettingsManager::get(columnVisible));
			
			// ctrlList.setSortColumn(FinishedItem::COLUMN_DONE);
			ctrlList.setSortColumn(SETTING(FINISHED_DL_COLUMNS_SORT));
			ctrlList.setAscending(BOOLSETTING(FINISHED_DL_COLUMNS_SORT_ASC));
			
			UpdateLayout();
			
			SettingsManager::getInstance()->addListener(this);
			FinishedManager::getInstance()->addListener(this);
			updateList(FinishedManager::getInstance()->lockList(m_type));
			FinishedManager::getInstance()->unlockList(m_type);
			
			ctxMenu.CreatePopupMenu();
//			ctxMenu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT));
			ctxMenu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN));
			ctxMenu.AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER));
			ctxMenu.AppendMenu(MF_STRING, IDC_GRANTSLOT, CTSTRING(GRANT_EXTRA_SLOT));
			ctxMenu.AppendMenu(MF_STRING, IDC_GETLIST, CTSTRING(GET_FILE_LIST));
			ctxMenu.AppendMenu(MF_SEPARATOR);
			ctxMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
			ctxMenu.AppendMenu(MF_STRING, IDC_TOTAL, CTSTRING(REMOVE_ALL));
			ctxMenu.SetMenuDefaultItem(IDC_OPEN_FILE);
			
			bHandled = FALSE;
			return TRUE;
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
				ItemInfo *ii = ctrlList.getItemData(item->iItem);
				WinUtil::openFile(Text::toT(ii->entry->getTarget()));
			}
			return 0;
		}
		
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			switch (wParam)
			{
				case SPEAK_ADD_LINE: //  https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=77059
				{
					FinishedItem* entry = reinterpret_cast<FinishedItem*>(lParam);
					addFinishedEntry(entry); // https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=110193 + http://www.flickr.com/photos/96019675@N02/11199325634/
					if (SettingsManager::get(boldFinished))
					{
						setDirty(1);
					}
					updateStatus();
				}
				break;
				case SPEAK_REMOVE_LINE: // [+] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=601
				{
					FinishedItem* entry = reinterpret_cast<FinishedItem*>(lParam);
					const int l_cnt = ctrlList.GetItemCount();
					for (int i = 0; i < l_cnt; ++i)
					{
						auto l_item = ctrlList.getItemData(i);
						if (l_item && l_item->entry == entry)
						{
							ctrlList.DeleteItem(i);
							delete l_item;
							break;
						}
					}
					updateStatus();
				}
				break;
				break;
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
						ItemInfo *ii = ctrlList.getItemData(i);
						totalBytes -= ii->entry->getSize();
						totalSpeed -= ii->entry->getAvgSpeed();
						FinishedManager::getInstance()->removeItem(ii->entry, m_type);
						safe_delete(ii->entry);
						ctrlList.DeleteItem(i);
						delete ii;
						p = i;
					}
					ctrlList.SelectItem((p < ctrlList.GetItemCount() - 1) ? p : ctrlList.GetItemCount() - 1);
					updateStatus();
					break;
				}
				case IDC_TOTAL:
					ctrlList.DeleteAndCleanAllItems(); // [!] IRainman
					FinishedManager::getInstance()->removeAll(m_type);
					totalBytes = 0;
					totalSpeed = 0;
					updateStatus();
					break;
			}
			return 0;
		}
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i;
			if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				ItemInfo *ii = ctrlList.getItemData(i);
				if (ii != NULL)
					TextFrame::openWindow(Text::toT(ii->entry->getTarget()));
			}
			return 0;
		}
#endif
		LRESULT onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i;
			if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				ItemInfo *ii = ctrlList.getItemData(i);
				if (ii != NULL)
					WinUtil::openFile(Text::toT(ii->entry->getTarget()));
			}
			return 0;
		}
//[+]  ZagZag  http://iceberg.leschat.net/forum/index.php?showtopic=265&view=findpost&p=66879
		LRESULT onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			int i;
			if ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				ItemInfo *ii = ctrlList.getItemData(i);
				if (ii)
				{
					WinUtil::openFolder(Text::toT(ii->entry->getTarget()));
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
				if (BOOLSETTING(SHOW_SHELL_MENU) && (ctrlList.GetSelectedCount() == 1))
				{
					const string path = ((FinishedItem*)ctrlList.GetItemData(ctrlList.GetSelectedIndex()))->getTarget();
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
				
				if (!bShellMenuShown)
					ctxMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
					
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
				if (const ItemInfo *ii = ctrlList.getItemData(i))
				{
					const UserPtr u = ClientManager::findUser(ii->entry->getCID());
					if (u && u->isOnline())
					{
						try // [+] IRainman fix done: [4] https://www.box.net/shared/0ac062dcc56424091537
						{
							QueueManager::getInstance()->addList(HintedUser(u, ii->entry->getHub()), QueueItem::FLAG_CLIENT_VIEW);
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
				if (const ItemInfo *ii = ctrlList.getItemData(i))
				{
					const UserPtr u = ClientManager::findUser(ii->entry->getCID());
					if (u && u->isOnline())
					{
						UploadManager::getInstance()->reserveSlot(HintedUser(u, ii->entry->getHub()), 600);
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
			
			if (ctrlStatus.IsWindow())
			{
				CRect sr;
				int w[4];
				ctrlStatus.GetClientRect(sr);
				w[3] = sr.right - 16;
				w[2] = max(w[3] - 100, 0);
				w[1] = max(w[2] - 100, 0);
				w[0] = max(w[1] - 100, 0);
				
				ctrlStatus.SetParts(4, w);
			}
			
			CRect rc(rect);
			ctrlList.MoveWindow(rc);
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
			/* TODO
			SPEAK_REMOVE,
			SPEAK_REMOVE_ALL,
			*/
		};
		
		class ItemInfo
		{
			public:
				ItemInfo(FinishedItem* fi) : entry(fi)
				{
					for (size_t i = FinishedItem::COLUMN_FIRST; i < FinishedItem::COLUMN_LAST; ++i) //-V104
						columns[i] = fi->getText(i); //-V107
				}
				tstring columns[FinishedItem::COLUMN_LAST];
				
				const tstring& getText(int col) const
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					// [-] IRainman fix: It's not even funny. This is sad. :(
					// [-] if (col >= COLUMN_LAST || col < 0)
					// [-]  return Util::emptyStringT; // TODO Log
					// [-] else
					return columns[col];
				}
				
				const tstring& copy(int col)
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					// [-] IRainman fix: It's not even funny. This is sad. :(
					// [-] if (col >= 0 && col < COLUMN_LAST)
					return getText(col);
					// [-] else
					// [-] return Util::emptyStringT;
				}
				
				static int compareItems(const ItemInfo* a, const ItemInfo* b, int col)
				{
					return FinishedItem::compareItems(a->entry, b->entry, col);
				}
				
				int getImageIndex() const
				{
					return g_fileImage.getIconIndex(entry->getTarget());
				}
				static uint8_t getStateImageIndex()
				{
					return 0;
				}
				
				FinishedItem* entry;
		};
		
		CStatusBarCtrl ctrlStatus;
		CMenu ctxMenu;
		
		TypedListViewCtrl<ItemInfo, id> ctrlList;
		
		int64_t totalBytes;
		int64_t totalSpeed;
		
		FinishedManager::eType m_type;
		SettingsManager::IntSetting boldFinished;
		SettingsManager::StrSetting columnWidth;
		SettingsManager::StrSetting columnOrder;
		SettingsManager::StrSetting columnVisible;
		
		
		static int columnSizes[FinishedItem::COLUMN_LAST];
		static int columnIndexes[FinishedItem::COLUMN_LAST];
		
		void addStatusLine(const tstring& aLine)
		{
			ctrlStatus.SetText(0, (Text::toT(Util::getShortTimeString()) + _T(' ') + aLine).c_str());
		}
		
		void updateStatus()
		{
			const int l_listCount = ctrlList.GetItemCount();
			ctrlStatus.SetText(1, (Util::toStringW(l_listCount) + _T(' ') + TSTRING(ITEMS)).c_str());
			ctrlStatus.SetText(2, Util::formatBytesW(totalBytes).c_str());
			ctrlStatus.SetText(3, (Util::formatBytesW(l_listCount > 0 ? totalSpeed / l_listCount : 0) + _T('/') + WSTRING(S)).c_str());
			setCountMessages(l_listCount);
		}
		
		void updateList(const FinishedItemList& fl)
		{
			CLockRedraw<true> l_lock_draw(ctrlList);
			for (auto i = fl.cbegin(); i != fl.cend(); ++i)
			{
				addFinishedEntry(*i);
			}
			updateStatus();
		}
		
		void addFinishedEntry(FinishedItem* p_entry)
		{
			const ItemInfo *ii = new ItemInfo(p_entry);
			totalBytes += p_entry->getSize();
			totalSpeed += p_entry->getAvgSpeed();
			const int loc = ctrlList.insertItem(ii, ii->getImageIndex()); // fix I_IMAGECALLBACK https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=47103
			ctrlList.EnsureVisible(loc, FALSE);
		}
		
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/)
		{
			dcassert(!ClientManager::isShutdown());
			if (!ClientManager::isShutdown())
			{
				if (ctrlList.isRedraw())
				{
					RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
				}
			}
		}
		
};

template <class T, int title, int id, int icon>
int FinishedFrameBase<T, title, id, icon>::columnIndexes[] = { FinishedItem::COLUMN_DONE, FinishedItem::COLUMN_FILE,
                                                               FinishedItem::COLUMN_PATH, FinishedItem::COLUMN_NICK, FinishedItem::COLUMN_HUB, FinishedItem::COLUMN_SIZE, FinishedItem::COLUMN_SPEED,
                                                               FinishedItem::COLUMN_IP
                                                             };

template <class T, int title, int id, int icon>
int FinishedFrameBase<T, title, id, icon>::columnSizes[] = { 100, 110, 290, 125, 80, 80, 80
                                                             , 80
                                                           };
static ResourceManager::Strings columnNames[] = { ResourceManager::FILENAME, ResourceManager::TIME, ResourceManager::PATH,
                                                  ResourceManager::NICK, ResourceManager::HUB, ResourceManager::SIZE, ResourceManager::SPEED,
                                                  ResourceManager::IP_BARE
                                                };

#endif // !defined(FINISHED_FRAME_BASE_H)

/**
* @file
* $Id: FinishedFrameBase.h 568 2011-07-24 18:28:43Z bigmuscle $
*/
