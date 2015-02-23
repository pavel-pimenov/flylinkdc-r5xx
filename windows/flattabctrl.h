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

#if !defined(FLAT_TAB_CTRL_H)
#define FLAT_TAB_CTRL_H

#include "wtl_flylinkdc.h"
#include "atlgdiraii.h"
#ifdef IRAINMAN_INCLUDE_GDI_INIT
#include <gdiplus.h>
#endif // IRAINMAN_USE_GDI_PLUS_TAB

#include "../client/ResourceManager.h"
#include "WinUtil.h"
#include "resource.h"
#include "TabCtrlSharedInterface.h"
#include "HIconWrapper.h"
#include "ResourceLoader.h"
#ifdef IRAINMAN_INCLUDE_GDI_OLE
# include "../GdiOle/GDIImageOle.h"
#endif

// [+] IRainman opt.
extern bool g_TabsCloseButtonEnabled;
extern bool g_isStartupProcess;
extern CMenu g_mnu;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
extern Gdiplus::Pen g_pen_conter_side;
#endif
extern int g_magic_width;

extern DWORD g_color_shadow;
extern DWORD g_color_light;
extern DWORD g_color_face;
extern DWORD g_color_filllight;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
extern Gdiplus::Color g_color_shadow_gdi;
extern Gdiplus::Color g_color_light_gdi;
extern Gdiplus::Color g_color_face_gdi;
extern Gdiplus::Color g_color_filllight_gdi;
#endif // IRAINMAN_USE_GDI_PLUS_TAB
// [~] IRainman opt.

enum
{
	FT_FIRST = WM_APP + 700,
	/** This will be sent when the user presses a tab. WPARAM = HWND */
	FTM_SELECTED,
	/** The number of rows changed */
	FTM_ROWS_CHANGED,
	/** Set currently active tab to the HWND pointed by WPARAM */
	FTM_SETACTIVE,
	/** Display context menu and return TRUE, or return FALSE for the default one */
	FTM_CONTEXTMENU,
	/** Close window with postmessage... */
	WM_REALLY_CLOSE
};

