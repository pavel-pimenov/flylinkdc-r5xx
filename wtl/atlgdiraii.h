//
// Windows Template Library Extension for safe
// GDI objects selection and attribute setting
//
// Written by Pete Kvitek <pete@kvitek.com>
//
// This file is NOT part of Windows Template Library.
// The code and information is provided "as-is" without
// warranty of any kind, either expressed or implied.
//

#ifndef __ATLGDIRAII_H__
#define __ATLGDIRAII_H__

#pragma once

#ifndef __cplusplus
	#error ATL requires C++ compilation (use a .cpp suffix)
#endif

#ifndef __ATLAPP_H__
	#error atlgdiraii.h requires atlapp.h to be included first
#endif

#ifndef __ATLGDI_H__
	#error atlgdiraii.h requires atlgdi.h to be included first
#endif

/////////////////////////////////////////////////////////////////////////////
// Classes in this file:
//
// CSelectPen
// CSelectBrush
// CSelectFont
// CSelectBitmap
// CSelectPalette
// CSetBkMode
// CSetBkColor
// CSetTextColor
// CSetPolyFillMode
// CSetROP2
// CSetStretchBltMode
// CSetMapMode
// CSetArcDirection
// CSetTextCharacterExtra
// CSetBrushOrg
// CSaveRestoreDC
//

namespace WTL
{

///////////////////////////////////////////////////////////////////////////////
// CSelectPen class -- safe GDI pen selector

class CSelectPen : public CDCHandle
{
public:
	
	// Attributes

	HPEN m_oldPen;

	// Construction/destruction

	CSelectPen(HDC hdc, HPEN pen = NULL)
	:	CDCHandle(hdc)
	{
		m_oldPen = pen ? SelectPen(pen) : NULL;
	}

	CSelectPen(HDC hdc, int nPen)
	:	CDCHandle(hdc)
	{
		m_oldPen = SelectStockPen(nPen);
	}

