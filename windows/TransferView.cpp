/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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
#include "../FlyFeatures/CFlyTorrentDialog.h"

#include "UsersFrame.h"

#include "WinUtil.h"
#include "TransferView.h"
#include "MainFrm.h"
#include "QueueFrame.h"
#include "WaitingUsersFrame.h"
#include "BarShader.h"
#include "ResourceLoader.h"
#include "ExMessageBox.h"

#include "libtorrent/hex.hpp"

tstring TransferView::g_sSelectedIP;

HIconWrapper TransferView::g_user_icon(IDR_TUSER);
int TransferView::g_columnIndexes[] =
{
	COLUMN_USER,
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	COLUMN_ANTIVIRUS,
#endif
	COLUMN_P2P_GUARD,
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
#ifdef FLYLINKDC_USE_COLUMN_RATIO
	COLUMN_RATIO,
#endif
	COLUMN_SHARE, //[+]PPA
	COLUMN_SLOTS //[+]PPA
};
int TransferView::g_columnSizes[] =
{
	150, // COLUMN_USER
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	50,  // COLUMN_ANTIVIRUS
#endif
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
#ifdef FLYLINKDC_USE_COLUMN_RATIO
	50,  // COLUMN_RATIO
#endif
	100, // COLUMN_SHARE
	75,  // COLUMN_SLOTS
	40  // COLUMN_P2P_GUARD
};

static ResourceManager::Strings g_columnNames[] = { ResourceManager::USER,
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
                                                    ResourceManager::ANTIVIRUS,
#endif
                                                    ResourceManager::HUB_SEGMENTS,
                                                    ResourceManager::STATUS,
                                                    ResourceManager::TIME_LEFT,
                                                    ResourceManager::SPEED,
                                                    ResourceManager::FILENAME,
                                                    ResourceManager::SIZE,
                                                    ResourceManager::PATH,
                                                    ResourceManager::CIPHER,
                                                    ResourceManager::LOCATION_BARE,
                                                    ResourceManager::IP_BARE
#ifdef FLYLINKDC_USE_COLUMN_RATIO
                                                    , ResourceManager::RATIO
#endif
                                                    , ResourceManager::SHARED, //[+]PPA
                                                    ResourceManager::SLOTS, //[+]PPA
                                                    ResourceManager::P2P_GUARD       // COLUMN_P2P_GUARD
                                                  };

TransferView::TransferView() : CFlyTimerAdapter(m_hWnd), CFlyTaskAdapter(m_hWnd), m_is_need_resort(false)
{
}

TransferView::~TransferView()
{
	m_arrows.Destroy();
	m_torrentImages.Destroy();
	m_speedImagesBW.Destroy();
	m_speedImages.Destroy();
	OperaColors::ClearCache();
}

LRESULT TransferView::onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_force_passive_tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	m_force_passive_tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(m_force_passive_tooltip.IsWindow());
	
	
	m_active_passive_tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	m_active_passive_tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(m_active_passive_tooltip.IsWindow());
	
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	m_avdb_block_tooltip.Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
	m_avdb_block_tooltip.SetDelayTime(TTDT_AUTOPOP, 15000);
	dcassert(m_avdb_block_tooltip.IsWindow());
#endif
	
	ResourceLoader::LoadImageList(IDR_ARROWS, m_arrows, 16, 16);
	ResourceLoader::LoadImageList(IDR_TSPEEDS, m_speedImages, 16, 16);
	ResourceLoader::LoadImageList(IDR_TSPEEDS_BW, m_speedImagesBW, 16, 16);
	ResourceLoader::LoadImageList(IDR_TORRENT_PNG, m_torrentImages, 16, 16);
	
	ctrlTransfers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                     WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_STATICEDGE, IDC_TRANSFERS);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlTransfers);
	
	WinUtil::splitTokens(g_columnIndexes, SETTING(TRANSFER_FRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(g_columnSizes, SETTING(TRANSFER_FRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(g_columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(g_columnNames) == COLUMN_LAST);
	
	for (uint8_t j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_SIZE || j == COLUMN_TIMELEFT || j == COLUMN_SPEED) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlTransfers.InsertColumn(j, TSTRING_I(g_columnNames[j]), fmt, g_columnSizes[j], j);
	}
	
	ctrlTransfers.setColumnOrderArray(COLUMN_LAST, g_columnIndexes);
	ctrlTransfers.setVisible(SETTING(TRANSFER_FRAME_VISIBLE));
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	ctrlTransfers.SetColumnWidth(COLUMN_ANTIVIRUS, 0);
#endif
	
	ctrlTransfers.setSortColumn(SETTING(TRANSFERS_COLUMNS_SORT));
	ctrlTransfers.setAscending(BOOLSETTING(TRANSFERS_COLUMNS_SORT_ASC));
	
	SET_LIST_COLOR(ctrlTransfers);
	ctrlTransfers.setFlickerFree(Colors::g_bgBrush);
	
	ctrlTransfers.SetImageList(m_arrows, LVSIL_SMALL);
	
	m_PassiveModeButton.Create(m_hWnd,
	                           rcDefault,
	                           NULL,
	                           //WS_CHILD| WS_VISIBLE | BS_ICON | BS_AUTOCHECKBOX| BS_PUSHLIKE | BS_FLAT
	                           WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | /*BS_AUTOCHECKBOX | */BS_FLAT
	                           , 0,
	                           IDC_FORCE_PASSIVE_MODE);
	m_PassiveModeButton.SetIcon(WinUtil::g_hFirewallIcon);
	m_PassiveModeButton.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	m_XXXBlockButton.Create(m_hWnd,
	                        rcDefault,
	                        NULL,
	                        //WS_CHILD| WS_VISIBLE | BS_ICON | BS_AUTOCHECKBOX| BS_PUSHLIKE | BS_FLAT
	                        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | /*BS_AUTOCHECKBOX | */BS_FLAT
	                        , 0,
	                        IDC_XXX_BLOCK_MODE);
	m_XXXBlockButton.SetIcon(WinUtil::g_hXXXBlockIcon);
	m_XXXBlockButton.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
	m_AutoPassiveModeButton.Create(m_hWnd,
	                               rcDefault,
	                               NULL,
	                               //WS_CHILD| WS_VISIBLE | BS_ICON | BS_AUTOCHECKBOX| BS_PUSHLIKE | BS_FLAT
	                               WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | /*BS_AUTOCHECKBOX | */BS_FLAT
	                               , 0,
	                               IDC_AUTO_PASSIVE_MODE);
	m_AutoPassiveModeButton.SetIcon(WinUtil::g_hClockIcon);
	m_AutoPassiveModeButton.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
#endif
	
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	m_AVDB_BlockButton.Create(m_hWnd,
	                          rcDefault,
	                          NULL,
	                          //WS_CHILD| WS_VISIBLE | BS_ICON | BS_AUTOCHECKBOX| BS_PUSHLIKE | BS_FLAT
	                          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | /*BS_AUTOCHECKBOX | */BS_FLAT
	                          , 0,
	                          IDC_AVDB_BLOCK_CONNECTIONS);
#ifdef FLYLINKDC_USE_SKULL_TAB
	m_AVDB_BlockButton.SetIcon(*WinUtil::g_HubVirusIcon[2]);
#else
	m_AVDB_BlockButton.SetIcon(*WinUtil::g_HubVirusIcon);
#endif
	m_AVDB_BlockButton.SetButtonStyle(BS_AUTOCHECKBOX, FALSE);
	
#endif
	
	//purgeContainer.SubclassWindow(ctrlPurge.m_hWnd);
	setButtonState();
	
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
	m_copyMenu.CreatePopupMenu();
#ifdef OLD_MENU_HEADER //[~]JhaoDa
	m_copyMenu.InsertSeparatorFirst(TSTRING(COPY));
#endif
	for (size_t i = 0; i < COLUMN_LAST; ++i)
	{
		m_copyMenu.AppendMenu(MF_STRING, IDC_COPY + i, CTSTRING_I(g_columnNames[i]));
	}
	m_copyMenu.AppendMenu(MF_SEPARATOR);
	m_copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(COPY_TTH));
	m_copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
	m_copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL));
	
	//m_copyTorrentMenu.CreatePopupMenu();
	
	usercmdsMenu.CreatePopupMenu();
	
	segmentedMenu.CreatePopupMenu();
	segmentedMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
	appendPreviewItems(segmentedMenu);
#ifdef FLYLINKDC_USE_DROP_SLOW
	segmentedMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
#endif
	segmentedMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)m_copyMenu, CTSTRING(COPY_USER_INFO)); // !SMT!-UI
#ifdef FLYLINKDC_USE_ASK_SLOT
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
	create_timer(1000, 4);
	return 0;
}

void TransferView::setButtonState()
{
	m_PassiveModeButton.SetCheck(BOOLSETTING(FORCE_PASSIVE_INCOMING_CONNECTIONS) ? BST_CHECKED : BST_UNCHECKED);
	m_force_passive_tooltip.AddTool(m_PassiveModeButton, ResourceManager::SETTINGS_FIREWALL_PASSIVE_FORCE);
	
	m_XXXBlockButton.SetCheck(BOOLSETTING(XXX_BLOCK_SHARE) ? BST_CHECKED : BST_UNCHECKED);
	m_force_passive_tooltip.AddTool(m_XXXBlockButton, ResourceManager::SETTINGS_XXX_BLOCK_SHARE);
	
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
	m_AutoPassiveModeButton.SetCheck(BOOLSETTING(AUTO_PASSIVE_INCOMING_CONNECTIONS) ? BST_CHECKED : BST_UNCHECKED);
	m_active_passive_tooltip.AddTool(m_AutoPassiveModeButton, ResourceManager::SETTINGS_FIREWALL_AUTO_PASSIVE);
#endif
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	m_AVDB_BlockButton.SetCheck(BOOLSETTING(AVDB_BLOCK_CONNECTIONS) ? BST_CHECKED : BST_UNCHECKED);
	m_avdb_block_tooltip.AddTool(m_AVDB_BlockButton, ResourceManager::SETTINGS_FIREWALL_BLOCK_DOWNLOAD_FROM_AVDB_USERS);
#endif
	UpdateLayout();
}
void TransferView::prepareClose()
{
	safe_destroy_timer();
	clear_and_destroy_task();
	ctrlTransfers.saveHeaderOrder(SettingsManager::TRANSFER_FRAME_ORDER, SettingsManager::TRANSFER_FRAME_WIDTHS,
	                              SettingsManager::TRANSFER_FRAME_VISIBLE);
	                              
	SET_SETTING(TRANSFERS_COLUMNS_SORT, ctrlTransfers.getSortColumn());
	SET_SETTING(TRANSFERS_COLUMNS_SORT_ASC, ctrlTransfers.isAscending());
	
	SettingsManager::getInstance()->removeListener(this);
	QueueManager::getInstance()->removeListener(this);
	UploadManager::getInstance()->removeListener(this);
	DownloadManager::getInstance()->removeListener(this);
	ConnectionManager::getInstance()->removeListener(this);
	
	//WinUtil::UnlinkStaticMenus(transferMenu); // !SMT!-UI
}

