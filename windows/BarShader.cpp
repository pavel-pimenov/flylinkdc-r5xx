#include "stdafx.h"
#define _USE_MATH_DEFINES // [+]IRainman
#include <math.h>

#include "MainFrm.h"
#include "BarShader.h"

CBarShader::CBarShader(uint32_t dwHeight, uint32_t dwWidth, COLORREF crColor /*= 0*/, uint64_t qwFileSize /*= 1*/)
{
	m_iWidth = dwWidth;
	m_iHeight = dwHeight;
	m_qwFileSize = 0;
	m_Spans.SetAt(0, crColor);
	m_Spans.SetAt(qwFileSize, 0);
	m_bIsPreview = false;
	m_used3dlevel = 0;
	SetFileSize(qwFileSize);
}

CBarShader::~CBarShader(void)
{
}

void CBarShader::BuildModifiers()
{
	static const double dDepths[5] = { 5.5, 4.0, 3.0, 2.50, 2.25 };     //aqua bar - smoother gradient jumps...
	double  depth = dDepths[((m_used3dlevel > 5) ? (256 - m_used3dlevel) : m_used3dlevel) - 1];
	uint32_t dwCount = CalcHalfHeight(m_iHeight);
	double piOverDepth = M_PI / depth;
	double base = M_PI_2 - piOverDepth;
	double increment = piOverDepth / static_cast<double>(dwCount - 1);
	
	m_pdblModifiers.resize(dwCount);
	for (uint32_t i = 0; i < dwCount; i++, base += increment)
		m_pdblModifiers[i] = sin(base);
}

void CBarShader::CalcPerPixelandPerByte()
{
	if (m_qwFileSize)
		m_dblPixelsPerByte = static_cast<double>(m_iWidth) / m_qwFileSize;
	else
		m_dblPixelsPerByte = 0.0;
	if (m_iWidth)
		m_dblBytesPerPixel = static_cast<double>(m_qwFileSize) / m_iWidth;
	else
		m_dblBytesPerPixel = 0.0;
}

void CBarShader::SetWidth(uint32_t width)
{
	if (m_iWidth != width)
	{
		m_iWidth = width;
		CalcPerPixelandPerByte();
	}
}

void CBarShader::SetFileSize(uint64_t qwFileSize)
{
	// !SMT!-F
	if ((int64_t)qwFileSize < 0) qwFileSize = 0;
	if (m_qwFileSize != qwFileSize)
	{
		m_qwFileSize = qwFileSize;
		CalcPerPixelandPerByte();
	}
}

void CBarShader::SetHeight(uint32_t height)
{
	if (m_iHeight != height)
	{
		m_iHeight = height;
		BuildModifiers();
	}
}

void CBarShader::FillRange(uint64_t qwStart, uint64_t qwEnd, COLORREF crColor)
{
	if (qwEnd > m_qwFileSize)
		qwEnd = m_qwFileSize;
		
	if (qwStart >= qwEnd)
		return;
	POSITION endprev, endpos = m_Spans.FindFirstKeyAfter(qwEnd + 1ui64);
	
	if ((endprev = endpos) != NULL)
		m_Spans.GetPrev(endpos);
	else
		endpos = m_Spans.GetTailPosition();
		
	COLORREF endcolor = m_Spans.GetValueAt(endpos);
	
	if ((endcolor == crColor) && (m_Spans.GetKeyAt(endpos) <= qwEnd) && (endprev != NULL))
		endpos = endprev;
	else
		endpos = m_Spans.SetAt(qwEnd, endcolor);
		
	for (POSITION pos = m_Spans.FindFirstKeyAfter(qwStart); pos != endpos;)
	{
		POSITION pos1 = pos;
		m_Spans.GetNext(pos);
		m_Spans.RemoveAt(pos1);
	}
	
	m_Spans.GetPrev(endpos);
	
	if ((endpos == NULL) || (m_Spans.GetValueAt(endpos) != crColor))
		m_Spans.SetAt(qwStart, crColor);
}

void CBarShader::Fill(COLORREF crColor)
{
	m_Spans.RemoveAll();
	m_Spans.SetAt(0, crColor);
	m_Spans.SetAt(m_qwFileSize, 0);
}

