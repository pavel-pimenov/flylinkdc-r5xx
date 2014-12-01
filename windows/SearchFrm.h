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

#if !defined(SEARCH_FRM_H)
#define SEARCH_FRM_H

#pragma once

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "WinUtil.h"
#include "UCHandler.h"

#include "../client/DCPlusPlus.h"
#include "../client/UserInfoBase.h"
#include "../client/SearchManager.h"
#include "../client/ClientManagerListener.h"
#include "../client/QueueManager.h"
#include "../client/SearchResult.h"
#include "../client/ShareManager.h"
#include "../FlyFeatures/GradientLabel.h"
#include "../FlyFeatures/flyServer.h"

#define SEARCH_MESSAGE_MAP 6        // This could be any number, really...
#define SHOWUI_MESSAGE_MAP 7
#define FILTER_MESSAGE_MAP 8

#define FLYLINKDC_USE_WINDOWS_TIMER_SEARCH_FRAME
// С виндовым таймером у меня иногда вешается на ноуте.
class HIconWrapper;
class SearchFrame : public MDITabChildWindowImpl < SearchFrame, RGB(127, 127, 255), IDR_SEARCH > ,
	private SearchManagerListener, private ClientManagerListener,
	public UCHandler<SearchFrame>, public UserInfoBaseHandler<SearchFrame, UserInfoGuiTraits::NO_COPY>,
	private SettingsManagerListener
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	, public PreviewBaseHandler<SearchFrame> // [+] IRainman fix.
#endif
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	, public CFlyServerAdapter
#endif
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_SEARCH_FRAME
	, private CFlyTimerAdapter
#else
	, private TimerManagerListener
#endif
#ifdef _DEBUG
	, virtual NonDerivable<SearchFrame>, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		static void openWindow(const tstring& str = Util::emptyStringT, LONGLONG size = 0, Search::SizeModes mode = Search::SIZE_ATLEAST, Search::TypeModes type = Search::TYPE_ANY);
		static void closeAll();
		
		DECLARE_FRAME_WND_CLASS_EX(_T("SearchFrame"), IDR_SEARCH, 0, COLOR_3DFACE)
		
		typedef MDITabChildWindowImpl < SearchFrame, RGB(127, 127, 255), IDR_SEARCH > baseClass;
		typedef UCHandler<SearchFrame> ucBase;
		typedef UserInfoBaseHandler<SearchFrame, UserInfoGuiTraits::NO_COPY> uicBase;
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		typedef PreviewBaseHandler<SearchFrame> prevBase; // [+] IRainman fix.
#endif
		BEGIN_MSG_MAP(SearchFrame)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_GETDISPINFO, ctrlResults.onGetDispInfo)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_COLUMNCLICK, ctrlResults.onColumnClick)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_GETINFOTIP, ctrlResults.onInfoTip)
		NOTIFY_HANDLER(IDC_HUB, LVN_GETDISPINFO, ctrlHubs.onGetDispInfo)
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		NOTIFY_HANDLER(IDC_HUB, NM_CUSTOMDRAW, ctrlHubs.onCustomDraw) // [+] IRainman
#endif
		NOTIFY_HANDLER(IDC_RESULTS, NM_DBLCLK, onDoubleClickResults)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_HUB, LVN_ITEMCHANGED, onItemChangedHub)
		NOTIFY_HANDLER(IDC_RESULTS, NM_CUSTOMDRAW, onCustomDraw)
		MESSAGE_HANDLER(WM_CREATE, onCreate)
		MESSAGE_HANDLER(WM_SETFOCUS, onFocus)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, onCtlColor)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_SEARCH_FRAME
		MESSAGE_HANDLER(WM_TIMER, onTimer)
