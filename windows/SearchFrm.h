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

#if !defined(SEARCH_FRM_H)
#define SEARCH_FRM_H

#pragma once

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "WinUtil.h"
#include "UCHandler.h"

#include "../client/UserInfoBase.h"
#include "../client/SearchManager.h"
#include "../client/ClientManagerListener.h"
#include "../client/QueueManager.h"
#include "../client/SearchResult.h"
#include "../client/ShareManager.h"
#include "../FlyFeatures/GradientLabel.h"
#include "../FlyFeatures/flyServer.h"

//#include "wtlbuilder/Panel.h"

//#ifdef _DEBUG
#define FLYLINKDC_USE_TREE_SEARCH
//#endif

#define SEARCH_MESSAGE_MAP 6
#define SHOWUI_MESSAGE_MAP 7
#define SEARCH_FILTER_MESSAGE_MAP 11


// #define SEARH_TREE_MESSAGE_MAP 9

//#define FLYLINKDC_USE_ADVANCED_GRID_SEARCH
#ifdef FLYLINKDC_USE_ADVANCED_GRID_SEARCH
#include "../client/WildcardsReg.h"
#include "grid/CGrid.h"
using namespace net::r_eg::ui;
using namespace net::r_eg::text::wildcards;
#endif

#define FLYLINKDC_USE_WINDOWS_TIMER_SEARCH_FRAME
// � �������� �������� � ���� ������ �������� �� �����.
class HIconWrapper;
class SearchFrame : public MDITabChildWindowImpl < SearchFrame, RGB(127, 127, 255), IDR_SEARCH >,
	private SearchManagerListener, private ClientManagerListener,
	public UCHandler<SearchFrame>, public UserInfoBaseHandler<SearchFrame, UserInfoGuiTraits::NO_COPY>,
	private SettingsManagerListener
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	, public PreviewBaseHandler<SearchFrame>
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
	, boost::noncopyable
#endif
#ifdef FLYLINKDC_USE_ADVANCED_GRID_SEARCH
	, public ICGridEventKeys
#endif
{
		friend class DirectoryListingFrame;
	public:
		static void openWindow(const tstring& str = Util::emptyStringT, LONGLONG size = 0, Search::SizeModes mode = Search::SIZE_ATLEAST, Search::TypeModes type = Search::TYPE_ANY);
		static void closeAll();
		
		DECLARE_FRAME_WND_CLASS_EX(_T("SearchFrame"), IDR_SEARCH, 0, COLOR_3DFACE)
		
		typedef MDITabChildWindowImpl < SearchFrame, RGB(127, 127, 255), IDR_SEARCH > baseClass;
		typedef UCHandler<SearchFrame> ucBase;
		typedef UserInfoBaseHandler<SearchFrame, UserInfoGuiTraits::NO_COPY> uicBase;
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		typedef PreviewBaseHandler<SearchFrame> prevBase;
#endif
		BEGIN_MSG_MAP(SearchFrame)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_GETDISPINFO, ctrlResults.onGetDispInfo)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_COLUMNCLICK, ctrlResults.onColumnClick)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_GETINFOTIP, ctrlResults.onInfoTip)
		NOTIFY_HANDLER(IDC_HUB, LVN_GETDISPINFO, ctrlHubs.onGetDispInfo)
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		NOTIFY_HANDLER(IDC_HUB, NM_CUSTOMDRAW, ctrlHubs.onCustomDraw)
#endif
		NOTIFY_HANDLER(IDC_RESULTS, NM_DBLCLK, onDoubleClickResults)
		NOTIFY_HANDLER(IDC_RESULTS, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_HUB, LVN_ITEMCHANGED, onItemChangedHub)
		NOTIFY_HANDLER(IDC_RESULTS, NM_CUSTOMDRAW, onCustomDraw)
#ifdef FLYLINKDC_USE_ADVANCED_GRID_SEARCH
		NOTIFY_CODE_HANDLER(PIN_ITEMCHANGED, onGridItemChanged);
		NOTIFY_CODE_HANDLER(PIN_CLICK, onGridItemClick);
