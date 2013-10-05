#ifndef __ATLWINMISC_H__
#define __ATLWINMISC_H__

/////////////////////////////////////////////////////////////////////////////
// Misc wrappers for the Windows API
//
// Written by Bjarke Viksoe (bjarke@viksoe.dk)
// Copyright (c) 2000-2002 Bjarke Viksoe.
//
// Thanks to:
//   Tim Tabor for the CResModule suggestion
//   Anatoly Ivasyuk for fixing the AtlLoadHTML sz-string problem
//
// Wraps:
//   SetLayeredWindowAttributes
//   AnimateWindow
//   DeferWindowPos
//   Windows Resource API
//   String types (with better localization support)
//   Window Text
//   LockWindowUpdate
//   WM_SETREDRAW
//   LoadLibrary
//   GetClientRect
//   GetWindowRect
//   WINDOWPLACEMENT
//   Windows Properties
//   Ini-files (WritePrivateProfileString() etc)
//
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

#pragma once

#ifndef __cplusplus
   #error ATL requires C++ compilation (use a .cpp suffix)
#endif

#if (_WTL_VER < 0x0700)
   #error This file requires WTL version 7.0 or higher
#endif


#ifndef _ATL_DLL_IMPL
namespace WTL
{
#endif // _ATL_DLL_IMPL


/////////////////////////////////////////////////////////////////////////////
// Windows Message macros

#define CHAIN_COMMANDS_HWND(hWnd) \
   if(uMsg == WM_COMMAND && hWnd != NULL) \
      ::SendMessage(hWnd, uMsg, wParam, lParam);

#define CHAIN_CLIENT_WMAPP() \
   if(uMsg >= WM_APP && m_hWndClient != NULL) \
      ::SendMessage(m_hWndClient, uMsg, wParam, lParam);

#define CHAIN_MDI_CHILD_WMAPP() \
   if(uMsg >= WM_APP) \
   { \
      HWND hWndChild = MDIGetActive(); \
      if(hWndChild != NULL) \
         ::SendMessage(hWndChild, uMsg, wParam, lParam); \
   }


/////////////////////////////////////////////////////////////////////////////
// Message Loop with Idle count

class CIdleHandlerEx
{
public:
   virtual BOOL OnIdle(int nCount) = 0;
};

class CMessageLoopEx : public CMessageLoop
{
public:
   CSimpleArray<CIdleHandlerEx*> m_aIdleHandler;

   BOOL AddIdleHandler(CIdleHandlerEx* pIdleHandler)
   {
      return m_aIdleHandler.Add(pIdleHandler);
   }
   
   BOOL RemoveIdleHandler(CIdleHandlerEx* pIdleHandler)
   {
      return m_aIdleHandler.Remove(pIdleHandler);
   }
   
   virtual BOOL OnIdle(int nIdleCount)
   {
      for( int i = 0; i < m_aIdleHandler.GetSize(); i++ )
      {
         CIdleHandlerEx* pIdleHandler = m_aIdleHandler[i];
         if( pIdleHandler != NULL ) if( !pIdleHandler->OnIdle(nIdleCount) ) return FALSE;
      }
      return TRUE;
   }
};


#ifdef __ATLWIN_H__

/////////////////////////////////////////////////////////////////////////////
// Transparent window - safe usage of Win2K alpha layer window

// Platform SDK constants for layered window (Win2K and above only)
#ifndef WS_EX_LAYERED
   #define WS_EX_LAYERED           0x00080000
   #define LWA_COLORKEY            0x00000001
   #define LWA_ALPHA               0x00000002
#endif // WS_EX_LAYERED

class CTransparentWindow : public CWindow
{
public:
   CTransparentWindow(HWND hWnd = NULL) : CWindow(hWnd)
   {
   }

   CTransparentWindow& operator=(HWND hWnd)
   {
      m_hWnd = hWnd;
      return *this;
   }

   BOOL SetLayeredWindowAttributes(COLORREF crKey, BYTE bAlpha, DWORD dwFlags)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      typedef BOOL (WINAPI *PFNSETLAYEREDWINDOWATTRIBUTES)(HWND, COLORREF, BYTE, DWORD);
      PFNSETLAYEREDWINDOWATTRIBUTES pfnSetLayeredWindowAttributes = 
         (PFNSETLAYEREDWINDOWATTRIBUTES) ::GetProcAddress( ::GetModuleHandle(_T("user32.dll")), "SetLayeredWindowAttributes");
      if( pfnSetLayeredWindowAttributes == NULL ) return FALSE;
      return pfnSetLayeredWindowAttributes(m_hWnd, crKey, bAlpha, dwFlags);
   }
   
   BOOL UpdateLayeredWindow(HDC hdcDst, POINT *pptDst, SIZE *psize, HDC hdcSrc, POINT *pptSrc, COLORREF crKey, BLENDFUNCTION *pblend, DWORD dwFlags)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      typedef BOOL (WINAPI *PFNUPDATELAYEREDWINDOW)(HWND, HDC, POINT*, SIZE*, HDC, POINT*, COLORREF, BLENDFUNCTION*, DWORD);
      PFNUPDATELAYEREDWINDOW pfnUpdateLayeredWindow = 
         (PFNUPDATELAYEREDWINDOW) ::GetProcAddress( ::GetModuleHandle(_T("user32.dll")), "UpdateLayeredWindow");
      if( pfnUpdateLayeredWindow == NULL ) return FALSE;
      return pfnUpdateLayeredWindow(m_hWnd, hdcDst, pptDst, psize, hdcSrc, pptSrc, crKey, pblend, dwFlags);
   }
};


