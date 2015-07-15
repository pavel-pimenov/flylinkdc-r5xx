/* POSSUM_MOD_BEGIN

Most of this code was stolen/adapted/massacred from...

Module : FileTreeCtrl.cpp
Purpose: Interface for an MFC class which provides a tree control similiar
         to the left hand side of explorer

Copyright (c) 1999 - 2003 by PJ Naughter.  (Web: www.naughter.com, Email: pjna@naughter.com)
*/

#include "stdafx.h"
#include "../client/Util.h"
#include "../client/ShareManager.h"
#include "Resource.h"
#include "LineDlg.h"
#include "WinUtil.h"
#include "foldertree.h"

//Pull in the WNet Lib automatically
#pragma comment(lib, "mpr.lib")


FolderTreeItemInfo::FolderTreeItemInfo(const FolderTreeItemInfo& ItemInfo)
{
	m_sFQPath       = ItemInfo.m_sFQPath;
	m_sRelativePath = ItemInfo.m_sRelativePath;
	m_bNetworkNode  = ItemInfo.m_bNetworkNode;
	m_pNetResource = new NETRESOURCE;
	if (ItemInfo.m_pNetResource)
	{
		//Copy the direct member variables of NETRESOURCE
		CopyMemory(m_pNetResource, ItemInfo.m_pNetResource, sizeof(NETRESOURCE));
		
		//Duplicate the strings which are stored in NETRESOURCE as pointers
		if (ItemInfo.m_pNetResource->lpLocalName)
			m_pNetResource->lpLocalName   = _tcsdup(ItemInfo.m_pNetResource->lpLocalName);
		if (ItemInfo.m_pNetResource->lpRemoteName)
			m_pNetResource->lpRemoteName = _tcsdup(ItemInfo.m_pNetResource->lpRemoteName);
		if (ItemInfo.m_pNetResource->lpComment)
			m_pNetResource->lpComment = _tcsdup(ItemInfo.m_pNetResource->lpComment);
		if (ItemInfo.m_pNetResource->lpProvider)
			m_pNetResource->lpProvider    = _tcsdup(ItemInfo.m_pNetResource->lpProvider);
	}
	else
		memzero(m_pNetResource, sizeof(NETRESOURCE));
}

