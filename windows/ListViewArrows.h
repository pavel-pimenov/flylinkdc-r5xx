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

#if !defined(LIST_VIEW_ARROWS_H)
#define LIST_VIEW_ARROWS_H

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
		
#ifdef FLYLINKDC_SUPPORT_WIN_2000
		void rebuildArrows()
		{
			//these arrows aren't used anyway so no need to create them
			if (CompatibilityManager::getComCtlVersion() >= MAKELONG(0, 6))
			{
				return;
			}
			
			POINT pathArrowLong[9] = {{0L, 7L}, {7L, 7L}, {7L, 6L}, {6L, 6L}, {6L, 4L}, {5L, 4L}, {5L, 2L}, {4L, 2L}, {4L, 0L}};
			POINT pathArrowShort[7] = {{0L, 6L}, {1L, 6L}, {1L, 4L}, {2L, 4L}, {2L, 2L}, {3L, 2L}, {3L, 0L}};
			
			CDC dc;
			CBrushHandle brush;
			CPen penLight;
			CPen penShadow;
			
			HPEN oldPen;
			HBITMAP oldBitmap;
			
			const int bitmapWidth = 8;
			const int bitmapHeight = 8;
			const RECT rect = {0, 0, bitmapWidth, bitmapHeight};
			
			T* pThis = (T*)this;
			
			if (!dc.CreateCompatibleDC(pThis->GetDC()))
				return;
				
			if (!brush.CreateSysColorBrush(COLOR_3DFACE))
				return;
				
			if (!penLight.CreatePen(PS_SOLID, 1, ::GetSysColor(COLOR_3DHIGHLIGHT)))
				return;
				
			if (!penShadow.CreatePen(PS_SOLID, 1, ::GetSysColor(COLOR_3DSHADOW)))
				return;
				
			if (upArrow.IsNull())
				upArrow.CreateCompatibleBitmap(pThis->GetDC(), bitmapWidth, bitmapHeight);
				
			if (downArrow.IsNull())
				downArrow.CreateCompatibleBitmap(pThis->GetDC(), bitmapWidth, bitmapHeight);
				
			// create up arrow
			oldBitmap = dc.SelectBitmap(upArrow);
			dc.FillRect(&rect, brush);
			oldPen = dc.SelectPen(penLight);
			dc.Polyline(pathArrowLong, _countof(pathArrowLong));
			dc.SelectPen(penShadow);
			dc.Polyline(pathArrowShort, _countof(pathArrowShort));
			
			// create down arrow
			dc.SelectBitmap(downArrow);
			dc.FillRect(&rect, brush);
			for (int i = 0; i < _countof(pathArrowShort); ++i)
			{
				POINT& pt = pathArrowShort[i];
				pt.x = bitmapWidth - pt.x;
				pt.y = bitmapHeight - pt.y;
			}
			dc.SelectPen(penLight);
			dc.Polyline(pathArrowShort, _countof(pathArrowShort));
			for (int i = 0; i < _countof(pathArrowLong); ++i)
			{
				POINT& pt = pathArrowLong[i];
				pt.x = bitmapWidth - pt.x;
				pt.y = bitmapHeight - pt.y;
			}
			dc.SelectPen(penShadow);
			dc.Polyline(pathArrowLong, _countof(pathArrowLong));
			
			dc.SelectPen(oldPen);
			dc.SelectBitmap(oldBitmap);
			penShadow.DeleteObject();
			penLight.DeleteObject();
			brush.DeleteObject();
		}
#endif // FLYLINKDC_SUPPORT_WIN_2000
		
		void updateArrow()
		{
			T* pThis = (T*)this;
			
			CHeaderCtrl headerCtrl = pThis->GetHeader();
			const int itemCount = headerCtrl.GetItemCount();
#ifdef FLYLINKDC_SUPPORT_WIN_XP
			if (CompatibilityManager::getComCtlVersion() >= MAKELONG(0, 6))
#endif
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
#ifdef FLYLINKDC_SUPPORT_WIN_2000
			else
			{
				if (upArrow.IsNull())
					return;
					
				HBITMAP bitmap = (pThis->isAscending() ? upArrow : downArrow);
				
				for (int i = 0; i < itemCount; ++i)
				{
					HDITEM item = {0};
					item.mask = HDI_FORMAT | HDI_BITMAP;
					headerCtrl.GetItem(i, &item);
					if (i == pThis->getSortColumn())
					{
						item.fmt |= HDF_BITMAP | HDF_BITMAP_ON_RIGHT;
						item.hbm = bitmap;
					}
					else
					{
						item.fmt &= ~(HDF_BITMAP | HDF_BITMAP_ON_RIGHT);
						item.hbm = 0;
					}
					headerCtrl.SetItem(i, &item);
				}
			}
#endif // FLYLINKDC_SUPPORT_WIN_2000
		}
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
#ifdef FLYLINKDC_SUPPORT_WIN_2000
			rebuildArrows();
#endif
			T* pThis = (T*)this;
			_Module.AddSettingChangeNotify(pThis->m_hWnd);
			
			if (BOOLSETTING(USE_EXPLORER_THEME)
#ifdef FLYLINKDC_SUPPORT_WIN_2000
			        && CompatibilityManager::IsXPPlus()
#endif
			   )
				SetWindowTheme(pThis->m_hWnd, L"explorer", NULL);
				
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			T* pThis = (T*)this;
			_Module.RemoveSettingChangeNotify(pThis->m_hWnd);
#ifdef FLYLINKDC_SUPPORT_WIN_2000
			if (downArrow)
				downArrow.DeleteObject();
			if (upArrow)
				upArrow.DeleteObject();
#endif
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onSettingChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
#ifdef FLYLINKDC_SUPPORT_WIN_2000
			rebuildArrows();
#endif
			bHandled = FALSE;
			return 1;
		}
	private:
#ifdef FLYLINKDC_SUPPORT_WIN_2000
		CBitmap upArrow;
		CBitmap downArrow;
#endif
};

#endif // !defined(LIST_VIEW_ARROWS_H)

/**
 * @file
 * $Id: ListViewArrows.h,v 1.11 2006/11/12 20:32:08 bigmuscle Exp $
 */