#ifdef FLYLINKDC_USE_ANTIVIRUS_DB

LRESULT TransferView::onAVDBBlockConnections(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_AVDB_BlockButton.GetCheck() == BST_CHECKED)
	{
		SettingsManager::set(SettingsManager::AVDB_BLOCK_CONNECTIONS, 1);
	}
	else
	{
		SettingsManager::set(SettingsManager::AVDB_BLOCK_CONNECTIONS, 0);
	}
	setButtonState();
	return 0;
}
#endif

#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
LRESULT TransferView::onForceAutoPassiveMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_AutoPassiveModeButton.GetCheck() == BST_CHECKED)
	{
		SettingsManager::set(SettingsManager::AUTO_PASSIVE_INCOMING_CONNECTIONS, 1);
	}
	else
	{
		SettingsManager::set(SettingsManager::AUTO_PASSIVE_INCOMING_CONNECTIONS, 0);
	}
	setButtonState();
	return 0;
}
#endif

LRESULT TransferView::onXXXBlockMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_XXXBlockButton.GetCheck() == BST_CHECKED)
	{
		SettingsManager::set(SettingsManager::XXX_BLOCK_SHARE, 1);
	}
	else
	{
		SettingsManager::set(SettingsManager::XXX_BLOCK_SHARE, 0);
	}
	setButtonState();
	return 0;
}
LRESULT TransferView::onForcePassiveMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_PassiveModeButton.GetCheck() == BST_CHECKED)
	{
		SettingsManager::set(SettingsManager::FORCE_PASSIVE_INCOMING_CONNECTIONS, 1);
	}
	else
	{
		SettingsManager::set(SettingsManager::FORCE_PASSIVE_INCOMING_CONNECTIONS, 0);
	}
	setButtonState();
	return 0;
}

void TransferView::UpdateLayout()
{
	RECT rc;
	GetClientRect(&rc);
	if (BOOLSETTING(SHOW_TRANSFERVIEW_TOOLBAR))
	{
		m_PassiveModeButton.MoveWindow(2, 2, 45, 24);
		m_XXXBlockButton.MoveWindow(2, 26, 45, 24);
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		
		m_AVDB_BlockButton.MoveWindow(2, 48, 45, 24);
#endif
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
		m_AutoPassiveModeButton.MoveWindow(2, 68, 45, 24);
#endif
		rc.left += 45;
	}
	ctrlTransfers.MoveWindow(&rc);
}

LRESULT TransferView::onSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	UpdateLayout();
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
			
			const bool main = !ii->parent && ctrlTransfers.findChildren(ii->getGroupCond()).size() > 1;
			
			clearPreviewMenu();
			OMenu transferMenu;
			transferMenu.CreatePopupMenu();
			if (!ii->m_is_torrent)
			{
				clearUserMenu();
				
				// [+] brain-ripper
				// Make menu dynamic, since its content depends of which
				// user selected (for add/remove favorites item)
				
				transferMenu.AppendMenu(MF_STRING, IDC_FORCE, CTSTRING(FORCE_ATTEMPT));
				transferMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSED)); //[+] Drakon
				transferMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
				transferMenu.AppendMenu(MF_SEPARATOR);
				appendPreviewItems(transferMenu);
				transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)usercmdsMenu, CTSTRING(SETTINGS_USER_COMMANDS));
				transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)m_copyMenu, CTSTRING(COPY)); // !SMT!-UI
				transferMenu.AppendMenu(MF_SEPARATOR);
				
				if (!ii->download)
				{
					transferMenu.AppendMenu(MF_STRING, IDC_UPLOAD_QUEUE, CTSTRING(OPEN_UPLOAD_QUEUE));
				}
				else
				{
					transferMenu.AppendMenu(MF_STRING, IDC_QUEUE, CTSTRING(OPEN_DOWNLOAD_QUEUE));
				}
				transferMenu.AppendMenu(MF_SEPARATOR);
#ifdef FLYLINKDC_USE_DROP_SLOW
				transferMenu.AppendMenu(MF_STRING, IDC_MENU_SLOWDISCONNECT, CTSTRING(SETCZDC_DISCONNECTING_ENABLE));
#endif
				transferMenu.AppendMenu(MF_STRING, IDC_ADD_P2P_GUARD, CTSTRING(CLOSE_CONNECTION_AND_ADD_IP_GUARD));
				transferMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(CLOSE_CONNECTION));
				transferMenu.AppendMenu(MF_SEPARATOR);
				if (!main && (i = ctrlTransfers.GetNextItem(i, LVNI_SELECTED)) != -1)
				{
					const ItemInfo* itemI = ctrlTransfers.getItemData(i);
					bCustomMenu = true;
					
#ifdef OLD_MENU_HEADER //[~]JhaoDa
					usercmdsMenu.InsertSeparatorFirst(TSTRING(SETTINGS_USER_COMMANDS));
#endif
					
					// !SMT!-S
					reinitUserMenu(itemI->m_hintedUser.user, itemI->m_hintedUser.hint);
					
					if (getSelectedUser())
					{
						appendUcMenu(usercmdsMenu, UserCommand::CONTEXT_USER, ClientManager::getHubs(getSelectedUser()->getCID(), getSelectedHint()));
					}
					// end !SMT!-S
				}
				
				appendAndActivateUserItems(transferMenu);
				
#ifdef FLYLINKDC_USE_DROP_SLOW
				segmentedMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_UNCHECKED);
				transferMenu.CheckMenuItem(IDC_MENU_SLOWDISCONNECT, MF_BYCOMMAND | MF_UNCHECKED);
#endif
				transferMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, MFS_ENABLED);
				if (ii->download)
				{
#ifdef FLYLINKDC_USE_DROP_SLOW
					transferMenu.EnableMenuItem(IDC_MENU_SLOWDISCONNECT, MFS_ENABLED);
#endif
					transferMenu.EnableMenuItem(IDC_PRIORITY_PAUSED, MFS_ENABLED); //[+] Drakon
					if (!ii->m_target.empty())
					{
						const string l_target = Text::fromT(ii->m_target);
#ifdef FLYLINKDC_USE_DROP_SLOW
						bool slowDisconnect;
						{
							QueueManager::LockFileQueueShared l_fileQueue;
							const auto& queue = l_fileQueue.getQueueL();
							const auto qi = queue.find(l_target);
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
						setupPreviewMenu(l_target);
					}
				}
				else
				{
#ifdef FLYLINKDC_USE_DROP_SLOW
					transferMenu.EnableMenuItem(IDC_MENU_SLOWDISCONNECT, MFS_DISABLED);
#endif
					transferMenu.EnableMenuItem(IDC_PRIORITY_PAUSED, MFS_DISABLED); //[+] Drakon
				}
				
#ifdef IRAINMAN_ENABLE_WHOIS
				if (!ii->m_transfer_ip.empty())
				{
					g_sSelectedIP = ii->m_transfer_ip;  // set tstring for 'openlink function'
					WinUtil::AppendMenuOnWhoisIP(transferMenu, g_sSelectedIP, false);
				}
#endif
				
				activatePreviewItems(transferMenu);
				transferMenu.SetMenuDefaultItem(IDC_PRIVATE_MESSAGE);
			} // Конец менюшке от DC++
			else
			{
				// Менюшка от торрента
				transferMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)m_copyMenu, CTSTRING(COPY)); // TODO m_copyTorrentMenu
				transferMenu.AppendMenu(MF_STRING, IDC_PAUSE_TORRENT, CTSTRING(PAUSE_TORRENT));
				transferMenu.AppendMenu(MF_STRING, IDC_RESUME_TORRENT, CTSTRING(RESUME));
				transferMenu.AppendMenu(MF_STRING, IDC_REMOVE_TORRENT, CTSTRING(REMOVE_TORRENT));
				transferMenu.AppendMenu(MF_STRING, IDC_REMOVE_TORRENT_AND_FILE, CTSTRING(REMOVE_TORRENT_AND_FILE));
			}
			
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
#ifdef IRAINMAN_ENABLE_WHOIS
LRESULT TransferView::onWhoisIP(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!g_sSelectedIP.empty())
	{
		WinUtil::CheckOnWhoisIP(wID, g_sSelectedIP);
	}
	return 0;
}
#endif // IRAINMAN_ENABLE_WHOIS
LRESULT TransferView::onOpenWindows(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	switch (wID)
	{
		case IDC_QUEUE:
			QueueFrame::openWindow();
			break;
		case IDC_UPLOAD_QUEUE:
			WaitingUsersFrame::openWindow();
			break;
		default:
			dcassert(0);
			break;
	}
	return 0;
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
		if (itemI->m_hintedUser.user && itemI->m_hintedUser.user->isOnline())  // [!] IRainman fix.
		{
			StringMap tmp = ucParams;
			ucParams["fileFN"] = Text::fromT(itemI->m_target);
			
			// compatibility with 0.674 and earlier
			ucParams["file"] = ucParams["fileFN"];
			
			ClientManager::userCommand(itemI->m_hintedUser, uc, tmp, true);
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
		
		if (ii->parent == NULL && ii->m_hits != -1)
		{
			const vector<ItemInfo*>& children = ctrlTransfers.findChildren(ii->getGroupCond());
			for (auto j = children.cbegin(); j != children.cend(); ++j)
			{
				ItemInfo* jj = *j;
				
				int h = ctrlTransfers.findItem(jj);
				if (h != -1)
					ctrlTransfers.SetItemText(h, COLUMN_STATUS, CTSTRING(CONNECTING_FORCED));
					
				jj->transferFailed = false;
				ConnectionManager::getInstance()->force(jj->m_hintedUser);
			}
		}
		else
		{
			ii->transferFailed = false;
			ConnectionManager::getInstance()->force(ii->m_hintedUser);
		}
	}
	return 0;
}

