#include "stdafx.h"

#ifdef RIP_USE_SKIN

#include "SkinableCmdBar.h"
#include "Bitmap.h"

#define DUPLICATE_TO_SCREEN

CSkinableCmdBar::CSkinableCmdBar():
	m_dwHilitedItemInd(0xffffffff),
	m_dwPushingItemInd(0xffffffff),
	m_bHilitedArrowInd(0xff),
	m_bPushingArrowInd(0xff),
	m_bUseArrows(false),
	m_iOffset(0),
	m_wItemW(0)
{
}

LRESULT CALLBACK CSkinableCmdBar::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
			/*
			case WM_DESTROY:
			    PostQuitMessage(0);
			break;
			*/
			
		case WM_MOUSEMOVE:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
				pCmdBar->OnMouseMove(LOWORD(lParam), HIWORD(lParam), wParam);
		}
		break;
		
		case WM_LBUTTONUP:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
				pCmdBar->OnMouseUp(LOWORD(lParam), HIWORD(lParam), wParam);
		}
		break;
		
		case WM_LBUTTONDOWN:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
				pCmdBar->OnMouseDown(LOWORD(lParam), HIWORD(lParam), wParam);
		}
		break;
		
		case WM_SIZE:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
				pCmdBar->OnSize(LOWORD(lParam), HIWORD(lParam), wParam);
		}
		break;
		
		case WM_GETMINMAXINFO:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
				pCmdBar->OnGetMinMaxInfo((MINMAXINFO*)lParam);
		}
		break;
		
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
				pCmdBar->OnPaint(&ps);
			EndPaint(hWnd, &ps);
		}
		break;
		
		case WM_MOUSELEAVE:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
				pCmdBar->ClearHilited();
		}
		break;
		
		case WM_TIMER:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
				pCmdBar->OnTimer();
		}
		break;
		
		case WM_CREATE:
		{
			CREATESTRUCT *pCreate = (CREATESTRUCT*)lParam;
			SetWindowLongPtr(hWnd, 0, (LONG_PTR)pCreate->lpCreateParams);
		}
		break;
		
		case TB_ADDBUTTONS:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
			{
				TBBUTTON *pBtn = (TBBUTTON *)lParam;
				
				for (DWORD i = 0; i < wParam; i++)
				{
					if (pBtn->fsStyle != TBSTYLE_SEP)
						pCmdBar->AddItem(pBtn->iBitmap, pBtn->idCommand, (BYTE)(pBtn->fsStyle == TBSTYLE_CHECK ? BS_CHECKBOX : BS_PUSHBUTTON), pBtn->fsState, 0, false);
				}
			}
		}
		break;
		
		case TB_CHECKBUTTON:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
			{
				DWORD dwItem = pCmdBar->GetItemByCmd((int)wParam);
				
				if (dwItem != 0xffffffff)
					pCmdBar->SetItemState(dwItem, (BYTE)lParam, true);
			}
		}
		break;
		
		case WM_DESTROY:
		{
			CSkinableCmdBar *pCmdBar = (CSkinableCmdBar *)GetWindowLongPtr(hWnd, 0);
			if (pCmdBar)
			{
				pCmdBar->m_Wnd.OnDestroy();
			}
		}
		break;
		
		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}

CSkinableCmdBar::~CSkinableCmdBar()
{
	if ((HBITMAP)m_WorkImg && (HDC)m_WorkImg)
		RestoreDC(m_WorkImg, -1);
}

static void Premultiply(HDC hDC, HBITMAP hBMP, int w, int h)
{
	int iSize = w * h * 4;
	char *buff = new char[iSize];
	BITMAPINFO bi =
	{
		{
			sizeof(bi),
			w, h,
			1,
			32,
			BI_RGB,
			0,
			72,
			72,
			0,
			0
		},
	};
	GetDIBits(hDC, hBMP, 0, h, buff, &bi, DIB_RGB_COLORS);
	
	struct RGBA
	{
		BYTE r;
		BYTE g;
		BYTE b;
		BYTE a;
	};
	
	bool bHaveAlpha = false;
	
	for (int i = 0; i < iSize; i += 4)
	{
		RGBA *pRGBA = (RGBA *) & buff[i];
		if (pRGBA->a > 0)
		{
			bHaveAlpha = true;
			break;
		}
	}
	
	if (bHaveAlpha)
	{
		for (int i = 0; i < iSize; i += 4)
		{
			RGBA *pRGBA = (RGBA *) & buff[i];
			
			pRGBA->r = unsigned char(int(pRGBA->r) * int(pRGBA->a) / 0xff);
			pRGBA->g = unsigned char(int(pRGBA->g) * int(pRGBA->a) / 0xff);
			pRGBA->b = unsigned char(int(pRGBA->b) * int(pRGBA->a) / 0xff);
		}
	}
	else
	{
		for (int i = 0; i < iSize; i += 4)
		{
			RGBA *pRGBA = (RGBA *) & buff[i];
			pRGBA->a = 0xff;
		}
	}
	
	SetDIBits(hDC, hBMP, 0, h, buff, &bi, DIB_RGB_COLORS);
	
	delete[] buff;
}

