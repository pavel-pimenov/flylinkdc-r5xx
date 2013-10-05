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

#include "stdafx.h"

#include "Resource.h"

#include "../client/ResourceManager.h"
#include "../client/SettingsManager.h"
#include "../client/ConnectionManager.h"
#include "../client/DownloadManager.h"
#include "../client/UploadManager.h"
#include "../client/QueueManager.h"
#include "../client/QueueItem.h"
#include "../client/ThrottleManager.h"

#include "UsersFrame.h"

#include "WinUtil.h"
#include "TransferView.h"
#include "MainFrm.h"

#include "BarShader.h"
#include "ResourceLoader.h" // [+] InfinitySky. PNG Support from Apex 1.3.8.

HIconWrapper TransferView::g_user_icon(IDR_TUSER);
int TransferView::columnIndexes[] =
{
	COLUMN_USER,
	COLUMN_HUB,
	COLUMN_STATUS,
	COLUMN_TIMELEFT,
	COLUMN_SPEED,
	COLUMN_FILE,
	COLUMN_SIZE,
	COLUMN_PATH,
	COLUMN_CIPHER,
	COLUMN_LOCATION,
	COLUMN_IP,
#ifdef PPA_INCLUDE_COLUMN_RATIO
	COLUMN_RATIO,
#endif
	COLUMN_SHARE, //[+]PPA
	COLUMN_SLOTS //[+]PPA
};
int TransferView::columnSizes[] =
{
	150, // COLUMN_USER
	150, // COLUMN_HUB
	250, // COLUMN_STATUS
	75,  // COLUMN_TIMELEFT
	75,  // COLUMN_SPEED
	175, // COLUMN_FILE
	100, // COLUMN_SIZE
	200, // COLUMN_PATH
	100, // COLUMN_CIPHER
	150,  // COLUMN_LOCATION
	75, // COLUMN_IP
#ifdef PPA_INCLUDE_COLUMN_RATIO
	50,  // COLUMN_RATIO
#endif
	100, // COLUMN_SHARE
	75,  // COLUMN_SLOTS
};

static ResourceManager::Strings columnNames[] = { ResourceManager::USER, ResourceManager::HUB_SEGMENTS, ResourceManager::STATUS,
                                                  ResourceManager::TIME_LEFT, ResourceManager::SPEED, ResourceManager::FILENAME, ResourceManager::SIZE, ResourceManager::PATH,
                                                  ResourceManager::CIPHER,
                                                  ResourceManager::LOCATION_BARE,
                                                  ResourceManager::IP_BARE
#ifdef PPA_INCLUDE_COLUMN_RATIO
                                                  , ResourceManager::RATIO
#endif
                                                  , ResourceManager::SHARED, //[+]PPA
                                                  ResourceManager::SLOTS //[+]PPA
                                                };
TransferView::~TransferView()
{
	m_arrows.Destroy();
	m_speedImagesBW.Destroy();
	m_speedImages.Destroy();
	OperaColors::ClearCache();
}

LRESULT TransferView::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	ResourceLoader::LoadImageList(IDR_ARROWS, m_arrows, 16, 16);
	ResourceLoader::LoadImageList(IDR_TSPEEDS, m_speedImages, 16, 16);
	ResourceLoader::LoadImageList(IDR_TSPEEDS_BW, m_speedImagesBW, 16, 16);
	ctrlTransfers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_STATICEDGE, IDC_TRANSFERS);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlTransfers);
	
	WinUtil::splitTokens(columnIndexes, SETTING(MAINFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(MAINFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (size_t j = 0; j < COLUMN_LAST; j++)
	{
		int fmt = (j == COLUMN_SIZE || j == COLUMN_TIMELEFT || j == COLUMN_SPEED) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlTransfers.InsertColumn(j, TSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlTransfers.setColumnOrderArray(COLUMN_LAST, columnIndexes);
	ctrlTransfers.setVisible(SETTING(MAINFRAME_VISIBLE));
	
	ctrlTransfers.setSortColumn(SETTING(TRANSFERS_COLUMNS_SORT));
	ctrlTransfers.setAscending(BOOLSETTING(TRANSFERS_COLUMNS_SORT_ASC));
	
	SET_LIST_COLOR(ctrlTransfers);
	ctrlTransfers.setFlickerFree(Colors::bgBrush);
	
	ctrlTransfers.SetImageList(m_arrows, LVSIL_SMALL);
	
	// [-] brain-ripper
	// Make menu dynamic (in context menu handler), since its content depends of which
	// user selected (for add/remove favorites item)
	/*
	transferMenu.CreatePopupMenu();
	appendUserItems(transferMenu, Util::emptyString); // TODO: hubhint
	transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)WinUtil::speedMenu, CTSTRING(UPLOAD_SPEED_LIMIT)); // !SMT!-S
	transferMenu.AppendMenu(MF_SEPARATOR);
	transferMenu.AppendMenu(MF_STRING, IDC_FORCE, CTSTRING(FORCE_ATTEMPT));
	transferMenu.AppendMenu(MF_SEPARATOR); //[+] Drakon
	transferMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSED)); //[+] Drakon
	transferMenu.AppendMenu(MF_SEPARATOR);
	transferMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
	*/
	
	// !SMT!-UI
	copyMenu.CreatePopupMenu();
#ifdef OLD_MENU_HEADER //[~]JhaoDa
	copyMenu.InsertSeparatorFirst(TSTRING(COPY));
#endif
	for (size_t i = 0; i < COLUMN_LAST; ++i)
		copyMenu.AppendMenu(MF_STRING, IDC_COPY + i, CTSTRING_I(columnNames[i]));
	copyMenu.AppendMenu(MF_SEPARATOR);
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(COPY_TTH));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));
	
	usercmdsMenu.CreatePopupMenu();
	
	/*
	transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)previewMenu, CTSTRING(PREVIEW_MENU));
	transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)usercmdsMenu, CTSTRING(SETTINGS_USER_COMMANDS));
	transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY)); // !SMT!-UI
	transferMenu.AppendMenu(MF_SEPARATOR);
	#ifdef PPA_INCLUDE_DROP_SLOW
	transferMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
	transferMenu.AppendMenu(MF_SEPARATOR);
	#endif
	transferMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(CLOSE_CONNECTION));
	transferMenu.SetMenuDefaultItem(IDC_PRIVATEMESSAGE);
	*/
	
	segmentedMenu.CreatePopupMenu();
	segmentedMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
	appendPreviewItems(segmentedMenu);
#ifdef PPA_INCLUDE_DROP_SLOW
	segmentedMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
#endif
	segmentedMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY)); // !SMT!-UI
#ifdef PPA_INCLUDE_ASK_SLOT
	segmentedMenu.AppendMenu(MF_STRING, IDC_ASK_SLOT, CTSTRING(ASK_SLOT)); // !SMT!-UI
#endif
	segmentedMenu.AppendMenu(MF_SEPARATOR); //[+] Drakon
	segmentedMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSED)); //[+] Drakon
	segmentedMenu.AppendMenu(MF_SEPARATOR);
	segmentedMenu.AppendMenu(MF_STRING, IDC_CONNECT_ALL, CTSTRING(CONNECT_ALL));
	segmentedMenu.AppendMenu(MF_STRING, IDC_DISCONNECT_ALL, CTSTRING(DISCONNECT_ALL));
	segmentedMenu.AppendMenu(MF_SEPARATOR);
	segmentedMenu.AppendMenu(MF_STRING, IDC_EXPAND_ALL, CTSTRING(EXPAND_ALL));
	segmentedMenu.AppendMenu(MF_STRING, IDC_COLLAPSE_ALL, CTSTRING(COLLAPSE_ALL));
	segmentedMenu.AppendMenu(MF_SEPARATOR);
	segmentedMenu.AppendMenu(MF_STRING, IDC_REMOVEALL, CTSTRING(REMOVE_ALL));
	
	ConnectionManager::getInstance()->addListener(this);
	DownloadManager::getInstance()->addListener(this);
	UploadManager::getInstance()->addListener(this);
	QueueManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	create_timer(1000);
	return 0;
}

void TransferView::prepareClose()
{
	safe_destroy_timer();
	ctrlTransfers.saveHeaderOrder(SettingsManager::MAINFRAME_ORDER, SettingsManager::MAINFRAME_WIDTHS,
	                              SettingsManager::MAINFRAME_VISIBLE);
	                              
	SET_SETTING(TRANSFERS_COLUMNS_SORT, ctrlTransfers.getSortColumn());
	SET_SETTING(TRANSFERS_COLUMNS_SORT_ASC, ctrlTransfers.isAscending());
	
	SettingsManager::getInstance()->removeListener(this);
	QueueManager::getInstance()->removeListener(this);
	UploadManager::getInstance()->removeListener(this);
	DownloadManager::getInstance()->removeListener(this);
	ConnectionManager::getInstance()->removeListener(this);
	
	//WinUtil::UnlinkStaticMenus(transferMenu); // !SMT!-UI
}

LRESULT TransferView::onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	RECT rc;
	GetClientRect(&rc);
	ctrlTransfers.MoveWindow(&rc);
	
	return 0;
}

