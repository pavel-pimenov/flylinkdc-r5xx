#include "stdafx.h"
#include "bitmap.h"

CBmp::CBmp(HBITMAP hBMP):
	m_hDC(NULL), m_hBMP(hBMP)
{
}

HBITMAP CBmp::CreateCompatibleBitmap(HDC hDC, int w, int h)
{

	bool bNeedFree;
	
	if (hDC == NULL)
	{
		bNeedFree = true;
		hDC = GetDC(NULL);
	}
	else
		bNeedFree = false;
		
	m_hBMP = ::CreateCompatibleBitmap(hDC, w, h);
	_ASSERTE(m_hBMP);
	
	if (bNeedFree)
	{
		int l_res = ReleaseDC(NULL, hDC);
		dcassert(l_res);
	}
	
	return m_hBMP;
}

HBITMAP CBmp::LoadBitmap(LPCWSTR lpszName)
{
	_ASSERTE(m_hDC == NULL);
	
#ifdef WINCE
	m_hBMP = SHLoadDIBitmap(lpszName);
#else
	m_hBMP = (HBITMAP)LoadImage(NULL, lpszName, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
#endif
	_ASSERTE(m_hBMP);
	
	return m_hBMP;
}

void CBmp::Unload()
{
	if (m_hDC)
	{
		SelectObject(m_hDC, m_hBMPorg);
		DeleteDC(m_hDC);
		m_hDC = NULL;
	}
	if (m_hBMP)
	{
		DeleteObject(m_hBMP);
		m_hBMP = NULL;
	}
}

CBmp::~CBmp()
{
	Unload();
}
void CBmp::Attach(HBITMAP hBMP)
{
	_ASSERTE(m_hBMP == NULL);
	m_hBMP = hBMP;
}


DWORD CBmp::GetDimension(unsigned char *iBits)
{
	if (!m_hBMP)
		return 0;
		
	BITMAP bm;
	GetObject(m_hBMP, sizeof(BITMAP), &bm);
	
	if (iBits)
		*iBits = (char)bm.bmBitsPixel;
		
	return MAKELONG(bm.bmWidth, bm.bmHeight);
}

CBmp::operator HDC()
{
	_ASSERTE(m_hBMP);
	
	if (!m_hDC)
	{
		HDC hDC = GetDC(NULL);
		m_hDC = CreateCompatibleDC(hDC);
		int l_res = ReleaseDC(NULL, hDC);
		dcassert(l_res);
		_ASSERTE(m_hDC);
		
		m_hBMPorg = (HBITMAP)SelectObject(m_hDC, m_hBMP);
	}
	else
		SelectObject(m_hDC, m_hBMP);
		
	return m_hDC;
}

void CBmp::FreeBMPfromDC()
{
	SelectObject(m_hDC, m_hBMPorg);
}

void CBmp::Detach()
{
	FreeBMPfromDC();
	m_hBMP = NULL;
}