#endif
		MESSAGE_HANDLER(WM_DRAWITEM, onDrawItem)
		MESSAGE_HANDLER(WM_MEASUREITEM, onMeasure)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu) // [+] InfinitySky.
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_SEARCH, onSearch)
		COMMAND_ID_HANDLER(IDC_SEARCH_PASSIVE, onSearchPassive)
		COMMAND_ID_HANDLER(IDC_SEARCH_UDP_PORT_TEST, onUDPPortTest)
		COMMAND_ID_HANDLER(IDC_SEARCH_PAUSE, onPause)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy)
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		MESSAGE_HANDLER(WM_SPEAKER_MERGE_FLY_SERVER, onMergeFlyServerResult)
		COMMAND_ID_HANDLER(IDC_COPY_FLYSERVER_INFORM, onCopy)
		COMMAND_ID_HANDLER(IDC_VIEW_FLYSERVER_INFORM, onShowFlyServerProperty)
#endif
		COMMAND_ID_HANDLER(IDC_COPY_PATH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_SIZE, onCopy)
		COMMAND_ID_HANDLER(IDC_FREESLOTS, onFreeSlots)
		COMMAND_ID_HANDLER(IDC_COLLAPSED, onCollapsed)
		COMMAND_ID_HANDLER(IDC_GETLIST, onGetList)
		COMMAND_ID_HANDLER(IDC_BROWSELIST, onBrowseList)
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchByTTH)
		COMMAND_ID_HANDLER(IDC_MARK_AS_DOWNLOADED, onMarkAsDownloaded)
#ifdef PPA_INCLUDE_BITZI_LOOKUP
		COMMAND_ID_HANDLER(IDC_BITZI_LOOKUP, onBitziLookup)
#endif
		COMMAND_ID_HANDLER(IDC_COPY_HUB_URL, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FULL_MAGNET_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy) // !SMT!-UI
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
#ifdef IRAINMAN_SEARCH_OPTIONS
		COMMAND_ID_HANDLER(IDC_HUB, onHubChange)