void CBarShader::Draw(CDC& dc, int iLeft, int iTop, int P3DDepth)
{
	m_used3dlevel = (byte)P3DDepth;
	COLORREF crLastColor = (COLORREF)~0, crPrevBkColor = dc.GetBkColor();
	POSITION pos = m_Spans.GetHeadPosition();
	RECT rectSpan;
	rectSpan.top = iTop;
	rectSpan.bottom = iTop + m_iHeight;
	rectSpan.right = iLeft;
	// flylinkdc-r5xx_7.7.502.16176_20131211_210642.mini.dmp: 0xC0000091: Floating-point overflow.
	// https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=48383
	int64_t iBytesInOnePixel = static_cast<int64_t>(m_dblBytesPerPixel + 0.5);
	uint64_t qwStart = 0;
	COLORREF crColor = m_Spans.GetNextValue(pos);
	
	iLeft += m_iWidth;
	while ((pos != NULL) && (rectSpan.right < iLeft))
	{
		uint64_t qwSpan = m_Spans.GetKeyAt(pos) - qwStart;
		int iPixels = static_cast<int>(qwSpan * m_dblPixelsPerByte + 0.5);
		
		if (iPixels > 0)
		{
			rectSpan.left = rectSpan.right;
			rectSpan.right += iPixels;
			FillRect(dc, &rectSpan, crLastColor = crColor);
			
			qwStart += static_cast<uint64_t>(iPixels * m_dblBytesPerPixel + 0.5);
		}
		else
		{
			double dRed = 0, dGreen = 0, dBlue = 0;
			uint32_t dwRed, dwGreen, dwBlue;
			uint64_t qwLast = qwStart, qwEnd = qwStart + iBytesInOnePixel;
			
			do
			{
				double  dblWeight = (min(m_Spans.GetKeyAt(pos), qwEnd) - qwLast) * m_dblPixelsPerByte;
				dRed   += GetRValue(crColor) * dblWeight;
				dGreen += GetGValue(crColor) * dblWeight;
				dBlue  += GetBValue(crColor) * dblWeight;
				if ((qwLast = m_Spans.GetKeyAt(pos)) >= qwEnd)
					break;
				crColor = m_Spans.GetValueAt(pos);
				m_Spans.GetNext(pos);
			}
			while (pos != NULL);
			rectSpan.left = rectSpan.right;
			rectSpan.right++;
			
			//  Saturation
			dwRed = static_cast<uint32_t>(dRed);
			if (dwRed > 255)
				dwRed = 255;
			dwGreen = static_cast<uint32_t>(dGreen);
			if (dwGreen > 255)
				dwGreen = 255;
			dwBlue = static_cast<uint32_t>(dBlue);
			if (dwBlue > 255)
				dwBlue = 255;
				
			FillRect(dc, &rectSpan, crLastColor = RGB(dwRed, dwGreen, dwBlue));
			qwStart += iBytesInOnePixel;
		}
		while ((pos != NULL) && (m_Spans.GetKeyAt(pos) <= qwStart))
			crColor = m_Spans.GetNextValue(pos);
	}
	if ((rectSpan.right < iLeft) && (crLastColor != ~0))
	{
		rectSpan.left = rectSpan.right;
		rectSpan.right = iLeft;
		FillRect(dc, &rectSpan, crLastColor);
	}
	dc.SetBkColor(crPrevBkColor);   //restore previous background color
}

void CBarShader::FillRect(CDC& dc, LPCRECT rectSpan, COLORREF crColor)
{
	if (!crColor)
		dc.FillSolidRect(rectSpan, crColor);
	else
	{
		if (m_pdblModifiers.empty())
			BuildModifiers();
			
		double  dblRed = GetRValue(crColor), dblGreen = GetGValue(crColor), dblBlue = GetBValue(crColor);
		double  dAdd;
		
		if (m_used3dlevel > 5)      //Cax2 aqua bar
		{
			const double dMod = 1.0 - .025 * (256 - m_used3dlevel);      //variable central darkness - from 97.5% to 87.5% of the original colour...
			dAdd = 255;
			
			dblRed = dMod * dblRed - dAdd;
			dblGreen = dMod * dblGreen - dAdd;
			dblBlue = dMod * dblBlue - dAdd;
		}
		else
			dAdd = 0;
			
		RECT        rect;
		int         iTop = rectSpan->top, iBot = rectSpan->bottom;
		const size_t l_count = static_cast<size_t>(CalcHalfHeight(m_iHeight));
		dcassert(m_pdblModifiers.size() <= l_count);
		
		rect.right = rectSpan->right;
		rect.left = rectSpan->left;
		
		for (size_t i = 0; i < l_count; ++i)
		{
			const auto& pdCurr  = m_pdblModifiers[i];
			crColor = RGB(static_cast<int>(dAdd + dblRed * pdCurr),
			              static_cast<int>(dAdd + dblGreen * pdCurr),
			              static_cast<int>(dAdd + dblBlue * pdCurr));
			rect.top = iTop++;
			rect.bottom = iTop;
			dc.FillSolidRect(&rect, crColor);
			
			rect.bottom = iBot--;
			rect.top = iBot;
			//  Fast way to fill, background color is already set inside previous FillSolidRect
			dc.FillSolidRect(&rect, crColor);
		}
	}
}