/////////////////////////////////////////////////////////////////////////////
// CAnimateWindow

// Platform SDK constants for animate effects (Win2k and above only)
#ifndef AW_BLEND
   #define AW_HOR_POSITIVE             0x00000001
   #define AW_HOR_NEGATIVE             0x00000002
   #define AW_VER_POSITIVE             0x00000004
   #define AW_VER_NEGATIVE             0x00000008
   #define AW_HIDE                     0x00010000
   #define AW_ACTIVATE                 0x00020000
   #define AW_SLIDE                    0x00040000
   #define AW_BLEND                    0x00080000
#endif // AW_BLEND

// Wraps AnimateWindow() on Win2K
class CAnimateWindow : public CWindow
{
public:
   CAnimateWindow(HWND hWnd = NULL) : CWindow(hWnd)
   {
   }
   
   CAnimateWindow& operator=(HWND hWnd)
   {
      m_hWnd = hWnd;
      return *this;
   }

   BOOL AnimateWindow(DWORD dwTime = 200, DWORD dwFlags = AW_BLEND|AW_ACTIVATE)
   {
      typedef BOOL (CALLBACK* PFNANIMATEWINDOW)(HWND,DWORD,DWORD);
      if( !AtlIsOldWindows() ) {
         PFNANIMATEWINDOW pfnAnimateWindow = (PFNANIMATEWINDOW)
            ::GetProcAddress(::GetModuleHandle(_T("user32.dll")), "AnimateWindow");
         if( pfnAnimateWindow != NULL ) return pfnAnimateWindow( m_hWnd, dwTime, dwFlags );
      }
      return FALSE;
   }
};


/////////////////////////////////////////////////////////////////////////////
// CLockStaticDataInit

struct CLockStaticDataInit
{
#if (_ATL_VER >= 0x0700)
   CComCritSecLock<CComCriticalSection> m_cslock;
   CLockStaticDataInit() : m_cslock(_pAtlModule->m_csStaticDataInitAndTypeInfo, true) { };
#else
   CLockStaticDataInit() { ::EnterCriticalSection(&_Module.m_csStaticDataInit); };
   ~CLockStaticDataInit() { ::LeaveCriticalSection(&_Module.m_csStaticDataInit); };
#endif // _ATL_VER
};


#endif // __ATLWIN_H__


#if _WTL_VER < 0x0750

/////////////////////////////////////////////////////////////////////////////
// CResource

// Wraps a Windows resource.
// Use it with custom resource types other than the standard
// RT_CURSOR, RT_BITMAP etc etc.
class CResource
{
public:
   HGLOBAL m_hglb;
   HRSRC m_hrsrc;
   bool m_bLocked;

   CResource() : m_hglb(NULL), m_hrsrc(NULL), m_bLocked(false)
   { 
   }
   
   ~CResource()
   {
      Release();
   }
   
   BOOL Load(_U_STRINGorID Type, _U_STRINGorID ID)
   {
      ATLASSERT(m_hrsrc==NULL);
      ATLASSERT(m_hglb==NULL);
#if (_ATL_VER >= 0x0700)
      m_hrsrc = ::FindResource(ATL::_AtlBaseModule.GetResourceInstance(), ID.m_lpstr, Type.m_lpstr);
#else
      m_hrsrc = ::FindResource(_Module.GetResourceInstance(), ID.m_lpstr, Type.m_lpstr);
#endif // _ATL_VER
      if( m_hrsrc == NULL ) return FALSE;
#if (_ATL_VER >= 0x0700)
      m_hglb = ::LoadResource(ATL::_AtlBaseModule.GetResourceInstance(), m_hrsrc);
#else
      m_hglb = ::LoadResource(_Module.GetResourceInstance(), m_hrsrc);
#endif // _ATL_VER
      if( m_hglb == NULL ) {
         m_hrsrc = NULL;
         return FALSE;
      }
      return TRUE;
   }
   
   BOOL LoadEx(_U_STRINGorID Type, _U_STRINGorID ID, WORD wLanguage)
   {
      ATLASSERT(m_hrsrc==NULL);
      ATLASSERT(m_hglb==NULL);
#if (_ATL_VER >= 0x0700)
      m_hrsrc = ::FindResourceEx(ATL::_AtlBaseModule.GetResourceInstance(), ID.m_lpstr, Type.m_lpstr, wLanguage);
#else
      m_hrsrc = ::FindResourceEx(_Module.GetResourceInstance(), ID.m_lpstr, Type.m_lpstr, wLanguage);
#endif // _ATL_VER
      if( m_hrsrc == NULL ) return FALSE;
#if (_ATL_VER >= 0x0700)
      m_hglb = ::LoadResource(ATL::_AtlBaseModule.GetResourceInstance(), m_hrsrc);
#else
      m_hglb = ::LoadResource(_Module.GetResourceInstance(), m_hrsrc);
#endif // _ATL_VER
      if( m_hglb == NULL ) {
         m_hrsrc = NULL;
         return FALSE;
      }
      return TRUE;
   }
   
   DWORD GetSize() const
   {
      ATLASSERT(m_hrsrc);
#if (_ATL_VER >= 0x0700)
      return ::SizeofResource(ATL::_AtlBaseModule.GetResourceInstance(), m_hrsrc);
#else
      return ::SizeofResource(_Module.GetResourceInstance(), m_hrsrc);
#endif // _ATL_VER
   }
   