void TransferView::ItemInfo::removeAll()
{
	// Не удаляются отдачи через контекстное меню
	if (m_hits <= 1)
	{
		QueueManager::getInstance()->removeSource(m_hintedUser, QueueItem::Source::FLAG_REMOVED);
	}
	else
	{
		QueueManager::getInstance()->removeTarget(Text::fromT(m_target), false);
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
			ItemInfo* l_ii = reinterpret_cast<ItemInfo*>(cd->nmcd.lItemlParam);
			if (!l_ii) //[+]PPA падаем под wine
				return CDRF_DODEFAULT;
			const int colIndex = ctrlTransfers.findColumn(cd->iSubItem);
			cd->clrTextBk = Colors::g_bgColor;
			
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
			Colors::alternationBkColor(cd);
#endif
			if (colIndex == COLUMN_STATUS)
			{
				CRect rc3;
				auto l_stat = l_ii->getText(COLUMN_STATUS);
				int l_shift = 0;
				if (l_ii->m_status == ItemInfo::STATUS_RUNNING || l_ii->m_is_torrent)
				{
					if (!BOOLSETTING(SHOW_PROGRESS_BARS))
					{
						bHandled = FALSE;
						return 0;
					}
					
					// Get the color of this bar
					COLORREF clr = SETTING(PROGRESS_OVERRIDE_COLORS) ?
					               (l_ii->download ? (!l_ii->parent ? SETTING(DOWNLOAD_BAR_COLOR) : SETTING(PROGRESS_SEGMENT_COLOR)) : SETTING(UPLOAD_BAR_COLOR)) :
					               GetSysColor(COLOR_HIGHLIGHT);
					if (!l_ii->download && BOOLSETTING(UP_TRANSFER_COLORS)) //[+]PPA
					{
						const auto l_NumSlot = l_ii->getUser()->getSlots();
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
					const bool useODCstyle = BOOLSETTING(PROGRESSBAR_ODC_STYLE);
					// Real rc, the original one.
					CRect real_rc = rc;
					// We need to offset the current rc to (0, 0) to paint on the New dc
					rc.MoveToXY(0, 0);
					
					CRect rc4;
					CRect rc2 = rc;
					
					// Text rect
					if (BOOLSETTING(STEALTHY_STYLE_ICO) || l_ii->m_is_force_passive)
					{
						rc2.left += 22; // indented for icon and text
						rc2.right -= 2; // and without messing with the border of the cell
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
					rc3 = rc2;
					
					CDC cdc;
					cdc.CreateCompatibleDC(cd->nmcd.hdc);
					HBITMAP hBmp = CreateCompatibleBitmap(cd->nmcd.hdc,  real_rc.Width(),  real_rc.Height());
					
					HBITMAP pOldBmp = cdc.SelectBitmap(hBmp);
					HDC& dc = cdc.m_hDC;
					
					// const COLORREF barPal[3] = { HLS_TRANSFORM(clr, -40, 50), clr, HLS_TRANSFORM(clr, 20, -30) };
					// const COLORREF barPal2[3] = { HLS_TRANSFORM(clr, -15, 0), clr, HLS_TRANSFORM(clr, 15, 0) };
					// The value throws off, usually with about 8-11 (usually negatively f.ex. in src use 190, the change might actually happen already at aprox 180)
					const  HLSCOLOR hls = RGB2HLS(clr);
					LONG top = rc2.top + (rc2.Height() - 15 /*WinUtil::getTextHeight(cd->nmcd.hdc)*/ - 1) / 2 + 1;
					
					const HFONT oldFont = (HFONT)SelectObject(dc, Fonts::g_systemFont); //font -> systemfont [~]Sergey Shushkanov
					SetBkMode(dc, TRANSPARENT);
					COLORREF oldcol = ::SetTextColor(dc, SETTING(PROGRESS_OVERRIDE_COLORS2) ?
					                                 (l_ii->download ? SETTING(PROGRESS_TEXT_COLOR_DOWN) : SETTING(PROGRESS_TEXT_COLOR_UP)) :
					                                 OperaColors::TextFromBackground(clr));
					                                 
					// Draw the background and border of the bar
					if (l_ii->m_size == 0)
					{
						l_ii->m_size = 1;
					}
					
					if (useODCstyle)
					{
						// New style progressbar tweaks the current colors
						const HLSTRIPLE hls_bk = OperaColors::RGB2HLS(cd->clrTextBk);
						
						// Create pen (ie outline border of the cell)
						HPEN penBorder = ::CreatePen(PS_SOLID, 1, OperaColors::blendColors(cd->clrTextBk, clr, (hls_bk.hlstLightness > 0.75) ? 0.6 : 0.4));
						HGDIOBJ pOldPen = ::SelectObject(dc, penBorder);
						
						// Draw the outline (but NOT the background) using pen
						HBRUSH hBrOldBg = CreateSolidBrush(cd->clrTextBk);
						hBrOldBg = (HBRUSH)::SelectObject(dc, hBrOldBg);
						
						::Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
						
						DeleteObject(::SelectObject(dc, hBrOldBg));
						
						// Set the background color, by slightly changing it
						HBRUSH hBrDefBg = CreateSolidBrush(OperaColors::blendColors(cd->clrTextBk, clr, (hls_bk.hlstLightness > 0.75) ? 0.85 : 0.70));
						HGDIOBJ oldBg = ::SelectObject(dc, hBrDefBg);
						
						// Draw the outline AND the background using pen+brush
						::Rectangle(dc, rc.left, rc.top, rc.left + (LONG)(rc.Width() * l_ii->getProgressPosition() + 0.5), rc.bottom);
						// Reset pen
						DeleteObject(::SelectObject(dc, pOldPen));
						// Reset bg (brush)
						DeleteObject(::SelectObject(dc, oldBg));
					}
					if (l_ii->m_actual != l_ii->m_size)
					{
						// Draw the background and border of the bar
						if (!useODCstyle)
						{
							CBarShader statusBar(rc.bottom - rc.top, rc.right - rc.left, SETTING(PROGRESS_BACK_COLOR), l_ii->m_size);
							
							rc.right = rc.left + (int)(rc.Width() * l_ii->m_pos / l_ii->m_size);
							if (!l_ii->download)
							{
								statusBar.FillRange(0, l_ii->m_actual, HLS_TRANSFORM(clr, -20, 30));
								statusBar.FillRange(l_ii->m_actual, l_ii->m_actual, clr);
							}
							else
							{
								statusBar.FillRange(0, l_ii->m_actual, clr);
								if (l_ii->parent)
									statusBar.FillRange(l_ii->m_actual, l_ii->m_actual, SETTING(PROGRESS_SEGMENT_COLOR));
							}
							if (l_ii->m_pos > l_ii->m_actual)
							{
								statusBar.FillRange(l_ii->m_actual, l_ii->m_pos, SETTING(PROGRESS_COMPRESS_COLOR));
							}
							statusBar.Draw(cdc, rc.top, rc.left, SETTING(PROGRESS_3DDEPTH));
						}
						else
						{
							const int right = rc.left + (int)((int64_t)rc.Width() * l_ii->m_actual / l_ii->m_size);
							COLORREF a, b;
							OperaColors::EnlightenFlood(clr, a, b);
							OperaColors::FloodFill(cdc, rc.left + 1, rc.top + 1, right, rc.bottom - 1, a, b, BOOLSETTING(PROGRESSBAR_ODC_BUMPED));
						}
					}
					if (l_ii->m_is_torrent && l_ii->parent == nullptr)
					{
						RECT rc9 = rc2;
						rc9.left -= 21 - l_shift; //[~] Sergey Shushkanov
						rc9.top += 1; //[~] Sergey Shushkanov
						rc9.right = rc9.left + 16;
						rc9.bottom = rc9.top + 16; //[~] Sergey Shushkanov
						const int index = l_ii->m_is_pause ? 2 : (l_ii->m_is_seeding ? 1 : 0);
						m_torrentImages.DrawEx(index, dc, rc9, CLR_DEFAULT, CLR_DEFAULT, ILD_IMAGE);
					}
					else if (BOOLSETTING(STEALTHY_STYLE_ICO) || l_ii->m_is_force_passive)
					{
						// Draw icon - Nasty way to do the filelist icon, but couldn't get other ways to work well,
						// TODO: do separating filelists from other transfers the proper way...
						if (l_ii->m_is_force_passive)
						{
							l_shift += 16;
							DrawIconEx(dc, rc2.left - 20, rc2.top + 2, WinUtil::g_hFirewallIcon, 16, 16, NULL, NULL, DI_NORMAL | DI_COMPAT);
						}
						if (l_ii->isFileList())
						{
							DrawIconEx(dc, rc2.left - 20 + l_shift, rc2.top, g_user_icon, 16, 16, NULL, NULL, DI_NORMAL | DI_COMPAT);
						}
						else if (l_ii->m_status == ItemInfo::STATUS_RUNNING)
						{
							RECT rc9 = rc2;
							rc9.left -= 19 - l_shift; //[~] Sergey Shushkanov
							rc9.top += 3; //[~] Sergey Shushkanov
							rc9.right = rc9.left + 16;
							rc9.bottom = rc9.top + 16; //[~] Sergey Shushkanov
							
							int64_t speedmark;
							if (!BOOLSETTING(THROTTLE_ENABLE))
							{
								const int64_t speedignore = Util::toInt64(SETTING(UPLOAD_SPEED));
								speedmark = BOOLSETTING(STEALTHY_STYLE_ICO_SPEEDIGNORE) ? (l_ii->download ? SETTING(TOP_SPEED) : SETTING(TOP_UP_SPEED)) / 5 : speedignore * 20;
							}
							else
							{
								if (!l_ii->download)
								{
									speedmark = ThrottleManager::getInstance()->getUploadLimitInKBytes() / 5;
								}
								else
								{
									speedmark = ThrottleManager::getInstance()->getDownloadLimitInKBytes() / 5;
								}
							}
							if (!l_ii->m_is_torrent) // TODO - показывать скорость и для торрентов
							{
								CImageList & l_images = HLS_S(hls > 30) || HLS_L(hls) < 70 ? m_speedImages : m_speedImagesBW;
								
								const int64_t speedkb = l_ii->m_speed / 1000;
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
					}
					// Draw the text, the other stuff here was moved upwards due to stealthy style being added
					//const auto& l_stat = ii->getText(COLUMN_STATUS);
					if (!l_stat.empty())
					{
						::ExtTextOut(dc, rc3.left + l_shift, top, ETO_CLIPPED, rc3, l_stat.c_str(), l_stat.length(), NULL);
					}
					
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
				else
				{
					// Отрисуем статусы отличные от RUNNING!
					if (l_ii->m_is_force_passive)
					{
						l_stat +=  WSTRING(FORCE_PASSIVE_MODE);
						l_shift += 16;
						// TODO - drawIcons
					}
					ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc3);
					ctrlTransfers.SetItemFilled(cd, rc3, cd->clrText, cd->clrText);
					if (l_ii->m_is_torrent && l_ii->parent == nullptr)
					{
						RECT rc9 = rc3;
						rc9.top += 1; //[~] Sergey Shushkanov
						rc9.right = rc9.left + 16;
						rc9.bottom = rc9.top + 16; //[~] Sergey Shushkanov
						const int index = l_ii->m_is_pause ? 2 : (l_ii->m_is_seeding ? 1 : 0);
						m_torrentImages.DrawEx(index, cd->nmcd.hdc, rc9, CLR_DEFAULT, CLR_DEFAULT, ILD_IMAGE);
						l_shift += 16;
					}
					if (!l_stat.empty())
					{
						LONG top = rc3.top + (rc3.Height() - 15 /*WinUtil::getTextHeight(cd->nmcd.hdc)*/) / 2;
						if (l_shift && l_ii->m_is_torrent == false)
						{
							if ((top - rc3.top) < 2)
								top = rc3.top + 1;
							//const POINT ps = { rc3.left, top };
							DrawIconEx(cd->nmcd.hdc, rc3.left, top + 2, WinUtil::g_hFirewallIcon, 16, 16, NULL, NULL, DI_NORMAL | DI_COMPAT);
							//g_userStateImage.Draw(cd->nmcd.hdc, 4 , ps);
						}
						::ExtTextOut(cd->nmcd.hdc, rc3.left + l_shift, top, ETO_CLIPPED, rc3, l_stat.c_str(), l_stat.length(), NULL);
					}
					return CDRF_SKIPDEFAULT;
				}
			}
			else if (colIndex == COLUMN_P2P_GUARD) // TODO
			{
				ItemInfo* jj = (ItemInfo*)cd->nmcd.lItemlParam;
				if (!jj) //[+]PPA падаем под wine
					return CDRF_DODEFAULT;
				ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				ctrlTransfers.SetItemFilled(cd, rc, cd->clrText, cd->clrText);
				const tstring& l_value = jj->getText(colIndex);
				if (!l_value.empty())
				{
					// LONG top = rc.top + (rc.Height() - 15) / 2;
					//if ((top - rc.top) < 2)
					//  top = rc.top + 1;
					//const POINT ps = { rc.left, top };
					// TODO g_userStateImage.Draw(cd->nmcd.hdc, 3, ps);
					::ExtTextOut(cd->nmcd.hdc, rc.left + 6 + 17, rc.top + 2, ETO_CLIPPED, rc, l_value.c_str(), l_value.length(), NULL);
				}
				return CDRF_SKIPDEFAULT;
			}
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			else if (colIndex == COLUMN_ANTIVIRUS)
			{
				ItemInfo* jj = (ItemInfo*)cd->nmcd.lItemlParam;
				if (!jj) //[+]PPA падаем под wine
					return CDRF_DODEFAULT;
				ctrlTransfers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				ctrlTransfers.SetItemFilled(cd, rc, cd->clrText, cd->clrText);
				const tstring& l_value = jj->getText(colIndex);
				if (!l_value.empty())
				{
					LONG top = rc.top + (rc.Height() - 15) / 2;
					if ((top - rc.top) < 2)
						top = rc.top + 1;
					const POINT ps = { rc.left, top };
					g_userStateImage.Draw(cd->nmcd.hdc, 3, ps);
					::ExtTextOut(cd->nmcd.hdc, rc.left + 6 + 17, rc.top + 2, ETO_CLIPPED, rc, l_value.c_str(), l_value.length(), NULL);
				}
				return CDRF_SKIPDEFAULT;
			}
#endif
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
					SetTextColor(cd->nmcd.hdc, Colors::g_textColor);
				}
				CRect rc2 = rc;
				rc2.left += 2;
				HGDIOBJ oldpen = ::SelectObject(cd->nmcd.hdc, CreatePen(PS_SOLID, 0, color));
				HGDIOBJ oldbr = ::SelectObject(cd->nmcd.hdc, CreateSolidBrush(color));
				Rectangle(cd->nmcd.hdc, rc.left, rc.top, rc.right, rc.bottom);
				
				DeleteObject(::SelectObject(cd->nmcd.hdc, oldpen));
				DeleteObject(::SelectObject(cd->nmcd.hdc, oldbr));
				
				LONG top = rc2.top + (rc2.Height() - 15) / 2;
				if ((top - rc2.top) < 2)
					top = rc2.top + 1;
				// TODO fix copy-paste
				if (ii->m_location.isNew() && !ii->m_transfer_ip.empty())
				{
					ii->m_location = Util::getIpCountry(Text::fromT(ii->m_transfer_ip));
				}
				if (ii->m_location.isKnown())
				{
					int l_step = 0;
#ifdef FLYLINKDC_USE_GEO_IP
					if (BOOLSETTING(ENABLE_COUNTRYFLAG))
					{
						const POINT ps = { rc2.left, top };
						g_flagImage.DrawCountry(cd->nmcd.hdc, ii->m_location, ps);
						l_step += 25;
					}
#endif
					const POINT p = { rc2.left + l_step, top };
					if (ii->m_location.getFlagIndex()  > 0)
					{
						g_flagImage.DrawLocation(cd->nmcd.hdc, ii->m_location, p);
						l_step += 25;
					}
					top = rc2.top + (rc2.Height() - 15 /*WinUtil::getTextHeight(cd->nmcd.hdc)*/ - 1) / 2;
					const auto& l_desc = ii->m_location.getDescription();
					if (!l_desc.empty())
					{
						::ExtTextOut(cd->nmcd.hdc, rc2.left + l_step + 5, top + 1, ETO_CLIPPED, rc2, l_desc.c_str(), l_desc.length(), nullptr);
					}
				}
				return CDRF_SKIPDEFAULT;
			}
			else if (colIndex != COLUMN_USER &&
			         colIndex != COLUMN_HUB &&
			         colIndex != COLUMN_STATUS &&
			         colIndex != COLUMN_LOCATION &&
			         l_ii->m_status != ItemInfo::STATUS_RUNNING)
			{
				cd->clrText = OperaColors::blendColors(Colors::g_bgColor, Colors::g_textColor, 0.4);
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
		if (!i->m_is_torrent)
		{
			if (i->parent != nullptr || children.size() <= 1)
			{
				switch (SETTING(TRANSFERLIST_DBLCLICK))
				{
					case 0:
						i->pm(i->m_hintedUser.hint); // [!] IRainman fix.
						break;
					case 1:
						i->getList(); // [!] IRainman fix.
						break;
					case 2:
						i->matchQueue(); // [!] IRainman fix.
					case 3:
						i->grantSlotPeriod(i->m_hintedUser.hint, 600); // [!] IRainman fix.
						break;
					case 4:
						i->addFav();
						break;
					case 5: // !SMT!-UI
					
						i->m_statusString = TSTRING(CONNECTING_FORCED);
						ctrlTransfers.updateItem(i);
						bool l_is_active_client;
						ClientManager::getInstance()->connect(i->m_hintedUser, Util::toString(Util::rand()), false, l_is_active_client); // [!] IRainman fix.
						break;
					case 6:
						i->browseList(); // [!] IRainman fix.
						break;
				}
			}
		}
	}
	return 0;
}

int TransferView::ItemInfo::compareItems(const ItemInfo* a, const ItemInfo* b, uint8_t col)
{
	if (a->m_status == b->m_status)
	{
		if (a->download != b->download)
		{
			return a->download ? -1 : 1;
		}
	}
	else
	{
		return (a->m_status == ItemInfo::STATUS_RUNNING) ? -1 : 1;
	}
	
	switch (col)
	{
		case COLUMN_USER:
			return a->m_hits == b->m_hits ? a->getText(COLUMN_USER).compare(b->getText(COLUMN_USER)) : compare(a->m_hits, b->m_hits); // [!] IRainman opt.
		case COLUMN_HUB:
			return a->running == b->running ? a->getText(COLUMN_HUB).compare(b->getText(COLUMN_HUB)) : compare(a->running, b->running); // [!] IRainman opt.
		case COLUMN_STATUS:
			return compare(a->getProgressPosition(), b->getProgressPosition());
		case COLUMN_TIMELEFT:
			return compare(a->m_timeLeft, b->m_timeLeft);
		case COLUMN_SPEED:
			return compare(a->m_speed, b->m_speed);
		case COLUMN_SIZE:
			return compare(a->m_size, b->m_size);
#ifdef FLYLINKDC_USE_COLUMN_RATIO
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
	ItemInfo* ii = nullptr;
	int j = 0;
	const auto l_count = ctrlTransfers.GetItemCount();
	for (; j < l_count; ++j)
	{
		ii = ctrlTransfers.getItemData(j);
		if (ii)
		{
			if (ui == *ii)
			{
				pos = j;
				return ii;
			}
			else if (ui.download == ii->download && !ii->parent && ii->m_is_torrent == ui.m_is_torrent)
			{
				if (ii->m_is_torrent == false)
				{
					dcassert(ii->m_sha1.is_all_zeros());
					dcassert(ui.m_sha1.is_all_zeros());
				}
				const auto& children = ctrlTransfers.findChildren(ii->getGroupCond()); // TODO - ссылка?
				for (auto k = children.cbegin(); k != children.cend(); ++k)
				{
					ItemInfo* jj = *k;
					if (ui == *jj)       // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=139847  https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=62292
					{
						return jj;
					}
				}
			}
		}
		else
		{
			dcassert(ii);
		}
	}
	return nullptr;
}


void TransferView::doTimerTask()
{
	m_is_need_resort = true;
	CFlyTaskAdapter::doTimerTask();
}

void TransferView::onSpeakerAddItem(const UpdateInfo& ui)
{
	int pos = -1;
	ItemInfo* l_find_ii = findItem(ui, pos);
	if (l_find_ii)
	{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
		LogManager::message("SKIP Dup TRANSFER_ADD_ITEM ErrorStatus: " + Text::fromT(ui.errorStatusString) + " Status = " + Text::fromT(ui.statusString) + " ui.token = " + ui.m_token);
#endif
		return;
	}
	if (ui.m_is_torrent == false)
	{
		dcassert(!ui.m_token.empty());
		if (!ConnectionManager::g_tokens_manager.isToken(ui.m_token))
		{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
			LogManager::message("SKIP missing token TRANSFER_ADD_ITEM ErrorStatus: ui.token = " + ui.m_token);
#endif
			return;
		}
	}
	ItemInfo* ii = new ItemInfo(ui.m_hintedUser, ui.download, ui.m_is_torrent);
#ifdef FLYLINKDC_USE_TORRENT
	ii->m_is_seeding = ui.m_is_seeding;
	ii->m_sha1 = ui.m_sha1;
#endif
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
	if (ui.m_token.empty())
	{
		LogManager::message("TRANSFER_ADD_ITEM ui.token.empty()");
	}
#endif
	ii->update(ui);
	if (ii->m_is_torrent == false)
	{
		if (ii->download)
		{
			ctrlTransfers.insertGroupedItem(ii, false, false, true);
		}
		else
		{
			ctrlTransfers.insertItem(ii, IMAGE_UPLOAD);
		}
	}
	else
	{
		ctrlTransfers.insertItem(ii, IMAGE_DOWNLOAD);
	}
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
	LogManager::message("TRANSFER_ADD_ITEM ErrorStatus: " + Text::fromT(ui.errorStatusString) + " Status = " + Text::fromT(ui.statusString) + " ui.token = " + ui.m_token);
#endif
}

LRESULT TransferView::onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	TaskQueue::List t;
	m_tasks.get(t);
	
	if (t.empty()) // [+] IRainman opt
		return 0;
		
	CFlyBusyBool l_busy(m_spoken);
	CLockRedraw<> l_lock_draw(ctrlTransfers);
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		dcassert(m_tasks.is_destroy_task() == false);
		switch (i->first)
		{
			case TRANSFER_ADD_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
				onSpeakerAddItem(ui);
			}
			break;
			case TRANSFER_REMOVE_TOKEN_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
				unsigned l_count_remove = 0;
#endif
				for (int j = 0; j < ctrlTransfers.GetItemCount(); ++j)
				{
					ItemInfo* ii = ctrlTransfers.getItemData(j);
					bool l_is_find = false;
					if (ui.m_is_torrent == true && ii->m_is_torrent == true)
						l_is_find = ii->m_sha1 == ui.m_sha1;
					else
					{
						if (ui.m_is_torrent == false && ii->m_is_torrent == false)
							l_is_find = ii->m_token == ui.m_token;
					}
					if (l_is_find)
					{
						ctrlTransfers.removeGroupedItem(ii);
						j = 0;
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
						LogManager::message("TRANSFER_REMOVE_TOKEN_ITEM ui.token = " + ui.m_token);
						l_count_remove++;
#endif
					}
				}
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
				if (!l_count_remove)
				{
					LogManager::message("[!] Not found TRANSFER_REMOVE_TOKEN_ITEM ui.token = " + ui.m_token);
				}
#endif
			}
			break;
			case TRANSFER_REMOVE_DOWNLOAD_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
				dcassert(!ui.m_target.empty());
				dcassert(ui.m_is_torrent == false);
				if (!ui.m_target.empty() && ui.m_is_torrent == false)
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					unsigned l_count_remove = 0;
#endif
					for (int j = 0; j < ctrlTransfers.GetItemCount(); ++j)
					{
						ItemInfo* ii = ctrlTransfers.getItemData(j);
						if (ui.download &&
						        ii->m_target == ui.m_target
						   )
						{
							ctrlTransfers.removeGroupedItem(ii);
							j = 0;
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
							LogManager::message("TRANSFER_REMOVE_DOWNLOAD_ITEM ui.m_target = " + Text::fromT(ui.m_target) + " ui.token = " + ui.m_token);
							l_count_remove++;
#endif
						}
					}
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					if (!l_count_remove)
					{
						LogManager::message("[!] Not found TRANSFER_REMOVE_DOWNLOAD_ITEM ii->m_target = " + Text::fromT(ui.m_target) + " ui.token = " + ui.m_token);
					}
#endif
				}
			}
			break;
			case TRANSFER_REMOVE_ITEM:
			{
				const auto &ui = static_cast<UpdateInfo&>(*i->second);
				int pos = -1;
				ItemInfo* ii = findItem(ui, pos);
				if (ii)
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					LogManager::message("TRANSFER_REMOVE_ITEM User = " + ii->getUser()->getLastNick() + " ErrorStatus: "
					                    + Text::fromT(ui.errorStatusString) + " Status = " + Text::fromT(ui.statusString) + " ui.token = " + ui.m_token);
#endif
					if (ui.download)
					{
						ctrlTransfers.removeGroupedItem(ii);
					}
					else
					{
						dcassert(pos != -1);
						if (ctrlTransfers.DeleteItem(pos) == FALSE)
						{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
							LogManager::message("Error delete pos = " + Util::toString(pos) +
							                    "TRANSFER_REMOVE_ITEM User = " + ii->getUser()->getLastNick() + " ErrorStatus: " +
							                    Text::fromT(ui.errorStatusString) + " Status = " + Text::fromT(ui.statusString) + " ui.token = " + ui.m_token);
#endif
						}
						delete ii;
					}
				}
				else
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					LogManager::message("[!] TRANSFER_REMOVE_ITEM not found ErrorStatus: " + Text::fromT(ui.errorStatusString) +
					                    " Status = " + Text::fromT(ui.statusString) + " ui.token = " + ui.m_token);
#endif
					//dcassert(0);
				}
			}
			break;
			case TRANSFER_UPDATE_ITEM:
			{
				auto &ui = static_cast<UpdateInfo&>(*i->second);
				
				int pos = -1;
				ItemInfo* ii = findItem(ui, pos);
				if (ii)
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					if (ii->getUser())
					{
						LogManager::message("TRANSFER_UPDATE_ITEM User = " + ii->getUser()->getLastNick() + " ErrorStatus: " +
						                    Text::fromT(ui.errorStatusString) + " Status = " + Text::fromT(ui.statusString) + " ui.token = " + ui.m_token);
					}
					else
					{
						LogManager::message("TRANSFER_UPDATE_ITEM ErrorStatus: " +
						                    Text::fromT(ui.errorStatusString) + " Status = " + Text::fromT(ui.statusString) + " ui.token = " + ui.m_token);
					}
#endif
					if (ui.download && ui.m_is_torrent == false)
					{
						const ItemInfo* parent = ii->parent ? ii->parent : ii;
						{
							if (!ui.m_token.empty())
							{
								if (!ConnectionManager::g_tokens_manager.isToken(ui.m_token))
								{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
									LogManager::message("SKIP missing token TRANSFER_UPDATE_ITEM  ui.token = " + ui.m_token);
#endif
									//UpdateInfo* l_remove_ui = new UpdateInfo(HintedUser(), true); // Костыль
									//l_remove_ui->setToken(ui.m_token);
									//m_tasks.add(TRANSFER_REMOVE_TOKEN_ITEM, l_remove_ui);
									break;
								}
							}
						}
						if (ui.type == Transfer::TYPE_FILE || ui.type == Transfer::TYPE_TREE)
						{
							/* parent item must be updated with correct info about whole file */
							if (ui.status == ItemInfo::STATUS_RUNNING && parent->m_status == ItemInfo::STATUS_RUNNING && parent->m_hits == -1)
							{
								// - пропадает из прогресса - не врубать
								// TODO - ui.updateMask &= ~UpdateInfo::MASK_TOKEN;
								ui.updateMask &= ~UpdateInfo::MASK_SIZE;
								if (ui.m_is_torrent == false)
								{
									ui.updateMask &= ~UpdateInfo::MASK_POS;
									ui.updateMask &= ~UpdateInfo::MASK_ACTUAL;
									ui.updateMask &= ~UpdateInfo::MASK_STATUS_STRING;
									ui.updateMask &= ~UpdateInfo::MASK_ERROR_STATUS_STRING;
								}
								ui.updateMask &= ~UpdateInfo::MASK_TIMELEFT;
							}
							else
							{
								//dcassert(0);
							}
						}
						
						/* if target has changed, regroup the item */
						bool changeParent = false;
						changeParent = ui.m_is_torrent == false && ii->m_is_torrent == false && (ui.updateMask & UpdateInfo::MASK_FILE) && ui.m_target != ii->m_target;
						if (changeParent)
						{
							ctrlTransfers.removeGroupedItem(ii, false);
						}
						
						ii->update(ui);
						
						if (changeParent)
						{
							ctrlTransfers.insertGroupedItem(ii, false, false, true);
							parent = ii->parent ? ii->parent : ii;
						}
						else if (ii == parent || !parent->collapsed)
						{
							const auto l_pos = ctrlTransfers.findItem(ii);
							//dcassert(l_pos == pos);
							updateItem(l_pos, ui.updateMask);
						}
						break;
					}
					ii->update(ui);
					dcassert(pos != -1);
					updateItem(pos, ui.updateMask);
				}
				else
				{
#ifdef FLYLINKDC_USE_DEBUG_TRANSFERS
					if (ui.m_hintedUser.user)
					{
						LogManager::message("[!] TRANSFER_UPDATE_ITEM not found error User = " + ui.m_hintedUser.user->getLastNick() + " status: " +
						                    Text::fromT(ui.errorStatusString) + " Status = " + Text::fromT(ui.statusString) +
						                    " info = " + ui.m_hintedUser.user->getLastNick() + "is download = " + Util::toString(ui.download) + " ui.token = " + ui.m_token);
					}
					else
					{
						LogManager::message("[!] TRANSFER_UPDATE_ITEM not found error status: " +
						                    Text::fromT(ui.errorStatusString) + " Status = " + Text::fromT(ui.statusString) +
						                    "is download = " + Util::toString(ui.download) + " ui.token = " + ui.m_token);
					}
#endif
					// dcassert(0);
					if (ui.m_is_torrent == true && !ui.m_sha1.is_all_zeros())
					{
						onSpeakerAddItem(ui);
						// https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1674
					}
				}
			}
			break;
			case TRANSFER_UPDATE_PARENT:
			{
				const auto& ui = static_cast<UpdateInfo&>(*i->second);
				const ItemInfoList::ParentPair* pp = ctrlTransfers.findParentPair(ui.m_target);
				
				if (!pp)
					break;
					
				if (ui.m_hintedUser.user || ui.m_is_torrent)
				{
					int pos = -1;
					ItemInfo* ii = findItem(ui, pos);
					if (ii)
					{
						ii->m_status = ui.status;
						ii->m_statusString = ui.statusString;
						ii->m_errorStatusString = ui.errorStatusString;
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
	if (m_is_need_resort && !MainFrame::isAppMinimized())
	{
		m_is_need_resort = false;
		ctrlTransfers.resort();
	}
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
		{
			WinUtil::searchHash(l_tth);
		}
	}
	
	return 0;
}

void TransferView::ItemInfo::update(const UpdateInfo& ui)
{
	if (ui.type != Transfer::TYPE_LAST)
		m_type = ui.type;
		
	m_is_torrent = ui.m_is_torrent;
	m_is_seeding = ui.m_is_seeding;
	m_is_pause = ui.m_is_pause;
	m_sha1 = ui.m_sha1;
	
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
	if (ui.updateMask & UpdateInfo::MASK_FORCE_PASSIVE)
	{
		m_is_force_passive = ui.m_is_force_passive;
	}
#endif
	if (ui.updateMask & UpdateInfo::MASK_STATUS)
	{
		m_status = ui.status;
	}
	if (ui.updateMask & UpdateInfo::MASK_ERROR_STATUS_STRING)
	{
		m_errorStatusString = ui.errorStatusString;
	}
	if (ui.updateMask & UpdateInfo::MASK_TOKEN)
	{
		m_token = ui.m_token;
	}
	if (ui.updateMask & UpdateInfo::MASK_STATUS_STRING)
	{
		// No slots etc from transfermanager better than disconnected from connectionmanager
		if (!transferFailed)
			m_statusString = ui.statusString;
		transferFailed = ui.transferFailed;
	}
	if (ui.updateMask & UpdateInfo::MASK_SIZE)
	{
		m_size = ui.size;
	}
	if (ui.updateMask & UpdateInfo::MASK_POS)
	{
		m_pos = ui.pos;
	}
	if (ui.updateMask & UpdateInfo::MASK_ACTUAL)
	{
		m_actual = ui.actual;
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
		m_is_file_list = ui.m_is_file_list;
	}
	if (ui.updateMask & UpdateInfo::MASK_TIMELEFT)
	{
		m_timeLeft = ui.timeLeft;
	}
	if (ui.updateMask & UpdateInfo::MASK_IP)
	{
		dcassert(!ui.m_ip.empty());
		if (m_transfer_ip.empty()) // [+] IRainman fix: if IP is set already, not try to set twice. IP can not change during a single connection.
		{
			m_transfer_ip = ui.m_ip;
			if (!m_transfer_ip.empty())
			{
				m_p2p_guard_text = Text::toT(CFlylinkDBManager::getInstance()->is_p2p_guard(Socket::convertIP4(Text::fromT(m_transfer_ip))));
			}
#ifdef FLYLINKDC_USE_COLUMN_RATIO
			m_ratio_as_text = ui.m_hintedUser.user->getUDratio();// [+] brain-ripper
#endif
#ifdef FLYLINKDC_USE_DNS
			columns[COLUMN_DNS] = ui.dns; // !SMT!-IP
#endif
		}
	}
	if (ui.updateMask & UpdateInfo::MASK_CIPHER)
	{
		m_cipher = ui.m_cipher;
	}
	if (ui.updateMask & UpdateInfo::MASK_SEGMENT)
	{
		running = ui.running;
	}
	if (ui.updateMask & UpdateInfo::MASK_USER)
	{
		if (!m_hintedUser.isEQU(ui.m_hintedUser))
		{
			m_hintedUser = ui.m_hintedUser;
			update_nicks();
		}
	}
}

void TransferView::updateItem(int ii, uint32_t updateMask)
{
	if (updateMask & UpdateInfo::MASK_STATUS ||
	        updateMask & UpdateInfo::MASK_STATUS_STRING ||
	        updateMask & UpdateInfo::MASK_ERROR_STATUS_STRING ||
	        updateMask & UpdateInfo::MASK_TOKEN ||
	        updateMask & UpdateInfo::MASK_POS ||
	        updateMask & UpdateInfo::MASK_ACTUAL
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
	        ||  updateMask & UpdateInfo::MASK_FORCE_PASSIVE
#endif
	   )
	{
		ctrlTransfers.updateItem(ii, COLUMN_STATUS);
	}
#ifdef FLYLINKDC_USE_COLUMN_RATIO
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
		//ctrlTransfers.updateItem(ii, COLUMN_LOCATION);
	}
	if (updateMask & UpdateInfo::MASK_SEGMENT)
	{
		ctrlTransfers.updateItem(ii, COLUMN_HUB);
	}
	if (updateMask & UpdateInfo::MASK_CIPHER)
	{
		ctrlTransfers.updateItem(ii, COLUMN_CIPHER);
	}
	if (updateMask & UpdateInfo::MASK_USER)
	{
		ctrlTransfers.updateItem(ii, COLUMN_USER);
	}
}
// TODO - паблик
/*
static HintedUser getHintedUser(const UserPtr& p_user)
{
    string l_hub_name;
    if (p_user->getHubID())
    {
        l_hub_name = CFlylinkDBManager::getInstance()->get_hub_name(p_user->getHubID());
    }
    return HintedUser(p_user, l_hub_name);
}

*/

