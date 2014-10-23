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
extern bool g_TabsCloseButtonAlt;
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
		
		explicit FlatTabCtrlImpl() : closing(nullptr), rows(1), m_height(0), active(nullptr), moving(nullptr), inTab(false)
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
				tabs.push_back(i);
				switch (WinUtil::GetTabsPosition())
				{
					case SettingsManager::TABS_TOP:
					case SettingsManager::TABS_BOTTOM:
						viewOrder.push_back(hWnd);
						break;
					default:
						viewOrder.push_front(hWnd);
						break;
				}
				
				nextTab = --viewOrder.end();
			}
			else
			{
				tabs.insert(tabs.begin(), i);
				viewOrder.insert(viewOrder.begin(), hWnd);
				nextTab = ++viewOrder.begin();
			}
			active = i;
#ifdef IRAINMAN_FAST_FLAT_TAB
			// do nothing, all updates in setActiv()
#else
			calcRows(false);
			Invalidate();
#endif
		}
		
		void removeTab(HWND aWnd)
		{
			if (!tabs.empty()) // бывает что табов нет http://www.flickr.com/photos/96019675@N02/9732602134/
			{
				TabInfo::List::const_iterator i;
				for (i = tabs.begin(); i != tabs.end(); ++i)
				{
					if ((*i)->hWnd == aWnd)
						break;
				}
				if (i != tabs.end()) //[+]PPA fix
				{
					TabInfo* ti = *i;
					tabs.erase(i);
					
					if (active == ti)
						active = nullptr;
					if (moving == ti)
						moving = nullptr;
						
					viewOrder.erase(std::find(begin(viewOrder), end(viewOrder), aWnd));
					nextTab = viewOrder.end();
					if (!viewOrder.empty())
						--nextTab;
						
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
			if (!viewOrder.empty())
				nextTab = --viewOrder.end();
			inTab = true;
		}
		
		void endSwitch()
		{
			inTab = false;
			if (active)
				setTop(active->hWnd);
		}
		
		HWND getNext()
		{
			if (viewOrder.empty())
				return nullptr;
				
			if (nextTab == viewOrder.begin())
			{
				nextTab = --viewOrder.end();
			}
			else
			{
				--nextTab;
			}
			return *nextTab;
		}
		HWND getPrev()
		{
			if (viewOrder.empty())
				return nullptr;
				
			++nextTab;
			if (nextTab == viewOrder.end())
			{
				nextTab = viewOrder.begin();
			}
			return *nextTab;
		}
		
		bool isActive(HWND aWnd) // [+] IRainman opt.
		{
			return active && // fix https://www.crash-server.com/DumpGroup.aspx?ClientID=ppa&Login=Guest&DumpGroupID=86322
			       active->hWnd == aWnd;
		}
		
		void setActive(HWND aWnd)
		{
#ifdef IRAINMAN_INCLUDE_GDI_OLE
			CGDIImageOle::g_ActiveMDIWindow = aWnd;
#endif
			if (!inTab)
				setTop(aWnd);
			if (TabInfo* ti = getTabInfo(aWnd))
			{
				active = ti;
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
			viewOrder.erase(std::find(begin(viewOrder), end(viewOrder), aWnd));
			viewOrder.push_back(aWnd);
			nextTab = --viewOrder.end();
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
				if (active != ti)
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
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_CHEVRON, onChevron)
		COMMAND_RANGE_HANDLER(IDC_SELECT_WINDOW, IDC_SELECT_WINDOW + tabs.size(), onSelectWindow)
		END_MSG_MAP()
		
		LRESULT onLButtonDown(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			const int row = getRows() - (pt.y / getTabHeight() + 1);
			
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
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
							moving = t;
					}
					break;
				}
			}
			return 0;
		}
		
		LRESULT onLButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			if (moving)
			{
				const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				const int row = getRows() - (pt.y / getTabHeight() + 1);
				
				bool moveLast = true;
				
				for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
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
							if (t == moving)
								::SendMessage(hWnd, FTM_SELECTED, (WPARAM)t->hWnd, 0);
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
					moveTabs(tabs.back(), true);
				moving = NULL;
			}
			return 0;
		}
		
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
			
			bClose.ShowWindow(FALSE);
			
			ScreenToClient(&pt);
			const int row = getRows() - (pt.y / getTabHeight() + 1);
			
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
			{
				TabInfo* t = *i;
				if (row == t->m_row &&
				        pt.x >= t->m_xpos &&
				        pt.x < t->m_xpos + t->getWidth()) // TODO copy-paste
				{
					// Bingo, this was clicked, check if the owner wants to handle it...
					if (!::SendMessage(t->hWnd, FTM_CONTEXTMENU, 0, lParam))
					{
						closing = t->hWnd;
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
			if (!cur_close)
			{
				bClose.EnableWindow(FALSE);
				bClose.ShowWindow(FALSE);
			}
			return 1;
		}
		
		LRESULT onMouseMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
			const int row = getRows() - (pt.y / getTabHeight() + 1);
#ifdef SCALOLAZ_CLOSEBUTTON
			const int yPos = (getRows() - row - 1) * getTabHeight();
#endif
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
			{
				TabInfo* t = *i;
				if (row == t->m_row &&
				        pt.x >= t->m_xpos &&
				        pt.x < t->m_xpos + t->getWidth())
				{
#ifdef SCALOLAZ_CLOSEBUTTON
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
							closing = t->hWnd;  // [+] NightOrion
							switch (WinUtil::GetTabsPosition())
							{
								case SettingsManager::TABS_LEFT:
								case SettingsManager::TABS_RIGHT:
									rcs.left = t->m_xpos + 149;
									break;
								default:
									rcs.left = t->m_xpos + t->getWidth() + (g_TabsCloseButtonAlt ? 1 : 0);
									break;
							}
							rcs.left -= (g_TabsCloseButtonAlt ? 17 : 18);
							
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
							
							bClose.MoveWindow(rcs.left, rcs.top, 16, 16);
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
								bClose.EnableWindow(TRUE);
								m_tab_tip_close.DelTool(m_hWnd);
								m_tab_tip_close.AddTool(bClose, ResourceManager::CLOSE);
								if (!BOOLSETTING(POPUPS_DISABLED) && BOOLSETTING(POPUPS_TABS_ENABLED)) // TODO -> updateTabs
									m_tab_tip_close.Activate(TRUE);
									
								cur_close = true;
							}
							else
							{
								bClose.EnableWindow(FALSE);
								m_tab_tip_close.Activate(FALSE);
								cur_close = false;
							}
							bClose.ShowWindow(TRUE);
						}
					}
#endif // SCALOLAZ_CLOSEBUTTON
					const size_t len = static_cast<size_t>(::GetWindowTextLength(t->hWnd) + 1);
					tstring buf;
					buf.resize(len);
					::GetWindowText(t->hWnd, &buf[0], len); //-V107
					if (buf != current_tip)
					{
						m_tab_tip.DelTool(m_hWnd);
						m_tab_tip.AddTool(m_hWnd, &buf[0]);
						if (!BOOLSETTING(POPUPS_DISABLED) && BOOLSETTING(POPUPS_TABS_ENABLED)) // TODO -> updateTabs
						{
							m_tab_tip.Activate(TRUE);
						}
						current_tip = buf;
					}
					return 1;
				}
			}
			m_tab_tip.Activate(FALSE);
			m_tab_tip_close.Activate(FALSE);
			current_tip.clear();
			bClose.EnableWindow(FALSE);
			bClose.ShowWindow(FALSE);
			cur_close = false;
			return 1;
		}
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			bClose.ShowWindow(FALSE);
			dcassert(::IsWindow(closing));
			if (::IsWindow(closing))
				::SendMessage(closing, WM_CLOSE, 0, 0);
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
			return rows;
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
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
			{
				TabInfo* ti = *i;
				if (r != 0 && w + ti->getWidth() > rc.Width())
				{
					if (r == l_MaxTabRows)
					{
						notify |= rows != r;
						rows = r;
						r = 0;
						chevron.EnableWindow(TRUE);
						chevron.ShowWindow(TRUE);
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
				chevron.EnableWindow(FALSE);
				chevron.ShowWindow(FALSE);
				notify |= (rows != r);
				rows = r;
			}
			
			bClose.ShowWindow(FALSE);
			
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
			chevron.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
			               BS_PUSHBUTTON , 0, IDC_CHEVRON);
			chevron.SetWindowText(_T("\u00bb"));
			
			// [+] SCALOlaz : Create a Close Button
			CImageList CloseImages;
			ResourceLoader::LoadImageList(IDR_CLOSE_PNG, CloseImages, 16, 16);
			bClose.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE | BS_FLAT, 0, IDC_CLOSE_WINDOW);
			bClose.SetImageList(CloseImages);
			
			g_mnu.CreatePopupMenu();
			
			m_tab_tip.Create(m_hWnd, rcDefault, NULL, TTS_ALWAYSTIP | TTS_NOPREFIX);
			m_tab_tip_close.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
			m_tab_tip_close.SetDelayTime(TTDT_AUTOPOP, 5000);
			ATLASSERT(m_tab_tip_close.IsWindow());
			
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
					m_height = 22;
				}
			}
			int l_res = ::ReleaseDC(m_hWnd, dc);
			dcassert(l_res);
			return 0;
		}
		
		LRESULT onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			calcRows();
			
			bClose.ShowWindow(FALSE);
			
			const SIZE sz = { LOWORD(lParam), HIWORD(lParam) };
			switch (WinUtil::GetTabsPosition())
			{
				case SettingsManager::TABS_TOP:
				case SettingsManager::TABS_BOTTOM:
					chevron.MoveWindow(sz.cx - 14, 1, 14, getHeight() - 2);
					break;
				default:
					chevron.MoveWindow(0, sz.cy - 15, 150, 15);
					break;
			}
			return 0;
		}
		
		LRESULT onPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
