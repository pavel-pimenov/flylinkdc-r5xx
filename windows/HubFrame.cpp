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
#include "HubFrame.h"
#include "SearchFrm.h"
#include "PrivateFrame.h"
#include "TextFrame.h"
#include "ChatBot.h"
#include "BarShader.h"
#include "../client/QueueManager.h"
#include "../client/ShareManager.h"
#include "../client/Util.h"
#ifdef STRONG_USE_DHT
#include "../dht/DHT.h"
#endif
#include "../windows/MainFrm.h"
#include "../client/LogManager.h"
#include "../client/AdcCommand.h"
#include "../client/SettingsManager.h"
#include "../client/ConnectionManager.h"
#include "../client/NmdcHub.h"
#ifdef SCALOLAZ_HUB_MODE
#include "HIconWrapper.h"
#include "../client/ResourceManager.h"
#endif
#include "FavHubProperties.h"


HubFrame::FrameMap HubFrame::g_frames;

#ifdef SCALOLAZ_HUB_SWITCH_BTN
HIconWrapper HubFrame::g_hSwitchPanelsIco(IDR_SWITCH_PANELS_ICON);
#endif

#ifdef SCALOLAZ_HUB_MODE
HIconWrapper HubFrame::g_hModeActiveIco(IDR_ICON_SUCCESS_ICON);
HIconWrapper HubFrame::g_hModePassiveIco(IDR_ICON_WARN_ICON);
HIconWrapper HubFrame::g_hModeNoneIco(IDR_ICON_FAIL_ICON);
#endif

int HubFrame::g_columnSizes[] = { 100,    // COLUMN_NICK
                                  75,     // COLUMN_SHARED
                                  150,    // COLUMN_EXACT_SHARED
                                  150,    // COLUMN_DESCRIPTION
                                  150,    // COLUMN_APPLICATION
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                  75,     // COLUMN_CONNECTION
#endif
                                  50,     // COLUMN_EMAIL
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                  50,     // COLUMN_VERSION
                                  40,     // COLUMN_MODE
#endif
                                  40,     // COLUMN_HUBS
                                  40,     // COLUMN_SLOTS
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                  40,     // COLUMN_UPLOAD_SPEED
#endif
                                  100,    // COLUMN_IP
                                  100,    // COLUMN_GEO_LOCATION
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
                                  50,     // COLUMN_UPLOAD
                                  50,     // COLUMN_DOWNLOAD
#endif
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
#ifdef PPA_INCLUDE_DNS
                                  100,    // COLUMN_DNS
#endif
#endif
                                  300,     // COLUMN_CID
                                  200      // COLUMN_TAG
                                }; // !SMT!-IP

int HubFrame::g_columnIndexes[] = { COLUMN_NICK,
                                    COLUMN_SHARED,
                                    COLUMN_EXACT_SHARED,
                                    COLUMN_DESCRIPTION,
                                    COLUMN_APPLICATION,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                    COLUMN_CONNECTION,
#endif
                                    COLUMN_EMAIL,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                    COLUMN_VERSION,
                                    COLUMN_MODE,
#endif
                                    COLUMN_HUBS,
                                    COLUMN_SLOTS,
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                    COLUMN_UPLOAD_SPEED,
#endif
                                    COLUMN_IP,
                                    COLUMN_GEO_LOCATION,
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
                                    COLUMN_UPLOAD,
                                    COLUMN_DOWNLOAD,
#endif
#ifdef PPA_INCLUDE_DNS
                                    COLUMN_DNS, // !SMT!-IP
#endif
//[-]PPA        COLUMN_PK
                                    COLUMN_CID,
                                    COLUMN_TAG
                                  };

static ResourceManager::Strings g_columnNames[] = { ResourceManager::NICK,            // COLUMN_NICK
                                                    ResourceManager::SHARED,          // COLUMN_SHARED
                                                    ResourceManager::EXACT_SHARED,    // COLUMN_EXACT_SHARED
                                                    ResourceManager::DESCRIPTION,     // COLUMN_DESCRIPTION
                                                    ResourceManager::APPLICATION,     // COLUMN_APPLICATION
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                                    ResourceManager::CONNECTION,      // COLUMN_CONNECTION
#endif
                                                    ResourceManager::EMAIL,           // COLUMN_EMAIL
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                                    ResourceManager::VERSION,         // COLUMN_VERSION
                                                    ResourceManager::MODE,            // COLUMN_MODE
#endif
                                                    ResourceManager::HUBS,            // COLUMN_HUBS
                                                    ResourceManager::SLOTS,           // COLUMN_SLOTS
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
                                                    ResourceManager::AVERAGE_UPLOAD,  // COLUMN_UPLOAD_SPEED
#endif
                                                    ResourceManager::IP_BARE,         // COLUMN_IP
                                                    ResourceManager::LOCATION_BARE,   // COLUMN_GEO_LOCATION
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
                                                    ResourceManager::UPLOADED,         // COLUMN_UPLOAD
                                                    ResourceManager::DOWNLOADED,         // COLUMN_DOWNLOAD
#endif
#ifdef PPA_INCLUDE_DNS
                                                    ResourceManager::DNS_BARE,        // COLUMN_DNS // !SMT!-IP
#endif
//[-]PPA  ResourceManager::PK
                                                    ResourceManager::CID,             // COLUMN_CID
                                                    ResourceManager::TAG,             // COLUMN_TAG
                                                  };

// [+] IRainman: copy-past fix.
void HubFrame::addFrameLogParams(StringMap& params)
{
	// TODO
}

void HubFrame::addMesageLogParams(StringMap& params, tstring aLine, bool bThirdPerson, tstring extra)
{
	// TODO
}
// [~] IRainman: copy-past fix.

HubFrame::HubFrame(const tstring& aServer,
                   const tstring& aName,
                   const tstring& aRawOne,
                   const tstring& aRawTwo,
                   const tstring& aRawThree,
                   const tstring& aRawFour,
                   const tstring& aRawFive,
                   int  p_ChatUserSplit,
                   bool p_UserListState
                   //bool p_ChatUserSplitState
                  ) :
	CFlyTimerAdapter(m_hWnd)
	, client(nullptr) // на всякий случай :)
	, m_second_count(60)
	, server(aServer)
	, m_waitingForPW(false)
	, m_password_do_modal(0)
	, m_needsUpdateStats(false)
	, m_needsResort(false)
	, m_showUsersContainer(nullptr)
	, ctrlClientContainer(WC_EDIT, this, EDIT_MESSAGE_MAP)
	, m_ctrlFilterContainer(nullptr)
	, m_ctrlFilterSelContainer(nullptr)
	, m_ctrlFilter(nullptr)
	, m_ctrlFilterSel(nullptr)
	, m_FilterSelPos(COLUMN_NICK)
#ifdef SCALOLAZ_HUB_SWITCH_BTN
	, m_ctrlSwitchPanels(nullptr)
	, m_isClientUsersSwitch(nullptr)
	, m_switchPanelsContainer(nullptr)
#endif
	, m_ctrlShowUsers(nullptr)
	, m_tooltip_hubframe(nullptr)
#ifdef SCALOLAZ_HUB_MODE
	, m_ctrlShowMode(nullptr)
#endif
	, m_tabMenuShown(false)
	, m_showJoins(false)
	, m_favShowJoins(false)
	, m_isUpdateColumnsInfoProcessed(false)
	, m_tabMenu(nullptr)
	, m_userMenu(nullptr)
	, m_ActivateCounter(0)
	, m_is_window_text_update(0)
	, m_Theme(nullptr)
{
	m_showUsersStore = p_UserListState;
	m_showUsers = false;
	client = ClientManager::getInstance()->getClient(Text::fromT(aServer));
	m_nProportionalPos = p_ChatUserSplit;
	m_ctrlStatusCache.resize(5);
	client->addListener(this);
	client->setName(Text::fromT(aName));
	client->setRawOne(Text::fromT(aRawOne));
	client->setRawTwo(Text::fromT(aRawTwo));
	client->setRawThree(Text::fromT(aRawThree));
	client->setRawFour(Text::fromT(aRawFour));
	client->setRawFive(Text::fromT(aRawFive));
}

void HubFrame::doDestroyFrame()
{
	destroyOMenu();
	destroyMessagePanel(true);
}

HubFrame::~HubFrame()
{
	ClientManager::getInstance()->putClient(client);
	// На форварде падает
	// dcassert(g_frames.find(server) != g_frames.end());
	// dcassert(g_frames[server] == this);
	g_frames.erase(server);
	dcassert(m_userMap.empty());
}

LRESULT HubFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	// [!]IRainman please update it
	// Could you please update all array entries according to a global enum located in UserInfo.h
	BOOST_STATIC_ASSERT(_countof(HubFrame::g_columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(HubFrame::g_columnIndexes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(g_columnNames) == COLUMN_LAST);
	// [~]IRainman
	
	BaseChatFrame::OnCreate(m_hWnd, rcDefault);
	
	ctrlClientContainer.SubclassWindow(ctrlClient.m_hWnd);
	
	
	ctrlClient.setHubParam(client->getHubUrl(), client->getMyNick()); // !SMT!-S
//[-]PPA    ctrlClient.setClient(client);

	ctrlUsers.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_STATICEDGE, IDC_USERS);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlUsers);
// TODO - может колонки не создвать пока они не нужны?
	bHandled = FALSE;
	client->connect();
#ifdef RIP_USE_CONNECTION_AUTODETECT
	ConnectionManager::getInstance()->addListener(this);
#endif
	// loadIgnoreList(); // Load ignore list [-] IRainman fix.
	create_timer(1000);
	return 1;
}
void HubFrame::updateColumnsInfo(const FavoriteHubEntry *p_fhe)
{
	if (!m_isUpdateColumnsInfoProcessed) // Апдейт колон делаем только один раз при первой активации т.к. ListItem не разрушается
	{
		m_isUpdateColumnsInfoProcessed = true;
		SettingsManager::getInstance()->addListener(this);
		BOOST_STATIC_ASSERT(_countof(g_columnSizes) == COLUMN_LAST);
		BOOST_STATIC_ASSERT(_countof(g_columnNames) == COLUMN_LAST);
		for (size_t j = 0; j < COLUMN_LAST; ++j)
		{
			const int fmt = (j == COLUMN_SHARED || j == COLUMN_EXACT_SHARED || j == COLUMN_SLOTS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
			ctrlUsers.InsertColumn(j, TSTRING_I(g_columnNames[j]), fmt, g_columnSizes[j], j); //-V107
		}
		if (p_fhe)
		{
			WinUtil::splitTokens(g_columnIndexes, p_fhe->getHeaderOrder(), COLUMN_LAST);
			WinUtil::splitTokensWidth(g_columnSizes, p_fhe->getHeaderWidths(), COLUMN_LAST);
		}
		else
		{
			WinUtil::splitTokens(g_columnIndexes, SETTING(HUBFRAME_ORDER), COLUMN_LAST);
			WinUtil::splitTokensWidth(g_columnSizes, SETTING(HUBFRAME_WIDTHS), COLUMN_LAST);
		}
		for (size_t j = 0; j < COLUMN_LAST; ++j)
		{
			ctrlUsers.SetColumnWidth(j, g_columnSizes[j]);
		}
		
		ctrlUsers.setColumnOrderArray(COLUMN_LAST, g_columnIndexes);
		
		if (p_fhe)
		{
			ctrlUsers.setVisible(p_fhe->getHeaderVisible());
		}
		else
		{
			ctrlUsers.setVisible(SETTING(HUBFRAME_VISIBLE));
		}
		
		SET_LIST_COLOR(ctrlUsers);
		ctrlUsers.setFlickerFree(Colors::bgBrush);
		// ctrlUsers.setSortColumn(-1); // TODO - научится сортировать после активации фрейма а не в начале
		if (p_fhe && p_fhe->getHeaderSort() >= 0)
		{
			ctrlUsers.setSortColumn(p_fhe->getHeaderSort());
			ctrlUsers.setAscending(p_fhe->getHeaderSortAsc());
		}
		else
		{
			ctrlUsers.setSortColumn(SETTING(HUBFRAME_COLUMNS_SORT));
			ctrlUsers.setAscending(BOOLSETTING(HUBFRAME_COLUMNS_SORT_ASC));
		}
		ctrlUsers.SetImageList(g_userImage.getIconList(), LVSIL_SMALL);
		m_Theme = GetWindowTheme(ctrlUsers.m_hWnd);
		m_showJoins = p_fhe ? p_fhe->getShowJoins() : BOOLSETTING(SHOW_JOINS);
		m_favShowJoins = BOOLSETTING(FAV_SHOW_JOINS);
	}
}
void HubFrame::updateSplitterPosition(const FavoriteHubEntry *p_fhe)
{
	dcassert(m_ActivateCounter == 1);
	m_nProportionalPos = 0;
	if (p_fhe && p_fhe->getChatUserSplit())
	{
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		m_isClientUsersSwitch = p_fhe->getChatUserSplitState();
#endif
		m_nProportionalPos = p_fhe->getChatUserSplit();
	}
	else
	{
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		m_isClientUsersSwitch = true;
#endif
	}
	if (m_nProportionalPos == 0)
	{
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		m_isClientUsersSwitch = SETTING(HUB_POSITION) == SettingsManager::POS_RIGHT;
		m_nProportionalPos = m_isClientUsersSwitch ? 7500 : 2500;
#else
		m_nProportionalPos = 7500;
#endif
	}
	TuneSplitterPanes();
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
}

void HubFrame::createMessagePanel()
{
	bool l_is_need_update = false;
	dcassert(!ClientManager::isShutdown());
	if (m_ctrlFilter == nullptr && g_isStartupProcess == false)
	{
		++m_ActivateCounter;
		BaseChatFrame::createMessageCtrl(this, EDIT_MESSAGE_MAP);
		
		m_ctrlFilterContainer    = new CContainedWindow(WC_EDIT, this, FILTER_MESSAGE_MAP);
		m_ctrlFilter = new CEdit;
		m_ctrlFilter->Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                     ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
		m_ctrlFilterContainer->SubclassWindow(m_ctrlFilter->m_hWnd);
		m_ctrlFilter->SetFont(Fonts::systemFont); // [~] Sergey Shushknaov
		if (!m_filter.empty())
		{
			m_ctrlFilter->SetWindowTextW(m_filter.c_str());
		}
		
		m_ctrlFilterSelContainer = new CContainedWindow(WC_COMBOBOX, this, FILTER_MESSAGE_MAP);
		m_ctrlFilterSel = new CComboBox;
		m_ctrlFilterSel->Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL |
		                        WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
		m_ctrlFilterSelContainer->SubclassWindow(m_ctrlFilterSel->m_hWnd);
		m_ctrlFilterSel->SetFont(Fonts::systemFont); // [~] Sergey Shuhkanov
		
		for (size_t j = 0; j < COLUMN_LAST; ++j)
		{
			m_ctrlFilterSel->AddString(CTSTRING_I(g_columnNames[j]));
		}
		m_ctrlFilterSel->AddString(CTSTRING(ANY));
		m_ctrlFilterSel->SetCurSel(m_FilterSelPos);
		
		m_tooltip_hubframe = new CFlyToolTipCtrl;
		m_tooltip_hubframe->Create(m_hWnd, rcDefault, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP /*| TTS_BALLOON*/, WS_EX_TOPMOST);
		m_tooltip_hubframe->SetDelayTime(TTDT_AUTOPOP, 10000);
		dcassert(m_tooltip_hubframe->IsWindow());
		m_tooltip_hubframe->SetMaxTipWidth(255);   //[+] SCALOlaz: activate tooltips
		
		CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
		BaseChatFrame::createStatusCtrl(m_hWndStatusBar);
		
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		m_switchPanelsContainer = new CContainedWindow(WC_BUTTON, this, HUBSTATUS_MESSAGE_MAP),
		m_ctrlSwitchPanels = new CButton;
		m_ctrlSwitchPanels->Create(m_ctrlStatus->m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | BS_ICON | BS_CENTER | BS_PUSHBUTTON , 0, IDC_HUBS_SWITCHPANELS);
		m_ctrlSwitchPanels->SetFont(Fonts::systemFont);
		m_ctrlSwitchPanels->SetIcon(g_hSwitchPanelsIco);
		m_switchPanelsContainer->SubclassWindow(m_ctrlSwitchPanels->m_hWnd);
		m_tooltip_hubframe->AddTool(*m_ctrlSwitchPanels, ResourceManager::CMD_SWITCHPANELS);
#endif
		m_ctrlShowUsers = new CButton;
		m_ctrlShowUsers->Create(m_ctrlStatus->m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		m_ctrlShowUsers->SetButtonStyle(BS_AUTOCHECKBOX, false);
		m_ctrlShowUsers->SetFont(Fonts::systemFont);
		setShowUsersCheck();
		
		m_showUsersContainer = new CContainedWindow(WC_BUTTON, this, EDIT_MESSAGE_MAP);
		m_showUsersContainer->SubclassWindow(m_ctrlShowUsers->m_hWnd);
		m_tooltip_hubframe->AddTool(*m_ctrlShowUsers, ResourceManager::CMD_USERLIST);
#ifdef SCALOLAZ_HUB_MODE
		m_ctrlShowMode = new CStatic;
		m_ctrlShowMode->Create(m_ctrlStatus->m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SS_ICON | BS_CENTER | BS_PUSHBUTTON , 0);
#endif
		dcassert(client->getHubUrl() == Text::fromT(server));
		const FavoriteHubEntry *fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(Text::fromT(server));
		createFavHubMenu(fhe);
		updateColumnsInfo(fhe); // Настроим колонки списка юзеров
		m_ctrlMessage->SetFocus();
		if (m_ActivateCounter == 1)
		{
			m_showUsers = m_showUsersStore;
			if (m_showUsers)
			{
				firstLoadAllUsers();
			}
			updateSplitterPosition(fhe); // Обновим сплитер
		}
		l_is_need_update = true;
	}
	BaseChatFrame::createMessagePanel();
	if (l_is_need_update)
	{
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		HubModeChange();
#endif
		UpdateLayout(TRUE); // TODO - сконструировать статус отдельным методом
		restoreStatusFromCache(); // Восстанавливать статус нужно после UpdateLayout
	}
}
void HubFrame::destroyMessagePanel(bool p_is_destroy)
{
	const bool l_is_shutdown = p_is_destroy || ClientManager::isShutdown();
	if (m_ctrlFilter)
	{
		if (!l_is_shutdown)
		{
			WinUtil::GetWindowText(m_filter, *m_ctrlFilter);
			m_FilterSelPos = m_ctrlFilterSel->GetCurSel();
		}
		safe_destroy_window(m_tooltip_hubframe); // использует m_ctrlSwitchPanels и m_ctrlShowUsers и m_ctrlShowMode
#ifdef SCALOLAZ_HUB_MODE
		safe_destroy_window(m_ctrlShowMode);
#endif
		//safe_unsubclass_window(m_showUsersContainer);
		safe_destroy_window(m_ctrlShowUsers);
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		//safe_unsubclass_window(m_switchPanelsContainer);
		safe_destroy_window(m_ctrlSwitchPanels);
#endif
		//safe_unsubclass_window(m_ctrlFilterContainer);
		safe_destroy_window(m_ctrlFilter);
		//safe_unsubclass_window(m_ctrlFilterSelContainer);
		safe_destroy_window(m_ctrlFilterSel);
		safe_delete(m_tooltip_hubframe); // использует m_ctrlSwitchPanels и m_ctrlShowUsers и m_ctrlShowMode
#ifdef SCALOLAZ_HUB_MODE
		safe_delete(m_ctrlShowMode);
#endif
		safe_delete(m_ctrlShowUsers);
		safe_delete(m_showUsersContainer);
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		safe_delete(m_ctrlSwitchPanels);
		safe_delete(m_switchPanelsContainer);
#endif
		safe_delete(m_ctrlFilter);
		safe_delete(m_ctrlFilterContainer);
		safe_delete(m_ctrlFilterSel);
		safe_delete(m_ctrlFilterSelContainer);
	}
	BaseChatFrame::destroyStatusbar(l_is_shutdown);
	BaseChatFrame::destroyMessagePanel(l_is_shutdown);
	BaseChatFrame::destroyMessageCtrl(l_is_shutdown);
}

OMenu* HubFrame::createTabMenu()
{
	if (!m_tabMenu)
	{
		m_tabMenu  = new OMenu;
		m_tabMenu->CreatePopupMenu();
	}
	return m_tabMenu;
}
OMenu* HubFrame::createUserMenu()
{
	if (!m_userMenu)
	{
		m_userMenu = new OMenu;
		m_userMenu->CreatePopupMenu();
	}
	return m_userMenu;
}

void HubFrame::destroyOMenu()
{
	safe_delete(m_tabMenu);
	safe_delete(m_userMenu);
}

void HubFrame::onBeforeActiveTab(HWND aWnd)
{
	dcassert(m_hWnd);
	dcdrun(const auto l_size_g_frames = g_frames.size());
	if (BaseChatFrame::g_isStartupProcess == false)
	{
		for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i) // TODO помнить последний и не перебирать все для разрушения.
		{
			i->second->destroyMessagePanel(false);
		}
	}
	dcassert(l_size_g_frames == g_frames.size());
}
void HubFrame::onAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isShutdown())
	{
		dcassert(!ClientManager::isShutdown());
		dcassert(m_hWnd);
		createMessagePanel();
	}
}
void HubFrame::onInvalidateAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isShutdown())
	{
		if (BaseChatFrame::g_isStartupProcess == false)
		{
			if (m_ctrlStatus)
			{
				//m_ctrlStatus->RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
				// TODO подобрать более легкую команду. без этой пропадаю иконки в статусе.
				m_ctrlShowUsers->Invalidate();
#ifdef SCALOLAZ_HUB_SWITCH_BTN
				m_ctrlSwitchPanels->Invalidate();
#endif
#ifdef SCALOLAZ_HUB_MODE
				m_ctrlShowMode->Invalidate();
#endif
			}
		}
	}
}