SystemImageList::SystemImageList()
{
	//Get the temp directory. This is used to then bring back the system image list
	TCHAR pszTempDir[_MAX_PATH + 1];
	GetTempPath(_MAX_PATH + 1, pszTempDir); // TODO - Util::getTempPath()
	TCHAR pszDrive[_MAX_DRIVE + 1];
	_tsplitpath(pszTempDir, pszDrive, NULL, NULL, NULL);
	const int nLen = _tcslen(pszDrive);
	if (nLen >= 1)
	{
		if (pszDrive[nLen - 1] != _T('\\'))
			_tcscat(pszDrive, _T("\\"));
			
		//Attach to the system image list
		SHFILEINFO sfi = { 0 };
		HIMAGELIST hSystemImageList = (HIMAGELIST)SHGetFileInfo(pszTempDir, 0, &sfi, sizeof(SHFILEINFO),
		                                                        SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
		m_ImageList.Attach(hSystemImageList);
	}
}

SystemImageList* SystemImageList::getInstance()
{
	static SystemImageList instance;
	return &instance;
}

SystemImageList::~SystemImageList()
{
	//Detach from the image list to prevent problems on 95/98 where
	//the system image list is shared across processes
	m_ImageList.Detach();
}

ShareEnumerator::ShareEnumerator()
{
	//Set out member variables to defaults
	m_pNTShareEnum = nullptr;
	// [-] IRainman old code: m_pWin9xShareEnum = nullptr;
	m_pNTBufferFree = nullptr;
	m_pNTShareInfo = nullptr;
	// [-] IRainman old code: m_pWin9xShareInfo = nullptr;
	// [-] IRainman old code: m_pWin9xShareInfo = nullptr;
	// [-] IRainman old code: m_hNetApi = nullptr;
	m_dwShares = 0;
	
	// [-] IRainman old code
	//Determine if we are running Windows NT or Win9x
	//OSVERSIONINFO osvi = {0};
	//osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	//m_bWinNT = (GetVersionEx(&osvi) && osvi.dwPlatformId == VER_PLATFORM_WIN32_NT);
	//if (m_bWinNT)
	//{
	//Load up the NETAPI dll
	m_hNetApi = LoadLibrary(_T("NETAPI32.dll"));
	if (m_hNetApi)
	{
		//Get the required function pointers
		m_pNTShareEnum = (NT_NETSHAREENUM*) GetProcAddress(m_hNetApi, "NetShareEnum");
		m_pNTBufferFree = (NT_NETAPIBUFFERFREE*) GetProcAddress(m_hNetApi, "NetApiBufferFree");
	}
	//}
	//else
	//{
	//  //Load up the NETAPI dll
	//  m_hNetApi = LoadLibrary(_T("SVRAPI.dll"));
	//  if (m_hNetApi)
	//  {
	//      //Get the required function pointer
	//      m_pWin9xShareEnum = (WIN9X_NETSHAREENUM*) GetProcAddress(m_hNetApi, "NetShareEnum");
	//  }
	//}
	
	//Update the array of shares we know about
	Refresh();
}

ShareEnumerator::~ShareEnumerator()
{
	// [-] IRainman old code
	//if (m_bWinNT)
	//{
	//Free the buffer if valid
	if (m_pNTShareInfo)
		m_pNTBufferFree(m_pNTShareInfo);
	//}
	// [-] IRainman old code:
	//else
	//  //Free up the heap memory we have used
	//  delete [] m_pWin9xShareInfo;
	
	//Free the dll now that we are finished with it
	if (m_hNetApi)
	{
		FreeLibrary(m_hNetApi);
		m_hNetApi = NULL;
	}
}

void ShareEnumerator::Refresh()
{
	m_dwShares = 0;
	// [-] IRainman old code:
	//if (m_bWinNT)
	//{
	//Free the buffer if valid
	if (m_pNTShareInfo)
		m_pNTBufferFree(m_pNTShareInfo);
		
	//Call the function to enumerate the shares
	if (m_pNTShareEnum)
	{
		DWORD dwEntriesRead = 0;
		m_pNTShareEnum(NULL, 502, (LPBYTE*) &m_pNTShareInfo, MAX_PREFERRED_LENGTH, &dwEntriesRead, &m_dwShares, NULL);
	}
	// [-] IRainman old code:
	/*}
	else
	{
	    //Free the buffer if valid
	    if (m_pWin9xShareInfo)
	        delete [] m_pWin9xShareInfo;
	
	    //Call the function to enumerate the shares
	    if (m_pWin9xShareEnum)
	    {
	        //Start with a reasonably sized buffer
	        unsigned short cbBuffer = 1024;
	        bool bNeedMoreMemory = true;
	        bool bSuccess = false;
	        while (bNeedMoreMemory && !bSuccess)
	        {
	            unsigned short nTotalRead = 0;
	            m_pWin9xShareInfo = (FolderTree_share_info_50*) new BYTE[cbBuffer];
	            memzero(m_pWin9xShareInfo, cbBuffer);
	            unsigned short nShares = 0;
	            NET_API_STATUS nStatus = m_pWin9xShareEnum(NULL, 50, (char FAR *)m_pWin9xShareInfo, cbBuffer, (unsigned short FAR *) & nShares, (unsigned short FAR *) & nTotalRead);
	            if (nStatus == ERROR_MORE_DATA)
	            {
	                //Free up the heap memory we have used
	                delete [] m_pWin9xShareInfo;
	
	                //And double the size, ready for the next loop around
	                cbBuffer *= 2;
	            }
	            else if (nStatus == NERR_Success)
	            {
	                m_dwShares = nShares;
	                bSuccess = true;
	            }
	            else
	                bNeedMoreMemory = false;
	        }
	    }
	}*/
}

bool ShareEnumerator::IsShared(const tstring& p_Path) const
{
	//Assume the item is not shared
	bool bShared = false;
	
	if (m_pNTShareInfo)
	{
		for (DWORD i = 0; i < m_dwShares && !bShared; i++)
		{
			bShared = false;
			if (m_pNTShareInfo[i].shi502_type == STYPE_DISKTREE || m_pNTShareInfo[i].shi502_type == STYPE_PRINTQ)
			{
				bShared = stricmp(p_Path.c_str(), m_pNTShareInfo[i].shi502_path) == 0;
			}
#ifdef _DEBUG
			static int g_count = 0;
			const string l_shi502_path = Text::fromT(tstring(m_pNTShareInfo[i].shi502_path));
			dcdebug("ShareEnumerator::IsShared count = %d m_pNTShareInfo[%d].shi502_path = %s p_Path = %s [bShared = %d]\n",
			        ++g_count,
			        i,
			        l_shi502_path.c_str(),
			        Text::fromT(p_Path).c_str(),
			        int(bShared)
			       );
#endif
		}
	}
	return bShared;
}

FolderTree::FolderTree()
{
	m_dwFileHideFlags = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
	                    FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_TEMPORARY;
	m_bShowCompressedUsingDifferentColor = true;
	m_rgbCompressed = RGB(0, 0, 255);
	m_bShowEncryptedUsingDifferentColor = true;
	m_rgbEncrypted = RGB(255, 0, 0);
	m_dwDriveHideFlags = 0;
	m_hNetworkRoot = NULL;
	m_bShowSharedUsingDifferentIcon = true;
	for (size_t i = 0; i < _countof(m_dwMediaID); i++)
		m_dwMediaID[i] = 0xFFFFFFFF;
	m_pShellFolder = 0;
	SHGetDesktopFolder(&m_pShellFolder);
	m_bDisplayNetwork = true;
	m_dwNetworkItemTypes = RESOURCETYPE_ANY;
	m_hMyComputerRoot = NULL;
	m_bShowMyComputer = true;
	m_bShowRootedFolder = false;
	m_hRootedFolder = NULL;
	m_bShowDriveLabels = true;
	m_pStaticCtrl = NULL;
	
	m_nShareSizeDiff = 0;
	m_bDirty = false;
}

FolderTree::~FolderTree()
{
	safe_release(m_pShellFolder);
}

void FolderTree::PopulateTree()
{
	//attach the image list to the tree control
	SetImageList(*SystemImageList::getInstance()->getImageList(), TVSIL_NORMAL);
	
	//Force a refresh
	Refresh();
	
	Expand(m_hMyComputerRoot, TVE_EXPAND);
}

void FolderTree::Refresh()
{
	//Just in case this will take some time
	//CWaitCursor l_cursor_wait;
	
	CLockRedraw<> l_lock_draw(m_hWnd);
	
	//Get the item which is currently selected
	HTREEITEM hSelItem = GetSelectedItem();
	tstring sItem;
	bool bExpanded = false;
	if (hSelItem)
	{
		sItem = ItemToPath(hSelItem);
		bExpanded = IsExpanded(hSelItem);
	}
	
	theSharedEnumerator.Refresh();
	
	//Remove all nodes that currently exist
	Clear();
	
	//Display the folder items in the tree
	if (m_sRootFolder.empty())
	{
		//Should we insert a "My Computer" node
		if (m_bShowMyComputer)
		{
			FolderTreeItemInfo* pItem = new FolderTreeItemInfo;
			pItem->m_bNetworkNode = false;
			int nIcon = 0;
			int nSelIcon = 0;
			
			//Get the localized name and correct icons for "My Computer"
			LPITEMIDLIST lpMCPidl;
			if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, CSIDL_DRIVES, &lpMCPidl)))
			{
				SHFILEINFO sfi = {0};
				if (SHGetFileInfo((LPCTSTR)lpMCPidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_DISPLAYNAME))
					pItem->m_sRelativePath = sfi.szDisplayName;
				nIcon = GetIconIndex(lpMCPidl);
				nSelIcon = GetSelIconIndex(lpMCPidl);
				WinUtil::safe_sh_free(lpMCPidl);
			}
			
			//Add it to the tree control
			m_hMyComputerRoot = InsertFileItem(TVI_ROOT, pItem, false, nIcon, nSelIcon, false);
			SetHasSharedChildren(m_hMyComputerRoot);
		}
		
		//Display all the drives
		if (!m_bShowMyComputer)
			DisplayDrives(TVI_ROOT, false);
			
		//Also add network neighborhood if requested to do so
		if (m_bDisplayNetwork)
		{
			FolderTreeItemInfo* pItem = new FolderTreeItemInfo;
			pItem->m_bNetworkNode = true;
			int nIcon = 0;
			int nSelIcon = 0;
			
			//Get the localized name and correct icons for "Network Neighborhood"
			LPITEMIDLIST lpNNPidl = 0;
			if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, CSIDL_NETWORK, &lpNNPidl)))
			{
				SHFILEINFO sfi = {0};
				if (SHGetFileInfo((LPCTSTR)lpNNPidl, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_DISPLAYNAME))
					pItem->m_sRelativePath = sfi.szDisplayName;
				nIcon = GetIconIndex(lpNNPidl);
				nSelIcon = GetSelIconIndex(lpNNPidl);
				WinUtil::safe_sh_free(lpNNPidl);
			}
			
			//Add it to the tree control
			m_hNetworkRoot = InsertFileItem(TVI_ROOT, pItem, false, nIcon, nSelIcon, false);
			SetHasSharedChildren(m_hNetworkRoot);
		}
	}
	else
	{
		DisplayPath(m_sRootFolder, TVI_ROOT, false);
	}
	
	//Reselect the initially selected item
	if (hSelItem)
		SetSelectedPath(sItem, bExpanded);
}

tstring FolderTree::ItemToPath(HTREEITEM hItem) const
{
	tstring sPath;
	if (hItem)
	{
		FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hItem);
		//ASSERT(pItem);
		sPath = pItem->m_sFQPath;
	}
	return sPath;
}

bool FolderTree::IsExpanded(HTREEITEM hItem)
{
	TVITEM tvItem;
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_HANDLE | TVIF_STATE;
	return (GetItem(&tvItem) && (tvItem.state & TVIS_EXPANDED));
}

void FolderTree::Clear()
{
	//Delete all the items
	DeleteAllItems();
	
	//Reset the member variables we have
	m_hMyComputerRoot = NULL;
	m_hNetworkRoot = NULL;
	m_hRootedFolder = NULL;
}

int FolderTree::GetIconIndex(HTREEITEM hItem)
{
	TV_ITEM tvi = {0};
	tvi.mask = TVIF_IMAGE;
	tvi.hItem = hItem;
	if (GetItem(&tvi))
		return tvi.iImage;
	else
		return -1;
}

