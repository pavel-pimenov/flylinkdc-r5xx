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

#if !defined(TYPED_LIST_VIEW_CTRL_H)
#define TYPED_LIST_VIEW_CTRL_H

#pragma once

#include "../client/SettingsManager.h"
#include "../client/FavoriteManager.h"
#include "../client/StringTokenizer.h"
#include "ListViewArrows.h"

class ColumnInfo
{
	public:
		ColumnInfo(const tstring &aName, int aPos, int aFormat, int aWidth): m_name(aName), m_pos(aPos), m_width(aWidth),
			m_format(aFormat), m_is_visible(true), m_is_owner_draw(false) {}
		~ColumnInfo() {}
		int m_format;
		int16_t m_width;
		int8_t  m_pos;
		bool m_is_visible;
		bool m_is_owner_draw;
		tstring m_name;
};

template<class T, int ctrlId>
class TypedListViewCtrl : public CWindowImpl<TypedListViewCtrl<T, ctrlId>, CListViewCtrl, CControlWinTraits>,
	public ListViewArrows<TypedListViewCtrl<T, ctrlId> >
{
	public:
		TypedListViewCtrl() : sortColumn(-1), sortAscending(true), hBrBg(Colors::bgBrush), leftMargin(0)
#ifndef IRAINMAN_NOT_USE_COUNT_UPDATE_INFO_IN_LIST_VIEW_CTRL
			, m_count_update_info(0)
#endif
		{
		}
		~TypedListViewCtrl()
		{
		}
		
		typedef TypedListViewCtrl<T, ctrlId> thisClass;
		typedef CListViewCtrl baseClass;
		typedef ListViewArrows<thisClass> arrowBase;
		
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_MENUCOMMAND, onHeaderMenu)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		MESSAGE_HANDLER(WM_ERASEBKGND, onEraseBkgnd)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		CHAIN_MSG_MAP(arrowBase)
		END_MSG_MAP();
		
#if 0
		class iterator : public std::iterator<std::random_access_iterator_tag, T*>
		{
			public:
				iterator() : typedList(NULL), cur(0), cnt(0) { }
				iterator(const iterator& rhs) : typedList(rhs.typedList), cur(rhs.cur), cnt(rhs.cnt) { }
				iterator& operator=(const iterator& rhs)
				{
					typedList = rhs.typedList;
					cur = rhs.cur;
					cnt = rhs.cnt;
					return *this;
				}
				
				bool operator==(const iterator& rhs) const
				{
					return cur == rhs.cur;
				}
				bool operator!=(const iterator& rhs) const
				{
					return !(*this == rhs);
				}
				bool operator<(const iterator& rhs) const
				{
					return cur < rhs.cur;
				}
				
				int operator-(const iterator& rhs) const
				{
					return cur - rhs.cur;
				}
				
				iterator& operator+=(int n)
				{
					cur += n;
					return *this;
				}
				iterator& operator-=(int n)
				{
					return (cur += -n);
				}
				
				T& operator*()
				{
					return *typedList->getItemData(cur);
				}
				T* operator->()
				{
					return &(*(*this));
				}
				T& operator[](size_t n) //-V302
				{
					return *typedList->getItemData(cur + n);//IRainman: MS use memsizetype in WinApi :(
				}
				
				iterator operator++(int)
				{
					iterator tmp(*this);
					operator++();
					return tmp;
				}
				iterator& operator++()
				{
					++cur;
					return *this;
				}
				
			private:
				iterator(thisClass* aTypedList) : typedList(aTypedList), cur(aTypedList->GetNextItem(-1, LVNI_ALL)), cnt(aTypedList->GetItemCount())
				{
					if (cur == -1)
						cur = cnt;
				}
				iterator(thisClass* aTypedList, int first) : typedList(aTypedList), cur(first), cnt(aTypedList->GetItemCount())
				{
					if (cur == -1)
						cur = cnt;
				}
				friend class thisClass;
				thisClass* typedList;
				int cur;
				int cnt;
		};