HubFrame* HubFrame::openWindow(
    const tstring& p_server,
    const tstring& p_name,
    const tstring& p_rawOne,
    const tstring& p_rawTwo,
    const tstring& p_rawThree,
    const tstring& p_rawFour,
    const tstring& p_rawFive,
    int  p_windowposx,
    int  p_windowposy,
    int  p_windowsizex,
    int  p_windowsizey,
    int  p_windowtype,
    int  p_ChatUserSplit,
    bool p_UserListState
    // bool p_ChatUserSplitState,
    //const string& p_ColumsOrder,
    //const string& p_ColumsWidth,
    //const string& p_ColumsVisible
)
{
	HubFrame* frm;
	const auto i = g_frames.find(p_server);
	if (i == g_frames.end())
	{
		frm = new HubFrame(p_server,
		                   p_name,
		                   p_rawOne,
		                   p_rawTwo,
		                   p_rawThree,
		                   p_rawFour,
		                   p_rawFive,
		                   p_ChatUserSplit,
		                   p_UserListState
		                   //, p_ChatUserSplitState
		                  );
		g_frames.insert(make_pair(p_server, frm));
		
		const int nCmdShow = SW_SHOWDEFAULT; // TODO: find out what it wanted to do
		CRect rc = frm->rcDefault;
		rc.left   = p_windowposx;
		rc.top    = p_windowposy;
		rc.right  = rc.left + p_windowsizex;
		rc.bottom = rc.top + p_windowsizey;
		if (rc.left < 0 || rc.top < 0 || (rc.right - rc.left < 10) || (rc.bottom - rc.top) < 10)
		{
			CRect l_rcmdiClient;
			::GetWindowRect(WinUtil::mdiClient, &l_rcmdiClient);
			rc = l_rcmdiClient; // frm->rcDefault;
		}
		frm->CreateEx(WinUtil::mdiClient, rc);
		if (p_windowtype)
		{
			frm->ShowWindow((nCmdShow == SW_SHOWDEFAULT || nCmdShow == SW_SHOWNORMAL) ? p_windowtype : nCmdShow);
		}
	}
	else
	{
		frm = i->second;
		if (::IsIconic(frm->m_hWnd))
			::ShowWindow(frm->m_hWnd, SW_RESTORE);
		frm->MDIActivate(frm->m_hWnd);
	}
	return frm;
}

void HubFrame::processFrameMessage(const tstring& fullMessageText, bool& resetInputMessageText)
{
	if (m_waitingForPW)
	{
		addStatus(TSTRING(DONT_REMOVE_SLASH_PASSWORD));
		addPasswordCommand();
	}
	else
	{
		sendMessage(fullMessageText);
	}
}

StringMap HubFrame::getFrameLogParams() const
{
	StringMap params;
	
	params["hubNI"] = client->getHubName();
	params["hubURL"] = client->getHubUrl();
	params["myNI"] = client->getMyNick();
	
	return params;
}
void HubFrame::readFrameLog()
{
#ifdef IRAINMAN_LOAD_LOG_FOR_HUBS
	const auto linesCount = SETTING(SHOW_LAST_LINES_LOG);
	if (linesCount)
	{
		const string path = Util::validateFileName(SETTING(LOG_DIRECTORY) + Util::formatParams(SETTING(LOG_FILE_MAIN_CHAT), getFrameLogParams(), false));
		appendLogToChat(path, linesCount);
	}
#endif
}

void HubFrame::processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText)
{
	if (stricmp(cmd.c_str(), _T("join")) == 0)
	{
		if (!param.empty())
		{
			m_redirect = Text::toT(Util::formatDchubUrl(Text::fromT(param))); // [!] IRainman fix http://code.google.com/p/flylinkdc/issues/detail?id=1237
			if (BOOLSETTING(JOIN_OPEN_NEW_WINDOW))
			{
				openWindow(m_redirect); // [!] IRainman fix http://code.google.com/p/flylinkdc/issues/detail?id=1237
			}
			else
			{
				BOOL whatever = FALSE;
				onFollow(0, 0, 0, whatever);
			}
		}
		else
		{
			addStatus(TSTRING(SPECIFY_SERVER));
		}
	}
	else if ((stricmp(cmd.c_str(), _T("password")) == 0) && m_waitingForPW)
	{
		client->setPassword(Text::fromT(param));
		client->password(Text::fromT(param));
		m_waitingForPW = false;
	}
	else if (stricmp(cmd.c_str(), _T("showjoins")) == 0)
	{
		m_showJoins = !m_showJoins;
		if (m_showJoins)
		{
			addStatus(TSTRING(JOIN_SHOWING_ON));
		}
		else
		{
			addStatus(TSTRING(JOIN_SHOWING_OFF));
		}
	}
	else if (stricmp(cmd.c_str(), _T("favshowjoins")) == 0)
	{
		m_favShowJoins = !m_favShowJoins;
		if (m_favShowJoins)
		{
			addStatus(TSTRING(FAV_JOIN_SHOWING_ON));
		}
		else
		{
			addStatus(TSTRING(FAV_JOIN_SHOWING_OFF));
		}
	}
	else if (stricmp(cmd.c_str(), _T("close")) == 0) // TODO
	{
		PostMessage(WM_CLOSE);
	}
	else if (m_ctrlShowUsers && stricmp(cmd.c_str(), _T("userlist")) == 0)
	{
		setShowUsersCheck();
	}
	else if (stricmp(cmd.c_str(), _T("connection")) == 0 || stricmp(cmd.c_str(), _T("con")) == 0)
	{
		const tstring l_con = TSTRING(IP) + Text::toT(client->getLocalIp()) + _T(", ") +
		                      TSTRING(PORT) + _T(' ') +
		                      _T("TCP: ") + Util::toStringW(ConnectionManager::getInstance()->getPort()) + _T('/') +
		                      _T("UDP: ") + Util::toStringW(SearchManager::getInstance()->getPort()) + _T('/') +
		                      _T("TLS: ") + Util::toStringW(ConnectionManager::getInstance()->getSecurePort()) + _T('/')
#ifdef STRONG_USE_DHT
		                      + _T("DHT: ") + Util::toStringW(dht::DHT::getInstance()->getPort())
#endif
		                      ;
		if (param == _T("pub"))
			sendMessage(l_con);
		else
			addStatus(l_con);
	}
	else if ((stricmp(cmd.c_str(), _T("favorite")) == 0) || (stricmp(cmd.c_str(), _T("fav")) == 0))
	{
		// [+] IRainman fav options
		FavoriteManager::AutoStartType l_autoConnect;
		if (param == _T("a"))
		{
			l_autoConnect = FavoriteManager::ADD;
		}
		else if (param == _T("-a"))
		{
			l_autoConnect = FavoriteManager::REMOVE;
		}
		else
		{
			l_autoConnect = FavoriteManager::NOT_CHANGE;
		}
		addAsFavorite(l_autoConnect);
		// [~] IRainman fav options
	}
	else if ((stricmp(cmd.c_str(), _T("removefavorite")) == 0) || (stricmp(cmd.c_str(), _T("removefav")) == 0) || (stricmp(cmd.c_str(), _T("remfav")) == 0))
	{
		removeFavoriteHub();
	}
	else if (stricmp(cmd.c_str(), _T("getlist")) == 0)
	{
		if (!param.empty())
		{
			UserInfo* ui = findUser(param);
			if (ui)
			{
				ui->getList();
			}
		}
	}
	else if (stricmp(cmd.c_str(), _T("log")) == 0)
	{
		if (param.empty())
		{
			openFrameLog();
		}
		else if (stricmp(param.c_str(), _T("status")) == 0)
		{
			WinUtil::openLog(SETTING(LOG_FILE_STATUS), getFrameLogParams(), TSTRING(NO_LOG_FOR_STATUS));
		}
	}
	else if (stricmp(cmd.c_str(), _T("pm")) == 0)
	{
		tstring::size_type j = param.find(_T(' '));
		if (j != tstring::npos)
		{
			tstring nick = param.substr(0, j);
			const OnlineUserPtr ou = client->findUser(Text::fromT(nick));
			
			if (ou)
			{
				if (param.size() > j + 1)
					PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), client->getHubUrl()), client->getMyNick(), param.substr(j + 1));
				else
					PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), client->getHubUrl()), client->getMyNick());
			}
		}
		else if (!param.empty())
		{
			const OnlineUserPtr ou = client->findUser(Text::fromT(param));
			if (ou)
			{
				PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), client->getHubUrl()), client->getMyNick());
			}
		}
	}
#ifdef SCALOLAZ_HUB_SWITCH_BTN
	// [~] InfinitySky. Положение в окне.
	else if (stricmp(cmd.c_str(), _T("switch")) == 0)
	{
		if (m_showUsers)
			OnSwitchedPanels();
	}
#endif
// SSA_SAVE_LAST_NICK_MACROS
	else if (stricmp(cmd.c_str(), _T("nick")) == 0 || stricmp(cmd.c_str(), _T("n")) == 0) // [+] SSA
	{
		tstring sayMessage;
		if (!m_lastUserName.empty())
			sayMessage = m_lastUserName + getChatRefferingToNick() + _T(' ');
			
		sayMessage += param;
		sendMessage(sayMessage);
		if (m_ctrlMessage)
			m_ctrlMessage->SetWindowText(_T(""));
	}
// ~SSA_SAVE_LAST_NICK_MACROS
	else
	{
		if (BOOLSETTING(SEND_UNKNOWN_COMMANDS))
		{
			sendMessage(fullMessageText);
		}
		else
		{
			addStatus(TSTRING(UNKNOWN_COMMAND) + _T(' ') + cmd);
		}
	}
}

struct CompareItems
{
	CompareItems(int aCol) : col(aCol) { }
	bool operator()(const UserInfo& a, const UserInfo& b) const
	{
		return UserInfo::compareItems(&a, &b, col) < 0;
	}
	const int col;
};
/*
const tstring& HubFrame::getNick(const UserPtr& aUser)
{
    UserInfo* ui = findUser(aUser);
    if(!ui)
        return Util::emptyStringT;

    return ui-> columns[COLUMN_NICK];
}
*/
FavoriteHubEntry* HubFrame::addAsFavorite(const FavoriteManager::AutoStartType p_autoconnect/* = FavoriteManager::NOT_CHANGE*/)// [!] IRainman fav options
{
	auto fm = FavoriteManager::getInstance();
	FavoriteHubEntry* existingHub = fm->getFavoriteHubEntry(client->getHubUrl());
	if (!existingHub)
	{
		FavoriteHubEntry aEntry;
		LocalArray<TCHAR, 256> buf;
		GetWindowText(buf.data(), 255);
		aEntry.setServer(Text::fromT(server));
		aEntry.setName(Text::fromT(buf.data()));
		aEntry.setDescription(Text::fromT(buf.data()));
		if (!client->getPassword().empty())
		{
			aEntry.setNick(client->getMyNick());
			aEntry.setPassword(client->getPassword());
		}
		// [-] IRainman fav options aEntry.setConnect(false);
		existingHub = fm->addFavorite(aEntry, p_autoconnect);// [!] IRainman fav options
		addStatus(TSTRING(FAVORITE_HUB_ADDED));
	}
	else
	{
		// [+] IRainman fav options
		if (p_autoconnect != FavoriteManager::NOT_CHANGE)
		{
			const bool l_autoConnect = p_autoconnect == FavoriteManager::ADD;
			existingHub->setConnect(l_autoConnect);
			
			// rebuild fav hubs list
			FavoriteHubEntry aEntry;
			aEntry.setServer(existingHub->getServer());
			fm->addFavorite(aEntry, p_autoconnect);
		}
		// [~] IRainman fav options
	}
	// [+] IRainman fav options
	if (p_autoconnect != FavoriteManager::NOT_CHANGE)
	{
		const bool l_autoConnect = p_autoconnect == FavoriteManager::ADD;
		addStatus(l_autoConnect ? TSTRING(AUTO_CONNECT_ADDED) : TSTRING(AUTO_CONNECT_REMOVED));
	}
	// [~] IRainman
	createFavHubMenu(existingHub);
	dcassert(existingHub); // Функция возвращает иногда nullptr https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=39084
	return existingHub; // [+] IRainman fav options
}

void HubFrame::removeFavoriteHub()
{
	const FavoriteHubEntry* removeHub = FavoriteManager::getInstance()->getFavoriteHubEntry(client->getHubUrl());
	if (removeHub)
	{
		FavoriteManager::getInstance()->removeFavorite(removeHub);
		addStatus(TSTRING(FAVORITE_HUB_REMOVED));
	}
	else
	{
		addStatus(TSTRING(FAVORITE_HUB_DOES_NOT_EXIST));
	}
	createFavHubMenu(removeHub);
}