bool CSkinableCmdBar::Create(HWND hParentWnd, int x, int y, int len, DefinedTypeFlags Type, HBITMAP hArrowsBMP, HBITMAP hTemplate, COLORREF clrBack, int iOverlap)
{
	bool bRet = false;
	
	m_dwWndLen = len;
	m_Type = Type;
	
	if (iOverlap < 0)
	{
		m_bOverlap = (BYTE) - iOverlap;
		m_bLeftOnTop = true;
	}
	else
	{
		m_bOverlap = (BYTE)iOverlap;
		m_bLeftOnTop = false;
	}
	
	if (hTemplate || hArrowsBMP)
	{
		HDC hDC = GetDC(NULL);
		
		if (hTemplate)
		{
			m_TemplateImg.Attach(hTemplate);
			DWORD dwDim = m_TemplateImg.GetDimension();
			Premultiply(hDC, hTemplate, LOWORD(dwDim), HIWORD(dwDim));
		}
		
		if (hArrowsBMP)
		{
			m_ArrowsImg.Attach(hArrowsBMP);
			DWORD dwDim = m_ArrowsImg.GetDimension();
			Premultiply(hDC, hArrowsBMP, LOWORD(dwDim), HIWORD(dwDim));
		}
		int l_res = ReleaseDC(NULL, hDC);
		dcassert(l_res);
	}
	
	m_clrBack = clrBack;
	
	DWORD w = m_dwWndLen;
	DWORD h = 0;
	MapCoord(w, h);
	bRet = m_Wnd.CreateWnd((HINSTANCE)GetWindowLongPtr(hParentWnd, GWLP_HINSTANCE), L"CmdBarWnd", NULL, x, y, w, h, WndProc, hParentWnd, (LPARAM)this, sizeof(this), WS_CHILD);
	
	ShowWindow(m_Wnd, SW_SHOW);
	
	return true;
}

bool CSkinableCmdBar::Create(HWND hParentWnd, int x, int y, int len, DefinedTypeFlags Type, size_t szItemCount,
                             HBITMAP hNormalBMP, HBITMAP hHilitedBMP, HBITMAP hSelectedBMP, HBITMAP hSelectedHilitedBMP, HBITMAP hArrowsBMP,
                             COLORREF clrBack, int iOverlap)
{
	bool bRet = false;
	
	if (szItemCount > 0)
	{
		m_dwWndLen = len;
		m_Type = Type;
		
		if (iOverlap < 0)
		{
			m_bOverlap = (BYTE) - iOverlap;
			m_bLeftOnTop = true;
		}
		else
		{
			m_bOverlap = (BYTE)iOverlap;
			m_bLeftOnTop = false;
		}
		
		bRet = InitializeSkin(szItemCount, hNormalBMP, hHilitedBMP, hSelectedBMP, hSelectedHilitedBMP, hArrowsBMP, clrBack);
		
		if (bRet)
		{
			WORD wItemW = m_wItemW;
			WORD wItemH = m_wItemH;
			MapCoord(wItemW, wItemH);
			DWORD w = m_dwWndLen;
			DWORD h = wItemH;
			MapCoord(w, h);
			bRet = m_Wnd.CreateWnd((HINSTANCE)GetWindowLongPtr(hParentWnd, GWLP_HINSTANCE), L"CmdBarWnd", NULL, x, y, w, h, WndProc, hParentWnd, (LPARAM)this, sizeof(this), WS_CHILD);
			
			ShowWindow(m_Wnd, SW_SHOW);
		}
	}
	
	return bRet;
}

bool CSkinableCmdBar::InitializeSkin(size_t szItemCount,
                                     HBITMAP hNormalBMP, HBITMAP hHilitedBMP, HBITMAP hSelectedBMP, HBITMAP hSelectedHilitedBMP, HBITMAP hArrowsBMP,
                                     COLORREF clrBack)
{
	m_NormalImg.Attach(hNormalBMP);
	m_HilitedImg.Attach(hHilitedBMP);
	m_SelectedImg.Attach(hSelectedBMP);
	m_SelectedHilitedImg.Attach(hSelectedHilitedBMP);
	
	if (hArrowsBMP)
		m_ArrowsImg.Attach(hArrowsBMP);
		
	if (hNormalBMP)
		m_wSkinItemCount = (WORD)szItemCount;
	else
		m_wSkinItemCount = 0;
		
		
	DWORD dwDim = m_NormalImg.GetDimension();
	WORD xDim = LOWORD(dwDim);
	WORD yDim = HIWORD(dwDim);
	
	dwDim = m_ArrowsImg.GetDimension();
	
	if (IsVert())
		m_wArrowLen = HIWORD(dwDim) / 6;
	else
		m_wArrowLen = LOWORD(dwDim) / 6;
		
	m_wItemW = WORD(xDim / szItemCount);
	m_wItemH = yDim;
	
	DWORD w;// = xDim;
	DWORD h;// = m_wItemH;
	//MapCoord(w, h);
	
	if (IsVert())
	{
		w = m_wItemW;
		h = m_wItemH * szItemCount;
	}
	else
	{
		w = xDim;
		h = m_wItemH;
	}
	
	HDC hDC = GetDC(NULL);
	
	if (hNormalBMP)
		Premultiply(hDC, hNormalBMP, xDim, yDim);
		
	if (hHilitedBMP)
		Premultiply(hDC, hHilitedBMP, xDim, yDim);
		
	if (hSelectedBMP)
		Premultiply(hDC, hSelectedBMP, xDim, yDim);
		
	if (hSelectedHilitedBMP)
		Premultiply(hDC, hSelectedHilitedBMP, xDim, yDim);
		
	if (hArrowsBMP)
		Premultiply(hDC, hArrowsBMP, LOWORD(dwDim), HIWORD(dwDim));
		
	HBITMAP hBMP = m_WorkImg.CreateCompatibleBitmap(hDC, w, h);
	int l_res = ReleaseDC(NULL, hDC);
	dcassert(l_res);
	
	if (!hBMP)
		return false;
		
	SaveDC(m_WorkImg);
	SelectObject(m_WorkImg, GetStockObject(DC_BRUSH));
	SetDCBrushColor(m_WorkImg, clrBack);
	
	return true;
}