#endif
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
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
		COMMAND_ID_HANDLER(IDC_REMOVE, onRemove)
		COMMAND_ID_HANDLER(IDC_SEARCH, onSearch)
		COMMAND_ID_HANDLER(IDC_SEARCH_PAUSE, onPause)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TORRENT_DATE, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TORRENT_COMMENT, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TORRENT_URL, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TORRENT_PAGE, onCopy)
		
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
		COMMAND_ID_HANDLER(IDC_COPY_HUB_URL, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_FULL_MAGNET_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
#ifdef IRAINMAN_SEARCH_OPTIONS
		COMMAND_ID_HANDLER(IDC_HUB, onHubChange)
#endif
		COMMAND_ID_HANDLER(IDC_PURGE, onPurge)
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_SEARCH_FRAME, onCloseAll)
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_CODE_HANDLER(CBN_EDITCHANGE, onEditChange)
		COMMAND_ID_HANDLER(IDC_DOWNLOADTO, onDownloadTo)
		COMMAND_ID_HANDLER(IDC_FILETYPES, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_SEARCH_SIZEMODE, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_SEARCH_SIZE, onFiletypeChange)
		COMMAND_ID_HANDLER(IDC_SEARCH_MODE, onFiletypeChange)
#ifdef FLYLINKDC_USE_TREE_SEARCH
		NOTIFY_HANDLER(IDC_TRANSFER_TREE, TVN_SELCHANGED, onSelChangedTree);
#endif
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_FAVORITE_DIRS, IDC_DOWNLOAD_FAVORITE_DIRS + 499, onDownload)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS, IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS + 499, onDownloadWhole)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET, IDC_DOWNLOAD_TARGET + 499, onDownloadTarget)
		COMMAND_RANGE_HANDLER(IDC_PRIORITY_PAUSED, IDC_PRIORITY_HIGHEST, onDownloadWithPrio)
		
#ifdef FLYLINKDC_USE_ADVANCED_GRID_SEARCH
		REFLECT_NOTIFICATIONS()