// OperaColors
#define MIN3(a, b, c) (((a) < (b)) ? ((((a) < (c)) ? (a) : (c))) : ((((b) < (c)) ? (b) : (c))))
#define MAX3(a, b, c) (((a) > (b)) ? ((((a) > (c)) ? (a) : (c))) : ((((b) > (c)) ? (b) : (c))))
#define CENTER(a, b, c) ((((a) < (b)) && ((a) < (c))) ? (((b) < (c)) ? (b) : (c)) : ((((b) < (a)) && ((b) < (c))) ? (((a) < (c)) ? (a) : (c)) : (((a) < (b)) ? (a) : (b))))
#define ABS(a) (((a) < 0) ? (-(a)): (a))

OperaColors::FCIMap OperaColors::g_flood_cache;

double OperaColors::RGB2HUE(double r, double g, double b)
{
	double H = 60;
	double F = 0;
	double m2 = MAX3(r, g, b);
	double m1 = CENTER(r, g, b);
	double m0 = MIN3(r, g, b);
	
	if (EqualD(m2, m1))
	{
		if (EqualD(r, g))
		{
			goto _RGB2HUE_END;
		}
		if (EqualD(g, b))
		{
			H = 180;
			goto _RGB2HUE_END;
		}
		goto _RGB2HUE_END;
	}
	F = 60 * (m1 - m0) / (m2 - m0);
	if (EqualD(r, m2))
	{
		H = 0 + F * (g - b);
		goto _RGB2HUE_END;
	}
	if (EqualD(g, m2))
	{
		H = 120 + F * (b - r);
		goto _RGB2HUE_END;
	}
	H = 240 + F * (r - g);
	
_RGB2HUE_END:
	if (H < 0)
		H = H + 360;
	return H;
}

RGBTRIPLE OperaColors::HUE2RGB(double m0, double m2, double h)
{
	RGBTRIPLE rgbt = {0, 0, 0};
	double m1, F = 0;
	int n;
	while (h < 0)
		h += 360;
	while (h >= 360)
		h -= 360;
	n = (int)(h / 60);
	
	if (h < 60)
		F = h;
	else if (h < 180)
		F = h - 120;
	else if (h < 300)
		F = h - 240;
	else
		F = h - 360;
	m1 = m0 + (m2 - m0) * sqrt(ABS(F / 60));
	switch (n)
	{
		case 0:
			rgbt.rgbtRed = (BYTE)(255 * m2);
			rgbt.rgbtGreen = (BYTE)(255 * m1);
			rgbt.rgbtBlue = (BYTE)(255 * m0);
			break;
		case 1:
			rgbt.rgbtRed = (BYTE)(255 * m1);
			rgbt.rgbtGreen = (BYTE)(255 * m2);
			rgbt.rgbtBlue = (BYTE)(255 * m0);
			break;
		case 2:
			rgbt.rgbtRed = (BYTE)(255 * m0);
			rgbt.rgbtGreen = (BYTE)(255 * m2);
			rgbt.rgbtBlue = (BYTE)(255 * m1);
			break;
		case 3:
			rgbt.rgbtRed = (BYTE)(255 * m0);
			rgbt.rgbtGreen = (BYTE)(255 * m1);
			rgbt.rgbtBlue = (BYTE)(255 * m2);
			break;
		case 4:
			rgbt.rgbtRed = (BYTE)(255 * m1);
			rgbt.rgbtGreen = (BYTE)(255 * m0);
			rgbt.rgbtBlue = (BYTE)(255 * m2);
			break;
		case 5:
			rgbt.rgbtRed = (BYTE)(255 * m2);
			rgbt.rgbtGreen = (BYTE)(255 * m0);
			rgbt.rgbtBlue = (BYTE)(255 * m1);
			break;
	}
	return rgbt;
}