#ifdef IRAINMAN_FAST_FLAT_TAB
			m_allowInvalidate = false;
#endif
			bClose.ShowWindow(FALSE);
			RECT rc;
			
			if (GetUpdateRect(&rc, FALSE))
			{
				CPaintDC dc(m_hWnd);
				CSelectFont l_font(dc, Fonts::g_systemFont); //-V808
				//ATLTRACE("%d, %d\n", rc.left, rc.right);
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
				std::unique_ptr<Gdiplus::Graphics> graphics(new Gdiplus::Graphics(dc));
				//Режим сглаживания линий (не менять, другие не работают)
				graphics->SetSmoothingMode(Gdiplus::SmoothingModeNone /*SmoothingModeHighQuality*/);
#endif
				//dcdebug("FlatTabCtrl::active = %d\n", active);
				for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
				{
					TabInfo* t = *i;
					if (t && t->m_row != -1 && t->m_xpos < rc.right && t->m_xpos + t->getWidth() >= rc.left)
					{
						//dcdebug("FlatTabCtrl::drawTab::t = %d\n", t);
						drawTab(dc,
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
						        graphics,
#endif
						        t, t->m_xpos, t->m_row, t == active);
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
				                    dcassert(active);
				                    drawTab(dc, active, active->xpos, active->row, true);
				                    dc.SelectPen(active->pen);
				                    int y = (rows - active->row - 1) * getTabHeight();
				                    dc.MoveTo(active->xpos, y);
				                    dc.LineTo(active->xpos + active->getWidth(), y);
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
			
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
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
			chevron.GetClientRect(&rc);
			pt.x = rc.right - rc.left;
			pt.y = 0;
			chevron.ClientToScreen(&pt);
			
			g_mnu.TrackPopupMenu(TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			return 0;
		}
		
		LRESULT onSelectWindow(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			bClose.ShowWindow(FALSE);
			
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
			bClose.ShowWindow(FALSE);
			
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
			{
				if ((*i)->hWnd == active->hWnd)
				{
					if (next)
					{
						++i;
						if (i == tabs.end())
							i = tabs.begin();
					}
					else
					{
						if (i == tabs.begin())
							i = tabs.end();
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
			bClose.ShowWindow(FALSE);
			const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			const int row = getRows() - (pt.y / getTabHeight() + 1);
			
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
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
			g_color_face = GetSysColor(COLOR_BTNFACE);
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
			
			if (g_TabsCloseButtonEnabled)
			{
				g_TabsCloseButtonAlt = BOOLSETTING(TABS_CLOSEBUTTONS_ALT);
				
				if (g_TabsCloseButtonAlt)   // Если дизайнерский вид
				{
					bClose.SetImages(4, 5, 6, 7);
				}
				else
				{
					bClose.SetImages(0, 1, 2, 3);
				}
			}
			else
				bClose.ShowWindow(FALSE);
				
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
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
			if (!moving)
				return;
				
			TabInfo::List::const_iterator i;
			//remove the tab we're moving
			for (auto j = tabs.begin(); j != tabs.end(); ++j)
			{
				if ((*j) == moving)
				{
					tabs.erase(j);
					break;
				}
			}
			
			//find the tab we're going to insert before or after
			for (i = tabs.begin(); i != tabs.end(); ++i)
			{
				if ((*i) == aTab)
				{
					if (after)
						++i;
					break;
				}
			}
			
			tabs.insert(i, moving);
			moving = nullptr;
#ifdef IRAINMAN_FAST_FLAT_TAB
			calcRows();
			SmartInvalidate();
#else
			calcRows(false);
			Invalidate();
#endif
		}
		
	private:
		HWND closing;
		CButton chevron;
		WTL::CBitmapButton bClose;
		CFlyToolTipCtrl m_tab_tip;
		CFlyToolTipCtrl m_tab_tip_close;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
//		CPen black;
//		CPen white;
#endif

		int rows;
		int m_height;
		int m_height_font;
		
		tstring current_tip;
		bool cur_close;
		
		TabInfo* active;
		TabInfo* moving;
		typename TabInfo::List tabs;
		
		typedef deque<HWND> WindowList; // [!] IRainman opt: change list to deque.
		typedef WindowList::const_iterator WindowIter;
		
		WindowList viewOrder;
		WindowIter nextTab;
		
		bool inTab;
#ifdef IRAINMAN_FAST_FLAT_TAB
		bool m_needsInvalidate;
		bool m_allowInvalidate;
#endif
	public:
		TabInfo* getTabInfo(HWND aWnd) const
		{
			for (auto i = tabs.cbegin(); i != tabs.cend(); ++i)
			{
				if ((*i)->hWnd == aWnd)
					return *i;
			}
			dcassert(0);
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
			
			// const int tabAnim = aActive ? 0 : 2;
			const int tabAnim = 0;
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
				                                                  Gdiplus::LinearGradientModeVertical)
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
					Gdiplus::Color(60, GetRValue(l_color_tab), GetGValue(l_color_tab), GetBValue(l_color_tab)),
					Gdiplus::Color(135, GetRValue(l_color_tab), GetGValue(l_color_tab), GetBValue(l_color_tab)),
					l_isBottom ? g_color_light_gdi : g_color_face_gdi
				};
				static const Gdiplus::REAL g_positions[] =
				{
					0.0f,
					0.40f,
					0.60f,
					1.0f
				};
				static const Gdiplus::REAL g_positions2[] =
				{
					0.0f,
					0.15f,
					0.98f,
					1.0f
				};
				BOOST_STATIC_ASSERT(_countof(l_colors) == _countof(g_positions) && _countof(l_colors) == _countof(g_positions2));
				if (g_TabsCloseButtonAlt)
				{
					l_tabBrush->SetInterpolationColors(l_colors, g_positions2, _countof(l_colors));
				}
				else
				{
					l_tabBrush->SetInterpolationColors(l_colors, g_positions, _countof(l_colors));
				}
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
			switch (WinUtil::GetTabsPosition())
			{
				case SettingsManager::TABS_TOP:
				{
					//Расчёт положения иконки и текста в зависимости от высоты вкладки
					height_plus = (m_height + 3 + tabAnim - m_height_font) / 2;
					height_plus_ico = (m_height + 4 + tabAnim - 16) / 2; //-V112
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
					if (l_tabsPath)
					{
						l_tabsPath->AddLine(pos, ypos + m_height, pos, ypos + 6 + tabAnim);
						l_tabsPath->AddLine(pos + 4, ypos + 2 + tabAnim, pos + magic_width - 5, ypos + 2 + tabAnim); //-V112
						l_tabsPath->AddLine(pos + magic_width, ypos + 6 + tabAnim, pos + magic_width, ypos + m_height);
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
						l_tabsPath->AddLine(pos, ypos, pos, ypos + m_height - 6 - tabAnim);
						l_tabsPath->AddLine(pos + 4, ypos + m_height - 2 - tabAnim, pos + magic_width - 5, ypos + m_height - 2 - tabAnim); //-V112
						l_tabsPath->AddLine(pos + magic_width, ypos + m_height - 6 - tabAnim, pos + magic_width, ypos);
					}
#endif // IRAINMAN_USE_GDI_PLUS_TAB
				}
				break;
				//case SettingsManager::TABS_LEFT:
				//case SettingsManager::TABS_RIGHT:
				default:
				{
					//Расчёт положения иконки и текста в зависимости от высоты вкладки
					height_plus = (m_height + 1 - m_height_font) / 2;
					height_plus_ico = (m_height + 2 - 16) / 2;
					magic_width -= 1;
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
					if (l_tabsPath)
					{
						l_tabsPath->AddLine(pos + 1, ypos + 5, pos + 1, ypos + m_height - 5);
						l_tabsPath->AddLine(pos + 5, ypos + m_height - 1, pos + magic_width - 9, ypos + m_height - 1);
						l_tabsPath->AddLine(pos + magic_width - 4, ypos + m_height - 5, pos + magic_width - 4, ypos + 5); //-V112
						l_tabsPath->AddLine(pos + magic_width - 9, ypos + 1, pos + 5, ypos + 1);
					}
#endif // IRAINMAN_USE_GDI_PLUS_TAB
				}
				break;
			}
#ifdef IRAINMAN_USE_GDI_PLUS_TAB
			//Заливка вкладки
			if (l_tabBrush && l_tabsPath)
			{
				graphics->FillPath(l_tabBrush.get(), l_tabsPath.get());
			}
			
			//Отрисовка контура поверх заливки
			//Создание "ручек" для контура - падаем под XP
			//Gdiplus::Pen pen(aActive ? Gdiplus::Color(0, 0, 0) : Gdiplus::Color(90, 60, 90) , 1);
			//pen.SetDashStyle(aActive ? Gdiplus::DashStyleDot : Gdiplus::DashStyleSolid);
			//graphics->DrawPath(&pen, &tabsPath);
			
//////////////////
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
				if (tab->m_row != (rows - 1))
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
			if (tab->m_row != (rows - 1))
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
			if (tab->m_count_messages)
			{
				dc.SetTextColor(RGB(255, 0, 0));
				CSelectFont l_half_font(dc, Fonts::g_halfFont);
				const auto l_cnt = Text::toT("+" + Util::toString(tab->m_count_messages));
				dc.TextOut(pos + tab->m_size.cx - l_cnt.size() * 3 , ypos + 4 , l_cnt.c_str(), l_cnt.length());
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
				
			// If the currently active MDI child is maximized, we want to create this one maximized too
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
			dcassert(getTab());
			getTab()->removeTab(m_hWnd);
			if (m_hMenu == WinUtil::mainMenu)
				m_hMenu = NULL;
				
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
			if (CompatibilityManager::isOsVistaPlus())
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
