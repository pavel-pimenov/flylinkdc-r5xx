// ImageDataObject.h: Impementation for IDataObject Interface to be used
//					     in inserting bitmap to the RichEdit Control.
//
// Author : Hani Atassi  (atassi@arabteam2000.com)
//
// How to use : Just call the static member InsertBitmap with
//				the appropriate parrameters.
//
// Known bugs :
//
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ImageDataObject.h"

// Static member functions
void CImageDataObject::InsertBitmap(HWND hWnd, IRichEditOle* pRichEditOle, IOleClientSite *pOleClientSite, IStorage *pStorage, IOleObject *pOleObject)//IRichEditOle* pRichEditOle, HBITMAP hBitmap, LPCTSTR pszPath)
{
	if (!pOleObject)
		return;
		
	// all items are "contained" -- this makes our reference to this object
	//  weak -- which is needed for links to embedding silent update.
	OleSetContainedObject(pOleObject, TRUE);
	
	// Now Add the object to the RichEdit
	REOBJECT reobject = { 0 };
	reobject.cbStruct = sizeof(REOBJECT);
	
	CLSID clsid;
	HRESULT sc = pOleObject->GetUserClassID(&clsid);
	
	if (sc != S_OK)
	{
		dcdebug("Thrown OLE Exception: %d\n", sc);
		safe_release(pOleObject);
		return;
	}
	
	reobject.clsid = clsid;
	reobject.cp = REO_CP_SELECTION;
	reobject.dvaspect = DVASPECT_CONTENT;
	reobject.dwFlags = REO_BELOWBASELINE;
	reobject.poleobj = pOleObject;
	reobject.polesite = pOleClientSite;
	reobject.pstg = pStorage;
	
	// Insert the bitmap at the current location in the richedit control
	pRichEditOle->InsertObject(&reobject);
	
	::SendMessage(hWnd, EM_SCROLLCARET, (WPARAM)0, (LPARAM)0);
	
	// Release all unnecessary interfaces
	safe_release(pOleObject);
	safe_release(pOleClientSite);
	
	// [-] brain-ripper
	// Don't release follow objects,
	// they must be released in parent's destructor
	//
	//lpLockBytes->Release();
	//pStorage->Release();
	//pRichEditOle->Release();
	//DeleteObject(hBitmap);
}

void CImageDataObject::SetBitmap(HBITMAP hBitmap)
{
	dcassert(hBitmap);
	
	STGMEDIUM stgm = {0};
	stgm.tymed = TYMED_GDI;
	stgm.hBitmap = hBitmap;
	stgm.pUnkForRelease = NULL;
	
	FORMATETC fm = {0};
	fm.cfFormat = CF_BITMAP;
	fm.ptd = NULL;
	fm.dwAspect = DVASPECT_CONTENT;
	fm.lindex = -1;
	fm.tymed = TYMED_GDI;
	
	SetData(&fm, &stgm, TRUE);
}

IOleObject *CImageDataObject::GetOleObject(IOleClientSite *pOleClientSite, IStorage *pStorage)
{
	dcassert(m_stgmed.hBitmap);
	
	IOleObject *pOleObject = nullptr;
	
	SCODE sc = ::OleCreateStaticFromData(this, IID_IOleObject, OLERENDER_FORMAT,
	                                     &m_fromat, pOleClientSite, pStorage, (void **) & pOleObject);
	                                     
	if (sc != S_OK)
	{
		dcdebug("Thrown OLE Exception: %d\n", sc);
		return nullptr;
	}
	return pOleObject;
}