   LPVOID Lock()
   {
      ATLASSERT(m_hrsrc);
      ATLASSERT(m_hglb);
      ATLASSERT(!m_bLocked);
      LPVOID pVoid = ::LockResource(m_hglb);
      ATLASSERT(pVoid);
      m_bLocked = true;
      return pVoid;
   }
   
   void Unlock()
   {
      // NOTE: UnlockResource is not required on Windows, but included
      //       here for completeness. Who knows what Microsoft needs to
      //       bring back to life in WinCE and related versions of Windows.
      ATLASSERT(m_hrsrc);
      ATLASSERT(m_hglb);
      ATLASSERT(m_bLocked);
      m_bLocked = false;
      UnlockResource(m_hglb);
   }
   
   void Release()
   {
      if( m_bLocked ) Unlock();
      if( m_hglb != NULL ) ::FreeResource(m_hglb);
      m_hglb = NULL;
      m_hrsrc = NULL;
   }
};

#endif // _WTL_VER


/////////////////////////////////////////////////////////////////////////////
// CResString

// Stack-based string buffer. Loads strings from the string-resource maps.
template< int nSize >
class CResString
{
public:
   TCHAR m_szStr[nSize];

   CResString()
   {
      m_szStr[0] = '\0';
   }
   
   CResString(UINT ID)
   {
      LoadString(ID);
   }
   
   inline int LoadString(UINT ID)
   {
#if (_ATL_VER >= 0x0700)
      return ::LoadString(ATL::_AtlBaseModule.GetResourceInstance(), ID, m_szStr, sizeof(m_szStr)/sizeof(TCHAR));
#else
      return ::LoadString(_Module.GetResourceInstance(), ID, m_szStr, sizeof(m_szStr)/sizeof(TCHAR));
#endif // _ATL_VER
   }
#ifdef _VA_LIST_DEFINED
   
   int FormatString(UINT ID, ...)
   {
      TCHAR szFormat[nSize];
#if (_ATL_VER >= 0x0700)
      int cch = ::LoadString(ATL::_AtlBaseModule.GetResourceInstance(), ID, szFormat, sizeof(szFormat)/sizeof(TCHAR));
#else
      int cch = ::LoadString(_Module.GetResourceInstance(), ID, szFormat, sizeof(szFormat)/sizeof(TCHAR));
#endif // _ATL_VER
      if( cch == 0 ) return 0;
      va_list arglist;
      va_start(arglist, ID);
      cch = ::wvsprintf(m_szStr, szFormat, arglist);
      va_end(arglist);
      return cch;
   }
#endif // _VA_LIST_DEFINED
   
   inline int GetLength() const
   {
      return ::lstrlen(m_szStr);
   }
   
   operator LPCTSTR() const 
   { 
      return m_szStr; 
   }
};


/////////////////////////////////////////////////////////////////////////////
// CLangString

#if defined(__ATLSTR_H__) || defined(_WTL_USE_CSTRING)

class CLangString : public CString
{
public:
#ifndef CHAR_FUDGE
   #ifdef _UNICODE
      #define CHAR_FUDGE 1    // one TCHAR unused is good enough
   #else
      #define CHAR_FUDGE 2    // two BYTES unused for case of DBC last char
   #endif
#endif // CHAR_FUDGE
   CLangString() { };
   
   CLangString(const CString& s) : CString(s) 
   { 
   }
   
   CLangString(const CLangString& s) : CString(s) 
   { 
   }
   
   CLangString(LPCTSTR pstr) : CString(pstr) 
   { 
   }
   
   CLangString(WORD wLanguage, UINT nID) 
   {
      LoadString(wLanguage, nID); 
   }

   BOOL LoadString(WORD wLanguage, UINT nID)
   {
      // NOTE: This is a copy of the original WTL::String::LoadString() method
      //       but tweaked to call our _LoadString() helper instead.

      // Try fixed buffer first (to avoid wasting space in the heap)
      TCHAR szTemp[256];
      int nCount =  sizeof(szTemp) / sizeof(szTemp[0]);
      int nLen = _LoadString(wLanguage, nID, szTemp, nCount);
      if( nCount - nLen > CHAR_FUDGE ) {
         * (CString*) this = szTemp;
         return nLen > 0;
      }

      // Try buffer size of 512, then larger size until entire string is retrieved
      int nSize = 256;
      do {
         nSize += 256;
         LPTSTR lpstr = GetBuffer(nSize - 1);
         if( lpstr == NULL ) {
            nLen = 0;
            break;
         }
         nLen = _LoadString(wLanguage, nID, lpstr, nSize);
      } while( nSize - nLen <= CHAR_FUDGE );
      ReleaseBuffer();

      return nLen > 0;
   }
   
   BOOL __cdecl Format(WORD wLanguage, UINT nFormatID, ...)
   {
      CLangString strFormat;
      BOOL bRet = strFormat.LoadString(wLanguage, nFormatID);
      ATLASSERT(bRet!=0);

      va_list argList;
      va_start(argList, nFormatID);
#ifdef __ATLSTR_H__
      FormatV(strFormat, argList);
      bRet = TRUE;
#else
      bRet = FormatV(strFormat, argList);
#endif // __ATLSTR_H__
      va_end(argList);
      return bRet;
   }

   // Load String helper