	~CSelectPen()
	{
		if (m_oldPen) SelectPen(m_oldPen);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSelectBrush class -- safe GDI brush selector

class CSelectBrush : public CDCHandle
{
public:
	
	// Attributes

	HBRUSH m_oldBrush;

	// Construction/destruction

	CSelectBrush(HDC hdc, HBRUSH brush = NULL)
	:	CDCHandle(hdc)
	{
		m_oldBrush = brush ? SelectBrush(brush) : NULL;
	}

	CSelectBrush(HDC hdc, int nBrush)
	:	CDCHandle(hdc)
	{
		m_oldBrush = SelectStockBrush(nBrush);
	}

	~CSelectBrush()
	{
		if (m_oldBrush) SelectBrush(m_oldBrush);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSelectFont class -- safe GDI FONT selector

class CSelectFont : public CDCHandle
{
public:
	
	// Attributes

	HFONT m_oldFont;

	// Construction/destruction

	CSelectFont(HDC hdc, HFONT font = NULL)
	:	CDCHandle(hdc)
	{
		m_oldFont = font ? SelectFont(font) : NULL;
	}

	CSelectFont(HDC hdc, int nFont)
	:	CDCHandle(hdc)
	{
		m_oldFont = SelectStockFont(nFont);
	}

	~CSelectFont()
	{
		if (m_oldFont) SelectFont(m_oldFont);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSelectBitmap class -- safe GDI bitmap selector

class CSelectBitmap : public CDCHandle
{
public:
	
	// Attributes

	HBITMAP m_oldBitmap;

	// Construction/destruction

	CSelectBitmap(HDC hdc, HBITMAP bitmap = NULL)
	:	CDCHandle(hdc)
	{
		m_oldBitmap = bitmap ? SelectBitmap(bitmap) : NULL;
	}

	~CSelectBitmap()
	{
		if (m_oldBitmap) SelectBitmap(m_oldBitmap);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSelectPalette class -- safe GDI palette selector

class CSelectPalette : public CDCHandle
{
public:
	
	// Attributes

	HPALETTE m_oldPalette;

	bool m_bForceBackground;

	// Construction/destruction

	CSelectPalette(HDC hdc, HPALETTE palette = NULL, bool bForceBackground = false)
	:	CDCHandle(hdc)
	,	m_bForceBackground(bForceBackground)
	{
		m_oldPalette = palette ? SelectPalette(palette, m_bForceBackground) : NULL;
	}

	CSelectPalette(HDC hdc, int nPalette, bool bForceBackground = false)
	:	CDCHandle(hdc)
	,	m_bForceBackground(bForceBackground)
	{
		m_oldPalette = SelectStockPalette(nPalette, m_bForceBackground);
	}

	~CSelectPalette()
	{
		if (m_oldPalette) SelectPalette(m_oldPalette, m_bForceBackground);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSetBkMode class -- safe GDI background mix mode setting

class CSetBkMode : public CDCHandle
{
public:
	
	// Attributes

	int m_oldBkMode;

	// Construction/destruction

	CSetBkMode(HDC hdc, int nBkMode)
	:	CDCHandle(hdc)
	{
		m_oldBkMode = SetBkMode(nBkMode);
	}

	~CSetBkMode()
	{
		if (m_oldBkMode) SetBkMode(m_oldBkMode);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSetBkColor class -- safe GDI background color setting

class CSetBkColor : public CDCHandle
{
public:
	
	// Attributes

	COLORREF m_oldBkColor;

	// Construction/destruction

	CSetBkColor(HDC hdc, COLORREF clr)
	:	CDCHandle(hdc)
	{
		m_oldBkColor = SetBkColor(clr);
	}

	~CSetBkColor()
	{
		if (m_oldBkColor != CLR_INVALID) SetBkColor(m_oldBkColor);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSetTextColor class -- safe GDI text color setting

class CSetTextColor : public CDCHandle
{
public:
	
	// Attributes

	COLORREF m_oldTextColor;

	// Construction/destruction

	CSetTextColor(HDC hdc, COLORREF clr)
	:	CDCHandle(hdc)
	{
		m_oldTextColor = SetTextColor(clr);
	}

	~CSetTextColor()
	{
		if (m_oldTextColor != CLR_INVALID) SetTextColor(m_oldTextColor);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSetPolyFillMode class -- safe GDI polygon filling mode setting

#ifndef _WIN32_WCE

class CSetPolyFillMode : public CDCHandle
{
public:
	
	// Attributes

	int m_oldPolyFillMode;

	// Construction/destruction

	CSetPolyFillMode(HDC hdc, int nPolyFillMode)
	:	CDCHandle(hdc)
	{
		m_oldPolyFillMode = SetPolyFillMode(nPolyFillMode);
	}

	~CSetPolyFillMode()
	{
		if (m_oldPolyFillMode) SetPolyFillMode(m_oldPolyFillMode);
	}

};

#endif // !_WIN32_WCE

///////////////////////////////////////////////////////////////////////////////
// CSetROP2 class -- safe GDI foreground mix mode setting

class CSetROP2 : public CDCHandle
{
public:
	
	// Attributes

	int m_oldROP2;

	// Construction/destruction

	CSetROP2(HDC hdc, int nROP2)
	:	CDCHandle(hdc)
	{
		m_oldROP2 = SetROP2(nROP2);
	}

	~CSetROP2()
	{
		if (m_oldROP2) SetROP2(m_oldROP2);
	}

};

///////////////////////////////////////////////////////////////////////////////
// CSetStretchBltMode class -- safe GDI bitmap stretching mode setting

#ifndef _WIN32_WCE

class CSetStretchBltMode : public CDCHandle
{
public:
	
	// Attributes

	int m_oldStretchBltMode;

	// Construction/destruction

	CSetStretchBltMode(HDC hdc, int nStretchBltMode)
	:	CDCHandle(hdc)
	{
		m_oldStretchBltMode = SetStretchBltMode(nStretchBltMode);
	}

	~CSetStretchBltMode()
	{
		if (m_oldStretchBltMode) SetStretchBltMode(m_oldStretchBltMode);
	}

};

#endif // !_WIN32_WCE

///////////////////////////////////////////////////////////////////////////////
// CSetMapMode class -- safe GDI mapping mode mode setting

#ifndef _WIN32_WCE

class CSetMapMode : public CDCHandle
{
public:
	
	// Attributes

	int m_oldMapMode;

	// Construction/destruction

	CSetMapMode(HDC hdc, int nMapMode)
	:	CDCHandle(hdc)
	{
		m_oldMapMode = SetMapMode(nMapMode);
	}

	~CSetMapMode()
	{
		if (m_oldMapMode) SetMapMode(m_oldMapMode);
	}

};

#endif // !_WIN32_WCE

///////////////////////////////////////////////////////////////////////////////
// CSetArcDirection class -- safe GDI arc/rectangle drawing direction setting

#ifndef _WIN32_WCE

class CSetArcDirection : public CDCHandle
{
public:
	
	// Attributes

	int m_oldArcDirection;

	// Construction/destruction

	CSetArcDirection(HDC hdc, int nArcDirection)
	:	CDCHandle(hdc)
	{
		m_oldArcDirection = SetArcDirection(nArcDirection);
	}

	~CSetArcDirection()
	{
		if (m_oldArcDirection) SetArcDirection(m_oldArcDirection);
	}

};

#endif // !_WIN32_WCE

///////////////////////////////////////////////////////////////////////////////
// CSetTextCharacterExtra class -- safe GDI intercharacter spacing setting

#ifndef _WIN32_WCE

class CSetTextCharacterExtra : public CDCHandle
{
public:
	
	// Attributes

	int m_oldTextCharacterExtra;

	// Construction/destruction

	CSetTextCharacterExtra(HDC hdc, int nTextCharacterExtra)
	:	CDCHandle(hdc)
	{
		m_oldTextCharacterExtra = SetTextCharacterExtra(nTextCharacterExtra);
	}

	~CSetTextCharacterExtra()
	{
		if (m_oldTextCharacterExtra != 0x80000000) SetTextCharacterExtra(m_oldTextCharacterExtra);
	}

};

#endif // !_WIN32_WCE

///////////////////////////////////////////////////////////////////////////////
// CSetBrushOrg class -- safe GDI brush origin setting

#ifndef _WIN32_WCE

class CSetBrushOrg : public CDCHandle
{
public:
	
	// Attributes

	CPoint m_ptOldBrushOrigin;

	// Construction/destruction

	CSetBrushOrg(HDC hdc, int x, int y)
	:	CDCHandle(hdc)
	{
		SetBrushOrg(x, y, &m_ptOldBrushOrigin);
	}

	CSetBrushOrg(HDC hdc, const CPoint& pt)
	:	CDCHandle(hdc)
	{
		SetBrushOrg(pt.x, pt.y, &m_ptOldBrushOrigin);
	}

	~CSetBrushOrg()
	{
		SetBrushOrg(m_ptOldBrushOrigin.x, m_ptOldBrushOrigin.y);
	}

};

#endif // !_WIN32_WCE

///////////////////////////////////////////////////////////////////////////////
// CSaveRestoreDC class -- safe GDI state save/restore

class CSaveRestoreDC : public CDCHandle
{
public:
	
	// Attributes

	int m_nSavedDC;

	// Construction/destruction

	CSaveRestoreDC(HDC hdc)
	:	CDCHandle(hdc)
	{
		m_nSavedDC = SaveDC();
	}

	~CSaveRestoreDC()
	{
		if (m_nSavedDC) RestoreDC(m_nSavedDC);
	}

};

}; //namespace WTL

#endif // __ATLGDIRAII_H__