int FolderTree::GetIconIndex(const tstring &sFilename)
{
	//Retreive the icon index for a specified file/folder
	SHFILEINFO sfi  = {0};
	SHGetFileInfo(sFilename.c_str(), 0, &sfi, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_SMALLICON);
	return sfi.iIcon;
}

int FolderTree::GetIconIndex(LPITEMIDLIST lpPIDL)
{
	SHFILEINFO sfi  = {0};
	SHGetFileInfo((LPCTSTR)lpPIDL, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_LINKOVERLAY);
	return sfi.iIcon;
}

int FolderTree::GetSelIconIndex(LPITEMIDLIST lpPIDL)
{
	SHFILEINFO sfi  = {0};
	SHGetFileInfo((LPCTSTR)lpPIDL, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_OPENICON);
	return sfi.iIcon;
}

int FolderTree::GetSelIconIndex(HTREEITEM hItem)
{
	TV_ITEM tvi  = {0};
	tvi.mask = TVIF_SELECTEDIMAGE;
	tvi.hItem = hItem;
	if (GetItem(&tvi))
		return tvi.iSelectedImage;
	else
		return -1;
}

int FolderTree::GetSelIconIndex(const tstring &sFilename)
{
	//Retreive the icon index for a specified file/folder
	SHFILEINFO sfi  = {0};
	SHGetFileInfo(sFilename.c_str(), 0, &sfi, sizeof(SHFILEINFO), SHGFI_SYSICONINDEX | SHGFI_OPENICON | SHGFI_SMALLICON);
	return sfi.iIcon;
}

HTREEITEM FolderTree::InsertFileItem(HTREEITEM hParent, FolderTreeItemInfo *pItem, bool bShared, int nIcon, int nSelIcon, bool bCheckForChildren)
{
	tstring sLabel;
	
	//Correct the label if need be
	if (IsDrive(pItem->m_sFQPath) && m_bShowDriveLabels)
		sLabel = GetDriveLabel(pItem->m_sFQPath);
	else
		sLabel = GetCorrectedLabel(pItem);
		
	//Add the actual item
	TV_INSERTSTRUCT tvis  = {0};
	tvis.hParent = hParent;
	tvis.hInsertAfter = TVI_LAST;
	tvis.item.mask = TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM;
	tvis.item.iImage = nIcon;
	tvis.item.iSelectedImage = nSelIcon;
	
	tvis.item.lParam = (LPARAM) pItem;
	tvis.item.pszText = (LPWSTR)sLabel.c_str();
	if (bCheckForChildren)
		tvis.item.cChildren = HasGotSubEntries(pItem->m_sFQPath);
	else
		tvis.item.cChildren = TRUE;
		
	if (bShared)
	{
		tvis.item.mask |= TVIF_STATE;
		tvis.item.stateMask |= TVIS_OVERLAYMASK;
		tvis.item.state |= INDEXTOOVERLAYMASK(1); //1 is the index for the shared overlay image
	}
	
	const HTREEITEM hItem = InsertItem(&tvis);
	
	string path = Text::fromT(pItem->m_sFQPath);
	AppendPathSeparator(path); //[+]PPA
	if (!path.empty())
	{
		const bool bChecked = ShareManager::isShareFolder(path, true);
		SetChecked(hItem, bChecked);
		if (!bChecked)
			SetHasSharedChildren(hItem);
	}
	return hItem;
}

void FolderTree::DisplayDrives(HTREEITEM hParent, bool bUseSetRedraw /* = true */)
{
	//CWaitCursor l_cursor_wait;
	
	//Speed up the job by turning off redraw
	if (bUseSetRedraw)
		SetRedraw(false);
		
	//Enumerate the drive letters and add them to the tree control
	DWORD dwDrives = GetLogicalDrives();
	DWORD dwMask = 1;
	for (int i = 0; i < 32; i++)
	{
		if (dwDrives & dwMask)
		{
			tstring sDrive;
			sDrive = (wchar_t)('A' + i);
			sDrive += _T(":\\");
			
			//check if this drive is one of the types to hide
			if (CanDisplayDrive(sDrive))
			{
				FolderTreeItemInfo* pItem = new FolderTreeItemInfo(sDrive, sDrive);
				//Insert the item into the view
				InsertFileItem(hParent, pItem, m_bShowSharedUsingDifferentIcon && IsShared(sDrive), GetIconIndex(sDrive), GetSelIconIndex(sDrive), true);
			}
		}
		dwMask <<= 1;
	}
	
	if (bUseSetRedraw)
		SetRedraw(true);
}

void FolderTree::DisplayPath(const tstring &sPath, HTREEITEM hParent, bool bUseSetRedraw /* = true */)
{
	CWaitCursor l_cursor_wait; //-V808
	
	//Speed up the job by turning off redraw
	if (bUseSetRedraw)
		SetRedraw(false);
		
	//Remove all the items currently under hParent
	HTREEITEM hChild = GetChildItem(hParent);
	while (hChild)
	{
		DeleteItem(hChild);
		hChild = GetChildItem(hParent);
	}
	
	//Should we display the root folder
	if (m_bShowRootedFolder && (hParent == TVI_ROOT))
	{
		FolderTreeItemInfo* pItem = new FolderTreeItemInfo(m_sRootFolder, m_sRootFolder);
		m_hRootedFolder = InsertFileItem(TVI_ROOT, pItem, false, GetIconIndex(m_sRootFolder), GetSelIconIndex(m_sRootFolder), true);
		Expand(m_hRootedFolder, TVE_EXPAND);
		return;
	}
	
	//find all the directories underneath sPath
	int nDirectories = 0;
	
	tstring sFile = sPath;
	AppendPathSeparator(sFile);
	
	WIN32_FIND_DATA fData;
	HANDLE hFind = FindFirstFileEx((sFile + _T('*')).c_str(),
	                               CompatibilityManager::g_find_file_level,
	                               &fData,
	                               FindExSearchNameMatch,
	                               NULL,
	                               CompatibilityManager::g_find_file_flags);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			const tstring filename = fData.cFileName;
			if ((fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
			        (filename != Util::m_dotT) && (filename != Util::m_dot_dotT))
			{
				++nDirectories;
				const tstring l_Path = sFile + filename;
				FolderTreeItemInfo* pItem = new FolderTreeItemInfo(l_Path, fData.cFileName);
				const int l_Icon = GetIconIndex(l_Path);
				const int l_SelIcon = GetSelIconIndex(l_Path);
				InsertFileItem(hParent, pItem, m_bShowSharedUsingDifferentIcon && IsShared(l_Path), l_Icon, l_SelIcon, true);
			}
		}
		while (FindNextFile(hFind, &fData));
		FindClose(hFind);
	}
	
	//Now sort the items we just added
	TVSORTCB tvsortcb = {0};
	tvsortcb.hParent = hParent;
	tvsortcb.lpfnCompare = CompareByFilenameNoCase;
	tvsortcb.lParam = 0;
	SortChildrenCB(&tvsortcb);
	
	//If no items were added then remove the "+" indicator from hParent
	if (nDirectories == 0)
		SetHasPlusButton(hParent, FALSE);
		
	//Turn back on the redraw flag
	if (bUseSetRedraw)
		SetRedraw(true);
}