TransferView::UpdateInfo* TransferView::createUpdateInfoForAddedEvent(const HintedUser& p_hinted_user, bool p_is_download, const string& p_token)
{
	UpdateInfo* ui = new UpdateInfo(p_hinted_user, p_is_download);
	dcassert(!p_token.empty());
	ui->setToken(p_token);
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
	ui->setForcePassive(aCqi->m_is_force_passive);
#endif
	if (ui->download)
	{
		string l_target;
		int64_t l_size = 0;
		int l_flags = 0;
		if (QueueManager::getQueueInfo(p_hinted_user.user, l_target, l_size, l_flags)) // deadlock
		{
			Transfer::Type l_type = Transfer::TYPE_FILE;
			if (l_flags & QueueItem::FLAG_USER_LIST)
				l_type = Transfer::TYPE_FULL_LIST;
			else if (l_flags & QueueItem::FLAG_DCLST_LIST)
				l_type = Transfer::TYPE_FULL_LIST;
			else if (l_flags & QueueItem::FLAG_PARTIAL_LIST)
				l_type = Transfer::TYPE_PARTIAL_LIST;
				
			ui->setType(l_type);
			ui->setTarget(l_target);
			ui->setSize(l_size);
		}
		else
		{
#ifdef _DEBUG
			LogManager::message("Skip TransferView::createUpdateInfoForAddedEvent - p_hinted_user.user not found: " + p_hinted_user.user->getLastNick());
#endif
			delete ui;
			return nullptr;
		}
	}
	
	ui->setStatus(ItemInfo::STATUS_WAITING);
	const string l_status = STRING(CONNECTING);
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
	aCqi->addAutoPassiveStatus(l_status);
#endif
	ui->setStatusString(Text::toT(l_status));
	return ui;
}