HLSTRIPLE OperaColors::RGB2HLS(BYTE red, BYTE green, BYTE blue)
{
	double r = (double)red / 255;
	double g = (double)green / 255;
	double b = (double)blue / 255;
	double m0 = MIN3(r, g, b), m2 = MAX3(r, g, b), d;
	HLSTRIPLE hlst = {0, -1, -1};
	
	hlst.hlstLightness = (m2 + m0) / 2;
	d = (m2 - m0) / 2;
	if (hlst.hlstLightness <= 0.5)
	{
		if (EqualD(hlst.hlstLightness, 0))
			hlst.hlstLightness = 0.1;
		hlst.hlstSaturation = d / hlst.hlstLightness;
	}
	else
	{
		if (EqualD(hlst.hlstLightness, 1))
			hlst.hlstLightness = 0.99;
		hlst.hlstSaturation = d / (1 - hlst.hlstLightness);
	}
	if (hlst.hlstSaturation > 0 && hlst.hlstSaturation < 1)
		hlst.hlstHue = RGB2HUE(r, g, b);
	return hlst;
}

RGBTRIPLE OperaColors::HLS2RGB(double hue, double lightness, double saturation)
{
	RGBTRIPLE rgbt = {0, 0, 0};
	double d;
	
	if (EqualD(lightness, 1))
	{
		rgbt.rgbtRed = rgbt.rgbtGreen = rgbt.rgbtBlue = 255;
		return rgbt;
	}
	if (EqualD(lightness, 0))
		return rgbt;
	if (lightness <= 0.5)
		d = saturation * lightness;
	else
		d = saturation * (1 - lightness);
	if (EqualD(d, 0))
	{
		rgbt.rgbtRed = rgbt.rgbtGreen = rgbt.rgbtBlue = (BYTE)(lightness * 255);
		return rgbt;
	}
	return HUE2RGB(lightness - d, lightness + d, hue);
}

void OperaColors::EnlightenFlood(const COLORREF& clr, COLORREF& a, COLORREF& b)
{
	const HLSCOLOR hls_a = ::RGB2HLS(clr);
	const HLSCOLOR hls_b = hls_a;
	BYTE buf = HLS_L(hls_a);
	if (buf < 38)
		buf = 0;
	else
		buf -= 38;
	a = ::HLS2RGB(HLS(HLS_H(hls_a), buf, HLS_S(hls_a)));
	buf = HLS_L(hls_b);
	if (buf > 217)
		buf = 255;
	else
		buf += 38;
	b = ::HLS2RGB(HLS(HLS_H(hls_b), buf, HLS_S(hls_b)));
}

COLORREF OperaColors::TextFromBackground(COLORREF bg)
{
	const HLSTRIPLE hlst = RGB2HLS(bg);
	if (hlst.hlstLightness > 0.63)
		return RGB(0, 0, 0);
	else
		return RGB(255, 255, 255);
}

void OperaColors::ClearCache()
{
	for (auto i = g_flood_cache.begin(); i != g_flood_cache.end(); ++i)
	{
		delete i->second;
	}
	g_flood_cache.clear();
}
void OperaColors::FloodFill(CDC& hDC, int x1, int y1, int x2, int y2, const COLORREF c1, const COLORREF c2, bool p_light /*= true */)
{
	if (x2 <= x1 || y2 <= y1 || x2 > 10000)
		return;
		
	int w = x2 - x1;
	int h = y2 - y1;
	
	FloodCacheItem::FCIMapper fcim = {c1 & (p_light ? 0x80FF : 0x00FF), c2 & 0x00FF, p_light}; // Make it hash-safe
	const auto& i = g_flood_cache.find(fcim); // TODO - убрать этот лишний find
	
	FloodCacheItem* fci = nullptr;
	if (i != g_flood_cache.end())
	{
		fci = i->second;
		if (fci->h >= h && fci->w >= w)
		{
			// Perfect, this kindof flood already exist in memory, lets paint it stretched
			SetStretchBltMode(hDC.m_hDC, HALFTONE);
			StretchBlt(hDC.m_hDC, x1, y1, w, h, fci->hDC, 0, 0, fci->w, fci->h, SRCCOPY);
			return;
		}
		DeleteDC(fci->hDC);
		fci->hDC = nullptr;
	}
	else
	{
		fci = new FloodCacheItem();
		g_flood_cache[fcim] = fci;
	}
	
	fci->hDC = ::CreateCompatibleDC(hDC.m_hDC); // Leak
	fci->w = w;
	fci->h = h;
	fci->m_mapper = fcim;
	
	BITMAPINFOHEADER bih;
	ZeroMemory(&bih, sizeof(BITMAPINFOHEADER));
	bih.biSize = sizeof(BITMAPINFOHEADER);
	bih.biWidth = w;
	bih.biHeight = -h;
	bih.biPlanes = 1;
	bih.biBitCount = 32;
	bih.biCompression = BI_RGB;
	bih.biClrUsed = 32;
	HBITMAP hBitmap = ::CreateDIBitmap(hDC.m_hDC, &bih, 0, NULL, NULL, DIB_RGB_COLORS);
	// [merge from AirDC++]
	// fix http://code.google.com/p/flylinkdc/issues/detail?id=1397
	::DeleteObject(::SelectObject(fci->hDC, hBitmap));
	
	if (!p_light)
	{
		for (int _x = 0; _x < w; ++_x)
		{
			HBRUSH hBr = CreateSolidBrush(blendColors(c2, c1, double(_x - x1) / (double)(w)));
			const RECT rc = { _x, 0, _x + 1, h };
			::FillRect(fci->hDC, &rc, hBr);
			DeleteObject(hBr);
		}
	}
	else
	{
		const int MAX_SHADE = 44;
		const int SHADE_LEVEL = 90;
		const static int g_blend_vector[MAX_SHADE] = {0, 8, 16, 20, 10, 4, 0, -2, -4, -6, -10, -12, -14, -16, -14, -12, -10, -8, -6, -4, -2, 0, //-V112
		                                              1, 2, 3, 8, 10, 12, 14, 16, 14, 12, 10, 6, 4, 2, 0, -4, -10, -20, -16, -8, 0 //-V112
		                                             };
		for (int _x = 0; _x <= w; ++_x)
		{
			const COLORREF cr = blendColors(c2, c1, double(_x) / double(w));
			for (int _y = 0; _y < h; ++_y)
			{
				const int l_index = (size_t)floor((double(_y) / h) * (MAX_SHADE - 1));
				dcassert(l_index < MAX_SHADE && l_index >= 0);
				//if (l_index < MAX_SHADE && l_index >= 0)
				{
					SetPixelV(fci->hDC, _x, _y, brightenColor(cr, (double)g_blend_vector[l_index] / (double)SHADE_LEVEL));
				}
			}
		}
	}
	BitBlt(hDC.m_hDC, x1, y1, x2, y2, fci->hDC, 0, 0, SRCCOPY);
}

