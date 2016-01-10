/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Automatic Directory Listing Search
 * Henrik Engström, henrikengstrom at home se
 */

#include "stdafx.h"
#include "Resource.h"
#include "ADLSearchFrame.h"
#include "AdlsProperties.h"

int ADLSearchFrame::columnIndexes[] =
{
	COLUMN_ACTIVE_SEARCH_STRING,
	COLUMN_SOURCE_TYPE,
	COLUMN_DEST_DIR,
	COLUMN_MIN_FILE_SIZE,
	COLUMN_MAX_FILE_SIZE
};
int ADLSearchFrame::columnSizes[] =
{
	120,
	90,
	90,
	90,
	90
};
static ResourceManager::Strings columnNames[] =
{
	ResourceManager::ACTIVE_SEARCH_STRING,
	ResourceManager::SOURCE_TYPE,
	ResourceManager::DESTINATION,
	ResourceManager::SIZE_MIN,
	ResourceManager::MAX_SIZE,
};

// Frame creation
LRESULT ADLSearchFrame::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// Create status bar   //[-] SCALOlaz
//	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
//	ctrlStatus.Attach(m_hWndStatusBar);
//	int w[1] = { 0 };
//	ctrlStatus.SetParts(1, w);

	// Create list control
	ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS, WS_EX_CLIENTEDGE, IDC_ADLLIST);
	SET_EXTENDENT_LIST_VIEW_STYLE_WITH_CHECK(ctrlList);
	
	// Set background color
	SET_LIST_COLOR(ctrlList);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(ADLSEARCHFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(ADLSEARCHFRAME_WIDTHS), COLUMN_LAST);
	for (size_t j = 0; j < COLUMN_LAST; j++) //-V104
	{
		const int fmt = LVCFMT_LEFT;
		ctrlList.InsertColumn(j, CTSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	ctrlList.SetColumnOrderArray(COLUMN_LAST, columnIndexes);
	
	// Create buttons
	ctrlAdd.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	               BS_PUSHBUTTON , 0, IDC_ADD);
	ctrlAdd.SetWindowText(CTSTRING(NEW));
	ctrlAdd.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlEdit.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                BS_PUSHBUTTON , 0, IDC_EDIT);
	ctrlEdit.SetWindowText(CTSTRING(PROPERTIES));
	ctrlEdit.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlRemove.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_DISABLED | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  BS_PUSHBUTTON , 0, IDC_REMOVE);
	ctrlRemove.SetWindowText(CTSTRING(REMOVE));
	ctrlRemove.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlMoveUp.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                  BS_PUSHBUTTON , 0, IDC_MOVE_UP);
	ctrlMoveUp.SetWindowText(CTSTRING(MOVE_UP));
	ctrlMoveUp.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlMoveDown.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    BS_PUSHBUTTON , 0, IDC_MOVE_DOWN);
	ctrlMoveDown.SetWindowText(CTSTRING(MOVE_DOWN));
	ctrlMoveDown.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	ctrlHelp.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                BS_PUSHBUTTON , 0, IDC_HELP_FAQ);
	ctrlHelp.SetWindowText(CTSTRING(WHATS_THIS));
	ctrlHelp.SetFont(Fonts::g_systemFont); // [~] Sergey Shuhskanov
	
	// Create context menu
	contextMenu.CreatePopupMenu();
	contextMenu.AppendMenu(MF_STRING, IDC_ADD,    CTSTRING(NEW));
	contextMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
	contextMenu.AppendMenu(MF_STRING, IDC_EDIT,   CTSTRING(PROPERTIES));
	
	SettingsManager::getInstance()->addListener(this);
	// Load all searches
	LoadAll();
	
	bHandled = FALSE;
	return TRUE;
}