template < class T, class TBase = CWindow, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE FlatTabCtrlImpl : public CWindowImpl< T, TBase, TWinTraits>
{
	public:
	
		enum { FT_EXTRA_SPACE = 18 };
		
		explicit FlatTabCtrlImpl() :
			m_rows(1),
			m_height(0),
			m_active(nullptr),
			m_moving(nullptr),
			m_is_intab(false),
			m_is_invalidate(false),
			m_is_cur_close(false),
			m_closing_hwnd(nullptr)
#ifdef IRAINMAN_FAST_FLAT_TAB
			, m_needsInvalidate(false)
			, m_allowInvalidate(false) // startup optimization: not set true here.
#endif
		{
#ifdef IRAINMAN_FAST_FLAT_TAB
//			black.CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
//			defaultgrey.CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNSHADOW));
//			facepen.CreatePen(PS_SOLID, 1, GetSysColor(COLOR_BTNFACE));
//			white.CreatePen(PS_SOLID, 1, GetSysColor(COLOR_WINDOW));
#endif
			updateTabs();
		}
		virtual ~FlatTabCtrlImpl() { }
		
		static LPCTSTR GetWndClassName()
		{
			return _T("FlatTabCtrl");
		}
#ifdef IRAINMAN_FAST_FLAT_TAB
		void SmartInvalidate(BOOL bErase = TRUE)
		{
			if (m_allowInvalidate && m_needsInvalidate)
			{
				// dcdebug("FlatTabCtrl:: m_allowInvalidate && m_needsInvalidate is its true\n");
				m_needsInvalidate = false;
				Invalidate(bErase);
			}
		}
#endif
		void addTab(HWND hWnd, COLORREF color = RGB(0, 0, 0), uint16_t icon = 0, uint16_t stateIcon = 0, bool p_mini = false)
		{
			TabInfo* i = new TabInfo(hWnd, color, icon, (stateIcon != 0) ? stateIcon : icon, !g_isStartupProcess);
			i->m_mini = p_mini;
			if ((icon == IDR_HUB || icon == IDR_PRIVATE
#ifdef USE_OFFLINE_ICON_FOR_FILELIST
			        || icon == IDR_FILE_LIST || icon == IDR_FILE_LIST_OFF// [+] InfinitySky.
#endif
			    ) || !BOOLSETTING(NON_HUBS_FRONT))
			{
				m_tabs.push_back(i);
				switch (WinUtil::GetTabsPosition())
				{
					case SettingsManager::TABS_TOP:
					case SettingsManager::TABS_BOTTOM:
						m_viewOrder.push_back(hWnd);
						break;
					default:
						m_viewOrder.push_front(hWnd);
						break;
				}
				
				m_nextTab = --m_viewOrder.end();
			}
			else
			{
				m_tabs.insert(m_tabs.begin(), i);
				m_viewOrder.insert(m_viewOrder.begin(), hWnd);
				m_nextTab = ++m_viewOrder.begin();
			}
			m_active = i;
#ifdef IRAINMAN_FAST_FLAT_TAB
			// do nothing, all updates in setActiv()
#else
			calcRows(false);
			Invalidate();
#endif
		}
		
		void removeTab(HWND aWnd)
		{
			if (!m_tabs.empty()) // бывает что табов нет http://www.flickr.com/photos/96019675@N02/9732602134/
			{
				TabInfo::List::const_iterator i;
				for (i = m_tabs.begin(); i != m_tabs.end(); ++i)
				{
					if ((*i)->hWnd == aWnd)
						break;
				}
				if (i != m_tabs.end()) //[+]PPA fix
				{
					TabInfo* ti = *i;
					m_tabs.erase(i);
					
					if (m_active == ti)
						m_active = nullptr;
					if (m_moving == ti)
						m_moving = nullptr;
						
					m_viewOrder.erase(std::find(begin(m_viewOrder), end(m_viewOrder), aWnd));
					m_nextTab = m_viewOrder.end();
					if (!m_viewOrder.empty())
						--m_nextTab;
						
#ifdef IRAINMAN_FAST_FLAT_TAB
					calcRows();
					SmartInvalidate();
#else
					calcRows(false);
					Invalidate();
#endif
					delete ti;
				}
			}
		}
		
		void startSwitch()
		{
			if (!m_viewOrder.empty())
				m_nextTab = --m_viewOrder.end();
			m_is_intab = true;
		}
		
		void endSwitch()
		{
			m_is_intab = false;
			if (m_active)
				setTop(m_active->hWnd);
		}
		
		HWND getNext()
		{
			if (m_viewOrder.empty())
				return nullptr;
				
			if (m_nextTab == m_viewOrder.begin())
			{
				m_nextTab = --m_viewOrder.end();
			}
			else
			{
				--m_nextTab;
			}
			return *m_nextTab;
		}
		HWND getPrev()
		{
			if (m_viewOrder.empty())
				return nullptr;
				
			++m_nextTab;
			if (m_nextTab == m_viewOrder.end())
			{
				m_nextTab = m_viewOrder.begin();
			}
			return *m_nextTab;
		}
		
		bool isActive(HWND aWnd) // [+] IRainman opt.
		{
			return m_active && // fix https://www.crash-server.com/DumpGroup.aspx?ClientID=ppa&Login=Guest&DumpGroupID=86322
			       m_active->hWnd == aWnd;
		}
		
		void setActive(HWND aWnd)
		{
#ifdef IRAINMAN_INCLUDE_GDI_OLE
			CGDIImageOle::g_ActiveMDIWindow = aWnd;
#endif
			if (!m_is_intab)
				setTop(aWnd);
			if (TabInfo* ti = getTabInfo(aWnd))
			{
				m_active = ti;
				ti->m_dirty = false;
#ifdef IRAINMAN_FAST_FLAT_TAB
				calcRows();
				SmartInvalidate();
#else
				calcRows(false);
				Invalidate(); // TODO не рисовать пока строимся/разрушаемся
#endif
			}
		}
		
		void setTop(HWND aWnd)
		{
			m_viewOrder.erase(std::find(begin(m_viewOrder), end(m_viewOrder), aWnd));
			m_viewOrder.push_back(aWnd);
			m_nextTab = --m_viewOrder.end();
		}
		
		void setCountMessages(HWND aWnd, int p_count_messages)
		{
			if (TabInfo* ti = getTabInfo(aWnd))
			{
				ti->m_count_messages = p_count_messages;
				ti->m_dirty = false;
			}
		}
		void setDirty(HWND aWnd, int p_count_messages)
		{
			if (TabInfo* ti = getTabInfo(aWnd))
			{
				bool inval = ti->update();
				if (m_active != ti)
				{
					if (p_count_messages)
					{
						ti->m_count_messages += p_count_messages;
						m_needsInvalidate = true;
						inval = true;
					}
					if (!ti->m_dirty)
					{
						ti->m_dirty = true;
						inval = true;
					}
				}
				if (inval || p_count_messages)
				{
#ifdef IRAINMAN_FAST_FLAT_TAB
					calcRows();
					SmartInvalidate();
#else
					calcRows(false);
					Invalidate();
#endif
				}
			}
		}
		
		void setIconState(HWND aWnd)
		{
			if (TabInfo* ti = getTabInfo(aWnd))
				if (ti->m_hCustomIcon == nullptr)
				{
					ti->m_bState = true;
#ifdef IRAINMAN_FAST_FLAT_TAB
					m_needsInvalidate = true;
#else
					Invalidate();
#endif
				}
		}
		void unsetIconState(HWND aWnd)
		{
			if (TabInfo* ti = getTabInfo(aWnd))
				if (ti->m_hCustomIcon == nullptr)
				{
					ti->m_bState = false;
#ifdef IRAINMAN_FAST_FLAT_TAB
					m_needsInvalidate = true;
#else
					Invalidate();
#endif
				}
		}
		
		// !SMT!-UI
		void setCustomIcon(HWND p_hWnd, HICON p_custom)
		{
			if (TabInfo* ti = getTabInfo(p_hWnd))
			{
				ti->m_hCustomIcon = p_custom;
#ifdef IRAINMAN_FAST_FLAT_TAB
				m_needsInvalidate = true;
#else
				Invalidate();
#endif
			}
		}
		
		void setColor(HWND aWnd, COLORREF p_color)
		{
			if (TabInfo* ti = getTabInfo(aWnd))
			{
				ti->m_color_pen = p_color;
#ifdef IRAINMAN_FAST_FLAT_TAB
				m_needsInvalidate = true;
#else
				Invalidate();
#endif
			}
		}
		
		void updateText(HWND aWnd, LPCTSTR text)
		{
			if (TabInfo* ti = getTabInfo(aWnd))
			{
#ifdef IRAINMAN_FAST_FLAT_TAB
				if (ti->updateText(text))
					calcRows();
#else
				ti->updateText(text);
				calcRows(false);
				Invalidate();
#endif
			}
		}
		
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_SIZE, onSize)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_PAINT, onPaint)
		MESSAGE_HANDLER(WM_LBUTTONDOWN, onLButtonDown)
		MESSAGE_HANDLER(WM_LBUTTONUP, onLButtonUp)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_MOUSEMOVE, onMouseMove)
		MESSAGE_HANDLER(WM_MOUSELEAVE, onMouseLeave)
		MESSAGE_HANDLER(WM_MBUTTONUP, onCloseTab)
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_CHEVRON, onChevron)
		COMMAND_RANGE_HANDLER(IDC_SELECT_WINDOW, IDC_SELECT_WINDOW + m_tabs.size(), onSelectWindow)
		END_MSG_MAP()
		
		LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 0;   // no background painting needed
		}
		
		LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			const int row = getRows() - (pt.y / getTabHeight() + 1);
			
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				TabInfo* t = *i;
				if (row == t->m_row &&
				        pt.x >= t->m_xpos &&
				        pt.x < t->m_xpos + t->getWidth())
				{
					// Bingo, this was clicked
					HWND hWnd = GetParent();
					if (hWnd)
					{
						if (wParam & MK_SHIFT)
							::SendMessage(t->hWnd, WM_CLOSE, 0, 0);
						else
						{
							m_moving = t;
						}
					}
					break;
				}
			}
			return 0;
		}
		
		LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			if (m_moving)
			{
				const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				const int row = getRows() - (pt.y / getTabHeight() + 1);
				
				bool moveLast = true;
				
				for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
				{
					TabInfo* t = *i;
					if (row == t->m_row &&
					        pt.x >= t->m_xpos &&
					        pt.x < t->m_xpos + t->getWidth())
					{
						// Bingo, this was clicked
						HWND hWnd = GetParent();
						if (hWnd)
						{
							if (t == m_moving)
							{
								::SendMessage(hWnd, FTM_SELECTED, (WPARAM)t->hWnd, 0);
							}
							else
							{
								//check if the pointer is on the left or right half of the tab
								//to determine where to insert the tab
								moveTabs(t, pt.x > (t->m_xpos + (t->getWidth() / 2)));
							}
						}
						moveLast = false;
						break;
					}
				}
				if (moveLast)
					moveTabs(m_tabs.back(), true);
				m_moving = nullptr;
			}
			return 0;
		}
		void activateCloseButton(bool p_is_visible, bool p_is_enable)
		{
			if (g_TabsCloseButtonEnabled)
			{
				m_bClose.ShowWindow(p_is_visible);
				m_bClose.EnableWindow(p_is_enable);
				if (p_is_visible == false && m_is_invalidate == false)
				{
					m_needsInvalidate = false;
					Invalidate();
					m_is_invalidate = true;
				}
				else
				{
					m_is_invalidate = false;
				}
			}
			else
			{
				m_bClose.EnableWindow(FALSE);
				m_bClose.ShowWindow(FALSE);
			}
		}
		
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
			
			activateCloseButton(FALSE, TRUE);
			
			ScreenToClient(&pt);
			const int row = getRows() - (pt.y / getTabHeight() + 1);
			
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				TabInfo* t = *i;
				if (row == t->m_row &&
				        pt.x >= t->m_xpos &&
				        pt.x < t->m_xpos + t->getWidth()) // TODO copy-paste
				{
					// Bingo, this was clicked, check if the owner wants to handle it...
					if (!::SendMessage(t->hWnd, FTM_CONTEXTMENU, 0, lParam))
					{
						m_closing_hwnd = t->hWnd;
						ClientToScreen(&pt);
						OMenu l_mnu;
						l_mnu.CreatePopupMenu();
						l_mnu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
						l_mnu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, m_hWnd);
					}
					break;
				}
			}
			return 0;
		}
		
		LRESULT onMouseLeave(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			if (!m_is_cur_close)
			{
				activateCloseButton(FALSE, FALSE);
			}
			return 1;
		}
		
		LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
			const int row = getRows() - (pt.y / getTabHeight() + 1);
			const int yPos = (getRows() - row - 1) * getTabHeight();
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				TabInfo* t = *i;
				if (row == t->m_row &&
				        pt.x >= t->m_xpos &&
				        pt.x < t->m_xpos + t->getWidth())
				{
					if (!CompatibilityManager::isWine())
					{
						TRACKMOUSEEVENT csTME;
						csTME.cbSize = sizeof(csTME);
						csTME.dwFlags = TME_LEAVE;
						csTME.hwndTrack = m_hWnd;
						_TrackMouseEvent(&csTME);
						
						// Bingo, the mouse was over this one
						if (g_TabsCloseButtonEnabled)
						{
							//Close Button. Visible on mouse over [+] SCALOlaz
							CRect rcs;
							m_closing_hwnd = t->hWnd;  // [+] NightOrion
							switch (WinUtil::GetTabsPosition())
							{
								case SettingsManager::TABS_LEFT:
									rcs.left = t->m_xpos + 147;
									break;
								case SettingsManager::TABS_RIGHT:
									rcs.left = t->m_xpos + 145;
									break;
								default:
									rcs.left = t->m_xpos + t->getWidth();
									break;
							}
							rcs.left -= 17;
							
							switch (WinUtil::GetTabsPosition())
							{
								case SettingsManager::TABS_TOP:
									rcs.top = yPos + 6;
									break;
								case SettingsManager::TABS_BOTTOM:
									rcs.top = yPos + 2;
									break;
								default:
									rcs.top = yPos + 4;
									break;
							}
							
							m_bClose.MoveWindow(rcs.left, rcs.top, 16, 16);
							bool l_is_enabled;
							if (
							    pt.x >= rcs.left
							    &&
							    pt.x < rcs.left + 16
							    &&
							    pt.y >= rcs.top
							    &&
							    pt.y < rcs.top + 16
							)
							{
								l_is_enabled = true;
								m_is_cur_close = true;
							}
							else
							{
								if (m_is_cur_close == true)
									activateCloseButton(FALSE, FALSE);
								l_is_enabled   = false;
								m_is_cur_close = false;
							}
							activateCloseButton(TRUE, l_is_enabled);
						}
					}
					tstring buf;
					WinUtil::GetWindowText(buf, t->hWnd);
					if (buf != m_current_tip)
					{
						m_tab_tip.DelTool(m_hWnd);
						if (!BOOLSETTING(POPUPS_DISABLED) && BOOLSETTING(POPUPS_TABS_ENABLED) && !buf.empty()) // TODO -> updateTabs
						{
							m_tab_tip.AddTool(m_hWnd, &buf[0]);
							m_tab_tip.Activate(TRUE);
						}
						m_current_tip = buf;
					}
					return 1;
				}
			}
			m_tab_tip.Activate(FALSE);
			m_current_tip.clear();
			activateCloseButton(FALSE, FALSE);
			m_is_cur_close = false;
			return 1;
		}
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			//activateCloseButton(FALSE);
			dcassert(::IsWindow(m_closing_hwnd));
			if (::IsWindow(m_closing_hwnd))
			{
				::SendMessage(m_closing_hwnd, WM_CLOSE, 0, 0);
			}
			return 0;
		}
		
		int getTabHeight() const
		{
			return m_height;
		}
		int getHeight() const
		{
			return (getRows() * getTabHeight()) + 1;
		}
		int getFill() const
		{
			return (getTabHeight() + 1) / 2;
		}
		
		int getRows() const
		{
			return m_rows;
		}
		
