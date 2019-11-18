/*
 * Copyright (C) 2003-2004 "Opera", <opera at home dot se>
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

#include "stdafx.h"
#include "atlgdiraii.h"
#include "WinUtil.h"
#include "BarShader.h"
#include <boost/algorithm/string.hpp>

OMenu::~OMenu()
{
	if (::IsMenu(m_hMenu))
	{
		while (GetMenuItemCount() != 0)
		{
			RemoveMenu(0, MF_BYPOSITION);
		}
	}
	else
	{
		// How can we end up here??? it sure happens sometimes..
		for (auto i = m_items.cbegin(); i != m_items.cend(); ++i)
		{
			delete *i;
		}
	}
	//pUnMap();
}
/*
void OMenu::pMap() {
    if (m_hMenu != NULL) {
        menus[m_hMenu] = this;
    }
}
void OMenu::pUnMap() {
    if (m_hMenu != NULL) {
        Iter i = menus.find(m_hMenu);
        dcassert(i != menus.end());
        menus.erase(i);
    }
}
*/
BOOL OMenu::CreatePopupMenu()
{
	//pUnMap();
	BOOL b = CMenu::CreatePopupMenu();
	//pMap();
	return b;
}

void OMenu::InsertSeparator(UINT uItem, BOOL byPosition, const tstring& caption, bool accels /*= false*/)
{
	OMenuItem* mi = new OMenuItem();
	mi->m_text = caption;
	if (!accels)
	{
		boost::replace_all(mi->m_text, _T("&"), Util::emptyStringT);
	}
	
	// if (mi->text.length() > 25)
	// {
	//     mi->text = mi->text.substr(0, 25) + _T("...");
	// }
	mi->m_parent = this;
	m_items.push_back(mi);
	MENUITEMINFO mii = {0};
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_FTYPE | MIIM_DATA;
	mii.fType = MFT_OWNERDRAW | MFT_SEPARATOR;
	mii.dwItemData = (ULONG_PTR)mi;
	InsertMenuItem(uItem, byPosition, &mii);
}

void OMenu::CheckOwnerDrawn(UINT uItem, BOOL byPosition)
{
	MENUITEMINFO mii = {0};
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_TYPE | MIIM_DATA | MIIM_SUBMENU;
	GetMenuItemInfo(uItem, byPosition, &mii);
	
	if (mii.dwItemData != NULL)
	{
		OMenuItem* mi = (OMenuItem*)mii.dwItemData;
		if (mii.fType &= MFT_OWNERDRAW) // ?
		{
			auto i = find(m_items.begin(), m_items.end(), mi);
			dcassert(i != m_items.end());
			m_items.erase(i);
		}
		delete mi;
	}
}

BOOL OMenu::InsertMenuItem(UINT uItem, BOOL bByPosition, LPMENUITEMINFO lpmii)
{
	if (lpmii->fMask & MIIM_TYPE && !(lpmii->fType & MFT_OWNERDRAW))
	{
		if (lpmii->fMask & MIIM_DATA)
		{
			// If we add this MENUITEMINFO to several items we might destroy the original data, so we copy it to be sure
			MENUITEMINFO mii; // [!] PVS V506   Pointer to local variable 'mii' is stored outside the scope of this variable. Such a pointer will become invalid.
			CopyMemory(&mii, lpmii, sizeof(MENUITEMINFO));
			lpmii = &mii;
			OMenuItem* omi = new OMenuItem();
			omi->m_is_ownerdrawn = false;
			omi->m_data = (void*)lpmii->dwItemData;
			omi->m_parent = this;
			lpmii->dwItemData = (ULONG_PTR)omi;
			// Do this later on? Then we're out of scope -> mii dead -> lpmii dead pointer
			return CMenu::InsertMenuItem(uItem, bByPosition, lpmii);
		}
	}
	return CMenu::InsertMenuItem(uItem, bByPosition, lpmii);
}

LRESULT OMenu::onInitMenuPopup(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	HMENU mnu = (HMENU)wParam;
	bHandled = TRUE; // Always true, since we do the following manually:
	/*LRESULT ret = */
	::DefWindowProc(hWnd, uMsg, wParam, lParam);
//  if (fixedMenus.find(mnu) == fixedMenus.end()) {
	for (int i = 0; i < ::GetMenuItemCount(mnu); ++i)
	{
		MENUITEMINFO mii = {0};
		mii.cbSize = sizeof(MENUITEMINFO);
		mii.fMask = MIIM_TYPE | MIIM_DATA;
		::GetMenuItemInfo(mnu, i, TRUE, &mii);
		if ((mii.fType &= MFT_OWNERDRAW) != MFT_OWNERDRAW && mii.dwItemData != NULL)
		{
			OMenuItem* omi = (OMenuItem*)mii.dwItemData;
			if (omi->m_is_ownerdrawn)
			{
				mii.fType |= MFT_OWNERDRAW;
				::SetMenuItemInfo(mnu, i, TRUE, &mii);
			}
		}
	}
//      fixedMenus[mnu] = true;
//  }
	return S_OK;
}