LRESULT TransferView::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlTransfers && ctrlTransfers.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlTransfers, pt);
		}
		
		if (ctrlTransfers.GetSelectedCount() > 0)
		{
			int i = -1;
			bool bCustomMenu = false;
			ItemInfo* ii = ctrlTransfers.getItemData(ctrlTransfers.GetNextItem(-1, LVNI_SELECTED));
			
			// Something wrong happened
			if (!ii)
			{
				bHandled = FALSE;
				return FALSE;
			}
			
			bool main = !ii->parent && ctrlTransfers.findChildren(ii->getGroupCond()).size() > 1;
			
			clearPreviewMenu();
			clearUserMenu();
			
			// [+] brain-ripper
			// Make menu dynamic, since its content depends of which
			// user selected (for add/remove favorites item)
			OMenu transferMenu;
			
			transferMenu.CreatePopupMenu();
			transferMenu.AppendMenu(MF_STRING, IDC_FORCE, CTSTRING(FORCE_ATTEMPT));
			transferMenu.AppendMenu(MF_SEPARATOR); //[+] Drakon
			transferMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSED)); //[+] Drakon
			transferMenu.AppendMenu(MF_SEPARATOR);
			transferMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
			appendPreviewItems(transferMenu);
			transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)usercmdsMenu, CTSTRING(SETTINGS_USER_COMMANDS));
			transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY)); // !SMT!-UI
			transferMenu.AppendMenu(MF_SEPARATOR);
#ifdef PPA_INCLUDE_DROP_SLOW
			transferMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
			transferMenu.AppendMenu(MF_SEPARATOR);
#endif
			transferMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(CLOSE_CONNECTION));
			transferMenu.SetMenuDefaultItem(IDC_PRIVATE_MESSAGE);
			
			if (!main && (i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				const ItemInfo* itemI = ctrlTransfers.getItemData(i);
				bCustomMenu = true;
				
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				usercmdsMenu.InsertSeparatorFirst(TSTRING(SETTINGS_USER_COMMANDS));
#endif
				
				// !SMT!-S
				reinitUserMenu(itemI->hintedUser.user, itemI->hintedUser.hint);
				
				if (getSelectedUser())
				{
					appendUcMenu(usercmdsMenu, UserCommand::CONTEXT_USER, ClientManager::getInstance()->getHubs(getSelectedUser()->getCID(), getSelectedHint()));
				}
				// end !SMT!-S
			}
			
			appendAndActivateUserItems(transferMenu);
			
#ifdef PPA_INCLUDE_DROP_SLOW
			segmentedMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_UNCHECKED);
			transferMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_UNCHECKED);
#endif
			transferMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, MFS_ENABLED);
			if (ii->download)
			{
#ifdef PPA_INCLUDE_DROP_SLOW
				transferMenu.EnableMenuItem(IDC_MENU_SLOWDISCONNECT, MFS_ENABLED);
#endif
				transferMenu.EnableMenuItem(IDC_PRIORITY_PAUSED, MFS_ENABLED); //[+] Drakon
				if (!ii->m_target.empty())
				{
					string target = Text::fromT(ii->m_target);
#ifdef PPA_INCLUDE_DROP_SLOW
					bool slowDisconnect;
					{
						QueueManager::LockFileQueueShared l_fileQueue;
						const QueueItem::QIStringMap& queue = l_fileQueue.getQueue();
						const auto qi = queue.find(&target);
						if (qi != queue.cend())
							slowDisconnect = qi->second->isAutoDrop(); // [!]
						else
							slowDisconnect = false;
					}
					if (slowDisconnect)
					{
						segmentedMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_CHECKED);
						transferMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_CHECKED);
					}
#endif
					setupPreviewMenu(target);
				}
			}
			else
			{
#ifdef PPA_INCLUDE_DROP_SLOW
				transferMenu.EnableMenuItem(IDC_MENU_SLOWDISCONNECT, MFS_DISABLED);
#endif
				transferMenu.EnableMenuItem(IDC_PRIORITY_PAUSED, MFS_DISABLED); //[+] Drakon
			}
			
			activatePreviewItems(transferMenu);
			
			if (!main)
			{
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				transferMenu.InsertSeparatorFirst(TSTRING(MENU_TRANSFERS));
#endif
				transferMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				transferMenu.RemoveFirstItem();
#endif
			}
			else
			{
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				segmentedMenu.InsertSeparatorFirst(TSTRING(SETTINGS_SEGMENT));
#endif
				segmentedMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
#ifdef OLD_MENU_HEADER //[~]JhaoDa
				segmentedMenu.RemoveFirstItem();
#endif
			}
			
			if (bCustomMenu)
			{
				usercmdsMenu.ClearMenu();
			}
			return TRUE;
		}
	}
	bHandled = FALSE;
	return FALSE;
}

void TransferView::runUserCommand(UserCommand& uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
		
	StringMap ucParams = ucLineParams;
	
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* itemI = ctrlTransfers.getItemData(i);
		if (itemI->hintedUser.user && itemI->hintedUser.user->isOnline())  // [!] IRainman fix.
		{
			StringMap tmp = ucParams;
			ucParams["fileFN"] = Text::fromT(itemI->m_target);
			
			// compatibility with 0.674 and earlier
			ucParams["file"] = ucParams["fileFN"];
			
			ClientManager::getInstance()->userCommand(itemI->hintedUser, uc, tmp, true);
		}
	}
}

LRESULT TransferView::onForce(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(i);
		ctrlTransfers.SetItemText(i, COLUMN_STATUS, CTSTRING(CONNECTING_FORCED));
		
		if (ii->parent == NULL && ii->hits != -1)
		{
			const vector<ItemInfo*>& children = ctrlTransfers.findChildren(ii->getGroupCond());
			for (auto j = children.cbegin(); j != children.cend(); ++j)
			{
				ItemInfo* ii = *j;
				
				int h = ctrlTransfers.findItem(ii);
				if (h != -1)
					ctrlTransfers.SetItemText(h, COLUMN_STATUS, CTSTRING(CONNECTING_FORCED));
					
				ii->transferFailed = false;
				ConnectionManager::getInstance()->force(ii->hintedUser);
			}
		}
		else
		{
			ii->transferFailed = false;
			ConnectionManager::getInstance()->force(ii->hintedUser);
		}
	}
	return 0;
}

void TransferView::ItemInfo::removeAll()
{
	if (hits <= 1) // TODO 2012-04-18_11-17-28_54VROXRMJ6YSH6HDVIHAPES3OCW6A42FKJLYBXI_F7657B36_crash-stack-r502-beta18-build-9768.dmp
	{
		QueueManager::getInstance()->removeSource(hintedUser, QueueItem::Source::FLAG_REMOVED);
	}
	else
	{
		QueueManager::getInstance()->remove(Text::fromT(m_target));
	}
}