DWORD CSkinableCmdBar::GetVisibleLen()
{
	return m_State.size() * ((IsVert() ? m_wItemH : m_wItemW) - m_bOverlap) + m_bOverlap;
}

void CSkinableCmdBar::Update()
{
	DWORD w = GetVisibleLen();
	DWORD h = m_wItemH;
	MapCoord(w, h);
	
	for (DWORD i = 0; i < m_State.size(); i++)
	{
		DrawItem(i, false, false);
	}
}

void CSkinableCmdBar::ClearHilited()
{
	if (m_dwHilitedItemInd != 0xffffffff)
	{
		DrawItem(m_dwHilitedItemInd, false, false);
		m_dwHilitedItemInd = 0xffffffff;
	}
	
	if (m_bHilitedArrowInd != 0xff)
	{
		DrawArrows(m_bHilitedArrowInd == 0, m_bHilitedArrowInd != 0, false, false, true);
		m_bHilitedArrowInd = 0xff;
	}
}

void CSkinableCmdBar::DrawItem(DWORD dwItem, bool bHilited, bool bForceSelected, bool bInternalCall)
{
	WORD wItemW = m_wItemW;
	WORD wItemH = m_wItemH;
	MapCoord(wItemW, wItemH);
	
	int x = dwItem * (wItemW - m_bOverlap) - m_iOffset;
	int y = 0;
	
	int xx = x, yy = y;
	//int ww = m_wItemW, hh = m_wItemH;
	
	MapCoord(xx, yy);
	
	if ((int)dwItem < 0 || dwItem >= m_State.size())
	{
		int ww = wItemW - m_bOverlap, hh = wItemH;
		MapCoord(ww, hh);
		
		if ((int)dwItem >= (int)m_State.size())
		{
			if (IsVert())
				yy += m_bOverlap;
			else
				xx += m_bOverlap;
		}
		
		BitBlt(m_WorkImg, xx, yy, ww, hh, NULL, 0, 0, PATCOPY);
		RECT rect = {xx, yy, xx + ww, yy + hh};
		InvalidateRect(m_Wnd, &rect, FALSE);
	}
	else
	{
	
HDC hDC = (bForceSelected || (m_State[dwItem].bState & BST_CHECKED)) ? bHilited ? m_SelectedHilitedImg : m_SelectedImg : bHilited ? m_HilitedImg : m_NormalImg;
		//BitBlt(m_WorkImg, xx, yy, m_wItemW, m_wItemH, hDC, m_State[dwItem].iImage * m_wItemH, 0, SRCCOPY);
		//HIMAGELIST hImg = (bForceSelected || (m_State[dwItem].bState & BST_CHECKED))? bHilited? m_hSelectedHilitedImgList: m_hSelectedImgList: bHilited? m_hHilightImgList: m_hNormalImgList;
		
		BLENDFUNCTION bf = {AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA};
		/*
		IMAGELISTDRAWPARAMS ildp = {sizeof(ildp), hImg, dwItem, m_WorkImg};
		ildp.rgbBk = 0xff00;
		ildp.rgbFg = 0xff00;
		ildp.fStyle = ILD_TRANSPARENT;
		ildp.dwRop = SRCCOPY;
		ildp.fState = ILS_NORMAL;
		ildp.Frame = 255;
		*/
		
		BitBlt(m_WorkImg, xx, yy, m_wItemW, m_wItemH, NULL, 0, 0, PATCOPY);
		
		bool bIsOnUp = ((m_State[dwItem].bState & BST_CHECKED) && (m_State[dwItem].bStyle & BS_AUTORADIOBUTTON));
		
		if (m_bOverlap)
		{
			int ww = m_bOverlap;
			int hh = m_wItemH;
			
			MapCoord(ww, hh);
			
			if ((bIsOnUp || !m_bLeftOnTop) && dwItem + 1 < m_State.size())
			{
				int xx = x + wItemW - m_bOverlap;
				int yy = y;
				
				MapCoord(xx, yy);
				HDC hNextDC = (m_State[dwItem + 1].bState & BST_CHECKED) ? m_SelectedImg : m_NormalImg;
				AlphaBlend(m_WorkImg, xx, yy, ww, hh, hNextDC, m_State[dwItem + 1].iImage * m_wItemW, 0, ww, hh, bf);
				
				//ildp.x = xx, ildp.y = yy, ildp.cx = ww, ildp.cy = hh, ildp.xBitmap = m_State[dwItem + 1].iImage * m_wItemH, ildp.yBitmap = 0;
				//ImageList_DrawIndirect(&ildp);
			}
			
			if ((bIsOnUp || m_bLeftOnTop) && dwItem > 0)
			{
				int xxSrc = IsVert() ? m_State[dwItem - 1].iImage * m_wItemW : (m_State[dwItem - 1].iImage + 1) * m_wItemW - m_bOverlap;
				int yySrc = IsVert() ? m_wItemH - m_bOverlap : 0;
				
				HDC hNextDC = (m_State[dwItem - 1].bState & BST_CHECKED) ? m_SelectedImg : m_NormalImg;
				AlphaBlend(m_WorkImg, xx, yy, ww, hh, hNextDC, xxSrc, yySrc, ww, hh, bf);
			}
		}
		
		//ildp.x = xx, ildp.y = yy, ildp.cx = m_wItemW, ildp.cy = m_wItemH, ildp.xBitmap = 0, ildp.yBitmap = 0;
		//ImageList_DrawIndirect(&ildp);
		//ImageList_Draw(hImg, dwItem, m_WorkImg, xx, yy, ILD_TRANSPARENT);
		AlphaBlend(m_WorkImg, xx, yy, m_wItemW, m_wItemH, hDC, m_State[dwItem].iImage * m_wItemW, 0, m_wItemW, m_wItemH, bf);
		
		if (m_bOverlap)
		{
			int ww = m_bOverlap;
			int hh = m_wItemH;
			
			MapCoord(ww, hh);
			
			if (dwItem + 1 < m_State.size() && ((!bIsOnUp && m_bLeftOnTop) || (((m_State[dwItem + 1].bState & BST_CHECKED) && (m_State[dwItem + 1].bStyle & BS_AUTORADIOBUTTON)))))
			{
				int xx = x + wItemW - m_bOverlap;
				int yy = y;
				int xxSrc = m_State[dwItem + 1].iImage * m_wItemW;
				int yySrc = 0;
				
				MapCoord(xx, yy);
				
				HDC hNextDC = (m_State[dwItem + 1].bState & BST_CHECKED) ? m_SelectedImg : m_NormalImg;
				AlphaBlend(m_WorkImg, xx, yy, ww, hh, hNextDC, xxSrc, yySrc, ww, hh, bf);
			}
			
			if (dwItem > 0 && ((!bIsOnUp && !m_bLeftOnTop) || (((m_State[dwItem - 1].bState & BST_CHECKED) && (m_State[dwItem - 1].bStyle & BS_AUTORADIOBUTTON)))))
			{
				int xxSrc = IsVert() ? m_State[dwItem - 1].iImage * m_wItemW : (m_State[dwItem - 1].iImage + 1) * m_wItemW - m_bOverlap;
				int yySrc = IsVert() ? m_wItemH - m_bOverlap : 0;
				
				HDC hNextDC = (m_State[dwItem - 1].bState & BST_CHECKED) ? m_SelectedImg : m_NormalImg;
				AlphaBlend(m_WorkImg, xx, yy, ww, hh, hNextDC, xxSrc, yySrc, ww, hh, bf);
			}
		}
		
		RECT rect = {xx, yy, xx + m_wItemW, yy + m_wItemH};
		InvalidateRect(m_Wnd, &rect, FALSE);
	}
	
	if (!bInternalCall && m_bUseArrows && dwItem < m_State.size())
	{
		bool bLeft = x <= m_wArrowLen;
		bool bRight = x + wItemW >= int(m_dwWndLen - m_wArrowLen);
		
		if ((bRight && x + wItemW < (int)m_dwWndLen) || (bLeft && x + wItemW <= (int)m_wArrowLen))
		{
			DrawItem(dwItem + 1, false, false, true);
		}
		
		if (((bLeft && x > 0) || (bRight && x >= (int)(m_dwWndLen - m_wArrowLen))))
		{
			DrawItem(dwItem - 1, false, false, true);
		}
		
		if (bLeft || bRight)
			DrawArrows(bLeft, bRight, false, ((m_bHilitedArrowInd == 1 && bRight) || (m_bHilitedArrowInd == 0 && bLeft)), false);
	}
}