HTREEITEM FolderTree::SetSelectedPath(const tstring &sPath, bool bExpanded /* = false */)
{
	tstring sSearch;
	Text::toLower(sPath, sSearch);
	if (sSearch.empty())
	{
		//TRACE(_T("Cannot select a empty path\n"));
		return NULL;
	}
	
	//Remove initial part of path if the root folder is setup
	tstring sRootFolder;
	Text::toLower(m_sRootFolder, sRootFolder);
	const tstring::size_type nRootLength = sRootFolder.size();
	if (nRootLength)
	{
		if (sSearch.find(sRootFolder) != 0)
			// TODO Either inefficent or wrong usage of string::find(). string::compare() will be faster if string::find's
			// result is compared with 0, because it will not scan the whole string. If your intention is to check that there
			// are no findings in the string, you should compare with std::string::npos.
		{
			//TRACE(_T("Could not select the path %s as the root has been configued as %s\n"), sPath, m_sRootFolder);
			return NULL;
		}
		sSearch = sSearch.substr(nRootLength);
	}
	
	//Remove trailing "\" from the path
	RemovePathSeparator(sSearch);
	
	if (sSearch.empty())
		return nullptr;
		
	CLockRedraw<> l_lock_draw(m_hWnd);
	
	HTREEITEM hItemFound = TVI_ROOT;
	if (nRootLength && m_hRootedFolder)
		hItemFound = m_hRootedFolder;
	bool bDriveMatch = sRootFolder.empty();
	bool bNetworkMatch = m_bDisplayNetwork && ((sSearch.size() > 2) && sSearch.find(_T("\\\\")) == 0);
	if (bNetworkMatch)
	{
		bDriveMatch = false;
		
		//Working here
		bool bHasPlus = HasPlusButton(m_hNetworkRoot);
		bool bHasChildren = (GetChildItem(m_hNetworkRoot) != NULL);
		
		if (bHasPlus && !bHasChildren)
			DoExpand(m_hNetworkRoot);
		else
			Expand(m_hNetworkRoot, TVE_EXPAND);
			
		hItemFound = FindServersNode(m_hNetworkRoot);
		sSearch = sSearch.substr(2);
	}
	if (bDriveMatch)
	{
		if (m_hMyComputerRoot)
		{
			//Working here
			bool bHasPlus = HasPlusButton(m_hMyComputerRoot);
			bool bHasChildren = (GetChildItem(m_hMyComputerRoot) != NULL);
			
			if (bHasPlus && !bHasChildren)
				DoExpand(m_hMyComputerRoot);
			else
				Expand(m_hMyComputerRoot, TVE_EXPAND);
				
			hItemFound = m_hMyComputerRoot;
		}
	}
	
	size_t nFound = sSearch.find(_T('\\'));
	while (nFound != tstring::npos)
	{
		tstring sMatch;
		if (bDriveMatch)
		{
			sMatch = sSearch.substr(0, nFound + 1);
			bDriveMatch = false;
		}
		else
			sMatch = sSearch.substr(0, nFound);
			
		hItemFound = FindSibling(hItemFound, sMatch);
		if (hItemFound == NULL)
			break;
		else if (!IsDrive(sPath))
		{
			SelectItem(hItemFound);
			
			//Working here
			bool bHasPlus = HasPlusButton(hItemFound);
			bool bHasChildren = (GetChildItem(hItemFound) != NULL);
			
			if (bHasPlus && !bHasChildren)
				DoExpand(hItemFound);
			else
				Expand(hItemFound, TVE_EXPAND);
		}
		
		sSearch = sSearch.substr(nFound - 1);
		nFound = sSearch.find(_T('\\'));
	};
	
	//The last item
	if (hItemFound)
	{
		if (sSearch.size())
			hItemFound = FindSibling(hItemFound, sSearch);
		if (hItemFound)
			SelectItem(hItemFound);
			
		if (bExpanded)
		{
			//Working here
			bool bHasPlus = HasPlusButton(hItemFound);
			bool bHasChildren = (GetChildItem(hItemFound) != NULL);
			
			if (bHasPlus && !bHasChildren)
				DoExpand(hItemFound);
			else
				Expand(hItemFound, TVE_EXPAND);
		}
	}
	
	return hItemFound;
}

bool FolderTree::IsDrive(HTREEITEM hItem)
{
	return IsDrive(ItemToPath(hItem));
}

bool FolderTree::IsDrive(const tstring &sPath)
{
	return sPath.size() == 3 && sPath[1] == _T(':') && sPath[2] == _T('\\');
}

tstring FolderTree::GetDriveLabel(const tstring &p_Drive)
{
	USES_CONVERSION;
	//Let's start with the drive letter
	tstring sLabel = p_Drive;
	//Try to find the item directory using ParseDisplayName
	LPITEMIDLIST lpItem = 0;
	if (SUCCEEDED(m_pShellFolder->ParseDisplayName(NULL, NULL, T2W((LPTSTR)(LPCTSTR)p_Drive.c_str()), NULL, &lpItem, NULL)))
	{
		SHFILEINFO sfi = {0};
		if (SHGetFileInfo((LPCTSTR)lpItem, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_DISPLAYNAME))
			sLabel = sfi.szDisplayName;
		WinUtil::safe_sh_free(lpItem);
	}
	
	return sLabel;
}

bool FolderTree::HasGotSubEntries(const tstring &p_Directory)
{
	if (p_Directory.empty())
		return false;
		
	if (DriveHasRemovableMedia(p_Directory))
	{
		return true; //we do not bother searching for files on drives
		//which have removable media as this would cause
		//the drive to spin up, which for the case of a
		//floppy is annoying
	}
	else
	{
		//First check to see if there is any sub directories
		tstring sFile;
		if (p_Directory[p_Directory.size() - 1] == _T('\\'))
			sFile = p_Directory + _T("*.*");
		else
			sFile = p_Directory + _T("\\*.*");
			
		WIN32_FIND_DATA fData;
		HANDLE hFind = FindFirstFileEx(sFile.c_str(),
		                               CompatibilityManager::g_find_file_level,
		                               &fData,
		                               FindExSearchNameMatch,
		                               NULL,
		                               CompatibilityManager::g_find_file_flags);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				const tstring cFileName = fData.cFileName;
				if ((fData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
				        (cFileName != Util::m_dotT) && (cFileName != Util::m_dot_dotT))
				{
					FindClose(hFind);
					return true;
				}
			}
			while (FindNextFile(hFind, &fData));
			FindClose(hFind);
		}
	}
	
	return false;
}

bool FolderTree::CanDisplayDrive(const tstring &sDrive)
{
	//check if this drive is one of the types to hide
	bool bDisplay = true;
	UINT nDrive = GetDriveType(sDrive.c_str());
	switch (nDrive)
	{
		case DRIVE_REMOVABLE:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_REMOVABLE)
				bDisplay = false;
			break;
		}
		case DRIVE_FIXED:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_FIXED)
				bDisplay = false;
			break;
		}
		case DRIVE_REMOTE:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_REMOTE)
				bDisplay = false;
			break;
		}
		case DRIVE_CDROM:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_CDROM)
				bDisplay = false;
			break;
		}
		case DRIVE_RAMDISK:
		{
			if (m_dwDriveHideFlags & DRIVE_ATTRIBUTE_RAMDISK)
				bDisplay = false;
			break;
		}
		default:
		{
			break;
		}
	}
	
	return bDisplay;
}

