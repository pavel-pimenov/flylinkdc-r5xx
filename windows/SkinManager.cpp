#include "stdafx.h"
#include "SkinableCmdBar.h"
#include "TabCtrlSharedInterface.h"
#include "Winutil.h"
#include "SkinManager.h"

#ifdef RIP_USE_SKIN

#include "../XMLParser/xmlParser.h"

static HBITMAP LoadImage(XMLParser::XMLNode &xNode, LPCWSTR pszAttrib, std::wstring &strCurDir)
{
	LPCWSTR pszText = xNode.getAttribute(pszAttrib);
	HBITMAP hBMP = NULL;
	
	if (pszText)
	{
		size_t szCurDirLen = strCurDir.size();
		strCurDir += pszText;
		hBMP = pszText ? (HBITMAP)LoadImage(NULL, strCurDir.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE) : NULL;
		strCurDir.resize(szCurDirLen);
	}
	
	return hBMP;
}

static BYTE GetStyle(DWORD dwCmd)
{
	BYTE bStyle = BS_PUSHBUTTON;
	for (size_t i = 0; i < _countof(g_ToolbarButtons); i++)
	{
		if ((DWORD)g_ToolbarButtons[i].id == dwCmd)
		{
			if (g_ToolbarButtons[i].check)
			{
				bStyle = BS_CHECKBOX;
				break;
			}
		}
	}
	
	return bStyle;
}

static bool LoadCmdBar(XMLParser::XMLNode &xToolbarNode, std::wstring &strXmlDir, CSkinableCmdBar &cmdBar, HWND hParentWnd)
{
	bool bRet = false;
	if (!xToolbarNode.isEmpty())
	{
		LPCWSTR pszText = xToolbarNode.getAttribute(L"ItemCount");
		DWORD dwCount = _wtol(pszText);
		
		pszText = xToolbarNode.getAttribute(L"Type");
		bool bVert = pszText ? lstrcmpi(pszText, L"Vertical") == 0 : false;
		
		HBITMAP hNormalBMP = LoadImage(xToolbarNode, L"NormalImage", strXmlDir);
		HBITMAP hHilitedBMP = LoadImage(xToolbarNode, L"HilitedImage", strXmlDir);
		HBITMAP hSelectedBMP = LoadImage(xToolbarNode, L"SelectedImage", strXmlDir);
		HBITMAP hSelectedHilitedBMP = LoadImage(xToolbarNode, L"SelectedHilitedImage", strXmlDir);
		HBITMAP hArrowsBMP = LoadImage(xToolbarNode, L"ArrowsImage", strXmlDir);
		
		pszText = xToolbarNode.getAttribute(L"BkgColor");
		COLORREF clrBkg = 0;
		if (pszText)
		{
			if (*pszText == L'#')
				clrBkg = wscanf(L"%x", &pszText[1]);
			else
				clrBkg = _wtol(pszText);
		}
		
		bRet = cmdBar.Create(hParentWnd, 0, 0, 0, bVert ? CSkinableCmdBar::TYPE_VERTICAL : CSkinableCmdBar::TYPE_HORIZONTAL, dwCount, hNormalBMP, hHilitedBMP, hSelectedBMP, hSelectedHilitedBMP, hArrowsBMP, clrBkg);
		
		/*
		if (bRet)
		{
		    int i = 0;
		
		    XMLParser::XMLNode xItemNode = xToolbarNode.getChildNode(L"Item", &i);
		
		    while (!xItemNode.isEmpty())
		    {
		        pszText = xItemNode.getAttribute(L"Cmd");
		
		        DWORD dwCmd = 0;
		        if (!pszText)
		        {
		            pszText = xItemNode.getAttribute(L"URL");
		        }
		        else
		        {
		            dwCmd = _wtol(pszText);
		        }
		
		        if (pszText)
		        {
		            pszText = xItemNode.getAttribute(L"ImageID");
		
		            if (pszText)
		            {
		                DWORD dwImageID = _wtol(pszText);
		
		                BYTE bStyle = GetStyle(dwCmd);
		
		                cmdBar.AddItem(dwImageID, dwCmd, bStyle);
		            }
		        }
		        xItemNode = xToolbarNode.getChildNode(L"Item", &i);
		    }
		}
		*/
	}
	
	return bRet;
}