LRESULT TransferView::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	// TODO - навтыкать профайлерные макросы и узнать что чаще зовется
	CRect rc;
	NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)pnmh;
	
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
		{
			// TODO - расчитать разово тут общие вещи для строки и сохранить в ItemInfo
			return CDRF_NOTIFYSUBITEMDRAW;
		}
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			ItemInfo* ii = reinterpret_cast<ItemInfo*>(cd->nmcd.lItemlParam);
			if (!ii) //[+]PPA падаем под wine
				return CDRF_DODEFAULT;
			const int colIndex = ctrlTransfers.findColumn(cd->iSubItem);
			cd->clrTextBk = Colors::bgColor;
			
			Colors::alternationBkColor(cd);
			
			if ((ii->status == ItemInfo::STATUS_RUNNING) && (colIndex == COLUMN_STATUS))
			{
				if (!BOOLSETTING(SHOW_PROGRESS_BARS))
				{
					bHandled = FALSE;
					return 0;
				}
				
				// Get the color of this bar
				COLORREF clr = SETTING(PROGRESS_OVERRIDE_COLORS) ?
				               (ii->download ? (!ii->parent ? SETTING(DOWNLOAD_BAR_COLOR) : SETTING(PROGRESS_SEGMENT_COLOR)) : SETTING(UPLOAD_BAR_COLOR)) :
					               GetSysColor(COLOR_HIGHLIGHT);
				if (!ii->download && BOOLSETTING(UP_TRANSFER_COLORS)) //[+]PPA
			{
					const auto l_NumSlot = ii->getUser()->getSlots();
					if (l_NumSlot != 0)
					{
						if (l_NumSlot < 5)
							clr = 0;
						else if (l_NumSlot < 10) //[+]PPA
							clr = 0x00AEAEAE;
					}
					else
						clr = 0x00FFD7FF;
				}
				//this is just severely broken, msdn says GetSubItemRect requires a one based index
				//but it wont work and index 0 gives the rect of the whole item
				if (cd->iSubItem == 0)
				{
					//use LVIR_LABEL to exclude the icon area since we will be painting over that
					//later
					ctrlTransfers.GetItemRect((int)cd->nmcd.dwItemSpec, &rc, LVIR_LABEL);
				}
				else
				{
					ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, &rc);
				}
				
				/* Thanks & credits for Stealthy style go to phaedrus */
				const bool useStealthyStyle = BOOLSETTING(STEALTHY_STYLE);
				const bool useODCstyle = BOOLSETTING(PROGRESSBAR_ODC_STYLE);
				//bool isMain = (!ii->parent || !ii->download);
				//bool isSmaller = (!ii->parent && ii->collapsed == false);
				
				/* fixes issues with double border
				if (useStealthyStyle)
				{
				    //rc.top -= 1;
				    //if(isSmaller)
				    //rc.bottom -= 1;
				}*/ //[-] Sergey Shuhskanov
				
				// Real rc, the original one.
				CRect real_rc = rc;
				// We need to offset the current rc to (0, 0) to paint on the New dc
				rc.MoveToXY(0, 0);
				
				CRect rc4;
				CRect rc2;
				rc2 = rc;
				
				// Text rect
				if (BOOLSETTING(STEALTHY_STYLE_ICO))
				{
					rc2.left += 22; // indented for icon and text
					rc2.right -= 2; // and without messing with the border of the cell
					//rc2.top -= 2; // and fix text vertical alignment
					// Background rect
					rc4 = rc;
				}
				else
				{
					rc2 = rc;
					rc2.left += 6; // indented with 6 pixels
					rc2.right -= 2; // and without messing with the border of the cell
					// Background rect
					rc4 = rc;
					//rc2.left += 9;
				}
				
				CDC cdc;
				cdc.CreateCompatibleDC(cd->nmcd.hdc);
				HBITMAP hBmp = CreateCompatibleBitmap(cd->nmcd.hdc,  real_rc.Width(),  real_rc.Height());
				
				HBITMAP pOldBmp = cdc.SelectBitmap(hBmp);
				HDC& dc = cdc.m_hDC;
				
				const COLORREF barPal[3] = { HLS_TRANSFORM(clr, -40, 50), clr, HLS_TRANSFORM(clr, 20, -30) };
				const COLORREF barPal2[3] = { HLS_TRANSFORM(clr, -15, 0), clr, HLS_TRANSFORM(clr, 15, 0) };
				COLORREF oldcol;
				// The value throws off, usually with about 8-11 (usually negatively f.ex. in src use 190, the change might actually happen already at aprox 180)
				const  HLSCOLOR hls = RGB2HLS(clr);
				LONG top = rc2.top + (rc2.Height() - WinUtil::getTextHeight(cd->nmcd.hdc) - 1) / 2 + 1;
				
				const HFONT oldFont = (HFONT)SelectObject(dc, Fonts::systemFont); //font -> systemfont [~]Sergey Shushkanov
				SetBkMode(dc, TRANSPARENT);
				
				// Get the color of this text bar - this way it ends up looking nice imo.
				if (!useStealthyStyle)
				{
					oldcol = ::SetTextColor(dc, SETTING(PROGRESS_OVERRIDE_COLORS2) ?
					                        (ii->download ? SETTING(PROGRESS_TEXT_COLOR_DOWN) : SETTING(PROGRESS_TEXT_COLOR_UP)) :
						                        OperaColors::TextFromBackground(clr));
				}
				else
			{
					if (clr == RGB(255, 255, 255)) // see if user is using white as clr, rare but you may never know
						oldcol = ::SetTextColor(dc, RGB(0, 0, 0));
					else
						oldcol = ::SetTextColor(dc, barPal2[1]);
				}
				
				// Draw the background and border of the bar
				if (ii->size == 0) ii->size = 1;
				
				if (useODCstyle || useStealthyStyle)
				{
					// New style progressbar tweaks the current colors
					const HLSTRIPLE hls_bk = OperaColors::RGB2HLS(cd->clrTextBk);
					
					// Create pen (ie outline border of the cell)
					HPEN penBorder = ::CreatePen(PS_SOLID, 1, OperaColors::blendColors(cd->clrTextBk, clr, (hls_bk.hlstLightness > 0.75) ? 0.6 : 0.4));
					HGDIOBJ pOldPen = ::SelectObject(dc, penBorder);
					
					// Draw the outline (but NOT the background) using pen
					HBRUSH hBrOldBg = CreateSolidBrush(cd->clrTextBk);
					hBrOldBg = (HBRUSH)::SelectObject(dc, hBrOldBg);
					
					if (useStealthyStyle)
						::Rectangle(dc, rc4.left, rc4.top, rc4.right, rc4.bottom);
					else
						::Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
						
					DeleteObject(::SelectObject(dc, hBrOldBg));
					
					// Set the background color, by slightly changing it
					HBRUSH hBrDefBg = CreateSolidBrush(OperaColors::blendColors(cd->clrTextBk, clr, (hls_bk.hlstLightness > 0.75) ? 0.85 : 0.70));
					HGDIOBJ oldBg = ::SelectObject(dc, hBrDefBg);
					
					// Draw the outline AND the background using pen+brush
					if (useStealthyStyle)
						::Rectangle(dc, rc4.left, rc4.top, rc4.left + (LONG)(rc4.Width() * ii->getProgressPosition() + 0.5), rc4.bottom);
					else
						::Rectangle(dc, rc.left, rc.top, rc.left + (LONG)(rc.Width() * ii->getProgressPosition() + 0.5), rc.bottom);
						
					if (useStealthyStyle)
					{
						// Draw the text over entire item
						::ExtTextOut(dc, rc2.left, top, ETO_CLIPPED, rc2, ii->getText(COLUMN_STATUS).c_str(), ii->getText(COLUMN_STATUS).length(), NULL);
						
						rc.right = rc.left + (int)(((int64_t)rc.Width()) * ii->actual / ii->size);
						
						if (ii->pos != 0)
							rc.bottom -= 1;
							
						rc.top += 1;
						
						//create bar pen
						if (HLS_S(hls) <= 30) // good values would be 20-30
							penBorder = ::CreatePen(PS_SOLID, 1, barPal2[0]);
						else
							penBorder = ::CreatePen(PS_SOLID, 1, barPal[0]);
							
						DeleteObject(::SelectObject(dc, penBorder));
						
						//create bar brush
						hBrDefBg = CreateSolidBrush(barPal[1]);
						
						DeleteObject(::SelectObject(dc, hBrDefBg));
						
						//draw bar
						::Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
						
						//draw bar highlight
						if (rc.Width() > 4)
						{
							DeleteObject(SelectObject(cdc, CreatePen(PS_SOLID, 1, barPal[2])));
							rc.top += 2;
							::MoveToEx(cdc, rc.left + 2, rc.top, (LPPOINT)NULL);
							::LineTo(cdc, rc.right - 2, rc.top);
						}
					}
					// Reset pen
					DeleteObject(::SelectObject(dc, pOldPen));
					// Reset bg (brush)
					DeleteObject(::SelectObject(dc, oldBg));
				}
				
				// Draw the background and border of the bar
				if (!useODCstyle && !useStealthyStyle)
				{
					CBarShader statusBar(rc.bottom - rc.top, rc.right - rc.left, SETTING(PROGRESS_BACK_COLOR), ii->size);
					
					rc.right = rc.left + (int)(rc.Width() * ii->pos / ii->size);
					if (!ii->download)
					{
						statusBar.FillRange(0, ii->actual, HLS_TRANSFORM(clr, -20, 30));
						statusBar.FillRange(ii->actual, ii->actual,  clr);
					}
					else
					{
						statusBar.FillRange(0, ii->actual, clr);
						if (ii->parent)
							statusBar.FillRange(ii->actual, ii->actual, SETTING(PROGRESS_SEGMENT_COLOR));
					}
					if (ii->pos > ii->actual)
						statusBar.FillRange(ii->actual, ii->pos, SETTING(PROGRESS_COMPRESS_COLOR));
						
					statusBar.Draw(cdc, rc.top, rc.left, SETTING(PROGRESS_3DDEPTH));
				}
				else
				{
					if (!useStealthyStyle)
					{
						int right = rc.left + (int)((int64_t)rc.Width() * ii->actual / ii->size);
						COLORREF a, b;
						OperaColors::EnlightenFlood(clr, a, b);
						OperaColors::FloodFill(cdc, rc.left + 1, rc.top + 1, right, rc.bottom - 1, a, b, BOOLSETTING(PROGRESSBAR_ODC_BUMPED));
					}
				}
				
				if (BOOLSETTING(STEALTHY_STYLE_ICO))
				{
					// Draw icon - Nasty way to do the filelist icon, but couldn't get other ways to work well,
					// TODO: do separating filelists from other transfers the proper way...
					if (ii->isFileList())
					{
						DrawIconEx(dc, rc2.left - 20, rc2.top, g_user_icon, 16, 16, NULL, NULL, DI_NORMAL | DI_COMPAT);
					}
					else if (ii->status == ItemInfo::STATUS_RUNNING)
					{
						RECT rc9 = rc2;
						rc9.left -= 19; //[~] Sergey Shushkanov
						rc9.top += 1; //[~] Sergey Shushkanov
						rc9.right = rc9.left + 16;
						rc9.bottom = rc9.top + 16; //[~] Sergey Shushkanov
						
						int64_t speedmark;
						if (!BOOLSETTING(THROTTLE_ENABLE))
						{
							const int64_t speedignore = Util::toInt64(SETTING(UPLOAD_SPEED));
speedmark = BOOLSETTING(STEALTHY_STYLE_ICO_SPEEDIGNORE) ? (ii->download ? SETTING(TOP_SPEED) : SETTING(TOP_UP_SPEED)) / 5 : speedignore * 20;
						}
						else
						{
							if (!ii->download)
							{
								speedmark = ThrottleManager::getInstance()->getUploadLimitInKBytes() / 5;
							}
							else
							{
								speedmark = ThrottleManager::getInstance()->getDownloadLimitInKBytes() / 5;
							}
						}
						CImageList & l_images = HLS_S(hls > 30) || HLS_L(hls) < 70 ? m_speedImages : m_speedImagesBW;
						
						const int64_t speedkb = ii->m_speed / 1000;
						if (speedkb >= speedmark * 4)
							l_images.DrawEx(4, dc, rc9, CLR_DEFAULT, CLR_DEFAULT, ILD_IMAGE);
						else if (speedkb >= speedmark * 3)
							l_images.DrawEx(3, dc, rc9, CLR_DEFAULT, CLR_DEFAULT, ILD_IMAGE);
						else if (speedkb >= speedmark * 2)
							l_images.DrawEx(2, dc, rc9, CLR_DEFAULT, CLR_DEFAULT, ILD_IMAGE);
						else if (speedkb >= speedmark * 1.5)
							l_images.DrawEx(1, dc, rc9, CLR_DEFAULT, CLR_DEFAULT, ILD_IMAGE);
						else
							l_images.DrawEx(0, dc, rc9, CLR_DEFAULT, CLR_DEFAULT, ILD_IMAGE);
					}
				}
				if (useStealthyStyle)
				{
					// use white to as many colors as possible (values might need some tweaking), I didn't like TextFromBackground...
					if (((HLS_L(hls) > 190) && (HLS_S(hls) <= 30)) || (HLS_L(hls) > 211))
						oldcol = ::SetTextColor(dc, HLS_TRANSFORM(clr, -40, 0));
					else
						oldcol = ::SetTextColor(dc, RGB(255, 255, 255));
					rc2.right = rc.right;
				}
				
				// Draw the text, the other stuff here was moved upwards due to stealthy style being added
				::ExtTextOut(dc, rc2.left, top, ETO_CLIPPED, rc2, ii->getText(COLUMN_STATUS).c_str(), ii->getText(COLUMN_STATUS).length(), NULL);
				
				SelectObject(dc, oldFont);
				::SetTextColor(dc, oldcol);
				
				// New way:
				BitBlt(cd->nmcd.hdc, real_rc.left, real_rc.top, real_rc.Width(), real_rc.Height(), dc, 0, 0, SRCCOPY);
				DeleteObject(cdc.SelectBitmap(pOldBmp));
				
				//bah crap, if we return CDRF_SKIPDEFAULT windows won't paint the icons
				//so we have to do it
				if (cd->iSubItem == 0)
				{
					LVITEM lvItem = {0};
					lvItem.iItem = cd->nmcd.dwItemSpec;
					lvItem.iSubItem = 0;
					lvItem.mask = LVIF_IMAGE | LVIF_STATE;
					lvItem.stateMask = LVIS_SELECTED;
					ctrlTransfers.GetItem(&lvItem);
					
					HIMAGELIST imageList = (HIMAGELIST)::SendMessage(ctrlTransfers.m_hWnd, LVM_GETIMAGELIST, LVSIL_SMALL, 0);
					if (imageList)
					{
						//let's find out where to paint it
						//and draw the background to avoid having
						//the selection color as background
						CRect iconRect;
						ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, 0, LVIR_ICON, iconRect);
						ImageList_Draw(imageList, lvItem.iImage, cd->nmcd.hdc, iconRect.left, iconRect.top, ILD_TRANSPARENT);
					}
				}
				return CDRF_SKIPDEFAULT;
			}
			else if (colIndex == COLUMN_LOCATION)   // !SMT!-IP
			{
				ItemInfo* ii = (ItemInfo*)cd->nmcd.lItemlParam;
				if (!ii) //[+]PPA падаем под wine
					return CDRF_DODEFAULT;
				ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				COLORREF color;
				if (ctrlTransfers.GetItemState((int)cd->nmcd.dwItemSpec, LVIS_SELECTED) & LVIS_SELECTED)
				{
					if (ctrlTransfers.m_hWnd == ::GetFocus())
					{
						color = GetSysColor(COLOR_HIGHLIGHT);
						SetBkColor(cd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHT));
						SetTextColor(cd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
					}
					else
					{
						color = GetBkColor(cd->nmcd.hdc);
						SetBkColor(cd->nmcd.hdc, color);
					}
				}
				else
				{
					color = Colors::getAlternativBkColor(cd);
					SetBkColor(cd->nmcd.hdc, color);
					SetTextColor(cd->nmcd.hdc, Colors::textColor);
				}
				CRect rc2 = rc;
				rc2.left += 2;
				HGDIOBJ oldpen = ::SelectObject(cd->nmcd.hdc, CreatePen(PS_SOLID, 0, color));
				HGDIOBJ oldbr = ::SelectObject(cd->nmcd.hdc, CreateSolidBrush(color));
				Rectangle(cd->nmcd.hdc, rc.left, rc.top, rc.right, rc.bottom);
				
				DeleteObject(::SelectObject(cd->nmcd.hdc, oldpen));
				DeleteObject(::SelectObject(cd->nmcd.hdc, oldbr));
				
				const wstring &str = ii->getText(colIndex);
				
				if (!str.empty())
				{
					LONG top = rc2.top + (rc2.Height() - 15) / 2;
					if ((top - rc2.top) < 2)
						top = rc2.top + 1;
					// TODO fix copy-paste
					if (ii->m_location.isKnown())
					{
						int l_step = 0;
						if (BOOLSETTING(ENABLE_COUNTRYFLAG) && ii->m_location.getCountryIndex())
						{
							const POINT ps = { rc2.left, top };
							g_flagImage.DrawCountry(cd->nmcd.hdc, ii->m_location, ps);
							l_step += 25;
						}
						const POINT p = { rc2.left + l_step, top };
						if (ii->m_location.getFlagIndex())
						{
							g_flagImage.DrawLocation(cd->nmcd.hdc, ii->m_location, p);
							l_step += 25;
						}
						top = rc2.top + (rc2.Height() - WinUtil::getTextHeight(cd->nmcd.hdc) - 1) / 2;
						const auto& l_desc = ii->m_location.getDescription();
						::ExtTextOut(cd->nmcd.hdc, rc2.left + l_step + 5, top + 1, ETO_CLIPPED, rc2, l_desc.c_str(), l_desc.length(), nullptr);
					}
					else
					{
						top = rc2.top + (rc2.Height() - WinUtil::getTextHeight(cd->nmcd.hdc) - 1) / 2;
						::ExtTextOut(cd->nmcd.hdc, rc2.left + 30, top + 1, ETO_CLIPPED, rc2, str.c_str(), str.size(), nullptr);
					}
					return CDRF_SKIPDEFAULT;
				}
			}
			else if (colIndex != COLUMN_USER && colIndex != COLUMN_HUB && colIndex != COLUMN_STATUS && colIndex != COLUMN_LOCATION &&
			         ii->status != ItemInfo::STATUS_RUNNING)
			{
				cd->clrText = OperaColors::blendColors(Colors::bgColor, Colors::textColor, 0.4);
				return CDRF_NEWFONT;
			}
			// Fall through
		}
		default:
			return CDRF_DODEFAULT;
	}
}