void CSkinableCmdBar::DrawArrows(bool bLeft, bool bRight, bool bHilited, bool bPushed, bool bDrawBackground)
{
	if (bDrawBackground)
	{
		DWORD dwItem;
		
		if (bLeft)
		{
			if (m_iOffset < 0)
				dwItem = (DWORD) - 1;
			else
				dwItem = GetItemPromPos(m_iOffset);
				
			DrawItem(dwItem, false, false, true);
			
			WORD wItemW = m_wItemW, wItemH = m_wItemH;
			MapCoord(wItemW, wItemH);
			dwItem++;
			if (dwItem * wItemW - m_iOffset < m_wArrowLen)
				DrawItem(dwItem, false, false, true);
		}
		
		if (bRight)
		{
			dwItem = GetItemPromPos(m_iOffset + m_dwWndLen - m_wArrowLen);
			
			DrawItem(dwItem, false, false, true);
			
			WORD wItemW = m_wItemW, wItemH = m_wItemH;
			MapCoord(wItemW, wItemH);
			dwItem++;
			if (dwItem * wItemW - m_iOffset < m_dwWndLen)
				DrawItem(dwItem, false, false, true);
		}
	}
	
	WORD wItemH = m_wItemH, wItemW = m_wItemW;
	MapCoord(wItemW, wItemH);
	WORD w = m_wArrowLen, h = wItemH;
	MapCoord(w, h);
	
	if (m_bUseArrows)
	{
		int dx = bPushed ? m_wArrowLen * 4 : bHilited ? m_wArrowLen * 2 : 0;
		
		BLENDFUNCTION bf = {AC_SRC_OVER, 0, 0xff, AC_SRC_ALPHA};
		if (bLeft)
		{
			int x1 = dx, y1 = 0;
			MapCoord(x1, y1);
			AlphaBlend(m_WorkImg, 0, 0, w, h, m_ArrowsImg, x1, y1, w, h, bf);
		}
		
		if (bRight)
		{
			int x1 = m_wArrowLen + dx, y1 = 0;
			int x2 = m_dwWndLen - m_wArrowLen, y2 = 0;
			MapCoord(x1, y1);
			MapCoord(x2, y2);
			AlphaBlend(m_WorkImg, x2, y2, w, h, m_ArrowsImg, x1, y1, w, h, bf);
		}
	}
	
	RECT rect = {0, 0, w, h};
	
	if (bLeft)
		InvalidateRect(m_Wnd, &rect, FALSE);
		
	if (bRight)
	{
		if (IsVert())
		{
			rect.top = m_dwWndLen - m_wArrowLen;
			rect.bottom += m_dwWndLen - m_wArrowLen;
		}
		else
		{
			rect.left = m_dwWndLen - m_wArrowLen;
			rect.right += m_dwWndLen - m_wArrowLen;
		}
		InvalidateRect(m_Wnd, &rect, FALSE);
	}
}