void HubFrame::createFavHubMenu(const FavoriteHubEntry* p_fhe)
{
	OMenu* l_tabMenu = createTabMenu();
	createTabMenu()->ClearMenu();
	const string& l_name = client->getHubName();
	l_tabMenu->InsertSeparatorFirst(Text::toT(l_name.size() > 50 ? (l_name.substr(0, 50) + "…") : l_name));
	if (BOOLSETTING(LOG_MAIN_CHAT))
	{
		l_tabMenu->AppendMenu(MF_STRING, IDC_OPEN_HUB_LOG, CTSTRING(OPEN_HUB_LOG));
		l_tabMenu->AppendMenu(MF_SEPARATOR);
	}
	if (p_fhe)
	{
		l_tabMenu->AppendMenu(MF_STRING, IDC_REM_AS_FAVORITE, CTSTRING(REMOVE_FROM_FAVORITES_HUBS));
		if (p_fhe->getConnect())
		{
			l_tabMenu->AppendMenu(MF_STRING, IDC_AUTO_START_FAVORITE, CTSTRING(AUTO_CONNECT_START_OFF));
		}
		else
		{
			l_tabMenu->AppendMenu(MF_STRING, IDC_AUTO_START_FAVORITE, CTSTRING(AUTO_CONNECT_START_ON));
		}
		l_tabMenu->AppendMenu(MF_STRING, IDC_EDIT_HUB_PROP, CTSTRING(PROPERTIES));
	}
	else
	{
		l_tabMenu->AppendMenu(MF_STRING, IDC_ADD_AS_FAVORITE, CTSTRING(ADD_TO_FAVORITES_HUBS));
	}
	l_tabMenu->AppendMenu(MF_STRING, ID_FILE_RECONNECT, CTSTRING(MENU_RECONNECT));
	l_tabMenu->AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)WinUtil::copyHubMenu, CTSTRING(COPY));
	l_tabMenu->AppendMenu(MF_STRING, ID_DISCONNECT, CTSTRING(DISCONNECT));
	l_tabMenu->AppendMenu(MF_SEPARATOR);
	l_tabMenu->AppendMenu(MF_STRING, IDC_RECONNECT_DISCONNECTED, CTSTRING(MENU_RECONNECT_DISCONNECTED));
	l_tabMenu->AppendMenu(MF_STRING, IDC_CLOSE_DISCONNECTED, CTSTRING(MENU_CLOSE_DISCONNECTED));
	l_tabMenu->AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
}

void HubFrame::autoConnectStart()
{
	const FavoriteHubEntry* existingHub = FavoriteManager::getInstance()->getFavoriteHubEntry(client->getHubUrl());
	if (existingHub)
	{
		if (existingHub->getConnect())
		{
			FavoriteManager::getInstance()->addFavorite(*existingHub, FavoriteManager::REMOVE);
			addStatus(TSTRING(AUTO_CONNECT_REMOVED));
		}
		else
		{
			FavoriteManager::getInstance()->addFavorite(*existingHub, FavoriteManager::ADD);
			addStatus(TSTRING(AUTO_CONNECT_ADDED));
		}
	}
	createFavHubMenu(existingHub);
}

LRESULT HubFrame::onEditHubProp(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	FavoriteHubEntry* editedHub = FavoriteManager::getInstance()->getFavoriteHubEntry(client->getHubUrl());
	if (editedHub)
	{
		FavHubProperties dlg(editedHub);
		if (dlg.DoModal((HWND)*this) == IDOK)
		{
			//  addStatus( _T("Open Hub Propertyes Dialog: "));
		}
	}
	return 0;
}

LRESULT HubFrame::onCopyHubInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string sCopy;
	
	switch (wID)
	{
		case IDC_COPY_HUBNAME:
			sCopy += client->getHubName();
			break;
		case IDC_COPY_HUBADDRESS:
			sCopy += client->getHubUrl();
			break;
	}
	
	if (!sCopy.empty())
	{
		WinUtil::setClipboard(sCopy);
	}
	return 0;
}

LRESULT HubFrame::onCopyUserInfo(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	// !SMT!-S
	const auto& su = getSelectedUser();
	if (su)
	{
		const auto& id = su->getIdentity();
		const auto& u = su->getUser();
		string sCopy;
		switch (wID)
		{
			case IDC_COPY_NICK:
				sCopy += id.getNick();
				break;
			case IDC_COPY_EXACT_SHARE:
				sCopy += Identity::formatShareBytes(id.getBytesShared());
				break;
			case IDC_COPY_DESCRIPTION:
				sCopy += id.getDescription();
				break;
			case IDC_COPY_APPLICATION:
				sCopy += id.getApplication();
				break;
			case IDC_COPY_TAG:
				sCopy += id.getTag();
				break;
			case IDC_COPY_CID:
				sCopy += id.getCID();
				break;
			case IDC_COPY_EMAIL_ADDRESS:
				sCopy += id.getEmail();
				break;
			case IDC_COPY_IP:
				sCopy += id.getIp();
				break;
			case IDC_COPY_NICK_IP:
			{
				// TODO translate
				sCopy += "User Info:\r\n"
				         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n" +
				         "\tIP: " + Identity::formatIpString(id.getIp());
				break;
			}
			case IDC_COPY_ALL:
			{
				// TODO translate
				sCopy += "User info:\r\n"
				         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n" +
				         "\tNicks: " + Util::toString(ClientManager::getNicks(u->getCID(), Util::emptyString)) + "\r\n" +
				         "\t" + STRING(HUBS) + ": " + Util::toString(ClientManager::getHubs(u->getCID(), Util::emptyString)) + "\r\n" +
				         "\t" + STRING(SHARED) + ": " + Identity::formatShareBytes(u->getBytesShared()) + (u->isNMDC() ? Util::emptyString : "(" + STRING(SHARED_FILES) + ": " + Util::toString(id.getSharedFiles()) + ")") + "\r\n" +
				         "\t" + STRING(DESCRIPTION) + ": " + id.getDescription() + "\r\n" +
				         "\t" + STRING(APPLICATION) + ": " + id.getApplication() + "\r\n";
				const auto con = Identity::formatSpeedLimit(id.getDownloadSpeed());
				if (!con.empty())
				{
					sCopy += "\t";
					sCopy += (u->isNMDC() ? STRING(CONNECTION) : "Download speed");
					sCopy += ": " + con + "\r\n";
				}
				const auto lim = Identity::formatSpeedLimit(u->getLimit());
				if (!lim.empty())
				{
					sCopy += "\tUpload limit: " + lim + "\r\n";
				}
				sCopy += "\tE-Mail: " + id.getEmail() + "\r\n" +
				         "\tClient Type: " + Util::toString(id.getClientType()) + "\r\n" +
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
				         "\tPk String: " + id.getStringParam("PK") + "\r\n" +
				         "\tLock: " + id.getStringParam("LO") + "\r\n" +
#endif
				         "\tMode: " + (id.isTcpActive(client) ? 'A' : 'P') + "\r\n" +
				         "\t" + STRING(SLOTS) + ": " + Util::toString(u->getSlots()) + "\r\n" +
				         "\tIP: " + Identity::formatIpString(id.getIp()) + "\r\n";
				const auto su = id.getSupports();
				if (!su.empty())
				{
					sCopy += "\tKnown supports: " + su;
				}
				break;
			}
			default:
				// TODO sCopy += ui->getText(wID - IDC_COPY);
				//break;
				dcdebug("HUBFRAME DON'T GO HERE\n");
				return 0;
		}
		if (!sCopy.empty())
		{
			WinUtil::setClipboard(sCopy);
		}
	}
	return 0;
}

LRESULT HubFrame::onDoubleClickUsers(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMITEMACTIVATE* item = (NMITEMACTIVATE*)pnmh;
	if (item && item->iItem != -1
	        /* [-] FlylinkDC allow menu in My entry: && (ctrlUsers.getItemData(item->iItem)->getUser() != ClientManager::getInstance()->getMe())*/
	   )
	{
		if (UserInfo* l_ui = ctrlUsers.getItemData(item->iItem))
			switch (SETTING(USERLIST_DBLCLICK))
			{
				case 0:
					l_ui->getList(); // [35] https://www.box.net/shared/b210d30d5247da9035f1
					break;
				case 1:
				{
					m_lastUserName = Text::toT(l_ui->getNick()); // SSA_SAVE_LAST_NICK_MACROS
					appendNickToChat(m_lastUserName);
					break;
				}
				case 2:
					l_ui->pm(getHubHint());
					break;
				case 3:
					l_ui->matchQueue();
					break;
				case 4:
					l_ui->grantSlotPeriod(getHubHint(), 600);
					break;
				case 5:
					l_ui->addFav();
					break;
				case 6:
					l_ui->browseList();
					break;
			}
	}
	return 0;
}

bool HubFrame::updateUser(const OnlineUserPtr& p_ou)
{
	if (ClientManager::isShutdown())
	{
		dcassert(!ClientManager::isShutdown());
		return false;
	}
	UserInfo* ui = findUser(p_ou); // TODO - часто ищем. связать в список?
	if (!ui)
	{
#ifdef IRAINMAN_USE_HIDDEN_USERS
		if (!p_ou->isHidden())
		{
#endif
			PROFILE_THREAD_SCOPED_DESC("HubFrame::updateUser-NEW_USER")
			ui = new UserInfo(p_ou);
			m_userMap.insert(make_pair(p_ou, ui));
			if (m_showUsers)// [+] IRainman optimization
			{
				//dcassert(!client->is_all_my_info_loaded());
				if (client->is_all_my_info_loaded())
				{
					m_needsResort |= ui->is_update(ctrlUsers.getSortColumn()); // [+] IRainman fix.
				}
				InsertUserList(ui); // [!] IRainman fix.
			}
			return true;
#ifdef IRAINMAN_USE_HIDDEN_USERS
		}
		else
		{
			return false;
		}
#endif
	}
	else
	{
#ifdef IRAINMAN_USE_HIDDEN_USERS
		if (ui->isHidden())
		{
			if (m_showUsers)
			{
				ctrlUsers.deleteItem(ui);
			}
			m_userMap.erase(ui->getOnlineUser());
			delete ui;
			return true;
		}
		else
		{
#endif // IRAINMAN_USE_HIDDEN_USERS
			if (m_showUsers)// [+] IRainman opt.
			{
				//dcassert(!client->is_all_my_info_loaded());
				// [!] TODO if (client->is_all_my_info_loaded()) // TODO нельзя тут отключать иначе глючит обновления своего ника если хаб PtoX у самого себя шара = 0
				{
					PROFILE_THREAD_SCOPED_DESC("HubFrame::updateUser-update")
					m_needsResort |= ui->is_update(ctrlUsers.getSortColumn());
					const int pos = ctrlUsers.findItem(ui); // TODO 2012-04-18_11-17-28_X543EFD4NB3G3HWBA6SW4KHRQKUBPK5V5S7ZMGY_803AF4F1_crash-stack-r502-beta18-build-9768.dmp
					if (pos != -1)
					{
					
						const int l_top_index      = ctrlUsers.GetTopIndex();
						if (pos >= l_top_index && pos <= l_top_index + ctrlUsers.GetCountPerPage()) // TODO ctrlUsers.GetCountPerPage() закешировать?
						{
#if 0
							const int l_item_count = ctrlUsers.GetItemCount();
							
							LogManager::getInstance()->message("[!!!!!!!!!!!] bool HubFrame::updateUser! ui->getUser()->getLastNick() = " + ui->getUser()->getLastNick()
							                                   + " top/count_per_page/all_count = " +
							                                   Util::toString(l_top_index) + "/" +
							                                   Util::toString(ctrlUsers.GetCountPerPage()) + "/" +
							                                   Util::toString(l_item_count) + "  pos =" + Util::toString(pos)
							                                  );
#endif
							ctrlUsers.updateItem(pos); // TODO - проверить что позиция видна?
							// ctrlUsers.SetItem(pos, 0, LVIF_IMAGE, NULL, I_IMAGECALLBACK, 0, 0, NULL); // TODO это нужно?
						}
#if 0
						else
						{
						
							LogManager::getInstance()->message("[///////] bool HubFrame::updateUser! ui->getUser()->getLastNick() = " + ui->getUser()->getLastNick()
							                                   + " ! pos >= l_top_index && pos <= l_top_index + l_count_per_page pos = " + Util::toString(pos));
						}
#endif
					}
				}
			}
			return false;
#ifdef IRAINMAN_USE_HIDDEN_USERS
		}
#endif
	}
}

void HubFrame::removeUser(const OnlineUserPtr& p_ou)
{
	UserInfo* ui = findUser(p_ou);
	if (!ui)
		return;
		
#ifdef IRAINMAN_USE_HIDDEN_USERS
	dcassert(ui->isHidden() == false);
#endif
	if (m_showUsers)
	{
		ctrlUsers.deleteItem(ui);  // Lock - redraw при закрытии?
	}
	m_userMap.erase(p_ou);
	delete ui;
}

void HubFrame::addStatus(const tstring& aLine, const bool bInChat /*= true*/, const bool bHistory /*= true*/, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextSystem*/)
{
	BaseChatFrame::addStatus(aLine, bInChat, bHistory, cf);
	if (BOOLSETTING(LOG_STATUS_MESSAGES))
	{
		StringMap params;
		
		client->getHubIdentity().getParams(params, "hub", false);
		params["hubURL"] = client->getHubUrl();
		client->getMyIdentity().getParams(params, "my", true);
		
		params["message"] = Text::fromT(aLine);
		LOG(STATUS, params);
	}
}
LRESULT HubFrame::OnSpeakerRange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled */)
{
	switch (uMsg)
	{
		case WM_SPEAKER_UPDATE_USER:
		{
			unique_ptr<OnlineUserPtr> l_ou(reinterpret_cast<OnlineUserPtr*>(wParam));
			m_needsUpdateStats |= updateUser(*l_ou);
		}
		break;
#ifdef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
		case WM_SPEAKER_UPDATE_USER_JOIN:
		{
			unique_ptr<OnlineUserPtr> l_ou(reinterpret_cast<OnlineUserPtr*>(wParam));
			if (!ClientManager::isShutdown())
			{
				if (updateUser(*l_ou))
				{
					const Identity& id    = (*l_ou)->getIdentity();
					const UserPtr& user   = (*l_ou)->getUser();
					const bool isFavorite = !FavoriteManager::getInstance()->isNoFavUserOrUserBanUpload(user); // [!] TODO: в ядро!
					if (isFavorite)
					{
						PLAY_SOUND(SOUND_FAVUSER);
						SHOW_POPUP(POPUP_FAVORITE_CONNECTED, id.getNickT() + _T(" - ") + Text::toT(client->getHubName()), TSTRING(FAVUSER_ONLINE));
					}
					
					if (!(id.isBot() || id.isHub())) // [+] IRainman fix: no show has come/gone for bots, and a hub.
					{
						if (m_showJoins || (m_favShowJoins && isFavorite))
						{
							BaseChatFrame::addLine(_T("*** ") + TSTRING(JOINS) + _T(' ') + id.getNickT(), Colors::g_ChatTextSystem);
						}
					}
					m_needsUpdateStats = true; // [+] IRainman fix.
				}
			}
		}
		break;
#endif // FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
#ifdef FLYLINKDC_REMOVE_USER_WIN_MESSAGES_Q
		case WM_SPEAKER_REMOVE_USER:
		{
			unique_ptr<OnlineUserPtr> l_ou(reinterpret_cast<OnlineUserPtr*>(wParam));
			dcassert(!ClientManager::isShutdown());
			if (!ClientManager::isShutdown())
			{
				const UserPtr& user = (*l_ou)->getUser();
				const Identity& id = (*l_ou)->getIdentity();
				
				if (!(id.isBot() || id.isHub())) // [+] IRainman fix: no show has come/gone for bots, and a hub.
				{
					// !SMT!-S !SMT!-UI
					const bool isFavorite = !FavoriteManager::getInstance()->isNoFavUserOrUserBanUpload(user); // [!] TODO: в ядро!
					
					const tstring& l_userNick = id.getNickT();
					if (isFavorite)
					{
						PLAY_SOUND(SOUND_FAVUSER_OFFLINE);
						SHOW_POPUP(POPUP_FAVORITE_DISCONNECTED, l_userNick + _T(" - ") + Text::toT(client->getHubName()), TSTRING(FAVUSER_OFFLINE));
					}
					
					if (m_showJoins || (m_favShowJoins && isFavorite))
					{
						BaseChatFrame::addLine(_T("*** ") + TSTRING(PARTS) + _T(' ') + l_userNick, Colors::g_ChatTextSystem); // !SMT!-fix
					}
				}
			}
			removeUser(*l_ou);
			m_needsUpdateStats = true; // [+] IRainman fix.
		}
#endif
		break;
		
		case WM_SPEAKER_ADD_CHAT_LINE:
		{
			dcassert(!ClientManager::isShutdown());
			// !SMT!-S
			unique_ptr<ChatMessage> msg(reinterpret_cast<ChatMessage*>(wParam));
			const Identity& from    = msg->from->getIdentity();
			const bool myMess       = ClientManager::isMe(msg->from);
			// [-] if (msg.m_fromId.isOp() && !client->isOp()) !SMT!-S
			{
				addLine(from, myMess, msg->thirdPerson, Text::toT(msg->format()), Colors::g_ChatTextGeneral);
			}
		}
		break;
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		case WM_SPEAKER_CONNECTED:
		{
			dcassert(!ClientManager::isShutdown());
			addStatus(TSTRING(CONNECTED), true, true, Colors::g_ChatTextServer);
			setTabColor(RGB(0, 255, 0));
			unsetIconState();
			
			ctrlClient.setHubParam(client->getHubUrl(), client->getMyNick()); // [+] IRainman fix.
			
			if (m_ctrlStatus)
			{
				setStatusText(1, Text::toT(client->getCipherName()));
				UpdateLayout(false);
			}
			SHOW_POPUP(POPUP_HUB_CONNECTED, Text::toT(client->getAddress()), TSTRING(CONNECTED));
			PLAY_SOUND(SOUND_HUBCON);
#ifdef SCALOLAZ_HUB_MODE
			HubModeChange();
#endif
			m_needsUpdateStats = true;
		}
		break;
		case WM_SPEAKER_DISCONNECTED:
		{
			dcassert(!ClientManager::isShutdown());
			clearUserList();
			setTabColor(RGB(255, 0, 0));
			setIconState();
			PLAY_SOUND(SOUND_HUBDISCON);
			SHOW_POPUP(POPUP_HUB_DISCONNECTED, Text::toT(client->getAddress()), TSTRING(DISCONNECTED));
#ifdef SCALOLAZ_HUB_MODE
			HubModeChange();
#endif
			m_needsUpdateStats = true;
		}
		break;
#ifdef FLYLINKDC_PRIVATE_MESSAGE_USE_WIN_MESSAGES_Q
		case WM_SPEAKER_PRIVATE_MESSAGE:
		{
			dcassert(!ClientManager::isShutdown());
			unique_ptr<ChatMessage> pm(reinterpret_cast<ChatMessage*>(wParam));
			// [-] if (pm.m_fromId.isOp() && !client->isOp()) !SMT!-S
			{
				const Identity& from = pm->from->getIdentity();
				const bool myPM = ClientManager::isMe(pm->replyTo);
				const Identity& replyTo = pm->replyTo->getIdentity();
				const Identity& to = pm->to->getIdentity();
				const tstring text = Text::toT(pm->format());
				const auto& id = myPM ? to : replyTo;
				const bool isOpen = PrivateFrame::isOpen(id.getUser());
				if (replyTo.isHub())
				{
					if (BOOLSETTING(POPUP_HUB_PMS) || isOpen)
					{
						PrivateFrame::gotMessage(from, to, replyTo, text, getHubHint(), myPM, pm->thirdPerson); // !SMT!-S
					}
					else
					{
						BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + id.getNickT() + _T(": ") + text, Colors::g_ChatTextPrivate);
					}
				}
				else if (replyTo.isBot())
				{
					if (BOOLSETTING(POPUP_BOT_PMS) || isOpen)
					{
						PrivateFrame::gotMessage(from, to, replyTo, text, getHubHint(), myPM, pm->thirdPerson); // !SMT!-S
					}
					else
					{
						BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + id.getNickT() + _T(": ") + text, Colors::g_ChatTextPrivate);
					}
				}
				else
				{
					if (BOOLSETTING(POPUP_PMS) || isOpen)
					{
						PrivateFrame::gotMessage(from, to, replyTo, text, getHubHint(), myPM, pm->thirdPerson); // !SMT!-S
					}
					else
					{
						BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + id.getNickT() + _T(": ") + text, Colors::g_ChatTextPrivate);
					}
					// !SMT!-S
					HWND hMainWnd = MainFrame::getMainFrame()->m_hWnd;//GetTopLevelWindow();
					::PostMessage(hMainWnd, WM_SPEAKER, MainFrame::SET_PM_TRAY_ICON, NULL); //-V106
				}
			}
		}
		break;
