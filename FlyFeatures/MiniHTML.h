#if !defined(AFX_MINIHTML_H__20030410_48EF_4F9B_B334_0080AD509054__INCLUDED_)
#define AFX_MINIHTML_H__20030410_48EF_4F9B_B334_0080AD509054__INCLUDED_

#pragma once

/////////////////////////////////////////////////////////////////////////////
// MiniHTML - Mini HTML Label
//
// Written by Bjarke Viksoe (bjarke@viksoe.dk)
// Copyright (c) 2003 Bjarke Viksoe.
//
// This code may be used in compiled form in any way you desire. This
// source file may be redistributed by any means PROVIDING it is 
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
  #error MiniHTML.h requires atlapp.h to be included first
#endif

#ifndef __ATLCTRLS_H__
  #error MiniHTML.h requires atlctrls.h to be included first
#endif

#ifdef _ATL_MIN_CRT
  #error MiniHTML.h does not compile with _ATL_MIN_CRT
#endif

#if (_WIN32_IE < 0x0400)
  #error MiniHTML.h requires _WIN32_IE >= 0x0400
#endif


/////////////////////////////////////////////////////////////////////////////
// Mini HTML Painter

class CMiniHTML
{
public:
   RECT m_rc;
   COLORREF m_clrText;
   COLORREF m_clrBack;
   LONG m_lMargin;
   HFONT m_hFont;
   int m_iCRC;
   bool m_bRecalc;
   bool m_bWordWrap;

   typedef enum COMMAND
   {
      CMD_DRAW,
      CMD_SPECIALCHAR,
      CMD_CONTEXT,
      CMD_NEWLINE,
   };
   typedef struct ACTION
   {
      COMMAND Cmd;           // Command
      int cchMax;            // Skip chars
      union
      {
         int iContext;       // New font to use
         int iHeight;        // Height of line
         TCHAR ch;           // Char
      };
   };
   typedef struct FONT
   {
      HFONT hFont;
      LOGFONT lf;
      TEXTMETRIC tm;
   };
   typedef struct CONTEXT
   {
      int iFont;
      COLORREF clrText;
      COLORREF clrBack;
      UINT uStyle;
   };
   CSimpleValArray<FONT> m_aFonts;
   CSimpleValArray<CONTEXT> m_aContexts;
   CSimpleArray<ACTION> m_aActions;
   CSimpleValArray<int> m_aFontStack;

   CMiniHTML() :
      m_iCRC(0L),
      m_clrText(CLR_INVALID),
      m_clrBack(CLR_INVALID),
      m_lMargin(0L),
      m_hFont(NULL),
      m_bRecalc(true),
      m_bWordWrap(true)
   {
   }
   ~CMiniHTML()
   {
      _ClearActions();
   }

   // Operations

   void SetMargin(LONG lMargin)
   {
      m_lMargin = lMargin;
      m_bRecalc = true;
   }
   void SetWordWrap(BOOL bWrap)
   {
      m_bWordWrap = bWrap == TRUE;
      m_bRecalc = true;
   }

