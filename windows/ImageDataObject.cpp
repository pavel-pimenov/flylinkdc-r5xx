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
#include "../client/LogManager.h"

// Static member functions
void CImageDataObject::InsertBitmap(HWND hWnd, IRichEditOle* pRichEditOle, IOleClientSite *pOleClientSite, IStorage *pStorage, IOleObject *pOleObject, bool& p_out_of_memory)//IRichEditOle* pRichEditOle, HBITMAP hBitmap, LPCTSTR pszPath)
{
	p_out_of_memory = false;
	if (!pOleObject)
		return;
		
	HRESULT sc = OleSetContainedObject(pOleObject, TRUE);
	// all items are "contained" -- this makes our reference to this object
	//  weak -- which is needed for links to embedding silent update.
	if (sc != S_OK)
	{
		LogManager::message("CImageDataObject::InsertBitmap, OLE OleSetContainedObject error = " + Util::toString(sc) + " GetLastError() = " + Util::toString(GetLastError()));
		p_out_of_memory = sc == E_OUTOFMEMORY;
		return;
	}
	
	// Now Add the object to the RichEdit
	REOBJECT reobject = { 0 };
	reobject.cbStruct = sizeof(REOBJECT);
	
	CLSID clsid;
	sc = pOleObject->GetUserClassID(&clsid);
	
	if (sc != S_OK)
	{
		dcdebug("Thrown OLE Exception: %d\n", sc);
		LogManager::message("CImageDataObject::InsertBitmap, OLE GetUserClassID error = " + Util::toString(sc) + " GetLastError() = " + Util::toString(GetLastError()));
		dcassert(0);
		safe_release(pOleObject);
		safe_release(pOleClientSite);
		p_out_of_memory = sc == E_OUTOFMEMORY;
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
	sc = pRichEditOle->InsertObject(&reobject);
	if (sc != S_OK)
	{
		dcdebug("Thrown OLE InsertObject: %d\n", sc);
		LogManager::message("CImageDataObject::InsertBitmap, OLE InsertObject error = " + Util::toString(sc) + " GetLastError() = " + Util::toString(GetLastError()));
		dcassert(0);
		safe_release(pOleObject);
		safe_release(pOleClientSite);
		p_out_of_memory = sc == E_OUTOFMEMORY;
		return;
	}
	dcassert(::IsWindow(hWnd));
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
	
	const SCODE sc = ::OleCreateStaticFromData(this, IID_IOleObject, OLERENDER_FORMAT,
	                                           &m_fromat, pOleClientSite, pStorage, (void **) & pOleObject);
	                                           
	if (sc != S_OK)
	{
		dcdebug("Thrown OLE Exception: %d\n", sc);
		dcassert(0);
		LogManager::message("CImageDataObject::GetOleObject, OleCreateStaticFromData error = " + Util::toString(sc) + " GetLastError() = " + Util::toString(GetLastError()));
		return nullptr;
	}
	return pOleObject;
}
