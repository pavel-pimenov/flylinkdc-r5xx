#if !defined(AFX_PROPERTYVIEW_H__20000930_54A8_5070_44B5_0080AD509054__INCLUDED_)
#define AFX_PROPERTYVIEW_H__20000930_54A8_5070_44B5_0080AD509054__INCLUDED_

#pragma once

#error "Заначка - пока не используется"

/////////////////////////////////////////////////////////////////////////////
// Property View control
//
// For this control to work, you must add the
//   REFLECT_NOTIFICATIONS()
// macro to the parent's message map.
//
// Written by Bjarke Viksoe (bjarke@viksoe.dk)
// Copyright (c) 2000 Bjarke Viksoe.
//
// This code may be used in compiled form in any way you desire. This
// file may be redistributed by any means PROVIDING it is
// not sold for profit without the authors written consent, and
// providing that this notice and the authors name is included.
//
// This file is provided "as is" with no expressed or implied warranty.
// The author accepts no liability if it causes any damage to you or your
// computer whatsoever. It's free, so don't hassle me about it.
//
// Beware of bugs.
//

#ifndef __cplusplus
#error ATL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLAPP_H__
#error PropertyView.h requires atlapp.h to be included first
#endif

#ifndef __ATLCTRLS_H__
#error PropertyView.h requires atlctrls.h to be included first
#endif


//////////////////////////////////////////////////////////
// Property class

typedef enum { PROP_ITEM, PROP_GROUP } PROPERTY_ITEMTYPE;

class CProperty
{
	public:
		CProperty(PROPERTY_ITEMTYPE Type,
		          LPCTSTR pstrTitle,
		          VARIANT* pvValue,
		          BOOL bReadOnly = FALSE)
		{
			ATLASSERT(pvValue);
			m_Type = Type;
			m_vValue = *pvValue;
			m_sTitle = pstrTitle;
			m_bReadOnly = m_Type == PROP_GROUP ? TRUE : bReadOnly;
		}
		
		virtual void DrawValue(CDCHandle& dc, RECT& rc, int iMiddlePos)
		{
			USES_CONVERSION;
			const int TEXT_INDENT = 6;
			LPCTSTR pstrValue = NULL;
			switch (m_vValue.vt)
			{
				case VT_I4:
				case VT_UI4:
				case VT_I2:
				case VT_UI2:
				case VT_I1:
				case VT_UI1:
				{
					CComVariant v(m_vValue);
					v.ChangeType(VT_BSTR);
					pstrValue = OLE2CT(v.bstrVal);
				}
				break;
				case VT_BSTR:
					pstrValue = OLE2CT(m_vValue.bstrVal);
					break;
			}
			if (pstrValue != NULL)
			{
				dc.TextOut(
				    rc.left + TEXT_INDENT,
				    rc.top + iMiddlePos,
				    pstrValue,
				    _tcslen(pstrValue));
			}
		}
		
		PROPERTY_ITEMTYPE m_Type;
		CString m_sTitle;
		CComVariant m_vValue;
		BOOL m_bReadOnly;
};


//////////////////////////////////////////////////////////
// PropertyView control