   BOOL Paint(HDC hDC, 
      RECT rc, 
      LPCTSTR pstrText, 
      COLORREF clrText, 
      COLORREF clrBack, 
      HFONT hFont)
   {
      ATLASSERT(!::IsBadStringPtr(pstrText,-1));
      ATLASSERT(::GetObjectType(hDC)==OBJ_DC);
      ATLASSERT(::GetObjectType(hFont)==OBJ_FONT);

      CDCHandle dc = hDC;
      int savedc = dc.SaveDC();

      // Detect changes in settings/colors/text...
      if( _GetTextCrc(pstrText) != m_iCRC 
          || clrText != m_clrText 
          || clrBack != m_clrBack 
          || hFont != m_hFont 
          || memcmp(&rc, &m_rc, sizeof(rc)) != 0 
          || m_bRecalc )
      {
         // Rebuild action set
         _Parse(dc, rc, pstrText, clrText, clrBack, hFont);
         m_bRecalc = false;
      }

      // Apply margin
      ::InflateRect(&rc, -LOWORD(m_lMargin), -HIWORD(m_lMargin));

      // And erase background
      CBrush brBack;
      brBack.CreateSolidBrush(clrBack);
      dc.FillRect(&rc, brBack);
      dc.SetBkMode(TRANSPARENT);

      // Parse action set...
      RECT rcPos = rc;
      UINT uStyle = 0;
      int nCount = m_aActions.GetSize();
      for( int i = 0; i < nCount; i++ ) {
         ACTION& action = m_aActions[i];
         switch( action.Cmd ) {
         case CMD_DRAW:
            {
               SIZE sizeText;
               dc.GetTextExtent(pstrText, action.cchMax, &sizeText);
               if( m_bWordWrap ) {
                  // Draw word-wrapped text
                  LPCTSTR pstr = pstrText;
                  int nLen = action.cchMax;
                  int nCopied = 0;
                  while( nCopied < action.cchMax ) {
                     while( nLen > 0 && rcPos.left + sizeText.cx > m_rc.right ) {
                        while( true ) {
                           nLen--;
                           if( nLen == 0 || *(pstr + nLen - 1) == _T(' ') ) break;
                           dc.GetTextExtent(pstr, nLen, &sizeText);
                        }
                     }
                     if( nLen == 0 ) nLen = action.cchMax - nCopied;
                     dc.DrawText(pstr, nLen, &rcPos, DT_SINGLELINE | DT_TOP | DT_NOPREFIX | uStyle);
                     pstr += nLen;
                     nCopied += nLen;
                     nLen = action.cchMax - nCopied;
                     if( nCopied < action.cchMax ) {
                        rcPos.left = rc.left;
                        rcPos.top += action.iHeight;
                     }
                     else {
                        rcPos.left += sizeText.cx;
                     }
                  }
               }
               else {
                  // Draw single line text
                  dc.DrawText(pstrText, action.cchMax, &rcPos, DT_SINGLELINE | DT_TOP | DT_NOPREFIX | uStyle);
                  rcPos.left += sizeText.cx;
               }
            }
            break;
         case CMD_SPECIALCHAR:
            {
               dc.DrawText(&action.ch, 1, &rcPos, DT_SINGLELINE | DT_TOP | DT_NOPREFIX | uStyle );
               SIZE sizeText;
               dc.GetTextExtent(&action.ch, 1, &sizeText);
               rcPos.left += sizeText.cx;
            }
            break;
         case CMD_NEWLINE:
            {
               if( action.iHeight == 0 ) {
                  TCHAR ch = _T('X');
                  SIZE sizeText;
                  dc.GetTextExtent(&ch, 1, &sizeText);
                  action.iHeight = sizeText.cy;
               }
               rcPos.left = rc.left;
               rcPos.top += action.iHeight;
            }
            break;
         case CMD_CONTEXT:
            {
               CONTEXT c = m_aContexts[action.iContext];
               FONT f = m_aFonts[c.iFont];
               dc.SelectFont(f.hFont);
               dc.SetBkColor(c.clrBack);
               dc.SetTextColor(c.clrText);
               uStyle = c.uStyle;
            }           
            break;
         }
         pstrText += action.cchMax;
      }

      dc.RestoreDC(savedc);
      return TRUE;
   }

   // Implementation