#ifdef IRAINMAN_FAST_FLAT_TAB
		void calcRows()
#else
		void calcRows(bool inval = true)
#endif
		{
			CRect rc;
			GetClientRect(rc);
			int r = 1;
			int w = 0;
			bool notify = false;
#ifndef IRAINMAN_FAST_FLAT_TAB
			bool needInval = false;
#endif
			
			const int& l_TabPos = WinUtil::GetTabsPosition();
			const int l_MaxTabRows = (l_TabPos == SettingsManager::TABS_TOP || l_TabPos == SettingsManager::TABS_BOTTOM) ? SETTING(MAX_TAB_ROWS) : (rc.Height() / getTabHeight());
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				TabInfo* ti = *i;
				if (r != 0 && w + ti->getWidth() > rc.Width())
				{
					if (r == l_MaxTabRows)
					{
						notify |= m_rows != r;
						m_rows = r;
						r = 0;
						m_chevron.EnableWindow(TRUE);
						m_chevron.ShowWindow(TRUE);
					}
					else
					{
						r++;
						w = 0;
					}
				}
				//dcdebug("FlatTabCtrl::calcRows::r = %d\t", r);
#ifndef IRAINMAN_FAST_FLAT_TAB
				needInval |= ti->row != r - 1;
#endif
				ti->m_row = r - 1;
				//dcdebug("FlatTabCtrl::calcRows::ti->row = %d\t", ti->row);
				ti->m_xpos = w;
				//dcdebug("FlatTabCtrl::calcRows::ti->xpos = %d\n", ti->xpos);
				w += ti->getWidth();
			}
			
			if (r != 0)
			{
				m_chevron.EnableWindow(FALSE);
				m_chevron.ShowWindow(FALSE);
				notify |= (m_rows != r);
				m_rows = r;
			}
			
			activateCloseButton(FALSE, FALSE);
			
			if (notify)
			{
				::SendMessage(GetParent(), FTM_ROWS_CHANGED, 0, 0);
			}
#ifdef IRAINMAN_FAST_FLAT_TAB
			m_needsInvalidate = true;
#else
			if (needInval && inval)
				Invalidate();
#endif
		}
		
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			m_chevron.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_PUSHBUTTON , 0, IDC_CHEVRON);
			m_chevron.SetWindowText(_T("\u00bb"));
			
			// [+] SCALOlaz : Create a Close Button
			CImageList l_closeImages;
			ResourceLoader::LoadImageList(IDR_CLOSE_PNG, l_closeImages, 16, 16);
			m_bClose.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | BS_FLAT, 0, IDC_CLOSE_WINDOW);
			m_bClose.SetImageList(l_closeImages);
			m_bClose.SetImages(0, 1, 2, 3);
			g_mnu.CreatePopupMenu();
			
			m_tab_tip.Create(m_hWnd, rcDefault, NULL, TTS_ALWAYSTIP | TTS_NOPREFIX);
			
			CDCHandle dc(::GetDC(m_hWnd)); // Error ~CDC() call DeleteDC
			/*
			http://msdn.microsoft.com/en-us/library/windows/desktop/dd162920%28v=vs.85%29.aspx
			Remarks
			The application must call the ReleaseDC function for each call to the GetWindowDC function and for each call
			to the GetDC function that retrieves a common DC.
			An application cannot use the ReleaseDC function to release a DC that was created by calling
			the CreateDC function; instead, it must use the DeleteDC function. ReleaseDC must be called from the same thread that called GetDC.
			*/
			{
				CSelectFont l_font(dc, Fonts::g_systemFont); //-V808
				//Получаем высоту шрифта
				m_height_font = WinUtil::getTextHeight(dc);
				//Высота вкладки равна высоте шрифта + отступы
				m_height = m_height_font + 4;
				//Если шрифт маленький, то высота вклаки 24
				if (m_height_font < 18)
				{
					m_height = 23;
				}
			}
			int l_res = ::ReleaseDC(m_hWnd, dc);
			dcassert(l_res);
			return 0;
		}
		
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			calcRows();
			
			activateCloseButton(FALSE, FALSE);
			
			const SIZE sz = { LOWORD(lParam), HIWORD(lParam) };
			switch (WinUtil::GetTabsPosition())
			{
				case SettingsManager::TABS_TOP:
				case SettingsManager::TABS_BOTTOM:
					m_chevron.MoveWindow(sz.cx - 14, 1, 14, getHeight() - 2);
					break;
				default:
					m_chevron.MoveWindow(0, sz.cy - 15, 150, 15);
					break;
			}
			return 0;
		}
		
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
#ifdef IRAINMAN_FAST_FLAT_TAB
			m_allowInvalidate = false;
#endif
			activateCloseButton(FALSE, FALSE);
			CRect rc;
			
			if (GetUpdateRect(&rc, FALSE))
			{
				CPaintDC l_dc(m_hWnd);
				CMemoryDC dcMem(l_dc, rc);
				
				HBRUSH hBr = GetSysColorBrush(COLOR_BTNFACE);
				dcMem.SelectBrush(hBr);
				dcMem.BitBlt(rc.left, rc.top, rc.Width(), rc.Height(), NULL, 0, 0, PATCOPY);
				CSelectFont l_font(dcMem, Fonts::g_systemFont); //-V808
				//ATLTRACE("%d, %d\n", rc.left, rc.right);
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
				std::unique_ptr<Gdiplus::Graphics> graphics(new Gdiplus::Graphics(dcMem));
				//Режим сглаживания линий (не менять, другие не работают)
				graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone /*SmoothingModeHighQuality*/);
#endif
				//dcdebug("FlatTabCtrl::m_active = %d\n", m_active);
				for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
				{
					TabInfo* t = *i;
					if (t && t->m_row != -1 && t->m_xpos < rc.right && t->m_xpos + t->getWidth() >= rc.left)
					{
						//dcdebug("FlatTabCtrl::drawTab::t = %d\n", t);
						drawTab(dcMem,
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
						        graphics,
#endif
						        t, t->m_xpos, t->m_row, t == m_active);
					}
				}
				
				const int& l_tabspos = WinUtil::GetTabsPosition();
				if (l_tabspos == SettingsManager::TABS_TOP || l_tabspos == SettingsManager::TABS_BOTTOM)
				{
					int r = 0;
					/*Различное колличество контуров разделителей для положения сверху/снизу:
					int r = (l_tabspos == SettingsManager::TABS_TOP) ? 0 : 1;
					no needs */
					for (; r < getRows(); r++) //Рисуем контур разделитель вкладок
					{
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
						graphics->DrawLine(&g_pen_conter_side, rc.left, r * getTabHeight(), rc.right, r * getTabHeight());
#else
						dc.MoveTo(rc.left, r * getTabHeight());
						dc.LineTo(rc.right, r * getTabHeight());
#endif // IRAINMAN_USE_GDI_PLUS_TAB
					}
				}
#ifndef IRAINMAN_USE_GDI_PLUS_TAB
				/* TODO move to drawTab
				                if(drawActive)
				                {
				                    dcassert(m_active);
				                    drawTab(dc, m_active, m_active->xpos, m_active->row, true);
				                    dc.SelectPen(m_active->pen);
				                    int y = (m_rows - m_active->row - 1) * getTabHeight();
				                    dc.MoveTo(m_active->xpos, y);
				                    dc.LineTo(m_active->xpos + m_active->getWidth(), y);
				                }
				
				                dc.SelectPen(oldpen);
				*/
#endif // IRAINMAN_USE_GDI_PLUS_TAB
			}
#ifdef IRAINMAN_FAST_FLAT_TAB
			m_allowInvalidate = true;