#endif
		
		CHAIN_COMMANDS(ucBase)
		CHAIN_COMMANDS(uicBase)
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		CHAIN_COMMANDS(prevBase)
#endif
		CHAIN_MSG_MAP(baseClass)
		ALT_MSG_MAP(SEARCH_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CHAR, onChar)
		MESSAGE_HANDLER(WM_KEYDOWN, onChar)
		MESSAGE_HANDLER(WM_KEYUP, onChar)
		ALT_MSG_MAP(SHOWUI_MESSAGE_MAP)
		MESSAGE_HANDLER(BM_SETCHECK, onShowUI)
		ALT_MSG_MAP(SEARCH_FILTER_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, onCtlColor)
		MESSAGE_HANDLER(WM_KEYUP, onFilterChar)
		COMMAND_CODE_HANDLER(CBN_SELCHANGE, onSelChange)
		END_MSG_MAP()
		
		SearchFrame();
		~SearchFrame();
		
		LRESULT onFiletypeChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onChar(UINT uMsg, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onClose(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
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
		LRESULT onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onFilterChar(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		LRESULT onSelChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPurge(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGetList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onBrowseList(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onEditChange(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPause(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
#ifdef FLYLINKDC_USE_ADVANCED_GRID_SEARCH
		LRESULT onKeyHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled);
		LRESULT onGridItemChanged(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/);
		LRESULT onGridItemClick(int idCtrl, LPNMHDR pnmh, BOOL& /*bHandled*/);
#endif
		
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
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept override;
#endif
#ifdef FLYLINKDC_USE_TREE_SEARCH
		LRESULT onSelChangedTree(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
#endif
		
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void runUserCommand(UserCommand& uc);
		void onSizeMode();
		void removeSelected();
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
			m_onlyFree = ctrlSlots.GetCheck() == BST_CHECKED;
			return 0;
		}
		
		LRESULT onCollapsed(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onSearch(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			onEnter();
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
		
		
		LRESULT onCloseWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			PostMessage(WM_CLOSE);
			return 0;
		}
		
		LRESULT onCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			closeAll();
			return 0;
		}
		
	private:
		tstring getDownloadDirectory(WORD wID);
		class SearchInfo;
		
	public:
		typedef MediainfoTypedTreeListViewCtrl<SearchInfo, IDC_RESULTS, TTHValue > SearchInfoList;
		SearchInfoList& getUserList()
		{
			return ctrlResults;
		}
#ifdef _DEBUG
// #define FLYLINKDC_USE_TORRENT_PANEL
#endif
#ifdef FLYLINKDC_USE_TORRENT_PANEL
		CHorSplitterWindow m_hzSplit;
		CSplitterWindow m_vSplit;
		CPaneContainer m_lPane;
		CPaneContainer m_tPane;
#endif
		
		
	private:
#ifdef FLYLINKDC_USE_ADVANCED_GRID_SEARCH
		/** time difference */
		clock_t _keypressPrevTime;
		
		/** permission to perform */
		bool filterAllowRunning;
		
		void updatePrevTimeFilter()
		{
			_keypressPrevTime = clock();
		}
		
		/**
		 * delay after the last key press
		 * pause - in msec delay
		 */
		void waitPerformAllow(clock_t pause = 300)
		{
			while ((clock() - _keypressPrevTime) < pause)
			{
#ifdef _DEBUG
				dcdebug("filter: wait...\r\n");
#endif
				Sleep(100);
			}
		};
		enum
		{
			FILTER_ITEM_FIRST       = COLUMN_LAST,
			FILTER_ITEM_PATHFILE    = FILTER_ITEM_FIRST,
			FILTER_ITEM_LAST
		};
		
		enum FilterGridColumns
		{
			FGC_INVERT,
			FGC_TYPE,
			FGC_FILTER,
			FGC_ADD,
			FGC_REMOVE
		};
		
		bool doFilter(WPARAM wParam);
		void filtering(SearchInfo* si);
		void insertIntofilter(CGrid& grid);
		vector<LPCTSTR> getFilterTypes();
#endif
		enum
		{
			COLUMN_FIRST,
			COLUMN_FILENAME = COLUMN_FIRST,
			COLUMN_LOCAL_PATH,
			COLUMN_HITS,
			COLUMN_NICK,
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
			COLUMN_ANTIVIRUS,
#endif
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
			COLUMN_HUB,
			COLUMN_EXACT_SIZE,
			COLUMN_LOCATION,
			COLUMN_IP,
#ifdef FLYLINKDC_USE_DNS
			COLUMN_DNS,
#endif
			COLUMN_TTH,
			COLUMN_P2P_GUARD,
			COLUMN_TORRENT_COMMENT,
			COLUMN_TORRENT_DATE,
			COLUMN_TORRENT_URL,
			COLUMN_TORRENT_TRACKER,
			COLUMN_TORRENT_PAGE,
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
		{
			public:
				typedef SearchInfo* Ptr;
				typedef vector<Ptr> Array;
				
				SearchInfo(const SearchResult &aSR) : m_sr(aSR), collapsed(true), parent(nullptr),
					m_hits(0), m_icon_index(-1), m_is_flush_ip_to_sqlite(false),
					m_is_torrent(false), m_is_top_torrent(false)
				{
				}
				~SearchInfo()
				{
				}
				
				const UserPtr& getUser() const override
				{
					return m_sr.getUser();
				}
				bool m_is_torrent;
				bool m_is_top_torrent;
				bool collapsed;
				SearchInfo* parent;
				size_t m_hits;
				int m_icon_index;
				
				void getList();
				void browseList();
				
				void view();
				struct Download
				{
					Download(const tstring& aTarget, SearchFrame* aSf, QueueItem::Priority aPrio, QueueItem::MaskType aMask = 0) : m_tgt(aTarget), m_sf(aSf), prio(aPrio), mask(aMask) { }
					void operator()(const SearchInfo* si);
					const tstring m_tgt;
					SearchFrame* m_sf;
					QueueItem::Priority prio;
					QueueItem::MaskType mask;
				};
				struct DownloadWhole
				{
					DownloadWhole(const tstring& aTarget) : m_tgt(aTarget) { }
					void operator()(const SearchInfo* si);
					const tstring m_tgt;
				};
				struct DownloadTarget
				{
					DownloadTarget(const tstring& aTarget) : m_tgt(aTarget) { }
					void operator()(const SearchInfo* si);
					const tstring m_tgt;
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
				Util::CustomNetworkIndex m_location;
				bool m_is_flush_ip_to_sqlite;
				SearchResult m_sr;
				tstring columns[COLUMN_LAST];
				const TTHValue& getGroupCond() const
				{
					return m_sr.getTTH();
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
				return col == 0 ? name : Util::emptyStringT;
			}
			static int compareItems(const HubInfo* a, const HubInfo* b, int col)
			{
				return col == 0 ? Util::DefaultSort(a->name.c_str(), b->name.c_str()) : 0;
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
			HUB_ADDED,
			HUB_CHANGED,
			HUB_REMOVED,
			QUEUE_STATS,
			UPDATE_STATUS,
			PREPARE_RESULT_TORRENT,
			PREPARE_RESULT_TOP_TORRENT
		};
		
		tstring m_initialString;
		int64_t m_initialSize;
		Search::SizeModes m_initialMode;
		Search::TypeModes m_initialType;
		
		CStatusBarCtrl ctrlStatus;
		CEdit ctrlSearch;
		CComboBox ctrlSearchBox;
		void init_last_search_box();
		
		CEdit ctrlSize;
		CComboBox ctrlMode;
		CComboBox ctrlSizeMode;
		CComboBox ctrlFiletype;
		CImageList m_searchTypesImageList;
		CButton ctrlPurge;
		CButton ctrlPauseSearch;
		CButton ctrlDoSearch;
		
		CHyperLink m_SearchHelp;
		
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		CButton m_ctrlFlyServer;
		//CContainedWindow m_FlyServerContainer;
		CGradientLabelCtrl m_FlyServerGradientLabel;
		//CContainedWindow m_FlyServerGradientContainer;
#endif
		CFlyToolTipCtrl m_tooltip;
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
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
		//CContainedWindow storeIPContainer;
		CButton m_ctrlStoreIP;
		bool m_storeIP;
#endif
		//CContainedWindow storeSettingsContainer;
		CContainedWindow showUIContainer;
		//CContainedWindow purgeContainer;
		//CContainedWindow doSearchContainer;
		//CContainedWindow doSearchPassiveContainer;
		
		CContainedWindow resultsContainer;
		CContainedWindow hubsContainer;
		CContainedWindow ctrlFilterContainer;
		CContainedWindow ctrlFilterSelContainer;
		tstring m_filter;
		
		CStatic searchLabel, sizeLabel, optionLabel, typeLabel, hubsLabel, srLabel;
#ifdef FLYLINKDC_USE_ADVANCED_GRID_SEARCH
		CStatic srLabelExcl;
		/** from bitbucket.org/3F/sandbox */
		CGrid ctrlGridFilters;
#endif
		CButton ctrlSlots, ctrlShowUI, ctrlCollapsed;
		
		CButton m_ctrlStoreSettings;
		CButton m_ctrlUseGroupTreeSettings;
		CButton m_ctrlUseTorrentSearch;
		CButton m_ctrlUseTorrentRSS;
		bool m_showUI;
		bool m_lastFindTTH;
		bool m_need_resort;
		CImageList images;
		SearchInfoList ctrlResults;
		TypedListViewCtrl<HubInfo, IDC_HUB> ctrlHubs;
		
#ifdef FLYLINKDC_USE_TREE_SEARCH
		
		enum CFlyTreeNodeType
		{
			e_Root = -1,
			e_Ext = 2,
			e_Folder = 3,
			e_Last
		};
		
		//CContainedWindow        m_treeContainer;
		CTreeViewCtrl           m_ctrlSearchFilterTree;
		HTREEITEM   m_RootTreeItem;
		HTREEITEM   m_RootVirusTreeItem;
		HTREEITEM   m_RootTorrentRSSTreeItem;
		HTREEITEM   m_24HTopTorrentTreeItem;
		int m_skull_index;
		HTREEITEM   m_CurrentTreeItem;
		HTREEITEM   m_OldTreeItem;
		std::unordered_map<string, HTREEITEM> m_category_map;
		std::unordered_map<string, HTREEITEM> m_tree_ext_map;
		std::unordered_map<string, HTREEITEM> m_tree_sub_torrent_map;
		std::unordered_map<string, HTREEITEM> m_top_subitem_map;
		std::unordered_set<SearchInfo*> m_si_set;
		FastCriticalSection m_si_set_cs;
		std::unordered_map<HTREEITEM, std::vector<std::pair<SearchInfo*, string > > > m_filter_map;
		CriticalSection m_filter_map_cs;
		std::unordered_map<Search::TypeModes, HTREEITEM> m_tree_type;
		bool m_is_expand_tree;
		bool m_is_expand_sub_tree;
		bool is_filter_item(const SearchInfo* si);
		void clear_tree_filter_contaners();
		void set_tree_item_status(const SearchInfo* p_si);
		HTREEITEM add_category(const std::string p_search, std::string p_group, SearchInfo* p_si,
		                       const SearchResult& p_sr, int p_type_node,  HTREEITEM p_parent_node, bool p_force_add = false, bool p_expand = false);
#endif
		void check_delete(SearchInfo*& p_ptr)
		{
			delete p_ptr;
			CFlyFastLock(m_si_set_cs);
			m_si_set.erase(p_ptr);
			p_ptr = nullptr;
		}
		
		OMenu targetMenu;
		OMenu targetDirMenu;
		OMenu priorityMenu;
		OMenu copyMenu;
		OMenu copyMenuTorrent;
		OMenu tabMenu;
		
		StringList m_search;
		StringList targets;
		StringList wholeTargets;
#ifdef FLYLINK_DC_USE_PAUSED_SEARCH
		SearchInfo::Array m_pausedResults;
#endif
		void clearPausedResults();
		
		CEdit ctrlFilter;
		CComboBox ctrlFilterSel;
		
		bool m_onlyFree;
		bool m_isHash;
		bool m_expandSR;
		bool m_storeSettings;
		bool m_is_use_tree;
		bool m_is_disable_torrent_RSS;
		bool m_running;
		bool m_isExactSize;
		bool m_waitingResults;
		bool m_needsUpdateStats;
		bool m_is_before_search;
		
		SearchParamTokenMultiClient m_search_param;
		int64_t m_exactSize2;
		size_t m_resultsCount;
		uint64_t m_searchEndTime;
		uint64_t m_searchStartTime;
		tstring m_target;
		tstring m_statusLine;
		
		FastCriticalSection m_fcs;
		
	public:
		static std::list<wstring> g_lastSearches;
		
	private:
		static HIconWrapper g_purge_icon;
		static HIconWrapper g_pause_icon;
		static HIconWrapper g_search_icon;
		
		static HIconWrapper g_UDPOkIcon;
		static HIconWrapper g_UDPWaitIcon;
		static tstring      g_UDPTestText;
		static boost::logic::tribool g_isUDPTestOK;
		
		CStatic m_ctrlUDPMode;
		CStatic m_ctrlUDPTestResult;
		
		size_t m_droppedResults;
		
		typedef  std::pair<TTHValue, string> CFlyAntivirusKey;
		std::map<CFlyAntivirusKey, std::unordered_set<string>> m_virus_detector;
		size_t check_antivirus_level(const CFlyAntivirusKey& p_key, const SearchResult &p_result, uint8_t p_level);
		static std::unordered_map<TTHValue, uint8_t> g_virus_level_tth_map;
		static std::unordered_set<string> g_virus_file_set;
		static FastCriticalSection g_cs_virus_level;
		static bool isVirusTTH(const TTHValue& p_tth);
		static bool isVirusFileNameCheck(const string& p_file, const TTHValue& p_tth);
		static bool registerVirusLevel(const SearchResult &p_result, int p_level);
		static bool registerVirusLevel(const string& p_file, const TTHValue& p_tth, int p_level);
		HTHEME m_Theme;
		
		StringMap m_ucLineParams;
		
		static int columnIndexes[];
		static int columnSizes[];
		
		typedef std::map<HWND, SearchFrame*> FrameMap;
		typedef pair<HWND, SearchFrame*> FramePair;
		
		static FrameMap g_search_frames;
		
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
		typedef std::map<int, TARGET_STRUCT> TargetsMap;
		TargetsMap dlTargets;
		string m_UDPTestExternalIP;
		void checkUDPTest();
		
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		void mergeFlyServerInfo();
		void runTestPort();
		bool scan_list_view_from_merge();
		typedef std::unordered_map<TTHValue, std::pair<SearchInfo*, CFlyServerCache> > CFlyMergeItem;
		CFlyMergeItem m_merge_item_map; // TODO - ������������ ��� ��� ���������, ����� ������ ��� �� ������ �� ����-������ c get-���������
		void update_column_after_merge(std::vector<int> p_update_index);
		
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
		void downloadSelected(const tstring& aDir, bool view = false);
		void downloadWholeSelected(const tstring& aDir);
		void onEnter();
		void onTab(bool shift);
		int makeTargetMenu(const SearchInfo* p_si);
		void download(SearchResult* aSR, const tstring& aDir, bool view);
		
		void on(SearchManagerListener::SR, const std::unique_ptr<SearchResult>& aResult) noexcept override;
		
		//void on(SearchManagerListener::Searching, SearchQueueItem* aSearch) noexcept override;
		
		// ClientManagerListener
		void on(ClientConnected, const Client* c) noexcept override
		{
			speak(HUB_ADDED, c);
		}
		void on(ClientUpdated, const Client* c) noexcept override
		{
			speak(HUB_CHANGED, c);
		}
		void on(ClientDisconnected, const Client* c) noexcept override
		{
			if (!ClientManager::isShutdown())
			{
				speak(HUB_REMOVED, c);
			}
		}
		void on(SettingsManagerListener::Repaint) override;
		
		void initHubs();
		void onHubAdded(HubInfo* info);
		void onHubChanged(HubInfo* info);
		void onHubRemoved(HubInfo* info);
		bool matchFilter(const SearchInfo* si, int sel, bool doSizeCompare = false, FilterModes mode = NONE, int64_t size = 0);
		bool parseFilter(FilterModes& mode, int64_t& size);
		void updateSearchList(SearchInfo* si = NULL);
#ifdef FLYLINKDC_USE_ADVANCED_GRID_SEARCH
		void updateSearchListSafe(SearchInfo* si = NULL);
#endif
		
		void addSearchResult(SearchInfo* si);
		bool isSkipSearchResult(SearchInfo*& si);
		
		LRESULT onItemChangedHub(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		
		void speak(Speakers s, const Client* aClient);
	private:
		class TorrentTopSender : public Thread
		{
			private:
				HWND m_wnd;
				bool m_is_run;
				int run() override;
			public:
				TorrentTopSender(): m_wnd(0), m_is_run(false) { }
				void start_torrent_top(HWND p_wnd)
				{
					m_wnd = p_wnd;
					if (m_is_run == false)
					{
						m_is_run = true;
						try
						{
							start(1024);
						}
						catch (const ThreadException& e)
						{
							LogManager::message("TorrentTopSender: = " + e.getError());
						}
					}
				}
		} m_torrentRSSThread;
		class TorrentSearchSender : public Thread
		{
			private:
				HWND m_wnd;
				tstring m_search;
				int run() override;
			public:
				TorrentSearchSender(): m_wnd(0) { }
				void start_torrent_search(HWND p_wnd, const tstring& p_search)
				{
					m_wnd = p_wnd;
					m_search = p_search;
					//CFlyBusy l(m_count_run);
					try
					{
						//join();
						start(1024);
					}
					catch (const ThreadException& e)
					{
						LogManager::message("TorrentSearchSender: = " + e.getError());
					}
				}
		} m_torrentSearchThread;
		
};

#endif // !defined(SEARCH_FRM_H)

/**
 * @file
 * $Id: SearchFrm.h,v 1.79 2006/10/22 18:57:56 bigmuscle Exp $
 */