bool CSkinManager::LoadSkin(HWND hParentWnd, LPCWSTR pszSkinXML)
{
	bool bRet = false;
	XMLParser::XMLResults xRes;
	
	XMLParser::XMLNode xRootNode = XMLParser::XMLNode::parseFile(pszSkinXML, L"Skin", &xRes);
	
	std::wstring strXmlDir(pszSkinXML);
	size_t n = strXmlDir.find_last_of(L'\\');
	
	if (n != strXmlDir.npos)
		strXmlDir.erase(strXmlDir.begin() + n, strXmlDir.end());
		
	strXmlDir += L'\\';
	
	if (xRes.error == XMLParser::eXMLErrorNone)
	{
		XMLParser::XMLNode xToolbarNode = xRootNode.getChildNode(L"Toolbar");
		bool ret = LoadCmdBar(xToolbarNode, strXmlDir, m_ctrlToolbar, hParentWnd);
		
		if (ret)
		{
			bRet = true;
			int i = 0;
			
			XMLParser::XMLNode xItemNode = xToolbarNode.getChildNode(L"Item", &i);
			
			while (!xItemNode.isEmpty())
			{
				LPCWSTR pszText = xItemNode.getAttribute(L"Cmd");
				
				if (pszText)
				{
					DWORD dwCmd = _wtol(pszText);
					pszText = xItemNode.getAttribute(L"ImageID");
					if (pszText)
					{
						DWORD dwImageID = _wtol(pszText);
						BYTE bStyle = GetStyle(dwCmd);
						m_ctrlToolbar.AddItem(dwImageID, dwCmd, bStyle);
					}
				}
				xItemNode = xToolbarNode.getChildNode(L"Item", &i);
			}
		}
		xToolbarNode = xRootNode.getChildNode(L"Bannerbar");
		ret = LoadCmdBar(xToolbarNode, strXmlDir, m_ctrlBannerbar, hParentWnd);
		
		if (ret)
		{
			bRet = true;
			int i = 0;
			
			XMLParser::XMLNode xItemNode = xToolbarNode.getChildNode(L"Item", &i);
			
			while (!xItemNode.isEmpty())
			{
				LPCWSTR pszURL = xItemNode.getAttribute(L"URL");
				
				if (pszURL)
				{
					LPCWSTR pszText = xItemNode.getAttribute(L"ImageID");
					if (pszText)
					{
						DWORD dwImageID = _wtol(pszText);
						m_ctrlBannerbar.AddBanner(dwImageID, pszURL);
					}
				}
				xItemNode = xToolbarNode.getChildNode(L"Item", &i);
			}
		}
	}
	
	return bRet;
}

void CSkinManager::OnSize(RECT *rect)
{
	if (HaveToolbar())
	{
		RECT BarRect;
		::GetClientRect(m_ctrlToolbar, &BarRect);
		rect->left += BarRect.right - BarRect.left;
		::SetWindowPos(m_ctrlToolbar, NULL, 0, rect->top, BarRect.right - BarRect.left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOOWNERZORDER);
	}
	
	if (HaveBannerbar())
	{
		RECT BarRect;
		::GetClientRect(m_ctrlBannerbar, &BarRect);
		rect->right -= BarRect.right - BarRect.left;
		::SetWindowPos(m_ctrlBannerbar, NULL, rect->right, rect->top, BarRect.right - BarRect.left, rect->bottom - rect->top, SWP_NOZORDER | SWP_NOOWNERZORDER);
	}
}

bool CSkinManager::HaveToolbar()
{
	return (HWND)m_ctrlToolbar != NULL;
}

HWND CSkinManager::GetToolbarHandle()
{
	return m_ctrlToolbar;
}

bool CSkinManager::HaveBannerbar()
{
	return (HWND)m_ctrlBannerbar != NULL;
}

HWND CSkinManager::GetBannerHandle()
{
	return m_ctrlBannerbar;
}

bool CSkinManager::HaveTabbar()
{
	return false;
}

ITabCtrl *CSkinManager::GetTabbarCtrl()
{
	return nullptr;
}
#endif