LRESULT TransferView::onDoubleClickTransfers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	if (item->iItem != -1)
	{
		CRect rect;
		ctrlTransfers.GetItemRect(item->iItem, rect, LVIR_ICON);
		
		// if double click on state icon, ignore...
		if (item->ptAction.x < rect.left)
			return 0;
			
		ItemInfo* i = ctrlTransfers.getItemData(item->iItem);
		const vector<ItemInfo*>& children = ctrlTransfers.findChildren(i->getGroupCond());
		if (i->parent != nullptr || children.size() <= 1)
		{
			switch (SETTING(TRANSFERLIST_DBLCLICK))
			{
				case 0:
					i->pm(i->hintedUser.hint); // [!] IRainman fix.
					break;
				case 1:
					i->getList(); // [!] IRainman fix.
					break;
				case 2:
					i->matchQueue(); // [!] IRainman fix.
				case 3:
					i->grant(i->hintedUser.hint); // [!] IRainman fix.
					break;
				case 4:
					i->addFav();
					break;
				case 5: // !SMT!-UI
					i->statusString = TSTRING(CONNECTING_FORCED);
					ctrlTransfers.updateItem(i);
					ClientManager::getInstance()->connect(i->hintedUser, Util::toString(Util::rand())); // [!] IRainman fix.
					break;
				case 6:
					i->browseList(); // [!] IRainman fix.
					break;
			}
		}
	}
	return 0;
}

int TransferView::ItemInfo::compareItems(const ItemInfo* a, const ItemInfo* b, uint8_t col)
{
	if (a->status == b->status)
	{
		if (a->download != b->download)
		{
			return a->download ? -1 : 1;
		}
	}
	else
	{
		return (a->status == ItemInfo::STATUS_RUNNING) ? -1 : 1;
	}
	
	switch (col)
	{
		case COLUMN_USER:
			return a->hits == b->hits ? a->getText(COLUMN_USER).compare(b->getText(COLUMN_USER)) : compare(a->hits, b->hits); // [!] IRainman opt.
		case COLUMN_HUB:
			return a->running == b->running ? a->getText(COLUMN_HUB).compare(b->getText(COLUMN_HUB)) : compare(a->running, b->running); // [!] IRainman opt.
		case COLUMN_STATUS:
			return compare(a->getProgressPosition(), b->getProgressPosition());
		case COLUMN_TIMELEFT:
			return compare(a->timeLeft, b->timeLeft);
		case COLUMN_SPEED:
			return compare(a->m_speed, b->m_speed);
		case COLUMN_SIZE:
			return compare(a->size, b->size);
#ifdef PPA_INCLUDE_COLUMN_RATIO
			//case COLUMN_RATIO:
			//  return compare(a->getRatio(), b->getRatio());
#endif
		case COLUMN_SHARE:
			return a->getUser() && b->getUser() ? compare(a->getUser()->getBytesShared(), b->getUser()->getBytesShared()) : 0;
		case COLUMN_SLOTS:
			return compare(Util::toInt(a->getText(col)), Util::toInt(b->getText(col).c_str()));
		case COLUMN_IP:
			return compare(Socket::convertIP4(Text::fromT(a->getText(COLUMN_IP))), Socket::convertIP4(Text::fromT(b->getText(COLUMN_IP))));
		default:
			return a->getText(col).compare(b->getText(col)); // [!] IRainman opt.
	}
}

