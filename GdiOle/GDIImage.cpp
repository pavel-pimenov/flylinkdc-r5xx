#include "stdafx.h"
#ifdef IRAINMAN_INCLUDE_GDI_OLE
#include "GdiImage.h"

#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
std::unique_ptr<webrtc::RWLockWrapper> CGDIImage::g_GDIcs = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::unordered_set<CGDIImage*> CGDIImage::g_GDIImageSet;
unsigned CGDIImage::g_AnimationDeathDetectCount = 0;
unsigned CGDIImage::g_AnimationCount = 0;
unsigned CGDIImage::g_AnimationCountMax = 0;
#endif // FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE

CGDIImage::CGDIImage(LPCWSTR pszFileName, HWND hCallbackWnd, DWORD dwCallbackMsg):
	m_dwFramesCount(0), m_pImage(nullptr), m_pItem(nullptr), m_hTimer(nullptr), m_lRef(1),
	m_hCallbackWnd(hCallbackWnd), m_dwCallbackMsg(dwCallbackMsg),
	m_dwWidth(0),
	m_dwHeight(0),
	m_dwCurrentFrame(0)
	, m_allowCreateTimer(true) // [+] IRainman fix.
{
	dcassert(!isShutdown());
	InitializeCriticalSectionAndSpinCount(&m_csCallback, CRITICAL_SECTION_SPIN_COUNT); // [!] IRainman opt.
	m_pImage = new Gdiplus::Image(pszFileName);
	if (m_pImage->GetLastStatus() != Gdiplus::Ok)
		safe_delete(m_pImage);
	if (!m_pImage)
		return;
	m_pItem = 0;
	if (UINT TotalBuffer = m_pImage->GetPropertyItemSize(PropertyTagFrameDelay))
	{
		m_pItem = (Gdiplus::PropertyItem*)new char[TotalBuffer]; //-V121
		memset(m_pItem, 0, TotalBuffer); //-V106
		m_dwCurrentFrame = 0;
		m_dwWidth  = 0;
		m_dwHeight = 0;
		if (!m_pImage->GetPropertyItem(PropertyTagFrameDelay, TotalBuffer, m_pItem))
		{
			if (SelectActiveFrame(m_dwCurrentFrame))
				if (const DWORD dwFrameCount = GetFrameCount())
				{
					// [!] brain-ripper
					// GDI+ seemed to have bug,
					// and for some GIF's returned zero delay for all frames.
					// Make such a workaround: take 50ms delay on every frame
					bool bHaveDelay = false;
					for (DWORD i = 0; i < dwFrameCount; i++)
					{
						if (GetFrameDelay(i) > 0)
						{
							bHaveDelay = true;
							break;
						}
					}
					if (!bHaveDelay)
					{
						for (DWORD i = 0; i < dwFrameCount; i++)
						{
							((UINT*)m_pItem[0].value)[i] = 5; //-V108
						}
					}
				}
			// [!] brain-ripper
			// Strange bug - m_pImage->GetWidth() and m_pImage->GetHeight()
			// sometimes returns zero, even object successfuly loaded.
			// Cache this values here to return correct values later
			m_dwWidth = m_pImage->GetWidth();
			m_dwHeight = m_pImage->GetHeight();
		}
		else
		{
			cleanup();
		}
	}
}
CGDIImage::~CGDIImage()
{
	_ASSERTE(m_Callbacks.empty());
	if (m_hTimer)
	{
			EnterCriticalSection(&m_csCallback);
			m_allowCreateTimer = false;
			if (!DeleteTimerQueueTimer(NULL, m_hTimer, INVALID_HANDLE_VALUE))
			{
				auto l_code = GetLastError();
				if (l_code != ERROR_IO_PENDING)
				{
					dcassert(0);
				}
			}
			m_hTimer = NULL;
			LeaveCriticalSection(&m_csCallback);
	}
	safe_delete(m_pImage);
	cleanup();
	DeleteCriticalSection(&m_csCallback);
}

void CGDIImage::Draw(HDC hDC, int xDst, int yDst, int wDst, int hDst, int xSrc, int ySrc, HDC hBackDC, int xBk, int yBk, int wBk, int hBk)
{
	dcassert(!isShutdown());
	if (hBackDC)
	{
		BitBlt(hBackDC, xBk, yBk, wBk, hBk, NULL, 0, 0, PATCOPY);
		
		Gdiplus::Graphics Graph(hBackDC);
		Graph.DrawImage(m_pImage,
		                xBk,
		                yBk,
		                xSrc, ySrc,
		                wBk, hBk,
		                Gdiplus::UnitPixel);
		                
		BitBlt(hDC, xDst, yDst, wDst, hDst, hBackDC, 0, 0, SRCCOPY);
	}
	else
	{
		Gdiplus::Graphics Graph(hDC);
		Graph.DrawImage(m_pImage,
		                xDst,
		                yDst,
		                0, 0,
		                wDst, hDst,
		                Gdiplus::UnitPixel);
	}
}