   static int __stdcall _LoadString(WORD wLanguage, UINT nID, LPTSTR lpszBuf, UINT nMaxBuf)
   {
#if (_ATL_VER >= 0x0700)
      HRSRC hRsrc = ::FindResourceEx(ATL::_AtlBaseModule.GetResourceInstance(), (LPCTSTR) RT_STRING, (LPCTSTR) MAKEINTRESOURCE((nID>>4)+1), wLanguage); 
#else
      HRSRC hRsrc = ::FindResourceEx(_Module.GetResourceInstance(), (LPCTSTR) RT_STRING, (LPCTSTR) MAKEINTRESOURCE((nID>>4)+1), wLanguage); 
#endif // _ATL_VER
      if( hRsrc == NULL ) { 
#ifdef _DEBUG
         LPVOID lpMsgBuf = NULL;
         ::FormatMessage( 
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            ::GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (LPTSTR) &lpMsgBuf,
            0,
            NULL 
         );
         // Display the error string.
         ::MessageBox(NULL, static_cast<LPTSTR>(lpMsgBuf), NULL, MB_OK | MB_ICONERROR ); 
         ::LocalFree(lpMsgBuf);
#endif // _DEBUG
         lpszBuf[0] = '\0';
         return 0;
      }
#if (_ATL_VER >= 0x0700)
      HGLOBAL hGlb = ::LoadResource(ATL::_AtlBaseModule.GetResourceInstance(), hRsrc);
#else
      HGLOBAL hGlb = ::LoadResource(_Module.GetResourceInstance(), hRsrc);
#endif // _ATL_VER
      LPWSTR lpwsz = static_cast<LPWSTR>(::LockResource(hGlb));
      // Increment the pointer in the resource block up to the required string
      for( UINT i = 0; i < (nID & 15); i++ ) {
         lpwsz += lpwsz[0] + 1; // Jump over preceding resources in the block
      }
      size_t nStrLen = lpwsz[0];
#ifndef _UNICODE
      // BUG: Doesn't really handle Chinese/Japanese DBCS!
      // OPTI: Should use CString::_wcstombsz instead.
      int nRet = wcstombs(lpszBuf, lpwsz + 1, min(nStrLen, (size_t) nMaxBuf));
      if( nRet < (int) nMaxBuf ) lpszBuf[nRet] = '\0';
#else
      wcsncpy(lpszBuf, lpwsz + 1, nMaxBuf);
      int nRet = min( (int) nStrLen, (int) nMaxBuf );
#endif // _UNICODE
      return nRet; 
   }
};

#endif // __ATLSTR_H__


/////////////////////////////////////////////////////////////////////////////
// CModulePath

// Wraps the ::GetModuleFileName() path
class CModulePath
{
public:
   LPTSTR m_pstr; // Reference to string; allows use in wsprintf("%s", xxx) style format

   CModulePath(HINSTANCE hInst = NULL) : m_pstr(NULL)
   {
      Set(hInst);
   }

   CModulePath(const CModulePath& src)
   {
      m_pstr = new TCHAR[MAX_PATH];
      ::lstrcpy(m_pstr, src.m_pstr);
   }

   ~CModulePath()
   {
      if( m_pstr != NULL ) delete [] m_pstr;
   }
   
   void Set(HINSTANCE hInst)
   {
      // Allocate string memory
      if( m_pstr ) delete [] m_pstr;
      m_pstr = new TCHAR[MAX_PATH];
      // Get entire module filepath
#if (_ATL_VER >= 0x0700)
      if( hInst == NULL ) hInst = ATL::_AtlBaseModule.GetResourceInstance();
#else
      if( hInst == NULL ) hInst = _Module.GetModuleInstance();
#endif // _ATL_VER
      ::GetModuleFileName(hInst, m_pstr, MAX_PATH);
      // Remove filename portion
      LPTSTR p = m_pstr, pSep = NULL;
      while( *p ) {
         if( *p == ':' || *p == '\\' || *p == '/' ) pSep = p;
         p = ::CharNext(p);
      }
      ATLASSERT(pSep);
      *(pSep + 1) = '\0';
   }

   operator LPCTSTR() const 
   { 
      return m_pstr; 
   }
};


/////////////////////////////////////////////////////////////////////////////
// CWindowText

class CWindowText
{
public:
   LPTSTR m_pstr;

   CWindowText() : m_pstr(NULL)
   {
   }

   CWindowText(const CWindowText& src)
   {
      int len = ::lstrlen(src.m_pstr) + 1;
      ATLTRY( m_pstr = new TCHAR[len] );
      ATLASSERT(m_pstr);
      ::lstrcpy(m_pstr, src.m_pstr);
   }
   
   CWindowText(HWND hWnd) : m_pstr(NULL)
   {
      Assign(hWnd);
   }
   
   ~CWindowText()
   {
      delete [] m_pstr;
   }
   
   int GetLength() const
   {
      return ::lstrlen(m_pstr);
   }
   
   CWindowText& operator=(HWND hWnd)
   {
      Assign(hWnd);
      return *this;
   }
   
   CWindowText& operator=(const CWindowText& src)
   {
      Assign(src.m_pstr);
      return *this;
   }
   
   operator LPCTSTR() const 
   { 
      return m_pstr; 
   }
   
   int Assign(HWND hWnd)
   {
      ATLASSERT(::IsWindow(hWnd));
      if( m_pstr != NULL ) delete [] m_pstr;
      int len = ::GetWindowTextLength(hWnd) + 1;
      ATLTRY( m_pstr = new TCHAR[len] );
      ATLASSERT(m_pstr);
      if( m_pstr == NULL ) return 0;
      return ::GetWindowText(hWnd, m_pstr, len);
   }