void CSkinableCmdBar::OnPaint(PAINTSTRUCT *pps)
{
	int w = pps->rcPaint.right - pps->rcPaint.left;
	int h = pps->rcPaint.bottom - pps->rcPaint.top;
	
	HBRUSH hOrgBrush = (HBRUSH)SelectObject(pps->hdc, GetStockObject(DC_BRUSH));
	SetDCBrushColor(pps->hdc, GetDCBrushColor(m_WorkImg));
	
	DWORD dwVisibleLen = GetVisibleLen();
	if (IsVert())
	{
		if (pps->rcPaint.bottom > (int)dwVisibleLen)
		{
			h = dwVisibleLen - pps->rcPaint.top;
			BitBlt(pps->hdc, pps->rcPaint.left, dwVisibleLen, w, pps->rcPaint.bottom - dwVisibleLen, NULL, 0, 0, PATCOPY);
		}
	}
	else
	{
		if (pps->rcPaint.right > (int)dwVisibleLen)
		{
			w = dwVisibleLen - pps->rcPaint.left;
			BitBlt(pps->hdc, dwVisibleLen, pps->rcPaint.top, pps->rcPaint.right - dwVisibleLen, h, NULL, 0, 0, PATCOPY);
		}
	}
	
	BitBlt(pps->hdc, pps->rcPaint.left, pps->rcPaint.top, w, h, m_WorkImg, pps->rcPaint.left, pps->rcPaint.top, SRCCOPY);
	SelectObject(pps->hdc, hOrgBrush);
}

void CSkinableCmdBar::OnMouseMove(WORD x, WORD y, WPARAM dwFlags)
{
	MapCoord(x, y);
	bool bLeft = x <= m_wArrowLen;
	bool bRight = x >= m_dwWndLen - m_wArrowLen;
	
	if (m_bUseArrows && (bLeft || bRight))
	{
		if (m_bHilitedArrowInd != (BYTE)bRight)
		{
			ClearHilited();
			
			DrawArrows(bLeft, bRight, true, (m_bPushingArrowInd == 1 && bRight) || (m_bPushingArrowInd == 0 && bLeft), true);
			m_bHilitedArrowInd = bRight;
		}
	}
	else
	{
		//DWORD dwItem = (x + m_iOffset) / m_wItemW;
		DWORD dwItem = GetItemPromPos(x + m_iOffset);
		
		if (dwItem < m_State.size())
		{
			bool bIsChanged = m_dwHilitedItemInd != dwItem;
			
			if (bIsChanged)
			{
				ClearHilited();
				
				if (m_dwPushingItemInd == 0xffffffff || m_dwPushingItemInd == dwItem)
				{
					m_dwHilitedItemInd = dwItem;
					DrawItem(dwItem, true, m_dwPushingItemInd != 0xffffffff);
				}
			}
		}
		else
			ClearHilited();
	}
	
	if (dwFlags & MK_LBUTTON)
	{
		MapCoord(x, y);
		POINT pt = {x, y};
		ClientToScreen(m_Wnd, &pt);
		
		if (WindowFromPoint(pt) != m_Wnd)
			ClearHilited();
	}
	
	TRACKMOUSEEVENT tme = {sizeof(tme), TME_LEAVE, m_Wnd, 0};
	_TrackMouseEvent(&tme);
}

