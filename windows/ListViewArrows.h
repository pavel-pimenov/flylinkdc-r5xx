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

#if !defined(LIST_VIEW_ARROWS_H)
#define LIST_VIEW_ARROWS_H

#pragma once


#include "WinUtil.h"

template<class T>
class ListViewArrows
{
	public:
		ListViewArrows() { }
		virtual ~ListViewArrows() { }
		
		typedef ListViewArrows<T> thisClass;
		
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SETTINGCHANGE, onSettingChange)
		END_MSG_MAP()
		
		void updateArrow()
		{
			T* pThis = (T*)this;
			
			CHeaderCtrl headerCtrl = pThis->GetHeader();
			const int itemCount = headerCtrl.GetItemCount();
			if (CompatibilityManager::getComCtlVersion() >= MAKELONG(0, 6))
			{
				for (int i = 0; i < itemCount; ++i)
				{
					HDITEM item = {0};
					item.mask = HDI_FORMAT;
					headerCtrl.GetItem(i, &item);
					
					//clear the previous state
					item.fmt &=  ~(HDF_SORTUP | HDF_SORTDOWN);
					
					if (i == pThis->getSortColumn())
					{
						item.fmt |= (pThis->isAscending() ? HDF_SORTUP : HDF_SORTDOWN);
					}
					
					headerCtrl.SetItem(i, &item);
				}
			}
		}
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			T* pThis = (T*)this;
			_Module.AddSettingChangeNotify(pThis->m_hWnd);
			
			WinUtil::SetWindowThemeExplorer(pThis->m_hWnd);
			
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			T* pThis = (T*)this;
			_Module.RemoveSettingChangeNotify(pThis->m_hWnd);
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onSettingChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			return 1;
		}
};

#endif // !defined(LIST_VIEW_ARROWS_H)

/**
 * @file
 * $Id: ListViewArrows.h,v 1.11 2006/11/12 20:32:08 bigmuscle Exp $
 */