// Close window
LRESULT ADLSearchFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		ADLSearchManager::getInstance()->save();
		SettingsManager::getInstance()->removeListener(this);
		WinUtil::setButtonPressed(IDC_FILE_ADL_SEARCH, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		WinUtil::saveHeaderOrder(ctrlList, SettingsManager::ADLSEARCHFRAME_ORDER,
		                         SettingsManager::ADLSEARCHFRAME_WIDTHS, COLUMN_LAST, columnIndexes, columnSizes);
		                         
		bHandled = FALSE;
		return 0;
	}
}

// Recalculate frame control layout
void ADLSearchFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	RECT rect;
	GetClientRect(&rect);
	
	// Position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	/*
	if (ctrlStatus.IsWindow())  //[-] SCALOlaz
	{
	    CRect sr;
	    int w[1];
	    ctrlStatus.GetClientRect(sr);
	    w[0] = sr.Width() - 16;
	    ctrlStatus.SetParts(1, w);
	}
	*/
	
	// Position list control
	CRect rc = rect;
	//rc.top += 2; [~] Sergey Shushkanov
	rc.bottom -= 28;
	ctrlList.MoveWindow(rc);
	
	// Position buttons
	const long bwidth = 90;
	const long bspace = 10;
	rc = rect;
	rc.bottom -= 2;
	rc.top = rc.bottom - 22;
	
	rc.left = 2;
	rc.right = rc.left + bwidth;
	ctrlAdd.MoveWindow(rc);
	
	rc.left += bwidth + 2;
	rc.right = rc.left + bwidth;
	ctrlEdit.MoveWindow(rc);
	
	rc.left += bwidth + 2;
	rc.right = rc.left + bwidth;
	ctrlRemove.MoveWindow(rc);
	
	rc.left += bspace;
	
	rc.left += bwidth + 2;
	rc.right = rc.left + bwidth;
	ctrlMoveUp.MoveWindow(rc);
	
	rc.left += bwidth + 2;
	rc.right = rc.left + bwidth;
	ctrlMoveDown.MoveWindow(rc);
	
	rc.left += bspace;
	
	rc.left += bwidth + 2;
	rc.right = rc.left + bwidth;
	ctrlHelp.MoveWindow(rc);
	
}

// Keyboard shortcuts
LRESULT ADLSearchFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	switch (kd->wVKey)
	{
		case VK_INSERT:
			PostMessage(WM_COMMAND, IDC_ADD, 0);
			break;
		case VK_DELETE:
			PostMessage(WM_COMMAND, IDC_REMOVE, 0);
			break;
		case VK_RETURN:
			PostMessage(WM_COMMAND, IDC_EDIT, 0);
			break;
		default:
			bHandled = FALSE;
	}
	return 0;
}

LRESULT ADLSearchFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlList)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlList, pt);
		}
		
		int status = ctrlList.GetSelectedCount() > 0 ? MFS_ENABLED : MFS_GRAYED;
		contextMenu.EnableMenuItem(IDC_EDIT, status);
		contextMenu.EnableMenuItem(IDC_REMOVE, status);
		contextMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		return TRUE;
	}
	bHandled = FALSE;
	return FALSE;
}

// Add new search
LRESULT ADLSearchFrame::onAdd(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// Invoke edit dialog with fresh search
	ADLSearch search;
	ADLSProperties dlg(&search);
	if (dlg.DoModal((HWND)*this) == IDOK)
	{
		// Add new search to the end or if selected, just before
		ADLSearchManager::SearchCollection& collection = ADLSearchManager::getInstance()->collection;
		
		
		int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
		if (i < 0)
		{
			// Add to end
			collection.push_back(search);
			i = static_cast<int>(collection.size() - 1);
		}
		else
		{
			// Add before selection
			collection.insert(collection.begin() + i, search);
		}
		
		// Update list control
		size_t j = static_cast<size_t>(i);
		while (j < collection.size())
		{
			UpdateSearch(j++);
		}
		ctrlList.EnsureVisible(i, FALSE);
	}
	
	return 0;
}