void CSkinableCmdBar::OnCommand(DWORD dwItem, ITEM_STRUCT &is)
{
	UNREFERENCED_PARAMETER(dwItem);
	if (is.bStyle & BS_AUTORADIOBUTTON)
		SendMessage(GetParent(m_Wnd), WM_APP + 701, is.iCmd, 0);
	else
		SendMessage(GetParent(m_Wnd), WM_COMMAND, is.iCmd, 0);
}

void CSkinableCmdBar::OnMouseUp(WORD x, WORD y, WPARAM dwFlags)
{
	ReleaseCapture();
	
	POINT pt = {x, y};
	ClientToScreen(m_Wnd, &pt);
	
	if (WindowFromPoint(pt) != m_Wnd)
	{
		ClearHilited();
		m_bPushingArrowInd = 0xff;
		m_bHilitedArrowInd = 0xff;
		
		m_dwPushingItemInd = 0xffffffff;
		m_dwHilitedItemInd = 0xffffffff;
	}
	else
	{
		MapCoord(x, y);
		bool bLeft = x <= m_wArrowLen;
		bool bRight = x >= m_dwWndLen - m_wArrowLen;
		
		if (m_bUseArrows && (bLeft || bRight))
		{
		}
		else
		{
			if (m_dwPushingItemInd != 0xffffffff)
			{
				DWORD dwItem = GetItemPromPos(x + m_iOffset);
				if (dwItem == m_dwPushingItemInd)
				{
					SetItemState(m_dwPushingItemInd, m_State[m_dwPushingItemInd].bState ^ BST_CHECKED, true);
					
					OnCommand(m_dwPushingItemInd, m_State[m_dwPushingItemInd]);
				}
			}
		}
		
		m_bPushingArrowInd = 0xff;
		m_bHilitedArrowInd = 0xff;
		
		m_dwPushingItemInd = 0xffffffff;
		m_dwHilitedItemInd = 0xffffffff;
		
		MapCoord(x, y);
		OnMouseMove(x, y, dwFlags);
	}
}

void CSkinableCmdBar::OnMouseDown(WORD x, WORD y, WPARAM dwFlags)
{
	SetCapture(m_Wnd);
	
	MapCoord(x, y);
	bool bLeft = x <= m_wArrowLen;
	bool bRight = x >= m_dwWndLen - m_wArrowLen;
	
	if (m_bUseArrows && (bLeft || bRight))
	{
		DrawArrows(bLeft, bRight, false, true, true);
		m_bPushingArrowInd = bRight;
		
		OnTimer();
		SetTimer(m_Wnd, 1000, 100, NULL);
	}
	else
	{
		DWORD dwItem = GetItemPromPos(x + m_iOffset);
		
		if (dwItem < m_State.size())
		{
			m_dwPushingItemInd = dwItem;
			m_dwHilitedItemInd = 0xffffffff;
			MapCoord(x, y);
			
			if (m_State[m_dwPushingItemInd].bStyle & BS_AUTORADIOBUTTON)
				OnMouseUp(x, y, dwFlags);
			else
				OnMouseMove(x, y, dwFlags);
		}
	}
}

void CSkinableCmdBar::OnSize(WORD w, WORD h, WPARAM dwFlags)
{
	if (m_State.empty())
		return;
		
	UNREFERENCED_PARAMETER(dwFlags);
	MapCoord(w, h);
	bool bUpdate = false;
	bool bOffsetChanged = false;
	bool bUsedArrows = m_bUseArrows;
	
	if (w < GetVisibleLen())
	{
		if (!m_bUseArrows)
		{
			m_bUseArrows = true;
			bUpdate = true;
		}
	}
	else
	{
		if (m_bUseArrows)
		{
			m_bUseArrows = false;
			bUpdate = true;
			
			if (m_iOffset != 0)
			{
				m_iOffset = 0;
				bOffsetChanged = true;
			}
		}
	}
	
	bool bDrawBkg = true;
	DWORD dwVisibleLen = GetVisibleLen();
	if ((w + m_iOffset > (int)dwVisibleLen && m_iOffset != 0) || bOffsetChanged)
	{
		if (w > dwVisibleLen)
			m_iOffset = 0;
		else
			m_iOffset = dwVisibleLen - w + m_wArrowLen;
			
		bUpdate = true;
		
		Update();
		bDrawBkg = false;
		InvalidateRect(m_Wnd, NULL, FALSE);
	}
	else
	{
		if (bUsedArrows)
		{
			bool bUseArrows = m_bUseArrows;
			
			// cleanup previous arrow position
			m_bUseArrows = false;
			DrawArrows(false, true, false, false, true);
			m_bUseArrows = bUseArrows;
		}
	}
	m_dwWndLen = w;
	DrawArrows(bUpdate, true, false, false, true);
}

