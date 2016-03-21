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
#include <regex>

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
#include "MainFrm.h"
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
#include "../client/MappingManager.h"

HubFrame::FrameMap HubFrame::g_frames;
CriticalSection HubFrame::g_frames_cs;

#ifdef SCALOLAZ_HUB_SWITCH_BTN
HIconWrapper HubFrame::g_hSwitchPanelsIco(IDR_SWITCH_PANELS_ICON);
#endif

#ifdef SCALOLAZ_HUB_MODE
HIconWrapper HubFrame::g_hModeActiveIco(IDR_MODE_ACTIVE_ICO);
HIconWrapper HubFrame::g_hModePassiveIco(IDR_MODE_PASSIVE_ICO);
HIconWrapper HubFrame::g_hModeNoneIco(IDR_MODE_OFFLINE_ICO);
#endif

int HubFrame::g_columnSizes[] = { 100,    // COLUMN_NICK
                                  75,     // COLUMN_SHARED
                                  150,    // COLUMN_EXACT_SHARED
                                  50,     // COLUMN_ANTIVIRUS
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
                                  10,     // COLUMN_MESSAGES
#endif
#ifdef IRAINMAN_INCLUDE_FULL_USER_INFORMATION_ON_HUB
#ifdef PPA_INCLUDE_DNS
                                  100,    // COLUMN_DNS
#endif
#endif
                                  300,     // COLUMN_CID
                                  200,      // COLUMN_TAG
                                  40      // COLUMN_P2P_GUARD
#ifdef FLYLINKDC_USE_EXT_JSON
                                  , 20   // FLY_HUB_GENDER
                                  , 50   // COLUMN_FLY_HUB_COUNTRY
                                  , 50   // COLUMN_FLY_HUB_CITY
                                  , 50   // COLUMN_FLY_HUB_ISP
                                  , 50   // COLUMN_FLY_HUB_COUNT_FILES
                                  , 100  // COLUMN_FLY_HUB_LAST_SHARE_DATE
                                  , 100   // COLUMN_FLY_HUB_RAM
                                  , 100   // COLUMN_FLY_HUB_SQLITE_DB_SIZE
                                  , 100   // COLUMN_FLY_HUB_QUEUE
                                  , 100   // COLUMN_FLY_HUB_TIMES
                                  , 100   // COLUMN_FLY_HUB_SUPPORT_INFO
#endif
                                }; // !SMT!-IP

int HubFrame::g_columnIndexes[] = { COLUMN_NICK,
                                    COLUMN_SHARED,
                                    COLUMN_EXACT_SHARED,
                                    COLUMN_ANTIVIRUS,
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
                                    COLUMN_MESSAGES,
#endif
#ifdef PPA_INCLUDE_DNS
                                    COLUMN_DNS, // !SMT!-IP
#endif
//[-]PPA        COLUMN_PK
                                    COLUMN_CID,
                                    COLUMN_TAG,
                                    COLUMN_P2P_GUARD
#ifdef FLYLINKDC_USE_EXT_JSON
                                    , COLUMN_FLY_HUB_GENDER
                                    , COLUMN_FLY_HUB_COUNTRY
                                    , COLUMN_FLY_HUB_CITY
                                    , COLUMN_FLY_HUB_ISP
                                    , COLUMN_FLY_HUB_COUNT_FILES
                                    , COLUMN_FLY_HUB_LAST_SHARE_DATE
                                    , COLUMN_FLY_HUB_RAM
                                    , COLUMN_FLY_HUB_SQLITE_DB_SIZE
                                    , COLUMN_FLY_HUB_QUEUE
                                    , COLUMN_FLY_HUB_TIMES
                                    , COLUMN_FLY_HUB_SUPPORT_INFO
#endif
                                  };

static ResourceManager::Strings g_columnNames[] = { ResourceManager::NICK,            // COLUMN_NICK
                                                    ResourceManager::SHARED,          // COLUMN_SHARED
                                                    ResourceManager::EXACT_SHARED,    // COLUMN_EXACT_SHARED
                                                    ResourceManager::ANTIVIRUS,       // COLUMN_ANTIVIRUS
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
                                                    ResourceManager::MESSAGES_COUNT,    // COLUMN_MESSAGES
#endif
#ifdef PPA_INCLUDE_DNS
                                                    ResourceManager::DNS_BARE,        // COLUMN_DNS // !SMT!-IP
#endif
//[-]PPA  ResourceManager::PK
                                                    ResourceManager::CID,             // COLUMN_CID
                                                    ResourceManager::TAG,             // COLUMN_TAG
                                                    ResourceManager::P2P_GUARD,       // COLUMN_P2P_GUARD
#ifdef FLYLINKDC_USE_EXT_JSON
                                                    ResourceManager::FLY_HUB_GENDER, // COLUMN_FLY_HUB_GENDER
                                                    ResourceManager::FLY_HUB_COUNTRY, // COLUMN_FLY_HUB_COUNTRY
                                                    ResourceManager::FLY_HUB_CITY,   // ,COLUMN_FLY_HUB_CITY
                                                    ResourceManager::FLY_HUB_ISP,    // COLUMN_FLY_HUB_ISP
                                                    ResourceManager::FLY_HUB_COUNT_FILES,     // COLUMN_FLY_HUB_COUNT_FILES
                                                    ResourceManager::FLY_HUB_LAST_SHARE_DATE, // COLUMN_FLY_HUB_LAST_SHARE_DATE
                                                    ResourceManager::FLY_HUB_RAM,             // COLUMN_FLY_HUB_RAM
                                                    ResourceManager::FLY_HUB_SQLITE_DB_SIZE,   // COLUMN_FLY_HUB_SQLITE_DB_SIZE
                                                    ResourceManager::FLY_HUB_QUEUE, // COLUMN_FLY_HUB_QUEUE
                                                    ResourceManager::FLY_HUB_TIMES, // COLUMN_FLY_HUB_TIMES
                                                    ResourceManager::FLY_HUB_SUPPORT_INFO // COLUMN_FLY_HUB_SUPPORT_INFO
#endif
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

HubFrame::HubFrame(bool p_is_auto_connect,
                   const string& aServer,
                   const string& aName,
                   const string& aRawOne,
                   const string& aRawTwo,
                   const string& aRawThree,
                   const string& aRawFour,
                   const string& aRawFive,
                   int  p_ChatUserSplit,
                   bool p_UserListState,
                   bool p_SuppressChatAndPM
                   //bool p_ChatUserSplitState
                  ) :
	CFlyTimerAdapter(m_hWnd)
	, CFlyTaskAdapter(m_hWnd)
	, m_client(nullptr)
	, m_ctrlUsers(nullptr)
	, m_second_count(60)
	, m_upnp_message_tick(10)
	, m_hub_name_update_count(0)
	, m_reconnect_count(0)
	, m_is_hub_name_updated(false)
	, m_is_first_goto_end(false)
	, m_waitingForPW(false)
	, m_password_do_modal(0)
	, m_needsUpdateStats(false)
	//, m_is_op_chat_opened(false)
	, m_needsResort(false)
	, m_is_init_load_list_view(false)
	, m_count_init_insert_list_view(0)
	, m_last_count_resort(0)
	, m_showUsersContainer(nullptr)
	, m_ctrlFilterContainer(nullptr)
	, m_ctrlChatContainer(nullptr)
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
	, m_isTabMenuShown(false)
	, m_showJoins(false)
	, m_favShowJoins(false)
	, m_isUpdateColumnsInfoProcessed(false)
	, m_tabMenu(nullptr)
	, m_ActivateCounter(0)
	, m_is_window_text_update(0)
	, m_Theme(nullptr)
	, m_is_process_disconnected(false)
#ifdef FLYLINKDC_USE_SKULL_TAB
	, m_virus_icon_index(0)
	, m_is_red_virus_icon_index(false)
#endif
	, m_is_ddos_detect(false)
	, m_is_ext_json_hub(false)
	, m_count_speak(0)
{
	m_userMapCS = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
	m_ctrlStatusCache.resize(5);
	m_showUsersStore = p_UserListState;
	m_showUsers = false;
	m_server = aServer;
	CFlyServerConfig::getAlternativeHub(m_server);
	m_client = ClientManager::getInstance()->getClient(m_server, p_is_auto_connect);
	m_nProportionalPos = p_ChatUserSplit;
	m_client->setName(aName);
	m_client->setRawOne(aRawOne);
	m_client->setRawTwo(aRawTwo);
	m_client->setRawThree(aRawThree);
	m_client->setRawFour(aRawFour);
	m_client->setRawFive(aRawFive);
	m_client->setSuppressChatAndPM(p_SuppressChatAndPM);
	m_client->addListener(this);
}

void HubFrame::doDestroyFrame()
{
	destroyUserMenu();
	destroyTabMenu();
	destroyMessagePanel(true);
}
void HubFrame::destroyTabMenu()
{
	safe_delete(m_tabMenu);
}

HubFrame::~HubFrame()
{
	erase_frame("");
	safe_delete(m_ctrlChatContainer);
	ClientManager::getInstance()->putClient(m_client);
	m_client = nullptr;
	// На форварде падает
	// dcassert(g_frames.find(server) != g_frames.end());
	// dcassert(g_frames[server] == this);
	dcassert(m_userMap.empty());
	safe_delete(m_ctrlUsers);
}
void HubFrame::createCtrlUsers()
{
	if (m_ctrlUsers == nullptr)
	{
		m_ctrlUsers = new CtrlUsers;
		m_ctrlUsers->Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                    WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_STATICEDGE, IDC_USERS);
		SET_EXTENDENT_LIST_VIEW_STYLE_PTR(m_ctrlUsers);
		init_gender_imagelist();
	}
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
	
	ctrlClient.setHubParam(m_client->getHubUrl(), m_client->getMyNick()); // !SMT!-S
	
	// TODO - отложить создание контрола...
// TODO - может колонки не создвать пока они не нужны?
	bHandled = FALSE;
	m_client->connect();
	const FavoriteHubEntry* fe = FavoriteManager::getFavoriteHubEntry(m_client->getHubUrl());
	const auto l_is_favorite_active = ClientManager::isActive(fe);
	LogManager::message("Connect: " + m_client->getHubUrl() + string(" Mode: ") +
	                    (m_client->isActive() ? ("Active" + (l_is_favorite_active ? string("(favorites)") : string())) : "Passive") + string(" Support: ") +
		                    MappingManager::getPortmapInfo(true, true));
		                    
#ifdef RIP_USE_CONNECTION_AUTODETECT
	ConnectionManager::getInstance()->addListener(this);
#endif
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME
	create_timer(1000, 2);
#endif
	return 1;
}
void HubFrame::updateColumnsInfo(const FavoriteHubEntry *p_fhe)
{
	if (!m_isUpdateColumnsInfoProcessed) // Апдейт колонок делаем только один раз при первой активации т.к. ListItem не разрушается
	{
		m_isUpdateColumnsInfoProcessed = true;
		FavoriteManager::getInstance()->addListener(this);
		SettingsManager::getInstance()->addListener(this);
		BOOST_STATIC_ASSERT(_countof(g_columnSizes) == COLUMN_LAST);
		BOOST_STATIC_ASSERT(_countof(g_columnNames) == COLUMN_LAST);
		for (size_t j = 0; j < COLUMN_LAST; ++j)
		{
			const int fmt = (j == COLUMN_SHARED || j == COLUMN_EXACT_SHARED || j == COLUMN_SLOTS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
			m_ctrlUsers->InsertColumn(j, TSTRING_I(g_columnNames[j]), fmt, g_columnSizes[j], j); //-V107
		}
		m_ctrlUsers->setColumnOwnerDraw(COLUMN_GEO_LOCATION);
		m_ctrlUsers->setColumnOwnerDraw(COLUMN_IP);
		m_ctrlUsers->setColumnOwnerDraw(COLUMN_UPLOAD);
		m_ctrlUsers->setColumnOwnerDraw(COLUMN_DOWNLOAD);
		m_ctrlUsers->setColumnOwnerDraw(COLUMN_MESSAGES);
		m_ctrlUsers->setColumnOwnerDraw(COLUMN_ANTIVIRUS);
		m_ctrlUsers->setColumnOwnerDraw(COLUMN_P2P_GUARD);
#ifdef FLYLINKDC_USE_EXT_JSON
		//m_ctrlUsers->setColumnOwnerDraw(COLUMN_FLY_HUB_COUNTRY);
		//m_ctrlUsers->setColumnOwnerDraw(COLUMN_FLY_HUB_CITY);
		//m_ctrlUsers->setColumnOwnerDraw(COLUMN_FLY_HUB_ISP);
		// m_ctrlUsers->setColumnOwnerDraw(COLUMN_FLY_HUB_GENDER);
#endif
		// m_ctrlUsers->SetCallbackMask(m_ctrlUsers->GetCallbackMask() | LVIS_STATEIMAGEMASK);
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
			m_ctrlUsers->SetColumnWidth(j, g_columnSizes[j]);
		}
#ifndef FLYLINKDC_USE_ANTIVIRUS_DB
		m_ctrlUsers->SetColumnWidth(COLUMN_ANTIVIRUS, 0);
#endif
		
		m_ctrlUsers->setColumnOrderArray(COLUMN_LAST, g_columnIndexes);
		
		if (p_fhe)
		{
			m_ctrlUsers->setVisible(p_fhe->getHeaderVisible());
		}
		else
		{
			m_ctrlUsers->setVisible(SETTING(HUBFRAME_VISIBLE));
		}
		
		SET_LIST_COLOR_PTR(m_ctrlUsers);
		m_ctrlUsers->setFlickerFree(Colors::g_bgBrush);
		// m_ctrlUsers->setSortColumn(-1); // TODO - научится сортировать после активации фрейма а не в начале
		if (p_fhe && p_fhe->getHeaderSort() >= 0)
		{
			m_ctrlUsers->setSortColumn(p_fhe->getHeaderSort());
			m_ctrlUsers->setAscending(p_fhe->getHeaderSortAsc());
		}
		else
		{
			m_ctrlUsers->setSortColumn(SETTING(HUBFRAME_COLUMNS_SORT));
			m_ctrlUsers->setAscending(BOOLSETTING(HUBFRAME_COLUMNS_SORT_ASC));
		}
		m_ctrlUsers->SetImageList(g_userImage.getIconList(), LVSIL_SMALL);
		m_Theme = GetWindowTheme(m_ctrlUsers->m_hWnd);
		initShowJoins(p_fhe);
	}
}
void HubFrame::on(ClientListener::FirstExtJSON, const Client*) noexcept
{
	m_is_ext_json_hub = true;
	init_gender_imagelist();
}
void HubFrame::initShowJoins(const FavoriteHubEntry *p_fhe)
{
	m_showJoins = p_fhe ? p_fhe->getShowJoins() : BOOLSETTING(SHOW_JOINS);
	m_favShowJoins = BOOLSETTING(FAV_SHOW_JOINS);
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
	if (m_ctrlFilter == nullptr && ClientManager::isStartup() == false)
	{
		++m_ActivateCounter;
		createCtrlUsers();
		BaseChatFrame::createMessageCtrl(this, EDIT_MESSAGE_MAP, isSupressChatAndPM());
		dcassert(!m_ctrlFilterContainer);
		m_ctrlFilterContainer    = new CContainedWindow(WC_EDIT, this, FILTER_MESSAGE_MAP);
		m_ctrlFilter = new CEdit;
		m_ctrlFilter->Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
		                     ES_AUTOHSCROLL, WS_EX_CLIENTEDGE);
		m_ctrlFilterContainer->SubclassWindow(m_ctrlFilter->m_hWnd);
		m_ctrlFilter->SetFont(Fonts::g_systemFont); // [~] Sergey Shushknaov
		if (!m_filter.empty())
		{
			m_ctrlFilter->SetWindowTextW(m_filter.c_str());
		}
		
		m_ctrlFilterSelContainer = new CContainedWindow(WC_COMBOBOX, this, FILTER_MESSAGE_MAP);
		m_ctrlFilterSel = new CComboBox;
		m_ctrlFilterSel->Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_HSCROLL |
		                        WS_VSCROLL | CBS_DROPDOWNLIST, WS_EX_CLIENTEDGE);
		m_ctrlFilterSelContainer->SubclassWindow(m_ctrlFilterSel->m_hWnd);
		m_ctrlFilterSel->SetFont(Fonts::g_systemFont); // [~] Sergey Shuhkanov
		
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
		m_ctrlSwitchPanels->SetFont(Fonts::g_systemFont);
		m_ctrlSwitchPanels->SetIcon(g_hSwitchPanelsIco);
		m_switchPanelsContainer->SubclassWindow(m_ctrlSwitchPanels->m_hWnd);
		m_tooltip_hubframe->AddTool(*m_ctrlSwitchPanels, ResourceManager::CMD_SWITCHPANELS);