// Edit existing search
LRESULT ADLSearchFrame::onEdit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// Get selection info
	const int i = ctrlList.GetNextItem(-1, LVNI_SELECTED);
	if (i < 0)
	{
		// Nothing selected
		return 0;
	}
	
	// Edit existing
	ADLSearchManager::SearchCollection& collection = ADLSearchManager::getInstance()->collection;
	ADLSearch search = collection[static_cast<size_t>(i)];
	
	// Invoke dialog with selected search
	ADLSProperties dlg(&search);
	if (dlg.DoModal((HWND)*this) == IDOK)
	{
		// Update search collection
		collection[static_cast<size_t>(i)] = search;
		
		// Update list control
		UpdateSearch(static_cast<size_t>(i));
	}
	
	return 0;
}

// Remove searches
LRESULT ADLSearchFrame::onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlList.GetSelectedCount())
	{
		ADLSearchManager::SearchCollection& collection = ADLSearchManager::getInstance()->collection;
		
		// Loop over all selected items
		int i;
		while ((i = ctrlList.GetNextItem(-1, LVNI_SELECTED)) >= 0)
		{
			collection.erase(collection.begin() + i);
			ctrlList.DeleteItem(i);
		}
	}
	return 0;
}

// Help... Remake by Drakon
LRESULT ADLSearchFrame::onHelp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	MessageBox(CTSTRING(ADL_BRIEF), CTSTRING(ADL_TITLE), MB_OK);
	return 0;
}

// Move selected entries up one step
LRESULT ADLSearchFrame::onMoveUp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ADLSearchManager::SearchCollection& collection = ADLSearchManager::getInstance()->collection;
	
	// Get selection
	vector<int> sel;
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) >= 0)
	{
		sel.push_back(i);
	}
	if (sel.size() < 1)
	{
		return 0;
	}
	
	// Find out where to insert
	int i0 = sel[0];
	if (i0 > 0)
	{
		i0 = i0 - 1;
	}
	
	// Backup selected searches
	ADLSearchManager::SearchCollection backup;
	for (i = 0; i < (int)sel.size(); ++i)
	{
		backup.push_back(collection[sel[static_cast<size_t>(i)]]);
	}
	
	// Erase selected searches
	for (i = static_cast<int>(sel.size()) - 1; i >= 0; --i)
	{
		collection.erase(collection.begin() + sel[static_cast<size_t>(i)]);
	}
	
	// Insert (grouped together)
	for (i = 0; i < (int)sel.size(); ++i)
	{
		collection.insert(collection.begin() + i0 + i, backup[static_cast<size_t>(i)]);
	}
	
	// Update UI
	LoadAll();
	
	// Restore selection
	for (i = 0; i < (int)sel.size(); ++i)
	{
		ctrlList.SetItemState(i0 + i, LVNI_SELECTED, LVNI_SELECTED);
	}
	
	return 0;
}

// Move selected entries down one step
LRESULT ADLSearchFrame::onMoveDown(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ADLSearchManager::SearchCollection& collection = ADLSearchManager::getInstance()->collection;
	
	// Get selection
	vector<int> sel;
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) >= 0)
	{
		sel.push_back(i);
	}
	if (sel.size() < 1)
	{
		return 0;
	}
	
	// Find out where to insert
	int i0 = sel[sel.size() - 1] + 2;
	if (i0 > (int)collection.size())
	{
		i0 = static_cast<int>(collection.size());
	}
	
	// Backup selected searches
	ADLSearchManager::SearchCollection backup;
	for (i = 0; i < (int)sel.size(); ++i)
	{
		backup.push_back(collection[sel[static_cast<size_t>(i)]]);
	}
	
	// Erase selected searches
	for (i = static_cast<int>(sel.size()) - 1; i >= 0; --i)
	{
		collection.erase(collection.begin() + sel[static_cast<size_t>(i)]);
		if (i < i0)
		{
			i0--;
		}
	}
	
	// Insert (grouped together)
	for (i = 0; i < (int)sel.size(); ++i)
	{
		collection.insert(collection.begin() + i0 + i, backup[static_cast<size_t>(i)]);
	}
	
	// Update UI
	LoadAll();
	
	// Restore selection
	for (i = 0; i < (int)sel.size(); ++i)
	{
		ctrlList.SetItemState(i0 + i, LVNI_SELECTED, LVNI_SELECTED);
	}
	ctrlList.EnsureVisible(i0, FALSE);
	
	return 0;
}

