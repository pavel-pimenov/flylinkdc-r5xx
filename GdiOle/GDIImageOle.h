// GDIImage.h : Declaration of the CGDIImageOle
#pragma once
#ifdef IRAINMAN_INCLUDE_GDI_OLE
#include <algorithm>
#include "resource.h"       // main symbols
#include <atlctl.h>
#include "GdiOle_i.h"
#include "GdiImage.h"

#if defined(_WIN32_WCE) && !defined(_CE_DCOM) && !defined(_CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA)
#error "Single-threaded COM objects are not properly supported on Windows CE platform, such as the Windows Mobile platforms that do not include full DCOM support. Define _CE_ALLOW_SINGLE_THREADED_OBJECTS_IN_MTA to force ATL to support creating single-thread COM object's and allow use of it's single-threaded COM object implementations. The threading model in your rgs file was set to 'Free' as that is the only threading model supported in non DCOM Windows CE platforms."
#endif


EXTERN_C const IID IID_IGDIImageDeleteNotify;

class IGDIImageDeleteNotify: public IUnknown
{
	public:
		virtual void SetDelete() = 0;
};

//#define CTRL_WINDOWED

// CGDIImageOle
class ATL_NO_VTABLE CGDIImageOle :
	public CComObjectRootEx<CComSingleThreadModel>,
#ifdef CTRL_WINDOWED
	public IDispatchImpl < IGDIImage, &IID_IGDIImage, &LIBID_GdiOleLib, /*wMajor =*/ 1, /*wMinor =*/ 0 > ,
#else
	public CStockPropImpl<CGDIImageOle, IGDIImage>,
#endif
	public IPersistStreamInitImpl<CGDIImageOle>,
	public IOleControlImpl<CGDIImageOle>,
	public IOleObjectImpl<CGDIImageOle>,
	public IOleInPlaceActiveObjectImpl<CGDIImageOle>,
	public IViewObjectExImpl<CGDIImageOle>,
	public IOleInPlaceObjectWindowlessImpl<CGDIImageOle>,
	public ISupportErrorInfo,
	public IPersistStorageImpl<CGDIImageOle>,
	public ISpecifyPropertyPagesImpl<CGDIImageOle>,
	public IQuickActivateImpl<CGDIImageOle>,
#ifndef _WIN32_WCE
	public IDataObjectImpl<CGDIImageOle>,
#endif
	public IProvideClassInfo2Impl<&CLSID_GDIImage, NULL, &LIBID_GdiOleLib>,
#ifdef _WIN32_WCE // IObjectSafety is required on Windows CE for the control to be loaded correctly
	public IObjectSafetyImpl<CGDIImageOle, INTERFACESAFE_FOR_UNTRUSTED_CALLER>,
#endif
	public CComCoClass<CGDIImageOle, &CLSID_GDIImage>,
#ifdef CTRL_WINDOWED
	public CComCompositeControl<CGDIImageOle>,
#else
	public CComControl<CGDIImageOle>,