#endif
			return 0;
		}
		
		LRESULT onChevron(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			while (g_mnu.GetMenuItemCount() > 0)
			{
				g_mnu.RemoveMenu(0, MF_BYPOSITION);
			}
			int n = 0;
			CRect rc;
			GetClientRect(&rc);
			CMenuItemInfo mi;
			mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA | MIIM_STATE;
			mi.fType = MFT_STRING | MFT_RADIOCHECK;
			
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				TabInfo* ti = *i;
				if (ti->m_row == -1)
				{
					mi.dwTypeData = (LPTSTR)ti->name.data();
					mi.dwItemData = (ULONG_PTR)ti->hWnd;
					mi.fState = MFS_ENABLED | (ti->m_dirty ? MFS_CHECKED : 0);
					mi.wID = IDC_SELECT_WINDOW + n;
					g_mnu.InsertMenuItem(n++, TRUE, &mi);
				}
			}
			
			POINT pt;
			m_chevron.GetClientRect(&rc);
			pt.x = rc.right - rc.left;
			pt.y = 0;
			m_chevron.ClientToScreen(&pt);
			
			g_mnu.TrackPopupMenu(TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			return 0;
		}
		
		LRESULT onSelectWindow(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			activateCloseButton(FALSE, FALSE);
			
			CMenuItemInfo mi;
			mi.fMask = MIIM_DATA;
			
			g_mnu.GetMenuItemInfo(wID, FALSE, &mi);
			HWND hWnd = GetParent();
			if (hWnd)
			{
				SendMessage(hWnd, FTM_SELECTED, (WPARAM)mi.dwItemData, 0);
			}
			return 0;
		}
		
		void SwitchTo(bool next = true)
		{
			activateCloseButton(FALSE, FALSE);
			
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				if ((*i)->hWnd == m_active->hWnd)
				{
					if (next)
					{
						++i;
						if (i == m_tabs.end())
							i = m_tabs.begin();
					}
					else
					{
						if (i == m_tabs.begin())
							i = m_tabs.end();
						--i;
					}
					setActive((*i)->hWnd);
					::SetWindowPos((*i)->hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
					break;
				}
			}
		}
		
		LRESULT onCloseTab(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			activateCloseButton(FALSE, FALSE);
			const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			const int row = getRows() - (pt.y / getTabHeight() + 1);
			
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				TabInfo* t = *i;
				if (row == t->m_row &&
				        pt.x >= t->m_xpos &&
				        pt.x < t->m_xpos + t->getWidth())
				{
					// Bingo, this was clicked
					HWND hWnd = GetParent();
					if (hWnd)
					{
						::SendMessage(t->hWnd, WM_CLOSE, 0, 0);
					}
					break;
				}
			}
			return 0;
		}
		
		void updateTabs()
		{
			g_color_shadow = GetSysColor(COLOR_BTNSHADOW);
			g_color_light = GetSysColor(COLOR_BTNHIGHLIGHT);
			g_color_face =  GetSysColor(COLOR_WINDOW); //SETTING(TAB_SELECTED_BORDER_COLOR);
			g_color_filllight = GetSysColor(COLOR_HIGHLIGHT);
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
			g_color_shadow_gdi = Gdiplus::Color(GetRValue(g_color_shadow), GetGValue(g_color_shadow), GetBValue(g_color_shadow));
			g_color_light_gdi = Gdiplus::Color(GetRValue(g_color_light), GetGValue(g_color_light), GetBValue(g_color_light));
			g_color_face_gdi = Gdiplus::Color(GetRValue(g_color_face), GetGValue(g_color_face), GetBValue(g_color_face));
			g_color_filllight_gdi = Gdiplus::Color(GetRValue(g_color_filllight), GetGValue(g_color_filllight), GetBValue(g_color_filllight));
			
			//Создание "ручки" для контура разделителя
			g_pen_conter_side.SetColor(Gdiplus::Color(120, GetRValue(g_color_shadow), GetGValue(g_color_shadow), GetBValue(g_color_shadow)));
#endif // IRAINMAN_USE_GDI_PLUS_TAB
			g_magic_width = ((WinUtil::GetTabsPosition() == SettingsManager::TABS_LEFT || WinUtil::GetTabsPosition() == SettingsManager::TABS_RIGHT) ? 29 : 0);
			
			g_TabsCloseButtonEnabled = BOOLSETTING(TABS_CLOSEBUTTONS);
			if (!g_TabsCloseButtonEnabled && m_bClose.IsWindow())
			{
				m_bClose.ShowWindow(FALSE);
			}
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				TabInfo* t = *i;
				t->update(true);
			}
			m_needsInvalidate = true;
		}
		
	private:
		class TabInfo
#ifdef _DEBUG
			: public boost::noncopyable
#endif
		{
			public:
				typedef vector<TabInfo*> List;
				
				static const size_t MAX_LENGTH = 20;
				
				TabInfo(HWND p_Wnd, COLORREF p_color, uint16_t p_icon, uint16_t p_stateIcon, bool p_is_update) :
					hWnd(p_Wnd), m_len(0), m_xpos(0), m_row(0), m_dirty(false),
					m_hCustomIcon(nullptr), m_bState(false), m_mini(false),
					m_color_pen(p_color), m_icon(p_icon), m_stateIcon(p_stateIcon), m_count_messages(0)
				{
					memzero(&m_size, sizeof(m_size));
					name[0] = 0;
					if (p_is_update)
					{
						update();
					}
				}
				
				~TabInfo()
				{
				}
				HWND hWnd;
				COLORREF m_color_pen;
				
				LocalArray<TCHAR, MAX_LENGTH> name; // TODO - заменить на tstring
				size_t m_len;
				
				SIZE m_size;
				int  m_xpos;
				int  m_row;
				
				uint16_t m_icon;
				uint16_t m_stateIcon;
				uint16_t m_count_messages;
				
				HICON m_hCustomIcon; // !SMT!-UI custom icon should be set / freed outside this class
				bool m_bState;
				bool m_mini;
				bool m_dirty;
				bool update(const bool always = false)
				{
					LocalArray<TCHAR, MAX_LENGTH> textNew;
					LONG_PTR ptr = ::GetWindowLongPtr(hWnd, GWLP_USERDATA);
					if (ptr)
					{
						tstring* pShortName = reinterpret_cast<tstring*>(ptr);
						if (pShortName)
						{
							m_len = min(static_cast<size_t>(MAX_LENGTH - 1), pShortName->size());
							if (m_len)
							{
								_tcsnccpy(textNew.data(), pShortName->c_str(), m_len);
							}
							textNew[m_len] = '\0';
						}
					}
					if (textNew[0] == '\0')
					{
						m_len = (size_t)::GetWindowTextLength(hWnd); // diff[2]
						if (m_len >= MAX_LENGTH)
						{
							::GetWindowText(hWnd, textNew.data(), MAX_LENGTH); // diff[0]
							textNew[MAX_LENGTH - 2] = _T('…');
							m_len = MAX_LENGTH - 1;
						}
						else
						{
							::GetWindowText(hWnd, textNew.data(), MAX_LENGTH); // diff[3]
						}
					}
					if (!always && _tcscmp(name.data(), textNew.data()) == 0)
						return false;
						
					_tcscpy(name.data(), textNew.data()); // diff[1]
					updateTextMetrics();
					return true;
				}
				
				bool updateText(const LPCTSTR text)
				{
					// TODO - кривизна
					const size_t old_len = m_len;
					LocalArray<TCHAR, MAX_LENGTH> textNew;
					textNew[0] = '\0';
					LONG_PTR ptr = ::GetWindowLongPtr(hWnd, GWLP_USERDATA);
					if (ptr)
					{
						tstring* pShortName = reinterpret_cast<tstring*>(ptr);
						if (pShortName)
						{
							m_len = min(static_cast<size_t>(MAX_LENGTH - 1), pShortName->size());
							if (m_len)
							{
								_tcsnccpy(textNew.data(), pShortName->c_str(), m_len);
							}
							textNew[m_len] = '\0';
							_tcscpy(name.data(), textNew.data()); // diff[1]
						}
					}
					if (textNew[0] == '\0')
					{
						m_len = _tcslen(text); // diff[2]
						if (m_len >= MAX_LENGTH)
						{
							::_tcsncpy(name.data(), text, MAX_LENGTH - 2); // diff[0]
							name[MAX_LENGTH - 2] = _T('…');
							name[MAX_LENGTH - 1] = _T('\0');
							m_len = MAX_LENGTH - 1;
						}
						else
						{
							_tcscpy(name.data(), text); // diff[3]
						}
					}
					if (old_len != m_len)
					{
						updateTextMetrics();
					}
					return old_len != m_len;
				}
			private:
				void updateTextMetrics()
				{
					switch (WinUtil::GetTabsPosition())
					{
						case SettingsManager::TABS_TOP:
						case SettingsManager::TABS_BOTTOM:
						{
							if (m_len)
							{
								CDCHandle dc(::GetDC(hWnd)); // Error ~CDC() call DeleteDC
								{
									CSelectFont l_font(dc, Fonts::g_systemFont); //-V808
									dc.GetTextExtent(name.data(), m_len, &m_size); //-V107
								}
								if (g_TabsCloseButtonEnabled)
								{
									m_size.cx     += 10;
								}
								int l_res = ::ReleaseDC(hWnd, dc);
								dcassert(l_res);
							}
						}
						break;
						default:
						{
							m_size.cx = 150;
						}
						break;
					}
				}
			public:
				int getWidth() const
				{
					return (!m_mini ? m_size.cx : 0) + FT_EXTRA_SPACE + 10 /*(m_hIcon != nullptr ? 10 : 0)*/ + 4;  //-V112
				}
		};
		
		void moveTabs(TabInfo* aTab, bool after)
		{
			if (!m_moving)
				return;
				
			TabInfo::List::const_iterator i;
			//remove the tab we're m_moving
			for (auto j = m_tabs.begin(); j != m_tabs.end(); ++j)
			{
				if ((*j) == m_moving)
				{
					m_tabs.erase(j);
					break;
				}
			}
			
			//find the tab we're going to insert before or after
			for (i = m_tabs.begin(); i != m_tabs.end(); ++i)
			{
				if ((*i) == aTab)
				{
					if (after)
						++i;
					break;
				}
			}
			
			m_tabs.insert(i, m_moving);
			m_moving = nullptr;
#ifdef IRAINMAN_FAST_FLAT_TAB
			calcRows();
			SmartInvalidate();
#else
			calcRows(false);
			Invalidate();
#endif
		}
		
	private:
		HWND m_closing_hwnd;
		CButton m_chevron;
		WTL::CBitmapButton m_bClose;
		CFlyToolTipCtrl m_tab_tip;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