TransferView::ItemInfo* TransferView::findItem(const UpdateInfo& ui, int& pos) const
{
	for (int j = 0; j < ctrlTransfers.GetItemCount(); ++j)
	{
		ItemInfo* ii = ctrlTransfers.getItemData(j);
		if (ui == *ii)
		{
			pos = j;
			return ii;
		}
		else if (ui.download == ii->download && !ii->parent) // [!] IRainman fix.
		{
			const vector<ItemInfo*>& children = ctrlTransfers.findChildren(ii->getGroupCond());
			for (auto k = children.cbegin(); k != children.cend(); ++k)
			{
				ItemInfo* ii = *k;
				if (ui == *ii)
				{
					return ii;
				}
			}
		}
	}
	return nullptr;
}

LRESULT TransferView::onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	TaskQueue::List t;
	tasks.get(t);
	
	if (t.empty()) // [+] IRainman opt
		return 0;
		
	CLockRedraw<> l_lock_draw(ctrlTransfers);
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		switch (i->first)
		{
			case ADD_ITEM:
			{
				auto &ui = static_cast<UpdateInfo&>(*i->second);
				ItemInfo* ii = new ItemInfo(ui.hintedUser, ui.download);
				ii->update(ui);
				if (ii->download)
				{
					ctrlTransfers.insertGroupedItem(ii, false);
				}
				else
				{
					ctrlTransfers.insertItem(ii, IMAGE_UPLOAD);
				}
			}
			break;
			case REMOVE_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
				
				int pos = -1;
				ItemInfo* ii = findItem(ui, pos);
				if (ii)
				{
					if (ui.download)
					{
						ctrlTransfers.removeGroupedItem(ii);
					}
					else
					{
						dcassert(pos != -1);
						ctrlTransfers.DeleteItem(pos);
						delete ii;
					}
				}
			}
			break;
			case UPDATE_ITEM:
			{
				auto &ui = static_cast<UpdateInfo&>(*i->second);
				
				int pos = -1;
				ItemInfo* ii = findItem(ui, pos);
				if (ii)
				{
					if (ui.download)
					{
						ItemInfo* parent = ii->parent ? ii->parent : ii;
						
						if (ui.type == Transfer::TYPE_FILE || ui.type == Transfer::TYPE_TREE)
						{
							/* parent item must be updated with correct info about whole file */
							if (ui.status == ItemInfo::STATUS_RUNNING && parent->status == ItemInfo::STATUS_RUNNING && parent->hits == -1)
							{
								ui.updateMask &= ~UpdateInfo::MASK_POS;
								ui.updateMask &= ~UpdateInfo::MASK_ACTUAL;
								ui.updateMask &= ~UpdateInfo::MASK_SIZE;
								ui.updateMask &= ~UpdateInfo::MASK_STATUS_STRING;
								ui.updateMask &= ~UpdateInfo::MASK_TIMELEFT;
							}
						}
						
						/* if target has changed, regroup the item */
						const bool changeParent = (ui.updateMask & UpdateInfo::MASK_FILE) && (ui.m_target != ii->m_target);
						if (changeParent)
							ctrlTransfers.removeGroupedItem(ii, false);
							
						ii->update(ui);
						
						if (changeParent)
						{
							ctrlTransfers.insertGroupedItem(ii, false);
							parent = ii->parent ? ii->parent : ii;
						}
						else if (ii == parent || !parent->collapsed)
						{
							updateItem(ctrlTransfers.findItem(ii), ui.updateMask);
						}
						break;
					}
					ii->update(ui);
					dcassert(pos != -1);
					updateItem(pos, ui.updateMask);
				}
			}
			break;
			case UPDATE_PARENT:
			case UPDATE_PARENT_WITH_PARSE:
				//case UPDATE_PARENT_WITH_PARSE_2:
			{
				switch (i->first)
				{
					case UPDATE_PARENT_WITH_PARSE:
						parseQueueItemUpdateInfo(reinterpret_cast<QueueItemUpdateInfo*>(i->second)); // [!] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1082
						break;
						/*
						                    case UPDATE_PARENT_WITH_PARSE_2:
						                        {
						                            auto qiui = reinterpret_cast<QueueItemUpdateInfo*>(i->second);
						                            SharedLock l(QueueItem::cs);
						                            for (auto it = qiui->m_queueItem->getSourcesL().begin(); it != qiui->m_queueItem->getSourcesL().end(); ++it)
						                            {
						                                auto ui = new UpdateInfo(it->getUser(), true);
						
						                                ui->setTarget(qiui->m_queueItem->getTarget());
						                                ui->setPos(0);
						                                ui->setActual(0);
						                                ui->setTimeLeft(0);
						                                ui->setStatusString(TSTRING(DISCONNECTED));
						                                ui->setStatus(ItemInfo::STATUS_WAITING);
						                                ui->setRunning(0);
						
						                                tasks.add(UPDATE_PARENT, ui);
						                            }
						                        }
						
						                        break;
						*/
				}
				const auto& ui = static_cast<UpdateInfo&>(*i->second);
				const ItemInfoList::ParentPair* pp = ctrlTransfers.findParentPair(ui.m_target);
				
				if (!pp)
					break;
					
				if (ui.hintedUser.user)
				{
					int pos = -1;
					ItemInfo* ii = findItem(ui, pos);
					if (ii)
					{
						ii->status = ui.status;
						ii->statusString = ui.statusString;
						
						if (!pp->parent->collapsed)
						{
							updateItem(ctrlTransfers.findItem(ii), ui.updateMask);
						}
					}
				}
				
				pp->parent->update(ui);
				updateItem(ctrlTransfers.findItem(pp->parent), ui.updateMask);
			}
			break;
			default:
				dcassert(0);
				break;
		};
		delete i->second;  // [1] https://www.box.net/shared/307aa981b9cef05fc096
	}
	
	ctrlTransfers.resort();
	m_spoken = false; // [+] IRainman opt.
	return 0; //TODO crash
}

LRESULT TransferView::onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *l_ii = ctrlTransfers.getItemData(i);
		TTHValue l_tth;
		if (l_ii && getTTH(l_ii, l_tth))
			WinUtil::searchHash(l_tth);
			
	}
	
	return 0;
}

void TransferView::ItemInfo::update(const UpdateInfo& ui)
{
	if (ui.type != Transfer::TYPE_LAST)
		type = ui.type;
		
	if (ui.updateMask & UpdateInfo::MASK_STATUS)
	{
		status = ui.status;
	}
	if (ui.updateMask & UpdateInfo::MASK_STATUS_STRING)
	{
		// No slots etc from transfermanager better than disconnected from connectionmanager
		if (!transferFailed)
			statusString = ui.statusString;
		transferFailed = ui.transferFailed;
	}
	if (ui.updateMask & UpdateInfo::MASK_SIZE)
	{
		size = ui.size;
	}
	if (ui.updateMask & UpdateInfo::MASK_POS)
	{
		pos = ui.pos;
	}
	if (ui.updateMask & UpdateInfo::MASK_ACTUAL)
	{
		actual = ui.actual;
		//[-]PPA        columns[COLUMN_RATIO] = Util::toStringW(getRatio());
		//[?]       columns[COLUMN_SHARE] = Util::formatBytesW(ui.user->getBytesShared());
	}
	if (ui.updateMask & UpdateInfo::MASK_SPEED)
	{
		m_speed = ui.speed;
	}
	if (ui.updateMask & UpdateInfo::MASK_FILE)
	{
		m_target = ui.m_target;
		m_isFilelist = ui.m_isFilelist;
	}
	if (ui.updateMask & UpdateInfo::MASK_TIMELEFT)
	{
		timeLeft = ui.timeLeft;
	}
	if (ui.updateMask & UpdateInfo::MASK_IP)
	{
		dcassert(!ui.ip.empty()); // http://code.google.com/p/flylinkdc/issues/detail?id=1298
		// dcassert(ip.empty()); TODO: fix me please.
		if (ip.empty()) // [+] IRainman fix: if IP is set already, not try to set twice. IP can not change during a single connection.
		{
			ip = ui.ip;
#ifdef PPA_INCLUDE_COLUMN_RATIO
			m_ratio_as_text = ui.hintedUser.user->getUDratio();// [+] brain-ripper
#endif
#ifdef PPA_INCLUDE_DNS
			columns[COLUMN_DNS] = ui.dns; // !SMT!-IP
#endif
		}
	}
	if (ui.updateMask & UpdateInfo::MASK_CIPHER)
	{
		dcassert(cipher.empty()); // [+] IRainman fix: if cipher is set already, not try to set twice. Cipher can not change during a single connection.
		cipher = ui.cipher;
	}
	if (ui.updateMask & UpdateInfo::MASK_SEGMENT)
	{
		running = ui.running;
	}
}

