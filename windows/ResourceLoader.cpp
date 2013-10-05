#include "stdafx.h"

#include "ResourceLoader.h"

void ExCImage::Destroy() noexcept
{
	CImage::Destroy();
	if (m_hBuffer)
	{
		::GlobalUnlock(m_hBuffer);
		::GlobalFree(m_hBuffer);
		m_hBuffer = nullptr;
	}
}

bool ExCImage::LoadFromResource(UINT id, LPCTSTR pType, HMODULE hInst
                                // [-], bool useDefaultHINST [-] IRainman
                               ) noexcept
{
	_ASSERTE(m_hBuffer == nullptr);
	
	HRSRC hResource = ::FindResource(hInst, MAKEINTRESOURCE(id), pType);
	if (!hResource)
	{
#if defined(USE_THEME_MANAGER)
		hInst = nullptr; // [!] SSA - try to search in original resource
		hResource = ::FindResource(hInst, MAKEINTRESOURCE(id), pType);
		if (!hResource)
#endif
			return false;
	}
	
	DWORD imageSize = ::SizeofResource(hInst, hResource);
	if (!imageSize)
		return false;
		
	const HGLOBAL hGlobal = ::LoadResource(hInst, hResource);
	if (!hGlobal)
		return false;
		
	const void* pResourceData = ::LockResource(hGlobal);
	if (!pResourceData)
		return false;
		
	HRESULT res = E_FAIL;
	m_hBuffer  = ::GlobalAlloc(GMEM_MOVEABLE, static_cast<size_t>(imageSize));
	if (m_hBuffer)
	{
		void* pBuffer = ::GlobalLock(m_hBuffer);
		if (pBuffer)
		{
			CopyMemory(pBuffer, pResourceData, static_cast<size_t>(imageSize));
			
			IStream* pStream = nullptr;
			if (::CreateStreamOnHGlobal(m_hBuffer, FALSE, &pStream) == S_OK)
			{
				res = Load(pStream);
				safe_release(pStream);
			}
			::GlobalUnlock(m_hBuffer);
		}
		::GlobalFree(m_hBuffer);
		m_hBuffer = nullptr;
	}
	return (res == S_OK);
}

int ResourceLoader::LoadImageList(LPCTSTR pszFileName, CImageList& aImgLst, int cx, int cy)
{
	if (cx <= 0 || cy <= 0)
		return 0;
	ExCImage img;
	img.Load(pszFileName);
	aImgLst.Create(cx, cy, ILC_COLOR32 | ILC_MASK, img.GetWidth() / cy, 0);
	aImgLst.Add(img, img.GetPixel(0, 0));
	img.Destroy();
	return aImgLst.GetImageCount();
}

int ResourceLoader::LoadImageList(UINT id, CImageList& aImgLst, int cx, int cy)
{
	int imageCount = 0;
	if (cx <= 0 || cy <= 0)
		return imageCount;
	ExCImage img;
	if (img.LoadFromResource(id, _T("PNG")))
	{
		imageCount = img.GetWidth() / cx;
		aImgLst.Create(cx, cy, ILC_COLOR32 | ILC_MASK, imageCount , 1);
		aImgLst.Add(img);
		img.Destroy();
#if defined(USE_THEME_MANAGER)
		if (ThemeManager::isResourceLibLoaded() && imageCount > 0)
		{
			// Only for Not original images -- load
			ExCImage img2;
			if (img2.LoadFromResource(id, _T("PNG"), nullptr))
			{
				const int imageOriginalCount = img2.GetWidth() / cx;
				dcassert(imageOriginalCount == imageCount);
				if (imageOriginalCount > imageCount)
				{
					CImageList originalImgLst;
					originalImgLst.Create(cx, cy, ILC_COLOR32 | ILC_MASK, imageOriginalCount , 1);
					originalImgLst.Add(img2);
//						int iAddedIcon = 0;
//						for (int i = imageCount; i<imageOriginalCount; i++)
//						{
//#if defined(ExtractIcon)
//#undef ExtractIcon
//							iAddedIcon = aImgLst.ReplaceIcon( -1, aOriginalImgLst.ExtractIcon(i));
//#endif
//						}
//						aOriginalImgLst.Destroy();

					aImgLst.Destroy();
					std::swap(aImgLst, originalImgLst);
				}
				img2.Destroy();
			}
		}
#endif
	}
	return aImgLst.GetImageCount();
}