   int Assign(LPCTSTR pstrSrc)
   {
      ATLASSERT(!::IsBadStringPtr(pstrSrc, -1));
      if( m_pstr != NULL ) delete [] m_pstr;
      int len = ::lstrlen(pstrSrc);
      ATLTRY( m_pstr = new TCHAR[len+1] );
      ATLASSERT(m_pstr);
      if( m_pstr == NULL ) return 0;
      ::lstrcpy(m_pstr, pstrSrc);
      return len;
   }
};


/////////////////////////////////////////////////////////////////////////////
// CLockWindowUpdate

class CLockWindowUpdate
{
public:
   CLockWindowUpdate(HWND hWnd)
   {
      // NOTE: A locked window cannot be moved.
      //       See also Q270624 for problems with layered windows.
      ATLASSERT(::IsWindow(hWnd));
      ::LockWindowUpdate(hWnd);
   }

   ~CLockWindowUpdate()
   {
      ::LockWindowUpdate(NULL);
   }
};


/////////////////////////////////////////////////////////////////////////////
// CWindowRedraw

class CWindowRedraw
{
public:
   HWND m_hWnd;

   CWindowRedraw(HWND hWnd) : m_hWnd(hWnd)
   {
      ATLASSERT(::IsWindow(hWnd));
      // NOTE: See Q130611 for using this with a TreeView control.
      ::SendMessage(m_hWnd, WM_SETREDRAW, (WPARAM) FALSE, 0);
   }
   
   ~CWindowRedraw()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ::SendMessage(m_hWnd, WM_SETREDRAW, (WPARAM) TRUE, 0);
      ::InvalidateRect(m_hWnd, NULL, TRUE);
   }
};


/////////////////////////////////////////////////////////////////////////////
// CLoadLibrary

class CLoadLibrary
{
public:
   HINSTANCE m_hInst;

   CLoadLibrary(HINSTANCE hInst = NULL) : m_hInst(hInst)
   {
   }

   CLoadLibrary(LPCTSTR pstrFileName) : m_hInst(NULL)
   {
      
      Load(pstrFileName);
   }
   
   ~CLoadLibrary()
   {
      Free();
   }
   
   BOOL Load(LPCTSTR pstrFileName, DWORD dwFlags = 0)
   {
      ATLASSERT(!::IsBadStringPtr(pstrFileName, MAX_PATH));
      Free();
      m_hInst = ::LoadLibraryEx(pstrFileName, NULL, dwFlags);
      return m_hInst != NULL;
   }
   
   void Free()
   {
      if( IsLoaded() ) {
         ::FreeLibrary(m_hInst);
         m_hInst = NULL;
      }
   }
   
   HINSTANCE Detach()
   {
      HINSTANCE hInst = m_hInst;
      m_hInst = NULL;
      return hInst;
   }
   
   BOOL IsLoaded() const 
   { 
      return m_hInst != NULL; 
   }
   
   FARPROC GetProcAddress(LPCSTR pszFuncName) const
   { 
      ATLASSERT(!::IsBadStringPtrA(pszFuncName,-1));
      ATLASSERT(IsLoaded()); 
      return ::GetProcAddress(m_hInst, pszFuncName);
   }
   
   BOOL GetFileName(LPTSTR pstrFilename, DWORD cchMax = MAX_PATH) const
   {
      ATLASSERT(IsLoaded());
      return ::GetModuleFileName(m_hInst, pstrFilename, cchMax);
   }
   
   operator HINSTANCE() const
   { 
      return m_hInst; 
   }
};



/////////////////////////////////////////////////////////////////////////////
// CDeferWindowPos

// Wraps the DeferWindowPos() Windows API
class CDeferWindowPos
{
public:
  HDWP m_hdwp; 

  CDeferWindowPos(int nWindows = 2)
  {
    m_hdwp = ::BeginDeferWindowPos(nWindows); 
    ATLASSERT(m_hdwp);
  }
  
  ~CDeferWindowPos()
  {
    ::EndDeferWindowPos(m_hdwp);       
  }
  
  void DeferWindowPos(HWND hWnd, RECT &rc, DWORD nFlags = 0)
  {
    ATLASSERT(m_hdwp);
    ATLASSERT(::IsWindow(hWnd));
    bool bIsParent = ::GetWindow(hWnd, GW_CHILD) != NULL;
    m_hdwp = ::DeferWindowPos(m_hdwp,
      hWnd,
      bIsParent ? HWND_TOP : 0,
      rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
      nFlags | (bIsParent ? SWP_NOZORDER : 0) );
  }
  
  void DeferWindowPos(HWND hWnd, int rcLeft, int rcTop, int rcWidth, int rcHeight, DWORD nFlags = 0)
  {
    ATLASSERT(m_hdwp);
    ATLASSERT(::IsWindow(hWnd));
    bool bIsParent = ::GetWindow(hWnd, GW_CHILD) != NULL;
    m_hdwp = ::DeferWindowPos(m_hdwp,
      hWnd,
      bIsParent ? HWND_TOP : 0,
      rcLeft, rcTop, rcWidth, rcHeight,
      nFlags | (bIsParent ? SWP_NOZORDER : 0) );
  }
};


/////////////////////////////////////////////////////////////////////////////
// RECT derives

#if (defined(__ATLMISC_H__) && !defined(_WTL_NO_WTYPES)) || ( defined (__ATLTYPES_H__) )

class CClientRect : public CRect
{
public:
   CClientRect(HWND hWnd)
   {
      ATLASSERT(::IsWindow(hWnd));
      ::GetClientRect(hWnd, this);
   }
};