//		CPen black;
//		CPen white;
#endif

		int m_rows;
		int m_height;
		int m_height_font;
		
		tstring m_current_tip;
		bool m_is_cur_close;
		
		TabInfo* m_active;
		TabInfo* m_moving;
		typename TabInfo::List m_tabs;
		
		typedef deque<HWND> WindowList; // [!] IRainman opt: change list to deque.
		typedef WindowList::const_iterator WindowIter;
		
		WindowList m_viewOrder;
		WindowIter m_nextTab;
		
		bool m_is_intab;
		bool m_is_invalidate;
#ifdef IRAINMAN_FAST_FLAT_TAB
		bool m_needsInvalidate;
		bool m_allowInvalidate;
#endif
	public:
		TabInfo* getTabInfo(HWND aWnd) const
		{
			for (auto i = m_tabs.cbegin(); i != m_tabs.cend(); ++i)
			{
				if ((*i)->hWnd == aWnd)
					return *i;
			}
			// dcassert(0);
			return nullptr;
		}
		
		/**
		 * Draws a tab
		 * @return The width of the tab
		 */
		void drawTab(CDC& dc,
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
		             std::unique_ptr<Gdiplus::Graphics>& graphics,
#endif
		             TabInfo* tab, int pos, const int row, const bool aActive)
		{
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
			if (graphics->GetLastStatus() != Gdiplus::Ok) //[+]PPA
				return;
#endif
			const int ypos = (getRows() - row - 1) * getTabHeight();
			int magic_width = tab->getWidth() - g_magic_width;
			
			const int tabAnim = aActive ? 0 : 1;
			//const int tabAnim = 0;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
			//Контур вкладки
			unique_ptr<Gdiplus::GraphicsPath> l_tabsPath;
			// TODO вынести большую часть проверок в updateTabs()
			//Цвет заливки в заивисимости от настроек и состояния вкладки
			
			unique_ptr<Gdiplus::LinearGradientBrush> l_tabBrush;
			if (CompatibilityManager::isOsVistaPlus())
			{
				l_tabBrush = unique_ptr<Gdiplus::LinearGradientBrush>(
				                 new Gdiplus::LinearGradientBrush(Gdiplus::RectF(pos, ((WinUtil::GetTabsPosition() == SettingsManager::TABS_TOP) ? (ypos + tabAnim + 3) : ypos), magic_width, ((SETTING(TABS_POS) == 0 || SETTING(TABS_POS) == 1) ? (m_height - tabAnim - 2) : m_height)),
				                                                  g_color_light,
				                                                  g_color_face,
				                                                  (SETTING(TABS_POS) == 0 || SETTING(TABS_POS) == 1) ? Gdiplus::LinearGradientModeVertical : Gdiplus::LinearGradientModeHorizontal)
				             );
				l_tabsPath = unique_ptr<Gdiplus::GraphicsPath>(new Gdiplus::GraphicsPath);
			}
			if (l_tabBrush)
			{
				DWORD l_color_tab;
				if (!tab->m_bState)
				{
					if (aActive)
						l_color_tab = SETTING(TAB_SELECTED_COLOR);
					else if (tab->m_dirty)
						l_color_tab = SETTING(TAB_ACTIVITY_COLOR);
					else
						l_color_tab = g_color_face;
				}
				else
				{
					l_color_tab = SETTING(TAB_OFFLINE_COLOR);
				}
				
				/*              const DWORD l_color_tab;
				                if(!g_TabsGdiPlusEnabled)
				                  l_color_tab  = (aActive ? g_color_filllight : g_color_face)
				                else
				                {
				                        if(!tab->m_bState)
				                       if(aActive)
				                           l_color_tab = SETTING(TAB_SELECTED_COLOR)
				                      else
				                        if(tab->m_dirty)
				                            l_color_tab = SETTING(TAB_ACTIVITY_COLOR);
				                        else
				                            l_color_tab = g_color_face;
				                  }
				                 else
				                      l_color_tab = SETTING(TAB_OFFLINE_COLOR);
				*/
				//Градиент заливки
				//Смена цветов градиента для вкладок снизу
				const bool l_isBottom = WinUtil::GetTabsPosition() == SettingsManager::TABS_BOTTOM;
				const Gdiplus::Color l_colors[] =
				{
					l_isBottom ? g_color_face_gdi : g_color_light_gdi,
					Gdiplus::Color(90, GetRValue(l_color_tab), GetGValue(l_color_tab), GetBValue(l_color_tab)),
					Gdiplus::Color(115, GetRValue(l_color_tab), GetGValue(l_color_tab), GetBValue(l_color_tab)),
					/*l_isBottom ? g_color_light_gdi : g_color_face_gdi*/
					Gdiplus::Color(155, GetRValue(l_color_tab), GetGValue(l_color_tab), GetBValue(l_color_tab))
				};
				static const Gdiplus::REAL g_positions[] =
				{
					0.0f,
					0.5f,
					0.90f,
					1.0f
				};
				l_tabBrush->SetInterpolationColors(l_colors, g_positions, _countof(l_colors));
				//Конец градиента заливки
			}
			
#else
			
//			HPEN oldpen = dc.SelectPen(black);
			
			POINT p[4];
			dc.BeginPath();
			dc.MoveTo(pos, ypos);
			p[0].x = pos + tab->getWidth();
			p[0].y = ypos;
			p[1].x = pos + tab->getWidth();
			p[1].y = ypos + getTabHeight();
			p[2].x = pos;
			p[2].y = ypos + getTabHeight();
			p[3].x = pos;
			p[3].y = ypos;
			
			dc.PolylineTo(p, 4);
			dc.CloseFigure();
			dc.EndPath();
			
			//HBRUSH hBr = GetSysColorBrush(aActive ? COLOR_WINDOW : COLOR_BTNFACE);
			HBRUSH hBr = aActive ? CreateSolidBrush(SETTING(TAB_SELECTED_COLOR)) : GetSysColorBrush(OperaColors::brightenColor(COLOR_BTNFACE, 0.5f));
			HBRUSH oldbrush = dc.SelectBrush(hBr);
			
			dc.FillPath();
			
#endif // IRAINMAN_USE_GDI_PLUS_TAB
			
			int height_plus = 0;
			int height_plus_ico = 0;
			int l_tabs_x_space = 0;     // Пикселов между кнопками в ряду
			switch (WinUtil::GetTabsPosition())
			{
					// pos + 1 : делает смещение левого края вкладки, чтобы визуально отделить следующую отрисованную вкладку от предыдущей - 2 отдельные линии
					// m_height : высота вкладки. Рассчитана где-то выше, от размера шрифта
					// tabAnim : от 0 до x  колво пикселов, на которое изменять высоту вкладок для выбранных
					// для вкладок TOP
				case SettingsManager::TABS_TOP:
				{
					//Расчёт положения иконки и текста в зависимости от высоты вкладки
					height_plus = (m_height + 3 + tabAnim - m_height_font) / 2;
					height_plus_ico = (m_height + 4 + tabAnim - 16) / 2; //-V112
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
					if (l_tabsPath)
					{
						l_tabsPath->AddLine(pos/* + 1*/, ypos + m_height + 1, pos/* + 1*/, ypos + 3 + tabAnim);
						l_tabsPath->AddLine(pos/* + 1*/, ypos + 3 + tabAnim, pos + magic_width - l_tabs_x_space - (aActive && l_tabs_x_space == 0 ? 1 : 0), ypos + 3 + tabAnim); //-V112
						l_tabsPath->AddLine(pos + magic_width - l_tabs_x_space - (aActive && l_tabs_x_space == 0 ? 1 : 0), ypos + 3 + tabAnim, pos + magic_width - l_tabs_x_space - (aActive && l_tabs_x_space == 0 ? 1 : 0), ypos + m_height + 1);
						if (!aActive)   // Отделяем вкладку как неактивную
							l_tabsPath->AddLine(pos/* + 1*/, ypos + m_height + 1, pos + magic_width - l_tabs_x_space, ypos + m_height + 1);
					}
#endif // IRAINMAN_USE_GDI_PLUS_TAB
				}
				break;
				case SettingsManager::TABS_BOTTOM:
				{
					//Расчёт положения иконки и текста в зависимости от высоты вкладки
					height_plus = (m_height - 2 - tabAnim - m_height_font) / 2;
					height_plus_ico = (m_height - 2 - tabAnim - 16) / 2;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
					if (l_tabsPath)
					{
						l_tabsPath->AddLine(pos/* + 1*/, ypos, pos/* + 1*/, ypos + m_height - 2 - tabAnim);
						l_tabsPath->AddLine(pos/* + 1*/, ypos + m_height - 2 - tabAnim, pos + magic_width - l_tabs_x_space - (aActive && l_tabs_x_space == 0 ? 1 : 0), ypos + m_height - 2 - tabAnim); //-V112
						l_tabsPath->AddLine(pos + magic_width - l_tabs_x_space - (aActive && l_tabs_x_space == 0 ? 1 : 0), ypos + m_height - 2 - tabAnim, pos + magic_width - l_tabs_x_space - (aActive && l_tabs_x_space == 0 ? 1 : 0), ypos);
						if (!aActive)   // Отделяем вкладку как неактивную
							l_tabsPath->AddLine(pos + magic_width - l_tabs_x_space, ypos, pos/* + 1*/, ypos);
					}
#endif // IRAINMAN_USE_GDI_PLUS_TAB
				}
				break;
				case SettingsManager::TABS_LEFT:
				{
					//Расчёт положения иконки и текста в зависимости от высоты вкладки
					int l_tab_height = m_height - (l_tabs_x_space / 2);
					height_plus = (l_tab_height + 1 - m_height_font) / 2;
					height_plus_ico = (l_tab_height + 2 - 16) / 2;
					magic_width -= 5;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
					if (l_tabsPath)
					{
						l_tabsPath->AddLine(pos + 1 + magic_width + 1, ypos + 1, pos + 1 + (tabAnim * 2), ypos + 1);
						l_tabsPath->AddLine(pos + 1 + (tabAnim * 2), ypos + 1, pos + 1 + (tabAnim * 2), ypos + l_tab_height);
						l_tabsPath->AddLine(pos + 1 + (tabAnim * 2), ypos + l_tab_height, pos + 1 + magic_width + 1, ypos + l_tab_height);
						if (!aActive)   // Отделяем вкладку как неактивную
							l_tabsPath->AddLine(pos + 1 + magic_width + 1, ypos + l_tab_height, pos + 1 + magic_width + 1, ypos + 1); //-V112
					}
#endif // IRAINMAN_USE_GDI_PLUS_TAB
				}
				break;
				case SettingsManager::TABS_RIGHT:
				{
					//Расчёт положения иконки и текста в зависимости от высоты вкладки
					int l_tab_height = m_height - (l_tabs_x_space / 2);
					height_plus = (l_tab_height + 1 - m_height_font) / 2;
					height_plus_ico = (l_tab_height + 2 - 16) / 2;
					magic_width -= 5;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
					if (l_tabsPath)
					{
						l_tabsPath->AddLine(pos, ypos + l_tab_height, pos + magic_width - (tabAnim * 2), ypos + l_tab_height);
						l_tabsPath->AddLine(pos + magic_width - (tabAnim * 2), ypos + l_tab_height, pos + magic_width - (tabAnim * 2), ypos + 1); //-V112
						l_tabsPath->AddLine(pos + magic_width - (tabAnim * 2), ypos + 1, pos, ypos + 1);
						if (!aActive)   // Отделяем вкладку как неактивную
							l_tabsPath->AddLine(pos, ypos + 1, pos, ypos + l_tab_height);
					}
#endif // IRAINMAN_USE_GDI_PLUS_TAB
				}
				break;
				default:
					break;
			}
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
			//Заливка вкладки
			if (l_tabBrush && l_tabsPath)
			{
				graphics->FillPath(l_tabBrush.get(), l_tabsPath.get());
				//Отрисовка контура поверх заливки
				//Создание "ручек" для контура - падаем под XP
				if (aActive)
				{
					const auto l_rgb_active =  SETTING(TAB_SELECTED_BORDER_COLOR);
					Gdiplus::Pen l_pen(Gdiplus::Color(GetRValue(l_rgb_active), GetGValue(l_rgb_active), GetBValue(l_rgb_active)), 1); // Толщина
					l_pen.SetDashStyle(Gdiplus::DashStyleSolid);
					graphics->DrawPath(&l_pen, l_tabsPath.get());
				}
				else
				{
					Gdiplus::Pen l_pen(Gdiplus::Color(160, 160, 160) , 1);
					l_pen.SetDashStyle(Gdiplus::DashStyleSolid); //DashDot, Dot
					graphics->DrawPath(&l_pen, l_tabsPath.get());
				}
			}
			
			
//////////////////
#if 0
			{
				POINT p[4];
				p[0].x = pos + tab->getWidth();
				p[0].y = ypos;
				p[1].x = pos + tab->getWidth();
				p[1].y = ypos + getTabHeight() - 1;
				p[2].x = pos;
				p[2].y = ypos + getTabHeight() - 1;
				p[3].x = pos;
				p[3].y = ypos;
				CPen l_pen;
				l_pen.CreatePen(PS_SOLID, 1, tab->m_color_pen);
				HPEN oldpen = dc.SelectPen(l_pen);
				if (tab->m_row != (m_rows - 1))
				{
					dc.MoveTo(p[3]);
					dc.LineTo(p[0]);
				}
				int sep_cut = aActive ? 0 : 2;
				dc.MoveTo(p[0].x, p[0].y + sep_cut);
				dc.LineTo(p[1].x, p[1].y - sep_cut);
				dc.MoveTo(p[2].x, p[2].y - sep_cut);
				dc.LineTo(p[3].x, p[3].y + sep_cut);
				if (aActive)
				{
					dc.MoveTo(p[1]);
					dc.LineTo(p[2]);
				}
				dc.SelectPen(oldpen);
			}
#endif
#if 0
			RECT l_rec;
			l_rec.left = pos + 2;
			l_rec.right = pos + tab->getWidth();
			l_rec.bottom = ypos + 4;
			l_rec.top = ypos + getTabHeight() - (aActive ? 1 : 0);
			
			// dc.DrawEdge(&l_rec, aActive ? EDGE_RAISED : EDGE_BUMP, BF_RECT);
			if (aActive)
			{
				dc.DrawEdge(&l_rec, EDGE_RAISED, BF_RECT);
			}
#endif
			if (aActive)
			{
				//DeleteObject(dc.SelectBrush(oldbrush)); // ???  http://www.cracklab.ru/pro/cpp.php?r=beginners&d=zgrt182
				//
				// ВАЖНО
				// Что не надо делать в WinAPI:
				// Удалять (DeleteObject) объект, полученный по SelectObject.
			}
			else
			{
				//dc.SelectBrush(oldbrush);
			}
////////////
#else
			if (tab->m_row != (m_rows - 1))
			{
				dc.MoveTo(p[3]);
				dc.LineTo(p[0]);
			}
			CPen l_pen;
			l_pen.CreatePen(PS_SOLID, 1, tab->m_color_pen);
			HPEN oldpen = dc.SelectPen(l_pen);
			const int sep_cut = aActive ? 0 : 2;
			dc.MoveTo(p[0].x, p[0].y + sep_cut);
			dc.LineTo(p[1].x, p[1].y - sep_cut);
			dc.MoveTo(p[2].x, p[2].y - sep_cut);
			dc.LineTo(p[3].x, p[3].y + sep_cut);
			if (aActive)
			{
				dc.MoveTo(p[1]);
				dc.LineTo(p[2]);
//					dc.SelectPen(white);
			}
			dc.SelectPen(oldpen);
			if (aActive)
			{
				DeleteObject(dc.SelectBrush(oldbrush)); // ???  http://www.cracklab.ru/pro/cpp.php?r=beginners&d=zgrt182
				//
				// ВАЖНО
				// Что не надо делать в WinAPI:
				// Удалять (DeleteObject) объект, полученный по SelectObject.
			}
			else
			{
				dc.SelectBrush(oldbrush);
			}
#endif // IRAINMAN_USE_GDI_PLUS_TAB

			dc.SetBkMode(TRANSPARENT);
			
			// [!] SSA http://code.google.com/p/flylinkdc/issues/detail?id=394 - проблема в случае с Лог файлом. Почему нет иконки??
			std::unique_ptr<HIconWrapper> l_tmp_hIcon;
			HICON l_hIcon = tab->m_hCustomIcon;
			if (l_hIcon == nullptr)
			{
				if (tab->m_bState && tab->m_stateIcon)
				{
					if (tab->m_stateIcon == IDR_HUB_OFF)
					{
						l_hIcon = *WinUtil::g_HubOffIcon.get();
					}
					else if (tab->m_stateIcon == IDR_HUB)
					{
						l_hIcon = *WinUtil::g_HubOnIcon.get();
					}
					else
					{
						l_tmp_hIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(tab->m_stateIcon));
					}
				}
				else if (tab->m_icon)
				{
					if (tab->m_icon == IDR_HUB_OFF)
					{
						l_hIcon = *WinUtil::g_HubOffIcon.get();
					}
					else if (tab->m_icon == IDR_HUB)
					{
						l_hIcon = *WinUtil::g_HubOnIcon.get();
					}
					else
					{
						l_tmp_hIcon = std::unique_ptr<HIconWrapper>(new HIconWrapper(tab->m_icon));
					}
				}
				if (l_hIcon == nullptr && l_tmp_hIcon)
					l_hIcon = * l_tmp_hIcon.get();
			}
			
			pos = pos + getFill() / 2 + FT_EXTRA_SPACE / 2;
			if (l_hIcon)
				pos += 10;
				
			// TODO вынести большую часть проверок в updateTabs()