bool FolderTree::IsShared(const tstring &p_Path) const
{
	//Defer all the work to the share enumerator class
	return theSharedEnumerator.IsShared(p_Path);
}

int CALLBACK FolderTree::CompareByFilenameNoCase(LPARAM lParam1, LPARAM lParam2, LPARAM /*lParamSort*/)
{
	FolderTreeItemInfo* pItem1 = (FolderTreeItemInfo*) lParam1;
	FolderTreeItemInfo* pItem2 = (FolderTreeItemInfo*) lParam2;
	
	return stricmp(pItem1->m_sRelativePath, pItem2->m_sRelativePath);
}

void FolderTree::SetHasPlusButton(HTREEITEM hItem, bool bHavePlus)
{
	//Remove all the child items from the parent
	TV_ITEM tvItem = {0};
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_CHILDREN;
	tvItem.cChildren = bHavePlus;
	SetItem(&tvItem);
}

bool FolderTree::HasPlusButton(HTREEITEM hItem)
{
	TVITEM tvItem;
	tvItem.hItem = hItem;
	tvItem.mask = TVIF_HANDLE | TVIF_CHILDREN;
	return GetItem(&tvItem) && (tvItem.cChildren != 0);
}

void FolderTree::DoExpand(HTREEITEM hItem)
{
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hItem);
	
	//Reset the drive node if the drive is empty or the media has changed
	if (IsMediaValid(pItem->m_sFQPath))
	{
		//Delete the item if the path is no longer valid
		if (IsFolder(pItem->m_sFQPath))
		{
			//Add the new items to the tree if it does not have any child items
			//already
			if (!GetChildItem(hItem))
				DisplayPath(pItem->m_sFQPath, hItem);
		}
		else if (hItem == m_hMyComputerRoot)
		{
			//Display an hour glass as this may take some time
			//CWaitCursor l_cursor_wait;
			
			//Enumerate the local drive letters
			DisplayDrives(m_hMyComputerRoot, FALSE);
		}
		else if ((hItem == m_hNetworkRoot) || (pItem->m_pNetResource))
		{
			//Display an hour glass as this may take some time
			//CWaitCursor l_cursor_wait;
			
			//Enumerate the network resources
			EnumNetwork(hItem);
		}
		else
		{
			//Before we delete it see if we are the only child item
			HTREEITEM hParent = GetParentItem(hItem);
			
			//Delete the item
			DeleteItem(hItem);
			
			//Remove all the child items from the parent
			SetHasPlusButton(hParent, false);
		}
	}
	else
	{
		//Display an hour glass as this may take some time
		//CWaitCursor l_cursor_wait;
		
		//Collapse the drive node and remove all the child items from it
		Expand(hItem, TVE_COLLAPSE);
		DeleteChildren(hItem, true);
	}
}

HTREEITEM FolderTree::FindServersNode(HTREEITEM hFindFrom) const
{
	if (m_bDisplayNetwork)
	{
		//Try to find some "servers" in the child items of hFindFrom
		HTREEITEM hChild = GetChildItem(hFindFrom);
		while (hChild)
		{
			FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hChild);
			
			if (pItem->m_pNetResource)
			{
				//Found a share
				if (pItem->m_pNetResource->dwDisplayType == RESOURCEDISPLAYTYPE_SERVER)
					return hFindFrom;
			}
			
			//Get the next sibling for the next loop around
			hChild = GetNextSiblingItem(hChild);
		}
		
		//Ok, since we got here, we did not find any servers in any of the child nodes of this
		//item. In this case we need to call ourselves recursively to find one
		hChild = GetChildItem(hFindFrom);
		while (hChild)
		{
			HTREEITEM hFound = FindServersNode(hChild);
			if (hFound)
				return hFound;
				
			//Get the next sibling for the next loop around
			hChild = GetNextSiblingItem(hChild);
		}
	}
	
	//If we got as far as here then no servers were found.
	return nullptr;
}

HTREEITEM FolderTree::FindSibling(HTREEITEM hParent, const tstring &sItem) const
{
	HTREEITEM hChild = GetChildItem(hParent);
	while (hChild)
	{
		FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hChild);
		
		if (stricmp(pItem->m_sRelativePath, sItem) == 0)
			return hChild;
		hChild = GetNextItem(hChild, TVGN_NEXT);
	}
	return nullptr;
}

bool FolderTree::DriveHasRemovableMedia(const tstring &sPath)
{
	bool bRemovableMedia = false;
	if (IsDrive(sPath))
	{
		UINT nDriveType = GetDriveType(sPath.c_str());
		bRemovableMedia = ((nDriveType == DRIVE_REMOVABLE) ||
		                   (nDriveType == DRIVE_CDROM));
	}
	
	return bRemovableMedia;
}

bool FolderTree::IsMediaValid(const tstring &sDrive)
{
	//return TRUE if the drive does not support removable media
	UINT nDriveType = GetDriveType(sDrive.c_str());
	if ((nDriveType != DRIVE_REMOVABLE) && (nDriveType != DRIVE_CDROM))
		return true;
		
	//Return FALSE if the drive is empty (::GetVolumeInformation fails)
	DWORD dwSerialNumber;
	int nDrive = sDrive[0] - _T('A');
	if (GetSerialNumber(sDrive, dwSerialNumber))
		m_dwMediaID[nDrive] = dwSerialNumber;
	else
	{
		m_dwMediaID[nDrive] = 0xFFFFFFFF;
		return false;
	}
	
	//Also return FALSE if the disk's serial number has changed
	if ((m_dwMediaID[nDrive] != dwSerialNumber) &&
	        (m_dwMediaID[nDrive] != 0xFFFFFFFF))
	{
		m_dwMediaID[nDrive] = 0xFFFFFFFF;
		return false;
	}
	
	return true;
}