// Clicked 'Active' check box
LRESULT ADLSearchFrame::onItemChanged(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NM_LISTVIEW* lv = (NM_LISTVIEW*) pnmh;
	::EnableWindow(GetDlgItem(IDC_EDIT), (lv->uNewState & LVIS_FOCUSED));
	::EnableWindow(GetDlgItem(IDC_REMOVE), (lv->uNewState & LVIS_FOCUSED));
	
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	
	if ((item->uChanged & LVIF_STATE) == 0)
		return 0;
	if ((item->uOldState & INDEXTOSTATEIMAGEMASK(0xf)) == 0)
		return 0;
	if ((item->uNewState & INDEXTOSTATEIMAGEMASK(0xf)) == 0)
		return 0;
		
	if (item->iItem >= 0)
	{
		// Set new active status check box
		ADLSearchManager::SearchCollection& collection = ADLSearchManager::getInstance()->collection;
		ADLSearch& search = collection[static_cast<size_t>(item->iItem)];
		search.isActive = (ctrlList.GetCheckState(item->iItem) != 0);
	}
	return 0;
}

// Double-click on list control
LRESULT ADLSearchFrame::onDoubleClickList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	
	if (item->iItem >= 0)
	{
		// Treat as onEdit command
		PostMessage(WM_COMMAND, IDC_EDIT, 0);
	}
	else if (item->iItem == -1)
	{
		PostMessage(WM_COMMAND, IDC_ADD, 0);
	}
	
	return 0;
}

// Load all searches from manager
void ADLSearchFrame::LoadAll()
{
	// Clear current contents
	ctrlList.DeleteAllItems();
	
	// Load all searches
	ADLSearchManager::SearchCollection& collection = ADLSearchManager::getInstance()->collection;
	for (size_t l = 0; l < collection.size(); l++)
	{
		UpdateSearch(l, FALSE);
	}
}

// Update a specific search item
void ADLSearchFrame::UpdateSearch(size_t index, BOOL doDelete)
{
	ADLSearchManager::SearchCollection& collection = ADLSearchManager::getInstance()->collection;
	
	// Check args
	if (index >= collection.size())
	{
		return;
	}
	ADLSearch& search = collection[index];
	
	// Delete from list control
	if (doDelete)
	{
		ctrlList.DeleteItem(index);
	}
	
	// Generate values
	TStringList line;
	line.push_back(Text::toT(search.searchString));
	line.push_back(search.SourceTypeToDisplayString(search.sourceType));
	line.push_back(Text::toT(search.destDir));
	
	tstring fs;
	if (search.minFileSize >= 0)
	{
		fs = Util::toStringW(search.minFileSize);
		fs += _T(' ');
		fs += search.SizeTypeToDisplayString(search.typeFileSize);
	}
	line.push_back(fs);
	
	fs.clear();
	if (search.maxFileSize >= 0)
	{
		fs = Util::toStringW(search.maxFileSize);
		fs += _T(' ');
		fs += search.SizeTypeToDisplayString(search.typeFileSize);
	}
	line.push_back(fs);
	
	// Insert in list control
	ctrlList.insert(index, line); //-V107
	
	// Update 'Active' check box
	ctrlList.SetCheckState(index, search.isActive);
}

void ADLSearchFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		if (ctrlList.isRedraw())
		{
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

/**
 * @file
 * $Id: ADLSearchFrame.cpp,v 1.24 2006/11/05 15:21:01 bigmuscle Exp $
 */
