// XMLMerge.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "XMLMerge.h"
#include "resource.h"
#include "XMLParser\XMLParser.h"

#include <Shellapi.h>

#include <string>
#include <vector>

#define DEFAULTCONVERTC2W(CHAR_BUFF,WCHAR_BUFF, SIZE) MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,(LPCSTR)CHAR_BUFF, (int)SIZE, WCHAR_BUFF, (int)SIZE)

int APIENTRY _tWinMain(HINSTANCE hInstance,
                       HINSTANCE hPrevInstance,
                       LPTSTR    lpCmdLine,
                       int       nCmdShow)
{
	LPCWSTR pszXML = NULL;
	std::vector<LPCWSTR> strUpdateNodeList;
	bool bRet = false;
	enum Flags {Portal, Favorites};
	int Flag = NULL;
	
	int iCount = 0;
	LPCWSTR lszCmdLine = GetCommandLine();
	LPWSTR *pArgs = CommandLineToArgvW(lszCmdLine, &iCount);
	
	for (int i = 0; i < iCount; i++)
	{
		if (pArgs[i][0] == L'-')
		{
			if (pArgs[i][2] == L':')
			{
				if (pArgs[i][1] == L'i')
				{
					strUpdateNodeList.push_back(&pArgs[i][3]);
				}
				else if (pArgs[i][1] == L'o')
				{
					pszXML = &pArgs[i][3];
				}
			}
		}
		else if (pArgs[i][0] == L'/')
		{
			if (pArgs[i][1] == L'p')
			{
				Flag = Portal;
			}
			else if (pArgs[i][1] == L'f')
			{
				Flag = Favorites;
			}
		}
	}
	
	if (!pszXML || strUpdateNodeList.size() == 0)  // [?] SSA  - Flag == NULL is PortalBrowser by Default||Flag == NULL)
		return 2;
		
	XMLParser::XMLResults xRes;
	XMLParser::XMLNode xStubRootNode;
	if (Flag == Portal)
		xStubRootNode = XMLParser::XMLNode::parseFile(pszXML, L"PortalBrowser", &xRes);
	else if (Flag == Favorites)
		xStubRootNode = XMLParser::XMLNode::parseFile(pszXML, L"Favorites", &xRes);
		
	if (xRes.error != XMLParser::eXMLErrorNone || xStubRootNode.isEmpty())
	{
		HRSRC hResInfo = FindResource(hInstance, MAKEINTRESOURCE(IDR_XML_STUB), RT_RCDATA);
		HGLOBAL hResGlobal = LoadResource(hInstance, hResInfo);
		LPCSTR pszXMLStub = (LPCSTR)LockResource(hResGlobal);
		// [!] Convert Char to Wchar
		wchar_t* buff = new wchar_t[strlen(pszXMLStub) + 1];
		DEFAULTCONVERTC2W(pszXMLStub, buff, strlen(pszXMLStub));
		buff[strlen(pszXMLStub)] = 0;
		xStubRootNode = XMLParser::XMLNode::parseString(buff, L"PortalBrowser", &xRes);
		delete[] buff;
	}
	
	if (xRes.error == XMLParser::eXMLErrorNone && !xStubRootNode.isEmpty())
	{
		for (std::vector<LPCWSTR>::const_iterator i = strUpdateNodeList.begin(); i != strUpdateNodeList.end(); ++i)
		{
			XMLParser::XMLNode xUpdateNode = XMLParser::XMLNode::parseFile(*i, L"Portal", &xRes);
			
			if (xRes.error == XMLParser::eXMLErrorNone && !xUpdateNode.isEmpty())
			{
				int i = 0;
				XMLParser::XMLNode xStubNode = xStubRootNode.getChildNode(L"Portal", &i);
				
				while (!xStubNode.isEmpty())
				{
					if (lstrcmpi(xUpdateNode.getAttribute(L"name"), xStubNode.getAttribute(L"name")) == 0)
					{
						xStubNode.deleteNodeContent();
					}
					
					xStubNode = xStubRootNode.getChildNode(L"Portal", &i);
				}
				
				xStubRootNode.addChild(xUpdateNode);
				bRet = true;
			}
		}
	}
	
	xStubRootNode.writeToFile(pszXML);
	return !bRet;
}