bool FolderTree::IsFolder(const tstring &sPath)
{
	DWORD dwAttributes = GetFileAttributes(sPath.c_str());
	return ((dwAttributes != INVALID_FILE_ATTRIBUTES) && (dwAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

bool FolderTree::EnumNetwork(HTREEITEM hParent)
{
	//What will be the return value from this function
	bool bGotChildren = false;
	
	//Check if the item already has a network resource and use it.
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hParent);
	
	NETRESOURCE* pNetResource = pItem->m_pNetResource;
	
	//Setup for the network enumeration
	HANDLE hEnum;
	DWORD dwResult = WNetOpenEnum(pNetResource ? RESOURCE_GLOBALNET : RESOURCE_CONTEXT, m_dwNetworkItemTypes,
	                              0, pNetResource ? pNetResource : NULL, &hEnum);
	                              
	//Was the read sucessful
	if (dwResult != NO_ERROR)
	{
		//TRACE(_T("Cannot enumerate network drives, Error:%d\n"), dwResult);
		return FALSE;
	}
	
	//Do the network enumeration
	DWORD cbBuffer = 16384;
	
	bool bNeedMoreMemory = true;
	bool bSuccess = false;
	LPNETRESOURCE lpnrDrv = NULL;
	DWORD cEntries = 0;
	while (bNeedMoreMemory && !bSuccess)
	{
		//Allocate the memory and enumerate
		lpnrDrv = (LPNETRESOURCE) new BYTE[cbBuffer]; //-V121
		cEntries = 0xFFFFFFFF;
		dwResult = WNetEnumResource(hEnum, &cEntries, lpnrDrv, &cbBuffer);
		
		if (dwResult == ERROR_MORE_DATA)
		{
			//Free up the heap memory we have used
			delete [] lpnrDrv;
			
			cbBuffer *= 2;
		}
		else if (dwResult == NO_ERROR)
			bSuccess = true;
		else
			bNeedMoreMemory = false;
	}
	
	//Enumeration successful?
	if (bSuccess)
	{
		//Scan through the results
		for (DWORD i = 0; i < cEntries; i++)
		{
			tstring sNameRemote;
			if (lpnrDrv[i].lpRemoteName != NULL)
				sNameRemote = lpnrDrv[i].lpRemoteName;
			else
				sNameRemote = lpnrDrv[i].lpComment;
				
			//Remove leading back slashes
			if (!sNameRemote.empty() && sNameRemote[0] == _T('\\'))
				sNameRemote = sNameRemote.substr(1);
			if (!sNameRemote.empty() && sNameRemote[0] == _T('\\'))
				sNameRemote = sNameRemote.substr(1);
				
			//Setup the item data for the new item
			FolderTreeItemInfo* l_pItem = new FolderTreeItemInfo;
			l_pItem->m_pNetResource = new NETRESOURCE;
			memzero(l_pItem->m_pNetResource, sizeof(NETRESOURCE));
			*l_pItem->m_pNetResource = lpnrDrv[i];
			
			if (lpnrDrv[i].lpLocalName)
				l_pItem->m_pNetResource->lpLocalName    = _tcsdup(lpnrDrv[i].lpLocalName);
			if (lpnrDrv[i].lpRemoteName)
				l_pItem->m_pNetResource->lpRemoteName = _tcsdup(lpnrDrv[i].lpRemoteName);
			if (lpnrDrv[i].lpComment)
				l_pItem->m_pNetResource->lpComment  = _tcsdup(lpnrDrv[i].lpComment);
			if (lpnrDrv[i].lpProvider)
				l_pItem->m_pNetResource->lpProvider = _tcsdup(lpnrDrv[i].lpProvider);
			if (lpnrDrv[i].lpRemoteName)
				l_pItem->m_sFQPath = lpnrDrv[i].lpRemoteName;
			else
				l_pItem->m_sFQPath = sNameRemote;
				
			l_pItem->m_sRelativePath = sNameRemote;
			l_pItem->m_bNetworkNode = true;
			
			//Display a share and the appropiate icon
			if (lpnrDrv[i].dwDisplayType == RESOURCEDISPLAYTYPE_SHARE)
			{
				//Display only the share name
				tstring::size_type nPos = l_pItem->m_sRelativePath.find(_T('\\'));
				if (nPos != tstring::npos)
					l_pItem->m_sRelativePath = l_pItem->m_sRelativePath.substr(nPos + 1);
					
				//Now add the item into the control
				InsertFileItem(hParent, l_pItem, m_bShowSharedUsingDifferentIcon, GetIconIndex(l_pItem->m_sFQPath),
				               GetSelIconIndex(l_pItem->m_sFQPath), TRUE);
			}
			else if (lpnrDrv[i].dwDisplayType == RESOURCEDISPLAYTYPE_SERVER)
			{
				//Now add the item into the control
				tstring sServer = _T("\\\\");
				sServer += l_pItem->m_sRelativePath;
				InsertFileItem(hParent, l_pItem, false, GetIconIndex(sServer), GetSelIconIndex(sServer), false);
			}
			else
			{
				//Now add the item into the control
				//Just use the generic Network Neighborhood icons for everything else
				LPITEMIDLIST lpNNPidl = 0;
				int nIcon = 0xFFFF;// TODO WTF?
				int nSelIcon = nIcon;
				if (SUCCEEDED(SHGetSpecialFolderLocation(NULL, CSIDL_NETWORK, &lpNNPidl)))
				{
					nIcon = GetIconIndex(lpNNPidl);
					nSelIcon = GetSelIconIndex(lpNNPidl);
					WinUtil::safe_sh_free(lpNNPidl);
				}
				InsertFileItem(hParent, l_pItem, false, nIcon, nSelIcon, false);
			}
			bGotChildren = true;
		}
	}
	/*  else
	        TRACE(_T("Cannot complete network drive enumeration, Error:%d\n"), dwResult);
	*/
	//Clean up the enumeration handle
	WNetCloseEnum(hEnum);
	
	//Free up the heap memory we have used
	delete [] lpnrDrv;
	
	//Return whether or not we added any items
	return bGotChildren;
}

int FolderTree::DeleteChildren(HTREEITEM hItem, bool bUpdateChildIndicator)
{
	int nCount = 0;
	HTREEITEM hChild = GetChildItem(hItem);
	while (hChild)
	{
		//Get the next sibling before we delete the current one
		HTREEITEM hNextItem = GetNextSiblingItem(hChild);
		
		//Delete the current child
		DeleteItem(hChild);
		
		//Get ready for the next loop
		hChild = hNextItem;
		++nCount;
	}
	
	//Also update its indicator to suggest that their is children
	if (bUpdateChildIndicator)
		SetHasPlusButton(hItem, (nCount != 0));
		
	return nCount;
}

BOOL FolderTree::GetSerialNumber(const tstring &sDrive, DWORD &dwSerialNumber)
{
	return GetVolumeInformation(sDrive.c_str(), NULL, 0, &dwSerialNumber, NULL, NULL, NULL, 0);
}

LRESULT FolderTree::OnSelChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL &bHandled)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pnmh;
	
	//Nothing selected
	if (pNMTreeView->itemNew.hItem == NULL)
	{
		bHandled = FALSE;
		return 0;
	}
	
	//Check to see if the current item is valid, if not then delete it (Exclude network items from this check)
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(pNMTreeView->itemNew.hItem);
	//ASSERT(pItem);
	tstring sPath = pItem->m_sFQPath;
	if ((pNMTreeView->itemNew.hItem != m_hNetworkRoot) && (pItem->m_pNetResource == NULL) &&
	        (pNMTreeView->itemNew.hItem != m_hMyComputerRoot) && !IsDrive(sPath) && (GetFileAttributes(sPath.c_str()) == INVALID_FILE_ATTRIBUTES))
	{
		//Before we delete it see if we are the only child item
		HTREEITEM hParent = GetParentItem(pNMTreeView->itemNew.hItem);
		
		//Delete the item
		DeleteItem(pNMTreeView->itemNew.hItem);
		
		//Remove all the child items from the parent
		SetHasPlusButton(hParent, false);
		
		bHandled = FALSE; //Allow the message to be reflected again
		return 1;
	}
	
	//Remeber the serial number for this item (if it is a drive)
	if (IsDrive(sPath))
	{
		int nDrive = sPath[0] - _T('A');
		GetSerialNumber(sPath, m_dwMediaID[nDrive]);
	}
	
	bHandled = FALSE; //Allow the message to be reflected again
	
	return 0;
}

