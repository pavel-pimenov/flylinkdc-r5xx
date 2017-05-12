// GDIImage.cpp : Implementation of CGDIImageOle
#include "stdafx.h"
#ifdef IRAINMAN_INCLUDE_GDI_OLE
#include "GDIImageOle.h"

// CGDIImageOle

const IID IID_IGDIImageDeleteNotify = {0xa991582, 0x6f1a, 0x4150, 0xbd, 0xcd, 0x25, 0x3b, 0xb7, 0x8c, 0xf9, 0xf};

HWND CGDIImageOle::g_ActiveMDIWindow = nullptr;

CGDIImageOle::CGDIImageOle():
	m_pImage(nullptr),
	m_FrameCount(0),
	m_hCallbackWnd(nullptr),
	m_dwUpdateMsg(WM_USER),
	m_bIsDeleting(false),
	m_bRegistered(false),
	m_dwCurrentFrame(0),
	m_dwW(0),
	m_dwH(0),
	m_hBackDC(nullptr)
{
	//m_bWindowOnly = TRUE;
	//CalcExtent(m_sizeExtent);
}

HRESULT CGDIImageOle::FinalConstruct()
{
	//InitializeCriticalSection(&m_csAdviseSink);
	return S_OK;
}

void CGDIImageOle::FinalRelease()
{
	if (m_pImage)
	{
		if (m_bRegistered)
		{
			m_pImage->UnregisterCallback(OnFrameChanged, (LPARAM)this);
			m_bRegistered = false;
		}
		m_pImage->DeleteBackDC(m_hBackDC);
		safe_release(m_pImage);
	}
	
	//DeleteCriticalSection(&m_csAdviseSink);
}

enum DefinedTestRectValue {TR_NOOVERLAP, TR_INTERSECT, TR_CONTAIN};

static DefinedTestRectValue TestRectInRect(RECT *pRect1, RECT *pRect2)
{
	RECT rcIntersect;
	DefinedTestRectValue ret = TR_NOOVERLAP;
	
	if (IntersectRect(&rcIntersect, pRect1, pRect2))
	{
		if (EqualRect(&rcIntersect, pRect2))
			ret = TR_CONTAIN;
		else
			ret = TR_INTERSECT;
	}
	
	return ret;
}