void TransferView::on(ConnectionManagerListener::Added, const HintedUser& p_hinted_user, bool p_is_download, const string& p_token) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!p_token.empty());
	m_tasks.add_safe(TRANSFER_ADD_ITEM, createUpdateInfoForAddedEvent(p_hinted_user, p_is_download, p_token));
}

void TransferView::on(ConnectionManagerListener::ConnectionStatusChanged, const HintedUser& p_hinted_user, bool p_is_download, const string& p_token) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!p_token.empty());
	m_tasks.add_safe(TRANSFER_UPDATE_ITEM, createUpdateInfoForAddedEvent(p_hinted_user, p_is_download, p_token));
}

void TransferView::on(ConnectionManagerListener::UserUpdated, const HintedUser& p_hinted_user, bool p_is_download, const string& p_token) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!p_token.empty());
	m_tasks.add_safe(TRANSFER_UPDATE_ITEM, createUpdateInfoForAddedEvent(p_hinted_user, p_is_download, p_token));
}
void TransferView::on(ConnectionManagerListener::Removed, const HintedUser& p_hinted_user, bool p_is_download, const string& p_token) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcassert(!p_token.empty());
	auto l_item = new UpdateInfo(p_hinted_user, p_is_download);
	l_item->setToken(p_token);
	m_tasks.add(TRANSFER_REMOVE_ITEM, l_item);
}