LRESULT FolderTree::OnItemExpanding(int /*idCtrl*/, LPNMHDR pnmh, BOOL &bHandled)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pnmh;
	if (pNMTreeView->action == TVE_EXPAND)
	{
		bool bHasPlus = HasPlusButton(pNMTreeView->itemNew.hItem);
		bool bHasChildren = (GetChildItem(pNMTreeView->itemNew.hItem) != NULL);
		
		if (bHasPlus && !bHasChildren)
			DoExpand(pNMTreeView->itemNew.hItem);
	}
	else if (pNMTreeView->action == TVE_COLLAPSE)
	{
		FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(pNMTreeView->itemNew.hItem);
		//ASSERT(pItem);
		
		//Display an hour glass as this may take some time
		//CWaitCursor l_cursor_wait;
		
		//Collapse the node and remove all the child items from it
		Expand(pNMTreeView->itemNew.hItem, TVE_COLLAPSE);
		
		//Never uppdate the child indicator for a network node which is not a share
		bool bUpdateChildIndicator = true;
		if (pItem->m_bNetworkNode)
		{
			if (pItem->m_pNetResource)
				bUpdateChildIndicator = (pItem->m_pNetResource->dwDisplayType == RESOURCEDISPLAYTYPE_SHARE);
			else
				bUpdateChildIndicator = false;
		}
		DeleteChildren(pNMTreeView->itemNew.hItem, bUpdateChildIndicator);
	}
	
	bHandled = FALSE; //Allow the message to be reflected again
	return 0;
}

LRESULT FolderTree::OnDeleteItem(int /*idCtrl*/, LPNMHDR pnmh, BOOL &bHandled)
{
	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pnmh;
	if (pNMTreeView->itemOld.hItem != TVI_ROOT)
	{
		FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) pNMTreeView->itemOld.lParam;
		if (pItem->m_pNetResource)
		{
			free(pItem->m_pNetResource->lpLocalName);
			free(pItem->m_pNetResource->lpRemoteName);
			free(pItem->m_pNetResource->lpComment);
			free(pItem->m_pNetResource->lpProvider);
			safe_delete(pItem->m_pNetResource);
		}
		delete pItem;
	}
	bHandled = FALSE; //Allow the message to be reflected again
	return 0;
}

// [+] birkoff.anarchist
LRESULT FolderTree::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_SPACE:
			HTREEITEM htItem = GetSelectedItem();
			
			if (!GetChecked(htItem))
				return OnChecked(htItem, bHandled);
			else
				return OnUnChecked(htItem, bHandled);
				
			break;
	}
	return 0;
}

bool FolderTree::GetChecked(HTREEITEM hItem) const
{
	UINT u = GetItemState(hItem, TVIS_STATEIMAGEMASK);
	return (u & INDEXTOSTATEIMAGEMASK(TVIF_IMAGE | TVIF_PARAM | TVIF_STATE)) ? true : false;
}

BOOL FolderTree::SetChecked(HTREEITEM hItem, bool fCheck)
{
	TVITEM item;
	item.mask = TVIF_HANDLE | TVIF_STATE;
	item.hItem = hItem;
	item.stateMask = TVIS_STATEIMAGEMASK;
	
	/*
	Since state images are one-based, 1 in this macro turns the check off, and
	2 turns it on.
	*/
	item.state = INDEXTOSTATEIMAGEMASK((fCheck ? TVIF_IMAGE : TVIF_TEXT));
	
	return SetItem(&item);
}

// !SMT!-P
LRESULT FolderTree::OnClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled)
{
	DWORD dwPos = GetMessagePos();
	POINT ptPos;
	ptPos.x = GET_X_LPARAM(dwPos);
	ptPos.y = GET_Y_LPARAM(dwPos);
	
	ScreenToClient(&ptPos);
	
	UINT uFlags;
	HTREEITEM htItemClicked = HitTest(ptPos, &uFlags);
	
	// if the item's checkbox was clicked ...
	if (uFlags & TVHT_ONITEMSTATEICON)
	{
		// retrieve is's soon-to-be former state
		if (!GetChecked(htItemClicked))
			return OnChecked(htItemClicked, bHandled);
		else
			return OnUnChecked(htItemClicked, bHandled);
	}
	
	bHandled = FALSE;
	return 0;
}
LRESULT FolderTree::OnRClick(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& bHandled)
{
	DWORD dwPos = GetMessagePos();
	POINT ptPos;
	ptPos.x = GET_X_LPARAM(dwPos);
	ptPos.y = GET_Y_LPARAM(dwPos);
	POINT ptMenu = ptPos;
	
	ScreenToClient(&ptPos);
	
	UINT uFlags;
	HitTest(ptPos, &uFlags);
	
	// if the item's label was clicked ...
	if (uFlags & TVHT_ONITEMLABEL)
	{
		CMenu prioMenu;
		prioMenu.CreatePopupMenu();
		prioMenu.AppendMenu(MF_STRING, IDC_U_PRIO_EXTRA, CTSTRING(GIVE_EXTRA_SLOT));
		prioMenu.AppendMenu(MF_SEPARATOR);
		prioMenu.AppendMenu(MF_STRING, IDC_U_PRIO_HIGH, CTSTRING(HIGH_PRIORITY));
		prioMenu.AppendMenu(MF_STRING, IDC_U_PRIO_NORMAL, CTSTRING(NORMAL_PRIORITY));
		prioMenu.AppendMenu(MF_STRING, IDC_U_PRIO_LOW, CTSTRING(LOW_PRIORITY));
		prioMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, ptMenu.x, ptMenu.y, m_hWnd);
		bHandled = FALSE;
		return 1;
		/*[-]PPA
		
		.\windows\FolderTree.cpp(1441): remark #111: statement is unreachable
		
		                // retrieve is's soon-to-be former state
		                if(!GetChecked(htItemClicked))
		                        return OnChecked(htItemClicked, bHandled);
		                else
		                        return OnUnChecked(htItemClicked, bHandled);
		*/
	}
	bHandled = FALSE;
	return 0;
}
// end !SMT!-P

LRESULT FolderTree::OnChecked(HTREEITEM hItem, BOOL &bHandled)
{
	CWaitCursor l_cursor_wait; //-V808
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hItem);
	if (!Util::validatePath(Text::fromT(pItem->m_sFQPath)))
	{
		// no checking myComp or network
		bHandled = TRUE;
		return 1;
	}
	
	HTREEITEM hSharedParent = HasSharedParent(hItem);
	// if a parent is checked then this folder should be removed from the ex list
	if (hSharedParent != NULL)
	{
		ShareParentButNotSiblings(hItem);
	}
	else
	{
		// if no parent folder is checked then this is a new root dir
		try
		{
			LineDlg virt;
			virt.title = TSTRING(VIRTUAL_NAME);
			virt.description = TSTRING(VIRTUAL_NAME_LONG);
			
			tstring path = pItem->m_sFQPath;
			AppendPathSeparator(path); //[+]PPA
			virt.line = Text::toT(ShareManager::getInstance()->validateVirtual(
			                          Util::getLastDir(Text::fromT(path))));
			                          
			if (virt.DoModal() == IDOK)
			{
				CWaitCursor l_cursor_wait; //-V808
				ShareManager::getInstance()->addDirectory(Text::fromT(path), Text::fromT(virt.line), true); // TODO hotpoint, mb add queue for this call and run it after OK is pressed?
			}
			else
			{
				bHandled = TRUE;
				return 1;
			}
			UpdateParentItems(hItem);
		}
		catch (const ShareException& e)
		{
			MessageBox(Text::toT(e.getError()).c_str(), _T(APPNAME) _T(" ") T_VERSIONSTRING, MB_ICONSTOP | MB_OK);
			bHandled = TRUE;
			return 1;
		}
	}
	
	UpdateStaticCtrl();
	
	UpdateChildItems(hItem, true);
	
	return 0;
}