void TransferView::updateItem(int ii, uint32_t updateMask)
{
	if (updateMask & UpdateInfo::MASK_STATUS || updateMask & UpdateInfo::MASK_STATUS_STRING ||
	        updateMask & UpdateInfo::MASK_POS || updateMask & UpdateInfo::MASK_ACTUAL)
	{
		ctrlTransfers.updateItem(ii, COLUMN_STATUS);
	}
#ifdef PPA_INCLUDE_COLUMN_RATIO
	if (updateMask & UpdateInfo::MASK_POS || updateMask & UpdateInfo::MASK_ACTUAL)
	{
		ctrlTransfers.updateItem(ii, COLUMN_RATIO);
	}
#endif
	if (updateMask & UpdateInfo::MASK_SIZE)
	{
		ctrlTransfers.updateItem(ii, COLUMN_SIZE);
	}
	if (updateMask & UpdateInfo::MASK_SPEED)
	{
		ctrlTransfers.updateItem(ii, COLUMN_SPEED);
	}
	if (updateMask & UpdateInfo::MASK_FILE)
	{
		ctrlTransfers.updateItem(ii, COLUMN_PATH);
		ctrlTransfers.updateItem(ii, COLUMN_FILE);
	}
	if (updateMask & UpdateInfo::MASK_TIMELEFT)
	{
		ctrlTransfers.updateItem(ii, COLUMN_TIMELEFT);
	}
	if (updateMask & UpdateInfo::MASK_IP)
	{
		ctrlTransfers.updateItem(ii, COLUMN_IP);
	}
	if (updateMask & UpdateInfo::MASK_SEGMENT)
	{
		ctrlTransfers.updateItem(ii, COLUMN_HUB);
	}
	if (updateMask & UpdateInfo::MASK_CIPHER)
	{
		ctrlTransfers.updateItem(ii, COLUMN_CIPHER);
	}
}

TransferView::UpdateInfo* TransferView::createUpdateInfoForAddedEvent(const ConnectionQueueItem* aCqi) // [+] IRainman fix.
{
	UpdateInfo* ui = new UpdateInfo(aCqi->getUser(), aCqi->getDownload());
	if (ui->download)
	{
		string target;
		int64_t size;
		int flags;
		if (QueueManager::getInstance()->getQueueInfo(aCqi->getUser(), target, size, flags))
		{
			Transfer::Type type = Transfer::TYPE_FILE;
			if (flags & QueueItem::FLAG_USER_LIST)
				type = Transfer::TYPE_FULL_LIST;
			else if (flags & QueueItem::FLAG_DCLST_LIST)
				type = Transfer::TYPE_FULL_LIST;
			else if (flags & QueueItem::FLAG_PARTIAL_LIST)
				type = Transfer::TYPE_PARTIAL_LIST;
				
			ui->setType(type);
			ui->setTarget(target);
			ui->setSize(size);
		}
	}
	
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setStatusString(TSTRING(CONNECTING));
	
	return ui;
}

void TransferView::on(ConnectionManagerListener::Added, const ConnectionQueueItem* aCqi)
{
	tasks.add(ADD_ITEM, createUpdateInfoForAddedEvent(aCqi)); // [!] IRainman fix.
}

void TransferView::on(ConnectionManagerListener::StatusChanged, const ConnectionQueueItem* aCqi)
{
	tasks.add(UPDATE_ITEM, createUpdateInfoForAddedEvent(aCqi)); // [!] IRainman fix.
}

void TransferView::on(ConnectionManagerListener::Removed, const ConnectionQueueItem* aCqi)
{
	tasks.add(REMOVE_ITEM, new UpdateInfo(aCqi->getUser(), aCqi->getDownload())); // [!] IRainman fix.
}

void TransferView::on(ConnectionManagerListener::Failed, const ConnectionQueueItem* aCqi, const string& aReason)
{
	UpdateInfo* ui = new UpdateInfo(aCqi->getUser(), aCqi->getDownload()); // [!] IRainman fix.
#ifdef PPA_INCLUDE_IPFILTER
	if (ui->hintedUser.user->isSet(User::PG_BLOCK))
	{
		ui->setStatusString(TSTRING(CONNECTION_BLOCKED) + _T(" [IPTrust.ini]"));
	}
	else
#endif
	{
		ui->setStatusString(Text::toT(aReason));
	}
	
	ui->setStatus(ItemInfo::STATUS_WAITING);
	tasks.add(UPDATE_ITEM, ui);
}

static tstring getFile(const Transfer::Type& type, const tstring& fileName)
{
	tstring file;
	
	if (type == Transfer::TYPE_TREE)
	{
		file = _T("TTH: ") + fileName;
	}
	else if (type == Transfer::TYPE_FULL_LIST || type == Transfer::TYPE_PARTIAL_LIST)
	{
		file = TSTRING(FILE_LIST);
	}
	else
	{
		file = fileName;
	}
	return file;
}

const tstring TransferView::ItemInfo::getText(uint8_t col) const
{
	switch (col)
	{
		case COLUMN_USER:
			return (hits == -1) ? WinUtil::getNicks(hintedUser) : (Util::toStringW(hits) + _T(' ') + TSTRING(USERS));
		case COLUMN_HUB:
			return (hits == -1) ? WinUtil::getHubNames(hintedUser).first : (Util::toStringW(running) + _T(' ') + TSTRING(NUMBER_OF_SEGMENTS));
		case COLUMN_STATUS:
			return statusString;
		case COLUMN_TIMELEFT:
			return (status == STATUS_RUNNING) ? Util::formatSecondsW(timeLeft) : Util::emptyStringT;
		case COLUMN_SPEED:
			return (status == STATUS_RUNNING) ? (Util::formatBytesW(m_speed) + _T('/') + WSTRING(S)) : Util::emptyStringT;
		case COLUMN_FILE:
			return getFile(type, Util::getFileName(m_target)); // TODO: opt me please.
		case COLUMN_SIZE:
			return Util::formatBytesW(size); // TODO: opt me please.
		case COLUMN_PATH:
			return Util::getFilePath(m_target); // TODO: opt me please.
		case COLUMN_IP:
			return ip;
#ifdef PPA_INCLUDE_COLUMN_RATIO
			// [~] brain-ripper
			//case COLUMN_RATIO: return (status == STATUS_RUNNING) ? Util::toStringW(ratio()) : Util::emptyStringT;
		case COLUMN_RATIO:
			return m_ratio_as_text;
#endif
		case COLUMN_CIPHER:
			return cipher;
		case COLUMN_SHARE:
			return hintedUser.user ? Util::formatBytesW(hintedUser.user->getBytesShared()) : Util::emptyStringT;
		case COLUMN_SLOTS:
			return hintedUser.user ? Util::toStringW(hintedUser.user->getSlots()) : Util::emptyStringT;
		case COLUMN_LOCATION:
		{
			// [!] IRainman opt: no need to get geolocation before the user sees it.
			if (!m_location.isSet() && !ip.empty()) // [!] IRainman opt: Prevent multiple repeated requests to the database if the location has not been found!
			{
				m_location = Util::getIpCountry(Text::fromT(ip));
			}
			
			return m_location.isKnown() ? m_location.getDescription() : Util::emptyStringT;
		}
		default:
			return Util::emptyStringT;
	}
}

void TransferView::starting(UpdateInfo* ui, const Transfer* t)
{
	ui->setPos(t->getPos());
	ui->setTarget(t->getPath());
	ui->setType(t->getType());
	const UserConnection& uc = t->getUserConnection();
	ui->setCipher(uc.getCipherName());
	
	const string& ip = uc.getRemoteIp();
	
	ui->setIP(ip);
}

void TransferView::on(DownloadManagerListener::Requesting, const Download* d) noexcept
{
	UpdateInfo* ui = new UpdateInfo(d->getHintedUser(), true); // [!] IRainman fix.
	
	starting(ui, d);
	
	ui->setActual(d->getActual());
	ui->setSize(d->getSize());
	ui->setStatus(ItemInfo::STATUS_RUNNING);
	ui->updateMask &= ~UpdateInfo::MASK_STATUS; // hack to avoid changing item status
	ui->setStatusString(TSTRING(REQUESTING) + _T(' ') + getFile(d->getType(), Text::toT(Util::getFileName(d->getPath()))) + _T("..."));
	
	tasks.add(UPDATE_ITEM, ui);
}

void TransferView::on(DownloadManagerListener::Starting, const Download* aDownload)
{
	UpdateInfo* ui = new UpdateInfo(aDownload->getHintedUser(), true); // [!] IRainman fix.
	
	ui->setStatus(ItemInfo::STATUS_RUNNING);
	ui->setStatusString(TSTRING(DOWNLOAD_STARTING));
	ui->setTarget(aDownload->getPath());
	ui->setType(aDownload->getType());
	// [-] ui->setIP(aDownload->getUserConnection().getRemoteIp()); // !SMT!-IP [-] IRainman opt.
	
	tasks.add(UPDATE_ITEM, ui);
}