#endif
		case WM_SPEAKER_CHEATING_USER:
		{
			unique_ptr<string> l_ptr_msg(reinterpret_cast<string*>(wParam));
			// TODO - делать один раз в winUtils?
			CHARFORMAT2 cf;
			memzero(&cf, sizeof(CHARFORMAT2));
			cf.cbSize = sizeof(cf);
			cf.dwMask = CFM_BACKCOLOR | CFM_COLOR | CFM_BOLD;
			cf.crBackColor = SETTING(BACKGROUND_COLOR);
			cf.crTextColor = SETTING(ERROR_COLOR);
			const tstring msg = Text::toT(*l_ptr_msg);
			if (msg.length() < 256)
				SHOW_POPUP(POPUP_CHEATING_USER, msg, TSTRING(CHEATING_USER));
			BaseChatFrame::addLine(msg, cf);
		}
		break;
		case WM_SPEAKER_USER_REPORT:
		{
			unique_ptr<string> l_ptr_msg(reinterpret_cast<string*>(wParam));
			BaseChatFrame::addLine(Text::toT(*l_ptr_msg), Colors::g_ChatTextSystem);
		}
		break;
		
		
		default:
		{
			dcassert(0);
			break;
		}
	}
	return 0;
}
LRESULT HubFrame::onSpeaker(UINT /*uMsg*/, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /*bHandled*/)
{
//	dcassert(g_isStartupProcess == false && !ClientManager::isShutdown());
	//PROFILE_THREAD_SCOPED()
	TaskQueue::List t;
	m_tasks.get(t);
	if (t.empty())
		return 0;
#ifdef _DEBUG
	//LogManager::getInstance()->message("LRESULT HubFrame::onSpeaker: m_tasks.size() = " + Util::toString(t.size()));
#endif
	CFlyBusy l_busy(m_spoken);
	CLockRedraw<> l_lock_draw(ctrlUsers);
	
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		switch (i->first)
		{
#ifndef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
			case UPDATE_USER_JOIN:
			{
				if (!ClientManager::isShutdown())
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					if (updateUser(u.m_ou))
					{
						const Identity& id = u.m_ou->getIdentity();
						const UserPtr& user = u.m_ou->getUser();
						const bool isFavorite = !FavoriteManager::getInstance()->isNoFavUserOrUserBanUpload(user); // [!] TODO: в ядро!
						if (isFavorite)
						{
							PLAY_SOUND(SOUND_FAVUSER);
							SHOW_POPUP(POPUP_FAVORITE_CONNECTED, id.getNickT() + _T(" - ") + Text::toT(client->getHubName()), TSTRING(FAVUSER_ONLINE));
						}
						
						if (!(id.isBot() || id.isHub())) // [+] IRainman fix: no show has come/gone for bots, and a hub.
						{
							if (m_showJoins || (m_favShowJoins && isFavorite))
							{
								BaseChatFrame::addLine(_T("*** ") + TSTRING(JOINS) + _T(' ') + id.getNickT(), Colors::g_ChatTextSystem);
							}
						}
						m_needsUpdateStats = true; // [+] IRainman fix.
					}
				}
			}
			break;
			
#endif // FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
#ifndef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
			case REMOVE_USER:
			{
				const OnlineUserTask& u = static_cast<const OnlineUserTask&>(*i->second);
				dcassert(!ClientManager::isShutdown());
				if (!ClientManager::isShutdown())
				{
					const UserPtr& user = u.m_ou->getUser();
					const Identity& id  = u.m_ou->getIdentity();
					
					if (!(id.isBot() || id.isHub())) // [+] IRainman fix: no show has come/gone for bots, and a hub.
					{
						// !SMT!-S !SMT!-UI
						const bool isFavorite = !FavoriteManager::getInstance()->isNoFavUserOrUserBanUpload(user); // [!] TODO: в ядро!
						
						const tstring& l_userNick = id.getNickT();
						if (isFavorite)
						{
							PLAY_SOUND(SOUND_FAVUSER_OFFLINE);
							SHOW_POPUP(POPUP_FAVORITE_DISCONNECTED, l_userNick + _T(" - ") + Text::toT(client->getHubName()), TSTRING(FAVUSER_OFFLINE));
						}
						
						if (m_showJoins || (m_favShowJoins && isFavorite))
						{
							BaseChatFrame::addLine(_T("*** ") + TSTRING(PARTS) + _T(' ') + l_userNick, Colors::g_ChatTextSystem); // !SMT!-fix
						}
					}
				}
				removeUser(u.m_ou);
				m_needsUpdateStats = true; // [+] IRainman fix.
			}
			break;
#endif // FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
			case ADD_STATUS_LINE:
			{
				dcassert(!ClientManager::isShutdown());
//				PROFILE_THREAD_SCOPED_DESC("ADD_STATUS_LINE")
				const StatusTask& status = static_cast<StatusTask&>(*i->second);
				addStatus(Text::toT(status.m_str), status.m_isInChat, true, Colors::g_ChatTextServer);
			}
			break;
			case STATS:
			{
				dcassert(!ClientManager::isShutdown());
				if (m_ctrlStatus)
				{
					if (client->is_all_my_info_loaded() == true)
					{
//				PROFILE_THREAD_SCOPED_DESC("STATS")
						const int64_t l_availableBytes = client->getAvailableBytes();
						const size_t l_allUsers = client->getUserCount();
						const size_t l_shownUsers = ctrlUsers.GetItemCount();
						const size_t l_diff = l_allUsers - l_shownUsers;
						setStatusText(2, (Util::toStringW(l_shownUsers) + (l_diff ? (_T('/') + Util::toStringW(l_allUsers)) : Util::emptyStringT) + _T(' ') + TSTRING(HUB_USERS)));
						setStatusText(3, Util::formatBytesW(l_availableBytes));
						setStatusText(4, l_allUsers ? (Util::formatBytesW(l_availableBytes / l_allUsers) + _T('/') + TSTRING(USER)) : Util::emptyStringT);
						if (m_needsResort)
						{
							m_needsResort = false;
							ctrlUsers.resort(); // убран ресорт если окно не активное!
						}
					}
				}
			}
			break;
			case GET_PASSWORD:
			{
				dcassert(m_ctrlMessage);
				if (!BOOLSETTING(PROMPT_PASSWORD))
				{
					addPasswordCommand();
					m_waitingForPW = true;
				}
				else if (m_password_do_modal == 0)
				{
					CFlySafeGuard<uint8_t> l_dlg_(m_password_do_modal); // fix Stack Overflow https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=103355
					LineDlg linePwd;
					linePwd.title = Text::toT(client->getHubName() + " (" + client->getHubUrl() + ')');
					linePwd.description = TSTRING(ENTER_PASSWORD);
					linePwd.password = true;
					if (linePwd.DoModal(m_hWnd) == IDOK)
					{
						const string l_pwd = Text::fromT(linePwd.line);
						client->setPassword(l_pwd);
						client->password(l_pwd);
						m_waitingForPW = false;
						if (linePwd.checked)
						{
							auto fhe = addAsFavorite(); // [+] IRainman fav options
							if (fhe) // https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=39084
							{
								fhe->setPassword(l_pwd);
							}
						}
					}
					else
					{
						client->disconnect(true);
					}
				}
			}
			break;
#ifndef FLYLINKDC_PRIVATE_MESSAGE_USE_WIN_MESSAGES_Q
			case PRIVATE_MESSAGE:
			{
				dcassert(!ClientManager::isShutdown());
				MessageTask& l_task = static_cast<MessageTask&>(*i->second);
				auto_ptr<ChatMessage> pm(l_task.m_message_ptr);
				l_task.m_message_ptr = nullptr;
				// [-] if (pm.m_fromId.isOp() && !client->isOp()) !SMT!-S
				{
					const Identity& from = pm->from->getIdentity();
					const bool myPM = ClientManager::isMe(pm->replyTo);
					const Identity& replyTo = pm->replyTo->getIdentity();
					const Identity& to = pm->to->getIdentity();
					const tstring text = Text::toT(pm->format());
					const auto& id = myPM ? to : replyTo;
					const bool isOpen = PrivateFrame::isOpen(id.getUser());
					if (replyTo.isHub())
					{
						if (BOOLSETTING(POPUP_HUB_PMS) || isOpen)
						{
							PrivateFrame::gotMessage(from, to, replyTo, text, getHubHint(), myPM, pm->thirdPerson); // !SMT!-S
						}
						else
						{
							BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + id.getNickT() + _T(": ") + text, Colors::g_ChatTextPrivate);
						}
					}
					else if (replyTo.isBot())
					{
						if (BOOLSETTING(POPUP_BOT_PMS) || isOpen)
						{
							PrivateFrame::gotMessage(from, to, replyTo, text, getHubHint(), myPM, pm->thirdPerson); // !SMT!-S
						}
						else
						{
							BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + id.getNickT() + _T(": ") + text, Colors::g_ChatTextPrivate);
						}
					}
					else
					{
						if (BOOLSETTING(POPUP_PMS) || isOpen)
						{
							PrivateFrame::gotMessage(from, to, replyTo, text, getHubHint(), myPM, pm->thirdPerson); // !SMT!-S
						}
						else
						{
							BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + id.getNickT() + _T(": ") + text, Colors::g_ChatTextPrivate);
						}
						// !SMT!-S
						HWND hMainWnd = MainFrame::getMainFrame()->m_hWnd;//GetTopLevelWindow();
						::PostMessage(hMainWnd, WM_SPEAKER, MainFrame::SET_PM_TRAY_ICON, NULL); //-V106
					}
				}
			}
			break;
#endif
#ifdef RIP_USE_CONNECTION_AUTODETECT
			case DIRECT_MODE_DETECTED:
			{
				//BaseChatFrame::addLine(_T("[!]FlylinkDC++ ") + _T("Detected direct connection type, switching to active mode"), WinUtil::m_ChatTextSystem);
				BaseChatFrame::addLine(Text::toT(static_cast<StatusTask&>(*i->second).str) + _T(": ") + _T("Detected direct connection type, switching to active mode"), Colors::g_ChatTextSystem);
			}
			break;
#endif
			default:
				dcassert(0);
				break;
		};
		delete i->second;
	}
	
	return 0;
}

void HubFrame::updateWindowText()
{
	if (BaseChatFrame::g_isStartupProcess == false) // Пока конструируемся не нужно апдейтить текст
	{
		if (m_is_window_text_update)
		{
			// TODO - ограничить размер текста
			SetWindowText(Text::toT(m_window_text).c_str());
			m_is_window_text_update = 0;
		}
	}
}

void HubFrame::setWindowTitle(const string& p_text)
{
	dcassert(!ClientManager::isShutdown());
	if (m_window_text != p_text)
	{
		m_window_text = p_text;
		++m_is_window_text_update;
		SetMDIFrameMenu(); // fix http://code.google.com/p/flylinkdc/issues/detail?id=1386
	}
	updateWindowText();
}

void HubFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (g_isStartupProcess)
	{
		return;
	}
	if (ClientManager::isShutdown())
	{
		dcassert(!ClientManager::isShutdown());
		return;
	}
	if (m_tooltip_hubframe)
	{
		m_tooltip_hubframe->Activate(FALSE);
	}
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	if (m_ctrlStatus)
	{
		if (m_ctrlStatus->IsWindow() && m_ctrlLastLinesToolTip->IsWindow())
		{
			CRect sr;
			m_ctrlStatus->GetClientRect(sr);
			int tmp = sr.Width() > 332 ? 232 : (sr.Width() > 132 ? sr.Width() - 100 : 32);
			int szCipherLen = m_ctrlStatus->GetTextLength(1);
			
			if (szCipherLen)
			{
				wstring strCipher;
				strCipher.resize(static_cast<size_t>(szCipherLen));
				m_ctrlStatus->GetText(1, &strCipher.at(0));
				szCipherLen = WinUtil::getTextWidth(strCipher, m_ctrlStatus->m_hWnd);
			}
			int HubPic = 0;
#ifdef SCALOLAZ_HUB_MODE
			if (BOOLSETTING(ENABLE_HUBMODE_PIC))
			{
				HubPic += 22;
			}
#endif
#ifdef SCALOLAZ_HUB_SWITCH_BTN
			if (m_showUsers)
			{
				HubPic += 20;
			}
#endif
			int w[6];
			w[0] = sr.right - tmp - 55 - HubPic - szCipherLen;
			w[1] = w[0] + szCipherLen;
			w[2] = w[1] + (tmp - 30) / 2;
			w[3] = w[1] + (tmp - 64); // [~] InfinitySky
			w[4] = w[3] + 100;
			w[5] = w[4] + 18 + HubPic; // [~] InfinitySky
			m_ctrlStatus->SetParts(6, w);
			
			m_ctrlLastLinesToolTip->SetMaxTipWidth(max(w[0], 400));
			
			// Strange, can't get the correct width of the last field...
			m_ctrlStatus->GetRect(4, sr);
			
#ifdef SCALOLAZ_HUB_MODE
			// Icon hub Mode : Active, Passive, Offline
			if (m_ctrlShowMode)
			{
				if (BOOLSETTING(ENABLE_HUBMODE_PIC))
				{
					sr.left = sr.right + 2;
					sr.right = sr.left + 20;
					m_ctrlShowMode->MoveWindow(sr);
				}
				else
					m_ctrlShowMode->MoveWindow(0, 0, 0, 0);
			}
#endif
			
#ifdef SCALOLAZ_HUB_SWITCH_BTN
			// Icon-button Switch panels. Analog for command /switch
			if (m_ctrlSwitchPanels)
			{
				if (m_showUsers)
				{
					sr.left = sr.right; // + 2;
					sr.right = sr.left + 20;
					m_ctrlSwitchPanels->MoveWindow(sr);
				}
				else
				{
					m_ctrlSwitchPanels->MoveWindow(0, 0, 0, 0);
				}
			}
#endif
			
			// Checkbox Show/Hide userlist
			sr.left = sr.right + 2;
			sr.right = sr.left + 16;
			if (m_ctrlShowUsers)
			{
				m_ctrlShowUsers->MoveWindow(sr);
			}
		}   // end  if (m_ctrlStatus->IsWindow()...
	}
	/*[+] Это условие для тех кто любит большие шрифты,
	будет включаться многострочный ввод принудительно,
	чтоб не портить расположение элементов Sergey Shushkanov */
	const bool bUseMultiChat = BOOLSETTING(MULTILINE_CHAT_INPUT) || m_bUseTempMultiChat
#ifdef MULTILINE_CHAT_IF_BIG_FONT_SET
	                           || Fonts::fontHeight > FONT_SIZE_FOR_AUTO_MULTILINE_CHAT //[+] TEST VERSION Sergey Shushkanov