#endif
		COMMAND_ID_HANDLER(IDC_PURGE, onPurge)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_SEARCH_FRAME, onCloseAll) // [+] InfinitySky.
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow) // [+] InfinitySky.
		COMMAND_CODE_HANDLER(CBN_EDITCHANGE, onEditChange)
		COMMAND_ID_HANDLER(IDC_DOWNLOADTO, onDownloadTo)
		COMMAND_ID_HANDLER(IDC_FILETYPES, onFiletypeChange) // [+] SCALOlaz: save type
		COMMAND_ID_HANDLER(IDC_SEARCH_SIZEMODE, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_SEARCH_SIZE, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_SEARCH_MODE, onFiletypeChange)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_FAVORITE_DIRS, IDC_DOWNLOAD_FAVORITE_DIRS + 499, onDownload)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS, IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS + 499, onDownloadWhole)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET, IDC_DOWNLOAD_TARGET + 499, onDownloadTarget)
		COMMAND_RANGE_HANDLER(IDC_PRIORITY_PAUSED, IDC_PRIORITY_HIGHEST, onDownloadWithPrio)
		
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uicBase)
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		CHAIN_COMMANDS(prevBase) // [+] IRainman fix.
#endif
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(SEARCH_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		MESSAGE_HANDLER(WM_KEYDOWN, onChar)
		MESSAGE_HANDLER(WM_KEYUP, onChar)
		ALT_MSG_MAP(SHOWUI_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onShowUI)
		ALT_MSG_MAP(FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onSelChange)
		END_MSG_MAP()
		
		SearchFrame() :
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_SEARCH_FRAME
			CFlyTimerAdapter(m_hWnd),
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			CFlyServerAdapter(m_hWnd, 5000),
#endif
#endif
			searchBoxContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			searchContainer(WC_EDIT, this, SEARCH_MESSAGE_MAP),
			sizeContainer(WC_EDIT, this, SEARCH_MESSAGE_MAP),
			modeContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			sizeModeContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			fileTypeContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			//showUIContainer(WC_COMBOBOX, this, SHOWUI_MESSAGE_MAP),
			//slotsContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			//collapsedContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
			//storeIPContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			m_storeIP(false),
#endif
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			//m_FlyServerContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			//m_FlyServerGradientContainer(WC_STATIC, this, SEARCH_MESSAGE_MAP),
#endif
			//storeSettingsContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			//purgeContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			//doSearchContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			//doSearchPassiveContainer(WC_COMBOBOX, this, SEARCH_MESSAGE_MAP),
			resultsContainer(WC_LISTVIEW, this, SEARCH_MESSAGE_MAP),
			hubsContainer(WC_LISTVIEW, this, SEARCH_MESSAGE_MAP),
			ctrlFilterContainer(WC_EDIT, this, FILTER_MESSAGE_MAP),
			ctrlFilterSelContainer(WC_COMBOBOX, this, FILTER_MESSAGE_MAP),
			m_initialSize(0), m_initialMode(Search::SIZE_ATLEAST), m_initialType(Search::TYPE_ANY),
			m_showUI(true), m_onlyFree(false), m_isHash(false), m_droppedResults(0), m_resultsCount(0),
			m_expandSR(false),
			m_storeSettings(false), m_isExactSize(false), m_exactSize2(0), m_sizeMode(Search::SIZE_DONTCARE), /*searches(0),*/
			m_ftype(Search::TYPE_ANY),
			m_lastFindTTH(false),
			m_TestPortGuard(false),
			m_running(false),
			m_searchEndTime(0),
			m_searchStartTime(0),
			m_waitingResults(false),
			m_needsUpdateStats(false), // [+] IRainman opt.
			m_Theme(nullptr)
		{
		}
		
		~SearchFrame();
		
		LRESULT onFiletypeChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChar(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/); // [+] InfinitySky.
		LRESULT onDrawItem(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onMeasure(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onCtlColor(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onDoubleClickResults(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		LRESULT onMergeFlyServerResult(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
#endif
		
		LRESULT onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMarkAsDownloaded(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef IRAINMAN_SEARCH_OPTIONS
		LRESULT onHubChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
#ifdef PPA_INCLUDE_BITZI_LOOKUP
		LRESULT onBitziLookup(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onFilterChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		
		LRESULT onDownloadTo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWhole(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadTarget(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		LRESULT onShowFlyServerProperty(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			const auto ii = ctrlResults.getSelectedItem();
			if (ii)
			{
				showFlyServerProperty(ii);
			}
			return 0;
		}
#endif
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
#ifdef FLYLINKDC_USE_WINDOWS_TIMER_SEARCH_FRAME
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
#else
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept;
#endif
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void runUserCommand(UserCommand& uc);
		void onSizeMode();
		void removeSelected()
		{
			int i = -1;
			FastLock l(cs);
			while ((i = ctrlResults.GetNextItem(-1, LVNI_SELECTED)) != -1)
			{
				ctrlResults.removeGroupedItem(ctrlResults.getItemData(i));
			}
		}
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			ctrlResults.forEachSelected(&SearchInfo::view);
			return 0;
		}
#endif
		LRESULT onRemove(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			removeSelected();
			return 0;
		}
		
		LRESULT onFreeSlots(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			m_onlyFree = (ctrlSlots.GetCheck() == 1);
			return 0;
		}
		
		LRESULT onCollapsed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			m_expandSR = ctrlCollapsed.GetCheck() == 1;
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
			m_storeIP = m_ctrlStoreIP.GetCheck() == 1;
#endif
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			SET_SETTING(ENABLE_FLY_SERVER, m_ctrlFlyServer.GetCheck() == 1);
			m_FlyServerGradientLabel.SetActive(BOOLSETTING(ENABLE_FLY_SERVER));
#endif
			m_storeSettings = m_ctrlStoreSettings.GetCheck() == 1;
			SET_SETTING(SAVE_SEARCH_SETTINGS, m_storeSettings);
			return 0;
		}
		
		LRESULT onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			onEnter(false);
			return 0;
		}
		LRESULT onSearchPassive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			onEnter(true);
			return 0;
		}
		LRESULT onUDPPortTest(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
			
			if (kd->wVKey == VK_DELETE)
			{
				removeSelected();
			}
			return 0;
		}
		
		LRESULT onFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			if (::IsWindow(ctrlSearch))
				ctrlSearch.SetFocus();
			return 0;
		}
		
		LRESULT onShowUI(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
		{
			bHandled = FALSE;
			m_showUI = (wParam == BST_CHECKED);
			UpdateLayout(FALSE);
			return 0;
		}
		
		void setInitial(const tstring& str, LONGLONG size, Search::SizeModes mode, Search::TypeModes type)
		{
			m_initialString = str;
			m_initialSize = size;
			m_initialMode = mode;
			m_initialType = type;
			m_running = true;
		}
		
		// [+] InfinitySky.
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		// [+] InfinitySky.
		LRESULT onCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			closeAll();
			return 0;
		}
		
	private:
		tstring getDownloadDirectory(WORD wID);
		class SearchInfo;
		
	public:
		typedef MediainfoTypedTreeListViewCtrl<SearchInfo, IDC_RESULTS, TTHValue, std::hash<TTHValue*>, std::equal_to<TTHValue*>> SearchInfoList;
		SearchInfoList& getUserList()
		{
			return ctrlResults;
		}
		
	private:
		enum
		{
			COLUMN_FIRST,
			COLUMN_FILENAME = COLUMN_FIRST,
			COLUMN_LOCAL_PATH,
			COLUMN_HITS,
			COLUMN_NICK,
			COLUMN_ANTIVIRUS,
			COLUMN_TYPE,
			COLUMN_SIZE,
			COLUMN_PATH,
			COLUMN_FLY_SERVER_RATING,
			COLUMN_BITRATE,
			COLUMN_MEDIA_XY,
			COLUMN_MEDIA_VIDEO,
			COLUMN_MEDIA_AUDIO,
			COLUMN_DURATION,
			COLUMN_SLOTS,
//[-]PPA        COLUMN_CONNECTION,
			COLUMN_HUB,
			COLUMN_EXACT_SIZE,
			COLUMN_LOCATION, // !SMT!-IP
			COLUMN_IP,
#ifdef PPA_INCLUDE_DNS
			COLUMN_DNS, // !SMT!-IP
#endif
			COLUMN_TTH,
			COLUMN_LAST
		};
		
		enum Images
		{
			IMAGE_UNKOWN,
			IMAGE_SLOW,
			IMAGE_NORMAL,
			IMAGE_FAST
		};
		
		enum FilterModes
		{
			NONE,
			EQUAL,
			GREATER_EQUAL,
			LESS_EQUAL,
			GREATER,
			LESS,
			NOT_EQUAL
		};
		
		class SearchInfo : public UserInfoBase
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			, public CFlyServerInfo
#endif
#ifdef _DEBUG
			, virtual NonDerivable<SearchInfo> // [+] IRainman fix.
#endif
		{
			public:
				typedef SearchInfo* Ptr;
				typedef vector<Ptr> Array;
				
				SearchInfo(const SearchResultPtr &aSR) : sr(aSR), collapsed(true), parent(nullptr), hits(0), m_icon_index(-1), m_is_flush_ip_to_sqlite(false)
				{
					sr->inc();
				}
				~SearchInfo()
				{
					sr->dec();
				}
				
				const UserPtr& getUser() const
				{
					return sr->getUser();
				}
				
				bool collapsed;
				SearchInfo* parent;
				size_t hits;
				int m_icon_index;
				
				void getList();
				void browseList();
				
				void view();
				struct Download
				{
					Download(const tstring& aTarget, SearchFrame* aSf, QueueItem::Priority aPrio, QueueItem::MaskType aMask = 0) : tgt(aTarget), sf(aSf), prio(aPrio), mask(aMask) { }
					void operator()(const SearchInfo* si);
					const tstring& tgt;
					SearchFrame* sf;
					QueueItem::Priority prio;
					QueueItem::MaskType mask;
				};
				struct DownloadWhole
				{
					DownloadWhole(const tstring& aTarget) : tgt(aTarget) { }
					void operator()(const SearchInfo* si);
					const tstring& tgt;
				};
				struct DownloadTarget
				{
					DownloadTarget(const tstring& aTarget) : tgt(aTarget) { }
					void operator()(const SearchInfo* si);
					const tstring& tgt;
				};
				struct CheckTTH
				{
					CheckTTH() : op(true), firstHubs(true), hasTTH(false), firstTTH(true) { }
					void operator()(const SearchInfo* si);
					bool firstHubs;
					StringList hubs;
					bool op;
					bool hasTTH;
					bool firstTTH;
					tstring tth;
				};
				
				const tstring getText(uint8_t col) const;
				
				static int compareItems(const SearchInfo* a, const SearchInfo* b, int col);
				
				int getImageIndex() const;
				void calcImageIndex();
				
				SearchInfo* createParent()
				{
					return this;
				}
				const tstring& getGroupingString() const
				{
					return columns[COLUMN_TTH];
				}
				/*
				void updateMainItem()
				{
				    uint32_t total = parent->subItems.size();
				    if (total != 0)
				    {
				        LocalArray<TCHAR, 256> buf;
				        snwprintf(buf, buf.size(), _T("%d %s"), total + 1, CTSTRING(USERS));
				
				        parent->columns[COLUMN_HITS] = buf;
				        if (total == 1)
				            parent->columns[COLUMN_SIZE] = columns[COLUMN_SIZE];
				    }
				    else
				    {
				        parent->columns[COLUMN_HITS].clear();
				    }
				}
				*/
				
				Util::CustomNetworkIndex m_location;
				bool m_is_flush_ip_to_sqlite;
				const SearchResultPtr sr;
				tstring columns[COLUMN_LAST];
				const TTHValue& getGroupCond() const
				{
					return sr->getTTH();
				}
		};
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		bool showFlyServerProperty(const SearchInfo* p_item_info);
#endif
		struct HubInfo
#ifdef _DEBUG
				: private boost::noncopyable
#endif
		{
			HubInfo(const tstring& aUrl, const tstring& aName, bool aOp) : url(aUrl),
				name(aName), m_is_op(aOp) { }
				
			const tstring& getText(int col) const
			{
				return (col == 0) ? name : Util::emptyStringT;
			}
			static int compareItems(const HubInfo* a, const HubInfo* b, int col)
			{
				return (col == 0) ? Util::DefaultSort(a->name.c_str(), b->name.c_str()) : 0;
			}
			static const int getImageIndex()
			{
				return 0;
			}
			static uint8_t getStateImageIndex()
			{
				return 0;
			}
			
			const tstring url;
			const tstring name;
			const bool m_is_op;
		};
		
		// WM_SPEAKER
		enum Speakers
		{
			ADD_RESULT,
			//FILTER_RESULT,[-]IRainman optimize SearchFrame
			HUB_ADDED,
			HUB_CHANGED,
			HUB_REMOVED,
			QUEUE_STATS,
			UPDATE_STATUS, // [+] IRainman opt.
			//SEARCH_START
		};
		
		tstring m_initialString;
		int64_t m_initialSize;
		Search::SizeModes m_initialMode;
		Search::TypeModes m_initialType;
		
		CStatusBarCtrl ctrlStatus;
		CEdit ctrlSearch;
		CComboBox ctrlSearchBox;
		CEdit ctrlSize;
		CComboBox ctrlMode;
		CComboBox ctrlSizeMode;
		CComboBox ctrlFiletype;
		CImageList searchTypes;
		CButton ctrlPurge;
		CButton ctrlPauseSearch;
		CButton ctrlDoSearch;
		CButton ctrlDoSearchPassive;
		
		CButton ctrlDoUDPTestPort;
		CHyperLink m_SearchHelp;
		
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		CButton m_ctrlFlyServer;
		//CContainedWindow m_FlyServerContainer;
		CGradientLabelCtrl m_FlyServerGradientLabel;
		//CContainedWindow m_FlyServerGradientContainer;
#endif
		CFlyToolTipCtrl m_tooltip;  // [+] SCALOlaz: add tooltips
		BOOL ListMeasure(HWND hwnd, UINT uCtrlId, MEASUREITEMSTRUCT *mis);
		BOOL ListDraw(HWND hwnd, UINT uCtrlId, DRAWITEMSTRUCT *dis);
		
		CContainedWindow searchContainer;
		CContainedWindow searchBoxContainer;
		CContainedWindow sizeContainer;
		CContainedWindow modeContainer;
		CContainedWindow sizeModeContainer;
		CContainedWindow fileTypeContainer;
		//CContainedWindow slotsContainer;
		//CContainedWindow collapsedContainer;
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		//CContainedWindow storeIPContainer;
		CButton m_ctrlStoreIP;
		bool m_storeIP;
#endif
		//CContainedWindow storeSettingsContainer;
		//CContainedWindow showUIContainer;
		//CContainedWindow purgeContainer;
		//CContainedWindow doSearchContainer;
		//CContainedWindow doSearchPassiveContainer;
		
		CContainedWindow resultsContainer;
		CContainedWindow hubsContainer;
		CContainedWindow ctrlFilterContainer;
		CContainedWindow ctrlFilterSelContainer;
		tstring filter;
		
		CStatic searchLabel, sizeLabel, optionLabel, typeLabel, hubsLabel, srLabel;
		CButton ctrlSlots, ctrlShowUI, ctrlCollapsed;
		
		CButton m_ctrlStoreSettings;
		bool m_showUI;
		bool m_lastFindTTH;
		bool m_TestPortGuard;
		
		CImageList images;
		SearchInfoList ctrlResults;
		TypedListViewCtrl<HubInfo, IDC_HUB> ctrlHubs;
		//OMenu resultsMenu;
		OMenu targetMenu;
		OMenu targetDirMenu;
		OMenu priorityMenu;
		OMenu copyMenu;
		OMenu tabMenu; // [+] InfinitySky
		
		StringList m_search;
		StringList targets;
		StringList wholeTargets;
		SearchInfo::Array m_pausedResults;
		void clearPausedResults()
		{
			for_each(m_pausedResults.begin(), m_pausedResults.end(), DeleteFunction());
			m_pausedResults.clear();
		}
		
		CEdit ctrlFilter;
		CComboBox ctrlFilterSel;
		
		bool m_onlyFree;
		bool m_isHash;
		bool m_expandSR;
		bool m_storeSettings;
		bool m_running;
		bool m_isExactSize;
		bool m_waitingResults;
		bool m_needsUpdateStats; // [+] IRainman opt.
		Search::TypeModes m_ftype;
		int64_t m_exactSize2;
		Search::SizeModes m_sizeMode;
		size_t m_resultsCount;
		uint64_t m_searchEndTime;
		uint64_t m_searchStartTime;
		tstring m_target;
		tstring m_statusLine; // [+] IRainman fix.
		uint32_t m_token;
		
		FastCriticalSection cs; // [!] IRainman opt: use spin lock here.
		
	public:
		static TStringList g_lastSearches;
		
	private:
		static HIconWrapper g_purge_icon; // [~] Sergey Shushkanov
		static HIconWrapper g_pause_icon; // [~] Sergey Shushkanov
		static HIconWrapper g_search_icon; // [~] Sergey Shushkanov
		
		static HIconWrapper g_UDPOkIcon;
		static HIconWrapper g_UDPWaitIcon;
		static tstring      g_UDPTestText;
		static bool         g_isUDPTestOK;
		
		CStatic m_ctrlUDPMode;
		CStatic m_ctrlUDPTestResult;
		
		size_t m_droppedResults;
		HTHEME m_Theme;
		
		StringMap m_ucLineParams;
		
		static int columnIndexes[];
		static int columnSizes[];
		
		typedef std::map<HWND, SearchFrame*> FrameMap;
		typedef pair<HWND, SearchFrame*> FramePair;
		
		static FrameMap g_frames;
		
		struct TARGET_STRUCT
		{
			enum DefinedTypes
			{
				PATH_DEFAULT,
				PATH_FAVORITE,
				PATH_SRC,
				PATH_LAST,
				PATH_BROWSE
			};
			
			TARGET_STRUCT(const tstring &_strPath, DefinedTypes _Type):
				strPath(_strPath),
				Type(_Type)
			{
			}
			
			TARGET_STRUCT():
				Type(PATH_DEFAULT)
			{
			}
			
			tstring strPath;
			DefinedTypes Type;
		};
		typedef std::map<int, TARGET_STRUCT> TargetsMap; // !SMT!-S
		TargetsMap dlTargets; // !SMT!-S
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		void mergeFlyServerInfo();
		bool scan_list_view_from_merge();
		typedef std::unordered_map<TTHValue, std::pair<SearchInfo*, CFlyServerCache> > CFlyMergeItem;
		CFlyMergeItem m_merge_item_map; // TODO - организовать кэш для медиаинфы, чтобы лишний раз не ходить на флай-сервер c get-запросами
		void update_column_after_merge(std::vector<int> p_update_index);
		
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
		void downloadSelected(const tstring& aDir, bool view = false);
		void downloadWholeSelected(const tstring& aDir);
		void onEnter(bool p_is_force_passive);
		void onTab(bool shift);
		void download(SearchResult* aSR, const tstring& aDir, bool view);
		
		void on(SearchManagerListener::SR, const SearchResultPtr &aResult) noexcept;
		void on(SearchManagerListener::UDPTest, const string& p_ip) noexcept;
		//void on(SearchManagerListener::Searching, SearchQueueItem* aSearch) noexcept;
		
		// ClientManagerListener
		void on(ClientConnected, const Client* c) noexcept
		{
			speak(HUB_ADDED, c);
		}
		void on(ClientUpdated, const Client* c) noexcept
		{
			speak(HUB_CHANGED, c);
		}
		void on(ClientDisconnected, const Client* c) noexcept
		{
			speak(HUB_REMOVED, c);
		}
		void on(SettingsManagerListener::Save, SimpleXML& /*xml*/);
		
		void initHubs();
		void onHubAdded(HubInfo* info);
		void onHubChanged(HubInfo* info);
		void onHubRemoved(HubInfo* info);
		bool matchFilter(const SearchInfo* si, int sel, bool doSizeCompare = false, FilterModes mode = NONE, int64_t size = 0);
		bool parseFilter(FilterModes& mode, int64_t& size);
		void updateSearchList(SearchInfo* si = NULL);
		void addSearchResult(SearchInfo* si);
		
		LRESULT onItemChangedHub(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		
		void speak(Speakers s, const Client* aClient)
		{
			HubInfo* hubInfo = new HubInfo(Text::toT(aClient->getHubUrl()), Text::toT(aClient->getHubName()), aClient->getMyIdentity().isOp());
			PostMessage(WM_SPEAKER, WPARAM(s), LPARAM(hubInfo));
		}
};

#endif // !defined(SEARCH_FRM_H)

/**
 * @file
 * $Id: SearchFrm.h,v 1.79 2006/10/22 18:57:56 bigmuscle Exp $
 */