LRESULT FolderTree::OnUnChecked(HTREEITEM hItem, BOOL& /*bHandled*/)
{
	CWaitCursor l_cursor_wait; //-V808
	FolderTreeItemInfo* pItem = (FolderTreeItemInfo*) GetItemData(hItem);
	
	HTREEITEM hSharedParent = HasSharedParent(hItem);
	// if no parent is checked remove this root folder from share
	if (hSharedParent == NULL)
	{
		//[+]PPA TODO fix copy-paste
		string path = Text::fromT(pItem->m_sFQPath);
		AppendPathSeparator(path); //[+]PPA
		
		int64_t temp = ShareManager::removeExcludeFolder(path);
		/* fun with math
		futureShareSize = currentShareSize + currentOffsetSize - (sizeOfDirToBeRemoved - sizeOfSubDirsWhichWhereAlreadyExcluded) */
		const int64_t futureShareSize = ShareManager::getShareSize() + m_nShareSizeDiff - (Util::getDirSize(Text::fromT(pItem->m_sFQPath)) - temp);
		ShareManager::getInstance()->removeDirectory(path); // TODO hotpoint, mb add queue for this call and run it after OK is pressed?
		/* more fun with math
		theNewOffset = whatWeKnowTheCorrectNewSizeShouldBe - whatTheShareManagerThinksIsTheNewShareSize */
		m_nShareSizeDiff = futureShareSize - ShareManager::getShareSize();
		UpdateParentItems(hItem);
	}
	else if (GetChecked(GetParentItem(hItem)))
	{
		// if the parent is checked add this folder to excludes
		//[+]PPA TODO fix copy-paste
		string path = Text::fromT(pItem->m_sFQPath);
		AppendPathSeparator(path); //[+]PPA
		m_nShareSizeDiff -= ShareManager::addExcludeFolder(path);
	}
	
	UpdateStaticCtrl();
	
	UpdateChildItems(hItem, false);
	
	return 0;
}

bool FolderTree::GetHasSharedChildren(HTREEITEM hItem)
{
	string searchStr;
	string::size_type startPos = 0;
	
	if (hItem == m_hMyComputerRoot)
	{
		startPos = 1;
		searchStr = ":\\";
	}
	else if (hItem == m_hNetworkRoot)
		searchStr = "\\\\";
	else if (hItem != NULL)
	{
		FolderTreeItemInfo* pItem  = (FolderTreeItemInfo*) GetItemData(hItem);
		searchStr = Text::fromT(pItem->m_sFQPath);
		
		if (searchStr.empty())
			return false;
			
	}
	else
		return false;
		
	CFlyDirItemArray Dirs;
	ShareManager::getDirectories(Dirs);
	
	for (auto i = Dirs.cbegin(); i != Dirs.cend(); ++i)
	{
		if (i->m_path.size() > searchStr.size() + startPos)
		{
			if (stricmp(i->m_path.substr(startPos, searchStr.size()), searchStr) == 0)
			{
				if (searchStr.size() <= 3)
				{
					const auto l_is_exists = File::isExist(i->m_path);
					return l_is_exists;
				}
				else
				{
					if (i->m_path.substr(searchStr.size()).substr(0, 1) == "\\")
					{
						const auto l_is_exists = File::isExist(i->m_path);
						return l_is_exists;
					}
					else
						return false;
				}
			}
		}
	}
	
	return false;
}

void FolderTree::SetHasSharedChildren(HTREEITEM hItem)
{
	SetHasSharedChildren(hItem, GetHasSharedChildren(hItem));
}

void FolderTree::SetHasSharedChildren(HTREEITEM hItem, bool bHasSharedChildren)
{
	if (bHasSharedChildren)
		SetItemState(hItem, TVIS_BOLD, TVIS_BOLD);
	else
		SetItemState(hItem, 0, TVIS_BOLD);
}

void FolderTree::UpdateChildItems(HTREEITEM hItem, bool bChecked)
{
	HTREEITEM hChild = GetChildItem(hItem);
	while (hChild)
	{
		SetChecked(hChild, bChecked);
		UpdateChildItems(hChild, bChecked);
		hChild = GetNextSiblingItem(hChild);
	}
}

void FolderTree::UpdateParentItems(HTREEITEM hItem)
{
	HTREEITEM hParent = GetParentItem(hItem);
	if (hParent != nullptr && HasSharedParent(hParent) == NULL)
	{
		SetHasSharedChildren(hParent);
		UpdateParentItems(hParent);
	}
}

HTREEITEM FolderTree::HasSharedParent(HTREEITEM hItem)
{
	HTREEITEM hParent = GetParentItem(hItem);
	while (hParent != nullptr)
	{
		if (GetChecked(hParent))
			return hParent;
			
		hParent = GetParentItem(hParent);
	}
	
	return NULL;
}

void FolderTree::ShareParentButNotSiblings(HTREEITEM hItem)
{
	FolderTreeItemInfo* pItem;
	HTREEITEM hParent = GetParentItem(hItem);
	if (!GetChecked(hParent))
	{
		SetChecked(hParent, true);
		pItem = (FolderTreeItemInfo*) GetItemData(hParent);
		//[+]PPA TODO fix copy-paste
		string path = Text::fromT(pItem->m_sFQPath);
		AppendPathSeparator(path); //[+]PPA
		m_nShareSizeDiff += ShareManager::removeExcludeFolder(path);
		
		ShareParentButNotSiblings(hParent);
		
		HTREEITEM hChild = GetChildItem(hParent);
		while (hChild)
		{
			HTREEITEM hNextItem = GetNextSiblingItem(hChild);
			if (!GetChecked(hChild))
			{
				pItem = (FolderTreeItemInfo*) GetItemData(hChild);
				if (hChild != hItem)
				{
					//[+]PPA TODO fix copy-paste
					string l_path = Text::fromT(pItem->m_sFQPath);
					AppendPathSeparator(path); //[+]PPA
					m_nShareSizeDiff -= ShareManager::addExcludeFolder(l_path);
				}
			}
			hChild = hNextItem;
		}
	}
	else
	{
		pItem = (FolderTreeItemInfo*) GetItemData(hItem);
		//[+]PPA TODO fix copy-paste
		string path = Text::fromT(pItem->m_sFQPath);
		AppendPathSeparator(path); //[+]PPA
		m_nShareSizeDiff += ShareManager::removeExcludeFolder(path);
	}
}

void FolderTree::SetStaticCtrl(CStatic *staticCtrl)
{
	m_pStaticCtrl = staticCtrl;
}

void FolderTree::UpdateStaticCtrl()
{
	m_bDirty = true;
	if (m_pStaticCtrl != NULL)
	{
		/* display theActualSizeOfTheShareAfterRefresh = WhatTheShareManagerThinksIsTheCorrectSize - theOffsetCreatedByPlayingAroundWithExcludeFolders */
		int64_t shareSize = ShareManager::getShareSize() + m_nShareSizeDiff;
		if (shareSize < 0)
			shareSize = 0;
		m_pStaticCtrl->SetWindowText((Util::formatBytesW(shareSize) + _T('*')).c_str());
	}
}

bool FolderTree::IsDirty() const
{
	return m_bDirty;
}