   BOOL _Parse(CDCHandle& dc, 
      RECT rc, 
      LPCTSTR pstrText, 
      COLORREF clrText, 
      COLORREF clrBack, 
      HFONT hFont)
   {
      // Destroy old action set
      _ClearActions();
      if( ::IsBadStringPtr(pstrText,(UINT)-1) ) return FALSE;

      // Remember these new settings.
      // This will allow us to determine if we need to recalculate
      // the entire action set if any visuals change...
      m_iCRC = _GetTextCrc(pstrText);
      m_rc = rc;
      m_clrText = clrText;
      m_clrBack = clrBack;
      m_hFont = hFont;

      FONT f = { 0 };
      CFontHandle font = hFont;
      font.GetLogFont(&f.lf);
      f.hFont = ::CreateFontIndirect(&f.lf);
      dc.SelectFont(f.hFont);
      dc.GetTextMetrics(&f.tm);
      m_aFonts.Add(f);

      CONTEXT c = { 0 };
      c.iFont = 0;
      c.clrText = clrText;
      c.clrBack = clrBack;
      m_aContexts.Add(c);

      ACTION start = { CMD_CONTEXT, 0, 0 };
      m_aActions.Add(start);

      ACTION draw = { CMD_DRAW };
      draw.iHeight = 0;
      while( *pstrText ) {
         switch( *pstrText ) {
         case _T('<'):
            {
               if( draw.cchMax > 0 ) m_aActions.Add(draw); draw.cchMax = 0;
               _ParseTag(dc, pstrText, c, f, draw.iHeight);
            }
            break;
         case _T('&'):
            {
               if( draw.cchMax > 0 ) m_aActions.Add(draw); draw.cchMax = 0;
               _ParseSpecialChar(pstrText);
            }
            break;
         case _T('\n'):
            {
               if( draw.cchMax > 0 ) m_aActions.Add(draw); draw.cchMax = 0;
               ACTION lf = { CMD_NEWLINE, 1, draw.iHeight };
               m_aActions.Add(lf);
               draw.iHeight = 0;
               pstrText = ::CharNext(pstrText);
            }
            break;
         default:
            draw.cchMax += ::CharNext(pstrText) - pstrText;
            draw.iHeight = std::max(LONG(draw.iHeight), f.tm.tmHeight);
            pstrText = ::CharNext(pstrText);
         }
      }
      if( draw.cchMax > 0 ) m_aActions.Add(draw);

      return TRUE;
   }