class CWindowRect : public CRect
{
public:
   CWindowRect(HWND hWnd)
   {
      ATLASSERT(::IsWindow(hWnd));
      ::GetWindowRect(hWnd, this);
   }
};

#else // __ATLMISC_H__ _WTL_NO_WTYPES

class CClientRect : public tagRECT
{
public:
   CClientRect(HWND hWnd)
   {
      ATLASSERT(::IsWindow(hWnd));
      ::GetClientRect(hWnd, this);
   }
};

class CWindowRect : public tagRECT
{
public:
   CWindowRect(HWND hWnd)
   {
      ATLASSERT(::IsWindow(hWnd));
      ::GetWindowRect(hWnd, this);
   }
};

#endif // _WTL_NO_WTYPES


/////////////////////////////////////////////////////////////////////////////
// CWindowPlacement

#ifndef _WTL_NO_WTYPES

class CWindowPlacement : public WINDOWPLACEMENT
{
public:
   CWindowPlacement()
   {
      ::ZeroMemory( (WINDOWPLACEMENT*) this, sizeof(WINDOWPLACEMENT) );
      length = sizeof(WINDOWPLACEMENT);
      showCmd = SW_NORMAL;
   }

   CWindowPlacement(HWND hWnd)
   {
      length = sizeof(WINDOWPLACEMENT);
      GetPosData(hWnd);
   }
   
   BOOL GetPosData(HWND hWnd)
   {
      ATLASSERT(::IsWindow(hWnd));
      return ::GetWindowPlacement(hWnd, this);
   }
   
   BOOL GetPosData(LPCTSTR pstr)
   {
      ATLASSERT(!::IsBadStringPtr(pstr, -1));
      if( ::lstrlen(pstr) == 0 ) return FALSE;
      flags = _GetInt(pstr);
      showCmd = _GetInt(pstr);
      ptMinPosition.x = _GetInt(pstr);
      ptMinPosition.y = _GetInt(pstr);
      ptMaxPosition.x = _GetInt(pstr);
      ptMaxPosition.y = _GetInt(pstr);
      rcNormalPosition.left = _GetInt(pstr);
      rcNormalPosition.top = _GetInt(pstr);
      rcNormalPosition.right = _GetInt(pstr);
      rcNormalPosition.bottom = _GetInt(pstr);
      return TRUE;
   }
   
   BOOL GetPosData(HKEY hReg, LPCTSTR pstrKeyName)
   {
      ATLASSERT(hReg);
      ATLASSERT(!::IsBadStringPtr(pstrKeyName,-1));
      TCHAR szData[80] = { 0 };
      DWORD dwCount = sizeof(szData) / sizeof(TCHAR);
      DWORD dwType = NULL;
      LONG lRes = ::RegQueryValueEx(hReg, pstrKeyName, NULL, &dwType, (LPBYTE)szData, &dwCount);
      if( lRes != ERROR_SUCCESS ) return FALSE;
      return GetPosData(szData);
   }
   
   BOOL SetPosData(HWND hWnd)
   {
      // NOTE: Do not place this call in the window's own WM_CREATE handler.
      //       Remember to call ShowWindow() if this method fails!
      ATLASSERT(::IsWindow(hWnd));
      ATLASSERT(!::IsRectEmpty(&rcNormalPosition)); // Initialize structures first
      // Make sure it is not out-of-bounds
      RECT rcTemp = { 0 };
      RECT rcScreen = { 0, 0, ::GetSystemMetrics(SM_CXSCREEN), ::GetSystemMetrics(SM_CYSCREEN) };
      if( ::IsRectEmpty(&rcNormalPosition) ) return FALSE;
      if( !::IntersectRect(&rcTemp, &rcNormalPosition, &rcScreen) ) return FALSE;
      // Show it...
      return ::SetWindowPlacement(hWnd, this);
   }

   BOOL SetPosData(LPTSTR pstr, UINT cchMax) const
   {
      ATLASSERT(!::IsBadWritePtr(pstr, cchMax));
      cchMax; // BUG: We don't validate on this
      ::wsprintf(pstr, _T("%ld %ld (%ld,%ld) (%ld,%ld) (%ld,%ld,%ld,%ld)"),
         flags,
         showCmd,
         ptMinPosition.x,
         ptMinPosition.y,
         ptMaxPosition.x,
         ptMaxPosition.y,
         rcNormalPosition.left,
         rcNormalPosition.top,
         rcNormalPosition.right,
         rcNormalPosition.bottom);
      return TRUE;
   }
   
   BOOL SetPosData(HKEY hReg, LPCTSTR pstrValueName) const
   {
      ATLASSERT(!::IsBadStringPtr(pstrValueName,-1));
      ATLASSERT(hReg);
      TCHAR szData[80];
      if( !SetPosData(szData, (UINT) sizeof(szData)/sizeof(TCHAR)) ) return FALSE;
      return ::RegSetValueEx(hReg, pstrValueName, NULL, REG_SZ, (CONST BYTE*) szData, (::lstrlen(szData)+1)*sizeof(TCHAR)) == ERROR_SUCCESS;
   }

   // Implementation
   
   long _GetInt(LPCTSTR& pstr) const
   {
      // NOTE: 'pstr' argument is "byref"
      bool fNeg = false;
      if( *pstr == '-' ) {
         fNeg = true;
         pstr = ::CharNext(pstr);
      }
      long n = 0;
      while( *pstr >= '0' && *pstr <= '9' ) {
         n = (n * 10L) + (*pstr - '0');
         pstr = ::CharNext(pstr);
      }
      while( *pstr >= ' ' && *pstr < '0' && *pstr != '-' ) pstr = ::CharNext(pstr);
      return fNeg ? -n : n;
   }
};