#endif
	                           ;
	                           
	const int h = bUseMultiChat ? 20 : 14;//[+] TEST VERSION Sergey Shushkanov
	const int chat_columns = bUseMultiChat ? 2 : 1; // !Decker! // [~] Sergey Shushkanov
	
	CRect rc = rect;
	//if(m_ctrlStatus) // Если фрейм активный - то поднимаем чат немного.
	rc.bottom -= h * chat_columns + 15;  // !Decker! //[~] Sergey Shushkanov
	if (m_ctrlStatus)
	{
		TuneSplitterPanes();
		if (!m_showUsers) // Если список пользователей не отображается.
		{
			if (GetSinglePaneMode() == SPLIT_PANE_NONE) // Если никакая сторона не скрыта.
			{
#ifdef SCALOLAZ_HUB_SWITCH_BTN
				SetSinglePaneMode((m_isClientUsersSwitch == true) ? SPLIT_PANE_LEFT : SPLIT_PANE_RIGHT);
#else
				SetSinglePaneMode(SPLIT_PANE_LEFT);
#endif
			}
		}
		else // Если список пользователей отображается.
		{
			if (GetSinglePaneMode() != SPLIT_PANE_NONE) // Если какая-то сторона скрыта.
				SetSinglePaneMode(SPLIT_PANE_NONE); // Никакая сторона не скрыта.
		}
		SetSplitterRect(rc);
	}
	
	int iButtonPanelLength = MessagePanel::GetPanelWidth();
	const int l_panelWidth = iButtonPanelLength;
	if (m_msgPanel)
	{
		if (!bUseMultiChat) // Only for Single Line chat
		{
			iButtonPanelLength += m_showUsers ?  222 : 0;
		}
		else
		{
			if (m_showUsers && iButtonPanelLength < 222)
			{
				iButtonPanelLength  = 222;
			}
		}
	}
	rc = rect;
	rc.bottom -= 4; //[~] Sergey Shushkanov
	rc.top = rc.bottom - h * chat_columns - 7; // !Decker!
	rc.left += 2; //[~] Sergey Shushkanov
	rc.right -= iButtonPanelLength + 2; //[~] Sergey Shushkanov
	CRect ctrlMessageRect = rc;
	if (m_ctrlMessage)
		m_ctrlMessage->MoveWindow(rc);
		
	if (bUseMultiChat) //[+] TEST VERSION Sergey Shushkanov
	{
		rc.top += h + 6; //[+] TEST VERSION Sergey Shushkanov
	}//[+] TEST VERSION Sergey Shushkanov
	//rc.top += h * (chat_columns - 1);  !Decker! // [-] Sergey Shushkanov
	rc.left = rc.right; // [~] Sergey Shushkanov
	rc.bottom -= 1; // [+] Sergey Shushkanov
	
	if (m_msgPanel)
		m_msgPanel->UpdatePanel(rc);
		
	rc.right  += l_panelWidth;
	rc.bottom += 1;
	
	if (m_ctrlFilter && m_ctrlFilterSel)
	{
		if (m_showUsers)
		{
			if (bUseMultiChat)
			{
				rc = ctrlMessageRect;
				rc.bottom = rc.top + h + 3; // [~] JhaoDa
				rc.left = rc.right + 2; // [~] Sergey Shushkanov
				rc.right += 119; // [~] Sergey Shushkanov
				m_ctrlFilter->MoveWindow(rc);
				
				rc.left = rc.right + 2; // [~] Sergey Shushkanov
				rc.right = rc.left + 101; // [~] Sergey Shushkanov
				rc.top = rc.top + 1; // [~] JhaoDa
				m_ctrlFilterSel->MoveWindow(rc);
			}
			else
			{
				rc.left = rc.right + 5; // [~] Sergey Shushkanov
				rc.right = rc.left + 116; // [~] Sergey Shushkanov
				m_ctrlFilter->MoveWindow(rc);
				
				rc.left = rc.right + 2; // [~] Sergey Shushkanov
				rc.right = rc.left + 99; // [~] Sergey Shushkanov
				m_ctrlFilterSel->MoveWindow(rc);
			}
		}
		else
		{
			rc.left = 0;
			rc.right = 0;
			m_ctrlFilter->MoveWindow(rc);
			m_ctrlFilterSel->MoveWindow(rc);
		}
	}
	if (m_tooltip_hubframe && !BOOLSETTING(POPUPS_DISABLED))
	{
		m_tooltip_hubframe->Activate(TRUE);
	}
}
void HubFrame::TuneSplitterPanes()
{
#ifdef SCALOLAZ_HUB_SWITCH_BTN
	if (m_isClientUsersSwitch == true)
	{
		SetSplitterPanes(ctrlClient.m_hWnd, ctrlUsers.m_hWnd, false); // Чат, список пользователей.
	}
	else // Если список пользователей слева.
	{
		SetSplitterPanes(ctrlUsers.m_hWnd, ctrlClient.m_hWnd, false); // Список пользователей, чат.
	}
#else
	SetSplitterPanes(ctrlClient.m_hWnd, ctrlUsers.m_hWnd, false);
#endif
}
#ifdef SCALOLAZ_HUB_MODE
void HubFrame::HubModeChange()
{
	if (BOOLSETTING(ENABLE_HUBMODE_PIC))
	{
		if (client->isConnected())
		{
			if (client->isActive())
			{
				if (m_ctrlShowMode)
				{
					m_ctrlShowMode->SetIcon(g_hModeActiveIco);
					if (m_tooltip_hubframe)
					{
						m_tooltip_hubframe->AddTool(*m_ctrlShowMode, ResourceManager::ACTIVE_NOTICE);
					}
				}
			}
			else
			{
				if (m_ctrlShowMode)
				{
					m_ctrlShowMode->SetIcon(g_hModePassiveIco);
					if (m_tooltip_hubframe)
					{
						m_tooltip_hubframe->AddTool(*m_ctrlShowMode, ResourceManager::PASSIVE_NOTICE);
					}
				}
			}
		}
		else
		{
			if (m_ctrlShowMode)
			{
				m_ctrlShowMode->SetIcon(g_hModeNoneIco);
				if (m_tooltip_hubframe)
				{
					m_tooltip_hubframe->AddTool(*m_ctrlShowMode, ResourceManager::UNKNOWN_MODE_NOTICE);
				}
			}
		}
	}
}
#endif // SCALOLAZ_HUB_MODE
void HubFrame::storeColumsInfo()
{

	string l_order, l_width, l_visible;
	if (m_isUpdateColumnsInfoProcessed)
	{
		ctrlUsers.saveHeaderOrder(l_order, l_width, l_visible);
	}
	FavoriteHubEntry *fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(Text::fromT(server));
	if (fhe)
	{
		if (m_isUpdateColumnsInfoProcessed)
		{
			WINDOWPLACEMENT wp = {0};
			wp.length = sizeof(wp);
			GetWindowPlacement(&wp);
			CRect rc;
			GetWindowRect(rc);
			CRect rcmdiClient;
			::GetWindowRect(WinUtil::mdiClient, &rcmdiClient);
			if (wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWNORMAL)
			{
				fhe->setWindowPosX(rc.left - (rcmdiClient.left + 2));
				fhe->setWindowPosY(rc.top - (rcmdiClient.top + 2));
				fhe->setWindowSizeX(rc.Width());
				fhe->setWindowSizeY(rc.Height());
			}
			if (wp.showCmd == SW_SHOWNORMAL || wp.showCmd == SW_SHOW || wp.showCmd == SW_SHOWMAXIMIZED || wp.showCmd == SW_MAXIMIZE)
			{
				fhe->setWindowType((int)wp.showCmd);
			}
			else
			{
				fhe->setWindowType(SW_SHOWMAXIMIZED);
			}
			fhe->setChatUserSplit(m_nProportionalPos);
#ifdef SCALOLAZ_HUB_SWITCH_BTN
			fhe->setChatUserSplitState(m_isClientUsersSwitch);
#endif
			fhe->setUserListState(m_showUsersStore);
			fhe->setHeaderOrder(l_order);
			fhe->setHeaderWidths(l_width);
			fhe->setHeaderVisible(l_visible);
			fhe->setHeaderSort(ctrlUsers.getSortColumn());
			fhe->setHeaderSortAsc(ctrlUsers.isAscending());
		}
		if (g_frames.size() == 1 || BaseChatFrame::g_isStartupProcess == false) // Сохраняем только на последней итерации или когда не закрываем приложение.
		{
			FavoriteManager::getInstance()->save();
		}
	}
	else
	{
		if (m_isUpdateColumnsInfoProcessed)
		{
			SET_SETTING(HUBFRAME_ORDER, l_order);
			SET_SETTING(HUBFRAME_WIDTHS, l_width);
			SET_SETTING(HUBFRAME_VISIBLE, l_visible);
			SET_SETTING(HUBFRAME_COLUMNS_SORT, ctrlUsers.getSortColumn());
			SET_SETTING(HUBFRAME_COLUMNS_SORT_ASC, ctrlUsers.isAscending());
		}
	}
}
LRESULT HubFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	const string l_server = Text::fromT(server);
	if (!m_closed)
	{
		m_closed = true;
		safe_destroy_timer();
		storeColumsInfo();
		RecentHubEntry* r = FavoriteManager::getInstance()->getRecentHubEntry(l_server);
		if (r) // hub has been removed by the user from a list of recent hubs at a time when it was opened. https://crash-server.com/Bug.aspx?ClientID=ppa&ProblemID=9897
		{
			LocalArray<TCHAR, 256> buf;
			GetWindowText(buf.data(), 255);
			r->setName(Text::fromT(buf.data()));
			r->setUsers(Util::toString(client->getUserCount()));
			r->setShared(Util::toString(client->getAvailableBytes()));
			r->setDateTime(Util::formatDigitalClock(time(NULL)));
			FavoriteManager::getInstance()->updateRecent(r);
		}
// TODO     ClientManager::getInstance()->addListener(this);
#ifdef RIP_USE_CONNECTION_AUTODETECT
		ConnectionManager::getInstance()->removeListener(this);
#endif
		if (m_isUpdateColumnsInfoProcessed)
		{
			SettingsManager::getInstance()->removeListener(this);
		}
		client->removeListener(this);
		client->disconnect(true);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		clearTaskList();
		clearUserList();
		bHandled = FALSE;
		return 0;
	}
}
/* [-] IRainman fix.
const string& HubFrame::getHubHint() const
{
    Client* l_client = getClient();
    return l_client ? l_client->getHubUrl() : Util::emptyString; //[24] https://www.box.net/shared/7c1959f6e1cff66ef72c  https://www.box.net/shared/15b9c3ed5b769d5f6a2d
    // TODO 2012-04-20_22-34-39_BKO32K2I7VK6XDBLHQBKYIGFEH3Q2AJIM5ZWAZI_A5A1D879_crash-stack-r502-beta20-build-9794.dmp
    // 2012-04-27_18-47-20_GHN3CDZCMJKHX4NER2G23LHYGPQJDYDP63UQNJY_1BF7FE17_crash-stack-r502-beta22-x64-build-9854.dmp
    // 2012-04-29_06-52-32_5HOPCRAULMWYJWGXMPINWI4LXEYMYNG2XVK5W4I_B5CB162B_crash-stack-r502-beta23-build-9860.dmp
    // 2012-05-11_23-57-17_ND2ECRYDTN5DHTZLDWHO2VETYFXMT3D7XRCNY4A_F6599807_crash-stack-r502-beta26-x64-build-9946.dmp
    // 2012-06-09_18-15-11_HPBFJ7PDHH4MSFNZYFIPGNTDPZ45DH3FZMV3FQQ_9009D338_crash-stack-r501-build-10294.dmp
    // 2012-06-17_22-40-29_AZ46YXHC7VYZWC6D56B5JTN254A2TYKOVUJ2OGQ_CF38B0D3_crash-stack-r502-beta36-x64-build-10378.dmp
    // 2012-06-25_22-17-42_OHUFDRKV6T6IU77SALTOCCN7LDYKICYNXLCEGHA_438E1A51_crash-stack-r502-beta39-build-10495.dmp
}
*/
void HubFrame::clearUserList()
{
	{
		CLockRedraw<> l_lock_draw(ctrlUsers); // TODO это нужно или опустить ниже?
		ctrlUsers.DeleteAllItems();
	}
	for (auto i = m_userMap.cbegin(); i != m_userMap.cend(); ++i)
		delete i->second; //[2] https://www.box.net/shared/202f89c842ee60bdecb9
	m_userMap.clear();
}

void HubFrame::clearTaskList()
{
	m_tasks.clear();
}

LRESULT HubFrame::onLButton(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND focus = GetFocus();
	bHandled = false;
	if (focus == ctrlClient.m_hWnd)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		tstring x;
		
		int i = ctrlClient.CharFromPos(pt);
		int line = ctrlClient.LineFromChar(i);
		int c = LOWORD(i) - ctrlClient.LineIndex(line);
		int len = ctrlClient.LineLength(i) + 1;
		if (len < 3)
		{
			return 0;
		}
		
		x.resize(len);
		ctrlClient.GetLine(line, &x[0], len);
		
		string::size_type start = x.find_last_of(_T(" <\t\r\n"), c);
		
		if (start == string::npos)
			start = 0;
		else
			start++;
			
			
		string::size_type end = x.find_first_of(_T(" >\t"), start + 1);
		
		if (end == string::npos) // get EOL as well
			end = x.length();
		else if (end == start + 1)
			return 0;
			
		// Nickname click, let's see if we can find one like it in the name list...
		const tstring nick = x.substr(start, end - start);
		UserInfo* ui = findUser(nick);
		if (ui)
		{
			bHandled = true;
			if (wParam & MK_CONTROL)   // MK_CONTROL = 0x0008
			{
				PrivateFrame::openWindow(ui->getOnlineUser(), HintedUser(ui->getUser(), client->getHubUrl()), client->getMyNick());
			}
			else if (wParam & MK_SHIFT)
			{
				try
				{
					QueueManager::getInstance()->addList(HintedUser(ui->getUser(), getHubHint()), QueueItem::FLAG_CLIENT_VIEW);
				}
				catch (const Exception& e)
				{
					addStatus(Text::toT(e.getError()));
				}
			}
			else
			{
				switch (SETTING(CHAT_DBLCLICK))
				{
					case 0:
					{
						const int l_items_count = ctrlUsers.GetItemCount();
						int pos = -1;
						CLockRedraw<> l_lock_draw(ctrlUsers);
						for (int i = 0; i < l_items_count; ++i)
						{
							if (ctrlUsers.getItemData(i) == ui)
								pos = i;
							ctrlUsers.SetItemState(i, (i == pos) ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
						}
						ctrlUsers.EnsureVisible(pos, FALSE);
						break;
					}
					case 1:
					{
						m_lastUserName = ui->getText(COLUMN_NICK); // SSA_SAVE_LAST_NICK_MACROS
						appendNickToChat(m_lastUserName);
						break;
					}
					case 2:
						ui->pm(getHubHint());
						break;
					case 3:
						ui->getList();
						break;
					case 4:
						ui->matchQueue();
						break;
					case 5:
						ui->grantSlotPeriod(getHubHint(), 600);
						break;
					case 6:
						ui->addFav();
						break;
				}
			}
		}
	}
	return 0;
}

void HubFrame::addLine(const Identity& from, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
{
	tstring extra;
	BaseChatFrame::addLine(from, bMyMess, bThirdPerson, aLine, cf, extra);
	if (g_isStartupProcess == false)
	{
		SHOW_POPUP(POPUP_CHAT_LINE, aLine, TSTRING(CHAT_MESSAGE));
	}
	if (BOOLSETTING(LOG_MAIN_CHAT))
	{
		StringMap params;
		
		params["message"] = ChatMessage::formatNick(from.getNick(), bThirdPerson) + Text::fromT(aLine);
		// TODO crash "<-FTTBkhv-> Running Verlihub 1.0.0 build Fri Mar 30 2012 ][ Runtime: 5 дн. 15 час. ][ User count: 533"
		if (!extra.empty())
			params["extra"] = Text::fromT(extra);
			
		client->getHubIdentity().getParams(params, "hub", false);
		params["hubURL"] = client->getHubUrl();
		client->getMyIdentity().getParams(params, "my", true);
		
		LOG(CHAT, params);
	}
	if (g_isStartupProcess == false && BOOLSETTING(BOLD_HUB))
	{
		if (client->is_all_my_info_loaded())
		{
			setDirty();
		}
	}
}
LRESULT HubFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	CMenu hSysMenu;
	m_tabMenuShown = true;
	OMenu* l_tabMenu = createTabMenu();
	const string& l_name = client->getHubName();
	l_tabMenu->InsertSeparatorFirst(Text::toT(!l_name.empty() ? (l_name.size() > 50 ? l_name.substr(0, 50) + ("…") : l_name) : client->getHubUrl()));
	appendUcMenu(*m_tabMenu, ::UserCommand::CONTEXT_HUB, client->getHubUrl());
	hSysMenu.Attach((wParam == NULL) ? (HMENU)*m_tabMenu : (HMENU)wParam);
	if (wParam != NULL)
	{
		hSysMenu.InsertMenu(hSysMenu.GetMenuItemCount() - 1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)(HMENU)*m_tabMenu, /*CTSTRING(USER_COMMANDS)*/ _T("User Commands"));
		hSysMenu.InsertMenu(hSysMenu.GetMenuItemCount() - 1, MF_BYPOSITION | MF_SEPARATOR);
	}
	hSysMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
	if (wParam != NULL)
	{
		hSysMenu.RemoveMenu(hSysMenu.GetMenuItemCount() - 2, MF_BYPOSITION);
		hSysMenu.RemoveMenu(hSysMenu.GetMenuItemCount() - 2, MF_BYPOSITION);
	}
	cleanUcMenu(*m_tabMenu);
	l_tabMenu->RemoveFirstItem();
	hSysMenu.Detach();
	return TRUE;
}