#endif
		m_ctrlShowUsers = new CButton;
		m_ctrlShowUsers->Create(m_ctrlStatus->m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
		m_ctrlShowUsers->SetButtonStyle(BS_AUTOCHECKBOX, false);
		m_ctrlShowUsers->SetFont(Fonts::g_systemFont);
		setShowUsersCheck();
		
		m_showUsersContainer = new CContainedWindow(WC_BUTTON, this, EDIT_MESSAGE_MAP);
		m_showUsersContainer->SubclassWindow(m_ctrlShowUsers->m_hWnd);
		m_tooltip_hubframe->AddTool(*m_ctrlShowUsers, ResourceManager::CMD_USERLIST);
#ifdef SCALOLAZ_HUB_MODE
		m_ctrlShowMode = new CStatic;
		m_ctrlShowMode->Create(m_ctrlStatus->m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SS_ICON | BS_CENTER | BS_PUSHBUTTON , 0);
		//  m_ctrlShowMode->SetIcon(g_hModeActiveIco);
#endif
		dcassert(m_client->getHubUrl() == m_server);
		const FavoriteHubEntry *fhe = FavoriteManager::getFavoriteHubEntry(m_server);
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
	setCountMessages(0);
	if (!m_ctrlChatContainer && ctrlClient.m_hWnd)
	{
		m_ctrlChatContainer     = new CContainedWindow(WC_EDIT, this, EDIT_MESSAGE_MAP);
		m_ctrlChatContainer->SubclassWindow(ctrlClient.m_hWnd);
	}
	if (l_is_need_update)
	{
#ifdef SCALOLAZ_HUB_MODE
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
		if (!l_is_shutdown && m_closed == false && m_before_close == false)
		{
			WinUtil::GetWindowText(m_filter, *m_ctrlFilter);
			m_FilterSelPos = m_ctrlFilterSel->GetCurSel();
		}
		safe_destroy_window(m_tooltip_hubframe); // использует m_ctrlSwitchPanels и m_ctrlShowUsers и m_ctrlShowMode
#ifdef SCALOLAZ_HUB_MODE
		safe_destroy_window(m_ctrlShowMode);
#endif
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

void HubFrame::onBeforeActiveTab(HWND aWnd)
{
	dcassert(m_hWnd);
	if (ClientManager::isStartup() == false)
	{
		CFlyLock(g_frames_cs);
		for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i) // TODO помнить последний и не перебирать все для разрушения.
		{
			auto& l_frame = i->second;
			if (!l_frame->isClosedOrShutdown())
			{
				i->second->destroyMessagePanel(false);
			}
			else
			{
				//      dcassert(0);
			}
		}
	}
}

void HubFrame::onAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isShutdown())
	{
		dcassert(m_hWnd);
		createMessagePanel();
	}
}
void HubFrame::onInvalidateAfterActiveTab(HWND aWnd)
{
	if (!ClientManager::isShutdown())
	{
		if (ClientManager::isStartup() == false)
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

HubFrame* HubFrame::openWindow(bool p_is_auto_connect,
                               const string& p_server,
                               const string& p_name,
                               const string& p_rawOne,
                               const string& p_rawTwo,
                               const string& p_rawThree,
                               const string& p_rawFour,
                               const string& p_rawFive,
                               int  p_windowposx,
                               int  p_windowposy,
                               int  p_windowsizex,
                               int  p_windowsizey,
                               int  p_windowtype,
                               int  p_ChatUserSplit,
                               bool p_UserListState,
                               bool p_SuppressChatAndPM
                               // bool p_ChatUserSplitState,
                               //const string& p_ColumsOrder,
                               //const string& p_ColumsWidth,
                               //const string& p_ColumsVisible
                              )
{
	CFlyLock(g_frames_cs);
	HubFrame* frm;
	const auto i = g_frames.find(p_server);
	if (i == g_frames.end())
	{
		frm = new HubFrame(p_is_auto_connect,
		                   p_server,
		                   p_name,
		                   p_rawOne,
		                   p_rawTwo,
		                   p_rawThree,
		                   p_rawFour,
		                   p_rawFive,
		                   p_ChatUserSplit,
		                   p_UserListState,
		                   p_SuppressChatAndPM
		                   //, p_ChatUserSplitState
		                  );
		CRect rc = frm->rcDefault;
		rc.left   = p_windowposx;
		rc.top    = p_windowposy;
		rc.right  = rc.left + p_windowsizex;
		rc.bottom = rc.top + p_windowsizey;
		if (rc.left < 0 || rc.top < 0 || (rc.right - rc.left < 10) || (rc.bottom - rc.top) < 10)
		{
			CRect l_rcmdiClient;
			::GetWindowRect(WinUtil::g_mdiClient, &l_rcmdiClient);
			rc = l_rcmdiClient; // frm->rcDefault;
		}
		frm->CreateEx(WinUtil::g_mdiClient, rc);
		if (p_windowtype)
		{
			frm->ShowWindow(p_windowtype);
		}
		g_frames.insert(make_pair(p_server, frm));
	}
	else
	{
		frm = i->second;
		if (::IsIconic(frm->m_hWnd))
			::ShowWindow(frm->m_hWnd, SW_RESTORE);
		frm->MDIActivate(frm->m_hWnd);
	}
	frm->setCustomVIPIcon();
	return frm;
}
void HubFrame::setCustomVIPIcon()
{
	if (m_is_ddos_detect == false)
	{
		if (const auto l_index = getVIPIconIndex())
		{
			dcassert((l_index - 1) < _countof(WinUtil::g_HubFlylinkDCIconVIP));
			if ((l_index - 1) < _countof(WinUtil::g_HubFlylinkDCIconVIP))
			{
				setCustomIcon(*WinUtil::g_HubFlylinkDCIconVIP[l_index - 1].get());
			}
		}
		if (isFlySupportHub())
		{
			setCustomIcon(*WinUtil::g_HubFlylinkDCIcon.get());
		}
		else if (isFlyAntivirusHub())
		{
			setCustomIcon(*WinUtil::g_HubAntivirusIcon.get());
		}
	}
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
	
	params["hubNI"] = m_client->getHubName();
	params["hubURL"] = m_client->getHubUrl();
	params["myNI"] = m_client->getMyNick();
	
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
	ctrlClient.GoToEnd(true);
}

void HubFrame::processFrameCommand(const tstring& fullMessageText, const tstring& cmd, tstring& param, bool& resetInputMessageText)
{
	if (stricmp(cmd.c_str(), _T("join")) == 0)
	{
		if (!param.empty())
		{
			m_redirect = Util::formatDchubUrl(Text::fromT(param)); // [!] IRainman fix http://code.google.com/p/flylinkdc/issues/detail?id=1237
			if (BOOLSETTING(JOIN_OPEN_NEW_WINDOW))
			{
				openWindow(false, m_redirect); // [!] IRainman fix http://code.google.com/p/flylinkdc/issues/detail?id=1237
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
		m_client->setPassword(Text::fromT(param));
		m_client->password(Text::fromT(param));
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
		const string l_desc = MappingManager::getPortmapInfo(true, true);
		tstring l_con = _T("\r\n-=[ ") + TSTRING(IP) + _T(' ') + Text::toT(m_client->getLocalIp()) + _T(" ]=-\r\n-=[ ") + Text::toT(l_desc) + _T(" ]=-");
		
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
			const OnlineUserPtr ou = m_client->findUser(Text::fromT(nick));
			
			if (ou)
			{
				if (param.size() > j + 1)
					PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), m_client->getHubUrl()), m_client->getMyNick(), param.substr(j + 1));
				else
					PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), m_client->getHubUrl()), m_client->getMyNick());
			}
		}
		else if (!param.empty())
		{
			const OnlineUserPtr ou = m_client->findUser(Text::fromT(param));
			if (ou)
			{
				PrivateFrame::openWindow(ou, HintedUser(ou->getUser(), m_client->getHubUrl()), m_client->getMyNick());
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
		clearMessageWindow();
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
	FavoriteHubEntry* existingHub = FavoriteManager::getFavoriteHubEntry(m_client->getHubUrl());
	if (!existingHub)
	{
		FavoriteHubEntry aEntry;
		LocalArray<TCHAR, 256> buf;
		GetWindowText(buf.data(), 255);
		aEntry.setServer(m_server);
		aEntry.setName(Text::fromT(buf.data()));
		aEntry.setDescription(Text::fromT(buf.data()));
		if (!m_client->getPassword().empty())
		{
			aEntry.setNick(m_client->getMyNick());
			aEntry.setPassword(m_client->getPassword());
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
	const FavoriteHubEntry* removeHub = FavoriteManager::getFavoriteHubEntry(m_client->getHubUrl());
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
	l_tabMenu->AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)WinUtil::g_copyHubMenu, CTSTRING(COPY));
	l_tabMenu->AppendMenu(MF_STRING, ID_DISCONNECT, CTSTRING(DISCONNECT));
	l_tabMenu->AppendMenu(MF_SEPARATOR);
	l_tabMenu->AppendMenu(MF_STRING, IDC_RECONNECT_DISCONNECTED, CTSTRING(MENU_RECONNECT_DISCONNECTED));
	l_tabMenu->AppendMenu(MF_STRING, IDC_CLOSE_DISCONNECTED, CTSTRING(MENU_CLOSE_DISCONNECTED));
	l_tabMenu->AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
	l_tabMenu->AppendMenu(MF_SEPARATOR);
}

void HubFrame::autoConnectStart()
{
	const FavoriteHubEntry* existingHub = FavoriteManager::getFavoriteHubEntry(m_client->getHubUrl());
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
	FavoriteHubEntry* editedHub = FavoriteManager::getFavoriteHubEntry(m_client->getHubUrl());
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
			sCopy += m_client->getHubName();
			break;
		case IDC_COPY_HUBADDRESS:
			sCopy += m_client->getHubUrl();
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
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			case IDC_COPY_ANTIVIRUS_DB_INFO:
				sCopy += id.getVirusDesc();
				break;
#endif
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
			case IDC_COPY_GEO_LOCATION:
			{
				sCopy += Text::fromT(Util::getIpCountry(id.getIp().to_ulong()).getDescription());
				break;
			}
			case IDC_COPY_IP:
				sCopy += id.getIpAsString();
				break;
			case IDC_COPY_NICK_IP:
			{
				// TODO translate
				sCopy += "User Info:\r\n"
				         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n" +
				         "\tIP: " + Identity::formatIpString(id.getIpAsString());
				break;
			}
			case IDC_COPY_ALL:
			{
				// TODO translate
				sCopy += "User info:\r\n"
				         "\t" + STRING(NICK) + ": " + id.getNick() + "\r\n" +
				         "\tLocation: " + Text::fromT(Util::getIpCountry(id.getIp().to_ulong()).getDescription()) + "\r\n" +
				         "\tNicks: " + Util::toString(ClientManager::getNicks(u->getCID(), Util::emptyString)) + "\r\n" +
				         "\t" + STRING(HUBS) + ": " + Util::toString(ClientManager::getHubs(u->getCID(), Util::emptyString)) + "\r\n" +
				         "\t" + STRING(SHARED) + ": " + Identity::formatShareBytes(u->getBytesShared()) + (u->isNMDC() ? Util::emptyString : "(" + STRING(SHARED_FILES) +
				                 ": " + Util::toString(id.getSharedFiles()) + ")") + "\r\n" +
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
				         "\tMode: " + (id.isTcpActive() ? 'A' : 'P') + "\r\n" +
				         "\t" + STRING(SLOTS) + ": " + Util::toString(u->getSlots()) + "\r\n" +
				         "\tIP: " + Identity::formatIpString(id.getIpAsString()) + "\r\n";
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
	        /* [-] FlylinkDC allow menu in My entry: && (m_ctrlUsers->getItemData(item->iItem)->getUser() != ClientManager::getInstance()->getMe())*/
	   )
	{
		if (UserInfo* l_ui = m_ctrlUsers->getItemData(item->iItem))
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

bool HubFrame::updateUser(const OnlineUserPtr& p_ou, const int p_index_column)
{
	if (ClientManager::isShutdown() && m_client && m_client->isConnected())
	{
		dcassert(!ClientManager::isShutdown());
		return false;
	}
	dcassert(!m_is_process_disconnected);
	UserInfo* ui = findUser(p_ou); // TODO - часто ищем. связать в список?
	if (!ui)
	{
#ifdef IRAINMAN_USE_HIDDEN_USERS
		if (!p_ou->isHidden()
		        && !p_ou->isHub()) // https://code.google.com/p/flylinkdc/issues/detail?id=1535
		{
#endif
			PROFILE_THREAD_SCOPED_DESC("HubFrame::updateUser-NEW_USER")
			ui = new UserInfo(p_ou);
			{
				CFlyWriteLock(*m_userMapCS);
				//CFlyLock(m_userMapCS);
				dcassert(!m_is_process_disconnected);
				m_userMap.insert(make_pair(p_ou, ui));
			}
			if (m_showUsers)// [+] IRainman optimization
			{
				//dcassert(!client->is_all_my_info_loaded());
				if (m_client->is_all_my_info_loaded())
				{
					m_needsResort |= ui->is_update(m_ctrlUsers->getSortColumn()); // [+] IRainman fix.
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
				m_ctrlUsers->deleteItem(ui);
			}
			{
				CFlyWriteLock(*m_userMapCS);
				//CFlyLock(m_userMapCS);
				m_userMap.erase(ui->getOnlineUser());
			}
			delete ui;
			return true;
		}
		else
		{
#endif // IRAINMAN_USE_HIDDEN_USERS
			if (m_showUsers)// [+] IRainman opt.
			{
				//dcassert(!client->is_all_my_info_loaded());
				// [!] TODO if (m_client->is_all_my_info_loaded()) // TODO нельзя тут отключать иначе глючит обновления своего ника если хаб PtoX у самого себя шара = 0
				{
					PROFILE_THREAD_SCOPED_DESC("HubFrame::updateUser-update")
					m_needsResort |= ui->is_update(m_ctrlUsers->getSortColumn());
					const int pos = m_ctrlUsers->findItem(ui);
					if (pos != -1)
					{
					
					
						// Для невидимых юзеров тоже нужно апдейтить колонки (Шара/сообщения и т.д.
						// if (pos >= l_top_index && pos <= l_top_index + m_ctrlUsers->GetCountPerPage()) // TODO m_ctrlUsers->GetCountPerPage() закешировать?
						{
#if 0
#ifdef _DEBUG
							const int l_top_index = m_ctrlUsers->GetTopIndex();
							const int l_item_count = m_ctrlUsers->GetItemCount();
							
							//if (Text::toT(ui->getUser()->getLastNick()) == _T("Талисман"))
							//{
							//  LogManager::message("Талисман");
							//}
							LogManager::message("[!!!!!!!!!!!] bool HubFrame::updateUser! ui->getUser()->getLastNick() = " + ui->getUser()->getLastNick()
							                    + " top/count_per_page/all_count = " +
							                    Util::toString(l_top_index) + "/" +
							                    Util::toString(m_ctrlUsers->GetCountPerPage()) + "/" +
							                    Util::toString(l_item_count) + "  pos =" + Util::toString(pos)
							                   );
#endif
#endif
							if (p_index_column <= 0)
							{
								if (m_is_ext_json_hub)
								{
									const auto l_gender = ui->getIdentity().getGenderType();
									if (l_gender > 1)
									{
										m_ctrlUsers->SetItemState(pos, INDEXTOSTATEIMAGEMASK(l_gender), LVIS_STATEIMAGEMASK);
									}
								}
								m_ctrlUsers->updateItem(pos);
							}
							else
							{
								m_ctrlUsers->updateItem(pos, p_index_column);
							}
						}
#if 0
						else
						{
						
							LogManager::message("[///////] bool HubFrame::updateUser! ui->getUser()->getLastNick() = " + ui->getUser()->getLastNick()
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
	dcassert(!m_is_process_disconnected);
	UserInfo* ui = findUser(p_ou);
	if (!ui)
		return;
		
#ifdef IRAINMAN_USE_HIDDEN_USERS
	dcassert(ui->isHidden() == false);
#endif
	if (m_showUsers)
	{
		m_ctrlUsers->deleteItem(ui);  // Lock - redraw при закрытии?
	}
	{
		CFlyWriteLock(*m_userMapCS);
		//CFlyLock(m_userMapCS);
		m_userMap.erase(p_ou);
	}
	delete ui;
}

void HubFrame::addStatus(const tstring& aLine, const bool bInChat /*= true*/, const bool bHistory /*= true*/, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextSystem*/)
{
	if (!isSupressChatAndPM())
	{
		BaseChatFrame::addStatus(aLine, bInChat, bHistory, cf);
	}
	{
		if (!m_client->isConnected() && !m_last_hub_message.empty())
		{
			const auto l_ipT = Text::toT(m_client->getLocalIp());
			const auto l_marker_current_ip = m_last_hub_message.find(l_ipT);
			if (l_marker_current_ip != tstring::npos)
			{
				// 1. Сохраняем последнее сообщение которое приходит от бота или хаба
				// 2. Если после этого получаем дисконнект - переходим к парсингу последней мессаги хаба.
				// 3. Ищем упоминание текущего IP в строке
				// 4. Если нашли - ищем следующий IP и используем его значение при следующем подключении к этому хабу
				
				// RusHub "В поисковом запросе вы отсылаете неверный IP адрес: 127.0.0.1, ваш реальный IP: 172.23.17.18."
				// <[BOT]VerliHub> Active Search: your IP is not 10.255.252.2 but 10.64.0.135. Disconnecting
				// <VerliHub> Active Search: Your ip is not 192.168.###.### it is 192.168.###.### bye bye.
				// <VerliHub> Your reported IP: 192.168.###.### does not match your real IP: 192.168.###.###
				const string l_new_ip_string = Text::fromT(m_last_hub_message.substr(l_marker_current_ip + l_ipT.length()));
				std::smatch l_match;
				const std::regex l_reg_exp_ip(CFlyServerConfig::g_regex_find_ip);
				if (std::regex_search(l_new_ip_string, l_match, l_reg_exp_ip))
				{
					const string l_ip = l_match[0].str();
					boost::system::error_code ec;
					const auto l_ip_boost = boost::asio::ip::address_v4::from_string(l_ip, ec);
					if (!ec && Text::fromT(l_ipT) != l_ip)
					{
						m_client->getMyIdentity().setIp(l_ip);
						const string l_message = "*** FlylinkDC++ automatic change IP " + Text::fromT(l_ipT) + " to " + l_ip + " [Hub " + m_client->getHubUrlAndIP() + "] Last Message: " + Text::fromT(m_last_hub_message);
						BaseChatFrame::addStatus(Text::toT(l_message), bInChat, bHistory, cf);
						CFlyServerJSON::pushError(26, Text::fromT(m_last_hub_message));
						LogManager::message(l_message);
					}
				}
			}
		}
	}
	if (BOOLSETTING(LOG_STATUS_MESSAGES))
	{
		StringMap params;
		
		m_client->getHubIdentity().getParams(params, "hub", false);
		params["hubURL"] = m_client->getHubUrl();
		m_client->getMyIdentity().getParams(params, "my", true);
		
		params["message"] = Text::fromT(aLine);
		LOG(STATUS, params);
	}
}
void HubFrame::doConnected()
{
	m_is_process_disconnected = false;
	dcassert(!ClientManager::isShutdown());
	addStatus(TSTRING(CONNECTED), true, true, Colors::g_ChatTextServer);
	setTabColor(RGB(10, 10, 10));
	unsetIconState();
	
	ctrlClient.setHubParam(m_client->getHubUrl(), m_client->getMyNick()); // [+] IRainman fix.
	
	setStatusText(1, Text::toT(m_client->getCipherName()));
	if (m_ctrlStatus)
	{
		UpdateLayout(false);
	}
	SHOW_POPUP(POPUP_HUB_CONNECTED, Text::toT(m_client->getHubUrl()), TSTRING(CONNECTED));
	PLAY_SOUND(SOUND_HUBCON);
#ifdef SCALOLAZ_HUB_MODE
	HubModeChange();
#endif
	m_needsUpdateStats = true;
}
void HubFrame::clearTaskAndUserList()
{
	CFlyBusyBool l_busy(m_is_process_disconnected);
	clearTaskList();
	clearUserList();
}

void HubFrame::doDisconnected()
{
	dcassert(!ClientManager::isShutdown());
#ifdef FLYLINKDC_USE_SKULL_TAB
	m_virus_icon_index = 0;
#endif
	clearTaskAndUserList();
	if (!ClientManager::isShutdown())
	{
		setTabColor(RGB(128, 0, 0));
		setIconState();
		PLAY_SOUND(SOUND_HUBDISCON);
		SHOW_POPUP(POPUP_HUB_DISCONNECTED, Text::toT(m_client->getHubUrl()), TSTRING(DISCONNECTED));
#ifdef SCALOLAZ_HUB_MODE
		HubModeChange();
#endif
		m_needsUpdateStats = true;
	}
}
#if 0 // Нельзя включит - мигают часы
LRESULT HubFrame::OnSpeakerRange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled */)
{
	if (ClientManager::isShutdown())
	{
		return 0;
	}
	switch (uMsg)
	{
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
					const bool isFavorite = !FavoriteManager::isNoFavUserOrUserBanUpload(user); // [!] TODO: в ядро!
					if (isFavorite)
					{
						PLAY_SOUND(SOUND_FAVUSER);
						SHOW_POPUP(POPUP_FAVORITE_CONNECTED, id.getNickT() + _T(" - ") + Text::toT(m_client->getHubName()), TSTRING(FAVUSER_ONLINE));
					}
					
					if (!id.isBotOrHub()) // [+] IRainman fix: no show has come/gone for bots, and a hub.
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
				
				if (!id.isBotOrHub()) // [+] IRainman fix: no show has come/gone for bots, and a hub.
				{
					// !SMT!-S !SMT!-UI
					const bool isFavorite = !FavoriteManager::isNoFavUserOrUserBanUpload(user); // [!] TODO: в ядро!
					
					const tstring& l_userNick = id.getNickT();
					if (isFavorite)
					{
						PLAY_SOUND(SOUND_FAVUSER_OFFLINE);
						SHOW_POPUP(POPUP_FAVORITE_DISCONNECTED, l_userNick + _T(" - ") + Text::toT(m_client->getHubName()), TSTRING(FAVUSER_OFFLINE));
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
		break;
#endif
		
#ifdef FLYLINKDC_ADD_CHAT_LINE_USE_WIN_MESSAGES_Q
		case WM_SPEAKER_ADD_CHAT_LINE:
		{
			dcassert(!ClientManager::isShutdown());
			unique_ptr<ChatMessage> msg(reinterpret_cast<ChatMessage*>(wParam));
			if (msg->m_from)
			{
				const Identity& from    = msg->m_from->getIdentity();
				const bool myMess       = ClientManager::isMe(msg->m_from);
				addLine(from, myMess, msg->thirdPerson, Text::toT(msg->format()), Colors::g_ChatTextGeneral);
				auto& l_user = msg->m_from->getUser();
				l_user->incMessagesCount();
				speak(UPDATE_COLUMN_MESSAGE, msg->m_from);
			}
			else
			{
				BaseChatFrame::addLine(Text::toT(msg->m_text), Colors::g_ChatTextPrivate);
			}
		}
		break;
#endif // FLYLINKDC_ADD_CHAT_LINE_USE_WIN_MESSAGES_Q
		/*
		case WM_SPEAKER_CONNECTED:
		        {
		            doConnected();
		        }
		        break;
		        case WM_SPEAKER_DISCONNECTED:
		        {
		            doDisconnected();
		        }
		        break;
		*/
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
		
		
		default:
		{
			dcassert(0);
			break;
		}
	}
	return 0;
}
#endif

LRESULT HubFrame::onSpeaker(UINT /*uMsg*/, WPARAM /* wParam */, LPARAM /* lParam */, BOOL& /*bHandled*/)
{
//	dcassert(ClientManager::isStartup() == false && !ClientManager::isShutdown());
	//PROFILE_THREAD_SCOPED()
	TaskQueue::List t;
	m_tasks.get(t);
	m_count_speak = 0;
	if (t.empty())
		return 0;
#ifdef _DEBUG
	//LogManager::message("LRESULT HubFrame::onSpeaker: m_tasks.size() = " + Util::toString(t.size()));
#endif
	CFlyBusyBool l_busy(m_spoken);
	unique_ptr<CLockRedraw < > > l_lock_redraw;
	if (m_ctrlUsers)
	{
		l_lock_redraw = unique_ptr<CLockRedraw < > >(new CLockRedraw<> (*m_ctrlUsers));
	}
	CLockRedraw<> l_lock_redraw_chat(ctrlClient);
	for (auto i = t.cbegin(); i != t.cend(); ++i)
	{
		if (!ClientManager::isShutdown())
		{
			switch (i->first)
			{
				case UPADTE_COLUMN_DESC:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					m_needsUpdateStats |= updateUser(u.m_ou, COLUMN_DESCRIPTION);
				}
				break;
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
				case UPADTE_COLUMN_SHARE:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					m_needsUpdateStats |= updateUser(u.m_ou, COLUMN_EXACT_SHARED);
					m_needsUpdateStats |= updateUser(u.m_ou, COLUMN_SHARED); // TODO  передать второй параметр
				}
				break;
#endif
				case UPDATE_COLUMN_MESSAGE:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					m_needsUpdateStats |= updateUser(u.m_ou, COLUMN_MESSAGES);
				}
				break;
				case UPDATE_USER:
				{
					const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
					m_needsUpdateStats |= updateUser(u.m_ou, 0);
				}
				break;
				
#ifndef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
				case UPDATE_USER_JOIN:
				{
					if (!ClientManager::isShutdown() && m_client && m_client->isConnected())
					{
						const OnlineUserTask& u = static_cast<OnlineUserTask&>(*i->second);
						if (updateUser(u.m_ou, -1))
						{
							const Identity& id = u.m_ou->getIdentity();
							if (m_client->is_all_my_info_loaded())
							{
								dcassert(!id.getNickT().empty());
								const bool isFavorite = !FavoriteManager::isNoFavUserOrUserBanUpload(u.m_ou->getUser()); // [!] TODO: в ядро!
								if (isFavorite)
								{
									PLAY_SOUND(SOUND_FAVUSER);
									SHOW_POPUP(POPUP_FAVORITE_CONNECTED, id.getNickT() + _T(" - ") + Text::toT(m_client->getHubName()), TSTRING(FAVUSER_ONLINE));
								}
								if (!id.isBotOrHub()) // [+] IRainman fix: no show has come/gone for bots, and a hub.
								{
									if (m_showJoins || (m_favShowJoins && isFavorite))
									{
										BaseChatFrame::addLine(_T("*** ") + TSTRING(JOINS) + _T(' ') + id.getNickT(), Colors::g_ChatTextSystem);
									}
								}
								m_needsUpdateStats = true;
							}
							else
							{
								m_needsUpdateStats = false;
							}
							// Automatically open "op chat"
							//if (!m_is_op_chat_opened)
							if (m_client->isInOperatorList(id.getNick()) && !PrivateFrame::isOpen(u.m_ou->getUser()))
							{
								PrivateFrame::openWindow(u.m_ou, HintedUser(u.m_ou->getUser(), m_client->getHubUrl()), m_client->getMyNick());
								//m_is_op_chat_opened = true;
							}
						}
						else
						{
							m_needsUpdateStats |= m_client->isChangeAvailableBytes();
						}
					}
				}
				break;
				
#endif // FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
#ifndef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
				case REMOVE_USER:
				{
					const OnlineUserTask& u = static_cast<const OnlineUserTask&>(*i->second);
					//dcassert(!ClientManager::isShutdown());
					dcassert(m_is_process_disconnected == false);
					if (m_is_process_disconnected == false)
					{
						if (!ClientManager::isShutdown())
						{
							const UserPtr& user = u.m_ou->getUser();
							const Identity& id = u.m_ou->getIdentity();
							
							if (!id.isBotOrHub()) // [+] IRainman fix: no show has come/gone for bots, and a hub.
							{
								// !SMT!-S !SMT!-UI
								const bool isFavorite = !FavoriteManager::isNoFavUserOrUserBanUpload(user); // [!] TODO: в ядро!
								
								const tstring& l_userNick = id.getNickT();
								if (isFavorite)
								{
									PLAY_SOUND(SOUND_FAVUSER_OFFLINE);
									SHOW_POPUP(POPUP_FAVORITE_DISCONNECTED, l_userNick + _T(" - ") + Text::toT(m_client->getHubName()), TSTRING(FAVUSER_OFFLINE));
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
				}
				break;
#endif // FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
#ifndef FLYLINKDC_ADD_CHAT_LINE_USE_WIN_MESSAGES_Q
				case ADD_CHAT_LINE:
				{
					dcassert(!ClientManager::isShutdown());
					if (!ClientManager::isShutdown())
					{
						MessageTask& l_task = static_cast<MessageTask&>(*i->second);
						auto_ptr<ChatMessage> msg(l_task.m_message_ptr);
						l_task.m_message_ptr = nullptr;
						if (msg->m_from && !ClientManager::isShutdown())
						{
							const Identity& from = msg->m_from->getIdentity();
							const bool myMess = ClientManager::isMe(msg->m_from);
							addLine(from, myMess, msg->thirdPerson, Text::toT(msg->format()), Colors::g_ChatTextGeneral);
							auto& l_user = msg->m_from->getUser();
							l_user->incMessagesCount();
							m_client->incMessagesCount();
							speak(UPDATE_COLUMN_MESSAGE, msg->m_from);
							// msg->m_from->getUser()->flushRatio();
						}
						else
						{
							BaseChatFrame::addLine(Text::toT(msg->m_text), Colors::g_ChatTextPrivate);
						}
					}
				}
				break;
#endif // FLYLINKDC_ADD_CHAT_LINE_USE_WIN_MESSAGES_Q
				case CONNECTED:
				{
					doConnected();
				}
				break;
				case DISCONNECTED:
				{
					doDisconnected();
				}
				break;
				case ADD_STATUS_LINE:
				{
					dcassert(!ClientManager::isShutdown());
					if (!ClientManager::isShutdown())
					{
						//              PROFILE_THREAD_SCOPED_DESC("ADD_STATUS_LINE")
						const StatusTask& status = static_cast<StatusTask&>(*i->second);
						addStatus(Text::toT(status.m_str), status.m_isInChat, true, Colors::g_ChatTextServer);
					}
				}
				break;
				case STATS:
				{
					dcassert(!ClientManager::isShutdown());
					if (m_client && m_client->is_all_my_info_loaded() == true)
					{
						//              PROFILE_THREAD_SCOPED_DESC("STATS")
						const int64_t l_availableBytes = m_client->getAvailableBytes();
						const size_t l_allUsers = m_client->getUserCount();
						const size_t l_count_item = m_ctrlUsers ? m_ctrlUsers->GetItemCount() : 0;
						const size_t l_shownUsers = m_ctrlUsers ? l_count_item : l_allUsers;
						const size_t l_diff = l_allUsers - l_shownUsers;
						setStatusText(2, (Util::toStringW(l_shownUsers) + (l_diff ? (_T('/') + Util::toStringW(l_allUsers)) : Util::emptyStringT) + _T(' ') + TSTRING(HUB_USERS)));
						setStatusText(3, Util::formatBytesW(l_availableBytes));
						setStatusText(4, l_allUsers ? (Util::formatBytesW(l_availableBytes / l_allUsers) + _T('/') + TSTRING(USER)) : Util::emptyStringT);
#ifdef _DEBUG
						if (l_count_item <= m_last_count_resort)
						{
							//LogManager::message("Skip resort! Hub = " + m_client->getHubUrl() + " count = " + Util::toString(l_count_item));
						}
#endif
						if (m_needsResort && m_ctrlUsers && m_ctrlStatus && l_count_item > m_last_count_resort && !MainFrame::isAppMinimized())
						{
							m_last_count_resort = l_count_item;
							m_needsResort = false;
							m_ctrlUsers->resort(); // убран ресорт если окно не активное!
#ifdef _DEBUG
							//LogManager::message("Resort! Hub = " + m_client->getHubUrl() + " count = " + Util::toString(m_ctrlUsers ? l_count_item : 0));
#endif
						}
					}
				}
				break;
				case GET_PASSWORD:
				{
					//dcassert(m_ctrlMessage);
					if (m_client->isConnected())
					{
						if (!BOOLSETTING(PROMPT_PASSWORD))
						{
							addPasswordCommand();
							m_waitingForPW = true;
						}
						else if (m_password_do_modal == 0)
						{
							if (m_client->getSuppressChatAndPM())
							{
								m_client->disconnect(false);
								m_client->fly_fire1(ClientListener::NickTaken(), m_client);
							}
							else
							{
								CFlySafeGuard<uint8_t> l_dlg_(m_password_do_modal); // fix Stack Overflow https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=103355
								LineDlg linePwd;
								linePwd.title = Text::toT(m_client->getHubName() + " (" + m_client->getHubUrl() + ')');
								linePwd.description = TSTRING(ENTER_PASSWORD) + _T(" ") + TSTRING(NICK) + Text::toT(": " + m_client->getMyNick());
								linePwd.password = true;
								if (linePwd.DoModal(m_hWnd) == IDOK)
								{
									const string l_pwd = Text::fromT(linePwd.line);
									m_client->setPassword(l_pwd);
									m_client->password(l_pwd);
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
									m_client->disconnect(false);
									m_client->fly_fire1(ClientListener::NickTaken(), m_client);
								}
							}
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
					const Identity& from = pm->m_from->getIdentity();
					const bool myPM = ClientManager::isMe(pm->m_replyTo);
					const Identity& replyTo = pm->m_replyTo->getIdentity();
					const Identity& to = pm->m_to->getIdentity();
					const tstring text = Text::toT(pm->format());
					const auto& id = myPM ? to : replyTo;
					const bool isOpen = PrivateFrame::isOpen(id.getUser());
					bool l_is_private_frame_ok = false;
					if ((BOOLSETTING(POPUP_HUB_PMS) && replyTo.isHub() ||
					        BOOLSETTING(POPUP_BOT_PMS) && replyTo.isBot() ||
					        BOOLSETTING(POPUP_PMS)
					    ) || isOpen)
					{
						l_is_private_frame_ok = PrivateFrame::gotMessage(from, to, replyTo, text, getHubHint(), myPM, pm->thirdPerson);
					}
					if (l_is_private_frame_ok == false)
					{
						BaseChatFrame::addLine(TSTRING(PRIVATE_MESSAGE_FROM) + _T(' ') + id.getNickT() + _T(": ") + text, Colors::g_ChatTextPrivate);
					}
					if (!replyTo.isHub() && !replyTo.isBot())
					{
						const HWND hMainWnd = MainFrame::getMainFrame()->m_hWnd;//GetTopLevelWindow();
						::PostMessage(hMainWnd, WM_SPEAKER, MainFrame::SET_PM_TRAY_ICON, NULL); //-V106
					}
				}
				break;
#endif
				
				case CHEATING_USER:
				{
					const StatusTask& l_task = static_cast<StatusTask&>(*i->second);
					CHARFORMAT2 cf;
					memzero(&cf, sizeof(CHARFORMAT2));
					cf.cbSize = sizeof(cf);
					cf.dwMask = CFM_BACKCOLOR | CFM_COLOR | CFM_BOLD;
					cf.crBackColor = SETTING(BACKGROUND_COLOR);
					cf.crTextColor = SETTING(ERROR_COLOR);
					const tstring msg = Text::toT(l_task.m_str);
					if (msg.length() < 256)
						SHOW_POPUP(POPUP_CHEATING_USER, msg, TSTRING(CHEATING_USER));
					BaseChatFrame::addLine(msg, cf);
				}
				break;
				case USER_REPORT:
				{
					const StatusTask& l_task = static_cast<StatusTask&>(*i->second);
					BaseChatFrame::addLine(Text::toT(l_task.m_str), Colors::g_ChatTextSystem);
					if (BOOLSETTING(LOG_MAIN_CHAT))
					{
						StringMap params;
						params["message"] = l_task.m_str;
						params["hubURL"] = m_client->getHubUrl();
						LOG(CHAT, params);
					}
				}
				break;
				
				
#ifdef RIP_USE_CONNECTION_AUTODETECT
				case OPEN_TCP_PORT_DETECTED:
				{
					const string l_message = static_cast<StatusTask&>(*i->second).m_str + ": [FlylinkDC++] TCP port is open! Connection type auto-switching to active mode";
					LogManager::message(l_message);
					// BaseChatFrame::addLine(Text::toT(l_message), Colors::g_ChatTextSystem);
					// BaseChatFrame::addLine(_T("[!]FlylinkDC++ ") + _T("Detected direct connection type, switching to active mode"), WinUtil::m_ChatTextSystem);
					HubModeChange();
				}
				break;
#endif
				default:
					dcassert(0);
					break;
			};
		}
		delete i->second;
	}
	
	return 0;
}

void HubFrame::updateWindowText()
{
	if (ClientManager::isStartup() == false) // Пока конструируемся не нужно апдейтить текст
	{
		if (m_is_window_text_update)
		{
			// TODO - ограничить размер текста
			SetWindowText(Text::toT(m_window_text).c_str());
			m_is_window_text_update = 0;
			if (m_client->is_all_my_info_loaded())
			{
				SetMDIFrameMenu(); // fix http://code.google.com/p/flylinkdc/issues/detail?id=1386
			}
		}
	}
}

void HubFrame::setWindowTitle(const string& p_text)
{
	dcassert(!ClientManager::isShutdown());
	if (m_window_text != p_text || m_is_window_text_update == 0)
	{
		m_window_text = p_text;
		++m_is_window_text_update;
		updateWindowText();
	}
}

void HubFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	if (ClientManager::isStartup() == true)
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
			const int tmp = sr.Width() > 332 ? 232 : (sr.Width() > 132 ? sr.Width() - 100 : 32);
			int szCipherLen = m_ctrlStatus->GetTextLength(1);
			
			if (szCipherLen)
			{
				wstring strCipher;
				strCipher.resize(static_cast<size_t>(szCipherLen));
				m_ctrlStatus->GetText(1, &strCipher.at(0));
				szCipherLen = WinUtil::getTextWidth(strCipher, m_ctrlStatus->m_hWnd);
			}
			int HubPic = 0;
			int l_hubIcoSize;   // Ширина иконки режима
#ifdef SCALOLAZ_HUB_MODE
			if (BOOLSETTING(ENABLE_HUBMODE_PIC))
			{
				l_hubIcoSize = 22;  // Ширина иконки режима ( 16 px )
				HubPic += l_hubIcoSize;
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
					sr.right = sr.left + l_hubIcoSize;
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
	if (m_msgPanel)
	{
		int h = 0, chat_columns = 0;
		const bool bUseMultiChat = isMultiChat(h, chat_columns);
		CRect rc = rect;
		rc.bottom -= h + (Fonts::g_fontHeightPixl + 1) * int(bUseMultiChat) + 18;
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
		rc.bottom -= 4;
		rc.top = rc.bottom - h - Fonts::g_fontHeightPixl * int(bUseMultiChat) - 12;
		rc.left += 2;
		rc.right -= iButtonPanelLength + 2;
		CRect ctrlMessageRect = rc;
		if (m_ctrlMessage)
		{
			m_ctrlMessage->MoveWindow(rc);
		}
		
		if (bUseMultiChat && m_MultiChatCountLines < 2)
		{
			rc.top += h + 6;
		}
		rc.left = rc.right;
		rc.bottom -= 1;
		
		if (m_msgPanel)
		{
			m_msgPanel->UpdatePanel(rc);
		}
		rc.right  += l_panelWidth;
		rc.bottom += 1;
		
		if (m_ctrlFilter && m_ctrlFilterSel)
		{
			if (m_showUsers)
			{
				if (bUseMultiChat)
				{
					rc = ctrlMessageRect;
					rc.bottom = rc.top + 18; // [~] JhaoDa
					rc.left = rc.right + 2; // [~] Sergey Shushkanov
					rc.right += 119; // [~] Sergey Shushkanov
					rc.top -= 3;
					m_ctrlFilter->MoveWindow(rc);
					
					rc.left = rc.right + 2; // [~] Sergey Shushkanov
					rc.right = rc.left + 101; // [~] Sergey Shushkanov
					rc.top -= 1;
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
}
void HubFrame::TuneSplitterPanes()
{
	if (isSupressChatAndPM())
	{
		m_nProportionalPos = 0;
		m_isClientUsersSwitch = true;
	}
	if (m_ctrlUsers && ctrlClient.IsWindow())
	{
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		if (m_isClientUsersSwitch == true)
		{
			SetSplitterPanes(ctrlClient.m_hWnd, m_ctrlUsers->m_hWnd, false); // Чат, список пользователей.
		}
		else // Если список пользователей слева.
		{
			SetSplitterPanes(m_ctrlUsers->m_hWnd, ctrlClient.m_hWnd, false); // Список пользователей, чат.
		}
#else
		SetSplitterPanes(ctrlClient.m_hWnd, m_ctrlUsers->m_hWnd, false);
#endif
	}
}
#ifdef SCALOLAZ_HUB_MODE
void HubFrame::HubModeChange()
{
	if (BOOLSETTING(ENABLE_HUBMODE_PIC))
	{
		if (m_client->isConnected())
		{
			if (m_client->isActive())
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
						if (BOOLSETTING(FORCE_PASSIVE_INCOMING_CONNECTIONS))
						{
							// пока не понял как добавить к подсказке ещё один текст...
							//  BaseChatFrame::addLine(_T("[!] FlylinkDC++ You have enabled 'Force firewall mode (passive!)' (window transfer, left bottom checkbox with wall)") /*+ TSTRING(PASSIVE_FORCE_NOTICE) + _T(" ") */, Colors::g_ChatTextSystem);
						}
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
		m_ctrlUsers->saveHeaderOrder(l_order, l_width, l_visible);
	}
	FavoriteHubEntry *fhe = FavoriteManager::getFavoriteHubEntry(m_server);
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
			::GetWindowRect(WinUtil::g_mdiClient, &rcmdiClient);
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
			fhe->setHeaderSort(m_ctrlUsers->getSortColumn());
			fhe->setHeaderSortAsc(m_ctrlUsers->isAscending());
		}
		{
			CFlyLock(g_frames_cs);
			if (g_frames.size() == 1 || ClientManager::isStartup() == false) // Сохраняем только на последней итерации, или когда не закрываем приложение.
			{
				FavoriteManager::save();
			}
		}
	}
	else
	{
		if (m_isUpdateColumnsInfoProcessed)
		{
			SET_SETTING(HUBFRAME_ORDER, l_order);
			SET_SETTING(HUBFRAME_WIDTHS, l_width);
			SET_SETTING(HUBFRAME_VISIBLE, l_visible);
			SET_SETTING(HUBFRAME_COLUMNS_SORT, m_ctrlUsers->getSortColumn());
			SET_SETTING(HUBFRAME_COLUMNS_SORT_ASC, m_ctrlUsers->isAscending());
		}
	}
}
LRESULT HubFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	const string l_server = m_server;
	if (!m_closed)
	{
		m_closed = true;
		m_client->removeListener(this);
		erase_frame("");
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME
		safe_destroy_timer();
#endif
		clear_and_destroy_task();
		storeColumsInfo();
		if (!ClientManager::isShutdown())
		{
			RecentHubEntry* r = FavoriteManager::getRecentHubEntry(l_server);
			if (r) // hub has been removed by the user from a list of recent hubs at a time when it was opened. https://crash-server.com/Bug.aspx?ClientID=ppa&ProblemID=9897
			{
				LocalArray<TCHAR, 256> buf;
				GetWindowText(buf.data(), 255);
				r->setName(Text::fromT(buf.data()));
				r->setUsers(Util::toString(m_client->getUserCount()));
				r->setShared(Util::toString(m_client->getAvailableBytes()));
				r->setDateTime(Util::formatDigitalClock(time(NULL)));
				FavoriteManager::getInstance()->updateRecent(r);
			}
		}
// TODO     ClientManager::getInstance()->addListener(this);
#ifdef RIP_USE_CONNECTION_AUTODETECT
		ConnectionManager::getInstance()->removeListener(this);
#endif
		if (m_isUpdateColumnsInfoProcessed)
		{
			SettingsManager::getInstance()->removeListener(this);
			FavoriteManager::getInstance()->removeListener(this);
		}
		m_client->disconnect(true);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		m_tasks.lock_task();
		clearTaskAndUserList();
		bHandled = FALSE;
		return 0;
	}
}
void HubFrame::clearUserList()
{
	if (m_ctrlUsers)
	{
		CLockRedraw<> l_lock_draw(*m_ctrlUsers); // TODO это нужно или опустить ниже?
		m_ctrlUsers->DeleteAllItems();
	}
	{
		CFlyWriteLock(*m_userMapCS);
		//CFlyLock(m_userMapCS);
		for (auto i = m_userMap.cbegin(); i != m_userMap.cend(); ++i)
		{
			delete i->second; //[2] https://www.box.net/shared/202f89c842ee60bdecb9
		}
		m_userMap.clear();
	}
	m_last_count_resort = 0;
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
				PrivateFrame::openWindow(ui->getOnlineUser(), HintedUser(ui->getUser(), m_client->getHubUrl()), m_client->getMyNick());
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
						const int l_items_count = m_ctrlUsers->GetItemCount();
						int pos = -1;
						CLockRedraw<> l_lock_draw(*m_ctrlUsers);
						for (int i = 0; i < l_items_count; ++i)
						{
							if (m_ctrlUsers->getItemData(i) == ui)
								pos = i;
							m_ctrlUsers->SetItemState(i, (i == pos) ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
						}
						m_ctrlUsers->EnsureVisible(pos, FALSE);
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

void HubFrame::addLine(const Identity& p_from, const bool bMyMess, const bool bThirdPerson, const tstring& aLine, const CHARFORMAT2& cf /*= WinUtil::m_ChatTextGeneral*/)
{
	tstring extra;
	if (p_from.isBotOrHub())
	{
		m_last_hub_message = aLine;
	}
	BaseChatFrame::addLine(p_from, bMyMess, bThirdPerson, aLine, cf, extra);
	if (ClientManager::isStartup() == false)
	{
		SHOW_POPUP(POPUP_CHAT_LINE, aLine, TSTRING(CHAT_MESSAGE));
	}
	if (BOOLSETTING(LOG_MAIN_CHAT))
	{
		StringMap params;
		
		params["message"] = ChatMessage::formatNick(p_from.getNick(), bThirdPerson) + Text::fromT(aLine);
		// TODO crash "<-FTTBkhv-> Running Verlihub 1.0.0 build Fri Mar 30 2012 ][ Runtime: 5 дн. 15 час. ][ User count: 533"
		if (!extra.empty())
			params["extra"] = Text::fromT(extra);
			
		m_client->getHubIdentity().getParams(params, "hub", false);
		params["hubURL"] = m_client->getHubUrl();
		m_client->getMyIdentity().getParams(params, "my", true);
		
		LOG(CHAT, params);
	}
	if (ClientManager::isStartup() == false && BOOLSETTING(BOLD_HUB))
	{
		if (m_client->is_all_my_info_loaded())
		{
			setDirty(1);
		}
	}
}
LRESULT HubFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	CMenu hSysMenu;
	m_isTabMenuShown = true;
	OMenu* l_tabMenu = createTabMenu();
	const string& l_name = m_client->getHubName();
	l_tabMenu->InsertSeparatorFirst(Text::toT(!l_name.empty() ? (l_name.size() > 50 ? l_name.substr(0, 50) + "…" : l_name) : m_client->getHubUrl()));
	appendUcMenu(*m_tabMenu, ::UserCommand::CONTEXT_HUB, m_client->getHubUrl());
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
	m_isTabMenuShown = false;
	
	m_ctrlUsers->GetHeader().GetWindowRect(&rc);
	
	if (PtInRect(&rc, pt) && m_showUsers)
	{
		m_ctrlUsers->showMenu(pt);
		return TRUE;
	}
	
	if (reinterpret_cast<HWND>(wParam) == *m_ctrlUsers && m_showUsers)
	{
		OMenu* l_user_menu = createUserMenu();
		l_user_menu->ClearMenu();
		clearUserMenu(); // !SMT!-S
		
		if (m_ctrlUsers->GetSelectedCount() == 1)
		{
			if (pt.x == -1 && pt.y == -1)
			{
				WinUtil::getContextMenuPos(*m_ctrlUsers, pt);
			}
			int i = -1;
			i = m_ctrlUsers->GetNextItem(i, LVNI_SELECTED);
			if (i >= 0)
			{
				reinitUserMenu((m_ctrlUsers->getItemData(i))->getOnlineUser(), getHubHint()); // !SMT!-S // [!] IRainman fix.
			}
		}
		
		appendHubAndUsersItems(*l_user_menu, false);
		
		appendUcMenu(*l_user_menu, ::UserCommand::CONTEXT_USER, m_client->getHubUrl());
		
		if (!(l_user_menu->GetMenuState(m_userMenu->GetMenuItemCount() - 1, MF_BYPOSITION) & MF_SEPARATOR))
		{
			l_user_menu->AppendMenu(MF_SEPARATOR);
		}
		
		l_user_menu->AppendMenu(MF_STRING, IDC_REFRESH, CTSTRING(REFRESH_USER_LIST));
		
		if (m_ctrlUsers->GetSelectedCount() > 0)
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
			appendUcMenu(*m_userMenu, ::UserCommand::CONTEXT_USER, m_client->getHubUrl());
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
	
	m_client->getMyIdentity().getParams(ucParams, "my", true);
	m_client->getHubIdentity().getParams(ucParams, "hub", false);
	
	if (m_isTabMenuShown)
	{
		m_client->escapeParams(ucParams);
		m_client->sendUserCmd(uc, ucParams);
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
				m_client->escapeParams(tmp);
				m_client->sendUserCmd(uc, tmp);
			}
		}
		else
		{
			int sel = -1;
			while ((sel = m_ctrlUsers->GetNextItem(sel, LVNI_SELECTED)) != -1)
			{
				UserInfo *u = m_ctrlUsers->getItemData(sel);
				if (u->getUser()->isOnline())
				{
					StringMap tmp = ucParams;
					u->getIdentity().getParams(tmp, "user", true);
					m_client->escapeParams(tmp);
					m_client->sendUserCmd(uc, tmp);
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
				m_ctrlUsers->SetFocus();
				return;
			}
			const int y = m_ctrlUsers->GetItemCount();
			
			for (int x = 0; x < y; ++x)
				m_ctrlUsers->SetItemState(x, 0, LVNI_FOCUSED | LVNI_SELECTED);
		}
		
		if (textStart == string::npos)
			textStart = 0;
		else
			textStart++;
			
		int start = m_ctrlUsers->GetNextItem(-1, LVNI_FOCUSED) + 1;
		int i = start;
		const int j = m_ctrlUsers->GetItemCount();
		
		bool firstPass = i < j;
		if (!firstPass)
			i = 0;
		while (firstPass || (!firstPass && i < start))
		{
			const UserInfo* ui = m_ctrlUsers->getItemData(i);
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
					m_ctrlUsers->SetItemState(start - 1, 0, LVNI_SELECTED | LVNI_FOCUSED);
				}
				m_ctrlUsers->SetItemState(i, LVNI_FOCUSED | LVNI_SELECTED, LVNI_FOCUSED | LVNI_SELECTED);
				m_ctrlUsers->EnsureVisible(i, FALSE);
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
	const FavoriteHubEntry *fhe = FavoriteManager::getFavoriteHubEntry(m_server);
	initShowJoins(fhe);
	if ((!fhe || fhe->getNick().empty()) && SETTING(NICK).empty())
	{
		MessageBox(CTSTRING(ENTER_NICK), T_APPNAME_WITH_VERSION, MB_ICONSTOP | MB_OK);// TODO Добавить адрес хаба в сообщение
		return 0;
	}
	m_client->reconnect();
	return 0;
}

LRESULT HubFrame::onDisconnect(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_client->disconnect(true);
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
// WM_SPEAKER_FIRST_USER_JOIN
#if 0
LRESULT HubFrame::OnSpeakerFirstUserJoin(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	unique_ptr<std::vector<OnlineUserPtr> > l_user_array(reinterpret_cast<std::vector<OnlineUserPtr> * >(wParam));
	for (auto i = l_user_array->begin(); i != l_user_array->end(); ++i)
	{
		CFlyLock(m_userMapCS);
		const auto l_user_info = m_userMap.findUser(*i);
		if (l_user_info)
		{
			InsertItemInternal(ui);
		}
	}
	return 0;
}
unsigned HubFrame::usermap2ListrView()
{
	dcassert(m_ctrlUsers->GetItemCount() == 0);
	//CFlyReadLock(*m_userMapCS);
	std::vector<const UserInfo*> l_user_array;
	{
		CFlyLock(m_userMapCS);
		l_user_array.reserve(m_userMap.size());
		for (auto i = m_userMap.cbegin(); i != m_userMap.cend(); ++i)
		{
			const UserInfo* ui = i->second;
			l_user_array.push_back(ui);
#ifdef IRAINMAN_USE_HIDDEN_USERS
			dcassert(ui->isHidden() == false);
#endif
		}
	}
	for (auto i = l_user_array.begin(); i != l_user_array.end(); ++i)
	{
		InsertItemInternal(ui);
	}
}
void HubFrame::firstLoadAllUsers()
{
	//CWaitCursor l_cursor_wait;
	m_needsResort = false;
	//CLockRedraw<> l_lock_draw(ctrlUsers);
	m_userMapInitThread.process_init_user_list(this);
	if (usermap2ListrView())
	{
		// m_ctrlUsers->resort();
	}
	m_needsResort = false;
}

#endif

unsigned HubFrame::usermap2ListrView()
{
	CFlyReadLock(*m_userMapCS);
	//CFlyLock(m_userMapCS);
	m_count_init_insert_list_view = m_ctrlUsers->GetItemCount();
	CFlyBusyBool l_busy(m_is_init_load_list_view);
	for (auto i = m_userMap.cbegin(); i != m_userMap.cend(); ++i, ++m_count_init_insert_list_view)
	{
		const UserInfo* ui = i->second;
#ifdef IRAINMAN_USE_HIDDEN_USERS
		dcassert(ui->isHidden() == false);
#endif
		InsertItemInternal(ui);
	}
	return m_userMap.size();
}
void HubFrame::firstLoadAllUsers()
{
	CWaitCursor l_cursor_wait; //-V808
	m_needsResort = false;
	CLockRedraw<> l_lock_draw(*m_ctrlUsers);
	if (usermap2ListrView())
	{
		//m_ctrlUsers->resort();
	}
	m_needsResort = false;
}

LRESULT HubFrame::onHubFrmCtlColor(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	const HWND hWnd = (HWND)lParam;
	const HDC hDC = (HDC)wParam;
	if (m_ctrlFilter && hWnd == m_ctrlFilter->m_hWnd)
	{
		/*
		const auto l_length = m_ctrlFilter->GetWindowTextLength();
		if (l_length > 0 && l_length < 2)   // HighLighting on control field only for 1 or 2 symbols !
		{
		    // Revert colors fore-back
		    ::SetTextColor(hDC, SETTING(TEXT_SYSTEM_BACK_COLOR));
		    ::SetBkColor(hDC, SETTING(TEXT_SYSTEM_FORE_COLOR));
		}
		else
		*/
		{
			// Normal colors
			::SetTextColor(hDC, SETTING(TEXT_SYSTEM_FORE_COLOR));
			::SetBkColor(hDC, SETTING(TEXT_SYSTEM_BACK_COLOR));
		}
		return (LRESULT)Colors::g_bgBrush;
	}
	return BaseChatFrame::onCtlColor(uMsg, wParam, lParam, bHandled);
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
			m_ctrlUsers->DeleteAllItems();
			m_last_count_resort = 0;
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
	UpdateLayout();
}
#endif
void HubFrame::erase_frame(const string& p_redirect)
{
	CFlyLock(g_frames_cs);
	g_frames.erase(m_server);
	if (!p_redirect.empty())
	{
		g_frames.insert(make_pair(p_redirect, this));
		m_server = p_redirect;
	}
}

LRESULT HubFrame::onFollow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_redirect.empty())
	{
		if (ClientManager::isConnected(m_redirect))
		{
			addStatus(TSTRING(REDIRECT_ALREADY_CONNECTED), true, false, Colors::g_ChatTextServer);
			CFlyServerJSON::pushError(46, "HubFrame::onFollow " + getHubHint() + " -> " + m_redirect + " ALREADY CONNECTED");
			return 0;
		}
		//dcassert(g_frames.find(server) != g_frames.end());
		//dcassert(g_frames[server] == this);
		erase_frame(m_redirect);
		// the client is dead, long live the client!
		m_client->removeListener(this);
		ClientManager::getInstance()->putClient(m_client);
		m_client = nullptr;
		clearTaskAndUserList();
		m_client = ClientManager::getInstance()->getClient(m_redirect, false);
		RecentHubEntry r;
		r.setServer(m_redirect);
		FavoriteManager::getInstance()->addRecent(r);
		m_client->addListener(this);
		m_client->connect();
	}
	return 0;
}

LRESULT HubFrame::onEnterUsers(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	int item = m_ctrlUsers->GetNextItem(-1, LVNI_FOCUSED);
	if (item != -1)
	{
		try
		{
			QueueManager::getInstance()->addList(HintedUser((m_ctrlUsers->getItemData(item))->getUser(), getHubHint()), QueueItem::FLAG_CLIENT_VIEW);
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
	CFlyLock(g_frames_cs);
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		if (!i->second->isClosedOrShutdown())
		{
			i->second->resortForFavsFirst(true);
		}
	}
}

void HubFrame::closeDisconnected()
{
	CFlyLock(g_frames_cs);
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		if (!i->second->isClosedOrShutdown())
		{
			const auto l_client = i->second->m_client;
			dcassert(l_client);
			if (l_client && !l_client->isConnected())
			{
				i->second->PostMessage(WM_CLOSE);
			}
		}
	}
}

void HubFrame::reconnectDisconnected()
{
	CFlyLock(g_frames_cs);
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		if (!i->second->isClosedOrShutdown())
		{
			const auto& l_client = i->second->m_client;
			dcassert(l_client);
			if (l_client && !l_client->isConnected())
			{
				l_client->reconnect();
			}
		}
	}
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
	{
		CFlyLock(g_frames_cs);
		for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
		{
			if (!i->second->isClosedOrShutdown())
			{
				dcassert(i->second->m_client);
				if (thershold == 0 || (
				            i->second->m_client &&
				            i->second->m_client->getUserCount() <= thershold))
				{
					i->second->PostMessage(WM_CLOSE);
				}
			}
		}
	}
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

#ifndef FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME
void HubFrame::timer_process_all()
{
	CFlyLock(g_frames_cs);
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		if (!i->second->isClosedOrShutdown())
		{
			i->second->timer_process_internal(); // TODO прокинуть флаг видимости чтобы не обнвлять статус
		}
	}
}
#endif
// [+] IRainman opt.
void HubFrame::timer_process_internal()
{
	if (!m_spoken)
	{
		if (ClientManager::isStartup() == false
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME
		        && !MainFrame::isAppMinimized(m_hWnd)
#endif
		        && !isClosedOrShutdown())
		{
			onTimerHubUpdated();
			if (m_needsUpdateStats
#ifndef IRAINMAN_NOT_USE_COUNT_UPDATE_INFO_IN_LIST_VIEW_CTRL
			        && m_ctrlUsers->getCountUpdateInfo() == 0 //[+]FlylinkDC++
#endif
			   )
			{
				dcassert(m_client);
#ifdef FLYLINKDC_USE_SKULL_TAB
				if (m_client)
				{
					const auto l_count_virus_bot = m_client->getVirusBotCount();
					if (m_virus_icon_index && l_count_virus_bot == 0)
					{
						m_virus_icon_index = 0;
						flickerVirusIcon();
					}
					else if (m_virus_icon_index == 0 && m_client->is_all_my_info_loaded())
					{
						if (l_count_virus_bot > 1)
						{
							if (l_count_virus_bot < 10)
							{
								m_virus_icon_index = 1;
							}
							else
							{
								m_virus_icon_index = 3;
							}
							flickerVirusIcon();
						}
					}
				}
#endif
				
				//dcdebug("HubFrame::timer_process_internal() [2] m_needsUpdateStats Hub = %s\n", this->getHubHint().c_str());
				dcassert(!ClientManager::isShutdown());
				speak(STATS);
				m_needsUpdateStats = false;
#if 0
				if (!m_is_first_goto_end)
				{
					m_is_first_goto_end = true;
					ctrlClient.GoToEnd(true); // Пока не пашет и не появляется скроллер
				}
#endif
			}
		}
		if (!m_tasks.empty() && !ClientManager::isShutdown())
		{
			//dcdebug("HubFrame::timer_process_internal() [3] force_speak Hub = %s\n", this->getHubHint().c_str());
			force_speak();
		}
	}
	if (--m_second_count == 0)
	{
		m_second_count = 60;
		ClientManager::infoUpdated(m_client);
	}
	if (m_upnp_message_tick > 0)
	{
		if (--m_upnp_message_tick == 0 && !ClientManager::isShutdown() && m_client && !m_client->isActive())
		{
			m_upnp_message_tick = -1;
			BaseChatFrame::addLine(_T("[!] FlylinkDC++ ") + TSTRING(PASSIVE_NOTICE) + _T(" ") + Text::toT(CFlyServerConfig::g_support_upnp), Colors::g_ChatTextSystem);
		}
	}
}
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_FOR_HUBFRAME
LRESULT HubFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (!ClientManager::isShutdown())
	{
		timer_process_internal();
	}
	return 0;
}
#endif

void HubFrame::on(Connecting, const Client*) noexcept
{
#ifdef RIP_USE_CONNECTION_AUTODETECT
	bool bWantAutodetect = false;
	if (!ClientManager::isActive(FavoriteManager::getFavoriteHubEntry(m_client->getHubUrl()), &bWantAutodetect))
	{
		if (bWantAutodetect)
		{
			// BaseChatFrame::addLine(_T("[!]FlylinkDC++ Detecting connection type: work in passive mode until direct mode is detected"), Colors::g_ChatTextSystem);
		}
	}
#endif
	const auto l_url_hub = m_client->getHubUrl();
	// speak(ADD_STATUS_LINE, STRING(CONNECTING_TO) + ' ' + l_url_hub + " ...");
	// force_speak();
	addStatus(Text::toT(STRING(CONNECTING_TO) + ' ' + l_url_hub + " ..."));
	// Явно звать addStatus нельзя - вешаемся почему-то
	// http://code.google.com/p/flylinkdc/issues/detail?id=1428
	++m_hub_name_update_count;
}
void HubFrame::on(ClientListener::Connected, const Client* c) noexcept
{

#if 0
	if (!m_client->isActive())
	{
		BaseChatFrame::addLine(_T("[!] FlylinkDC++ ") + TSTRING(PASSIVE_NOTICE) + _T(" ") + WinUtil::GetWikiLink() + L"connection_settings", Colors::g_ChatTextSystem);
	}
	else if (BOOLSETTING(SEARCH_PASSIVE))
	{
		BaseChatFrame::addLine(_T("[!] FlylinkDC++ ") + TSTRING(PASSIVE_SEARCH_NOTICE) + _T(" ") + WinUtil::GetWikiLink() + L"advanced", Colors::g_ChatTextSystem);
	}
	if (BOOLSETTING(FORCE_PASSIVE_INCOMING_CONNECTIONS))
	{
		BaseChatFrame::addLine(_T("[!] FlylinkDC++ You have enabled 'Force firewall mode (passive!)' (window transfer, left bottom checkbox with wall)") /*+ TSTRING(PASSIVE_FORCE_NOTICE) + _T(" ") */, Colors::g_ChatTextSystem);
	}
#endif
	
	speak(CONNECTED);
	//PostMessage(WM_SPEAKER_CONNECTED);
#ifdef FLYLINKDC_USE_CHAT_BOT
	ChatBot::getInstance()->onHubAction(BotInit::RECV_CONNECT, c->getHubUrl());
#endif
}

void HubFrame::on(ClientListener::DDoSSearchDetect, const string&) noexcept
{
	dcassert(!ClientManager::isShutdown());
	if (m_is_ddos_detect == false && !ClientManager::isShutdown())
	{
		setCustomIcon(*WinUtil::g_HubDDoSIcon.get());
		m_is_ddos_detect = true;
	}
}
void HubFrame::flickerVirusIcon()
{
#ifdef FLYLINKDC_USE_SKULL_TAB
	dcassert(!ClientManager::isShutdown());
	if (!isClosedOrShutdown())
	{
		if (m_is_ddos_detect == false)
		{
			if (m_virus_icon_index)
			{
				const auto l_index = m_virus_icon_index - (m_is_red_virus_icon_index ? 1 : 0);
				setCustomIcon(*WinUtil::g_HubVirusIcon[l_index].get());
				m_is_red_virus_icon_index = !m_is_red_virus_icon_index;
			}
			else
			{
				setCustomVIPIcon();
			}
		}
	}
#endif
}
void HubFrame::on(ClientListener::UserDescUpdated, const OnlineUserPtr& user) noexcept
{
	dcassert(!ClientManager::isShutdown());
	if (!isClosedOrShutdown())
	{
		speak(UPADTE_COLUMN_DESC, user);
	}
}
#ifdef FLYLINKDC_USE_CHECK_CHANGE_MYINFO
void HubFrame::on(ClientListener::UserShareUpdated, const OnlineUserPtr& user) noexcept
{
	dcassert(!ClientManager::isShutdown());
	if (!isClosedOrShutdown())
	{
		speak(UPADTE_COLUMN_SHARE, user);
	}
}
#endif

void HubFrame::on(ClientListener::UserUpdatedMyINFO, const OnlineUserPtr& user) noexcept   // !SMT!-fix
{
	// TODO - добавить команду первого входа юзера
	// dcassert(!ClientManager::isShutdown());
	if (!isClosedOrShutdown())
	{
#ifdef FLYLINKDC_UPDATE_USER_JOIN_USE_WIN_MESSAGES_Q
		const auto l_ou_ptr = new OnlineUserPtr(user);
		if (PostMessage(WM_SPEAKER_UPDATE_USER_JOIN, WPARAM(l_ou_ptr)) == FALSE)
		{
			dcassert(0);
			delete l_ou_ptr;
		}
#else
		speak(UPDATE_USER_JOIN, user);
#endif
#ifdef _DEBUG
//		LogManager::message("[single OnlineUserPtr] void HubFrame::on(ClientListener::UserUpdatedMyINFO nick = " + user->getUser()->getLastNick() + " this = " + Util::toString(__int64(this)));
#endif
#ifdef FLYLINKDC_USE_CHAT_BOT
		ChatBot::getInstance()->onUserAction(BotInit::RECV_UPDATE, user->getUser());
#endif
	}
}

void HubFrame::on(ClientListener::StatusMessage, const Client*, const string& line, int statusFlags) noexcept
{
	speak(ADD_STATUS_LINE, Text::toDOS(line), !BOOLSETTING(FILTER_MESSAGES) || !(statusFlags & ClientListener::FLAG_IS_SPAM));
}

void HubFrame::on(ClientListener::UsersUpdated, const Client*, const OnlineUserList& aList) noexcept
{
	for (auto i = aList.cbegin(); i != aList.cend(); ++i)
	{
		speak(UPDATE_USER, *i);
#ifdef _DEBUG
//		LogManager::message("[array OnlineUserPtr] void HubFrame::on(UsersUpdated nick = " + (*i)->getUser()->getLastNick());
#endif
#ifdef FLYLINKDC_USE_CHAT_BOT
		ChatBot::getInstance()->onUserAction(BotInit::RECV_UPDATE, (*i)->getUser());
#endif
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
#ifdef FLYLINKDC_USE_CHAT_BOT
	ChatBot::getInstance()->onUserAction(BotInit::RECV_PART, user->getUser()); // !SMT!-fix
#endif
}

void HubFrame::on(Redirect, const Client*, const string& line) noexcept
{
	string redirAdr = Util::formatDchubUrl(line); // [+] IRainman fix http://code.google.com/p/flylinkdc/issues/detail?id=1237
	const int l_code = CFlyServerConfig::getAlternativeHub(redirAdr);
	bool l_is_double_redir = false;
	//const string l_reserve_server = "dchub://dc.livedc.ru";
	if (ClientManager::isConnected(redirAdr))
	{
		speak(ADD_STATUS_LINE, STRING(REDIRECT_ALREADY_CONNECTED), true);
		//if (ClientManager::isConnected(l_reserve_server))
		//{
		//  return;
		//}
		//else
		{
			//redirAdr = l_reserve_server;
			l_is_double_redir = true;
			const string l_redirect = "HubFrame::on(Redirect) " + getHubHint() + " -> " + redirAdr + " REDIRECT_ALREADY_CONNECTED!";
			if (m_last_redirect != l_redirect)
			{
				m_last_redirect = l_redirect;
				CFlyServerJSON::pushError(l_code, m_last_redirect);
			}
		}
	}
	
	m_redirect = redirAdr;
	if (l_is_double_redir == false)
	{
		string l_loop_message;
		if (++m_count_redirect_map[m_redirect] > 2)
		{
			/*
			if (!ClientManager::isConnected(l_reserve_server))
			{
			    m_redirect = l_reserve_server;
			    l_loop_message = "(stop loop) ";
			}
			*/
		}
		const string l_redirect = "HubFrame::on(Redirect) " + l_loop_message + getHubHint() + " -> " + m_redirect + " auto follow = " + Util::toString(BOOLSETTING(AUTO_FOLLOW));
		if (m_last_redirect != l_redirect)
		{
			m_last_redirect = l_redirect;
			CFlyServerJSON::pushError(l_loop_message.empty() ? l_code : 51, m_last_redirect);
		}
	}
#ifdef PPA_INCLUDE_AUTO_FOLLOW
	if (BOOLSETTING(AUTO_FOLLOW) || l_is_double_redir == true)
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
	if (!ClientManager::isShutdown())
	{
		speak(ADD_STATUS_LINE, "[Hub = " + c->getHubUrl() + "] " + line);
		// speak(WM_SPEAKER_DISCONNECTED, nullptr);
		//PostMessage(WM_SPEAKER_DISCONNECTED);
	}
	speak(DISCONNECTED);
#ifdef FLYLINKDC_USE_CHAT_BOT
	ChatBot::getInstance()->onHubAction(BotInit::RECV_DISCONNECT, c->getHubUrl());
#endif
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
}
void HubFrame::onTimerHubUpdated()
{
	if (m_client && m_hub_name_update_count)
	{
		m_hub_name_update_count = 0;
		string fullHubName;
		if (m_client->isSecureConnect())
		{
			if (m_client->isTrusted()) // https://drdump.com/DumpGroup.aspx?DumpGroupID=489257
			{
				fullHubName = "[S] ";
			}
			else if (m_client->isSecure())
			{
				fullHubName = "[U] ";
			}
		}
		
		fullHubName += m_client->getHubName();
		
		if (!m_client->getHubDescription().empty())
		{
			fullHubName += " - " + m_client->getHubDescription();
		}
		fullHubName += " (" + m_client->getHubUrl() + ')';
		
		setShortHubName(Text::toT(fullHubName));
		
		if (!BOOLSETTING(SHOW_FULL_HUB_INFO_ON_TAB)) // Не показываем инфу с хаба
		{
			if (!m_client->getName().empty())
			{
				const auto l_name = Text::toT(m_client->getName());
				setShortHubName(l_name);
			}
		}
		
		dcassert(!fullHubName.empty());
		setWindowTitle(fullHubName);
		if (!m_is_hub_name_updated)
		{
			m_is_hub_name_updated = true;
			setDirty(0);
		}
	}
}
void HubFrame::on(ClientListener::HubUpdated, const Client*) noexcept
{
	m_hub_name_update_count++;
}

void HubFrame::on(ClientListener::Message, const Client*,  std::unique_ptr<ChatMessage>& message) noexcept
{
	const auto l_message_ptr = message.release();
#ifdef _DEBUG
	if (l_message_ptr->m_text.find("&#124") != string::npos)
	{
		dcassert(0);
	}
#endif
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
#ifndef FLYLINKDC_ADD_CHAT_LINE_USE_WIN_MESSAGES_Q
		speak(ADD_CHAT_LINE, l_message_ptr); // fix ? http://code.google.com/p/flylinkdc/issues/detail?id=1438
#else
		PostMessage(WM_SPEAKER_ADD_CHAT_LINE, WPARAM(l_message_ptr));
#endif
	}
}
void HubFrame::on(ClientListener::HubFull, const Client*) noexcept
{
	speak(ADD_STATUS_LINE, STRING(HUB_FULL), true);
}
static string getRandomSuffix()
{
	string l_random = Util::toString(Util::rand()).substr(0, 3);
	while (l_random.length() < 3)
	{
		l_random += '0';
	}
	return l_random;
}
void HubFrame::on(ClientListener::NickTaken, const Client*) noexcept
{
	const string l_my_nick = m_client->getMyNick();
	speak(ADD_STATUS_LINE, STRING(NICK_TAKEN) + " (Nick = " + l_my_nick + ")", true);
	auto l_fe = FavoriteManager::getFavoriteHubEntry(m_client->getHubUrl());
	if (l_fe)
	{
		if (l_fe->getPassword().empty())
		{
			string l_fly_user;
			string l_nick = l_my_nick;
			if (l_nick.length() >= 5) // "_R123"
			{
				int i = l_nick.length() - 5;
				if (l_nick[i] == '_' && l_nick[i + 1] == 'R' && isdigit(l_nick[i + 2]) && isdigit(l_nick[i + 3]) && isdigit(l_nick[i + 4]))
				{
					const string l_new_number = getRandomSuffix();
					l_nick[i + 2] = l_new_number[0];
					l_nick[i + 3] = l_new_number[1];
					l_nick[i + 4] = l_new_number[2];
					l_fly_user = l_nick;
				}
				else
				{
					l_fly_user = l_nick + "_R" + getRandomSuffix();
				}
			}
			else
			{
				l_fly_user = l_nick + "_R" + getRandomSuffix();
			}
			auto l_max_len = m_client->getMaxLenNick();
			if (l_max_len == 0)
				l_max_len = 15;
			if (l_fly_user.length() > l_max_len)
			{
				l_fly_user = l_nick.substr(0, l_max_len - 5);
				l_fly_user += "_R" + getRandomSuffix();
			}
			m_client->setMyNick(l_fly_user);
			m_client->setRandomNick(l_fly_user);
			ctrlClient.setHubParam(m_client->getHubUrl(), m_client->getMyNick());
			CFlyServerJSON::pushError(54, "Hub = " + m_client->getHubUrl() + " New random nick = " + l_fly_user);
			if (m_reconnect_count < 3)
			{
				m_client->reconnect();
				m_reconnect_count++;
			}
		}
	}
}
void HubFrame::on(ClientListener::CheatMessage, const string& line) noexcept
{
	speak(CHEATING_USER, line, true);
	//const auto l_message_ptr = new string(line);
	//PostMessage(WM_SPEAKER_CHEATING_USER, WPARAM(l_message_ptr));
}
void HubFrame::on(ClientListener::UserReport, const Client*, const string& report) noexcept // [+] IRainman
{
	speak(USER_REPORT, report, true);
	//const auto l_message_ptr = new string(report);
	//PostMessage(WM_SPEAKER_USER_REPORT, WPARAM(l_message_ptr));
}
#ifdef FLYLINKDC_SUPPORT_HUBTOPIC
void HubFrame::on(ClientListener::HubTopic, const Client*, const string& line) noexcept
{
	speak(ADD_STATUS_LINE, STRING(HUB_TOPIC) + " " + line, true);
}
#endif
#ifdef RIP_USE_CONNECTION_AUTODETECT
void HubFrame::on(OpenTCPPortDetected, const string& strHubUrl)  noexcept
{
	if (m_client && m_client->getHubUrl() == strHubUrl)
	{
		speak(OPEN_TCP_PORT_DETECTED, strHubUrl, true);
	}
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
void HubFrame::InsertItemInternal(const UserInfo* ui)
{
	if (m_is_ext_json_hub)
	{
		const auto l_gender = ui->getIdentity().getGenderType();
		if (l_gender > 1)
		{
			m_ctrlUsers->insertItemState(ui, I_IMAGECALLBACK, ui->getIdentity().getGenderType());
		}
		else
		{
			if (m_is_init_load_list_view)
				m_ctrlUsers->insertItemLast(ui, I_IMAGECALLBACK, m_count_init_insert_list_view);
			else
				m_ctrlUsers->insertItem(ui, I_IMAGECALLBACK);
		}
	}
	else
	{
		if (m_is_init_load_list_view)
			m_ctrlUsers->insertItemLast(ui, I_IMAGECALLBACK, m_count_init_insert_list_view);
		else
			m_ctrlUsers->insertItem(ui, I_IMAGECALLBACK);
	}
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
		dcassert(m_ctrlUsers->findItem(ui) == -1);
		if (m_client && m_client->isConnected())
		{
			InsertItemInternal(ui);
		}
	}
	else
	{
		int64_t size = -1;
		FilterModes mode = NONE;
		const int sel = getFilterSelPos();
		bool doSizeCompare = sel == COLUMN_SHARED && parseFilter(mode, size);
		
		if (matchFilter(*ui, sel, doSizeCompare, mode, size))
		{
			dcassert(m_ctrlUsers->findItem(ui) == -1);
			if (m_client && m_client->isConnected())
			{
				InsertItemInternal(ui);
			}
		}
		else
		{
			//deleteItem checks to see that the item exists in the list
			//unnecessary to do it twice.
			if (m_client && m_client->isConnected())
			{
				m_ctrlUsers->deleteItem(ui);
			}
		}
	}
}

void HubFrame::updateUserList() // [!] IRainman opt.
{
	CLockRedraw<> l_lock_draw(*m_ctrlUsers);
	m_ctrlUsers->DeleteAllItems();
	m_last_count_resort = 0;
	if (m_filter.empty())
	{
		usermap2ListrView();
		//m_userMapInitThread.process_init_user_list(this);
	}
	else
	{
		int64_t size = -1;
		FilterModes mode = NONE;
		dcassert(m_ctrlFilterSel);
		const int sel = getFilterSelPos();
		const bool doSizeCompare = sel == COLUMN_SHARED && parseFilter(mode, size);
		CFlyReadLock(*m_userMapCS);
		//CFlyLock(m_userMapCS);
		for (auto i = m_userMap.cbegin(); i != m_userMap.cend(); ++i)
		{
			UserInfo* ui = i->second;
#ifdef IRAINMAN_USE_HIDDEN_USERS
			dcassert(ui->isHidden() == false);
#endif
			if (matchFilter(*ui, sel, doSizeCompare, mode, size))
			{
				InsertItemInternal(ui);
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
			m_ctrlUsers->SetFocus();
		}
		else if (m_ctrlUsers && focus == m_ctrlUsers->m_hWnd)
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
			m_ctrlUsers->SetFocus();
		}
		else if (m_ctrlUsers && m_ctrlMessage && focus == m_ctrlUsers->m_hWnd)
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
				else if (sel == COLUMN_ANTIVIRUS)
				{
					ui.calcVirusType();
				}
				else if (sel == COLUMN_P2P_GUARD)
				{
					ui.calcP2PGuard();
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
			appendAndActivateUserItems(p_menu, false);
			
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
			int iCount = m_ctrlUsers->GetSelectedCount();
			p_menu.InsertSeparatorFirst(Util::toStringW(iCount) + _T(' ') + TSTRING(HUB_USERS));
			appendAndActivateUserItems(p_menu, false);
		}
	}
	
	if (isChat)
	{
		appendChatCtrlItems(p_menu, m_client);
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
	if (getSelectedUser() && m_ctrlUsers)
	{
		const int pos = m_ctrlUsers->findItem(getSelectedUser()->getIdentity().getNickT());
		if (pos != -1)
		{
			CLockRedraw<> l_lock_draw(*m_ctrlUsers);
			const auto l_count_per_page = m_ctrlUsers->GetCountPerPage();
			const int items = m_ctrlUsers->GetItemCount();
			for (int i = 0; i < items; ++i)
			{
				m_ctrlUsers->SetItemState(i, i == pos ? LVIS_SELECTED | LVIS_FOCUSED : 0, LVIS_SELECTED | LVIS_FOCUSED);
			}
			m_ctrlUsers->EnsureVisible(pos, FALSE);
			const auto l_last_pos = pos + l_count_per_page / 2; // fix https://code.google.com/p/flylinkdc/issues/detail?id=1476
			if (!m_ctrlUsers->EnsureVisible(l_last_pos, FALSE))
			{
				m_ctrlUsers->EnsureVisible(pos, FALSE);
			}
		}
	}
	return 0;
}

LRESULT HubFrame::onAddNickToChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (!m_client->isConnected())
		return 0;
		
	if (getSelectedUser())
	{
		m_lastUserName = getSelectedUser()->getIdentity().getNickT();// SSA_SAVE_LAST_NICK_MACROS
	}
	else
	{
		m_lastUserName.clear(); // SSA_SAVE_LAST_NICK_MACROS
		int i = -1;
		while ((i = m_ctrlUsers->GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			if (!m_lastUserName.empty())// SSA_SAVE_LAST_NICK_MACROS
				m_lastUserName += _T(", ");// SSA_SAVE_LAST_NICK_MACROS
				
			m_lastUserName += Text::toT((m_ctrlUsers->getItemData(i))->getNick());// SSA_SAVE_LAST_NICK_MACROS
		}
	}
	appendNickToChat(m_lastUserName); // SSA_SAVE_LAST_NICK_MACROS
	return 0;
}

LRESULT HubFrame::onAutoScrollChat(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ctrlClient.invertAutoScroll();
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
		if (m_ctrlUsers)
			if ((i = m_ctrlUsers->GetNextItem(i, LVNI_SELECTED)) != -1)
			{
				ui = m_ctrlUsers->getItemData(i);
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

void HubFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isShutdown());
	if (m_ctrlUsers && !ClientManager::isShutdown())
	{
		m_ctrlUsers->SetImageList(g_userImage.getIconList(), LVSIL_SMALL);
		//!!!!m_ctrlUsers->SetImageList(g_userStateImage.getIconList(), LVSIL_STATE);
		if (m_ctrlUsers->isRedraw())
		{
			ctrlClient.SetBackgroundColor(Colors::g_bgColor);
			m_ctrlUsers->setFlickerFree(Colors::g_bgBrush);
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
		UpdateLayout();
	}
}

LRESULT HubFrame::onSizeMove(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	bHandled = FALSE;
	if (ctrlClient.IsWindow())
	{
		ctrlClient.GoToEnd(false);
	}
	else
	{
		//  dcassert(0);
	}
	return 0;
}

LRESULT HubFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	//dcassert(ClientManager::isStartup() != true);
	if (ClientManager::isStartup() == true)
	{
		return CDRF_DODEFAULT;
	}
	if (ClientManager::isShutdown() == true)
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
			const int l_column_id = m_ctrlUsers->findColumn(cd->iSubItem);
#if 0 // FLYLINKDC_USE_XXX_ICON
			if (l_column_id == COLUMN_FLY_HUB_GENDER)
			{
				const int l_indx_icon = ui->getIdentity().getGenderType();
				if (l_indx_icon)
				{
					m_ctrlUsers->GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
					LONG top = rc.top + (rc.Height() - 15) / 2;
					if ((top - rc.top) < 2)
						top = rc.top + 1;
					const POINT p = { rc.left, top };
					g_userImage.Draw(cd->nmcd.hdc, 19 + l_indx_icon, p);
					const tstring l_w_full_name = ui->getIdentity().getGenderTypeAsString(l_indx_icon);
					if (!l_w_full_name.empty())
					{
						::ExtTextOut(cd->nmcd.hdc, rc.left + 16, top + 1, ETO_CLIPPED, rc, l_w_full_name.c_str(), l_w_full_name.size(), NULL);
					}
					return CDRF_SKIPDEFAULT;
				}
			}
			else
#endif
				if (l_column_id == COLUMN_UPLOAD || l_column_id == COLUMN_DOWNLOAD || l_column_id == COLUMN_MESSAGES || l_column_id == COLUMN_ANTIVIRUS || l_column_id == COLUMN_P2P_GUARD)
				{
					m_ctrlUsers->GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
					m_ctrlUsers->SetItemFilled(cd, rc, cd->clrText, cd->clrText);
					const tstring& l_value = ui->getText(l_column_id);
					if (!l_value.empty())
					{
						int l_step = 0;
						if (l_column_id == COLUMN_ANTIVIRUS)
						{
							LONG top = rc.top + (rc.Height() - 15) / 2;
							if ((top - rc.top) < 2)
								top = rc.top + 1;
							const POINT ps = { rc.left, top };
							POINT p;
							int l_icon_index = 0;
							if (!ui->getIdentity().isVirusOnlySingleType(Identity::VT_NICK))
							{
								if (ui->getIdentity().isVirusOnlySingleType(Identity::VT_SHARE))
									l_icon_index = 2;
								else
									l_icon_index = 1;
								if (l_icon_index != 2 && GetCursorPos(&p))
								{
									if (ScreenToClient(&p))
									{
										LVHITTESTINFO lvhti = {0};
										lvhti.pt = p;
										int pos = m_ctrlUsers->SubItemHitTest(&lvhti);
										if (pos != -1)
										{
											l_icon_index = 3;
										}
									}
								}
							}
							g_userStateImage.Draw(cd->nmcd.hdc, l_icon_index , ps);
							l_step += 17;
						}
						::ExtTextOut(cd->nmcd.hdc, rc.left + 6 + l_step, rc.top + 2, ETO_CLIPPED, rc, l_value.c_str(), l_value.length(), NULL);
					}
					return CDRF_SKIPDEFAULT;
				}
				else if (l_column_id == COLUMN_IP)
				{
					const tstring& l_ip = ui->getText(COLUMN_IP);
					if (!l_ip.empty())
					{
						const bool l_is_fantom_ip = ui->getOnlineUser()->getIdentity().isFantomIP();
						CRect rc;
						m_ctrlUsers->GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
						//const COLORREF col_brit = OperaColors::brightenColor(cd->clrText, l_is_fantom_ip ? 0.6f : 0.4f);
						//m_ctrlUsers->SetItemFilled(cd, rc, /*color_text*/ col_brit, /*color_text_unfocus*/ col_brit);
						m_ctrlUsers->SetItemFilled(cd, rc, cd->clrText, cd->clrText);
						const auto l_old_color = cd->clrText;
						if (l_is_fantom_ip)
						{
							cd->clrText = OperaColors::blendColors(cd->clrText, SETTING(BACKGROUND_COLOR), 0.4);
							SetTextColor(cd->nmcd.hdc, cd->clrText);
						}
						::ExtTextOut(cd->nmcd.hdc, rc.left + 6, rc.top + 2, ETO_CLIPPED, rc, l_ip.c_str(), l_ip.length(), NULL);
						if (l_is_fantom_ip) // fix https://code.google.com/p/flylinkdc/issues/detail?id=1477
						{
							cd->clrText =  l_old_color;
							//SetTextColor(cd->nmcd.hdc, cd->clrText);
							const auto l_width_ip = WinUtil::getTextWidth(l_ip, cd->nmcd.hdc); // TODO - cache ?
							::ExtTextOut(cd->nmcd.hdc, rc.left + 6 + l_width_ip + 1, rc.top - 1 , ETO_CLIPPED, rc, _T("*"), 1, NULL);
						}
						//unique_ptr<CSelectFont> l_font(!l_is_fantom_ip ? nullptr : unique_ptr<CSelectFont>(new CSelectFont(cd->nmcd.hdc, Fonts::g_halfFont)));
						//::ExtTextOut(cd->nmcd.hdc, rc.left + 6, rc.top + 2, ETO_CLIPPED, rc, l_ip.c_str(), l_ip.length(), NULL);
					}
					return CDRF_SKIPDEFAULT;
				}
				else if (l_column_id == COLUMN_GEO_LOCATION)
				{
					m_ctrlUsers->GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
					if (WinUtil::isUseExplorerTheme())
					{
					
						SetTextColor(cd->nmcd.hdc, cd->clrText);
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
								m_ctrlUsers->SetItemFilled(cd, rc,  cd->clrText, cd->clrText);  // TODO fix copy-paste
							}
						}
						else
						{
							m_ctrlUsers->SetItemFilled(cd, rc, cd->clrText , cd->clrText); // TODO fix copy-paste
						}
					}
					else
					{
						m_ctrlUsers->SetItemFilled(cd, rc, cd->clrText , cd->clrText); // TODO fix copy-paste
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
#ifdef FLYLINKDC_USE_GEO_IP
						if (BOOLSETTING(ENABLE_COUNTRYFLAG))
						{
							const POINT ps = { rc.left, top };
							g_flagImage.DrawCountry(cd->nmcd.hdc, l_location, ps);
							l_step += 25;
						}
#endif
						const POINT p = { rc.left + l_step, top };
						if (l_location.getFlagIndex() > 0)
						{
							g_flagImage.DrawLocation(cd->nmcd.hdc, l_location, p);
							l_step += 25;
						}
						// ~TODO: move this to FlagImage and cleanup!
						top = rc.top + (rc.Height() - 15 /*WinUtil::getTextHeight(cd->nmcd.hdc)*/ - 1) / 2;
						const auto& l_desc = l_location.getDescription();
						if (!l_desc.empty())
						{
							::ExtTextOut(cd->nmcd.hdc, rc.left + l_step + 5, top + 1, ETO_CLIPPED, rc, l_desc.c_str(), l_desc.length(), nullptr);
						}
					}
					return CDRF_SKIPDEFAULT;
				}
			bHandled = FALSE;
			return CDRF_DODEFAULT;
		}
		case CDDS_ITEMPREPAINT:
		{
			UserInfo* ui = (UserInfo*)cd->nmcd.lItemlParam;
			if (ui)
			{
//				PROFILE_THREAD_SCOPED_DESC("CDDS_ITEMPREPAINT");
				ui->calcLocation();
				ui->calcVirusType();
				ui->calcP2PGuard();
				Colors::getUserColor(m_client->isOp(), ui->getUser(), cd->clrText, cd->clrTextBk, ui->m_flag_mask, ui->getOnlineUser()); // !SMT!-UI
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
void HubFrame::addDupeUsersToSummaryMenu(ClientManager::UserParams& p_param)
{
	// Данная функция ломает меню - http://youtu.be/GaWw-S4ZYJA
	// Причину пока не знаю - есть краши https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=27075
	// L: ретурн убрал, ведь не помогло же! return; // http://www.flylinkdc.ru/2013/07/flylinkdc-r502-beta92-build-14457.html
	/*
	r502-beta94-x64 build 14474
	косяк с пкм после alt+d ни куда не делся
	L: есть значительная вероятность того, что после моего рефакторинга проблемы исчезнут, прошу отписаться.
	*/
	vector<std::pair<tstring, UINT> > l_menu_strings;
	{
		CFlyLock(g_frames_cs);
		for (auto f = g_frames.cbegin(); f != g_frames.cend(); ++f)
		{
			const auto& frame = f->second;
			if (frame->isClosedOrShutdown())
				continue;
			CFlyReadLock(*frame->m_userMapCS);
			//CFlyLock(frame->m_userMapCS);
			for (auto i = frame->m_userMap.cbegin(); i != frame->m_userMap.cend(); ++i) // TODO https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=28097
			{
				if (frame->isClosedOrShutdown())
					continue;
				const auto& l_id = i->second->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'i->second->getIdentity()' expression repeatedly. hubframe.cpp 3673
				const auto l_cur_ip = l_id.getUser()->getLastIPfromRAM().to_string();
				if ((p_param.m_bytesShared && l_id.getBytesShared() == p_param.m_bytesShared) ||
				        (p_param.m_nick == l_id.getNick()) ||
				        (!p_param.m_ip.empty() && p_param.m_ip == l_cur_ip)) // .getIpAsString() - нельзя она забирает адрес из базы и тормозит
				{
					tstring info = Text::toT(frame->m_client->getHubName() + " ( " + frame->m_client->getHubUrl() + " ) ") + _T(" - ") + i->second->getText(COLUMN_NICK);
					const UINT flags = (!p_param.m_ip.empty() && p_param.m_ip == l_cur_ip) ? MF_CHECKED : 0;
					FavoriteUser favUser;
					if (FavoriteManager::getFavoriteUser(i->second->getUser(), favUser))
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
					l_menu_strings.push_back(make_pair(info, flags));
					if (!l_id.getApplication().empty() || !l_cur_ip.empty())
					{
						string l_menu_ip;
						if (!l_cur_ip.empty() && l_cur_ip != "0.0.0.0")
						{
							l_menu_ip = ",   IP: " + l_cur_ip;
						}
						l_menu_strings.push_back(make_pair(Text::toT(l_id.getTag() + l_menu_ip), 0));
					}
					else
					{
						l_menu_strings.push_back(make_pair(_T(""), 0));
					}
				}
			}
		}
	}
	for (auto i = l_menu_strings.cbegin(); i != l_menu_strings.cend(); ++i)
	{
		userSummaryMenu.AppendMenu(MF_SEPARATOR);
		userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED | i->second, IDC_NONE, i->first.c_str());
		++i;
		if (i != l_menu_strings.cend() && !i->first.empty())
		{
			userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, i->first.c_str());
		}
	}
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
UserInfo* HubFrame::findUser(const OnlineUserPtr& p_user)
{
	dcassert(!m_is_process_disconnected);
	//if(m_is_fynally_clear_user_list)
	//{
	//  LogManager::message("findUser after m_is_fynally_clear_user_list = " + p_user->getUser()->getLastNick());
	//}
	CFlyReadLock(*m_userMapCS);
	//CFlyLock(m_userMapCS);
	return m_userMap.findUser(p_user);
}

UserInfo* HubFrame::findUser(const tstring& p_nick)   // !SMT!-S
{
	dcassert(!m_is_process_disconnected);
	dcassert(!p_nick.empty());
	if (p_nick.empty())
	{
		dcassert(0);
		return nullptr;
	}
	
	const OnlineUserPtr ou = m_client->findUser(Text::fromT(p_nick));
	if (ou)
	{
		return findUser(ou);
	}
	else
	{
		return nullptr;
	}
}

/**
 * @file
 * $Id: HubFrame.cpp,v 1.132 2006/11/04 14:08:28 bigmuscle Exp $
 */
