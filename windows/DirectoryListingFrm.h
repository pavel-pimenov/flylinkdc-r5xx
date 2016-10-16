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

#if !defined(DIRECTORY_LISTING_FRM_H)
#define DIRECTORY_LISTING_FRM_H

#pragma once

#include "FlatTabCtrl.h"
#include "TypedListViewCtrl.h"
#include "UCHandler.h"

#include "../client/DirectoryListing.h"
#include "../client/StringSearch.h"
#include "../client/ADLSearch.h"
#include "../client/ShareManager.h" // !PPA!
#include "../FlyFeatures/VideoPreview.h" // [!] SSA because of WM_ identificator
#include "../client/SettingsManager.h"
#include "../FlyFeatures/flyServer.h"

class ThreadedDirectoryListing;

#define STATUS_MESSAGE_MAP 9
#define CONTROL_MESSAGE_MAP 10
class DirectoryListingFrame : public MDITabChildWindowImpl < DirectoryListingFrame, RGB(255, 0, 255), IDR_FILE_LIST
#ifdef USE_OFFLINE_ICON_FOR_FILELIST
	, IDR_FILE_LIST_OFF // [~] InfinitySky. Вторая иконка.
#endif
	> , public CSplitterImpl<DirectoryListingFrame>,
public UCHandler<DirectoryListingFrame>, private SettingsManagerListener
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	, public CFlyServerAdapter
#endif
	, private CFlyTimerAdapter
	// BUG-MENU , public UserInfoBaseHandler < DirectoryListingFrame, UserInfoGuiTraits::NO_FILE_LIST | UserInfoGuiTraits::NO_COPY >
	, public InternetSearchBaseHandler<DirectoryListingFrame> // [+] IRainman fix.
	, public PreviewBaseHandler<DirectoryListingFrame> // [+] IRainman fix.
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		static void openWindow(const tstring& aFile, const tstring& aDir, const HintedUser& aUser, int64_t aSpeed, bool p_isDCLST = false);
		static void openWindow(const HintedUser& aUser, const string& txt, int64_t aSpeed);
		static void closeAll();
		
		typedef MDITabChildWindowImpl < DirectoryListingFrame, RGB(255, 0, 255), IDR_FILE_LIST
#ifdef USE_OFFLINE_ICON_FOR_FILELIST
		, IDR_FILE_LIST_OFF // [~] InfinitySky. Вторая иконка.
#endif
		> baseClass;
		typedef UCHandler<DirectoryListingFrame> ucBase;
		typedef InternetSearchBaseHandler<DirectoryListingFrame> isBase; // [+] IRainman fix.
		// BUG-MENU  typedef UserInfoBaseHandler < DirectoryListingFrame, UserInfoGuiTraits::NO_FILE_LIST | UserInfoGuiTraits::NO_COPY > uiBase;
		typedef PreviewBaseHandler<DirectoryListingFrame> prevBase; // [+] IRainamn fix.
		
		enum
		{
			COLUMN_FILENAME,
			COLUMN_TYPE,
			COLUMN_EXACTSIZE,
			COLUMN_SIZE,
			COLUMN_TTH,
			COLUMN_PATH,
			COLUMN_HIT,
			COLUMN_TS,
			COLUMN_FLY_SERVER_RATING,
			COLUMN_BITRATE,
			COLUMN_MEDIA_XY,
			COLUMN_MEDIA_VIDEO,
			COLUMN_MEDIA_AUDIO,
			COLUMN_DURATION,
			
			COLUMN_LAST
		};
		
		enum
		{
			FINISHED,
			ABORTED
		};
		
		enum
		{
			STATUS_TEXT,
			STATUS_SPEED,
			STATUS_TOTAL_FILES,
			STATUS_TOTAL_FOLDERS,
			STATUS_TOTAL_SIZE,
			STATUS_SELECTED_FILES,
			STATUS_SELECTED_SIZE,
			STATUS_FILE_LIST_DIFF,
			STATUS_MATCH_QUEUE,
			STATUS_FIND,
			STATUS_NEXT,
			STATUS_DUMMY,
			STATUS_LAST
		};
		FileImage::TypeDirectoryImages GetTypeDirectory(const DirectoryListing::Directory* dir) const;
		
		DirectoryListingFrame(const HintedUser& aUser, int64_t aSpeed);
		~DirectoryListingFrame();
		
		DECLARE_FRAME_WND_CLASS(_T("DirectoryListingFrame"), IDR_FILE_LIST)
		
		BEGIN_MSG_MAP(DirectoryListingFrame)
		NOTIFY_HANDLER(IDC_FILES, LVN_GETDISPINFO, ctrlList.onGetDispInfo)
		NOTIFY_HANDLER(IDC_FILES, LVN_COLUMNCLICK, ctrlList.onColumnClick)
		NOTIFY_HANDLER(IDC_FILES, NM_CUSTOMDRAW, onCustomDrawList) // !fulDC!
		NOTIFY_HANDLER(IDC_FILES, LVN_KEYDOWN, onKeyDown)
		NOTIFY_HANDLER(IDC_FILES, NM_DBLCLK, onDoubleClickFiles)
		NOTIFY_HANDLER(IDC_FILES, LVN_ITEMCHANGED, onItemChanged)
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
		NOTIFY_HANDLER(IDC_FILES, NM_CUSTOMDRAW, ctrlList.onCustomDraw) // [+] IRainman