void TransferView::on(ConnectionManagerListener::FailedDownload, const HintedUser& p_hinted_user, const string& p_reason, const string& p_token) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
#ifdef _DEBUG
		LogManager::message("ConnectionManagerListener::FailedDownload user = " + p_hinted_user.user->getLastNick() + " Reason = " + p_reason);
#endif
		UpdateInfo* ui = new UpdateInfo(p_hinted_user, true);
		ui->setToken(p_token);
#ifdef FLYLINKDC_USE_IPFILTER
		if (ui->m_hintedUser.user->isAnySet(User::PG_IPTRUST_BLOCK | User::PG_IPGUARD_BLOCK | User::PG_P2PGUARD_BLOCK
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		                                    | User::PG_AVDB_BLOCK
#endif
		                                   ))
		{
			string l_status = STRING(CONNECTION_BLOCKED);
			if (ui->m_hintedUser.user->isSet(User::PG_IPTRUST_BLOCK))
			{
				l_status += " [IPTrust.ini]";
			}
			if (ui->m_hintedUser.user->isSet(User::PG_IPGUARD_BLOCK))
			{
				l_status += " [IPGuard.ini]";
			}
			if (ui->m_hintedUser.user->isSet(User::PG_P2PGUARD_BLOCK))
			{
				l_status += " [P2PGuard.ini]";
			}
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			if (ui->m_hintedUser.user->isSet(User::PG_AVDB_BLOCK))
			{
				l_status += " [Antivirus DB]";
			}
#endif
			ui->setErrorStatusString(Text::toT(l_status + " [" + p_reason + "]"));
		}
		else
#endif
		{
			const string l_status = p_reason;
#ifdef FLYLINKDC_USE_AUTOMATIC_PASSIVE_CONNECTION
			aCqi->addAutoPassiveStatus(l_status);
#endif
			ui->setErrorStatusString(Text::toT(l_status));
		}
		
		ui->setStatus(ItemInfo::STATUS_WAITING);
		m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
	}
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
void TransferView::ItemInfo::update_nicks()
{
	if (m_hintedUser.user)
	{
		m_nicks = WinUtil::getNicks(m_hintedUser);
		m_hubs = WinUtil::getHubNames(m_hintedUser).first;
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		m_antivirus_text = Text::toT(Util::toString(ClientManager::getAntivirusNicks(m_hintedUser.user->getCID())));
#endif
	}
}
const tstring TransferView::ItemInfo::getText(uint8_t col) const
{
	if (ClientManager::isBeforeShutdown())
		return Util::emptyStringT;
	switch (col)
	{
		case COLUMN_USER:
			if (m_is_torrent)
			{
				return m_hits == -1 ? Util::emptyStringT : (Util::toStringW(m_hits) + _T(' ') + TSTRING(FILES));
			}
			else
			{
				return m_hits == -1 ? m_nicks : (Util::toStringW(m_hits) + _T(' ') + TSTRING(USERS));
			}
		case COLUMN_HUB:
			if (m_is_torrent)
			{
				// TODO - показать тут трэккеры
			}
			else
			{
				return m_hits == -1 ? m_hubs : (Util::toStringW(running) + _T(' ') + TSTRING(NUMBER_OF_SEGMENTS));
			}
		case COLUMN_STATUS:
		{
#ifndef _DEBUG
			if (!m_errorStatusString.empty())
			{
				return m_statusString + _T(" [") + m_errorStatusString  + _T("]"); // bad_alloc https://drdump.com/UploadedReport.aspx?DumpID=11192168
			}
			else
			{
				return m_statusString;
			}
#else
			if (m_is_torrent)
			{
				if (!m_errorStatusString.empty())
					return m_statusString + _T(" [") + m_errorStatusString + _T("]");
				else
					return m_statusString;
			}
			else
			{
				return m_statusString + _T(" [") + m_errorStatusString + _T("] Token:") + Text::toT(m_token);
			}
#endif
		}
		case COLUMN_TIMELEFT:
			//dcassert(m_timeLeft >= 0);
			return (m_status == STATUS_RUNNING && m_timeLeft > 0) ? Util::formatSecondsW(m_timeLeft) : Util::emptyStringT;
		case COLUMN_SPEED:
			if (m_is_torrent)
			{
				if (m_hits == -1 && m_speed)
				{
					return Util::formatBytesW(m_speed) + _T('/') + WSTRING(S);
				}
				else
				{
					return Util::emptyStringT;
				}
			}
			else
			{
				return m_status == STATUS_RUNNING ? (Util::formatBytesW(m_speed) + _T('/') + WSTRING(S)) : Util::emptyStringT;
			}
		case COLUMN_FILE:
			if (m_is_torrent)
			{
				return Util::getFileName(m_target);
			}
			else
			{
				return getFile(m_type, Util::getFileName(m_target));
			}
		case COLUMN_SIZE:
			return m_size >= 0 ? Util::formatBytesW(m_size) : Util::emptyStringT;
		case COLUMN_PATH:
		{
			if (m_is_torrent)
				return Util::getFilePath(m_target);
			else
				return Util::getFilePath(m_target);
		}
		case COLUMN_IP:
			return m_transfer_ip;
#ifdef FLYLINKDC_USE_COLUMN_RATIO
		// [~] brain-ripper
		//case COLUMN_RATIO: return (status == STATUS_RUNNING) ? Util::toStringW(ratio()) : Util::emptyStringT;
		case COLUMN_RATIO:
			return m_ratio_as_text;
#endif
		case COLUMN_CIPHER:
			if (m_is_torrent)
			{
#ifdef _DEBUG
				const string l_sha = libtorrent::aux::to_hex(m_sha1);
				return _T("SHA1: ") + Text::toT(l_sha);
#endif
			}
			else
			{
				return m_cipher + _T(" [Token: ") + Text::toT(m_token) + _T("]");
			}
		case COLUMN_SHARE:
			return m_hintedUser.user ? Util::formatBytesW(m_hintedUser.user->getBytesShared()) : Util::emptyStringT;
		case COLUMN_SLOTS:
			return m_hintedUser.user ? Util::toStringW(m_hintedUser.user->getSlots()) : Util::emptyStringT;
		case COLUMN_P2P_GUARD:
			return m_p2p_guard_text;
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		case COLUMN_ANTIVIRUS:
			return m_antivirus_text;
#endif
		case COLUMN_LOCATION:
		{
			if (m_location.isKnown())
				return m_location.getDescription();
			else
				return Util::emptyStringT;
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
	//const auto l_result = ConnectionManager::getCipherNameAndIP(const_cast<UserConnection*>(t->getUserConnection()) , l_chiper_name, l_ip);
	//dcassert(l_result);
	ui->setCipher(t->getChiperName());
	ui->setIP(t->getIP());
}

void TransferView::on(DownloadManagerListener::Requesting, const DownloadPtr& aDownload) noexcept
{
#ifdef _DEBUG
	//LogManager::message("Requesting " + aDownload->getConnectionToken());
#endif
	UpdateInfo* ui = new UpdateInfo(aDownload->getHintedUser(), true); // [!] IRainman fix.
	// TODO - AirDC++
	// if (hubChanged)
	//  ui->setUser(d->getHintedUser());
	
	starting(ui, aDownload.get());
	ui->setActual(aDownload->getActual());
	ui->setSize(aDownload->getSize());
	ui->setStatus(ItemInfo::STATUS_RUNNING);
	ui->setTarget(aDownload->getPath());
	ui->updateMask &= ~UpdateInfo::MASK_STATUS; // hack to avoid changing item status
	ui->setStatusString(TSTRING(REQUESTING) + _T(' ') + getFile(aDownload->getType(), Text::toT(Util::getFileName(aDownload->getPath()))) + _T("..."));
	if (aDownload->getUserConnection())
	{
		const auto l_token = aDownload->getUserConnection()->getConnectionQueueToken();
		dcassert(!l_token.empty());
		ui->setToken(l_token);
	}
	else
	{
		dcassert(0);
	}
	m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
}

#ifdef FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE
void TransferView::on(DownloadManagerListener::Starting, const DownloadPtr& aDownload) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(aDownload->getHintedUser(), true); // [!] IRainman fix.
		
		ui->setStatus(ItemInfo::STATUS_RUNNING);
		ui->setStatusString(TSTRING(DOWNLOAD_STARTING));
		ui->setTarget(aDownload->getPath());
		ui->setType(aDownload->getType());
		// [-] ui->setIP(aDownload->getUserConnection()->getRemoteIp()); // !SMT!-IP [-] IRainman opt.
		
		m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
	}
}
#endif // FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE

void TransferView::on(DownloadManagerListener::Failed, const DownloadPtr& aDownload, const string& aReason) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(aDownload->getHintedUser(), true, true);
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setSize(aDownload->getSize());
		ui->setTarget(aDownload->getPath());
		ui->setType(aDownload->getType());
		// [-] ui->setIP(aDownload->getUserConnection()->getRemoteIp()); // !SMT!-IP [-] IRainman opt.
		if (aDownload->getUserConnection())
		{
			//const auto l_token = aDownload->getUserConnection()->getConnectionQueueToken();
			//dcassert(!l_token.empty());
			
			//ui->setToken(l_token); // fix https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/1671
		}
		else
		{
			dcassert(0);
		}
		
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
		            TSTRING(USER) + _T(": ") + WinUtil::getNicks(ui->m_hintedUser) + _T('\n') +
		            TSTRING(REASON) + _T(": ") + tmpReason, TSTRING(DOWNLOAD_FAILED) + _T(' '), NIIF_WARNING);
		m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
	}
}

void TransferView::on(DownloadManagerListener::Status, const UserConnection* p_conn, const std::string& aReason) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		// dcassert(const_cast<UserConnection*>(uc)->getDownload()); // TODO при окончании закачки это поле уже пустое https://www.box.net/shared/4cknwlue3njzksmciu63
		UpdateInfo* ui = new UpdateInfo(p_conn->getHintedUser(), true); // [!] IRainman fix.
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setStatusString(Text::toT(aReason));
		// TODO ui->setToken(p_conn->getConnectionQueueToken());
		m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
	}
}

void TransferView::on(UploadManagerListener::Starting, const UploadPtr& aUpload) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(aUpload->getHintedUser(), false); // [!] IRainman fix.
		
		starting(ui, aUpload.get());
		
		ui->setStatus(ItemInfo::STATUS_RUNNING);
		ui->setActual(aUpload->getStartPos() + aUpload->getActual());
		ui->setSize(aUpload->getType() == Transfer::TYPE_TREE ? aUpload->getSize() : aUpload->getFileSize());
		ui->setTarget(aUpload->getPath());
		ui->setRunning(1);
		// [-] ui->setIP(aUpload->getUserConnection()->getRemoteIp()); // !SMT!-IP [-] IRainman opt.
		
		if (!aUpload->isSet(Upload::FLAG_RESUMED))
		{
			ui->setStatusString(TSTRING(UPLOAD_STARTING));
		}
		// TODO ui->setToken(aUpload->getUserConnection()->getConnectionQueueToken());
		m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
	}
}
void TransferView::on(DownloadManagerListener::TorrentEvent, const DownloadArray& p_torrent_event) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		for (auto j = p_torrent_event.cbegin(); j != p_torrent_event.cend(); ++j)
		{
			UpdateInfo* ui = new UpdateInfo(j->m_hinted_user, true);
			ui->setStatus(ItemInfo::STATUS_RUNNING);
			ui->setActual(j->m_actual);
			ui->setPos(j->m_pos);
			dcassert(j->m_is_torrent);
			ui->m_is_torrent = j->m_is_torrent;
			ui->m_sha1 = j->m_sha1;
			ui->m_is_seeding = j->m_is_seeding;
			ui->m_is_pause = j->m_is_pause;
			ui->setSpeed(j->m_speed);
			ui->setSize(j->m_size);
			ui->setTimeLeft(j->m_second_left);
			ui->setType(Transfer::Type(j->m_type)); // TODO
			ui->setStatusString(j->m_status_string);
			ui->setTarget(j->m_path);
			//ui->setToken(j->m_token);
			//dcassert(!j->m_token.empty());
			m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
		}
	}
}

// TODO - убрать тики для массива
void TransferView::on(DownloadManagerListener::Tick, const DownloadArray& dl) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		if (!MainFrame::isAppMinimized())// [+]IRainman opt
		{
			for (auto j = dl.cbegin(); j != dl.cend(); ++j)
			{
				dcassert(j->m_is_torrent == false);
				UpdateInfo* ui = new UpdateInfo(j->m_hinted_user, true); // [!] IRainman fix.
				ui->setStatus(ItemInfo::STATUS_RUNNING);
				ui->setActual(j->m_actual);
				ui->setPos(j->m_pos);
				ui->m_is_torrent = j->m_is_torrent;
				ui->setSpeed(j->m_speed);
				ui->setSize(j->m_size);
				ui->setTimeLeft(j->m_second_left);
				ui->setType(Transfer::Type(j->m_type)); // TODO
				ui->setStatusString(j->m_status_string);
				ui->setTarget(j->m_path);
				ui->setToken(j->m_token);
				dcassert(!j->m_token.empty());
				m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
			}
		}
	}
}

// TODO - убрать тики для массива
void TransferView::on(UploadManagerListener::Tick, const UploadArray& ul) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		if (!MainFrame::isAppMinimized())// [+]IRainman opt
		{
			for (auto j = ul.cbegin(); j != ul.cend(); ++j)
			{
				if (j->m_pos == 0)
				{
					dcassert(0);
					continue;
				}
				UpdateInfo* ui = new UpdateInfo(j->m_hinted_user, false); // [!] IRainman fix.
				ui->setStatus(ItemInfo::STATUS_RUNNING);
				ui->setActual(j->m_actual);
				ui->setPos(j->m_pos);
				ui->setSize(j->m_size);
				ui->setTimeLeft(j->m_second_left);
				ui->setSpeed(j->m_speed);
				ui->setType(Transfer::Type(j->m_type)); // TODO
				ui->setStatusString(j->m_status_string);
				ui->setTarget(j->m_path);
				ui->setToken(j->m_token);
				dcassert(!j->m_token.empty());
				m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
			}
		}
	}
}

void TransferView::onTransferComplete(const Transfer* aTransfer, const bool download, const string& aFileName)
{
	if (!ClientManager::isBeforeShutdown())
	{
	
#ifdef _DEBUG
		//LogManager::message("Transfer complete " + aTransfer->getConnectionToken());
#endif
		UpdateInfo* ui = new UpdateInfo(aTransfer->getHintedUser(), download); // [!] IRainman fix.
		
		ui->setTarget(aTransfer->getPath());
		//ui->setToken(aTransfer->getConnectionToken());
		ui->setStatus(ItemInfo::STATUS_WAITING);
		/*
		    if (aTransfer->getType() == Transfer::TYPE_FULL_LIST)
		    {
		        ui->setPos(aTransfer->getSize());
		    }
		    else
		    {
		        ui->setPos(0);
		    }
		*/
		ui->setPos(aTransfer->getFileSize());
		ui->setActual(aTransfer->getFileSize());
		ui->setSize(aTransfer->getFileSize());
		ui->setTimeLeft(0);
		ui->setRunning(0);
		ui->setStatusString(download ? TSTRING(DOWNLOAD_FINISHED_IDLE) : TSTRING(UPLOAD_FINISHED_IDLE));
		// TODO  ui->setToken(aTransfer->getUserConnection()->getConnectionQueueToken());
		if (!download && aTransfer->getType() != Transfer::TYPE_TREE)
		{
			SHOW_POPUP(POPUP_UPLOAD_FINISHED,
			           TSTRING(FILE) + _T(": ") + Text::toT(aFileName) + _T('\n') +
			           TSTRING(USER) + _T(": ") + WinUtil::getNicks(aTransfer->getHintedUser()), TSTRING(UPLOAD_FINISHED_IDLE));
		}
		
		m_tasks.add(TRANSFER_UPDATE_ITEM, ui);
	}
}

void TransferView::ItemInfo::pauseTorrentFile()
{
	DownloadManager::getInstance()->pause_torrent_file(m_sha1, false);
}

void TransferView::ItemInfo::resumeTorrentFile()
{
	DownloadManager::getInstance()->pause_torrent_file(m_sha1, true);
}