void CSkinableCmdBar::OnGetMinMaxInfo(MINMAXINFO *pMinMaxInfo)
{
	pMinMaxInfo->ptMaxTrackSize.x = GetVisibleLen();
	pMinMaxInfo->ptMaxTrackSize.y = m_wItemW;
	pMinMaxInfo->ptMinTrackSize.x = 0;
	pMinMaxInfo->ptMinTrackSize.y = m_wItemW;
	
	MapCoord(pMinMaxInfo->ptMaxTrackSize.x, pMinMaxInfo->ptMaxTrackSize.y);
}

void CSkinableCmdBar::OnTimer()
{
	bool bUpdate = true;
	
	if (m_bPushingArrowInd == 0)
	{
		if (m_iOffset > m_wItemW - m_wArrowLen)
			m_iOffset -= m_wItemW;
		else
		{
			m_iOffset = -m_wArrowLen;
			KillTimer(m_Wnd, 1000);
		}
	}
	else if (m_bPushingArrowInd == 1)
	{
		WORD wItemW = m_wItemW, wItemH = m_wItemH;
		MapCoord(wItemW, wItemH);
		
		DWORD dwVisibleLen = GetVisibleLen();
		
		if (m_iOffset < int(dwVisibleLen - m_dwWndLen - wItemW + m_wArrowLen))
			m_iOffset += wItemW;
		else
		{
			m_iOffset = dwVisibleLen - m_dwWndLen + m_wArrowLen;
			KillTimer(m_Wnd, 1000);
		}
	}
	else
	{
		KillTimer(m_Wnd, 1000);
		bUpdate = false;
	}
	
	if (bUpdate)
	{
		DWORD dwItem = GetItemPromPos(m_iOffset);
		DWORD dwEnd = dwItem + GetItemPromPos(m_dwWndLen + m_wItemW - 1);
		
		if (dwEnd > m_State.size())
		{
			dwEnd = m_State.size();
		}
		
		for (; dwItem < dwEnd; dwItem++)
		{
			DrawItem(dwItem, false, false);
		}
		
		InvalidateRect(m_Wnd, NULL, FALSE);
	}
}

template<typename T>
void CSkinableCmdBar::MapCoord(T &x, T &y) const
{
	if (IsVert())
	{
		T t = x;
		x = y;
		y = t;
	}
}

static HBITMAP CopyBMP(HBITMAP hBMP, RECT *prect)
{
	HBITMAP hOutBMP = NULL;
	
	if (hBMP)
	{
		CBmp bmpIn(hBMP);
		CBmp bmpOut;
		
		WORD w, h;
		WORD xSrc = 0, ySrc = 0;
		
		if (prect)
		{
			w = WORD(prect->right - prect->left);
			h = WORD(prect->bottom - prect->top);
			xSrc = WORD(prect->left);
			ySrc = WORD(prect->top);
		}
		else
		{
			DWORD dwDim = bmpIn.GetDimension();
			w = LOWORD(dwDim);
			h = HIWORD(dwDim);
		}
		
		bmpOut.CreateCompatibleBitmap(NULL, w, h);
		
		BitBlt(bmpOut, 0, 0, w, h, bmpIn, 0, 0, SRCCOPY);
		
		hOutBMP = bmpOut;
		
		bmpOut.Detach();
		bmpIn.Detach();
	}
	
	return hOutBMP;
}

int CSkinableCmdBar::AddItemToSkin(HICON hIcon, CBmp &Img, DWORD dwItem, BYTE bState)
{
	int iImage;
	CBmp bmpTemp;
	
	WORD xDim;
	WORD yDim;
	
	if (dwItem == 0xffffffff && (HBITMAP)Img)
	{
		// Expand image by 1 item
		DWORD dwDim = Img.GetDimension();
		xDim = LOWORD(dwDim);
		yDim = HIWORD(dwDim);
		bmpTemp.CreateCompatibleBitmap(NULL, xDim + m_wItemW, m_wItemH);
		BitBlt(bmpTemp, 0, 0, xDim, yDim, Img, 0, 0, SRCCOPY);
		
		// Swap current and temp images
		HBITMAP hCurBMP = Img;
		HBITMAP hNewBMP = bmpTemp;
		Img.Detach();
		bmpTemp.Detach();
		Img.Attach(hNewBMP);
		DeleteObject(hCurBMP);
		
		iImage = xDim / m_wItemW;
	}
	else
	{
		xDim = WORD(dwItem * m_wItemW);
		iImage = 0;
	}
	
	BitBlt(Img, xDim, 0, m_wItemW, m_wItemH, m_TemplateImg, m_wItemW * bState, 0, SRCCOPY);
	DrawIconEx(Img, xDim, 0, hIcon, 0, 0, 0, NULL, DI_NORMAL);
	
	return iImage;
}