LRESULT OMenu::onMeasureItem(HWND /*hWnd*/, UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	if (wParam == NULL)
	{
		MEASUREITEMSTRUCT* mis = (MEASUREITEMSTRUCT*)lParam;
		if (mis->CtlType == ODT_MENU)
		{
			const OMenuItem* mi = (OMenuItem*)mis->itemData;
			if (mi)
			{
				bHandled = TRUE;
				SIZE size;
				CalcTextSize(mi->m_text, Fonts::g_boldFont, &size);
				mis->itemWidth = size.cx + 4;
				mis->itemHeight = size.cy + 8;
				return TRUE;
			}
		}
	}
	return S_OK;
}

LRESULT OMenu::onDrawItem(HWND /*hWnd*/, UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	bHandled = FALSE;
	if (wParam == NULL)
	{
		DRAWITEMSTRUCT dis = *(DRAWITEMSTRUCT*)lParam;
		if (dis.CtlType == ODT_MENU)
		{
			OMenuItem* mi = (OMenuItem*)dis.itemData;
			if (mi)
			{
				bHandled = TRUE;
				CRect rc(dis.rcItem);
				//rc.top += 2;
				rc.bottom -= 2;
				
				CDC dc;
				dc.Attach(dis.hDC);
				
				if (BOOLSETTING(MENUBAR_TWO_COLORS))
					OperaColors::FloodFill(dc, rc.left, rc.top, rc.right, rc.bottom, SETTING(MENUBAR_LEFT_COLOR), SETTING(MENUBAR_RIGHT_COLOR), BOOLSETTING(MENUBAR_BUMPED));
				else
					dc.FillSolidRect(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SETTING(MENUBAR_LEFT_COLOR));
					
				dc.SetBkMode(TRANSPARENT);
				dc.SetTextColor(OperaColors::TextFromBackground(SETTING(MENUBAR_LEFT_COLOR)));
				{
					CSelectFont l_font(dc, Fonts::g_boldFont); //-V808
					dc.DrawText(mi->m_text.c_str(), mi->m_text.length(), rc, DT_CENTER | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
				}
				
				{
				
					/*                  HICON hIcon = g_flagImage.getIconList().GetIcon(0);
					                    SIZE l_szBitmap;
					                    g_flagImage.getIconList().GetIconSize(l_szBitmap);
					                    CBrush brush;
					                    const bool bOver = false;
					                    brush.CreateSolidBrush(bOver ? HLS_TRANSFORM(::GetSysColor(COLOR_HIGHLIGHT), +50, -66) : HLS_TRANSFORM(::GetSysColor(COLOR_3DFACE), -27, 0));
					                    dc.DrawState(CPoint(rc.left + (bOver ? 4 : 3), rc.top + (bOver ? 5 : 4)),
					                        CSize(l_szBitmap.cx, l_szBitmap.cx), hIcon, DSS_MONO, brush);
					                    DestroyIcon(hIcon);
					                    */
					// TODO - нарисовать иконки
					//bool bSelected = false;
					//bool bChecked  = false;
					//::ImageList_Draw(g_flagImage.getIconList(), 1, dc.m_hDC,
					//                  rc.left + ((bSelected && !bChecked) ? 2 : 3), rc.top + ((bSelected && !bChecked) ? 3 : 4), ILD_TRANSPARENT);
				}
				dc.Detach();
				return TRUE;
			}
		}
	}
	return S_OK;
}
#ifdef IRAINMAN_INCLUDE_SMILE
void CEmotionMenu::CreateEmotionMenu(const POINT& p_pt, const HWND& p_hWnd, int p_IDC_EMOMENU)
{
	if ((*this) != NULL)
	{
		DestroyMenu();
	}
	CreatePopupMenu();
	m_menuItems = 0;
	AppendMenu(MF_STRING, p_IDC_EMOMENU, CTSTRING(DISABLED));
	if (SETTING(EMOTICONS_FILE) == "Disabled")
	{
		CheckMenuItem(p_IDC_EMOMENU, MF_BYCOMMAND | MF_CHECKED);
	}
	// nacteme seznam emoticon packu (vsechny *.xml v adresari EmoPacks)
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFileEx(Text::toT(Util::getEmoPacksPath() + "*.xml").c_str(),
	                               CompatibilityManager::g_find_file_level,
	                               &data,
	                               FindExSearchNameMatch,
	                               NULL,
	                               0);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			tstring name = data.cFileName;
			tstring::size_type i = name.rfind('.');
			name = name.substr(0, i);
			m_menuItems++;
			AppendMenu(MF_STRING, p_IDC_EMOMENU + m_menuItems, name.c_str());
			if (name == Text::toT(SETTING(EMOTICONS_FILE)))
			{
				CheckMenuItem(p_IDC_EMOMENU + m_menuItems, MF_BYCOMMAND | MF_CHECKED);
			}
		}
		while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	InsertSeparatorFirst(TSTRING(AUTOUPDATE_EMOPACKS));
	if (m_menuItems > 0)
	{
		TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, p_pt.x, p_pt.y, p_hWnd);
	}
	RemoveFirstItem();
}
#endif // IRAINMAN_INCLUDE_SMILE