LRESULT HubFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	CRect rc;            // client area of window
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	m_tabMenuShown = false;
	
	ctrlUsers.GetHeader().GetWindowRect(&rc);
	
	if (PtInRect(&rc, pt) && m_showUsers)
	{
		ctrlUsers.showMenu(pt);
		return TRUE;
	}
	
	if (reinterpret_cast<HWND>(wParam) == ctrlUsers && m_showUsers)
	{
		OMenu* l_user_menu = createUserMenu();
		l_user_menu->ClearMenu();
		clearUserMenu(); // !SMT!-S
		
		if (ctrlUsers.GetSelectedCount() == 1)
		{
			if (pt.x == -1 && pt.y == -1)
			{
				WinUtil::getContextMenuPos(ctrlUsers, pt);
			}
			int i = -1;
			i = ctrlUsers.GetNextItem(i, LVNI_SELECTED);
			if (i >= 0)
			{
				reinitUserMenu(((UserInfo*)ctrlUsers.getItemData(i))->getOnlineUser(), getHubHint()); // !SMT!-S // [!] IRainman fix.
			}
		}
		
		appendHubAndUsersItems(*l_user_menu, false);
		
		appendUcMenu(*l_user_menu, ::UserCommand::CONTEXT_USER, client->getHubUrl());
		
		if (!(l_user_menu->GetMenuState(m_userMenu->GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
		{
			l_user_menu->AppendMenu(MF_SEPARATOR);
		}
		
		l_user_menu->AppendMenu(MF_STRING, IDC_REFRESH, CTSTRING(REFRESH_USER_LIST));
		
		if (ctrlUsers.GetSelectedCount() > 0)
		{
			l_user_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		WinUtil::unlinkStaticMenus(*l_user_menu); // TODO - fix copy-paste
		cleanUcMenu(*l_user_menu);
		l_user_menu->ClearMenu();
		return TRUE;
	}
	
	if (reinterpret_cast<HWND>(wParam) == ctrlClient)
	{
		OMenu* l_user_menu = createUserMenu();
		l_user_menu->ClearMenu();
		clearUserMenu(); // !SMT!-S
		
		if (pt.x == -1 && pt.y == -1)
		{
			CRect erc;
			ctrlClient.GetRect(&erc);
			pt.x = erc.Width() / 2;
			pt.y = erc.Height() / 2;
			ctrlClient.ClientToScreen(&pt);
		}
		POINT ptCl = pt;
		ctrlClient.ScreenToClient(&ptCl);
		ctrlClient.OnRButtonDown(ptCl);
		
		UserInfo *ui = nullptr;
		if (!ChatCtrl::g_sSelectedUserName.empty())
		{
			ui = findUser(ChatCtrl::g_sSelectedUserName);
		}
		reinitUserMenu(ui ? ui->getOnlineUser() : nullptr, getHubHint()); // [!] IRainman fix.
		
		appendHubAndUsersItems(*l_user_menu, true);
		
		if (!getSelectedUser())
		{
			l_user_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		else
		{
			appendUcMenu(*m_userMenu, ::UserCommand::CONTEXT_USER, client->getHubUrl());
			if (!(l_user_menu->GetMenuState(m_userMenu->GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
			{
				l_user_menu->AppendMenu(MF_SEPARATOR);
			}
			l_user_menu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		}
		WinUtil::unlinkStaticMenus(*l_user_menu); // TODO - fix copy-paste
		cleanUcMenu(*l_user_menu);
		l_user_menu->ClearMenu();
		return TRUE;
	}
	
	if (m_msgPanel && m_msgPanel->OnContextMenu(pt, wParam))
	{
		return TRUE;
	}
	
	return FALSE;
}

void HubFrame::runUserCommand(UserCommand& uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
		
	StringMap ucParams = ucLineParams;
	
	client->getMyIdentity().getParams(ucParams, "my", true);
	client->getHubIdentity().getParams(ucParams, "hub", false);
	
	if (m_tabMenuShown)
	{
		client->escapeParams(ucParams);
		client->sendUserCmd(uc, ucParams);
	}
	else
	{
		if (getSelectedUser())
		{
			// !SMT!-S
			UserInfo* u = findUser(getSelectedUser());
			if (u && u->getUser()->isOnline())
			{
				StringMap tmp = ucParams;
				u->getIdentity().getParams(tmp, "user", true);
				client->escapeParams(tmp);
				client->sendUserCmd(uc, tmp);
			}
		}
		else
		{
			int sel = -1;
			while ((sel = ctrlUsers.GetNextItem(sel, LVNI_SELECTED)) != -1)
			{
				UserInfo *u = (UserInfo*)ctrlUsers.getItemData(sel);
				if (u->getUser()->isOnline())
				{
					StringMap tmp = ucParams;
					u->getIdentity().getParams(tmp, "user", true);
					client->escapeParams(tmp);
					client->sendUserCmd(uc, tmp);
				}
			}
		}
	}
}
void HubFrame::onTab()
{
	if (m_ctrlMessage && m_ctrlMessage->GetWindowTextLength() == 0)
	{
		handleTab(WinUtil::isShift());
		return;
	}
	const HWND focus = GetFocus();
	if (m_ctrlMessage && focus == m_ctrlMessage->m_hWnd && !WinUtil::isShift())
	{
		tstring text;
		WinUtil::GetWindowText(text, *m_ctrlMessage);
		
		string::size_type textStart = text.find_last_of(_T(" \n\t"));
		
		if (m_complete.empty())
		{
			if (textStart != string::npos)
			{
				m_complete = text.substr(textStart + 1);
			}
			else
			{
				m_complete = text;
			}
			if (m_complete.empty())
			{
				// Still empty, no text entered...
				ctrlUsers.SetFocus();
				return;
			}
			const int y = ctrlUsers.GetItemCount();
			
			for (int x = 0; x < y; ++x)
				ctrlUsers.SetItemState(x, 0, LVNI_FOCUSED | LVNI_SELECTED);
		}
		
		if (textStart == string::npos)
			textStart = 0;
		else
			textStart++;
			
		int start = ctrlUsers.GetNextItem(-1, LVNI_FOCUSED) + 1;
		int i = start;
		const int j = ctrlUsers.GetItemCount();
		
		bool firstPass = i < j;
		if (!firstPass)
			i = 0;
		while (firstPass || (!firstPass && i < start))
		{
			const UserInfo* ui = ctrlUsers.getItemData(i);
			const tstring& nick = ui->getText(COLUMN_NICK);
			bool found = strnicmp(nick, m_complete, m_complete.length()) == 0;
			tstring::size_type x = 0;
			if (!found)
			{
				// Check if there's one or more [ISP] tags to ignore...
				tstring::size_type y = 0;
				while (nick[y] == _T('['))
				{
					x = nick.find(_T(']'), y);
					if (x != string::npos)
					{
						if (strnicmp(nick.c_str() + x + 1, m_complete.c_str(), m_complete.length()) == 0)
						{
							found = true;
							break;
						}
					}
					else
					{
						break;
					}
					y = x + 1; // assuming that nick[y] == '\0' is legal
				}
			}
			if (found)
			{
				if ((start - 1) != -1)
				{
					ctrlUsers.SetItemState(start - 1, 0, LVNI_SELECTED | LVNI_FOCUSED);
				}
				ctrlUsers.SetItemState(i, LVNI_FOCUSED | LVNI_SELECTED, LVNI_FOCUSED | LVNI_SELECTED);
				ctrlUsers.EnsureVisible(i, FALSE);
				if (m_ctrlMessage)
				{
					m_ctrlMessage->SetSel(static_cast<int>(textStart), m_ctrlMessage->GetWindowTextLength(), TRUE);
					m_ctrlMessage->ReplaceSel(nick.c_str());
				}
				return;
			}
			i++;
			if (i == j)
			{
				firstPass = false;
				i = 0;
			}
		}
	}
}

LRESULT HubFrame::onCloseWindows(WORD , WORD wID, HWND , BOOL&)
{
	switch (wID)
	{
		case IDC_CLOSE_DISCONNECTED:
			closeDisconnected();
			break;
		case IDC_RECONNECT_DISCONNECTED:
			reconnectDisconnected();
			break;
		case IDC_CLOSE_WINDOW:
			PostMessage(WM_CLOSE);
			break;
	}
	return 0;
}

LRESULT HubFrame::onFileReconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const FavoriteHubEntry *fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(Text::fromT(server));
	m_showJoins = fhe ? fhe->getShowJoins() : BOOLSETTING(SHOW_JOINS);
	m_favShowJoins = BOOLSETTING(FAV_SHOW_JOINS);
	
	if ((!fhe || fhe->getNick().empty()) && SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), T_APPNAME_WITH_VERSION, MB_ICONSTOP | MB_OK);// TODO Добавить адрес хаба в сообщение
		return 0;
	}
	client->reconnect();
	return 0;
}

LRESULT HubFrame::onDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	client->disconnect(true);
	return 0;
}

LRESULT HubFrame::onChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (!m_complete.empty() && wParam != VK_TAB && uMsg == WM_KEYDOWN)
	{
		m_complete.clear();
	}
	if (!processingServices(uMsg, wParam, lParam, bHandled))
	{
		switch (wParam)
		{
			case VK_TAB:
				onTab();
				break;
			default:
				processingHotKeys(uMsg, wParam, lParam, bHandled);
		}
	}
	return 0;
}
void HubFrame::usermap2ListrView()
{
	for (auto i = m_userMap.cbegin(); i != m_userMap.cend(); ++i)
	{
		const UserInfo* ui = i->second;
#ifdef IRAINMAN_USE_HIDDEN_USERS
		dcassert(ui->isHidden() == false);
#endif
		ctrlUsers.insertItem(ui, I_IMAGECALLBACK);
	}
}
void HubFrame::firstLoadAllUsers()
{
	CWaitCursor l_cursor_wait;
	m_needsResort = false;
	CLockRedraw<> l_lock_draw(ctrlUsers);
	usermap2ListrView();
	ctrlUsers.resort();
	m_needsResort = false;
}
LRESULT HubFrame::onShowUsers(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	if (m_ActivateCounter >= 1)
	{
		if (wParam == BST_CHECKED)
		{
			setShowUsers(true);
			firstLoadAllUsers();
		}
		else
		{
			setShowUsers(false);
			m_needsResort = false;
			// [!] IRainman Speed optimization and support for the full menu on selected nick in chat when user list is hided.
			ctrlUsers.DeleteAllItems();
			// [~] IRainman
		}
		UpdateLayout(FALSE);
		m_needsUpdateStats = true;
	}
	return 0;
}
#ifdef SCALOLAZ_HUB_SWITCH_BTN
void HubFrame::OnSwitchedPanels()
{
	m_isClientUsersSwitch = !m_isClientUsersSwitch;
	m_nProportionalPos = 10000 - m_nProportionalPos;
	TuneSplitterPanes();
	UpdateLayout();
}
#endif
LRESULT HubFrame::onFollow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_redirect.empty())
	{
		const string l_redirect = Text::fromT(m_redirect);
		if (ClientManager::isConnected(l_redirect))
		{
			addStatus(TSTRING(REDIRECT_ALREADY_CONNECTED), true, false, Colors::g_ChatTextServer);
			return 0;
		}
		//dcassert(g_frames.find(server) != g_frames.end());
		//dcassert(g_frames[server] == this);
		g_frames.erase(server);
		g_frames.insert(make_pair(m_redirect, this));
		server = m_redirect;
		// the client is dead, long live the client!
		client->removeListener(this);
		clearUserList();
		ClientManager::getInstance()->putClient(client);
		clearTaskList();
		client = ClientManager::getInstance()->getClient(l_redirect);
		RecentHubEntry r;
		r.setServer(l_redirect);
		FavoriteManager::getInstance()->addRecent(r);
		client->addListener(this);
		client->connect();
	}
	return 0;
}

LRESULT HubFrame::onEnterUsers(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	int item = ctrlUsers.GetNextItem(-1, LVNI_FOCUSED);
	if (item != -1)
	{
		try
		{
			QueueManager::getInstance()->addList(HintedUser((ctrlUsers.getItemData(item))->getUser(), getHubHint()), QueueItem::FLAG_CLIENT_VIEW); // TODO 2012-04-18_11-27-14_HBPJ2TOADS52IGWM7DSZBKMA5DX2ISVTKV3WYNI_E341CD83_crash-stack-r502-beta18-x64-build-9768.dmp
		}
		catch (const Exception& e)
		{
			addStatus(Text::toT(e.getError()));
		}
	}
	return 0;
}

void HubFrame::resortUsers()
{
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		i->second->resortForFavsFirst(true);
	}
	dcassert(l_size_g_frames == g_frames.size());
}

void HubFrame::closeDisconnected()
{
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		const auto& l_client = i->second->client;
		dcassert(l_client);
		if (!l_client->isConnected())
		{
			i->second->PostMessage(WM_CLOSE);
		}
	}
	dcassert(l_size_g_frames == g_frames.size());
}

void HubFrame::reconnectDisconnected()
{
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		const auto& l_client = i->second->client;
		dcassert(l_client);
		if (!l_client->isConnected())
		{
			l_client->reconnect();
		}
	} // http://www.flickr.com/photos/96019675@N02/9756341426/
	dcassert(l_size_g_frames == g_frames.size());
}

void HubFrame::closeAll(size_t thershold)
{
	if (thershold == 0)
	{
		// Ускорим закрытие всех хабов
		FavoriteManager::getInstance()->prepareClose();
		ClientManager::getInstance()->prepareClose();
		// TODO http://code.google.com/p/flylinkdc/issues/detail?id=1368
		// ClientManager::getInstance()->prepareClose(); // Отпишемся от подписок клиента
		// SearchManager::getInstance()->prepareClose(); // Отпишемся от подписок поиска
	}
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		dcassert(i->second->client);
		if (thershold == 0 || (
#ifndef FLYLINKDC_HE
		            i->second->client && // [+] fix https://crash-server.com/Bug.aspx?ClientID=ppa&ProblemID=27659
#endif
		            i->second->client->getUserCount() <= thershold))
		{
			i->second->PostMessage(WM_CLOSE);
		}
	}
	dcassert(l_size_g_frames == g_frames.size());
}
void HubFrame::on(FavoriteManagerListener::UserAdded, const FavoriteUser& /*aUser*/) noexcept
{
	resortForFavsFirst();
}
void HubFrame::on(FavoriteManagerListener::UserRemoved, const FavoriteUser& /*aUser*/) noexcept
{
	resortForFavsFirst();
}
void HubFrame::resortForFavsFirst(bool justDoIt /* = false */)
{
	if (justDoIt || BOOLSETTING(SORT_FAVUSERS_FIRST))
	{
		m_needsResort = true;
	}
}
LRESULT HubFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	// [+] IRainman opt.
	if (!m_spoken)
	{
		if (m_needsUpdateStats &&
		        BaseChatFrame::g_isStartupProcess == false && // Пока открываем хабы не обновляем статистику - мерцает
		        !MainFrame::isAppMinimized() && WinUtil::g_tabCtrl->isActive(m_hWnd) // [+] IRainman opt.
#ifndef IRAINMAN_NOT_USE_COUNT_UPDATE_INFO_IN_LIST_VIEW_CTRL
		        && ctrlUsers.getCountUpdateInfo() == 0 //[+]FlylinkDC++
#endif
		   )
		{
			dcassert(!ClientManager::isShutdown());
			speak(STATS);
			m_needsUpdateStats = false;
		}
		if (!m_tasks.empty())
		{
			force_speak();
		}
	}
	if (--m_second_count == 0)
	{
		m_second_count = 60;
		ClientManager::infoUpdated(client);
	}
	// [~] IRainman opt.
	return 0;
}

void HubFrame::on(Connecting, const Client*) noexcept
{
#ifdef RIP_USE_CONNECTION_AUTODETECT
	bool bWantAutodetect = false;
	if (!ClientManager::isActive(client->getHubUrl(), &bWantAutodetect))
	{
		if (bWantAutodetect)
			BaseChatFrame::addLine(_T("[!]FlylinkDC++ ") + _T("Detecting connection type: work in passive mode until direct mode is detected"), Colors::g_ChatTextSystem);
	}
#endif
	const auto l_url_hub = client->getHubUrl();
	// speak(ADD_STATUS_LINE, STRING(CONNECTING_TO) + ' ' + l_url_hub + " ...");
	// force_speak();
	addStatus(Text::toT(STRING(CONNECTING_TO) + ' ' + l_url_hub + " ..."));
	// Явно звать addStatus нельзя - вешаемся почему-то
	// http://code.google.com/p/flylinkdc/issues/detail?id=1428
	setWindowTitle(l_url_hub);
}
void HubFrame::on(ClientListener::Connected, const Client* c) noexcept
{
	if (!client->isActive())
	{
		BaseChatFrame::addLine(_T("[!]FlylinkDC++ ") + TSTRING(PASSIVE_NOTICE) + WinUtil::GetWikiLink() + L"connection_settings", Colors::g_ChatTextSystem);
	}
	else if (BOOLSETTING(SEARCH_PASSIVE))
	{
		BaseChatFrame::addLine(_T("[!]FlylinkDC++ ") + TSTRING(PASSIVE_SEARCH_NOTICE) + WinUtil::GetWikiLink() + L"advanced", Colors::g_ChatTextSystem);
	}
	//speak(CONNECTED);
	PostMessage(WM_SPEAKER_CONNECTED);
	ChatBot::getInstance()->onHubAction(BotInit::RECV_CONNECT, c->getHubUrl());
}