#endif
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_KEYDOWN, onKeyDownDirs)
		NOTIFY_HANDLER(IDC_DIRECTORIES, TVN_SELCHANGED, onSelChangedDirectories)
		NOTIFY_HANDLER(IDC_DIRECTORIES, NM_CUSTOMDRAW, onCustomDrawTree) // !fulDC!
		MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBackground)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_CONTEXTMENU, onContextMenu)
		MESSAGE_HANDLER(WM_CLOSE, onClose)
		MESSAGE_HANDLER(WM_TIMER, onTimer)
		MESSAGE_HANDLER(WM_SETFOCUS, onSetFocus)
		MESSAGE_HANDLER(FTM_CONTEXTMENU, onTabContextMenu)
		MESSAGE_HANDLER(WM_SPEAKER, onSpeaker)
		COMMAND_ID_HANDLER(IDC_OPEN_FILE, onOpenFile) // !SMT!-UI
		COMMAND_ID_HANDLER(IDC_OPEN_FOLDER, onOpenFolder) // [+] NightOrion
		COMMAND_ID_HANDLER(IDC_DOWNLOAD, onDownload)
		COMMAND_ID_HANDLER(IDC_DOWNLOADDIR, onDownloadDir)
		COMMAND_ID_HANDLER(IDC_DOWNLOADDIRTO, onDownloadDirTo)
		COMMAND_ID_HANDLER(IDC_DOWNLOADTO, onDownloadTo)
		COMMAND_ID_HANDLER(IDC_GO_TO_DIRECTORY, onGoToDirectory)
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		COMMAND_ID_HANDLER(IDC_VIEW_AS_TEXT, onViewAsText)
#endif
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		MESSAGE_HANDLER(WM_SPEAKER_MERGE_FLY_SERVER, onMergeFlyServerResult)
		COMMAND_ID_HANDLER(IDC_VIEW_FLYSERVER_INFORM, onShowFlyServerProperty)
#ifdef _DEBUG
		COMMAND_ID_HANDLER(IDC_GET_TTH_MEDIAINFO_SERVER, onGetTTHMediainfoServer)
		COMMAND_ID_HANDLER(IDC_SET_TTH_MEDIAINFO_SERVER, onSetTTHMediainfoServer)
#endif
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
		
		COMMAND_ID_HANDLER(IDC_SEARCH_ALTERNATES, onSearchByTTH)
		COMMAND_ID_HANDLER(IDC_COPY_LINK, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_TTH, onCopy)
		COMMAND_ID_HANDLER(IDC_COPY_WMLINK, onCopy) // !SMT!-UI
		COMMAND_ID_HANDLER(IDC_ADD_TO_FAVORITES, onAddToFavorites)
		COMMAND_ID_HANDLER(IDC_MARK_AS_DOWNLOADED, onMarkAsDownloaded)
		COMMAND_ID_HANDLER(IDC_PRIVATE_MESSAGE, onPM)
		COMMAND_ID_HANDLER(IDC_COPY_NICK, onCopy);
		COMMAND_ID_HANDLER(IDC_COPY_FILENAME, onCopy);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		COMMAND_ID_HANDLER(IDC_COPY_FLYSERVER_INFORM, onCopy)
