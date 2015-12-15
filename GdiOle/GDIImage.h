#pragma once
#ifdef IRAINMAN_INCLUDE_GDI_OLE
#include <gdiplus.h>
#include "../client/util_flylinkdc.h"
#include <boost/noncopyable.hpp>
#include <boost/atomic.hpp>
#include <unordered_set>
#include <set>
#include <memory>
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"


#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
#include "../client/CFlyThread.h"
#include "../client/debug.h"
#endif // FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE

class CGDIImage
#ifdef _DEBUG
	: public boost::noncopyable // [+] IRainman fix.
#endif
{
	    friend class MainFrame;
		typedef bool (__cdecl *ONFRAMECHANGED)(CGDIImage *pImage, LPARAM lParam);
		
		Gdiplus::Image *m_pImage;
		Gdiplus::PropertyItem* m_pItem;
		DWORD m_dwWidth;
		DWORD m_dwHeight;
		DWORD m_dwFramesCount;
		DWORD m_dwCurrentFrame;
		HANDLE m_hTimer;
		volatile LONG m_lRef;
		boost::atomic_bool m_allowCreateTimer; // [+] IRainman fix.
		
		static VOID CALLBACK OnTimer(PVOID lpParameter, BOOLEAN TimerOrWaitFired);
		static void destroyTimer(CGDIImage *pGDIImage, HANDLE p_CompletionEvent);
		
		struct CALLBACK_STRUCT
		{
			CALLBACK_STRUCT(ONFRAMECHANGED _pOnFrameChangedProc, LPARAM _lParam):
				pOnFrameChangedProc(_pOnFrameChangedProc),
				lParam(_lParam)
			{}
			
			ONFRAMECHANGED pOnFrameChangedProc;
			LPARAM lParam;
			
			bool operator<(const CALLBACK_STRUCT &cb) const
			{
				return (pOnFrameChangedProc < cb.pOnFrameChangedProc || (pOnFrameChangedProc == cb.pOnFrameChangedProc && lParam < cb.lParam));
			}
		};
		
		CRITICAL_SECTION m_csCallback;
		typedef std::set<CALLBACK_STRUCT> tCALLBACK;
		tCALLBACK m_Callbacks;
		HWND m_hCallbackWnd;
		DWORD m_dwCallbackMsg;
#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
		static std::unique_ptr<webrtc::RWLockWrapper> g_GDIcs;
		static std::unordered_set<CGDIImage*> g_GDIImageSet;
		friend class CFlyServerJSON;
		friend class CGDIImageOle;
		static unsigned g_AnimationDeathDetectCount;
		static unsigned g_AnimationCount;
		static unsigned g_AnimationCountMax;
		void calcStatisticsL() const
		{
			g_AnimationCount = m_Callbacks.size();
			if (g_AnimationCount > g_AnimationCountMax)
			{
				g_AnimationCountMax = g_AnimationCount;
			}
		}

		static bool isGDIImageLive(CGDIImage* p_image)
		{
			if (isShutdown())
			{
				return false;
			}
			CFlyReadLock(*g_GDIcs);
			const bool l_res = g_GDIImageSet.find(p_image) != g_GDIImageSet.end();
			if (!l_res)
			{
				dcassert(0);
				++g_AnimationDeathDetectCount;
			}
			return l_res;
		}
		static void GDIImageDeath(CGDIImage* p_image)
		{
			CFlyWriteLock(*g_GDIcs);
			const auto l_size = g_GDIImageSet.size();
			g_GDIImageSet.erase(p_image);
			dcassert(g_GDIImageSet.size() == l_size-1);
		}
#endif // FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
		
		CGDIImage(LPCWSTR pszFileName, HWND hCallbackWnd, DWORD dwCallbackMsg);
		~CGDIImage();
		
	public:
		static bool isShutdown()
		{
			extern bool g_isShutdown;
			return g_isShutdown;
		}
		static CGDIImage *CreateInstance(LPCWSTR pszFileName, HWND hCallbackWnd, DWORD dwCallbackMsg);
		bool IsInited() const
		{
			return m_pImage != nullptr;
		}
		void cleanup()
		{
			delete [](char*) m_pItem;  
			m_pItem = 0;
		}
		
		void Draw(HDC hDC, int xDst, int yDst, int wDst, int hDst, int xSrc, int ySrc, HDC hBackDC, int xBk, int yBk, int wBk, int hBk);
		void DrawFrame();
		
		DWORD GetFrameDelay(DWORD dwFrame);
		bool SelectActiveFrame(DWORD dwFrame);
		DWORD GetFrameCount();
		DWORD GetWidth();
		DWORD GetHeight();
		
		void RegisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam);
		void UnregisterCallback(ONFRAMECHANGED pOnFrameChangedProc, LPARAM lParam);
		
		HDC CreateBackDC(COLORREF clrBack, int iPaddingW, int iPaddingH);
		void DeleteBackDC(HDC hBackDC);
		
		LONG AddRef()
		{
			return InterlockedIncrement(&m_lRef);
		}
		
		LONG Release()
		{
			const LONG lRef = InterlockedDecrement(&m_lRef);
			
			if (lRef == 0)
			{
#ifdef FLYLINKDC_USE_CHECK_GDIIMAGE_LIVE
				GDIImageDeath(this);
#endif 
				delete this; // [39] https://www.box.net/shared/05cc9b528dc37cc78229
			}
			
			return lRef;
		}
};
#endif // IRAINMAN_INCLUDE_GDI_OLE