#endif
	public IGDIImageDeleteNotify
{
	public:
	
	
		CGDIImageOle();
		
		DECLARE_OLEMISC_STATUS(OLEMISC_RECOMPOSEONRESIZE |
		                       OLEMISC_CANTLINKINSIDE |
		                       OLEMISC_INSIDEOUT |
		                       OLEMISC_ACTIVATEWHENVISIBLE |
		                       OLEMISC_SETCLIENTSITEFIRST
		                      )
		                      
		DECLARE_REGISTRY_RESOURCEID(IDR_GDIIMAGE)
		
		
		BEGIN_COM_MAP(CGDIImageOle)
		COM_INTERFACE_ENTRY(IGDIImage)
		COM_INTERFACE_ENTRY(IDispatch)
		COM_INTERFACE_ENTRY(IViewObjectEx)
		COM_INTERFACE_ENTRY(IViewObject2)
		COM_INTERFACE_ENTRY(IViewObject)
		//COM_INTERFACE_ENTRY_FUNC(IID_IViewObject, 0, QueryViewObject)
		COM_INTERFACE_ENTRY(IOleInPlaceObjectWindowless)
		COM_INTERFACE_ENTRY(IOleInPlaceObject)
		COM_INTERFACE_ENTRY2(IOleWindow, IOleInPlaceObjectWindowless)
		COM_INTERFACE_ENTRY(IOleInPlaceActiveObject)
		COM_INTERFACE_ENTRY(IOleControl)
		COM_INTERFACE_ENTRY(IOleObject)
		COM_INTERFACE_ENTRY(IPersistStreamInit)
		COM_INTERFACE_ENTRY2(IPersist, IPersistStreamInit)
		COM_INTERFACE_ENTRY(ISupportErrorInfo)
		COM_INTERFACE_ENTRY(ISpecifyPropertyPages)
		COM_INTERFACE_ENTRY(IQuickActivate)
		COM_INTERFACE_ENTRY(IPersistStorage)
#ifndef _WIN32_WCE
		COM_INTERFACE_ENTRY(IDataObject)
#endif
		COM_INTERFACE_ENTRY(IProvideClassInfo)
		COM_INTERFACE_ENTRY(IProvideClassInfo2)
#ifdef _WIN32_WCE // IObjectSafety is required on Windows CE for the control to be loaded correctly
		COM_INTERFACE_ENTRY_IID(IID_IObjectSafety, IObjectSafety)
#endif
		COM_INTERFACE_ENTRY_IID(IID_IGDIImageDeleteNotify, IGDIImageDeleteNotify)
		END_COM_MAP()
		
		BEGIN_PROP_MAP(CGDIImageOle)
		PROP_DATA_ENTRY("_cx", m_sizeExtent.cx, VT_UI4)
		PROP_DATA_ENTRY("_cy", m_sizeExtent.cy, VT_UI4)
		// Example entries
		// PROP_ENTRY_TYPE("Property Name", dispid, clsid, vtType)
		// PROP_PAGE(CLSID_StockColorPage)
		END_PROP_MAP()
		
		
		BEGIN_MSG_MAP(CGDIImageOle)
#ifdef CTRL_WINDOWED
		CHAIN_MSG_MAP(CComCompositeControl<CGDIImageOle>)
#else
		CHAIN_MSG_MAP(CComControl<CGDIImageOle>)
		DEFAULT_REFLECTION_HANDLER()
#endif
		END_MSG_MAP()
// Handler prototypes:
//  LRESULT MessageHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
//  LRESULT CommandHandler(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled);
//  LRESULT NotifyHandler(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);

		BEGIN_SINK_MAP(CGDIImageOle)
		//Make sure the Event Handlers have __stdcall calling convention
		END_SINK_MAP()
		
// ISupportsErrorInfo
		STDMETHOD(InterfaceSupportsErrorInfo)(REFIID riid)
		{
			static const IID* arr[] =
			{
				&IID_IGDIImage,
			};
			
			for (int i = 0; i < _countof(arr); i++)
			{
				if (InlineIsEqualGUID(*arr[i], riid))
					return S_OK;
			}
			return S_FALSE;
		}
		
		/*
		STDMETHOD(SetAdvise)(DWORD aspects, DWORD advf, IAdviseSink* pAdvSink)
		{
		    ATLTRACE(atlTraceControls,2,_T("IViewObjectExImpl::SetAdvise\n"));
		
		    EnterCriticalSection(&m_csAdviseSink);
		    m_spAdviseSink = pAdvSink;
		    LeaveCriticalSection(&m_csAdviseSink);
		
		    return S_OK;
		}
		*/
		
		static HRESULT WINAPI QueryViewObject(void* pv, REFIID riid, LPVOID* ppv, DWORD dw);
		LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		
// IViewObjectEx
		DECLARE_VIEW_STATUS(0)
		
// IGDIImage
	public:
		HRESULT OnDraw(ATL_DRAWINFO& di)
		{
			if (!CGDIImage::isShutdown())
			{
				m_pImage->Draw(di.hdcDraw,
				               di.prcBounds->left,
				               di.prcBounds->top,
				               std::min(m_dwW, DWORD(di.prcBounds->right - di.prcBounds->left)),
				               std::min(m_dwH, DWORD(di.prcBounds->bottom - di.prcBounds->top)), 0, 0, m_hBackDC, 0, 0,
				               std::min(m_dwW, DWORD(di.prcBounds->right - di.prcBounds->left)),
				               std::min(m_dwH, DWORD(di.prcBounds->bottom - di.prcBounds->top)));
				               
				if (!m_bRegistered)
				{
					// Object became visible
					m_pImage->RegisterCallback(OnFrameChanged, (LPARAM)this);
					m_bRegistered = true;
				}
			}
			return S_OK;
		}
		
		void SetDelete()
		{
			m_bIsDeleting = true;
		}
		
		enum { IDD = 100 };
		
		DECLARE_PROTECT_FINAL_CONSTRUCT()
		
		HRESULT FinalConstruct();
		
		void FinalRelease();
		
		STDMETHOD(put_SetImage)(CGDIImage *pImage, COLORREF clrBack, HWND hCallbackWnd, DWORD dwUpdateMsg);
		
	private:
		CGDIImage *m_pImage;
		
		UINT m_dwCurrentFrame;
		UINT m_FrameCount;
		DWORD m_dwW;
		DWORD m_dwH;
		
		HWND m_hCallbackWnd;
		DWORD m_dwUpdateMsg;
		
		HDC m_hBackDC;
		
		bool m_bIsDeleting: 1;
		bool m_bRegistered: 1;
		
		//CRITICAL_SECTION m_csAdviseSink;
		
		HRESULT FireViewChangeEx(BOOL bEraseBackground);
		static bool __cdecl OnFrameChanged(CGDIImage *pImage, LPARAM lParam);
	public:
		static HWND g_ActiveMDIWindow;
};

OBJECT_ENTRY_AUTO(__uuidof(GDIImage), CGDIImageOle)

#endif //IRAINMAN_INCLUDE_GDI_OLE