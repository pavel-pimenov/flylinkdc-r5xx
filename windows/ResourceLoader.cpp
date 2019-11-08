#include "stdafx.h"

#include "ResourceLoader.h"
#include "resource.h"
#include "File.h"
#include "../FlyFeatures/ThemeManager.h"


ExCImage::ExCImage(LPCTSTR pszFileName):
    m_hBuffer(nullptr)
{
    Load(pszFileName);
}
ExCImage::ExCImage(UINT id, LPCTSTR pType /*= RT_RCDATA*/):
    m_hBuffer(nullptr)
{
    LoadFromResource(id, pType, ThemeManager::getResourceLibInstance());
}
ExCImage::ExCImage(UINT id, UINT type):
    m_hBuffer(nullptr)
{
    LoadFromResource(id, MAKEINTRESOURCE(type));
}

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

bool ExCImage::LoadFromResourcePNG(UINT id)
{
	return LoadFromResource(id, _T("PNG"));
}

bool ExCImage::LoadFromResource(UINT id, LPCTSTR pType, HMODULE hInst)
{
	HRESULT res = E_FAIL;
	dcassert(m_hBuffer == nullptr);
	if (id == IDR_FLYLINK_PNG)
	{
		if (File::isExist(Util::getExternalLogoPath()))
		{
			res = Load(Text::toT(Util::getExternalLogoPath()).c_str());
			return res == S_OK;
		}
	}
	HRSRC hResource = ::FindResource(hInst, MAKEINTRESOURCE(id), pType);
	if (!hResource)
	{
#if defined(USE_THEME_MANAGER)
		hInst = nullptr; // [!] SSA - try to search in original resource
		hResource = ::FindResource(hInst, MAKEINTRESOURCE(id), pType);
		dcassert(hResource);
		if (!hResource)
#endif
			return false;
	}
	
	DWORD imageSize = ::SizeofResource(hInst, hResource);
	dcassert(imageSize);
	if (!imageSize)
		return false;
		
	const HGLOBAL hGlobal = ::LoadResource(hInst, hResource);
	dcassert(hGlobal);
	if (!hGlobal)
		return false;
		
	const void* pResourceData = ::LockResource(hGlobal);
	dcassert(pResourceData);
	if (!pResourceData)
		return false;
		
	m_hBuffer  = ::GlobalAlloc(GMEM_MOVEABLE, static_cast<size_t>(imageSize));
	dcassert(m_hBuffer);
	if (m_hBuffer)
	{
		void* pBuffer = ::GlobalLock(m_hBuffer);
		dcassert(pBuffer);
		if (pBuffer)
		{
			CopyMemory(pBuffer, pResourceData, static_cast<size_t>(imageSize));
			::GlobalUnlock(m_hBuffer);
			// http://msdn.microsoft.com/en-us/library/windows/desktop/aa378980%28v=vs.85%29.aspx
			IStream* pStream = nullptr;
			const auto l_stream = ::CreateStreamOnHGlobal(m_hBuffer, FALSE, &pStream);
			dcassert(l_stream == S_OK);
			if (l_stream == S_OK)
			{
				res = Load(pStream); //Leak
				safe_release(pStream);
			}
		}
		::GlobalFree(m_hBuffer);
		m_hBuffer = nullptr;
	}
	return res == S_OK;
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
		aImgLst.Create(cx, cy, ILC_COLOR32 | ILC_MASK, imageCount, 1);
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
					originalImgLst.Create(cx, cy, ILC_COLOR32 | ILC_MASK, imageOriginalCount, 1);
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