#endif
		COMMAND_ID_HANDLER(IDC_COPY_SIZE, onCopy);
		COMMAND_ID_HANDLER(IDC_CLOSE_ALL_DIR_LIST, onCloseAll) // [+] InfinitySky.
		COMMAND_ID_HANDLER(IDC_CLOSE_WINDOW, onCloseWindow)
		COMMAND_ID_HANDLER(IDC_GENERATE_DCLST, onGenerateDCLST) // [+] SSA
		COMMAND_ID_HANDLER(IDC_GENERATE_DCLST_FILE, onGenerateDCLST) // [+] SSA
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET, IDC_DOWNLOAD_TARGET + targets.size() + LastDir::get().size(), onDownloadTarget)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_TARGET_DIR, IDC_DOWNLOAD_TARGET_DIR + LastDir::get().size(), onDownloadTargetDir)
		COMMAND_RANGE_HANDLER(IDC_PRIORITY_PAUSED, IDC_PRIORITY_HIGHEST, onDownloadWithPrio)
		COMMAND_RANGE_HANDLER(IDC_PRIORITY_PAUSED + 90, IDC_PRIORITY_HIGHEST + 90, onDownloadDirWithPrio)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_FAVORITE_DIRS, IDC_DOWNLOAD_FAVORITE_DIRS + FavoriteManager::getFavoriteDirsCount(), onDownloadFavoriteDirs)
		COMMAND_RANGE_HANDLER(IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS, IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS + FavoriteManager::getFavoriteDirsCount(), onDownloadWholeFavoriteDirs)
		CHAIN_COMMANDS(isBase) // [+] IRainamn fix.
		CHAIN_COMMANDS(prevBase) // [+] IRainamn fix.
		// BUG-MENU CHAIN_COMMANDS(uiBase) // [+] IRainamn fix.
		CHAIN_COMMANDS(ucBase)
		CHAIN_MSG_MAP(baseClass)
		CHAIN_MSG_MAP(CSplitterImpl<DirectoryListingFrame>)
		ALT_MSG_MAP(STATUS_MESSAGE_MAP)
		COMMAND_ID_HANDLER(IDC_FIND, onFind)
		COMMAND_ID_HANDLER(IDC_NEXT, onNext)
		COMMAND_ID_HANDLER(IDC_MATCH_QUEUE, onMatchQueue)
		COMMAND_ID_HANDLER(IDC_FILELIST_DIFF, onListDiff)
		ALT_MSG_MAP(CONTROL_MESSAGE_MAP)
		MESSAGE_HANDLER(WM_XBUTTONUP, onXButtonUp)
		END_MSG_MAP()
		
		LRESULT onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // !SMT!-UI
		LRESULT onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/); // [+] NightOrion
		LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		LRESULT onMergeFlyServerResult(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
#endif
		LRESULT onDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadDirWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadDirTo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadTo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		LRESULT onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
		LRESULT onSearchFileInInternet(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			const auto ii = ctrlList.getSelectedItem();
			if (ii && ii->type == ItemInfo::FILE)
			{
				searchFileInInternet(wID, ii->m_file->getName());
			}
			return 0;
		}
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		LRESULT onShowFlyServerProperty(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			const auto ii = ctrlList.getSelectedItem();
			if (ii && ii->type == ItemInfo::FILE)
			{
				showFlyServerProperty(ii);
			}
			return 0;
		}
#ifdef _DEBUG
		LRESULT onGetTTHMediainfoServer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onSetTTHMediainfoServer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
#endif
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
		LRESULT onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onAddToFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onMarkAsDownloaded(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onGoToDirectory(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadTarget(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadTargetDir(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDoubleClickFiles(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onSelChangedDirectories(int idCtrl, LPNMHDR pnmh, BOOL& bHandled);
		LRESULT onContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onXButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled);
		LRESULT onDownloadFavoriteDirs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onDownloadWholeFavoriteDirs(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled);
		LRESULT onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/);
		LRESULT onCustomDrawList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/); // !fulDC!
		LRESULT onCustomDrawTree(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/); // !fulDC!
		LRESULT onGenerateDCLST(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
		
		void downloadList(const tstring& aTarget, bool view = false,  QueueItem::Priority prio = QueueItem::DEFAULT);
		void downloadList(bool view = false,  QueueItem::Priority prio = QueueItem::DEFAULT)
		{
			downloadList(Util::emptyStringT, view, prio);
		}
		void downloadList(const FavoriteManager::FavDirList& spl, int newId)
		{
			dcassert(newId < (int)spl.size());
			downloadList(Text::toT(spl[newId].dir));
		}
		
		void updateTree(DirectoryListing::Directory* tree, HTREEITEM treeItem);
		void UpdateLayout(BOOL bResizeBars = TRUE);
		void findFile(bool findNext);
		void runUserCommand(UserCommand& uc);
		void loadFile(const tstring& name, const tstring& dir);
		void loadXML(const string& txt);
		void refreshTree(const tstring& root);
		
		// [+] FlylinkDC DCLST
		GETSET(string, fileName, FileName);
		void setDclstFlag(bool p_isDclst)
		{
			m_isDclst = p_isDclst;
		}
		bool isDclst() const
		{
			return m_isDclst;
		}
		// [~] FlylinkDC DCLST
		HTREEITEM findItem(HTREEITEM ht, const tstring& name);
		void selectItem(const tstring& name);
		
		LRESULT onItemChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)
		{
			++m_count_item_changed;
			return 0;
		}
		
		LRESULT onSetFocus(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			ctrlList.SetFocus();
			return 0;
		}
		
		void setWindowTitle()
		{
			if (m_error.empty())
			{
				if (isDclst())
					SetWindowText(Text::toT(Util::getFileName(getFileName())).c_str());
				else if (dl->getUser() && !dl->getUser()->getCID().isZero())
				{
					const pair<tstring, bool> l_hub = WinUtil::getHubNames(dl->getHintedUser());
					const tstring l_nicks = WinUtil::getNicks(dl->getHintedUser());
					SetWindowText((l_nicks + _T(" - ") + l_hub.first).c_str());
				}
			}
			else
				SetWindowText(m_error.c_str());
		}
		
		void addToUserMap(const UserPtr& aUser)// [+] IRainman dclst support
		{
			if (aUser && !aUser->getCID().isZero())
			{
				CFlyLock(g_csUsersMap);
				g_usersMap.insert(UserPair(aUser, this));
			}
		}
		
		LRESULT OnEraseBackground(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
		{
			return 1;
		}
		
		LRESULT onFind(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			m_searching = true;
			findFile(false);
			m_searching = false;
			updateStatus();
			return 0;
		}
		LRESULT onNext(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
		{
			m_searching = true;
			findFile(true);
			m_searching = false;
			updateStatus();
			return 0;
		}
		
		LRESULT onMatchQueue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		LRESULT onListDiff(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
		
		LRESULT onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/);
		
		LRESULT onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
		{
			NMTVKEYDOWN* kd = (NMTVKEYDOWN*) pnmh;
			if (kd->wVKey == VK_TAB)
			{
				onTab();
			}
			return 0;
		}
		
		void onTab()
		{
			const HWND l_focus = ::GetFocus();
			if (l_focus == ctrlTree.m_hWnd)
			{
				ctrlList.SetFocus();
			}
			else if (l_focus == ctrlList.m_hWnd)
			{
				ctrlTree.SetFocus();
			}
		}
		
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
		friend class ThreadedDirectoryListing;
		uint64_t m_FL_LoadSec;
		void getItemColor(const Flags::MaskType flags, COLORREF &fg, COLORREF &bg); // !SMT!-UI
		void changeDir(DirectoryListing::Directory* p_dir);
		HTREEITEM findFile(const StringSearch& str, HTREEITEM root, int &foundFile, int &skipHits);
		void updateStatus();
		void initStatus();
		void addHistory(const string& name);
		void up();
		void back();
		void forward();
		void openFileFromList(const tstring& file); // [+] SSA
		
		class ItemInfo
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			: public CFlyServerInfo
#endif
		{
				friend class DirectoryListingFrame;
			public:
				enum ItemType
				{
					FILE,
					DIRECTORY
				} type;
				
				union
				{
					DirectoryListing::File* m_file; //-V117
					DirectoryListing::Directory* m_dir; //-V117
				};
				
				const tstring& getText(int col) const
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					return columns[col];
				}
				
				struct TotalSize
				{
					TotalSize() : total(0) { }
					void operator()(ItemInfo* a)
					{
						total += a->type == DIRECTORY ? a->m_dir->getTotalSize() : a->m_file->getSize();
					}
					int64_t total;
				};
				
				ItemInfo(DirectoryListing::File* f);
				ItemInfo(DirectoryListing::Directory* d);
				
				static int compareItems(const ItemInfo* a, const ItemInfo* b, int col)
				{
					dcassert(col >= 0 && col < COLUMN_LAST);
					if (a->type == DIRECTORY)
					{
						if (b->type == DIRECTORY)
						{
							switch (col)
							{
								case COLUMN_EXACTSIZE:
									return compare(a->m_dir->getTotalSize(), b->m_dir->getTotalSize());
								case COLUMN_SIZE:
									return compare(a->m_dir->getTotalSize(), b->m_dir->getTotalSize());
								case COLUMN_HIT:
									return compare(a->m_dir->getTotalHit(), b->m_dir->getTotalHit());
								case COLUMN_TS:
									return compare(a->m_dir->getTotalTS(), b->m_dir->getTotalTS());
								default:
									return Util::DefaultSort(a->columns[col].c_str(), b->columns[col].c_str());
							}
						}
						else
						{
							return -1;
						}
					}
					else if (b->type == DIRECTORY)
					{
						return 1;
					}
					else
					{
						switch (col)
						{
							case COLUMN_TYPE:
							{
								return compare(a->columns[COLUMN_TYPE] + _T('~') + a->columns[COLUMN_FILENAME],
								               b->columns[COLUMN_TYPE] + _T('~') + b->columns[COLUMN_FILENAME]);
							}
							case COLUMN_EXACTSIZE:
								return compare(a->m_file->getSize(), b->m_file->getSize());
							case COLUMN_SIZE:
								return compare(a->m_file->getSize(), b->m_file->getSize());
							case COLUMN_HIT:
								return compare(a->m_file->getHit(), b->m_file->getHit());
							case COLUMN_TS:
								return compare(a->m_file->getTS(), b->m_file->getTS());
							case COLUMN_BITRATE:
								if (a->m_file->m_media && b->m_file->m_media)
									return compare(a->m_file->m_media->m_bitrate, b->m_file->m_media->m_bitrate);
								else
									return 0;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
							case COLUMN_FLY_SERVER_RATING:
								return compare(Util::toInt64(a->columns[col]), Util::toInt64(b->columns[col])); // TODO - распарсить x/y
#endif
							case COLUMN_MEDIA_XY:
								if (a->m_file->m_media && b->m_file->m_media)
									return compare(a->m_file->m_media->m_mediaX * 1000000 + a->m_file->m_media->m_mediaY, b->m_file->m_media->m_mediaX * 1000000 + b->m_file->m_media->m_mediaY);
								else
									return 0;
							default:
								return Util::DefaultSort(a->columns[col].c_str(), b->columns[col].c_str(), false);
						}
					}
				}
				int getImageIndex() const
				{
					dcassert(m_icon_index >= 0);
					return m_icon_index;
				}
				static uint8_t getStateImageIndex()
				{
					return 0;
				}
				void calcImageIndex();
				
				void UpdatePathColumn(const DirectoryListing::File* f);
				
			private:
				tstring columns[COLUMN_LAST];
				int m_icon_index;
		};
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		bool showFlyServerProperty(const ItemInfo* p_item_info);
#endif
		OMenu targetMenu;
		OMenu targetDirMenu;
		void appendTargetMenu(OMenu& p_menu, int p_id_menu);
		OMenu directoryMenu;
		OMenu priorityMenu;
		OMenu priorityDirMenu;
		OMenu copyMenu;
		
		CContainedWindow statusContainer;
		CContainedWindow treeContainer;
		CContainedWindow listContainer;
		
		StringList targets;
		
		deque<string> history;
		size_t historyIndex;
		
		CTreeViewCtrl ctrlTree;
		MediainfoTypedListViewCtrl<ItemInfo, IDC_FILES> ctrlList;
		boost::unordered_map<DirectoryListing::Directory*, string> m_selected_file_history;
		DirectoryListing::Directory* m_prev_directory;
		CStatusBarCtrl ctrlStatus;
		HTREEITEM treeRoot;
		
		CButton ctrlFind, ctrlFindNext;
		CButton ctrlListDiff;
		CButton ctrlMatchQueue;
		
		string findStr;
		tstring m_error;
		string size;
		
		int m_skipHits;
		
		size_t files;
		int64_t speed;      /**< Speed at which this file list was downloaded */
		
		bool m_isDclst; // [+] FlylinkDC DCLST
		
		bool m_updating;
		bool m_searching;
		bool m_loading;
		//GETSET(bool, isMyOwnList, IsMyOwnList); // [+] IRainman TODO
		
		int statusSizes[STATUS_LAST];
		
		std::unique_ptr<DirectoryListing> dl;
		
		StringMap ucLineParams;
		
#ifdef USE_OFFLINE_ICON_FOR_FILELIST
		bool isoffline; // [+] InfinitySky.
		void updateTitle(); // [+] InfinitySky. Изменять заголовок окна.
#endif // USE_OFFLINE_ICON_FOR_FILELIST
		
		typedef boost::unordered_map<UserPtr, DirectoryListingFrame*, User::Hash> UserMap;
		typedef pair<UserPtr, DirectoryListingFrame*> UserPair;
		
		static UserMap g_usersMap;
		static CriticalSection g_csUsersMap;
		
		static int columnIndexes[COLUMN_LAST];
		static int columnSizes[COLUMN_LAST];
		
		typedef std::map< HWND , DirectoryListingFrame* > FrameMap;
		typedef pair< HWND , DirectoryListingFrame* > FramePair;
		static FrameMap g_dir_list_frames;
		
		void on(SettingsManagerListener::Repaint) override;
	private:
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		void mergeFlyServerInfo();
		bool scan_list_view_from_merge();
		typedef boost::unordered_map<TTHValue, ItemInfo*> CFlyMergeItem;
		CFlyMergeItem m_merge_item_map; // TODO - организовать кэш для медиаинфы, чтобы лишний раз не ходить на флай-сервер c get-запросами
		void update_column_after_merge(std::vector<int> p_update_index);
#endif
		int m_count_item_changed;
};

class ThreadedDirectoryListing : public Thread
{
	public:
		ThreadedDirectoryListing(DirectoryListingFrame* pWindow,
		                         const string& pFile, const string& pTxt, const tstring& aDir = Util::emptyStringT) : mWindow(pWindow),
			mFile(pFile), mTxt(pTxt), mDir(aDir)
		{ }
		
	protected:
		DirectoryListingFrame* mWindow;
		string mFile;
		string mTxt;
		tstring mDir;
		
	private:
		int run()
		{
			try
			{
				if (!mFile.empty())
				{
					const string l_filename = Util::getFileName(mFile);
					const bool l_list = (_strnicmp(l_filename.c_str(), "files", 5)  // !SMT!-UI
					                     || _strnicmp(l_filename.c_str() + l_filename.length() - 8, ".xml.bz2", 8));
					                     
					const bool l_own_list = _stricmp(l_filename.c_str(), Util::getFileName(ShareManager::getInstance()->getOwnListFile()).c_str()) == 0;
					mWindow->dl->loadFile(mFile, l_own_list);
					mWindow->addToUserMap(mWindow->dl->getUser());
					mWindow->setWindowTitle();
					ADLSearchManager::getInstance()->matchListing(*mWindow->dl);
					if (l_list)
						mWindow->dl->checkDupes(); // !fulDC!
					if (l_own_list)
						mWindow->refreshTree(Text::toT(SETTING(NICK)));
					else
						mWindow->refreshTree(mDir); // mWindow->refreshTree(Text::toT(WinUtil::getInitialDir(mWindow->dl->getUser()))); [!] IRainman merge
				}
				else
				{
					mWindow->refreshTree(Text::toT(Util::toNmdcFile(mWindow->dl->updateXML(mTxt, false))));
				}
				
				mWindow->PostMessage(WM_SPEAKER, DirectoryListingFrame::FINISHED);
			}
			catch (const AbortException&)
			{
				mWindow->PostMessage(WM_SPEAKER, DirectoryListingFrame::ABORTED);
			}
			catch (const Exception& e)
			{
				mWindow->m_error = Text::toT(ClientManager::getNicks(mWindow->dl->getUser()->getCID(),
				                                                     mWindow->dl->getHintedUser().hint)[0] + ": " + e.getError());
				const tstring l_email_message = Text::toT(string("\r\nSend the corrupted file '") + mFile + "' to e-mail ppa74@ya.ru for analyze and correct the error!");
				::MessageBox(NULL, (mWindow->m_error + l_email_message).c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
				mWindow->PostMessage(WM_SPEAKER, DirectoryListingFrame::ABORTED);
			}
			
			//cleanup the thread object
			delete this;
			
			return 0;
		}
};

#endif // !defined(DIRECTORY_LISTING_FRM_H)

/**
 * @file
 * $Id: DirectoryListingFrm.h,v 1.52 2006/10/22 18:57:56 bigmuscle Exp $
 */