template< class T, class TBase = CListBox, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CPropertyViewImpl : public CWindowImpl< T, TBase, TWinTraits >
{
	public:
		CFont m_hFont;
		CFont m_hGroupFont;
		COLORREF m_clrText;
		COLORREF m_clrBack;
		COLORREF m_clrHighlite;
		COLORREF m_clrHighliteText;
		COLORREF m_clrBorder;
		//
		TEXTMETRIC m_tm;
		int m_yposMiddle;
		int m_xposMiddle;
		
		CPropertyViewImpl() :
			m_hFont(NULL),
			m_hGroupFont(NULL),
			m_clrText(::GetSysColor(COLOR_WINDOWTEXT)),
			m_clrBack(::GetSysColor(COLOR_WINDOW)),
			m_clrHighlite(::GetSysColor(COLOR_HIGHLIGHT)),
			m_clrHighliteText(::GetSysColor(COLOR_HIGHLIGHTTEXT)),
			m_clrBorder(::GetSysColor(COLOR_3DLIGHT))
		{
			::ZeroMemory(&m_tm, sizeof(m_tm));
		}
		
		// Message map and handlers
		
		BEGIN_MSG_MAP(CPropertyViewImpl)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
		MESSAGE_HANDLER(OCM_MEASUREITEM, OnMeasureItem)
		MESSAGE_HANDLER(OCM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(OCM_DELETEITEM, OnDeleteItem)
		DEFAULT_REFLECTION_HANDLER()
		END_MSG_MAP()
		
		// Operations
		
		BOOL SubclassWindow(HWND hWnd)
		{
			ATLASSERT(m_hWnd == NULL);
			ATLASSERT(::IsWindow(hWnd));
			BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
			if (bRet) _Init();
			return bRet;
		}
		
		BOOL AddItem(CProperty* prop)
		{
			ATLASSERT(prop);
			int nItem = AddString(_T(""));
			if (nItem < 0) return FALSE;
			SetItemData(nItem, (LPARAM)prop);
			return TRUE;
		}
		
		// Implementation
		
		void _Init()
		{
			ATLASSERT(::IsWindow(m_hWnd));
			
			ATLASSERT(GetStyle() & LBS_OWNERDRAWVARIABLE); // need this
			ModifyStyle(LBS_SORT | LBS_HASSTRINGS, 0);
			
#ifdef _DEBUG
			// Check class
			TCHAR lpszBuffer[8];
			if (::GetClassName(m_hWnd, lpszBuffer, 8))
			{
				if (::lstrcmpi(lpszBuffer, _T("listbox")) != 0)
				{
					ATLASSERT(!"Invalid class for PropertyView");
				}
			}
#endif
			
			// Set font
			CWindow wnd = GetParent();
			CFontHandle font = wnd.GetFont();
			if (!font.IsNull())
			{
				LOGFONT lf;
				::GetObject(font, sizeof(LOGFONT), &lf);
				m_hFont.CreateFontIndirect(&lf);
				lf.lfWeight += FW_BOLD;
				m_hGroupFont.CreateFontIndirect(&lf);
			}
		}
		
		LRESULT OnCreate(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
			_Init();
			return lRes;
		}
		
		LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ResetContent(); // Make sure to delete item-data memory
			return 0;
		}
		
		LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
		{
			::ZeroMemory(&m_tm, sizeof(m_tm)); // reset settings
			bHandled = FALSE;
			return 0;
		}
		
		LRESULT OnSettingChange(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			::ZeroMemory(&m_tm, sizeof(m_tm)); // reset settings
			return 0;
		}
		
		LRESULT OnMeasureItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
		{
			LRESULT lRes = DefWindowProc(uMsg, wParam, lParam);
			LPMEASUREITEMSTRUCT lpmis = (LPMEASUREITEMSTRUCT) lParam;
			lpmis->itemHeight += 3;
			return lRes;
		}
		
		LRESULT OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT) lParam;
			// If there are no list box items, skip this message.
			if (lpdis->itemID == -1) return 0;
			
			CDCHandle dc(lpdis->hDC);
			CProperty* pProp = (CProperty*) lpdis->itemData;
			ATLASSERT(pProp);
			
			COLORREF clrOldTextColor = dc.GetTextColor();
			COLORREF clrOldBkColor = dc.GetBkColor();
			
			// If we didn't calculate the text position, then do it
			if (m_tm.tmHeight == 0)
			{
				dc.GetTextMetrics(&m_tm);
				m_yposMiddle = (lpdis->rcItem.bottom + lpdis->rcItem.top - m_tm.tmHeight) / 2;
				m_xposMiddle = ((lpdis->rcItem.right - lpdis->rcItem.left) / 2) + lpdis->rcItem.left;
			}
			
			// Draw item...
			switch (lpdis->itemAction)
			{
				case ODA_SELECT:
				case ODA_DRAWENTIRE:
				
					COLORREF clrFront;
					COLORREF clrBack;
					if ((lpdis->itemAction == ODA_SELECT) &&
					        (lpdis->itemState & ODS_SELECTED))
					{
						clrBack = m_clrHighlite;
						clrFront = m_clrHighliteText;
					}
					else
					{
						clrBack = m_clrBack;
						clrFront = m_clrText;
					}
					
					if (pProp->m_Type == PROP_GROUP)
					{
						const int XPOS_INDENT = 3;
						
						RECT rcMiddle = lpdis->rcItem;
						dc.FillSolidRect(&rcMiddle, clrBack);
						
						dc.SetBkColor(clrBack);
						dc.SetTextColor(clrFront);
						
						HFONT hOldFont = dc.SelectFont(m_hGroupFont);
						dc.TextOut(
						    lpdis->rcItem.left + XPOS_INDENT,
						    lpdis->rcItem.top + m_yposMiddle,
						    pProp->m_sTitle,
						    pProp->m_sTitle.GetLength());
						dc.SelectFont(hOldFont);
						
					}
					else
					{
						const int XPOS_INDENT = 8;
						
						CBrush brBack;
						brBack.CreateSolidBrush(clrBack);
						
						dc.SetBkColor(clrBack);
						dc.SetTextColor(clrFront);
						
						RECT rcWest = lpdis->rcItem;
						rcWest.right = m_xposMiddle;
						dc.FillRect(&rcWest, brBack);
						
						HFONT hOldFont = dc.SelectFont(m_hFont);
						dc.TextOut(
						    lpdis->rcItem.left + XPOS_INDENT,
						    lpdis->rcItem.top + m_yposMiddle,
						    pProp->m_sTitle,
						    pProp->m_sTitle.GetLength());
						    
						dc.SetBkColor(m_clrBack);
						dc.SetTextColor(m_clrText);
						
						RECT rcEast = lpdis->rcItem;
						rcEast.left = m_xposMiddle + 1;
						pProp->DrawValue(dc, rcEast, m_yposMiddle);
						
						dc.SelectFont(hOldFont);
					}
					
					break;
			}
			
			// Draw lines
			CPen pen;
			pen.CreatePen(PS_SOLID, 1, m_clrBorder);
			HPEN hOldPen = dc.SelectPen(pen);
			dc.MoveTo(lpdis->rcItem.left, lpdis->rcItem.bottom - 1);
			dc.LineTo(lpdis->rcItem.right, lpdis->rcItem.bottom - 1);
			// Draw separator line
			if (pProp->m_Type != PROP_GROUP)
			{
				dc.MoveTo(m_xposMiddle, lpdis->rcItem.top);
				dc.LineTo(m_xposMiddle, lpdis->rcItem.bottom - 1);
			}
			dc.SelectPen(hOldPen);
			
			// Reset the background color and the text color back to their
			// original values.
			dc.SetTextColor(clrOldTextColor);
			dc.SetBkColor(clrOldBkColor);
			
			return TRUE;
		}
		
		LRESULT OnDeleteItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
		{
			LPDELETEITEMSTRUCT lpdis = (LPDELETEITEMSTRUCT)lParam;
			ATLASSERT(lpdis->itemData);
			delete(CProperty*) lpdis->itemData;
			return 0;
		}
		
};

class CPropertyViewCtrl : public CPropertyViewImpl<CPropertyViewCtrl>
{
	public:
		DECLARE_WND_CLASS(_T("WTL_PropertyView"))
};


#endif // !defined(AFX_PROPERTYVIEW_H__20000930_54A8_5070_44B5_0080AD509054__INCLUDED_)