void TransferView::on(DownloadManagerListener::Tick, const DownloadList& dl, uint64_t CurrentTick)
{
	if (!MainFrame::isAppMinimized())// [+]IRainman opt
	{
		for (auto j = dl.cbegin(); j != dl.cend(); ++j)
		{
			Download* d = *j;
			
			UpdateInfo* ui = new UpdateInfo(d->getHintedUser(), true); // [!] IRainman fix.
			ui->setStatus(ItemInfo::STATUS_RUNNING);
			ui->setActual(d->getActual());
			ui->setPos(d->getPos());
			ui->setSize(d->getSize());
			ui->setTimeLeft(d->getSecondsLeft());
			ui->setSpeed(d->getRunningAverage());
			ui->setType(d->getType());
			tstring pos = Util::formatBytesW(d->getPos());
			const double percent = (double)d->getPos() * 100.0 / (double)d->getSize();
			tstring elapsed;
			if (d->getStart())
				elapsed = Util::formatSecondsW((CurrentTick - d->getStart()) / 1000); // [!] IRainman refactoring transfer mechanism
				
			tstring l_statusString;
			
			if (d->isSet(Download::FLAG_PARTIAL))
			{
				l_statusString += _T("[P]");
			}
			if (d->m_isSecure)
			{
				if (d->m_isTrusted)
				{
					l_statusString += _T("[S]");
				}
				else
				{
					l_statusString += _T("[U]");
				}
			}
			if (d->isSet(Download::FLAG_TTH_CHECK))
			{
				l_statusString += _T("[T]");
			}
			if (d->isSet(Download::FLAG_ZDOWNLOAD))
			{
				l_statusString += _T("[Z]");
			}
			if (d->isSet(Download::FLAG_CHUNKED))
			{
				l_statusString += _T("[C]");
			}
			if (!l_statusString.empty())
			{
				l_statusString += _T(' ');
			}
			l_statusString += Text::tformat(TSTRING(DOWNLOADED_BYTES), pos.c_str(), percent, elapsed.c_str());
			ui->setStatusString(l_statusString);
			tasks.add(UPDATE_ITEM, ui);
		}
	}
}

void TransferView::on(DownloadManagerListener::Failed, const Download* aDownload, const string& aReason)
{
	UpdateInfo* ui = new UpdateInfo(aDownload->getHintedUser(), true, true); // [!] IRainman fix. https://code.google.com/p/flylinkdc/issues/detail?id=1291
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setPos(0);
	ui->setSize(aDownload->getSize());
	ui->setTarget(aDownload->getPath());
	ui->setType(aDownload->getType());
	// [-] ui->setIP(aDownload->getUserConnection().getRemoteIp()); // !SMT!-IP [-] IRainman opt.
	
	tstring tmpReason = Text::toT(aReason);
	if (aDownload->isSet(Download::FLAG_SLOWUSER))
	{
		tmpReason += _T(": ") + TSTRING(SLOW_USER);
	}
	else if (aDownload->getOverlapped() && !aDownload->isSet(Download::FLAG_OVERLAP))
	{
		tmpReason += _T(": ") + TSTRING(OVERLAPPED_SLOW_SEGMENT);
	}
	
	ui->setStatusString(tmpReason);
	
	SHOW_POPUPF(POPUP_DOWNLOAD_FAILED,
	            TSTRING(FILE) + _T(": ") + Util::getFileName(ui->m_target) + _T('\n') +
	            TSTRING(USER) + _T(": ") + WinUtil::getNicks(ui->hintedUser) + _T('\n') +
	            TSTRING(REASON) + _T(": ") + tmpReason, TSTRING(DOWNLOAD_FAILED) + _T(' '), NIIF_WARNING);
	            
	tasks.add(UPDATE_ITEM, ui);
}

void TransferView::on(DownloadManagerListener::Status, const UserConnection* uc, const string& aReason) noexcept
{
	// dcassert(const_cast<UserConnection*>(uc)->getDownload()); // TODO при окончании закачки это поле уже пустое https://www.box.net/shared/4cknwlue3njzksmciu63
	UpdateInfo* ui = new UpdateInfo(uc->getHintedUser(), true); // [!] IRainman fix.
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setPos(0);
	ui->setStatusString(Text::toT(aReason));
	
	tasks.add(UPDATE_ITEM, ui);
}

void TransferView::on(UploadManagerListener::Starting, const Upload* aUpload)
{
	UpdateInfo* ui = new UpdateInfo(aUpload->getHintedUser(), false); // [!] IRainman fix.
	
	starting(ui, aUpload);
	
	ui->setStatus(ItemInfo::STATUS_RUNNING);
	ui->setActual(aUpload->getStartPos() + aUpload->getActual());
	ui->setSize(aUpload->getType() == Transfer::TYPE_TREE ? aUpload->getSize() : aUpload->getFileSize());
	ui->setRunning(1);
	// [-] ui->setIP(aUpload->getUserConnection().getRemoteIp()); // !SMT!-IP [-] IRainman opt.
	
	if (!aUpload->isSet(Upload::FLAG_RESUMED))
	{
		ui->setStatusString(TSTRING(UPLOAD_STARTING));
	}
	
	tasks.add(UPDATE_ITEM, ui);
}

void TransferView::on(UploadManagerListener::Tick, const UploadList& ul, uint64_t CurrentTick)
{
	if (!MainFrame::isAppMinimized())// [+]IRainman opt
	{
		for (auto j = ul.cbegin(); j != ul.cend(); ++j)
		{
			Upload* u = *j;
			
			if (!u)
				continue;
				
			if (u->getPos() == 0)
				continue;
				
			UpdateInfo* ui = new UpdateInfo(u->getHintedUser(), false); // [!] IRainman fix.
			ui->setActual(u->getStartPos() + u->getActual());
			ui->setPos(u->getStartPos() + u->getPos());
			ui->setTimeLeft(u->getSecondsLeft(true)); // we are interested when whole file is finished and not only one chunk
			ui->setSpeed(u->getRunningAverage());
			
			const tstring pos = Util::formatBytesW(ui->pos);
			const double percent = (double)ui->pos * 100.0 / (double)(u->getType() == Transfer::TYPE_TREE ? u->getSize() : u->getFileSize());
			tstring elapsed;
			if (u->getStart())
				elapsed = Util::formatSecondsW((CurrentTick - u->getStart()) / 1000); // [!] IRainman refactoring transfer mechanism
				
			tstring l_statusString;
			
			if (u->isSet(Upload::FLAG_PARTIAL))
			{
				l_statusString += _T("[P]");
			}
			if (u->m_isSecure)
			{
				if (u->m_isTrusted)
				{
					l_statusString += _T("[S]");
				}
				else
				{
					l_statusString += _T("[U]");
				}
			}
			if (u->isSet(Upload::FLAG_ZUPLOAD))
			{
				l_statusString += _T("[Z]");
			}
			if (u->isSet(Upload::FLAG_CHUNKED))
			{
				l_statusString += _T("[C]");
			}
			if (!l_statusString.empty())
			{
				l_statusString += _T(' ');
			}
			l_statusString += Text::tformat(TSTRING(UPLOADED_BYTES), pos.c_str(), percent, elapsed.c_str());
			
			ui->setStatusString(l_statusString);
			tasks.add(UPDATE_ITEM, ui);
		}
	}
}

void TransferView::onTransferComplete(const Transfer* aTransfer, const bool download, const string& aFileName, const bool isTree)
{
	UpdateInfo* ui = new UpdateInfo(aTransfer->getHintedUser(), download); // [!] IRainman fix.
	
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setPos(0);
	ui->setStatusString(download ? TSTRING(DOWNLOAD_FINISHED_IDLE) : TSTRING(UPLOAD_FINISHED_IDLE));
	ui->setRunning(0);
	
	if (!download && !isTree)
	{
		SHOW_POPUP(POPUP_UPLOAD_FINISHED,
		           TSTRING(FILE) + _T(": ") + Text::toT(aFileName) + _T('\n') +
		           TSTRING(USER) + _T(": ") + WinUtil::getNicks(aTransfer->getHintedUser()), TSTRING(UPLOAD_FINISHED_IDLE));
	}
	
	tasks.add(UPDATE_ITEM, ui);
}

void TransferView::ItemInfo::disconnect()
{
	ConnectionManager::getInstance()->disconnect(hintedUser.user, download);
}

LRESULT TransferView::onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *ii = ctrlTransfers.getItemData(i);
		// [!] IRainman fix.
		const string target = Text::fromT(ii->m_target);
		if (ii->download)
		{
			const auto qi = QueueManager::getInstance()->fileQueue.find(target);
			if (qi)
				startMediaPreview(wID, qi); // [!]
		}
		else
		{
			startMediaPreview(wID, target
#ifdef SSA_VIDEO_PREVIEW_FEATURE
			                  , ii->size
#endif
			                 );
		}
		// [~] IRainman fix.
	}
	
	return 0;
}

void TransferView::CollapseAll()
{
	for (int q = ctrlTransfers.GetItemCount() - 1; q != -1; --q)
	{
		ItemInfo* m = (ItemInfo*)ctrlTransfers.getItemData(q);
		if (m->download)
		{
			if (m->parent)
			{
				ctrlTransfers.deleteItem(m);
			}
			else if (!m->collapsed)
			{
				m->collapsed = true;
				ctrlTransfers.SetItemState(ctrlTransfers.findItem(m), INDEXTOSTATEIMAGEMASK(1), LVIS_STATEIMAGEMASK);
			}
		}
	}
}

void TransferView::ExpandAll()
{
	for (auto i = ctrlTransfers.getParents().cbegin(); i != ctrlTransfers.getParents().cend(); ++i)
	{
		ItemInfo* l = (*i).second.parent;
		if (l->collapsed)
		{
			ctrlTransfers.Expand(l, ctrlTransfers.findItem(l));
		}
	}
}

LRESULT TransferView::onDisconnectAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = ctrlTransfers.getItemData(i);
		
		const vector<ItemInfo*>& children = ctrlTransfers.findChildren(ii->getGroupCond());
		for (auto j = children.cbegin(); j != children.cend(); ++j)
		{
			ItemInfo* ii = *j;
			ii->disconnect();
			
			int h = ctrlTransfers.findItem(ii);
			if (h != -1)
			{
				ctrlTransfers.SetItemText(h, COLUMN_STATUS, CTSTRING(DISCONNECTED));
			}
		}
		
		ctrlTransfers.SetItemText(i, COLUMN_STATUS, CTSTRING(DISCONNECTED));
	}
	return 0;
}