/*
        static void FloodFill(CDC& hDC, int x1, int y1, int x2, int y2, COLORREF c1, COLORREF c2, bool light = true)
        {
            if (x2 <= x1 || y2 <= y1 || x2 > 10000)
                return;

            int w = x2 - x1;
            int h = y2 - y1;

            FloodCacheItem::FCIMapper fcim = {c1, c2, light}; // Make it hash-safe // we must map all colors but not only R component
            auto& l_flood_item = g_flood_cache[fcim];

            FloodCacheItem* fci = nullptr;
            if (l_flood_item != nullptr)
            {
                fci = l_flood_item;
                if (fci->h >= h && fci->w >= w)
                {
                    // Perfect, this kindof flood already exist in memory, lets paint it stretched
                    SetStretchBltMode(hDC.m_hDC, HALFTONE);
                    StretchBlt(hDC.m_hDC, x1, y1, w, h, fci->hDC, 0, 0, fci->w, fci->h, SRCCOPY);
                    return;
                }
                DeleteDC(fci->hDC);
                fci->hDC = nullptr;
            }
            else
            {
                fci = new FloodCacheItem();
                l_flood_item = fci;
            }

            fci->hDC = ::CreateCompatibleDC(hDC.m_hDC); // Leak
            fci->w = w;
            fci->h = h;
            fci->m_mapper = fcim;

            HBITMAP hBitmap = CreateBitmap(w, h, 1, 32, NULL); // Leak
            HBITMAP hOldBitmap = (HBITMAP)::SelectObject(fci->hDC, hBitmap);

            if (!light)
            {
                for (int _x = 0; _x < w; ++_x)
                {
                    HBRUSH hBr = CreateSolidBrush(blendColors(c2, c1, (double)(_x - x1) / (double)(w)));
                    const RECT rc = { _x, 0, _x + 1, h };
                    ::FillRect(fci->hDC, &rc, hBr);
                    DeleteObject(hBr);
                }
            }
            else
            {
                for (int _x = 0; _x <= w; ++_x)
                {
                    const COLORREF cr = blendColors(c2, c1, (double)(_x) / (double)(w));
                    for (int _y = 0; _y < h; ++_y)
                    {
                        SetPixelV(fci->hDC, _x, _y, brightenColor(cr, (double)blend_vector[(size_t)floor(((double)(_y) / h) * MAX_SHADE - 1)] / (double)SHADE_LEVEL));
                    }
                }
            }
            BitBlt(hDC.m_hDC, x1, y1, x2, y2, fci->hDC, 0, 0, SRCCOPY);
            DeleteObject (SelectObject(fci->hDC, hOldBitmap));
        }

*/