#endif // _WTL_NO_WTYPES


/////////////////////////////////////////////////////////////////////////////
// CWinProp

class CWinProp
{
public:
   HWND m_hWnd;

   CWinProp(HWND hWnd = NULL) : m_hWnd(hWnd)
   {
   }

   CWinProp& operator=(HWND hWnd)
   {
      m_hWnd = hWnd;
      return *this;
   }
   
   BOOL SetProperty(LPCTSTR pstrName, long lValue)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      return ::SetProp(m_hWnd, pstrName, (HANDLE) lValue);
   }
   
   BOOL SetProperty(LPCTSTR pstrName, LPCVOID pValue)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      return ::SetProp(m_hWnd, pstrName, (HANDLE) pValue);
   }
   
   void GetProperty(LPCTSTR pstrName, long& lValue) const
   {
      ATLASSERT(::IsWindow(m_hWnd));
      lValue = (long) ::GetProp(m_hWnd, pstrName);
   }
   
   void GetProperty(LPCTSTR pstrName, LPVOID& pValue) const
   {
      ATLASSERT(::IsWindow(m_hWnd));
      pValue = (LPVOID) ::GetProp(m_hWnd, pstrName);
   }
   
   BOOL Enumerate(PROPENUMPROCEX Proc, LPARAM lParam) const
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ATLASSERT(!::IsBadCodePtr((FARPROC)Proc));
      return ::EnumPropsEx(m_hWnd, Proc, lParam) != -1;
   }  
   
   typedef struct PROPFIND { LPCTSTR pstrName; BOOL bFound; };
   
   BOOL FindProperty(LPCTSTR pstrName) const
   {
      ATLASSERT(::IsWindow(m_hWnd));
      PROPFIND pf = { pstrName, FALSE };
      ::EnumPropsEx(m_hWnd, _FindProc, (LPARAM) &pf); 
      return pf.bFound;
   }
   
   void RemoveProperty(LPCTSTR pstrName)
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ::RemoveProp(m_hWnd, pstrName);
   }
   
   void RemoveAll()
   {
      ATLASSERT(::IsWindow(m_hWnd));
      ::EnumPropsEx(m_hWnd, _RemoveAllProc, 0); 
   }

   // Static members
   
   static BOOL CALLBACK _FindProc(HWND /*hWnd*/, LPTSTR pstrName, HANDLE /*hData*/, ULONG_PTR lParam)
   {
      PROPFIND* pf = (PROPFIND*) lParam;
      if( (HIWORD(pstrName) == 0 && pstrName == pf->pstrName) 
          || (HIWORD(pstrName) != 0 && ::lstrcmp(pstrName, pf->pstrName) == 0) ) 
      {
         pf->bFound = TRUE;
         return FALSE;
      }
      return TRUE;
   }
   
   static BOOL CALLBACK _RemoveAllProc(HWND hWnd, LPTSTR pstrName, HANDLE /*hData*/, ULONG_PTR /*lParam*/)
   {
      ::RemoveProp(hWnd, pstrName);
      return TRUE;
   }
};


/////////////////////////////////////////////////////////////////////////////
// CIniFile

class CIniFile
{
public:
   TCHAR m_szFilename[MAX_PATH];

   CIniFile()
   {
      m_szFilename[0] = '\0';
   }

   void SetFilename(LPCTSTR pstrFilename)
   {
      ATLASSERT(!::IsBadStringPtr(pstrFilename,-1));
      ::lstrcpy(m_szFilename, pstrFilename);
   }
   
   BOOL GetString(LPCTSTR pstrSection, LPCTSTR pstrKey, LPTSTR pstrValue, UINT cchMax, LPCTSTR pstrDefault = NULL) const
   {
      ATLASSERT(m_szFilename[0]);
      ATLASSERT(!::IsBadStringPtr(pstrSection,-1));
      ATLASSERT(!::IsBadStringPtr(pstrKey,-1));
      ATLASSERT(!::IsBadWritePtr(pstrValue,cchMax));
      return ::GetPrivateProfileString(pstrSection, pstrKey, pstrDefault, pstrValue, cchMax,  m_szFilename) > 0;
   }

#if defined(__ATLSTR_H__) || defined(_WTL_USE_CSTRING)

   BOOL GetString(LPCTSTR pstrSection, LPCTSTR pstrKey, CString& sValue, LPCTSTR pstrDefault = NULL) const
   {      
      enum { MAX_INIVALUE_LEN = 1024 };
      BOOL bRes = GetString(pstrSection, pstrKey, sValue.GetBuffer(MAX_INIVALUE_LEN), MAX_INIVALUE_LEN, pstrDefault);
      sValue.ReleaseBuffer(bRes ? -1 : 0);
      return bRes;
   }

#ifdef _UNICODE