void TransferView::ItemInfo::removeTorrentAndFile()
{
	DownloadManager::getInstance()->remove_torrent_file(m_sha1, lt::session::delete_files);
}

void TransferView::ItemInfo::removeTorrent()
{
	DownloadManager::getInstance()->remove_torrent_file(m_sha1, { });
}

void TransferView::ItemInfo::disconnectAndP2PGuard()
{
	CFlyP2PGuardArray l_sqlite_array;
	uint32_t l_ip = Socket::convertIP4(Text::fromT(m_transfer_ip));
	if (l_ip)
	{
		const string l_marker = "Manual block IP";
		l_sqlite_array.push_back(CFlyP2PGuardIP(l_marker, l_ip, l_ip));
		CFlylinkDBManager::getInstance()->save_p2p_guard(l_sqlite_array, l_marker, 1);
	}
	disconnect();
}

void TransferView::ItemInfo::disconnect()
{
	ConnectionManager::disconnect(m_hintedUser.user, download);
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
			const auto qi = QueueManager::FileQueue::find_target(target);
			if (qi)
			{
				startMediaPreview(wID, qi); // [!]
			}
		}
		else
		{
			startMediaPreview(wID, target
#ifdef SSA_VIDEO_PREVIEW_FEATURE
			                  , ii->m_size
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
		ItemInfo* m = ctrlTransfers.getItemData(q);
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
			ItemInfo* jj = *j;
			jj->disconnect();
			
			int h = ctrlTransfers.findItem(jj);
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
			const string l_tmp = Text::fromT(ii->m_target);
			
			QueueManager::LockFileQueueShared l_fileQueue;
			const auto& queue = l_fileQueue.getQueueL();
			const auto qi = queue.find(l_tmp);
			if (qi != queue.cend())
				qi->second->changeAutoDrop(); // [!]
		}
		// [!] IRainman fix.
	}
	
	return 0;
}

void TransferView::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlTransfers.isRedraw())
		{
			ctrlTransfers.setFlickerFree(Colors::g_bgBrush);
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

void TransferView::on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept // [+] IRainman opt.
{
	if (!ClientManager::isBeforeShutdown())
	{
		if (!MainFrame::isAppMinimized())
		{
			on(QueueManagerListener::StatusUpdatedList(), p_list);
		}
	}
}

void TransferView::on(QueueManagerListener::StatusUpdatedList, const QueueItemList& p_list) noexcept // [+] IRainman opt.
{
	dcassert(!p_list.empty());
	if (!ClientManager::isBeforeShutdown() && !p_list.empty())
	{
		for (auto i = p_list.cbegin(); i != p_list.cend(); ++i)
		{
			on(QueueManagerListener::StatusUpdated(), *i);
		}
	}
}
void TransferView::on(QueueManagerListener::StatusUpdated, const QueueItemPtr& qi) noexcept
{
	if (qi->isUserList())
		return;
	auto l_ui = new UpdateInfo();
	parseQueueItemUpdateInfo(l_ui, qi);
	m_tasks.add(TRANSFER_UPDATE_PARENT, l_ui);
}

void TransferView::parseQueueItemUpdateInfo(UpdateInfo* ui, const QueueItemPtr& qi)
{
	ui->setTarget(qi->getTarget());
	ui->setType(Transfer::TYPE_FILE);
	
	if (qi->isRunning())
	{
		double ratio = 0;
		bool partial = false, trusted = false, untrusted = false, tthcheck = false, zdownload = false, chunked = false;
		const int64_t totalSpeed = qi->getAverageSpeed();
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
			
			if (qi->getTimeFileBegin() == 0)
			{
				// file is starting
				qi->setTimeFileBegin(GET_TICK());
				ui->setStatusString(TSTRING(DOWNLOAD_STARTING));
				PLAY_SOUND(SOUND_BEGINFILE);
				SHOW_POPUP(POPUP_DOWNLOAD_START, TSTRING(FILE) + _T(": ") + Util::getFileName(ui->m_target), TSTRING(DOWNLOAD_STARTING));
			}
			else
			{
				const uint64_t time = GET_TICK() - qi->getTimeFileBegin();
				if (time > 1000)
				{
					const tstring pos = Util::formatBytesW(ui->pos);
					double percent = double(ui->pos) * 100.0 / double(ui->size);
					//dcassert(percent <= 100);
					if (percent > 100)
					{
						percent = 100;
					}
					const tstring elapsed = Util::formatSecondsW(time / 1000);
					tstring flag;
					if (partial)
					{
						flag += _T("[P]");
					}
					if (trusted)
					{
						flag += _T("[SSL+T]");
					}
					if (untrusted)
					{
						flag += _T("[SSL+U]");
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
					ui->setErrorStatusString(Util::emptyStringT);
				}
			}
		}
	}
	else
	{
		qi->setTimeFileBegin(0);
		ui->setSize(qi->getSize());
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setRunning(0);
	}
} // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=152461

void TransferView::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string&, const DownloadPtr& p_download) noexcept
{

	if (!ClientManager::isBeforeShutdown())
	{
		if (qi->isUserList())
		{
			return;
		}
		
		// update download item
		UpdateInfo* ui = new UpdateInfo(p_download->getHintedUser(), true); // [!] IRainman fix.
		
		ui->setTarget(qi->getTarget());
		ui->setStatus(ItemInfo::STATUS_WAITING);
		ui->setStatusString(TSTRING(DOWNLOAD_FINISHED_IDLE));
		m_tasks.add(TRANSFER_UPDATE_ITEM, ui); // [!] IRainman opt.
		// update file item
		ui = new UpdateInfo(p_download->getHintedUser(), true); // [!] IRainman fix.
		ui->setTarget(qi->getTarget());
		ui->setStatusString(TSTRING(DOWNLOAD_FINISHED_IDLE));
		ui->setStatus(ItemInfo::STATUS_WAITING);
		SHOW_POPUP(POPUP_DOWNLOAD_FINISHED, TSTRING(FILE) + _T(": ") + Util::getFileName(ui->m_target), TSTRING(DOWNLOAD_FINISHED_IDLE));
		m_tasks.add(TRANSFER_UPDATE_PARENT, ui);  // [!] IRainman opt.
	}
}

void TransferView::on(DownloadManagerListener::CompleteTorrentFile, const std::string& p_name) noexcept
{
	SHOW_POPUP(POPUP_DOWNLOAD_FINISHED, TSTRING(FILE) + _T(": ") + Text::toT(p_name), TSTRING(DOWNLOAD_FINISHED_IDLE));
}

void TransferView::on(DownloadManagerListener::SelectTorrent, const libtorrent::sha1_hash& p_sha1, CFlyTorrentFileArray& p_files) noexcept
{
	// TODO - PostMessage
	try {
		CFlyTorrentDialog l_dlg(p_files);
		if (l_dlg.DoModal(WinUtil::g_mainWnd) == IDOK)
		{
			DownloadManager::getInstance()->set_file_priority(p_sha1, l_dlg.m_files, l_dlg.m_selected_files); // TODO - убрать m_selected_files
			DownloadManager::getInstance()->fire_added_torrent(p_sha1);
		}
		else
		{
			DownloadManager::getInstance()->remove_torrent_file(p_sha1, lt::session::delete_files);
		}
	}
	catch (const Exception &e)
	{
		LogManager::message("DownloadManagerListener::SelectTorrent - error " + e.getError());
	}
}

void TransferView::on(DownloadManagerListener::RemoveTorrent, const libtorrent::sha1_hash& p_sha1) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(p_sha1);
		m_tasks.add(TRANSFER_REMOVE_TOKEN_ITEM, ui);
	}
}
void TransferView::on(DownloadManagerListener::RemoveToken, const string& p_token) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(HintedUser(), true);
		ui->setToken(p_token);
		m_tasks.add(TRANSFER_REMOVE_TOKEN_ITEM, ui);
	}
}
void TransferView::on(ConnectionManagerListener::RemoveToken, const string& p_token) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UpdateInfo* ui = new UpdateInfo(HintedUser(), true);
		ui->setToken(p_token);
		m_tasks.add(TRANSFER_REMOVE_TOKEN_ITEM, ui);
	}
}
void TransferView::on(QueueManagerListener::RemovedTransfer, const QueueItemPtr& qi) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		dcassert(!qi->getTarget().empty());
		if (!qi->getTarget().empty())
		{
			UpdateInfo* ui = new UpdateInfo(HintedUser(qi->getFirstUser(), Util::emptyString), true);
			ui->setTarget(qi->getTarget());
			m_tasks.add(TRANSFER_REMOVE_DOWNLOAD_ITEM, ui);
		}
	}
}
void TransferView::on(QueueManagerListener::Removed, const QueueItemPtr& qi) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		UserList l_users;
		qi->getAllDownloadsUsers(l_users);
		for (auto i = l_users.cbegin(); i != l_users.cend(); ++i)
		{
			dcassert(!qi->getTarget().empty());
			if (!qi->getTarget().empty())
			{
				UpdateInfo* ui = new UpdateInfo(HintedUser(*i, Util::emptyString), true);
				ui->setTarget(qi->getTarget());
				m_tasks.add(TRANSFER_REMOVE_DOWNLOAD_ITEM, ui);
			}
		}
	}
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
		if (l_ii)
		{
			if (getTTH(l_ii, l_tth))
			{
				tstring l_sdata;
				if (wID == IDC_COPY_TTH)
					l_sdata = Text::toT(l_tth.toBase32());
				else if (wID == IDC_COPY_LINK)
					l_sdata = Text::toT(Util::getMagnet(l_tth, Text::fromT(Util::getFileName(l_ii->m_target)), l_ii->m_size));
				else if (wID == IDC_COPY_WMLINK)
					l_sdata = Text::toT(Util::getWebMagnet(l_tth, Text::fromT(Util::getFileName(l_ii->m_target)), l_ii->m_size));
				else
					l_sdata = l_ii->getText(columnId);
					
				if (l_data.empty())
					l_data = l_sdata;
				else
					l_data += L"\r\n" + l_sdata;
			}
			else if (columnId >= COLUMN_FIRST && columnId < COLUMN_LAST)
			{
				l_data += l_ii->getText(columnId);
			}
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
		return QueueManager::getTTH(Text::fromT(p_ii->m_target), p_tth);
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

LRESULT TransferView::onRemoveAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UINT checkState = BOOLSETTING(CONFIRM_DELETE) ? BST_UNCHECKED : BST_CHECKED; // [+] InfinitySky.
	if (checkState == BST_CHECKED || ::MessageBox(NULL, CTSTRING(REALLY_REMOVE), getFlylinkDCAppCaptionWithVersionT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES) // [~] InfinitySky.
		ctrlTransfers.forEachSelected(&ItemInfo::removeAll); // [6] https://www.box.net/shared/4eed8e2e275210b6b654
	// Let's update the setting unchecked box means we bug user again...
	SET_SETTING(CONFIRM_DELETE, checkState != BST_CHECKED); // [+] InfinitySky.
	return 0;
}

/**
 * @file
 * $Id: TransferView.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