DWORD CGDIImage::GetFrameDelay(DWORD dwFrame)
{
	if (m_pItem)
		return ((UINT*)m_pItem[0].value)[dwFrame] * 10; //-V108
	else
		return 5;
}

bool CGDIImage::SelectActiveFrame(DWORD dwFrame)
{
	dcassert(!isShutdown());
	if (!isShutdown())
	{
			static const GUID g_Guid = Gdiplus::FrameDimensionTime;
			if (m_pImage) // crash https://drdump.com/DumpGroup.aspx?DumpGroupID=230505&Login=guest
				m_pImage->SelectActiveFrame(&g_Guid, dwFrame); // [1] https://www.box.net/shared/x4tgntvw818gzd274nek
			return true;
			/* [!]TODO
			if(m_pImage)
			return m_pImage->SelectActiveFrame(&g_Guid, dwFrame) == Ok;
			else
			return false;
			*/
	}
	else
	{
		return false;
	}
}

DWORD CGDIImage::GetFrameCount()
{
	dcassert(!isShutdown());
	if (m_dwFramesCount == 0)
	{
		//First of all we should get the number of frame dimensions
		//Images considered by GDI+ as:
		//frames[animation_frame_index][how_many_animation];
		if (const UINT count = m_pImage->GetFrameDimensionsCount())
		{
			//Now we should get the identifiers for the frame dimensions
			std::vector<GUID> l_pDimensionIDs(count); //-V121
			m_pImage->GetFrameDimensionsList(&l_pDimensionIDs[0], count);
			//For gif image , we only care about animation set#0
			m_dwFramesCount = m_pImage->GetFrameCount(&l_pDimensionIDs[0]);
		}
	}
	return m_dwFramesCount;
}

DWORD CGDIImage::GetWidth()
{
	return m_dwWidth;
}

DWORD CGDIImage::GetHeight()
{
	return m_dwHeight;
}

void CGDIImage::DrawFrame()
{
	dcassert(!isShutdown());

	EnterCriticalSection(&m_csCallback); // crash-full-r501-build-9869.dmp
	static int g_count = 0;
	for (auto i = m_Callbacks.cbegin(); i != m_Callbacks.cend(); ++i)
	{
/*        dcdebug("CGDIImage::DrawFrame  this = %p m_Callbacks.size() = %d m_Callbacks i = %p g_count = %d\r\n",
			this, 
			m_Callbacks.size(), 
			i->lParam,
			++g_count);
*/
		if (!i->pOnFrameChangedProc(this, i->lParam))
		{
			i = m_Callbacks.erase(i);
			if (i == m_Callbacks.end())
				break;
		}
	}
	LeaveCriticalSection(&m_csCallback);
}
void CGDIImage::destroyTimer(CGDIImage *pGDIImage, HANDLE p_CompletionEvent)
{
	EnterCriticalSection(&pGDIImage->m_csCallback);
	if (pGDIImage->m_hTimer)
	{
		pGDIImage->m_allowCreateTimer = false;
		if(!DeleteTimerQueueTimer(NULL, pGDIImage->m_hTimer, p_CompletionEvent))
		{
			auto l_code = GetLastError();
			if (l_code != ERROR_IO_PENDING)
			{
				dcassert(0);
			}
		}
		pGDIImage->m_hTimer = NULL;
	}
	LeaveCriticalSection(&pGDIImage->m_csCallback); // 
}
VOID CALLBACK CGDIImage::OnTimer(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	CGDIImage *pGDIImage = (CGDIImage *)lpParameter;
	if (pGDIImage)
	{
		if(isShutdown())
		{
			destroyTimer(pGDIImage,NULL);
			return;
		}
#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
			if(isGDIImageLive(pGDIImage)) // fix http://code.google.com/p/flylinkdc/issues/detail?id=1255
			{
#endif
		if (pGDIImage->SelectActiveFrame(pGDIImage->m_dwCurrentFrame)) //Change Active frame
		{
			DWORD dwDelay = pGDIImage->GetFrameDelay(pGDIImage->m_dwCurrentFrame);
			if (dwDelay == 0)
				dwDelay++;
			destroyTimer(pGDIImage,NULL);
			if (pGDIImage->m_hCallbackWnd)
			{
				// We should call DrawFrame in context of window thread
				SendMessage(pGDIImage->m_hCallbackWnd, pGDIImage->m_dwCallbackMsg, 0, (LPARAM)pGDIImage);
			}
			else
			{
					pGDIImage->DrawFrame();
			}
#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
			if(isGDIImageLive(pGDIImage)) // fix http://code.google.com/p/flylinkdc/issues/detail?id=1255
			{
#endif
			EnterCriticalSection(&pGDIImage->m_csCallback); 
			// [!] IRainman fix.
			// [-] pGDIImage->m_hTimer = NULL;
			pGDIImage->m_allowCreateTimer = true;
			// [~] IRainman fix.
			if (!pGDIImage->m_Callbacks.empty())
			{
				dcassert(!isShutdown());
				if (!isShutdown())
				{
					CreateTimerQueueTimer(&pGDIImage->m_hTimer, NULL, OnTimer, pGDIImage, dwDelay, 0, WT_EXECUTEDEFAULT); // TODO - разрушать все таймера при стопе
				}
			}
			LeaveCriticalSection(&pGDIImage->m_csCallback);
			// Move to the next frame
			if (pGDIImage->m_dwFramesCount)
			{
				pGDIImage->m_dwCurrentFrame++;
				pGDIImage->m_dwCurrentFrame %= pGDIImage->m_dwFramesCount;
			}
#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
		}
#endif
}
#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
}
#endif
}
}