   void _ParseSpecialChar(LPCTSTR& pstrText)
   {
      static LPCTSTR szPseudo[] = 
      {
         _T("&nbsp;"), _T(" "),
         _T("&quot;"), _T("\""),
         _T("&apos;"), _T("\'"),
         _T("&amp;"), _T("&"),
         _T("&lt;"), _T("<"),
         _T("&gt;"), _T(">"),
         NULL, NULL 
      };
      LPCTSTR* ppstrPseudo = szPseudo;
      while( *ppstrPseudo ) {
         int nLen = ::lstrlen(*ppstrPseudo);
         if( _tcsncmp(pstrText, *ppstrPseudo, nLen) == 0 ) {
            pstrText += nLen;
            ACTION action = { CMD_SPECIALCHAR, nLen, *(ppstrPseudo + 1)[0] };
            m_aActions.Add(action);
            break;
         }
         ppstrPseudo += 2;
      }
      ATLASSERT(*ppstrPseudo); // Found a replacement?
   }
   void _ParseTag(CDCHandle dc, LPCTSTR& pstrText, CONTEXT& c, FONT& f, int& iHeight)
   {
      LPCTSTR pstrStart = pstrText;
      while( *pstrText && *pstrText != _T('>') && *pstrText != _T(' ') ) pstrText = ::CharNext(pstrText);
      int nTagLen = (pstrText - pstrStart) - 1;
      TCHAR szTag[16] = { 0 };
      ::memcpy(szTag, pstrStart + 1, nTagLen * sizeof(TCHAR));
      ::CharUpperBuff(szTag, nTagLen);

      LPCTSTR pstrAttribs = pstrText;
      while( *pstrText && *pstrText != _T('>') ) pstrText = ::CharNext(pstrText);
      pstrText = ::CharNext(pstrText);
      TCHAR szAttribs[128] = { 0 };
      ::lstrcpyn(szAttribs, pstrAttribs, min(pstrText - pstrAttribs, 127));
      ::CharUpperBuff(szAttribs, ::lstrlen(szAttribs));

      TCHAR szValue[128];
      bool bPushStack = false;
      bool bPopStack = false;

      if( szTag[0] != _T('/') ) 
      {
         if( ::lstrcmp(szTag, _T("B")) == 0 ) f.lf.lfWeight = FW_BOLD;
         else if( ::lstrcmp(szTag, _T("I")) == 0 ) f.lf.lfItalic = TRUE;
         else if( ::lstrcmp(szTag, _T("UL")) == 0 ) f.lf.lfUnderline = TRUE;
         else if( ::lstrcmp(szTag, _T("FONT")) == 0 ) {
            if( _GetAttribute(szAttribs, _T(" COLOR"), szValue) ) c.clrText = _GetColor(szValue);
            if( _GetAttribute(szAttribs, _T(" SIZE"), szValue) ) f.lf.lfHeight = _GetFontSize(szValue);
            if( _GetAttribute(szAttribs, _T(" FACE"), szValue) ) ::lstrcpy(f.lf.lfFaceName, szValue);
            bPushStack = true;
         }
         else if( ::lstrcmp(szTag, _T("DIV")) == 0 ) {
            if( _GetAttribute(szAttribs, _T(" ALIGN"), szValue) ) {
               if( ::lstrcmp(szValue, _T("CENTER")) == 0 ) c.uStyle = DT_CENTER;
               if( ::lstrcmp(szValue, _T("LEFT")) == 0 ) c.uStyle = DT_LEFT;
               if( ::lstrcmp(szValue, _T("RIGHT")) == 0 ) c.uStyle = DT_RIGHT;
            }
            bPushStack = true;
         }
         else if( ::lstrcmp(szTag, _T("BR")) == 0 ) {
            ACTION lf = { CMD_NEWLINE, 0, iHeight };
            m_aActions.Add(lf);
            iHeight = 0;
         }
      }
      else
      {
         if( ::lstrcmp(szTag, _T("/B")) == 0 ) f.lf.lfWeight = FW_NORMAL;
         else if( ::lstrcmp(szTag, _T("/I")) == 0 ) f.lf.lfItalic = FALSE;
         else if( ::lstrcmp(szTag, _T("/UL")) == 0 ) f.lf.lfUnderline = FALSE;
         else if( ::lstrcmp(szTag, _T("/FONT")) == 0 ) bPopStack = true;
         else if( ::lstrcmp(szTag, _T("/DIV")) == 0 ) bPopStack = true;
      }

      if( bPushStack ) {
         m_aFontStack.Add(_GetContext(c));
      }
      else if( bPopStack ) {
         ATLASSERT(m_aFontStack.GetSize()>0); // Tag mismatch
         int iContext = m_aFontStack[m_aFontStack.GetSize() - 1];
         m_aFontStack.RemoveAt(m_aFontStack.GetSize() - 1);
         c = m_aContexts[iContext];
         f = m_aFonts[c.iFont];
      }

      c.iFont = _GetFont(dc, f);
      int iContext = _GetContext(c);
      int cchMax = pstrText - pstrStart;
      ACTION action = { CMD_CONTEXT, cchMax, iContext };
      m_aActions.Add(action);
   }
   int _GetTextCrc(LPCTSTR pstrText) const
   {
      if( pstrText == NULL ) return 0;
      int iCrc = 0;
      while( *pstrText ) iCrc = (iCrc << 5) + iCrc + *pstrText++;
      return iCrc;
   }
   int _GetFont(CDCHandle dc, FONT& f)
   {
      for( int i = 0; i < m_aFonts.GetSize(); i++ ) {
         LOGFONT lf = m_aFonts[i].lf;
         if( memcmp(&lf, &f.lf, sizeof(LOGFONT)) == 0 ) return i;
      }
      f.hFont = ::CreateFontIndirect(&f.lf);
      dc.SelectFont(f.hFont);
      dc.GetTextMetrics(&f.tm);
      m_aFonts.Add(f);
      return m_aFonts.GetSize() - 1;
   }
   int _GetContext(CONTEXT& c)
   {
      for( int i = 0; i < m_aContexts.GetSize(); i++ ) {
         CONTEXT oc = m_aContexts[i];
         if( memcmp(&oc, &c, sizeof(CONTEXT)) == 0 ) return i;
      }
      m_aContexts.Add(c);
      return m_aContexts.GetSize() - 1;
   }
   void _ClearActions()
   {
      for( int i = 0; i < m_aFonts.GetSize(); i++ ) ::DeleteObject(m_aFonts[i].hFont);
      m_aFonts.RemoveAll();
      m_aContexts.RemoveAll();
      m_aActions.RemoveAll();
      m_aFontStack.RemoveAll();
   }
   bool _GetAttribute(LPCTSTR pszTag, LPCTSTR pszName, LPTSTR pstrValue) const
   {
      *pstrValue = _T('\0');
      LPCTSTR p = _tcsstr(pszTag, pszName);
      if( p == NULL ) return false;
      p += ::lstrlen(pszName);
      while( _istspace(*p) ) p = ::CharNext(p);
      if( *p != _T('=') ) return true;
      p = ::CharNext(p);
      while( _istspace(*p) ) p = ::CharNext(p);
      TCHAR c = *p;
      switch( c ) {
      case _T('\"'):
      case _T('\''):
         p = ::CharNext(p);
         while( *p && *p != c ) *pstrValue++ = *p++;
         break;
      default:
         while( *p && (::IsCharAlphaNumeric(*p) || *p == _T('-') || *p == _T('#')) ) *pstrValue++ = *p++;
         break;
      }
      *pstrValue = _T('\0');
      return true;
   }
   COLORREF _GetColor(LPCTSTR pstrValue) const
   {
      COLORREF clr;
      if( *pstrValue == _T('#') ) {
         pstrValue = ::CharNext(pstrValue);
         LPTSTR pstrStop;
         DWORD BGR = _tcstol(pstrValue, &pstrStop, 16); // convert hex value
         clr = RGB( (BGR & 0xFF0000) >> 16, (BGR & 0xFF00) >> 8, BGR & 0xFF);
      }
      else {
         // TODO: Implement more colors-by-name
         if( ::lstrcmp(pstrValue, _T("WINDOW"))==0 ) clr = ::GetSysColor(COLOR_WINDOW);
         else if( ::lstrcmp(pstrValue, _T("WINDOWTEXT"))==0 ) clr = ::GetSysColor(COLOR_WINDOWTEXT);
         else if( ::lstrcmp(pstrValue, _T("BUTTONFACE"))==0 ) clr = ::GetSysColor(COLOR_BTNFACE);
         else if( ::lstrcmp(pstrValue, _T("BUTTONTEXT"))==0 ) clr = ::GetSysColor(COLOR_BTNTEXT);
         else clr = RGB(0,0,0);
      }
      return clr;
   }
   int _GetFontSize(LPCTSTR pstrValue) const
   {
      int lFontSize = _GetInteger(pstrValue);
      switch( lFontSize ) {
      case 1: return 15; // 7.5
      case 2: return 20; // 10
      case 3: return 24; // 12
      case 4: return 27; // 13.5
      case 5: return 36; // 18
      case 6: return 48; // 24
      case 7: return 72; // 36
      default: return 24;
      }
   }
   int _GetInteger(LPCTSTR pstrValue) const
   {
      return _ttol(pstrValue);
   }
};