HRESULT CGDIImageOle::FireViewChangeEx(BOOL bEraseBackground)
{
	dcassert(!CGDIImage::isShutdown());
	HRESULT Res = S_OK;
	if (m_bInPlaceActive)
	{
		// Active
		if (m_hWndCD != NULL)
			::InvalidateRect(m_hWndCD, NULL, bEraseBackground); // Window based
		else if (m_bWndLess && m_spInPlaceSite != NULL)
			m_spInPlaceSite->InvalidateRect(NULL, bEraseBackground); // Windowless
	}
	else // Inactive
	{
		/*
		if (CComCompositeControl<CGDIImageOle>::m_hWnd)
		    ::InvalidateRect(CComCompositeControl<CGDIImageOle>::m_hWnd, NULL, FALSE);
		else if (m_hCallbackWnd)
		{
		    ::PostMessage(m_hCallbackWnd, m_dwUpdateMsg, 0, (LPARAM)this);
		}
		else
		{
		    SendOnViewChange(DVASPECT_CONTENT);
		}
		*/
		if (m_spClientSite && !m_bIsDeleting)
		{
			/*
			IOleClientSite *pClientSite = m_spClientSite;
			pClientSite->AddRef();
			::PostMessage(m_hCallbackWnd, m_dwUpdateMsg, 0, (LPARAM)pClientSite);
			*/
			CComPtr<IOleInPlaceSite> spInPlaceSite;
			m_spClientSite->QueryInterface(__uuidof(IOleInPlaceSite), (void **)&spInPlaceSite);
			
			HWND hwndParent = NULL;
			
			if (spInPlaceSite &&
			        spInPlaceSite->GetWindow(&hwndParent) == S_OK &&
			        hwndParent &&
			        ::GetParent(hwndParent) == g_ActiveMDIWindow) // Чуток быстрее и проще добраться чем MDIGetActive()
			        
			{
				OLEINPLACEFRAMEINFO frameInfo;
				IOleInPlaceFrame *pInPlaceFrame = nullptr;
				IOleInPlaceUIWindow *pInPlaceUIWindow = nullptr;
				RECT rcPos, rcClip;
				frameInfo.cb = sizeof(OLEINPLACEFRAMEINFO);
				
				if (spInPlaceSite->GetWindowContext(&pInPlaceFrame,
				                                    &pInPlaceUIWindow, &rcPos, &rcClip, &frameInfo) == S_OK)
				{
					safe_release(pInPlaceFrame);
					safe_release(pInPlaceUIWindow);
					
					DefinedTestRectValue TestRect = TestRectInRect(&rcClip, &rcPos);
					
					if (TestRect == TR_CONTAIN)
					{
						// Object fully visible
						SendOnViewChange(DVASPECT_CONTENT);
					}
					else if (TestRect == TR_INTERSECT)
					{
						// Object partially visible
						
						// [!] brain-ripper
						// this implementation flickers smiles
						::InvalidateRect(hwndParent, &rcPos, FALSE);
						
						/*
						// [!] brain-ripper
						// this implementation doesn't respect selection color
						HDC hDC = ::GetDC(hwndParent);
						m_pImage->Draw(hDC,
						    rcPos.left,
						    rcPos.top,
						    min(m_dwW, DWORD(rcPos.right - rcPos.left)),
						    min(m_dwH, DWORD(rcPos.bottom - rcPos.top)), 0, 0, m_hBackDC, 0, 0,
						    min(m_dwW, DWORD(rcPos.right - rcPos.left)),
						    min(m_dwH, DWORD(rcPos.bottom - rcPos.top)));
						int l_res = ::ReleaseDC(hwndParent, hDC);
						dcassert(l_res);
						//*/
					}
					else
					{
						// Object hidden
						if (m_bRegistered)
						{
							// Returning S_FALSE on callback makes
							// pImage to unregister callback function.
							// This mechanism used because it is not allowed Register/Unregister
							// callback functions from inside callback functions
							Res = S_FALSE;
							m_bRegistered = false;
						}
					}
				}
			}
		}
	}
	return Res;
}


STDMETHODIMP CGDIImageOle::put_SetImage(CGDIImage *pImage, COLORREF clrBack, HWND hCallbackWnd, DWORD dwUpdateMsg)
{
	dcassert(!CGDIImage::isShutdown());
	if (m_pImage)
		return S_FALSE;
		
	m_pImage = pImage;
	
	if (!m_pImage)
		return S_FALSE;
		
	m_pImage->AddRef();
	
	m_dwW = m_pImage->GetWidth();
	m_dwH = m_pImage->GetHeight();
	
	SIZEL szPix = {0};
	szPix.cx = m_dwW;
	szPix.cy = m_dwH;
	SIZEL szHi;
	AtlPixelToHiMetric(&szPix, &szHi);
	m_sizeExtent.cx = szHi.cx;
	m_sizeExtent.cy = szHi.cy;
	
	m_hCallbackWnd = hCallbackWnd;
	m_dwUpdateMsg = dwUpdateMsg;
	//m_clrBack = clrBack;
	m_hBackDC = m_pImage->CreateBackDC(clrBack, 0, 0);
	
	m_pImage->RegisterCallback(OnFrameChanged, (LPARAM)this);
	m_bRegistered = true;
	
	return S_OK;
}

bool CGDIImageOle::OnFrameChanged(CGDIImage *pImage, LPARAM lParam)
{
	dcassert(!CGDIImage::isShutdown());
	if (CGDIImage::isShutdown())
		return true;
	CGDIImageOle *pGDIImage = (CGDIImageOle *)lParam;
	return pGDIImage->FireViewChangeEx(FALSE) == S_OK;
}

LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	return S_OK;
}

#endif // IRAINMAN_INCLUDE_GDI_OLE