   void CreateUnicodeStub() const
   {
      ATLASSERT(m_szFilename[0]);
      if( ::GetFileAttributes(m_szFilename) != (DWORD) -1 ) return;
      HANDLE hFile = ::CreateFile(m_szFilename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
      if( hFile == NULL ) return;
      WORD wBOM = 0xFEFF;
      DWORD dwWritten = 0;
      ::WriteFile(hFile, &wBOM, sizeof(wBOM), &dwWritten, NULL);
      ::CloseHandle(hFile);
   }

#endif // _UNICODE

#endif // __ATLSTR_H__ _WTL_USE_CSTRING

   BOOL GetInt(LPCTSTR pstrSection, LPCTSTR pstrKey, int& iValue, int iDefault = 0) const
   {
      ATLASSERT(m_szFilename[0]);
      ATLASSERT(!::IsBadStringPtr(pstrSection,-1));
      ATLASSERT(!::IsBadStringPtr(pstrKey,-1));
      iValue = ::GetPrivateProfileInt(pstrSection, pstrKey, iDefault, m_szFilename);
      return TRUE;
   }
   
   BOOL GetBool(LPCTSTR pstrSection, LPCTSTR pstrKey, bool& bValue, bool bDefault = true) const
   {
      TCHAR szValue[2] = { 0 };
      GetString(pstrSection, pstrKey, szValue, sizeof(szValue) / sizeof(TCHAR), bDefault ? _T("Y") : _T("N"));
      switch( szValue[0] ) {
      case 'y': // Yes
      case 'Y':
      case 't': // True
      case 'T':
      case '1': // 1
         bValue = true;
         break;
      case 'n': // No
      case 'N':
      case 'f': // False
      case 'F':
      case '0': // 0
         bValue = false;
         break;
      default:
         bValue = bDefault;
      }
      return TRUE;
   }
   
   BOOL PutString(LPCTSTR pstrSection, LPCTSTR pstrKey, LPCTSTR pstrValue)
   {
      ATLASSERT(m_szFilename[0]);
      ATLASSERT(!::IsBadStringPtr(pstrSection,-1));
      ATLASSERT(!::IsBadStringPtr(pstrKey,-1));
      return ::WritePrivateProfileString(pstrSection, pstrKey, pstrValue, m_szFilename);
   }
   
   BOOL PutInt(LPCTSTR pstrSection, LPCTSTR pstrKey, int iValue)
   {
      TCHAR szValue[32] = { 0 };
      ::wsprintf(szValue, _T("%d"), iValue);
      return PutString(pstrSection, pstrKey, szValue);
   }
   
   BOOL PutBool(LPCTSTR pstrSection, LPCTSTR pstrKey, bool bValue)
   {
      TCHAR szValue[2] = { 0 };
      szValue[0] = bValue ? 'Y' : 'N'; // TODO: What about localization? Use 0/1?
      return PutString(pstrSection, pstrKey, szValue);
   }

   void DeleteKey(LPCTSTR pstrSection, LPCTSTR pstrKey)
   {
      ATLASSERT(m_szFilename[0]);
      ATLASSERT(!::IsBadStringPtr(pstrSection,-1));
      ATLASSERT(!::IsBadStringPtr(pstrKey,-1));
      ::WritePrivateProfileString(pstrSection, pstrKey, NULL, m_szFilename);
   }

   void DeleteSection(LPCTSTR pstrSection)
   {
      ATLASSERT(m_szFilename[0]);
      ATLASSERT(!::IsBadStringPtr(pstrSection,-1));
      ::WritePrivateProfileString(pstrSection, NULL, NULL, m_szFilename);
   }
   
   void Flush()
   {
      ATLASSERT(m_szFilename[0]);
      ::WritePrivateProfileString(NULL, NULL, NULL, m_szFilename);
   }
};


/////////////////////////////////////////////////////////////////////////////
// Misc helper methods

inline bool AtlIsEditControl(HWND hWnd)
{
   return (::SendMessage(hWnd, WM_GETDLGCODE, 0, 0L) & DLGC_HASSETSEL) != 0;
}

#if defined(__ATLSTR_H__) || defined(_WTL_USE_CSTRING)

// Load HTML from resource (resource type RT_HTML only).
// Thanks to Anatoly Ivasyuk for fixing a sz-string problem.
inline CString AtlLoadHtmlResource(_U_STRINGorID html)
{
#if (_ATL_VER >= 0x0700)
   HRSRC hrsrc = ::FindResource(ATL::_AtlBaseModule.GetResourceInstance(), html.m_lpstr, RT_HTML);
   if( hrsrc != NULL ) {
      DWORD dwSize = ::SizeofResource(ATL::_AtlBaseModule.GetResourceInstance(), hrsrc);
      HGLOBAL hglb = ::LoadResource(ATL::_AtlBaseModule.GetResourceInstance(), hrsrc);
#else
   HRSRC hrsrc = ::FindResource(_Module.GetResourceInstance(), html.m_lpstr, RT_HTML);
   if( hrsrc != NULL ) {
      DWORD dwSize = ::SizeofResource(_Module.GetResourceInstance(), hrsrc);
      HGLOBAL hglb = ::LoadResource(_Module.GetResourceInstance(), hrsrc);
#endif // _ATL_VER
      if( hglb != NULL ) {
         LPCSTR pData = (LPCSTR) ::LockResource(hglb);
         if( pData != NULL ) {
            // Assumes HTML is in ANSI/UTF-8
            CString s;
#ifndef _UNICODE
            ::CopyMemory(s.GetBufferSetLength(dwSize), pData, dwSize);
#else
            AtlA2WHelper(s.GetBufferSetLength(dwSize), pData, dwSize);
#endif // _UNICODE
            s.ReleaseBuffer(dwSize);
            UnlockResource(hglb);
            ::FreeResource(hglb);
            return s;
         }
      }
   }
   return CString();
}

#endif // __ATLSTR_H__ _WTL_USE_CSTRING

#ifndef _ATL_DLL_IMPL
}; // namespace WTL
#endif // _ATL_DLL_IMPL


#endif // __ATLWINMISC_H__