void HubFrame::on(ClientListener::UserUpdated, const Client*, const OnlineUserPtr& user) noexcept   // !SMT!-fix
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
#ifdef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
		const auto l_ou_ptr = new OnlineUserPtr(user);
		PostMessage(WM_SPEAKER_UPDATE_USER_JOIN, WPARAM(l_ou_ptr));
#else
		speak(UPDATE_USER_JOIN, user);
#endif
#ifdef _DEBUG
//		LogManager::getInstance()->message("[single OnlineUserPtr] void HubFrame::on(ClientListener::UserUpdated nick = " + user->getUser()->getLastNick());
#endif
		ChatBot::getInstance()->onUserAction(BotInit::RECV_UPDATE, user->getUser());
	}
}

void HubFrame::on(ClientListener::StatusMessage, const Client*, const string& line, int statusFlags)
{
	speak(ADD_STATUS_LINE, Text::toDOS(line), !BOOLSETTING(FILTER_MESSAGES) || !(statusFlags & ClientListener::FLAG_IS_SPAM));
}

void HubFrame::on(ClientListener::UsersUpdated, const Client*, const OnlineUserList& aList) noexcept
{
	for (auto i = aList.cbegin(); i != aList.cend(); ++i)
	{
		const auto l_ou_ptr = new OnlineUserPtr(*i);
		PostMessage(WM_SPEAKER_UPDATE_USER, WPARAM(l_ou_ptr));
//		speak(UPDATE_USER, *i); // !SMT!-fix
#ifdef _DEBUG
//		LogManager::getInstance()->message("[array OnlineUserPtr] void HubFrame::on(UsersUpdated nick = " + (*i)->getUser()->getLastNick());
#endif
		ChatBot::getInstance()->onUserAction(BotInit::RECV_UPDATE, (*i)->getUser());
	}
}
void HubFrame::on(ClientListener::UserRemoved, const Client*, const OnlineUserPtr& user) noexcept
{
#ifdef FLYLINKDC_REMOVE_USER_WIN_MESSAGES_Q
	const auto l_ou_ptr = new OnlineUserPtr(user);
	PostMessage(WM_SPEAKER_REMOVE_USER, WPARAM(l_ou_ptr));
#else
	speak(REMOVE_USER, user);
#endif
	ChatBot::getInstance()->onUserAction(BotInit::RECV_PART, user->getUser()); // !SMT!-fix
}

void HubFrame::on(Redirect, const Client*, const string& line) noexcept
{
	const auto redirAdr = Util::formatDchubUrl(line); // [+] IRainman fix http://code.google.com/p/flylinkdc/issues/detail?id=1237
	if (ClientManager::isConnected(redirAdr))
	{
		speak(ADD_STATUS_LINE, STRING(REDIRECT_ALREADY_CONNECTED), true);
		return;
	}
	
	m_redirect = Text::toT(redirAdr);
#ifdef PPA_INCLUDE_AUTO_FOLLOW
	if (BOOLSETTING(AUTO_FOLLOW))
	{
		PostMessage(WM_COMMAND, IDC_FOLLOW, 0);
	}
	else
#endif
	{
		speak(ADD_STATUS_LINE, STRING(PRESS_FOLLOW) + ' ' + line, true);
	}
}
void HubFrame::on(ClientListener::Failed, const Client* c, const string& line) noexcept
{
	speak(ADD_STATUS_LINE, line);
	//speak(DISCONNECTED);
	PostMessage(WM_SPEAKER_DISCONNECTED);
	ChatBot::getInstance()->onHubAction(BotInit::RECV_DISCONNECT, c->getHubUrl());
}
void HubFrame::on(ClientListener::GetPassword, const Client*) noexcept
{
	speak(GET_PASSWORD);
}
void HubFrame::setShortHubName(const tstring& p_name)
{
	if (m_shortHubName != p_name)
	{
		++m_is_window_text_update;
	}
	m_shortHubName = p_name;
	if (!p_name.empty())
	{
		SetWindowLongPtr(GWLP_USERDATA, (LONG_PTR)&m_shortHubName);
	}
	else
	{
		SetWindowLongPtr(GWLP_USERDATA, (LONG_PTR)nullptr);
	}
}
void HubFrame::on(ClientListener::HubUpdated, const Client*) noexcept
{
	string fullHubName;
	if (client->isTrusted())
	{
		fullHubName = "[S] ";
	}
	else if (client->isSecure())
	{
		fullHubName = "[U] ";
	}
	
	fullHubName += client->getHubName();
	
	if (!client->getName().empty())
	{
		const auto l_name = Text::toT(client->getName());
		setShortHubName(l_name);
	}
	else
	{
		setShortHubName(Text::toT(fullHubName));
	}
	if (!client->getHubDescription().empty())
		fullHubName += " - " + client->getHubDescription();
	fullHubName += " (" + client->getHubUrl() + ')';
	
#ifdef _DEBUG
	const string version = client->getHubIdentity().getStringParam("VE");
	if (!version.empty())
	{
		setShortHubName(m_shortHubName + Text::toT(" - " + version));
		fullHubName += " - " + version;
	}
#endif
	setWindowTitle(fullHubName); // TODO перенести на таймер? http://code.google.com/p/flylinkdc/issues/detail?id=1428
	// SetWindowLongPtr(GWLP_USERDATA, (LONG_PTR)&m_shortHubName);
	// Нельзя тут setDirty();
}

void HubFrame::on(ClientListener::Message, const Client*,  std::unique_ptr<ChatMessage>& message) noexcept
{
	const auto l_message_ptr = message.release();
	if (l_message_ptr->isPrivate())
	{
#ifndef FLYLINKDC_PRIVATE_MESSAGE_USE_WIN_MESSAGES_Q
		speak(PRIVATE_MESSAGE, l_message_ptr);
#else
		PostMessage(WM_SPEAKER_PRIVATE_MESSAGE, WPARAM(l_message_ptr));
#endif
	}
	else
	{
		PostMessage(WM_SPEAKER_ADD_CHAT_LINE, WPARAM(l_message_ptr));
	}
}
/*[-] IRainman fix.
void HubFrame::on(PrivateMessage, const Client*, const string &strFromUserName, const UserPtr& from, const UserPtr &to, const UserPtr &replyTo, const string& line, bool thirdPerson) noexcept   // !SMT!-S
{
speak(PRIVATE_MESSAGE, strFromUserName, from, to, replyTo, Util::formatMessage(line), thirdPerson); // !SMT!-S
}
[~]*/
void HubFrame::on(ClientListener::NickTaken, const Client*) noexcept
{
	speak(ADD_STATUS_LINE, STRING(NICK_TAKEN), true);
}
#ifdef IRAINMAN_USE_SEARCH_FLOOD_FILTER
void HubFrame::on(SearchFlood, const Client*, const string& line) noexcept
{
	speak(ADD_STATUS_LINE, STRING(SEARCH_SPAM_FROM) + ' ' + line, true);
}
#endif
void HubFrame::on(ClientListener::CheatMessage, const string& line) noexcept
{
	const auto l_message_ptr = new string(line);
	PostMessage(WM_SPEAKER_CHEATING_USER, WPARAM(l_message_ptr));
}
void HubFrame::on(ClientListener::UserReport, const Client*, const string& report) noexcept // [+] IRainman
{
	const auto l_message_ptr = new string(report);
	PostMessage(WM_SPEAKER_USER_REPORT, WPARAM(l_message_ptr));
}
void HubFrame::on(ClientListener::HubTopic, const Client*, const string& line) noexcept
{
	speak(ADD_STATUS_LINE, STRING(HUB_TOPIC) + "\t" + line, true);
}
#ifdef RIP_USE_CONNECTION_AUTODETECT
void HubFrame::on(DirectModeDetected, const string& strHubUrl)  noexcept
{
	if (client->getHubUrl() == strHubUrl)
		speak(DIRECT_MODE_DETECTED, strHubUrl, true);
}
#endif
LRESULT HubFrame::onFilterChar(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (uMsg == WM_CHAR && wParam == VK_TAB)
	{
		handleTab(WinUtil::isShift());
		return 0;
	}
	
	dcassert(m_ctrlFilter);
	if (m_ctrlFilter)
	{
		if (wParam == VK_RETURN || !BOOLSETTING(FILTER_ENTER))
		{
			WinUtil::GetWindowText(m_filter, *m_ctrlFilter);
			
			// [+] birkoff.anarchist | Escape .^+(){}[] symbols in filter
			static const std::wregex rx(L"(\\.|\\^|\\+|\\[|\\]|\\{|\\}|\\(|\\))");
			static const tstring replacement = L"\\$1";
			m_filter = std::regex_replace(m_filter, rx, replacement);
			
			updateUserList();
		}
	}
	
	bHandled = FALSE;
	
	return 0;
}

LRESULT HubFrame::onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	dcassert(m_ctrlFilter);
	if (m_ctrlFilter)
	{
		WinUtil::GetWindowText(m_filter, *m_ctrlFilter);
		updateUserList();
	}
	
	bHandled = FALSE;
	
	return 0;
}

bool HubFrame::parseFilter(FilterModes& mode, int64_t& size)
{
	tstring::size_type start = (tstring::size_type)tstring::npos;
	tstring::size_type end = (tstring::size_type)tstring::npos;
	int64_t multiplier = 1;
	
	if (m_filter.empty())
	{
		return false;
	}
	if (m_filter.compare(0, 2, _T(">="), 2) == 0)
	{
		mode = GREATER_EQUAL;
		start = 2;
	}
	else if (m_filter.compare(0, 2, _T("<="), 2) == 0)
	{
		mode = LESS_EQUAL;
		start = 2;
	}
	else if (m_filter.compare(0, 2, _T("=="), 2) == 0)
	{
		mode = EQUAL;
		start = 2;
	}
	else if (m_filter.compare(0, 2, _T("!="), 2) == 0)
	{
		mode = NOT_EQUAL;
		start = 2;
	}
	else if (m_filter[0] == _T('<'))
	{
		mode = LESS;
		start = 1;
	}
	else if (m_filter[0] == _T('>'))
	{
		mode = GREATER;
		start = 1;
	}
	else if (m_filter[0] == _T('='))
	{
		mode = EQUAL;
		start = 1;
	}
	
	if (start == tstring::npos)
		return false;
	if (m_filter.length() <= start)
		return false;
		
	if ((end = Util::findSubString(m_filter, _T("TiB"))) != tstring::npos)
	{
		multiplier = 1024LL * 1024LL * 1024LL * 1024LL;
	}
	else if ((end = Util::findSubString(m_filter, _T("GiB"))) != tstring::npos)
	{
		multiplier = 1024 * 1024 * 1024;
	}
	else if ((end = Util::findSubString(m_filter, _T("MiB"))) != tstring::npos)
	{
		multiplier = 1024 * 1024;
	}
	else if ((end = Util::findSubString(m_filter, _T("KiB"))) != tstring::npos)
	{
		multiplier = 1024;
	}
	else if ((end = Util::findSubString(m_filter, _T("TB"))) != tstring::npos)
	{
		multiplier = 1000LL * 1000LL * 1000LL * 1000LL;
	}
	else if ((end = Util::findSubString(m_filter, _T("GB"))) != tstring::npos)
	{
		multiplier = 1000 * 1000 * 1000;
	}
	else if ((end = Util::findSubString(m_filter, _T("MB"))) != tstring::npos)
	{
		multiplier = 1000 * 1000;
	}
	else if ((end = Util::findSubString(m_filter, _T("kB"))) != tstring::npos)
	{
		multiplier = 1000;
	}
	else if ((end = Util::findSubString(m_filter, _T("B"))) != tstring::npos)
	{
		multiplier = 1;
	}
	
	if (end == tstring::npos)
	{
		end = m_filter.length();
	}
	
	const tstring tmpSize = m_filter.substr(start, end - start);
	size = static_cast<int64_t>(Util::toDouble(Text::fromT(tmpSize)) * multiplier);
	
	return true;
}

void HubFrame::InsertUserList(UserInfo* ui) // [!] IRainman opt.
{
#ifdef IRAINMAN_USE_HIDDEN_USERS
	dcassert(ui->isHidden() == false);
#endif
	//single update
	//avoid refreshing the whole list and just update the current item
	//instead
	if (m_filter.empty())
	{
		dcassert(ctrlUsers.findItem(ui) == -1);
		ctrlUsers.insertItem(ui, I_IMAGECALLBACK);
	}
	else
	{
		int64_t size = -1;
		FilterModes mode = NONE;
		const int sel = getFilterSelPos();
		bool doSizeCompare = sel == COLUMN_SHARED && parseFilter(mode, size);
		
		if (matchFilter(*ui, sel, doSizeCompare, mode, size))
		{
			dcassert(ctrlUsers.findItem(ui) == -1);
			ctrlUsers.insertItem(ui, I_IMAGECALLBACK);
		}
		else
		{
			//deleteItem checks to see that the item exists in the list
			//unnecessary to do it twice.
			ctrlUsers.deleteItem(ui);
		}
	}
}

void HubFrame::updateUserList() // [!] IRainman opt.
{
	CLockRedraw<> l_lock_draw(ctrlUsers);
	ctrlUsers.DeleteAllItems();
	if (m_filter.empty())
	{
		usermap2ListrView();
	}
	else
	{
		int64_t size = -1;
		FilterModes mode = NONE;
		dcassert(m_ctrlFilterSel);
		const int sel = getFilterSelPos();
		const bool doSizeCompare = sel == COLUMN_SHARED && parseFilter(mode, size);
		for (auto i = m_userMap.cbegin(); i != m_userMap.cend(); ++i)
		{
			UserInfo* ui = i->second;
#ifdef IRAINMAN_USE_HIDDEN_USERS
			dcassert(ui->isHidden() == false);
#endif
			if (matchFilter(*ui, sel, doSizeCompare, mode, size))
			{
				ctrlUsers.insertItem(ui, I_IMAGECALLBACK);
			}
		}
	}
	m_needsUpdateStats = true; // [+] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=811
}

void HubFrame::handleTab(bool reverse)
{
	HWND focus = GetFocus();
	
	if (reverse)
	{
		if (m_ctrlFilterSel && focus == m_ctrlFilterSel->m_hWnd)
		{
			m_ctrlFilter->SetFocus();
		}
		else if (m_ctrlMessage && m_ctrlFilter && focus == m_ctrlFilter->m_hWnd)
		{
			m_ctrlMessage->SetFocus();
		}
		else if (m_ctrlMessage && focus == m_ctrlMessage->m_hWnd)
		{
			ctrlUsers.SetFocus();
		}
		else if (focus == ctrlUsers.m_hWnd)
		{
			ctrlClient.SetFocus();
		}
		else if (m_ctrlFilterSel && focus == ctrlClient.m_hWnd)
		{
			m_ctrlFilterSel->SetFocus();
		}
	}
	else
	{
		if (focus == ctrlClient.m_hWnd)
		{
			ctrlUsers.SetFocus();
		}
		else if (m_ctrlMessage && focus == ctrlUsers.m_hWnd)
		{
			m_ctrlMessage->SetFocus();
		}
		else if (m_ctrlMessage && focus == m_ctrlMessage->m_hWnd)
		{
			if (m_ctrlFilter)
				m_ctrlFilter->SetFocus();
		}
		else if (m_ctrlFilter &&  m_ctrlFilterSel && focus == m_ctrlFilter->m_hWnd)
		{
			m_ctrlFilterSel->SetFocus();
		}
		else if (m_ctrlFilterSel && focus == m_ctrlFilterSel->m_hWnd)
		{
			ctrlClient.SetFocus();
		}
	}
}

bool HubFrame::matchFilter(UserInfo& ui, int sel, bool doSizeCompare, FilterModes mode, int64_t size)
{

	if (m_filter.empty())
		return true;
		
	bool insert = false;
	if (doSizeCompare)
	{
		switch (mode)
		{
			case EQUAL:
				insert = (size == ui.getIdentity().getBytesShared());
				break;
			case GREATER_EQUAL:
				insert = (size <=  ui.getIdentity().getBytesShared());
				break;
			case LESS_EQUAL:
				insert = (size >=  ui.getIdentity().getBytesShared());
				break;
			case GREATER:
				insert = (size < ui.getIdentity().getBytesShared());
				break;
			case LESS:
				insert = (size > ui.getIdentity().getBytesShared());
				break;
			case NOT_EQUAL:
				insert = (size != ui.getIdentity().getBytesShared());
				break;
		}
	}
	else
	{
		try
		{
			std::wregex reg(m_filter, std::regex_constants::icase);
			if (sel >= COLUMN_LAST)
			{
				for (uint8_t i = COLUMN_FIRST; i < COLUMN_LAST; ++i)
				{
					const tstring s = ui.getText(i);
					if (std::regex_search(s.begin(), s.end(), reg))
					{
						insert = true;
						break;
					}
				}
			}
			else
			{
				if (sel == COLUMN_GEO_LOCATION) // http://code.google.com/p/flylinkdc/issues/detail?id=1319
				{
					ui.calcLocation();
				}
				const tstring s = ui.getText(static_cast<uint8_t>(sel));
				if (regex_search(s.begin(), s.end(), reg))
					insert = true;
			}
		}
		catch (...) // ??
		{
			dcassert(0);
			insert = true;
		}
	}
	return insert;
}