/////////////////////////////////////////////////////////////////////////////
// Mini HTML Label

template< class T, class TBase = CStatic, class TWinTraits = CControlWinTraits >
class ATL_NO_VTABLE CMiniHtmlImpl : public CWindowImpl< T, TBase, TWinTraits >
{
public:
   typedef CMiniHtmlImpl< T, TBase, TWinTraits > thisClass;

   CMiniHTML m_minihtml;
   CToolTipCtrl m_tip;
   COLORREF m_clrText;
   COLORREF m_clrBack;

   // Operations

   BOOL SubclassWindow(HWND hWnd)
   {
      ATLASSERT(m_hWnd==NULL);
      ATLASSERT(::IsWindow(hWnd));
      BOOL bRet = CWindowImpl< T, TBase, TWinTraits >::SubclassWindow(hWnd);
      if( bRet ) _Init();
      return bRet;
   }

   void SetTextColor(COLORREF clrText)
   {
      m_clrText = clrText;
      if( IsWindow() ) Invalidate();
   }
   void SetBkColor(COLORREF clrBack)
   {
      m_clrBack = clrBack;
      if( IsWindow() ) Invalidate();
   }
   void SetMargin(SIZE szMargin)
   {
      m_minihtml.SetMargin(MAKELONG(szMargin.cx, szMargin.cy));
   }
   void SetWordWrap(BOOL bWrap)
   {
      m_minihtml.SetWordWrap(bWrap);
   }
   BOOL SetToolTipText(LPCTSTR pstrText)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ATLASSERT(GetStyle() & SS_NOTIFY);
      if( m_tip.IsWindow() ) m_tip.DestroyWindow();  
      if( pstrText == NULL ) return FALSE;
      m_tip.Create(m_hWnd);
      ATLASSERT(m_tip.IsWindow());
      m_tip.Activate(TRUE);
      m_tip.AddTool(m_hWnd, pstrText);
      return m_tip.IsWindow();
   }

   // Implementation

   void _Init()
   {
      ATLASSERT(::IsWindow(m_hWnd));

      m_clrText = CLR_INVALID;
      m_clrBack = CLR_INVALID;
   }

   // Message map and handlers

   BEGIN_MSG_MAP( thisClass )
      MESSAGE_HANDLER(WM_CREATE, OnCreate)
      MESSAGE_HANDLER(WM_SETTEXT, OnSetText)
      MESSAGE_RANGE_HANDLER(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseMessage)
      MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
      MESSAGE_HANDLER(WM_PAINT, OnPaint)
      MESSAGE_HANDLER(WM_PRINTCLIENT, OnPaint)
   END_MSG_MAP()

   LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      _Init();
      return 0;
   }
   LRESULT OnSetText(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      LRESULT lRes = DefWindowProc();
      Invalidate();
      UpdateWindow();
      return lRes;
   }
   LRESULT OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      return 1; // We're painting it all
   }
   LRESULT OnPaint(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
   {
      T* pT = static_cast<T*>(this);
      if( wParam != NULL ) {
         pT->DoPaint( (HDC) wParam );
      }
      else {
         CPaintDC dc(m_hWnd);
         pT->DoPaint( (HDC) dc );
      }
      return 0;
   }
   LRESULT OnMouseMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
   {
      MSG msg = { m_hWnd, uMsg, wParam, lParam };
      if( m_tip.IsWindow() ) m_tip.RelayEvent(&msg);
      bHandled = FALSE;
      return 0;
   }

   // Paint methods

   void DoPaint(CDCHandle dc)
   {
      RECT rcClient;
      GetClientRect(&rcClient);

      int nLen = GetWindowTextLength();
      LPTSTR pstrText = (LPTSTR) _alloca((nLen + 1) * sizeof(TCHAR));
      GetWindowText(pstrText, nLen + 1);

      COLORREF clrText = m_clrText == CLR_INVALID ? ::GetSysColor(COLOR_BTNTEXT) : m_clrText;
      COLORREF clrBack = m_clrBack == CLR_INVALID ? ::GetSysColor(COLOR_BTNFACE) : m_clrBack;
      HFONT hFont = GetFont();

      m_minihtml.Paint(dc, 
         rcClient, 
         pstrText, 
         clrText, 
         clrBack, 
         hFont);
   }
};

class CMiniHtmlCtrl : public CMiniHtmlImpl<CMiniHtmlCtrl>
{
public:
  DECLARE_WND_CLASS(_T("WTL_MiniHtml"))
};


#endif // !defined(AFX_MINIHTML_H__20030410_48EF_4F9B_B334_0080AD509054__INCLUDED_)