//			COLORREF oldclr = tab->m_bState ? dc.SetTextColor(g_color_shadow) : dc.SetTextColor(GetSysColor(COLOR_BTNTEXT)); // [+] Sergey Shuhskanov and Dmitriy F
			const COLORREF oldclr = !tab->m_bState ?
			                        (aActive ?
			                         dc.SetTextColor(SETTING(TAB_SELECTED_TEXT_COLOR))
			                         :
			                         (tab->m_dirty ?
			                          dc.SetTextColor(SETTING(TAB_ACTIVITY_TEXT_COLOR))
			                          :
			                          dc.SetTextColor(GetSysColor(COLOR_BTNTEXT))))
				                        :
				                        dc.SetTextColor(SETTING(TAB_OFFLINE_TEXT_COLOR));    //[~] SCALOlaz
				                        
			//Цвет шрифта в зависимости от состояния юзера или хаба
			//DWORD color_text_tab = tab->m_bState ? g_color_shadow : GetSysColor(COLOR_BTNTEXT);
			
			//Кисть для заливки шрифта
			//SolidBrush textBrush(Color(GetRValue(color_text_tab),GetGValue(color_text_tab),GetBValue(color_text_tab)));
			
			//Шрифт на вкладках
			//HFONT m_Font = SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &m_Font, SPIF_SENDWININICHANGE);
			//Font textFont(graphics->GetHDC(), m_Font);
			//Font textFont(L"Tahoma", 8, 0);
			
			
			if (!tab->m_mini)
		{
				dc.TextOut(pos, ypos + height_plus, tab->name.data(), tab->m_len); // [~] Sergey Shuhskanov //-V107
			}
			
			int width_plus_ico = 0; // - X position for COUNTER on LEFT\RIGHT tabs
			// Я нихера не понял откуда в координатах взялись эти ~35 пикс, но только УБАВИВ их мы увидим счётчики на вкладках
			if (WinUtil::GetTabsPosition() == SettingsManager::TABS_LEFT)
			{
				width_plus_ico = 32;
			}
			else if (WinUtil::GetTabsPosition() == SettingsManager::TABS_RIGHT)
			{
				width_plus_ico = 35;
			}
			if (tab->m_count_messages)
			{
				dc.SetTextColor(RGB(255, 0, 0));
				CSelectFont l_half_font(dc, Fonts::g_halfFont); //-V808
				const auto l_cnt = Text::toT("+" + Util::toString(tab->m_count_messages));
				dc.TextOut(pos + tab->m_size.cx - width_plus_ico - (l_cnt.size() * 3), ypos + height_plus_ico , l_cnt.c_str(), l_cnt.length());
			}
			
			if (l_hIcon)
			{
				const BOOL l_res = DrawIconEx(dc.m_hDC, pos - 18, ypos + height_plus_ico, l_hIcon, 16, 16, NULL, NULL, DI_NORMAL | DI_COMPAT); // [~] Sergey Shuhskanov
				dcassert(l_res);
			}
			
			dc.SetTextColor(oldclr);
		}
};