LRESULT TransferView::onSlowDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo *ii = ctrlTransfers.getItemData(i);
		// [!] IRainman fix.
		if (ii->download) // [+]
		{
			string tmp = Text::fromT(ii->m_target);
			
			QueueManager::LockFileQueueShared qm;
			const QueueItem::QIStringMap& queue = qm.getQueue();
			const auto qi = queue.find(&tmp);
			if (qi != queue.cend())
				qi->second->changeAutoDrop(); // [!]
		}
		// [!] IRainman fix.
	}
	
	return 0;
}

void TransferView::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
{
	bool refresh = false;
	if (ctrlTransfers.GetBkColor() != Colors::bgColor)
	{
		ctrlTransfers.SetBkColor(Colors::bgColor);
		ctrlTransfers.SetTextBkColor(Colors::bgColor);
		ctrlTransfers.setFlickerFree(Colors::bgBrush);
		refresh = true;
	}
	if (ctrlTransfers.GetTextColor() != Colors::textColor)
	{
		ctrlTransfers.SetTextColor(Colors::textColor);
		refresh = true;
	}
	if (refresh == true)
	{
		RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
	}
}

void TransferView::on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept // [+] IRainman opt.
{
	if (!MainFrame::isAppMinimized())
	{
		on(QueueManagerListener::StatusUpdatedList(), p_list);
	}
}

void TransferView::on(QueueManagerListener::StatusUpdated, const QueueItemPtr& qi) noexcept
{
	if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
		return;
		
	tasks.add(UPDATE_PARENT_WITH_PARSE, new QueueItemUpdateInfo(qi)); // [!] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1082
}

void TransferView::parseQueueItemUpdateInfo(QueueItemUpdateInfo* ui) // [!] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1082
{
	const auto& qi = ui->m_queueItem;
	
	ui->setTarget(qi->getTarget()); // TODO getTarget - возвращает мусорок... [!] IRainman fix: исправлено добавлением умного указателя.
	ui->setType(Transfer::TYPE_FILE);
	
	if (qi->isRunningL())
	{
		double ratio = 0;
		bool partial = false, trusted = false, untrusted = false, tthcheck = false, zdownload = false, chunked = false;
		const int64_t totalSpeed = qi->getAverageSpeed(); // [!] IRainman opt.
		const int16_t segs = qi->calcTransferFlag(partial, trusted, untrusted, tthcheck, zdownload, chunked, ratio);
		ui->setRunning(segs);
		if (segs > 0)
		{
			ratio = ratio / segs;
			ui->setStatus(ItemInfo::STATUS_RUNNING);
			ui->setSize(qi->getSize());
			ui->setPos(qi->getDownloadedBytes());
			ui->setActual((int64_t)((double)ui->pos * (ratio == 0 ? 1.00 : ratio)));
			ui->setTimeLeft((totalSpeed > 0) ? ((ui->size - ui->pos) / totalSpeed) : 0);
			ui->setSpeed(totalSpeed);
			
			if (qi->getFileBegin() == 0)
			{
				// file is starting
				qi->setFileBegin(GET_TICK());
				ui->setStatusString(TSTRING(DOWNLOAD_STARTING));
				PLAY_SOUND(SOUND_BEGINFILE);
				SHOW_POPUP(POPUP_DOWNLOAD_START, TSTRING(FILE) + _T(": ") + Util::getFileName(ui->m_target), TSTRING(DOWNLOAD_STARTING));
			}
			else
			{
				const uint64_t time = GET_TICK() - qi->getFileBegin();
				if (time > 1000)
				{
					const tstring pos = Util::formatBytesW(ui->pos);
					const double percent = (double)ui->pos * 100.0 / (double)ui->size;
					const tstring elapsed = Util::formatSecondsW(time / 1000);
					tstring flag;
					if (partial)
					{
						flag += _T("[P]");
					}
					if (trusted)
					{
						flag += _T("[S]");
					}
					if (untrusted)
					{
						flag += _T("[U]");
					}
					if (tthcheck)
					{
						flag += _T("[T]");
					}
					if (zdownload)
					{
						flag += _T("[Z]");
					}
					if (chunked)
					{
						flag += _T("[C]");
					}
					if (!flag.empty())
					{
						flag += _T(' ');
					}
					ui->setStatusString(flag + Text::tformat(TSTRING(DOWNLOADED_BYTES), pos.c_str(), percent, elapsed.c_str()));
				}
			}
		}
	}
	else
	{
		qi->setFileBegin(0);
		ui->setSize(qi->getSize());
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setRunning(0);
	}
}

void TransferView::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string&, const Download* download) noexcept
{
	if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
		return;
		
	// update download item
	UpdateInfo* ui = new UpdateInfo(download->getHintedUser(), true); // [!] IRainman fix.
	
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setPos(0);
	ui->setStatusString(TSTRING(DOWNLOAD_FINISHED_IDLE));
	
	tasks.add(UPDATE_ITEM, ui); // [!] IRainman opt.
	
	// update file item
	ui = new UpdateInfo(download->getHintedUser(), true); // [!] IRainman fix.
	
	ui->setTarget(qi->getTarget());
	ui->setPos(0);
	ui->setActual(0);
	ui->setTimeLeft(0);
	ui->setStatusString(TSTRING(DOWNLOAD_FINISHED_IDLE));
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setRunning(0);
	
	SHOW_POPUP(POPUP_DOWNLOAD_FINISHED, TSTRING(FILE) + _T(": ") + Util::getFileName(ui->m_target), TSTRING(DOWNLOAD_FINISHED_IDLE));
	
	tasks.add(UPDATE_PARENT, ui);  // [!] IRainman opt.
}

void TransferView::on(QueueManagerListener::Removed, const QueueItemPtr& qi) noexcept
{
	if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
		return;
		
	UpdateInfo* ui = new UpdateInfo(); // [!] IRainman fix.
	ui->setTarget(qi->getTarget());
	ui->setPos(0);
	ui->setActual(0);
	ui->setTimeLeft(0);
	ui->setStatusString(TSTRING(DISCONNECTED));
	ui->setStatus(ItemInfo::STATUS_WAITING);
	ui->setRunning(0);
	
	tasks.add(UPDATE_PARENT, ui);
	//tasks.add(UPDATE_PARENT_WITH_PARSE_2, new QueueItemUpdateInfo(qi)); // [!] IRainman fix.
}

// [+] Flylink
// !SMT!-UI. todo: move same code to template CopyBaseHandler
LRESULT TransferView::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring l_data;
	int i = -1, columnId = wID - IDC_COPY; // !SMT!-UI: copy several rows
	while ((i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* l_ii = ctrlTransfers.getItemData(i);
		TTHValue l_tth;
		if (l_ii && getTTH(l_ii, l_tth))
		{
			tstring l_sdata;
			if (wID == IDC_COPY_TTH)
				l_sdata = Text::toT(l_tth.toBase32());
			else if (wID == IDC_COPY_LINK)
				l_sdata = Text::toT(Util::getMagnet(l_tth, Text::fromT(Util::getFileName(l_ii->m_target)), l_ii->size));
			else if (wID == IDC_COPY_WMLINK)
				l_sdata = Text::toT(Util::getWebMagnet(l_tth, Text::fromT(Util::getFileName(l_ii->m_target)), l_ii->size));
			else
				l_sdata = l_ii->getText(columnId);
				
			if (l_data.empty())
				l_data = l_sdata;
			else
				l_data += L"\r\n" + l_sdata;
		}
	}
	WinUtil::setClipboard(l_data);
	return 0;
}

// [+]Drakon. Pause selected transfer.
void TransferView::PauseSelectedTransfer(void)
{
	const ItemInfo* ii = ctrlTransfers.getItemData(ctrlTransfers.GetNextItem(-1, LVNI_SELECTED));
	if (ii)
	{
		const string l_target = Text::fromT(ii->m_target);
		// TODO - двойное обращение к менеджеру - склеить вместе
		QueueManager::getInstance()->setAutoPriority(l_target, false);
		QueueManager::getInstance()->setPriority(l_target, QueueItem::PAUSED);
	}
}

bool TransferView::getTTH(const ItemInfo* p_ii, TTHValue& p_tth)
{
	if (p_ii->download)
	{
		return QueueManager::getInstance()->getTTH(Text::fromT(p_ii->m_target), p_tth);
	}
	else
	{
		return ShareManager::getInstance()->findByRealPathName(Text::fromT(p_ii->m_target), &p_tth);
	}
}

// !SMT!-S
LRESULT TransferView::onSetUserLimit(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	MENUINFO menuInfo = {0};
	menuInfo.cbSize = sizeof(MENUINFO);
	menuInfo.fMask = MIM_MENUDATA;
	speedMenu.GetMenuInfo(&menuInfo);
	const int iLimit = menuInfo.dwMenuData;
	const int lim = getSpeedLimitByCtrlId(wID, iLimit);
	// TODO - двойное обращение к менеджеру - склеить вместе
	FavoriteManager::getInstance()->addFavoriteUser(getSelectedUser());
	FavoriteManager::getInstance()->setUploadLimit(getSelectedUser(), lim);
	// close favusers window (it contains incorrect info, too lazy to update)
	UsersFrame::closeWindow();
	return 0;
}

/**
 * @file
 * $Id: TransferView.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