void CGDIImage::RegisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam)
{
	dcassert(!isShutdown());

	if (GetFrameCount() > 1)
	{
		EnterCriticalSection(&m_csCallback);
		m_Callbacks.insert(CALLBACK_STRUCT(pOnFrameChangedProc, lParam));
		if (!m_hTimer && m_allowCreateTimer ) // [+] IRainman fix.
		{
				CreateTimerQueueTimer(&m_hTimer, NULL, OnTimer, this, 0, 0, WT_EXECUTEDEFAULT); // TODO - разрушать все таймера при стопе
		}
		calcStatisticsL();
		LeaveCriticalSection(&m_csCallback);
	}
}

void CGDIImage::UnregisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam)
{
	if(isShutdown())
	{
		EnterCriticalSection(&m_csCallback);
		m_Callbacks.clear(); // TODO - часто зовется на пустой коллекции при наличии нескольки смайлов в чатах.
		LeaveCriticalSection(&m_csCallback);
	}
	else
	{
	if (GetFrameCount() > 1)
	{
		EnterCriticalSection(&m_csCallback);
		tCALLBACK::iterator i = m_Callbacks.find(CALLBACK_STRUCT(pOnFrameChangedProc, lParam));
		if (i != m_Callbacks.end())
			{
				m_Callbacks.erase(i);
				calcStatisticsL();
			}
		LeaveCriticalSection(&m_csCallback);
		}
	}
}

HDC CGDIImage::CreateBackDC(COLORREF clrBack, int iPaddingW, int iPaddingH)
{
	dcassert(!isShutdown());
	HDC hRetDC = NULL;
	if (m_pImage)
	{
		const HDC hDC = ::GetDC(NULL);
		if (hDC)
		{
			hRetDC = CreateCompatibleDC(hDC);
			if (hRetDC)
			{
				::SaveDC(hRetDC);
				const HBITMAP hBitmap = CreateCompatibleBitmap(hDC, m_pImage->GetWidth() + iPaddingW, m_pImage->GetHeight() + iPaddingH);
				if (hBitmap)
				{
					SelectObject(hRetDC, hBitmap);
					const HBRUSH hBrush = CreateSolidBrush(clrBack);
					if (hBrush)
					{
						SelectObject(hRetDC, hBrush);
						int l_res = ::ReleaseDC(NULL, hDC);
						ATLASSERT(l_res);
						BitBlt(hRetDC, 0, 0, m_pImage->GetWidth() + iPaddingW, m_pImage->GetHeight() + iPaddingH, NULL, 0, 0, PATCOPY);
					}
				}
			}
		}
	}
	
	return hRetDC;
}

void CGDIImage::DeleteBackDC(HDC hBackDC)
{
	HBITMAP hBmp = (HBITMAP)GetCurrentObject(hBackDC, OBJ_BITMAP);
	HBRUSH hBrush = (HBRUSH)GetCurrentObject(hBackDC, OBJ_BRUSH);
	
	RestoreDC(hBackDC, -1);
	
	DeleteDC(hBackDC);
	DeleteObject(hBmp);
	DeleteObject(hBrush);
}

CGDIImage *CGDIImage::CreateInstance(LPCWSTR pszFileName, HWND hCallbackWnd, DWORD dwCallbackMsg)
{
	CGDIImage* l_image = new CGDIImage(pszFileName, hCallbackWnd, dwCallbackMsg);
#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
	CFlyWriteLock(*g_GDIcs);
	g_GDIImageSet.insert(l_image);
#endif
	return l_image;
}
#endif // IRAINMAN_INCLUDE_GDI_OLE