class FlatTabCtrl : public FlatTabCtrlImpl<FlatTabCtrl>
#ifdef RIP_USE_SKIN
	, public ITabCtrl
#endif
{
		typedef FlatTabCtrlImpl<FlatTabCtrl> BASE_CLASS;
	public:
		DECLARE_FRAME_WND_CLASS_EX(GetWndClassName(), IDR_FLATTAB, 0, COLOR_3DFACE);
		
		int getHeight() const
		{
			return BASE_CLASS::getHeight();
		}
		
		void SwitchTo(bool next = true)
		{
			BASE_CLASS::SwitchTo(next);
		}
		
		void Invalidate(BOOL bErase = TRUE)
		{
			BASE_CLASS::Invalidate(bErase);
		}
		
		void MoveWindow(LPCRECT lpRect, BOOL bRepaint = TRUE)
		{
			BASE_CLASS::MoveWindow(lpRect, bRepaint);
		}
		
		BOOL IsWindow() const
		{
			return BASE_CLASS::IsWindow();
		}
		
		HWND getNext()
		{
			return BASE_CLASS::getNext();
		}
		
		HWND getPrev()
		{
			return BASE_CLASS::getPrev();
		}
		
		void setActive(HWND hWnd)
		{
			BASE_CLASS::setActive(hWnd);
		}
		
		void addTab(HWND hWnd, COLORREF color = RGB(0, 0, 0), uint16_t icon = 0, uint16_t stateIcon = 0, bool mini = false)
		{
			BASE_CLASS::addTab(hWnd, color, icon, stateIcon, mini);
		}
		
		void removeTab(HWND aWnd)
		{
			BASE_CLASS::removeTab(aWnd);
		}
		
		void updateText(HWND aWnd, LPCTSTR text)
		{
			BASE_CLASS::updateText(aWnd, text);
		}
		
		void startSwitch()
		{
			BASE_CLASS::startSwitch();
		}
		
		void endSwitch()
		{
			BASE_CLASS::endSwitch();
		}
		
		void setCountMessages(HWND aWnd, int p_count_messages)
		{
			BASE_CLASS::setCountMessages(aWnd, p_count_messages);
		}
		void setDirty(HWND aWnd, int p_count_messages)
		{
			BASE_CLASS::setDirty(aWnd, p_count_messages);
		}
		
		void setColor(HWND aWnd, COLORREF color)
		{
			BASE_CLASS::setColor(aWnd, color);
		}
		
		void setIconState(HWND aWnd)
		{
			BASE_CLASS::setIconState(aWnd);
		}
		
		void unsetIconState(HWND aWnd)
		{
			BASE_CLASS::unsetIconState(aWnd);
		}
		
		void setCustomIcon(HWND p_aWnd, HICON p_custom)
		{
			BASE_CLASS::setCustomIcon(p_aWnd, p_custom);
		}
};

template < class T, int C = RGB(128, 128, 128), long I = 0, long I_state = 0, class TBase = CMDIWindow, class TWinTraits = CMDIChildWinTraits >
class ATL_NO_VTABLE MDITabChildWindowImpl : public CMDIChildWindowImpl<T, TBase, TWinTraits>
{
	public:
	
		MDITabChildWindowImpl() : m_closed(false), m_before_close(false) { }
#ifdef RIP_USE_SKIN
		ITabCtrl*
#else
		FlatTabCtrl*
#endif
		getTab()
		{
			return WinUtil::g_tabCtrl;
		}
		virtual void OnFinalMessage(HWND /*hWnd*/)
		{
			delete this;
		}
		