#endif
		bool isRedraw()
		{
			bool refresh = false;
			if (GetBkColor() != Colors::bgColor)
			{
				SetBkColor(Colors::bgColor);
				SetTextBkColor(Colors::bgColor);
				refresh = true;
			}
			if (GetTextColor() != Colors::textColor)
			{
				SetTextColor(Colors::textColor);
				refresh = true;
			}
			return refresh;
		}
		
		void update_all_columns(const std::vector<int>& p_items)
		{
			if (!p_items.empty())
			{
				CLockRedraw<false> l_lock_draw(m_hWnd);
				for (auto i = p_items.begin(); i != p_items.end(); ++i)
				{
					updateItem(*i);
				}
			}
		}
		void update_columns(const std::vector<int>& p_items, const std::vector<int>& p_columns)
		{
			if (!p_items.empty())
			{
				CLockRedraw<false> l_lock_draw(m_hWnd);
				// TODO - обощить
				// const auto l_top_index         = GetTopIndex();
				// const auto l_count_per_page    = GetCountPerPage();
				// const auto l_item_count        = GetItemCount();
				for (auto i = p_items.cbegin(); i != p_items.cend(); ++i)
				{
					// TODO if (*i >= l_top_index && *i < l_item_count && *i < (l_top_index + l_count_per_page))
					for (auto j = p_columns.cbegin(); j != p_columns.cend(); ++j)
					{
						updateItem(*i, *j);
					}
				}
			}
		}
		void autosize_columns()
		{
			const int l_columns = GetHeader().GetItemCount();
			for (int i = 0; i < l_columns; ++i)
			{
				SetColumnWidth(i, LVSCW_AUTOSIZE);
			}
		}
		
		LRESULT onChar(UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if ((GetKeyState(VkKeyScan('A') & 0xFF) & 0xFF00) > 0 && (GetKeyState(VK_CONTROL) & 0xFF00) > 0)
			{
				const int l_cnt = GetItemCount();
				for (int i = 0; i < l_cnt; ++i)
					ListView_SetItemState(m_hWnd, i, LVIS_SELECTED, LVIS_SELECTED);
					
				return 0;
			}
			
			bHandled = FALSE;
			return 1;
		}
		
		void setText(LVITEM& i, const tstring &text)
		{
			wcsncpy(i.pszText, text.c_str(), static_cast<size_t>(i.cchTextMax));// [!] PVS V303 The function 'lstrcpyn' is deprecated in the Win64 system. It is safer to use the 'wcsncpy' function.
		}
		void setText(LVITEM& i, const TCHAR* text)
		{
			i.pszText = const_cast<TCHAR*>(text);
		}
		
		LRESULT onGetDispInfo(int /* idCtrl */, LPNMHDR pnmh, BOOL& /* bHandled */)
		{
			NMLVDISPINFO* di = (NMLVDISPINFO*)pnmh;
			if (di && di->item.iSubItem >= 0)
			{
				if (di->item.lParam)
				{
					if (di->item.mask & LVIF_TEXT)
					{
						di->item.mask |= LVIF_DI_SETITEM;
						const auto l_sub_item = static_cast<size_t>(di->item.iSubItem);
						const auto& l_column_info = m_columnList[l_sub_item];
						if (l_column_info.m_is_owner_draw == false)
						{
							const auto l_index = m_columnIndexes[l_sub_item];
							const auto& l_text = ((T*)di->item.lParam)->getText(l_index); // TODO - на OwnerDraw пропускать вызов getText
							setText(di->item, l_text);
						}
						else
						{
							//dcdebug("!!!!!!!!!!! OWNER_DRAW - onGetDispInfo l_index = %d \n", l_sub_item);
						}
					}
					if (di->item.mask & LVIF_IMAGE) // http://support.microsoft.com/KB/141834
					{
						/*
						#ifdef _DEBUG
						        static int g_count = 0;
						        dcdebug("onGetDispInfo  count = %d di->item.iItem = %d di->item.iSubItem = %d, di->item.iIndent = %d, di->item.lParam = %d "
						                "mask = %d "
						                "state = %d "
						                "stateMask = %d "
						                "pszText = %d "
						                "cchTextMax = %d "
						                "iGroupId = %d "
						                "cColumns = %d "
						                "puColumns = %d "
						                "hdr.code = %d "
						                "hdr.hwndFrom = %d "
						                "hdr.idFrom = %d\n"
						                ,++g_count, di->item.iItem, di->item.iSubItem, di->item.iIndent, di->item.lParam,
						    di->item.mask,
						    di->item.state,
						    di->item.stateMask,
						    di->item.pszText,
						    di->item.cchTextMax,
						//    di->item.iImage,
						    di->item.iGroupId,
						    di->item.cColumns,
						    di->item.puColumns,
						    di->hdr.code,
						    di->hdr.hwndFrom,
						    di->hdr.idFrom
						                );
						#endif
						*/
						//[?] di->item.mask |= LVIF_DI_SETITEM;
						di->item.iImage = ((T*)di->item.lParam)->getImageIndex();
					}
				}
			}
			return 0;
		}
		LRESULT onInfoTip(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			if (BOOLSETTING(POPUPS_DISABLED) || !BOOLSETTING(SHOW_INFOTIPS)) return 0;
			
			NMLVGETINFOTIP* pInfoTip = (NMLVGETINFOTIP*) pnmh;
			const BOOL NoColumnHeader = (BOOL)(GetWindowLongPtr(GWL_STYLE) & LVS_NOCOLUMNHEADER);
			tstring InfoTip;
			tstring buffer;
			const size_t BUF_SIZE = 300;
			buffer.resize(BUF_SIZE);
			const int l_cnt = GetHeader().GetItemCount();
			std::vector<int> l_indexes(l_cnt);
			GetColumnOrderArray(l_cnt, l_indexes.data());
			for (int i = 0; i < l_cnt; ++i)
			{
				if (!NoColumnHeader)
				{
					LV_COLUMN lvCol = {0};
					lvCol.mask = LVCF_TEXT;
					lvCol.pszText = &buffer[0];
					lvCol.cchTextMax = BUF_SIZE;
					GetColumn(l_indexes[i], &lvCol);
					InfoTip += lvCol.pszText;
					InfoTip += _T(": ");
				}
				LVITEM lvItem = {0};
				lvItem.iItem = pInfoTip->iItem;
				buffer[0] = 0;
				buffer[BUF_SIZE - 1] = 0;
				GetItemText(pInfoTip->iItem, l_indexes[i],  &buffer[0], BUF_SIZE - 2);
				InfoTip += &buffer[0];
				InfoTip += _T("\r\n");
			}
			
			if (InfoTip.size() > 2)
				InfoTip.erase(InfoTip.size() - 2);
				
			pInfoTip->cchTextMax = static_cast<int>(InfoTip.size());
			
			_tcsncpy(pInfoTip->pszText, InfoTip.c_str(), INFOTIPSIZE);
			pInfoTip->pszText[INFOTIPSIZE - 1] = NULL;
			
			return 0;
		}
		// Sorting
		LRESULT onColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			const NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
			if (l->iSubItem != sortColumn)
			{
				sortAscending = true;
				sortColumn = l->iSubItem;
			}
			else if (sortAscending)
			{
				sortAscending = false;
			}
			else
			{
				sortColumn = -1;
			}
			updateArrow();
			resort();
			return 0;
		}
		void resort()
		{
			if (sortColumn != -1)
			{
				/*
				if(m_columnList[sortColumn].m_is_owner_draw) //TODO - проверить сортировку
				                {
				                        const int l_item_count = GetItemCount();
				                        for(int i=0;i<l_item_count;++i)
				                        {
				                          // updateItem(i,sortColumn);
				                        }
				                }
				*/
				SortItems(&compareFunc, (LPARAM)this);
			}
		}
		
		int insertItem(const T* item, int image)
		{
			return insertItem(getSortPos(item), item, image);
		}
		int insertItem(int i, const T* item, int image)
		{
			return InsertItem(LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE, i,
			                  LPSTR_TEXTCALLBACK, 0, 0, image, (LPARAM)item); // TODO I_IMAGECALLBACK
		}
		
		T* getItemData(const int iItem) const
		{
			return (T*)GetItemData(iItem);
		}
		int getSelectedCount() const
		{
			return GetSelectedCount();    // !SMT!-S
		}
		void forEachSelectedParam(void (T::*func)(void*), void *param)   // !SMT!-S
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(param);
		}
		
		void forEachSelectedParam(void (T::*func)(const string&, const tstring&), const string& hubHint, const tstring& p_message)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(hubHint, p_message);
		}
		void forEachSelectedParam(void (T::*func)(const string&, uint64_t), const string& hubHint, uint64_t p_data)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
				(getItemData(i)->*func)(hubHint, p_data);
		}
		
		int findItem(const T* item) const
		{
			LVFINDINFO l_fi = { 0 };
			l_fi.flags  = LVFI_PARAM;
			l_fi.lParam = (LPARAM)item;
			return FindItem(&l_fi, -1);
		}
		struct CompFirst
		{
			CompFirst() { }
			bool operator()(T& a, const tstring& b)
			{
				return stricmp(a.getText(0), b) < 0;
			}
		};
		int findItem(const tstring& b, int start = -1, bool aPartial = false) const
		{
			LVFINDINFO l_fi = { 0 };
			l_fi.flags  = aPartial ? LVFI_PARTIAL : LVFI_STRING;
			l_fi.psz = b.c_str();
			return FindItem(&l_fi, start);
		}
		
		void forEach(void (T::*func)())
		{
			const int l_cnt = GetItemCount();
			for (int i = 0; i < l_cnt; ++i)
			{
				T* itemData = getItemData(i);
				if (itemData)// [+] IRainman fix.
					(itemData->*func)();
			}
		}
		void forEachSelected(void (T::*func)())
		{
			CLockRedraw<> l_lock_draw(m_hWnd); // [+] IRainman opt.
			int n = GetItemCount();
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				T* item_data = getItemData(i);
				// TODO: with code compiled by Intel Compiler - code always crashes... me sad :(
				if (item_data) // [+] IRainman fix
					(item_data->*func)(); //[9] https://www.box.net/shared/95775557fcc1825a65cf
					
				const int l_new_count = GetItemCount();
				if (n != l_new_count)
				{
					n = l_new_count;
					--i;
				}
			}
		}
		template<class _Function>
		_Function forEachT(_Function pred)
		{
			int l_cnt = GetItemCount();
			for (int i = 0; i < l_cnt; ++i)
			{
				T* itemData = getItemData(i);
				if (itemData)// [+] IRainman fix.
					pred(itemData);
			}
			return pred;
		}
		template<class _Function>
		_Function forEachSelectedT(_Function pred)
		{
			int i = -1;
			while ((i = GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				T* itemData = getItemData(i);
				if (itemData)// [+] IRainman fix.
					pred(itemData);
			}
			return pred;
		}
		void forEachAtPos(int iIndex, void (T::*func)())
		{
			(getItemData(iIndex)->*func)();
		}
		
		void updateItem(int i)
		{
			const int l_cnt = GetHeader().GetItemCount();
			for (int j = 0; j < l_cnt; ++j)
				SetItemText(i, j, LPSTR_TEXTCALLBACK);
		}
		
		void updateItem(int i, int col)
		{
			SetItemText(i, col, LPSTR_TEXTCALLBACK);
		}
		int updateItem(const T* item)
		{
			int i = findItem(item);
			if (i != -1)
			{
				updateItem(i);
			}
			else
			{
				dcassert(i != -1);
			}
			return i;
		}
		int deleteItem(const T* item)
		{
			int i = findItem(item);
			if (i != -1)
			{
				DeleteItem(i);
			}
			else
			{
				// dcassert(i != -1);
			}
			return i;
		}
		
		void DeleteAndCleanAllItemsNoLock()
		{
			const int l_cnt = GetItemCount();
			for (int i = 0; i < l_cnt; i++)
			{
				T* si = getItemData(i);
				delete si;
			}
			DeleteAllItems();
		}
		void DeleteAndCleanAllItems() // [+] IRainman
		{
			CLockRedraw<> l_lock_draw(m_hWnd);
			DeleteAndCleanAllItemsNoLock();
		}
		
		int getSortPos(const T* a) const
		{
			int high = GetItemCount();
			if (sortColumn == -1 || high == 0)
				return high;
				
			//PROFILE_THREAD_SCOPED()
			high--;
			
			int low = 0;
			int mid = 0;
			T* b = nullptr;
			int comp = 0;
			while (low <= high)
			{
				mid = (low + high) / 2;
				b = getItemData(mid);
				comp = T::compareItems(a, b, static_cast<uint8_t>(sortColumn));
				// 2012-04-18_11-17-28_B3BB3VL7PHSN5IZDVYVHG2DRYABDDNJKCNRCLTA_FB684B71_crash-stack-r502-beta18-build-9768.dmp
				// 2012-05-03_22-00-59_Z3A2HUBL63PXNXFOWUL464IBUWZMFX4OFFEMPIQ_86105807_crash-stack-r502-beta24-build-9900.dmp
				// 2012-06-17_22-40-29_WLKKNKN2VQZTGIBREBJAGIOA2SD57ZYUC5ZQOAQ_5912005A_crash-stack-r502-beta36-x64-build-10378.dmp
				
				if (!sortAscending)
					comp = -comp;
					
				if (comp == 0)
				{
					return mid;
				}
				else if (comp < 0)
				{
					high = mid - 1;
				}
				else if (comp > 0)
				{
					low = mid + 1;
				}
			}
			
			comp = T::compareItems(a, b, static_cast<uint8_t>(sortColumn));
			if (!sortAscending)
				comp = -comp;
			if (comp > 0)
				mid++;
				
			return mid;
		}
		
		void setSortColumn(int aSortColumn)
		{
			sortColumn = aSortColumn;
			updateArrow();
		}
#ifndef IRAINMAN_NOT_USE_COUNT_UPDATE_INFO_IN_LIST_VIEW_CTRL
		int getCountUpdateInfo() const
		{
			return m_count_update_info;
		}
#endif
		int getSortColumn() const
		{
			return sortColumn;
		}
		uint8_t getRealSortColumn() const
		{
			return findColumn(sortColumn); //-V106
		}
		bool isAscending() const
		{
			return sortAscending;
		}
		void setAscending(bool s)
		{
			sortAscending = s;
			updateArrow();
		}
		
#if 0
		iterator begin()
		{
			return iterator(this);
		}
		iterator end()
		{
			return iterator(this, GetItemCount());
		}
#endif
		
		int InsertColumn(uint8_t nCol, const tstring &columnHeading, int nFormat = LVCFMT_LEFT, int nWidth = -1, int nSubItem = -1)
		{
			if (nWidth == 0)
			{
				nWidth = 80;
			}
			m_columnList.push_back(ColumnInfo(columnHeading, nCol, nFormat, nWidth));
			m_columnIndexes.push_back(nCol);
			return CListViewCtrl::InsertColumn(nCol, columnHeading.c_str(), nFormat, nWidth, nSubItem);
		}
		// [+] InfinitySky. Alpha for PNG.
#ifdef FLYLINKDC_USE_LIST_VIEW_WATER_MARK
		BOOL SetBkColor(COLORREF cr, UINT nID = 0)
		{
			if (nID != 0)
			{
				WinUtil::setListCtrlWatermark(m_hWnd, nID, cr);
			}
			return CListViewCtrl::SetBkColor(cr);
		}
#endif
		void showMenu(const POINT &pt)
		{
			headerMenu.DestroyMenu();
			headerMenu.CreatePopupMenu();
			MENUINFO inf;
			inf.cbSize = sizeof(MENUINFO);
			inf.fMask = MIM_STYLE;
			inf.dwStyle = MNS_NOTIFYBYPOS;
			headerMenu.SetMenuInfo(&inf);
			
			int j = 0;
			for (auto i = m_columnList.cbegin(); i != m_columnList.cend(); ++i, ++j)
			{
				headerMenu.AppendMenu(MF_STRING, IDC_HEADER_MENU, i->m_name.c_str());
				if (i->m_is_visible)
					headerMenu.CheckMenuItem(j, MF_BYPOSITION | MF_CHECKED);
			}
			headerMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		
		LRESULT onEraseBkgnd(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			return 1; // AirDC++
			/*          bHandled = FALSE;
			            if (!leftMargin || !hBrBg)
			                return 0;
			
			            dcassert(hBrBg);
			            if (!hBrBg) return 0;
			
			            bHandled = TRUE;
			            HDC dc = (HDC)wParam;
			            const int n = GetItemCount();
			            RECT r = {0, 0, 0, 0}, full;
			            GetClientRect(&full);
			
			            if (n > 0)
			            {
			                GetItemRect(0, &r, LVIR_BOUNDS);
			                r.bottom = r.top + ((r.bottom - r.top) * n);
			            }
			
			            RECT full2 = full; // Keep a backup
			
			
			            full.bottom = r.top;
			            FillRect(dc, &full, hBrBg);
			
			            full = full2; // Revert from backup
			            full.right = r.left + leftMargin; // state image
			            //full.left = 0;
			            FillRect(dc, &full, hBrBg);
			
			            full = full2; // Revert from backup
			            full.left = r.right;
			            FillRect(dc, &full, hBrBg);
			
			            full = full2; // Revert from backup
			            full.top = r.bottom;
			            full.right = r.right;
			            FillRect(dc, &full, hBrBg);
			            return S_OK;
			*/
		}
		void setFlickerFree(HBRUSH flickerBrush)
		{
			hBrBg = flickerBrush;
		}
		
		LRESULT onContextMenu(UINT /*msg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			//make sure we're not handling the context menu if it's invoked from the
			//keyboard
			if (pt.x == -1 && pt.y == -1)
			{
				bHandled = FALSE;
				return 0;
			}
			
			CRect rc;
			GetHeader().GetWindowRect(&rc);
			
			if (PtInRect(&rc, pt))
			{
				showMenu(pt);
				return 0;
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onHeaderMenu(UINT /*msg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ColumnInfo& ci = m_columnList[wParam];
			ci.m_is_visible = ! ci.m_is_visible;
#ifndef IRAINMAN_NOT_USE_COUNT_UPDATE_INFO_IN_LIST_VIEW_CTRL
			m_count_update_info++;
#endif
			{
				CLockRedraw<true> l_lock_draw(m_hWnd);
				if (!ci.m_is_visible)
				{
					removeColumn(ci);
				}
				else
				{
					if (ci.m_width == 0)
					{
						ci.m_width = 80;
					}
					CListViewCtrl::InsertColumn(ci.m_pos, ci.m_name.c_str(), ci.m_format, ci.m_width, static_cast<int>(wParam));
					LVCOLUMN lvcl = {0};
					lvcl.mask = LVCF_ORDER;
					lvcl.iOrder = ci.m_pos;
					SetColumn(ci.m_pos, &lvcl);
					updateAllImages(true); //[+]PPA
				}
				
				updateColumnIndexes();
#ifndef IRAINMAN_NOT_USE_COUNT_UPDATE_INFO_IN_LIST_VIEW_CTRL
				m_count_update_info--;
#endif
			}
			
			UpdateWindow();
			
			return 0;
		}
		
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled) //[!]PPA TODO Copy-Paste
		{
			return Colors::alternationonCustomDraw(pnmh, bHandled);
		}
#endif
		
		void saveHeaderOrder(SettingsManager::StrSetting order, SettingsManager::StrSetting widths,
		                     SettingsManager::StrSetting visible)
		{
			string tmp, tmp2, tmp3;
			
			saveHeaderOrder(tmp, tmp2, tmp3);
			
			SettingsManager::set(order, tmp);
			SettingsManager::set(widths, tmp2);
			SettingsManager::set(visible, tmp3);
		}
		
		void saveHeaderOrder(string& order, string& widths, string& visible) noexcept
		{
			AutoArray<TCHAR> l_buf(512);
			int size = GetHeader().GetItemCount();
			for (int i = 0; i < size; ++i)
			{
				LVCOLUMN lvc = {0};
				lvc.mask = LVCF_TEXT | LVCF_ORDER | LVCF_WIDTH;
				lvc.cchTextMax = 512;
				lvc.pszText = l_buf;
				l_buf[0] = 0;
				GetColumn(i, &lvc);
				for (auto j = m_columnList.begin(); j != m_columnList.end(); ++j)
				{
					if (_tcscmp(l_buf.data(), j->m_name.c_str()) == 0)
					{
						j->m_pos = lvc.iOrder;
						j->m_width = lvc.cx;
						break;
					}
				}
			}
			string l_separator;
			for (auto i = m_columnList.begin(); i != m_columnList.end(); ++i)
			{
				if (i->m_is_visible)
				{
					visible += l_separator + "1";
				}
				else
				{
					i->m_pos = size++; // TODO зачем это?
					visible += l_separator + "0";
				}
				order  += l_separator + Util::toString(i->m_pos);
				widths += l_separator + Util::toString(i->m_width);
				if (l_separator.empty())
					l_separator  = ",";
			}
		}
		
		void setVisible(const string& vis)
		{
			const StringTokenizer<string> tok(vis, ',');
			const StringList& l = tok.getTokens();
			CLockRedraw<> l_lock_draw(m_hWnd); // [+] IRainman opt.
			auto i = l.cbegin();
			for (auto j = m_columnList.begin(); j != m_columnList.end() && i != l.cend(); ++i, ++j)
			{
			
				if (Util::toInt(*i) == 0)
				{
					j->m_is_visible = false;
					removeColumn(*j);
				}
			}
			
			updateColumnIndexes();
		}
		
		void setColumnOrderArray(size_t iCount, int* columns)
		{
			LVCOLUMN lvc = {0};
			lvc.mask = LVCF_ORDER;
			
			int j = 0;
			for (size_t i = 0; i < iCount;)
			{
				if (columns[i] == j)
				{
					lvc.iOrder = m_columnList[i].m_pos = columns[i];
					SetColumn(static_cast<int>(i), &lvc);
					
					j++; //-V127 An overflow of the 32-bit 'j' variable is possible inside a long cycle which utilizes a memsize-type loop counter. typedlistviewctrl.h 720
					i = 0;
				}
				else
				{
					i++;
				}
			}
		}
		
		//find the original position of the column at the current position.
		uint8_t findColumn(uint8_t p_col) const
		{
			dcassert(p_col < m_columnIndexes.size());
			// [-] if (p_col < m_columnIndexes.size()) [-] IRainman fix.
			return m_columnIndexes[p_col];
			// [-] else return 0; [-] IRainman fix.
		}
		
		T* getSelectedItem() const
		{
			return GetSelectedCount() > 0 ? getItemData(GetNextItem(-1, LVNI_SELECTED)) : nullptr;
		}
		void setColumnOwnerDraw(const uint8_t p_col)
		{
			m_columnList[p_col].m_is_owner_draw = true;
		}
	private:
		int sortColumn;
#ifndef IRAINMAN_NOT_USE_COUNT_UPDATE_INFO_IN_LIST_VIEW_CTRL
		int m_count_update_info; //[+]FlylinkDC++
#endif
		bool sortAscending;
		int leftMargin;
		HBRUSH hBrBg;
		CMenu headerMenu;
		
		static int CALLBACK compareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
		{
			thisClass* t = (thisClass*)lParamSort;
			int result = T::compareItems((T*)lParam1, (T*)lParam2, t->getRealSortColumn()); // https://www.box.net/shared/043aea731a61c46047fe
			return (t->sortAscending ? result : -result);
		}
		
		vector<ColumnInfo> m_columnList;
		vector<uint8_t> m_columnIndexes;
		
		void updateAllImages(bool p_updateItems = false)
		{
			const int l_cnt = GetItemCount();
			for (int i = 0; i < l_cnt; ++i)
			{
				LVITEM lvItem = {0};
				lvItem.iItem = i;
				lvItem.iSubItem = 0;
				lvItem.mask = LVIF_PARAM | LVIF_IMAGE;
				GetItem(&lvItem);
				lvItem.iImage = ((T*)lvItem.lParam)->getImageIndex();
				SetItem(&lvItem);
				if (p_updateItems)
					updateItem(i);
			}
		}
		void removeColumn(ColumnInfo& ci)
		{
			AutoArray<TCHAR> l_buf(512);
			l_buf[0] = 0;
			LVCOLUMN lvcl = {0};
			lvcl.mask = LVCF_TEXT | LVCF_ORDER | LVCF_WIDTH;
			lvcl.pszText = l_buf.data();
			lvcl.cchTextMax = 512;
			for (int k = 0; k < GetHeader().GetItemCount(); ++k)
			{
				GetColumn(k, &lvcl);
				if (_tcscmp(ci.m_name.c_str(), lvcl.pszText) == 0)
				{
					ci.m_width = lvcl.cx;
					ci.m_pos = lvcl.iOrder;
					
					int itemCount = GetHeader().GetItemCount();
					if (itemCount >= 0 && sortColumn > itemCount - 2)
						setSortColumn(0);
						
					if (sortColumn == ci.m_pos)
						setSortColumn(0);
						
					DeleteColumn(k);
					updateAllImages(); //[+]PPA
					break;
				}
			}
		}
		
		void updateColumnIndexes()
		{
			m_columnIndexes.clear();
			
			const int columns = GetHeader().GetItemCount();
			
			m_columnIndexes.reserve(static_cast<size_t>(columns));
			
			AutoArray<TCHAR> l_buf(128);
			l_buf[0] = 0;
			LVCOLUMN lvcl = {0};
			
			for (int i = 0; i < columns; ++i)
			{
				lvcl.mask = LVCF_TEXT;
				lvcl.pszText = l_buf.data();
				lvcl.cchTextMax = 128;
				GetColumn(i, &lvcl);
				
				for (size_t j = 0; j < m_columnList.size(); ++j)
				{
					if (stricmp(m_columnList[j].m_name.c_str(), lvcl.pszText) == 0)
					{
						m_columnIndexes.push_back(static_cast<uint8_t>(j));
						break;
					}
				}
			}
		}
	public:
		void SetItemFilled(const LPNMLVCUSTOMDRAW p_cd, const CRect& p_rc2, COLORREF p_textColor = Colors::textColor, COLORREF p_textColorUnfocus = Colors::textColor)
		{
			COLORREF color;
			if (GetItemState((int)p_cd->nmcd.dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED)
			{
				if (m_hWnd == ::GetFocus())
				{
					// TODO: GetSysColor break the color of the theme for FlylinkDC? Use color from WinUtil to themed here?
					color = GetSysColor(COLOR_HIGHLIGHT);
					::SetBkColor(p_cd->nmcd.hdc, color);
					::SetTextColor(p_cd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
				}
				else
				{
					color = Colors::getAlternativBkColor(p_cd); // [!] IRainman
					::SetBkColor(p_cd->nmcd.hdc, color); // [!] IRainman
					::SetTextColor(p_cd->nmcd.hdc, p_textColorUnfocus);
				}
			}
			else
			{
				color = Colors::getAlternativBkColor(p_cd); // [!] IRainman
				::SetBkColor(p_cd->nmcd.hdc, color); // [!] IRainman
				::SetTextColor(p_cd->nmcd.hdc, p_textColor);
			}
			HGDIOBJ oldpen = ::SelectObject(p_cd->nmcd.hdc, CreatePen(PS_SOLID, 0, color));
			HGDIOBJ oldbr = ::SelectObject(p_cd->nmcd.hdc, CreateSolidBrush(color));
			Rectangle(p_cd->nmcd.hdc, p_rc2.left + 1, p_rc2.top, p_rc2.right, p_rc2.bottom);
			DeleteObject(::SelectObject(p_cd->nmcd.hdc, oldpen));
			DeleteObject(::SelectObject(p_cd->nmcd.hdc, oldbr));
		}
};

// Copyright (C) 2005-2009 Big Muscle, StrongDC++
template<class T, int ctrlId, class K, class hashFunc, class equalKey>
class TypedTreeListViewCtrl : public TypedListViewCtrl<T, ctrlId>
{
	public:
	
		TypedTreeListViewCtrl() : uniqueParent(false) // [cppcheck] Member variable 'TypedTreeListViewCtrl<T,ctrlId,K,hashFunc,equalKey>::uniqueParent' is not initialized in the constructor.
		{
		}
		~TypedTreeListViewCtrl()
		{
			states.Destroy();
		}
		
		typedef TypedTreeListViewCtrl<T, ctrlId, K, hashFunc, equalKey> thisClass;
		typedef TypedListViewCtrl<T, ctrlId> baseClass;
		
		struct ParentPair
		{
			T* parent;
			vector<T*> children;
		};
		
		typedef std::pair<K*, ParentPair> ParentMapPair;
		typedef std::unordered_map<K*, ParentPair, hashFunc, equalKey> ParentMap;
		
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButton)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP();
		
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			ResourceLoader::LoadImageList(IDR_STATE, states, 16, 16);
			SetImageList(states, LVSIL_STATE);
			
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onLButton(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
			CPoint pt;
			pt.x = GET_X_LPARAM(lParam);
			pt.y = GET_Y_LPARAM(lParam);
			
			LVHITTESTINFO lvhti;
			lvhti.pt = pt;
			
			int pos = SubItemHitTest(&lvhti);
			if (pos != -1)
			{
				CRect rect;
				GetItemRect(pos, rect, LVIR_ICON);
				
				if (pt.x < rect.left)
				{
					T* i = getItemData(pos);
					if (i->parent == NULL)
					{
						if (i->collapsed)
						{
							Expand(i, pos);
						}
						else
						{
							Collapse(i, pos);
						}
					}
				}
			}
			
			bHandled = false;
			return 0;
		}
		
		void Collapse(T* parent, int itemPos)
		{
			SetRedraw(false);
			const vector<T*>& children = findChildren(parent->getGroupCond());
			for (auto i = children.cbegin(); i != children.cend(); ++i)
			{
				deleteItem(*i);
			}
			parent->collapsed = true;
			SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			SetRedraw(true);
		}
		
		void Expand(T* parent, int itemPos)
		{
			SetRedraw(false);
			const vector<T*>& children = findChildren(parent->getGroupCond());
			if (children.size() > (size_t)(uniqueParent ? 1 : 0))
			{
				parent->collapsed = false;
				for (auto i = children.cbegin(); i != children.cend(); ++i)
				{
					insertChild(*i, itemPos + 1);
				}
				SetItemState(itemPos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
				resort();
			}
			SetRedraw(true);
		}
		
		void insertChild(const T* item, int idx)
		{
			LV_ITEM lvi;
			lvi.mask = LVIF_TEXT | LVIF_PARAM | LVIF_IMAGE | LVIF_INDENT;
			lvi.iItem = idx;
			lvi.iSubItem = 0;
			lvi.iIndent = 1;
			lvi.pszText = LPSTR_TEXTCALLBACK;
			lvi.iImage = item->getImageIndex();
			lvi.lParam = (LPARAM)item;
			lvi.state = 0;
			lvi.stateMask = 0;
			InsertItem(&lvi);
		}
		
		T* findParent(const K& groupCond) const
		{
			ParentMap::const_iterator i = parents.find(const_cast<K*>(&groupCond));
			return i != parents.end() ? (*i).second.parent : NULL;
		}
		
		static const vector<T*> g_emptyVector;
		const vector<T*>& findChildren(const K& groupCond) const
		{
			ParentMap::const_iterator i = parents.find(const_cast<K*>(&groupCond));
			if (i != parents.end())
			{
				return  i->second.children;
			}
			else
			{
				return g_emptyVector;
			}
		}
		
		ParentPair* findParentPair(const K& groupCond)
		{
			ParentMap::iterator i = parents.find(const_cast<K*>(&groupCond));
			if (i != parents.end())
			{
				return &i->second;
			}
			else
			{
				return nullptr;
			}
		}
		
		void insertGroupedItem(T* item, bool autoExpand, bool extra, bool p_use_image_callback)
		{
			T* parent = nullptr;
			ParentPair* pp = nullptr;
			
			if (!extra)
				pp = findParentPair(item->getGroupCond());
				
			int pos = -1;
			
			if (pp == NULL)
			{
				parent = item;
				
				ParentPair newPP = { parent };
				parents.insert(ParentMapPair(const_cast<K*>(&parent->getGroupCond()), newPP));
				
				parent->parent = nullptr; // ensure that parent of this item is really NULL
				insertItem(getSortPos(parent), parent, p_use_image_callback ? I_IMAGECALLBACK : parent->getImageIndex());
				return;
			}
			else if (pp->children.empty())
			{
				T* oldParent = pp->parent;
				parent = oldParent->createParent();
				if (parent != oldParent)
				{
					uniqueParent = true;
					parents.erase(const_cast<K*>(&oldParent->getGroupCond()));
					deleteItem(oldParent);
					
					ParentPair newPP = { parent };
					pp = &(parents.insert(ParentMapPair(const_cast<K*>(&parent->getGroupCond()), newPP)).first->second);
					
					parent->parent = nullptr; // ensure that parent of this item is really NULL
					oldParent->parent = parent;
					pp->children.push_back(oldParent); // mark old parent item as a child
					parent->hits++;
					
					pos = insertItem(getSortPos(parent), parent, p_use_image_callback ? I_IMAGECALLBACK : parent->getImageIndex());
				}
				else
				{
					uniqueParent = false;
					pos = findItem(parent);
				}
				
				if (pos != -1)
				{
					if (autoExpand)
					{
						SetItemState(pos, INDEXTOSTATEIMAGEMASK(2), LVIS_STATEIMAGEMASK);
						parent->collapsed = false;
					}
					else
					{
						SetItemState(pos, INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
					}
				}
			}
			else
			{
				parent = pp->parent;
				pos = findItem(parent);
			}
			
			pp->children.push_back(item);
			parent->hits++;
			item->parent = parent;
			//item->updateMainItem();
			
			if (pos != -1)
			{
				if (!parent->collapsed)
				{
					insertChild(item, pos + static_cast<int>(pp->children.size()));
				}
				updateItem(pos);
			}
		}
		
		void removeParent(T* parent)
		{
			ParentPair* pp = findParentPair(parent->getGroupCond());
			if (pp)
			{
				for (auto i = pp->children.cbegin(); i != pp->children.cend(); ++i)
				{
					deleteItem(*i);
					delete *i;
				}
				pp->children.clear();
				parents.erase(const_cast<K*>(&parent->getGroupCond()));
			}
			deleteItem(parent);
		}
		
		void removeGroupedItem(T* item, bool removeFromMemory = true)
		{
			if (!item->parent)
			{
				removeParent(item);
			}
			else
			{
				T* parent = item->parent;
				ParentPair* pp = findParentPair(parent->getGroupCond());
				
				deleteItem(item); // TODO - разобраться почему тут не удаляет.
				
				const auto n = find(pp->children.begin(), pp->children.end(), item);
				if (n != pp->children.end())
				{
					pp->children.erase(n);
					pp->parent->hits--;
				}
				
				if (uniqueParent)
				{
					dcassert(!pp->children.empty());
					if (pp->children.size() == 1)
					{
						const T* oldParent = parent;
						parent = pp->children.front();
						
						deleteItem(oldParent);
						parents.erase(const_cast<K*>(&oldParent->getGroupCond()));
						delete oldParent;
						
						ParentPair newPP = { parent };
						parents.insert(ParentMapPair(const_cast<K*>(&parent->getGroupCond()), newPP));
						
						parent->parent = nullptr; // ensure that parent of this item is really NULL
						deleteItem(parent);
						insertItem(getSortPos(parent), parent, parent->getImageIndex());
					}
				}
				else
				{
					if (pp->children.empty())
					{
						SetItemState(findItem(parent), INDEXTOSTATEIMAGEMASK(0), LVIS_STATEIMAGEMASK);
					}
				}
				
				updateItem(parent);
			}
			
			if (removeFromMemory)
				delete item;
		}
		
		void DeleteAndClearAllItems() // [!] IRainman Dear BM: please use actual name!
		{
			CLockRedraw<> l_lock_draw(m_hWnd); // [+] IRainman opt.
			// HACK: ugly hack but at least it doesn't crash and there's no memory leak
			for (auto i = parents.cbegin(); i != parents.cend(); ++i)
			{
				T* ti = i->second.parent;
				for (auto j = i->second.children.cbegin(); j != i->second.children.cend(); ++j)
				{
					deleteItem(*j);
					delete *j;
				}
				deleteItem(ti);
				delete ti;
			}
			const int l_Count = GetItemCount();
			for (int i = 0; i < l_Count; i++)
			{
				T* si = getItemData(i);
				delete si;
			}
			
			parents.clear();
			DeleteAllItems();
		}
		
		LRESULT onColumnClick(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLISTVIEW* l = (NMLISTVIEW*)pnmh;
			if (l->iSubItem != getSortColumn())
			{
				setAscending(true);
				setSortColumn(l->iSubItem);
			}
			else if (isAscending())
			{
				setAscending(false);
			}
			else
			{
				setSortColumn(-1);
			}
			resort();
			return 0;
		}
		
		void resort()
		{
			if (getSortColumn() != -1)
			{
				SortItems(&compareFunc, (LPARAM)this);
			}
		}
		
		int getSortPos(const T* a)
		{
			int high = GetItemCount();
			if ((getSortColumn() == -1) || (high == 0))
				return high;
				
			high--;
			
			int low = 0;
			int mid = 0;
			T* b = nullptr;
			int comp = 0;
			while (low <= high)
			{
				mid = (low + high) / 2;
				b = getItemData(mid);
				comp = compareItems(a, b, static_cast<uint8_t>(getSortColumn()));  // https://www.box.net/shared/9411c0b86a2a66b073af
				
				if (!isAscending())
					comp = -comp;
					
				if (comp == 0)
				{
					return mid;
				}
				else if (comp < 0)
				{
					high = mid - 1;
				}
				else if (comp > 0)
				{
					low = mid + 1;
				}
				else if (comp == 2)
				{
					if (isAscending())
						low = mid + 1;
					else
						high = mid - 1;
				}
				else if (comp == -2)
				{
					if (!isAscending())
						low = mid + 1;
					else
						high = mid - 1;
				}
			}
			
			comp = compareItems(a, b, static_cast<uint8_t>(getSortColumn()));
			if (!isAscending())
				comp = -comp;
			if (comp > 0)
				mid++;
				
			return mid;
		}
		ParentMap& getParents()
		{
			return parents;
		}
		
	private:
	
		/** map of all parent items with their associated children */
		ParentMap parents;
		
		/** +/- images */
		CImageList states;
		
		/** is extra item needed for parent items? */
		bool uniqueParent;
		
		static int CALLBACK compareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
		{
			thisClass* t = (thisClass*)lParamSort;
			int result = compareItems((T*)lParam1, (T*)lParam2, t->getRealSortColumn());
			
			if (result == 2)
				result = (t->isAscending() ? 1 : -1);
			else if (result == -2)
				result = (t->isAscending() ? -1 : 1);
				
			return (t->isAscending() ? result : -result);
		}
		
		static int compareItems(const T* a, const T* b, uint8_t col)
		{
			// Copyright (C) Liny, RevConnect
			
			// both are children
			if (a->parent && b->parent)
			{
				// different parent
				if (a->parent != b->parent)
					return compareItems(a->parent, b->parent, col);
			}
			else
			{
				if (a->parent == b)
					return 2;  // a should be displayed below b
					
				if (b->parent == a)
					return -2; // b should be displayed below a
					
				if (a->parent)
					return compareItems(a->parent, b, col);
					
				if (b->parent)
					return compareItems(a, b->parent, col);
			}
			
			return T::compareItems(a, b, col);
		}
};

template<class T, int ctrlId, class K, class hashFunc, class equalKey>
const vector<T*> TypedTreeListViewCtrl<T, ctrlId, K, hashFunc, equalKey>::g_emptyVector;


// [+] FlylinkDC++: support MediaInfo in Lists.

template <typename T1, int ctrlId, class Base>
class MediainfoCtrl : public Base
{
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
	public:
		// [+]PPA
		bool drawHDIcon(const LPNMLVCUSTOMDRAW p_cd, const tstring& p_coulumn_media_xy, int p_coulumn_xy_index)
		{
			if (!p_coulumn_media_xy.empty() && findColumn(p_cd->iSubItem) == p_coulumn_xy_index)
			{
				CRect rc, rc2;
				GetSubItemRect((int)p_cd->nmcd.dwItemSpec, p_cd->iSubItem, LVIR_BOUNDS, rc);
				rc2 = rc;
				SetItemFilled(p_cd, rc2);
#ifdef PPA_MEDIAVIDEO_BOLD_TEXT
				HGDIOBJ l_old_font = nullptr;
#endif
				const auto l_ico_index = VideoImage::getMediaVideoIcon(p_coulumn_media_xy);
				if (l_ico_index != -1)
				{
					rc.left = rc.right - 19;
					const POINT p = { rc.left, rc.top };
					g_videoImage.Draw(p_cd->nmcd.hdc, l_ico_index, p);
#ifdef PPA_MEDIAVIDEO_BOLD_TEXT
					l_old_font = ::SelectObject(p_cd->nmcd.hdc, Fonts::g_boldFont);
#endif
				}
				if (!p_coulumn_media_xy.empty())
				{
					::ExtTextOut(p_cd->nmcd.hdc, rc2.left + 6, rc2.top + 2, ETO_CLIPPED, rc2, p_coulumn_media_xy.c_str(), p_coulumn_media_xy.length(), NULL);
				}
#ifdef PPA_MEDIAVIDEO_BOLD_TEXT
				if (l_old_font)
				{
					::SelectObject(p_cd->nmcd.hdc, l_old_font);
				}
#endif
				return true;
			}
			return false;
		}
#endif // SCALOLAZ_MEDIAVIDEO_ICO
};

template<class T, int ctrlId>
class MediainfoTypedListViewCtrl : public MediainfoCtrl<T, ctrlId, TypedListViewCtrl<T, ctrlId>>
{
};

template<class T, int ctrlId, class K, class hashFunc, class equalKey>
class MediainfoTypedTreeListViewCtrl : public MediainfoCtrl<T, ctrlId, TypedTreeListViewCtrl<T, ctrlId, K, hashFunc, equalKey>>
{
};

// [~] FlylinkDC++: support MediaInfo in Lists.

#endif // !defined(TYPED_LIST_VIEW_CTRL_H)

/**
 * @file
 * $Id: TypedListViewCtrl.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