void HubFrame::appendHubAndUsersItems(OMenu& p_menu, const bool isChat)
{
	if (getSelectedUser())
	{
		// UserInfo *ui = findUser(aUser); // !SMT!-S [-] IRainman opt.
		const bool isMe = ClientManager::isMe(getSelectedUser());  // [!] IRainman fix: if crash here - please report me! Don't add check aUser->getUser() !!!!!
		// Jediny nick
		p_menu.InsertSeparatorFirst(getSelectedUser()->getIdentity().getNickT());
		// some commands starts in UserInfoBaseHandler, that requires user visible
		// in ListView. for now, just disable menu item to workaronud problem
		if (!isMe)
		{
			appendAndActivateUserItems(p_menu);
			
			if (isChat)
			{
				p_menu.AppendMenu(MF_STRING, IDC_SELECT_USER, CTSTRING(SELECT_USER_LIST));
			}
			p_menu.AppendMenu(MF_SEPARATOR);
		}
	}
	else
	{
		// Muze byt vice nicku
		if (!isChat)
		{
			// Pocet oznacenych
			int iCount = ctrlUsers.GetSelectedCount();
			p_menu.InsertSeparatorFirst(Util::toStringW(iCount) + _T(' ') + TSTRING(HUB_USERS));
			appendAndActivateUserItems(p_menu);
		}
	}
	
	if (isChat)
	{
		appendChatCtrlItems(p_menu, client);
	}
	
	if (isChat)
	{
		switch (SETTING(CHAT_DBLCLICK))
		{
			case 0:
				p_menu.SetMenuDefaultItem(IDC_SELECT_USER);
				break;
			case 1:
				p_menu.SetMenuDefaultItem(IDC_ADD_NICK_TO_CHAT);
				break;
			case 2:
				p_menu.SetMenuDefaultItem(IDC_PRIVATE_MESSAGE);
				break;
			case 3:
				p_menu.SetMenuDefaultItem(IDC_GETLIST);
				break;
			case 4:
				p_menu.SetMenuDefaultItem(IDC_MATCH_QUEUE);
				break;
			case 6:
				p_menu.SetMenuDefaultItem(IDC_ADD_TO_FAVORITES);
				break;
			case 7:
				p_menu.SetMenuDefaultItem(IDC_BROWSELIST);
		}
	}
	else
	{
		switch (SETTING(USERLIST_DBLCLICK))
		{
			case 0:
				p_menu.SetMenuDefaultItem(IDC_GETLIST);
				break;
			case 1:
				p_menu.SetMenuDefaultItem(IDC_ADD_NICK_TO_CHAT);
				break;
			case 2:
				p_menu.SetMenuDefaultItem(IDC_PRIVATE_MESSAGE);
				break;
			case 3:
				p_menu.SetMenuDefaultItem(IDC_MATCH_QUEUE);
				break;
			case 5:
				p_menu.SetMenuDefaultItem(IDC_ADD_TO_FAVORITES);
				break;
			case 6:
				p_menu.SetMenuDefaultItem(IDC_BROWSELIST);
		}
	}
}

LRESULT HubFrame::onSelectUser(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!getSelectedUser())
	{
		// No nick selected
		return 0;
	}
	
	const int pos = ctrlUsers.findItem(getSelectedUser()->getIdentity().getNickT());
	if (pos == -1)
	{
		// User not found is list
		return 0;
	}
	
	CLockRedraw<> l_lock_draw(ctrlUsers);
	const int items = ctrlUsers.GetItemCount();
	for (int i = 0; i < items; ++i)
	{
		ctrlUsers.SetItemState(i, i == pos ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
	}
	ctrlUsers.EnsureVisible(pos, FALSE);
	return 0;
}

LRESULT HubFrame::onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!client->isConnected())
		return 0;
		
	if (getSelectedUser())
	{
		m_lastUserName = getSelectedUser()->getIdentity().getNickT();// SSA_SAVE_LAST_NICK_MACROS
	}
	else
	{
		m_lastUserName.clear(); // SSA_SAVE_LAST_NICK_MACROS
		int i = -1;
		while ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			if (!m_lastUserName.empty())// SSA_SAVE_LAST_NICK_MACROS
				m_lastUserName += _T(", ");// SSA_SAVE_LAST_NICK_MACROS
				
			m_lastUserName += Text::toT(((UserInfo*)ctrlUsers.getItemData(i))->getNick());// SSA_SAVE_LAST_NICK_MACROS
		}
	}
	appendNickToChat(m_lastUserName); // SSA_SAVE_LAST_NICK_MACROS
	return 0;
}

LRESULT HubFrame::onAutoScrollChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlClient.SetAutoScroll(!ctrlClient.GetAutoScroll());
	return 0;
}

LRESULT HubFrame::onBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!ChatCtrl::g_sSelectedIP.empty())
	{
		const tstring s = _T("!banip ") + ChatCtrl::g_sSelectedIP;
		sendMessage(s);
	}
	return 0;
}

LRESULT HubFrame::onUnBanIP(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!ChatCtrl::g_sSelectedIP.empty())
	{
		const tstring s = _T("!unban ") + ChatCtrl::g_sSelectedIP;
		sendMessage(s);
	}
	return 0;
}

LRESULT HubFrame::onOpenUserLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	UserInfo* ui = nullptr;
	if (getSelectedUser())
	{
		ui = findUser(getSelectedUser()); // !SMT!-S
	}
	else
	{
		int i = -1;
		if ((i = ctrlUsers.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			ui = (UserInfo*)ctrlUsers.getItemData(i);
		}
	}
	
	if (!ui)
		return 0;
		
	StringMap params = getFrameLogParams();
	
	params["userNI"] = ui->getNick();
	params["userCID"] = ui->getUser()->getCID().toBase32();
	
	WinUtil::openLog(SETTING(LOG_FILE_PRIVATE_CHAT), params, TSTRING(NO_LOG_FOR_USER));
	
	return 0;
}

LRESULT HubFrame::onOpenHubLog(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	openFrameLog();
	return 0;
}

LRESULT HubFrame::onStyleChange(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	if ((wParam & MK_LBUTTON) && ::GetCapture() == m_hWnd)
	{
		UpdateLayout(FALSE);
	}
	return 0;
}

LRESULT HubFrame::onStyleChanged(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	UpdateLayout(FALSE);
	return 0;
}

void HubFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
{
	ctrlUsers.SetImageList(g_userImage.getIconList(), LVSIL_SMALL);
	if (ctrlUsers.GetBkColor() != Colors::bgColor)
	{
		ctrlClient.SetBackgroundColor(Colors::bgColor);
		ctrlUsers.SetBkColor(Colors::bgColor);
		ctrlUsers.SetTextBkColor(Colors::bgColor);
		ctrlUsers.setFlickerFree(Colors::bgBrush);
	}
	if (ctrlUsers.GetTextColor() != Colors::textColor)
	{
		ctrlUsers.SetTextColor(Colors::textColor);
	}
	UpdateLayout();
	RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
}

LRESULT HubFrame::onSizeMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	ctrlClient.GoToEnd();
	return 0;
}

LRESULT HubFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	//dcassert(g_isStartupProcess != true);
	if (g_isStartupProcess == true)
	{
		return CDRF_DODEFAULT;
	}
	// http://forums.codeguru.com/showthread.php?512490-NM_CUSTOMDRAW-on-Listview
	// Последовательность событий при отрисовке
	CRect rc;
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
//			PROFILE_THREAD_SCOPED_DESC("CDDS_SUBITEM | CDDS_ITEMPREPAINT");
			UserInfo* ui = (UserInfo*)cd->nmcd.lItemlParam;
			if (!ui)
				return CDRF_DODEFAULT;
			const int l_column_id = ctrlUsers.findColumn(cd->iSubItem);
#ifdef SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
			bool l_is_last_ip = false;
			if (l_column_id == COLUMN_IP || l_column_id == COLUMN_GEO_LOCATION)
				l_is_last_ip = ui->isIPFromSQL();
#endif
#ifndef IRAINMAN_TEMPORARY_DISABLE_XXX_ICON
			if (l_column_id == COLUMN_DESCRIPTION &&
			        (ui->getIdentity().getUser()->isSet(User::GREY_XXX_5) ||
			         ui->getIdentity().getUser()->isSet(User::GREY_XXX_6)))
			{
				ctrlUsers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				LONG top = rc.top + (rc.Height() - 15) / 2;
				if ((top - rc.top) < 2)
					top = rc.top + 1;
				const POINT p = { rc.left, top };
				const int l_indx_icon = int(ui->getIdentity().getUser()->isSet(User::GREY_XXX_6)) << 1 | int(ui->getIdentity().getUser()->isSet(User::GREY_XXX_5));
				g_userImage.Draw(cd->nmcd.hdc, 104 + l_indx_icon , p);
				const tstring l_w_full_name = ctrlUsers.ExGetItemTextT((int)cd->nmcd.dwItemSpec, cd->iSubItem));
				if (!l_w_full_name.empty())
					::ExtTextOut(cd->nmcd.hdc, rc.left + 16, top + 1, ETO_CLIPPED, rc, l_w_full_name.c_str(), l_w_full_name.length(), NULL);
					return CDRF_SKIPDEFAULT;
				}
			else
#endif
#ifdef SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
				if (l_column_id == COLUMN_IP)
				{
//					PROFILE_THREAD_SCOPED_DESC("COLUMN_IP");
					if (l_is_last_ip)
					{
						CRect rc;
						ctrlUsers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
						// gray color for text
						COLORREF col_brit = OperaColors::brightenColor(cd->clrText, 0.6f);
						ctrlUsers.SetItemFilled(cd, rc, /*color_text*/ col_brit, /*color_text_unfocus*/ col_brit);
						
						const tstring l_ip = ui->getText(COLUMN_IP);
						::ExtTextOut(cd->nmcd.hdc, rc.left + 6, rc.top + 2, ETO_CLIPPED, rc, l_ip.c_str(), l_ip.length(), NULL);
						
						return CDRF_SKIPDEFAULT;
					}
				}
				else
#endif // SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
					// !SMT!-IP
					if (l_column_id == COLUMN_GEO_LOCATION)
					{
//				PROFILE_THREAD_SCOPED_DESC("COLUMN_GEO_LOCATION");
#ifdef SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
						const COLORREF col_brit = OperaColors::brightenColor(cd->clrText, 0.5f);
#endif // SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
						ctrlUsers.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
						if (BOOLSETTING(USE_EXPLORER_THEME)
#ifdef FLYLINKDC_SUPPORT_WIN_2000
						        && CompatibilityManager::IsXPPlus()
#endif
						   )
						{
						
#ifdef SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
							SetTextColor(cd->nmcd.hdc, !l_is_last_ip ? cd->clrText : col_brit);
#else
							SetTextColor(cd->nmcd.hdc, cd->clrText);
#endif //SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
							
							if (m_Theme)
							{
#if _MSC_VER < 1700 // [!] FlylinkDC++
								const auto l_res = DrawThemeBackground(m_Theme, cd->nmcd.hdc, LVP_LISTITEM, 3, &rc, &rc);
#else
								const auto l_res = DrawThemeBackground(m_Theme, cd->nmcd.hdc, 1, 3, &rc, &rc);
#endif // _MSC_VER < 1700
								//dcassert(l_res == S_OK);
								if (l_res != S_OK)
								{
#ifdef SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
									ctrlUsers.SetItemFilled(cd, rc, !l_is_last_ip ? cd->clrText : col_brit, !l_is_last_ip ? cd->clrText : col_brit); // TODO fix copy-paste
#else
									ctrlUsers.SetItemFilled(cd, rc, cd->clrText);
#endif //SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
								}
							}
							else
							{
#ifdef SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
								ctrlUsers.SetItemFilled(cd, rc, !l_is_last_ip ? cd->clrText : col_brit, !l_is_last_ip ? cd->clrText : col_brit); // TODO fix copy-paste
#else
								ctrlUsers.SetItemFilled(cd, rc, cd->clrText, cd->clrText);
#endif
							}
						}
						else
						{
#ifdef SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
							ctrlUsers.SetItemFilled(cd, rc, !l_is_last_ip ? cd->clrText : col_brit, !l_is_last_ip ? cd->clrText : col_brit); // TODO fix copy-paste
#else
							ctrlUsers.SetItemFilled(cd, rc, cd->clrText);
#endif //SCALOLAZ_BRIGHTEN_LOCATION_WITH_LASTIP
						}
						// TODO fix copy-paste
						const auto& l_location = ui->getLocation();
						if (l_location.isKnown())
						{
							rc.left += 2;
							LONG top = rc.top + (rc.Height() - 15) / 2;
							if ((top - rc.top) < 2)
								top = rc.top + 1;
							// TODO: move this to FlagImage and cleanup!
							int l_step = 0;
							if (BOOLSETTING(ENABLE_COUNTRYFLAG) && l_location.getCountryIndex())
							{
								const POINT ps = { rc.left, top };
								g_flagImage.DrawCountry(cd->nmcd.hdc, l_location, ps);
								l_step += 25;
							}
							const POINT p = { rc.left + l_step, top };
							if (l_location.getFlagIndex())
							{
								g_flagImage.DrawLocation(cd->nmcd.hdc, l_location, p);
								l_step += 25;
							}
							// ~TODO: move this to FlagImage and cleanup!
							top = rc.top + (rc.Height() - WinUtil::getTextHeight(cd->nmcd.hdc) - 1) / 2;
							const auto& l_desc = l_location.getDescription();
							::ExtTextOut(cd->nmcd.hdc, rc.left + l_step + 5, top + 1, ETO_CLIPPED, rc, l_desc.c_str(), l_desc.length(), nullptr);
						}
						return CDRF_SKIPDEFAULT;
					}
			bHandled = FALSE;
			return 0;
		}
		case CDDS_ITEMPREPAINT:
		{
			UserInfo* ui = (UserInfo*)cd->nmcd.lItemlParam;
			if (ui)
			{
//				PROFILE_THREAD_SCOPED_DESC("CDDS_ITEMPREPAINT");
				ui->calcLocation();
				Colors::getUserColor(ui->getUser(), cd->clrText, cd->clrTextBk, ui->getOnlineUser()); // !SMT!-UI
				dcassert(client);
				if (client->isOp()) // Возможно фикс https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=38000
				{
//					PROFILE_THREAD_SCOPED_DESC("CDDS_ITEMPREPAINT-isOP");
					const auto fc = ui->getIdentity().getFakeCard();
					if (fc & Identity::BAD_CLIENT)
					{
						cd->clrText = SETTING(BAD_CLIENT_COLOUR);
					}
					else if (fc & Identity::BAD_LIST)
					{
						cd->clrText = SETTING(BAD_FILELIST_COLOUR);
					}
					else if (fc & Identity::CHECKED && BOOLSETTING(SHOW_SHARE_CHECKED_USERS))
					{
						cd->clrText = SETTING(FULL_CHECKED_COLOUR);
					}
				}
			}
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
			Colors::alternationBkColor(cd); // [+] IRainman
#endif
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
		}
		
		default:
			return CDRF_DODEFAULT;
	}
}
// !SMT!-UI
void HubFrame::addDupeUsersToSummaryMenu(const int64_t &share, const string& ip)
{
	// Данная функция ломает меню - http://youtu.be/GaWw-S4ZYJA
	// Причину пока не знаю - есть краши https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=27075
	// L: ретурн убрал, ведь не помогло же! return; // http://www.flylinkdc.ru/2013/07/flylinkdc-r502-beta92-build-14457.html
	/*
	r502-beta94-x64 build 14474
	косяк с пкм после alt+d ни куда не делся
	L: есть значительная вероятность того, что после моего рефакторинга проблемы исчезнут, прошу отписаться.
	*/
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto f = g_frames.cbegin(); f != g_frames.cend(); ++f)
	{
		const auto& frame = f->second;
		for (auto i = frame->m_userMap.cbegin(); i != frame->m_userMap.cend(); ++i) // TODO https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=28097
		{
			const auto& l_id = i->second->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'i->second->getIdentity()' expression repeatedly. hubframe.cpp 3673
			if (share && l_id.getBytesShared() == share)
			{
				tstring info = Text::toT(frame->client->getHubName()) + _T(" - ") + i->second->getText(COLUMN_NICK);
				const UINT flags = (!ip.empty() && ip == l_id.getIp()) ? MF_CHECKED : 0;
				FavoriteUser favUser;
				if (FavoriteManager::getInstance()->getFavoriteUser(i->second->getUser(), favUser))
				{
					string favInfo;
					if (favUser.isSet(FavoriteUser::FLAG_GRANT_SLOT))
					{
						favInfo += ' ' + STRING(AUTO_GRANT);
					}
					if (favUser.isSet(FavoriteUser::FLAG_IGNORE_PRIVATE))
					{
						favInfo += ' ' + STRING(IGNORE_PRIVATE);
					}
					if (favUser.getUploadLimit() != FavoriteUser::UL_NONE)
					{
						favInfo += ' ' + FavoriteUser::getSpeedLimitText(favUser.getUploadLimit());
					}
					if (!favUser.getDescription().empty())
					{
						favInfo += " \"" + favUser.getDescription() + '\"';
					}
					if (!favInfo.empty())
					{
						info += _T(",   FavInfo: ") + Text::toT(favInfo);
					}
				}
				userSummaryMenu.AppendMenu(MF_SEPARATOR);
				userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED | flags, IDC_NONE, info.c_str());
				userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, Text::toT(l_id.getApplication() + ",   IP: " + l_id.getIp()).c_str());
			}
		}
	}
	dcassert(l_size_g_frames == g_frames.size());
}

void HubFrame::addPasswordCommand()
{
	const TCHAR* l_pass = _T("/password ");
	if (m_ctrlMessage)
	{
		m_ctrlMessage->SetWindowText(l_pass);
		m_ctrlMessage->SetFocus();
		m_ctrlMessage->SetSel(10, 10);
	}
	else
	{
		m_LastMessage = l_pass;
	}
}

/**
 * @file
 * $Id: HubFrame.cpp,v 1.132 2006/11/04 14:08:28 bigmuscle Exp $
 */