		typedef MDITabChildWindowImpl<T, C, I, I_state, TBase, TWinTraits> thisClass;
		typedef CMDIChildWindowImpl<T, TBase, TWinTraits> baseClass;
		BEGIN_MSG_MAP(thisClass)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_SYSCOMMAND, onSysCommand)
		MESSAGE_HANDLER(WM_FORWARDMSG, onForwardMsg)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_MDIACTIVATE, onMDIActivate)
		MESSAGE_HANDLER(WM_DESTROY, onDestroy)
		MESSAGE_HANDLER(WM_SETTEXT, onSetText)
		MESSAGE_HANDLER(WM_REALLY_CLOSE, onReallyClose)
		MESSAGE_HANDLER(WM_NOTIFYFORMAT, onNotifyFormat)
		MESSAGE_HANDLER(WM_GETMINMAXINFO, onGetMinMaxInfo)
		MESSAGE_HANDLER_HWND(WM_INITMENUPOPUP, OMenu::onInitMenuPopup)
		MESSAGE_HANDLER_HWND(WM_MEASUREITEM, OMenu::onMeasureItem)
		MESSAGE_HANDLER_HWND(WM_DRAWITEM, OMenu::onDrawItem)
		CHAIN_MSG_MAP(baseClass)
		END_MSG_MAP()
		
		HWND Create(HWND hWndParent, ATL::_U_RECT rect = NULL, LPCTSTR szWindowName = NULL,
		            DWORD dwStyle = 0, DWORD dwExStyle = 0,
		            UINT nMenuID = 0, LPVOID lpCreateParam = NULL)
		{
			ATOM atom = T::GetWndClassInfo().Register(&m_pfnSuperWindowProc);
			
			if (nMenuID != 0)
#if (_ATL_VER >= 0x0700)
				m_hMenu = ::LoadMenu(ATL::_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(nMenuID));
#else //!(_ATL_VER >= 0x0700)
				m_hMenu = ::LoadMenu(_Module.GetResourceInstance(), MAKEINTRESOURCE(nMenuID));
#endif //!(_ATL_VER >= 0x0700)
				
			if (m_hMenu == NULL)
				m_hMenu = WinUtil::mainMenu;
				
			dwStyle = T::GetWndStyle(dwStyle);
			dwExStyle = T::GetWndExStyle(dwExStyle);
			
			dwExStyle |= WS_EX_MDICHILD;    // force this one
			m_pfnSuperWindowProc = ::DefMDIChildProc;
			m_hWndMDIClient = hWndParent;
			ATLASSERT(::IsWindow(m_hWndMDIClient));
			
			if (rect.m_lpRect == NULL)
				rect.m_lpRect = &TBase::rcDefault;
				
			// If the currently m_active MDI child is maximized, we want to create this one maximized too
			ATL::CWindow wndParent = hWndParent;
			BOOL bMaximized = FALSE;
			
			if (MDIGetActive(&bMaximized) == NULL)
				bMaximized = BOOLSETTING(MDI_MAXIMIZED);
				
			if (bMaximized)
				wndParent.SetRedraw(FALSE);
				
			HWND hWnd = CFrameWindowImplBase<TBase, TWinTraits >::Create(hWndParent, rect.m_lpRect, szWindowName, dwStyle, dwExStyle, (UINT)0U, atom, lpCreateParam);
			
			if (bMaximized)
			{
				// Maximize and redraw everything
				if (hWnd != NULL)
					MDIMaximize(hWnd);
				wndParent.SetRedraw(TRUE);
				wndParent.RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
				::SetFocus(GetMDIFrame());   // focus will be set back to this window
			}
			else if (hWnd != NULL && WinUtil::isAppActive && ::IsWindowVisible(m_hWnd) && !::IsChild(hWnd, ::GetFocus()))
			{
				::SetFocus(hWnd);
			}
			
			return hWnd;
		}
		
		LRESULT onNotifyFormat(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
#ifdef _UNICODE
			return NFR_UNICODE;
#else
			return NFR_ANSI;
#endif
		}
		
		// All MDI windows must have this in wtl it seems to handle ctrl-tab and so on...
		LRESULT onForwardMsg(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			return baseClass::PreTranslateMessage((LPMSG)lParam); //-V109
		}
		
		LRESULT onSysCommand(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (wParam == SC_CLOSE)
			{
				m_before_close = true;
			}
			else if (wParam == SC_NEXTWINDOW)
			{
				HWND next = getTab()->getNext();
				if (next != NULL)
				{
					MDIActivate(next);
					return 0;
				}
			}
			else if (wParam == SC_PREVWINDOW)
			{
				HWND next = getTab()->getPrev();
				if (next != NULL)
				{
					MDIActivate(next);
					return 0;
				}
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onCreate(UINT /* uMsg */, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			dcassert(getTab());
			getTab()->addTab(m_hWnd, C, I, I_state);
			return 0;
		}
		virtual void onBeforeActiveTab(HWND aWnd) {}
		virtual void onAfterActiveTab(HWND aWnd) {}
		virtual void onInvalidateAfterActiveTab(HWND aWnd) {}
		
		LRESULT onMDIActivate(UINT /*uMsg*/, WPARAM /*wParam */, LPARAM lParam, BOOL& bHandled)
		{
			dcassert(getTab());
			if (m_hWnd == (HWND)lParam)
			{
				// не помогает от мерцания - искать другой способ.
				// CLockRedraw<> l_lock_draw(m_hWnd);
				onBeforeActiveTab(m_hWnd);
				getTab()->setActive(m_hWnd);
				onAfterActiveTab(m_hWnd);
			}
			onInvalidateAfterActiveTab(m_hWnd);
			bHandled = FALSE;
			return 1;
		}
		
		LRESULT onDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			if (getTab())
			{
				getTab()->removeTab(m_hWnd);
			}
			if (m_hMenu == WinUtil::mainMenu)
			{
				m_hMenu = NULL;
			}
			BOOL bMaximized = FALSE;
			if (::SendMessage(m_hWndMDIClient, WM_MDIGETACTIVE, 0, (LPARAM)&bMaximized) != NULL)
			{
				SET_SETTING(MDI_MAXIMIZED, (bMaximized > 0));
			}
			return 0;
		}
		
		LRESULT onReallyClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			MDIDestroy(m_hWnd);
			return 0;
		}
		
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled */)
		{
			m_before_close = true;
			PostMessage(WM_REALLY_CLOSE);
			return 0;
		}
		
		LRESULT onSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled)
		{
			bHandled = FALSE;
			dcassert(getTab());
			getTab()->updateText(m_hWnd, (LPCTSTR)lParam);
			return 0;
		}
		
		// For buggy Aero MDI, since it doesn't look like MS is gonna fix it... idea taken from MSDN
		LRESULT onGetMinMaxInfo(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
#ifdef FLYLINKDC_SUPPORT_WIN_XP
//			if (CompatibilityManager::isOsVistaPlus())
#endif
			{
				PMINMAXINFO pmmi = (PMINMAXINFO)lParam;
				if ((pmmi->ptMaxTrackSize.x < pmmi->ptMaxSize.x) || (pmmi->ptMaxTrackSize.y < pmmi->ptMaxSize.y))
				{
					pmmi->ptMaxTrackSize.x = max(pmmi->ptMaxTrackSize.x, pmmi->ptMaxSize.x);
					pmmi->ptMaxTrackSize.y = max(pmmi->ptMaxTrackSize.y, pmmi->ptMaxSize.y);
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
					/*
					                    if (!CompatibilityManager::isWin7Plus())
					                    {
					                        if(IsZoomed())
					                        {
					                            SetWindowLongPtr(GWL_STYLE, GetWindowLongPtr(GWL_STYLE) & ~WS_CAPTION);
					                        }
					                        else
					                        {
					                            SetWindowLongPtr(GWL_STYLE, GetWindowLongPtr(GWL_STYLE) | WS_CAPTION);
					                        }
					                        if(IsZoomed())
					                        {
					                            SetWindowLongPtr(GWL_STYLE, GetWindowLongPtr(GWL_STYLE) & ~WS_CAPTION);
					                        }
					                        else
					                        {
					                            SetWindowLongPtr(GWL_STYLE, GetWindowLongPtr(GWL_STYLE) | WS_CAPTION);
					                        }
					                    }
					*/
#endif
				}
				
#ifdef FLYLINKDC_SUPPORT_WIN_VISTA
				/*
				                // Vista sucks so we took over
				                // but Seven don't sucks )
				                if (!CompatibilityManager::isWin7Plus())
				                    return 0;
				*/
#endif
			}
			
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onKeyDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			if (wParam == VK_CONTROL && LOWORD(lParam) == 1)
			{
				getTab()->startSwitch();
			}
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT onKeyUp(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			if (wParam == VK_CONTROL)
			{
				getTab()->endSwitch();
			}
			bHandled = FALSE;
			return 0;
		}
		void setCountMessages(int p_count_messages)
		{
			dcassert(getTab());
			getTab()->setCountMessages(m_hWnd, p_count_messages);
		}
		void setDirty(int p_count_messages)
		{
			dcassert(getTab());
			getTab()->setDirty(m_hWnd, p_count_messages);
		}
		void setTabColor(COLORREF color)
		{
			dcassert(getTab());
			getTab()->setColor(m_hWnd, color);
		}
		void setIconState()
		{
			dcassert(getTab());
			getTab()->setIconState(m_hWnd);
		}
		void unsetIconState()
		{
			dcassert(getTab());
			getTab()->unsetIconState(m_hWnd);
		}
		
		// !SMT!-UI
		void setCustomIcon(HICON p_custom)
		{
			dcassert(getTab());
			getTab()->setCustomIcon(m_hWnd, p_custom);
		}
		
	protected:
		bool m_closed;
		bool m_before_close;
		bool isClosedOrShutdown() const
		{
			return m_closed || m_before_close || ClientManager::isShutdown();
		}
};

#endif // !defined(FLAT_TAB_CTRL_H)

/**
 * @file
 * $Id: flattabctrl.h,v 1.44 2006/11/05 15:21:01 bigmuscle Exp $
 */