DWORD CSkinableCmdBar::AddItem(HICON hIcon, LPARAM iCmd, BYTE bStyle, BYTE bState, LPARAM lParam)
{
	DWORD dwRet = 0xffffffff;
	
	if (hIcon && (HBITMAP)m_TemplateImg)
	{
		int iImage;
		DWORD dwItem = 0xffffffff;
		
		if (!(HBITMAP)m_WorkImg)
		{
			DWORD dwTemplateDim = m_TemplateImg.GetDimension();
			WORD w = LOWORD(dwTemplateDim) / 4;
			WORD h = HIWORD(dwTemplateDim);
			
			RECT rect = {0, 0, w, h};
			HBITMAP hNormalBMP = CopyBMP(m_TemplateImg, &rect);
			rect.left += w, rect.right += w;
			HBITMAP hHilitedBMP = CopyBMP(m_TemplateImg, &rect);
			rect.left += w, rect.right += w;
			HBITMAP hSelectedBMP = CopyBMP(m_TemplateImg, &rect);
			rect.left += w, rect.right += w;
			HBITMAP hSelectedHilitedBMP = CopyBMP(m_TemplateImg, &rect);
			InitializeSkin(1, hNormalBMP, hHilitedBMP, hSelectedBMP, hSelectedHilitedBMP, NULL, m_clrBack);
			dwItem = 0;
			
			GetClientRect(m_Wnd, &rect);
			SetWindowPos(m_Wnd, NULL, 0, 0, rect.right - rect.left, m_wItemH, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE);
		}
		
		iImage = AddItemToSkin(hIcon, m_NormalImg, dwItem, 0);
		iImage = AddItemToSkin(hIcon, m_HilitedImg, dwItem, 1);
		iImage = AddItemToSkin(hIcon, m_SelectedImg, dwItem, 2);
		iImage = AddItemToSkin(hIcon, m_SelectedHilitedImg, dwItem, 3);
		
		HBITMAP hWorkBMP = m_WorkImg;
		m_WorkImg.Detach();
		DeleteObject(hWorkBMP);
		
		m_wSkinItemCount++;
		
		m_WorkImg.CreateCompatibleBitmap(NULL, m_wItemW * m_wSkinItemCount, m_wItemH);
		
		dwRet = AddItem(iImage, iCmd, bStyle, bState, lParam, true);
	}
	
	return dwRet;
}

DWORD CSkinableCmdBar::AddItem(int iImage, LPARAM iCmd, BYTE bStyle, BYTE bState, LPARAM lParam, bool bUpdateAll)
{
	if (iImage >= m_wSkinItemCount)
		return 0xffffffff;
		
	m_State.push_back(ITEM_STRUCT(bStyle, 0, iImage, iCmd, lParam));
	
	if (bState != 0)
		SetItemState(m_State.size() - 1, bState, false);
		
	if (bUpdateAll)
		Update();
	else
	{
		RECT rect;
		GetClientRect(m_Wnd, &rect);
		OnSize((WORD)(rect.right - rect.left), (WORD)(rect.bottom - rect.top), 0);
		DrawItem(m_State.size() - 1, false, false);
	}
	
	return (DWORD)m_State.size();
}

bool CSkinableCmdBar::RemoveItem(DWORD dwItem)
{
	bool bRet = false;
	if (dwItem < m_State.size())
	{
		m_State.erase(m_State.begin() + dwItem);
		Update();
		bRet = true;
	}
	
	return true;
}

void CSkinableCmdBar::SetItemState(DWORD dwItem, BYTE bState, bool bUpdate)
{
	if (m_State[dwItem].bStyle & BS_AUTORADIOBUTTON)
	{
		if (m_State[dwItem].bState & BST_CHECKED)
		{
			// already checked, skip action
			return;
		}
		
		DWORD ind = 0;
		for (auto i = m_State.cbegin(); i != m_State.cend(); ++i, ind++)
		{
			if (ind != dwItem && (i->bState & BST_CHECKED))
			{
				m_State[ind].bState &= ~BST_CHECKED;
				DrawItem(ind, false, false);
			}
		}
	}
	
	if ((m_State[dwItem].bStyle & (BS_CHECKBOX | BS_AUTORADIOBUTTON)))
	{
		if (m_State[dwItem].bState != bState)
		{
			m_State[dwItem].bState = bState;
			
			if (bUpdate)
				DrawItem(dwItem, m_dwHilitedItemInd == dwItem, false);
		}
	}
}

DWORD CSkinableCmdBar::GetItemPromPos(int iPos)
{
	WORD w = m_wItemW, h = m_wItemH;
	MapCoord(w, h);
	return iPos / (w - m_bOverlap);// - (iPos < 0? 1: 0);
}

DWORD CSkinableCmdBar::GetItemByCmd(LPARAM iCmd)
{
	DWORD dwItem = 0xffffffff;
	DWORD ind = 0;
	for (auto i = m_State.cbegin(); i != m_State.cend(); ++i, ind++)
	{
		if (i->iCmd == iCmd)
			dwItem = ind;
	}
	
	return dwItem;
}

DWORD CSkinableCmdBar::GetHeight() const
{
	WORD w = m_wItemW, h = m_wItemW;
	
	MapCoord(w, h);
	return w;
}
#endif