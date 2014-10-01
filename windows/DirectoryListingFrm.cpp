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

#include "../client/QueueManager.h"
#include "../client/ClientManager.h"
#include "../client/util_flylinkdc.h"
#include "DirectoryListingFrm.h"
#include "LineDlg.h"
#include "PrivateFrame.h"
#include "dclstGenDlg.h"
#include "MainFrm.h"
#include "BarShader.h"
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
#include "../FlyFeatures/flyServer.h"
#include "../FlyFeatures/CFlyServerDialogNavigator.h"
#include "../jsoncpp/include/json/value.h"
#include "../jsoncpp/include/json/reader.h"
#include "../jsoncpp/include/json/writer.h"
#endif

DirectoryListingFrame::FrameMap DirectoryListingFrame::g_frames;
int DirectoryListingFrame::columnIndexes[] = { COLUMN_FILENAME, COLUMN_TYPE, COLUMN_EXACTSIZE, COLUMN_SIZE, COLUMN_TTH,
                                               COLUMN_PATH, COLUMN_HIT, COLUMN_TS,
                                               COLUMN_FLY_SERVER_RATING,
                                               COLUMN_BITRATE, COLUMN_MEDIA_XY, COLUMN_MEDIA_VIDEO, COLUMN_MEDIA_AUDIO, COLUMN_DURATION
                                             }; // !PPA!
int DirectoryListingFrame::columnSizes[] = { 300, 60, 100, 100, 200, 300, 30, 100,
                                             // COLUMN_FLY_SERVER_RATING
                                             50,
                                             50, 100, 100, 100, 30
                                           };

static ResourceManager::Strings columnNames[] = { ResourceManager::FILE, ResourceManager::TYPE, ResourceManager::EXACT_SIZE,
                                                  ResourceManager::SIZE, ResourceManager::TTH_ROOT , ResourceManager::PATH, ResourceManager::DOWNLOADED,
                                                  ResourceManager::ADDED,
                                                  ResourceManager::FLY_SERVER_RATING, // COLUMN_FLY_SERVER_RATING
                                                  ResourceManager::BITRATE, ResourceManager::MEDIA_X_Y,
                                                  ResourceManager::MEDIA_VIDEO, ResourceManager::MEDIA_AUDIO, ResourceManager::DURATION
                                                }; //TODO // !PPA!

DirectoryListingFrame::UserMap DirectoryListingFrame::g_usersMap;
CriticalSection DirectoryListingFrame::g_csUsersMap;

DirectoryListingFrame::~DirectoryListingFrame()
{
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	dcassert(m_merge_item_map.empty());
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
	Lock l(g_csUsersMap);
	if (dl->getUser() && !dl->getUser()->getCID().isZero() && g_usersMap.find(dl->getUser()) != g_usersMap.end())
		g_usersMap.erase(dl->getUser());
}

void DirectoryListingFrame::openWindow(const tstring& aFile, const tstring& aDir, const HintedUser& aHintedUser, int64_t aSpeed, bool p_isDCLST /*= false*/)
{
	bool l_is_users_map_exists;
	{
		Lock l(g_csUsersMap);
		auto i = g_usersMap.end();
		if (!p_isDCLST && aHintedUser.user && !aHintedUser.user->getCID().isZero())
			i = g_usersMap.find(aHintedUser);
			
		l_is_users_map_exists = i != g_usersMap.end();
		if (l_is_users_map_exists)
		{
			if (!BOOLSETTING(POPUNDER_FILELIST))
			{
				i->second->speed = aSpeed;
				i->second->MDIActivate(i->second->m_hWnd);
			}
		}
	}
	if (!l_is_users_map_exists)
	{
		HWND aHWND = NULL;
		DirectoryListingFrame* frame = new DirectoryListingFrame(aHintedUser, aSpeed);
		frame->setDclstFlag(p_isDCLST);
		frame->setFileName(Text::fromT(aFile));
		if (BOOLSETTING(POPUNDER_FILELIST))
		{
			aHWND = WinUtil::hiddenCreateEx(frame);
		}
		else
		{
			aHWND = frame->CreateEx(WinUtil::mdiClient);
		}
		if (aHWND != 0)
		{
			frame->loadFile(aFile, aDir);
			g_frames.insert(FramePair(frame->m_hWnd, frame));
		}
		else
		{
			delete frame;
		}
	}
}

void DirectoryListingFrame::openWindow(const HintedUser& aUser, const string& txt, int64_t aSpeed)
{
	bool l_is_users_map_exists;
	{
		Lock l(g_csUsersMap);
		auto i = g_usersMap.find(aUser);
		l_is_users_map_exists = i != g_usersMap.end();
		if (l_is_users_map_exists)
		{
			i->second->speed = aSpeed;
			i->second->loadXML(txt);
		}
	}
	if (!l_is_users_map_exists)
	{
		DirectoryListingFrame* frame = new DirectoryListingFrame(aUser, aSpeed);
		if (BOOLSETTING(POPUNDER_FILELIST))
		{
			WinUtil::hiddenCreateEx(frame);
		}
		else
		{
			frame->CreateEx(WinUtil::mdiClient);
		}
		frame->loadXML(txt);
		g_frames.insert(FramePair(frame->m_hWnd, frame));
	}
}

DirectoryListingFrame::DirectoryListingFrame(const HintedUser& aHintedUser, int64_t aSpeed) :
	CFlyTimerAdapter(m_hWnd),
	CFlyServerAdapter(m_hWnd),
	statusContainer(STATUSCLASSNAME, this, STATUS_MESSAGE_MAP), treeContainer(WC_TREEVIEW, this, CONTROL_MESSAGE_MAP),
	listContainer(WC_LISTVIEW, this, CONTROL_MESSAGE_MAP), historyIndex(0), m_loading(true),
	treeRoot(NULL), m_skipHits(0), files(0), speed(aSpeed), m_updating(false),
#ifdef USE_OFFLINE_ICON_FOR_FILELIST
	isoffline(false), /* <[+] InfinitySky */
#endif // USE_OFFLINE_ICON_FOR_FILELIST
	dl(new DirectoryListing(aHintedUser)), m_searching(false), m_isDclst(false),
	m_count_item_changed(0),
	m_prev_directory(nullptr)
{
	addToUserMap(aHintedUser); // [!] IRainman dclst support
}

void DirectoryListingFrame::loadFile(const tstring& name, const tstring& dir)
{
	m_FL_LoadSec = GET_TICK();
	ctrlStatus.SetText(0, CTSTRING(PROCESSING_FILE_LIST));
	//don't worry about cleanup, the object will delete itself once the thread has finished it's job
	ThreadedDirectoryListing* tdl = new ThreadedDirectoryListing(this, Text::fromT(name), Util::emptyString, dir);
	m_loading = true;
	tdl->start(0);
}

void DirectoryListingFrame::loadXML(const string& txt)
{
	m_FL_LoadSec = GET_TICK();
	ctrlStatus.SetText(0, CTSTRING(PROCESSING_FILE_LIST));
	//don't worry about cleanup, the object will delete itself once the thread has finished it's job
	ThreadedDirectoryListing* tdl = new ThreadedDirectoryListing(this, Util::emptyString, txt);
	m_loading = true;
	tdl->start(0);
}

LRESULT DirectoryListingFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{

	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	ctrlStatus.SetFont(Fonts::g_boldFont); // [~] Sergey Shuhskanov
	statusContainer.SubclassWindow(ctrlStatus.m_hWnd);
	
	ctrlTree.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP, WS_EX_CLIENTEDGE, IDC_DIRECTORIES);
	
	if (BOOLSETTING(USE_EXPLORER_THEME)
#ifdef FLYLINKDC_SUPPORT_WIN_2000
	        && CompatibilityManager::IsXPPlus()
#endif
	   )
		SetWindowTheme(ctrlTree.m_hWnd, L"explorer", NULL);
		
	treeContainer.SubclassWindow(ctrlTree);
	ctrlList.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_FILES);
	listContainer.SubclassWindow(ctrlList);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlList);
	
	SET_LIST_COLOR(ctrlList);
	ctrlTree.SetBkColor(Colors::bgColor);
	ctrlTree.SetTextColor(Colors::textColor);
	
	WinUtil::splitTokens(columnIndexes, SETTING(DIRECTORYLISTINGFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(DIRECTORYLISTINGFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (size_t j = 0; j < COLUMN_LAST; j++) //-V104
	{
		const int fmt = ((j == COLUMN_SIZE) || (j == COLUMN_EXACTSIZE) || (j == COLUMN_TYPE) || (j == COLUMN_HIT)) ? LVCFMT_RIGHT : LVCFMT_LEFT; //-V104
		ctrlList.InsertColumn(j, TSTRING_I(columnNames[j]), fmt, columnSizes[j], j); //-V107
	}
	ctrlList.setColumnOrderArray(COLUMN_LAST, columnIndexes); //-V106
	ctrlList.setVisible(SETTING(DIRECTORYLISTINGFRAME_VISIBLE));
	
	//ctrlList.setSortColumn(COLUMN_FILENAME);
	ctrlList.setSortColumn(SETTING(DIRLIST_COLUMNS_SORT));
	ctrlList.setAscending(BOOLSETTING(DIRLIST_COLUMNS_SORT_ASC));
	
	ctrlTree.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlList.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	
	ctrlFind.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                BS_PUSHBUTTON, 0, IDC_FIND);
	ctrlFind.SetWindowText(CTSTRING(FIND));
	ctrlFind.SetFont(Fonts::g_systemFont);
	
	ctrlFindNext.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    BS_PUSHBUTTON, 0, IDC_NEXT);
	ctrlFindNext.SetWindowText(CTSTRING(NEXT));
	ctrlFindNext.SetFont(Fonts::g_systemFont);
	
	ctrlMatchQueue.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                      BS_PUSHBUTTON, 0, IDC_MATCH_QUEUE);
	ctrlMatchQueue.SetWindowText(CTSTRING(MATCH_QUEUE));
	ctrlMatchQueue.SetFont(Fonts::g_systemFont);
	
	ctrlListDiff.Create(ctrlStatus.m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                    BS_PUSHBUTTON, 0, IDC_FILELIST_DIFF);
	ctrlListDiff.SetWindowText(CTSTRING(FILE_LIST_DIFF));
	ctrlListDiff.SetFont(Fonts::g_systemFont);
	
	SetSplitterExtendedStyle(SPLIT_PROPORTIONAL);
	SetSplitterPanes(ctrlTree.m_hWnd, ctrlList.m_hWnd);
	m_nProportionalPos = SETTING(DIRECTORYLISTINGFRAME_SPLIT);
	string nick = isDclst() ? Util::getFileName(getFileName()) : (dl->getUser() ? dl->getUser()->getLastNick() : Util::emptyString); // [!] IRainman dclst support
	treeRoot = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM,
	                               Text::toT(nick).c_str(), isDclst() ? FileImage::DIR_DSLCT : FileImage::DIR_ICON,
	                               isDclst() ? FileImage::DIR_DSLCT : FileImage::DIR_ICON, 0, 0,
	                               (LPARAM)dl->getRoot(), NULL, TVI_SORT); // [!] SSA - adds DCLST or Directory icon here.
	dcassert(treeRoot != NULL);
	
	memzero(statusSizes, sizeof(statusSizes));
	statusSizes[STATUS_FILE_LIST_DIFF] = WinUtil::getTextWidth(TSTRING(FILE_LIST_DIFF), m_hWnd) + 8;
	statusSizes[STATUS_MATCH_QUEUE] = WinUtil::getTextWidth(TSTRING(MATCH_QUEUE), m_hWnd) + 8;
	statusSizes[STATUS_FIND] = WinUtil::getTextWidth(TSTRING(FIND), m_hWnd) + 8;
	statusSizes[STATUS_NEXT] = WinUtil::getTextWidth(TSTRING(NEXT), m_hWnd) + 8;
	
	ctrlStatus.SetParts(STATUS_LAST, statusSizes);
	
	targetMenu.CreatePopupMenu();
	directoryMenu.CreatePopupMenu();
	targetDirMenu.CreatePopupMenu();
	priorityMenu.CreatePopupMenu();
	priorityDirMenu.CreatePopupMenu();
	copyMenu.CreatePopupMenu();
	
	if (!ClientManager::isMe(dl->getUser())) // Don't append COPY_NICK if browse main share
	{
		copyMenu.AppendMenu(MF_STRING, IDC_COPY_NICK, CTSTRING(COPY_NICK));
	}
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_FLYSERVER_INFORM, CTSTRING(FLY_SERVER_INFORM));
#endif
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_SIZE, CTSTRING(SIZE));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_TTH, CTSTRING(TTH_ROOT));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
	copyMenu.AppendMenu(MF_STRING, IDC_COPY_WMLINK, CTSTRING(COPY_MLINK_TEMPL)); // !SMT!-UI
	
	directoryMenu.AppendMenu(MF_STRING, IDC_DOWNLOADDIR, CTSTRING(DOWNLOAD));
	directoryMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)targetDirMenu, CTSTRING(DOWNLOAD_TO));
	directoryMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityDirMenu, CTSTRING(DOWNLOAD_WITH_PRIORITY));
	directoryMenu.AppendMenu(MF_SEPARATOR);
	directoryMenu.AppendMenu(MF_STRING, IDC_GENERATE_DCLST, CTSTRING(DCLS_GENERATE_LIST));
//	directoryMenu.AppendMenu(MF_SEPARATOR);
//	directoryMenu.AppendMenu(MF_STRING, IDC_ADD_TO_FAVORITES, CTSTRING(ADD_TO_FAVORITES)); // [-] NightOrion : Добавлять файлы и папки в друзья О_о ?

	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSED));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_LOWEST, CTSTRING(LOWEST));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_LOW, CTSTRING(LOW));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_NORMAL, CTSTRING(NORMAL));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_HIGH, CTSTRING(HIGH));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_HIGHEST, CTSTRING(HIGHEST));
	
	// TODO not use magical constant!!!! Why is 90????
	priorityDirMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED + 90, CTSTRING(PAUSED));
	priorityDirMenu.AppendMenu(MF_STRING, IDC_PRIORITY_LOWEST + 90, CTSTRING(LOWEST));
	priorityDirMenu.AppendMenu(MF_STRING, IDC_PRIORITY_LOW + 90, CTSTRING(LOW));
	priorityDirMenu.AppendMenu(MF_STRING, IDC_PRIORITY_NORMAL + 90, CTSTRING(NORMAL));
	priorityDirMenu.AppendMenu(MF_STRING, IDC_PRIORITY_HIGH + 90, CTSTRING(HIGH));
	priorityDirMenu.AppendMenu(MF_STRING, IDC_PRIORITY_HIGHEST + 90, CTSTRING(HIGHEST));
	
	ctrlTree.EnableWindow(FALSE);
	SettingsManager::getInstance()->addListener(this);
	m_closed = false;
	setWindowTitle();
	bHandled = FALSE;
	m_count_item_changed = 0;
	create_timer(1000); // Раз в 1 секунду. TODO - настройку частоты запросов унести в конфиг
	return 1;
}

void DirectoryListingFrame::updateTree(DirectoryListing::Directory* aTree, HTREEITEM aParent)
{
	if (aTree)
	{
		for (auto i = aTree->directories.cbegin(); i != aTree->directories.cend(); ++i)
		{
			if (!m_loading)
			{
				throw AbortException(STRING(ABORT_EM));
			}
			
			tstring name = Text::toT((*i)->getName()); // 2012-04-29_06-52-32_N3Q2H3UJNJDFO5S756HDN37HCS7EYEXQEHWMJKA_B3975CEC_crash-stack-r502-beta23-build-9860.dmp
			
			// Определяем иконку для папки
			const auto typeDirectory = GetTypeDirectory(*i);
			
			HTREEITEM ht = ctrlTree.InsertItem(TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_TEXT | TVIF_PARAM, name.c_str(), typeDirectory, typeDirectory, 0, 0, (LPARAM) * i, aParent, TVI_SORT);
			if ((*i)->getAdls())
				ctrlTree.SetItemState(ht, TVIS_BOLD, TVIS_BOLD);
				
			updateTree(*i, ht);
		}
	}
}

void DirectoryListingFrame::refreshTree(const tstring& root)
{
	if (!m_loading)
	{
		throw AbortException(STRING(ABORT_EM));
	}
	
	CLockRedraw<> l_lock_draw(ctrlTree);
	
	HTREEITEM ht = findItem(treeRoot, root);
	if (ht == nullptr)
	{
		ht = treeRoot;
	}
	
	if (DirectoryListing::Directory* d = (DirectoryListing::Directory*)ctrlTree.GetItemData(ht))
	{
//+BugMaster: fix crash when subtract filelists and selected node is not root
		ctrlTree.SelectItem(NULL);
//-BugMaster: fix crash when subtract filelists and selected node is not root

		HTREEITEM next = nullptr;
		while ((next = ctrlTree.GetChildItem(ht)) != NULL)
		{
			ctrlTree.DeleteItem(next);
		}
		
		updateTree(d, ht);
		
		ctrlTree.Expand(treeRoot);
		
		if (d)
		{
			// Определяем иконку для папки
			const auto typeDirectory = GetTypeDirectory(d);
			
			ctrlTree.SetItemImage(ht, typeDirectory, typeDirectory);
		}
		
		ctrlTree.SelectItem(NULL);
		selectItem(root);
	}
}

void DirectoryListingFrame::updateStatus()
{
	if (!isClosedOrShutdown() &&
	        !m_searching &&
	        !m_updating &&
	        ctrlStatus.IsWindow())
	{
		int cnt = ctrlList.GetSelectedCount();
		int64_t total = 0;
		if (cnt == 0)
		{
			cnt = ctrlList.GetItemCount();
			total = ctrlList.forEachT(ItemInfo::TotalSize()).total;
		}
		else
		{
			total = ctrlList.forEachSelectedT(ItemInfo::TotalSize()).total;
		}
		
		tstring tmp = TSTRING(ITEMS) + _T(": ") + Util::toStringW(cnt);
		bool u = false;
		
		int w = WinUtil::getTextWidth(tmp, ctrlStatus.m_hWnd);
		if (statusSizes[STATUS_SELECTED_FILES] < w)
		{
			statusSizes[STATUS_SELECTED_FILES] = w;
			u = true;
		}
		ctrlStatus.SetText(STATUS_SELECTED_FILES, tmp.c_str());
		
		tmp = TSTRING(SIZE) + _T(": ") + Util::formatBytesW(total);
		w = WinUtil::getTextWidth(tmp, ctrlStatus.m_hWnd);
		if (statusSizes[STATUS_SELECTED_SIZE] < w)
		{
			statusSizes[STATUS_SELECTED_SIZE] = w;
			u = true;
		}
		ctrlStatus.SetText(STATUS_SELECTED_SIZE, tmp.c_str());
		
		if (u)
			UpdateLayout(TRUE);
			
		m_count_item_changed = 0;
	}
}

void DirectoryListingFrame::initStatus()
{
	files = dl->getTotalFileCount();
	size = Util::formatBytes(dl->getTotalSize());
	
	tstring tmp = TSTRING(FILES) + _T(": ") + Util::toStringW(files);
	statusSizes[STATUS_TOTAL_FILES] = WinUtil::getTextWidth(tmp, m_hWnd);
	ctrlStatus.SetText(STATUS_TOTAL_FILES, tmp.c_str());
	
	tmp = TSTRING(FOLDERS) + _T(": ") + Util::toStringW(dl->getTotalFolderCount());
	statusSizes[STATUS_TOTAL_FOLDERS] = WinUtil::getTextWidth(tmp, m_hWnd);
	ctrlStatus.SetText(STATUS_TOTAL_FOLDERS, tmp.c_str());
	
	tmp = TSTRING(SIZE) + _T(": ") + Util::formatBytesW(dl->getTotalSize(true));
	statusSizes[STATUS_TOTAL_SIZE] = WinUtil::getTextWidth(tmp, m_hWnd);
	ctrlStatus.SetText(STATUS_TOTAL_SIZE, tmp.c_str());
	
	tmp = TSTRING(SPEED) + _T(": ") + Util::formatBytesW(speed) + _T('/') + WSTRING(S);
	statusSizes[STATUS_SPEED] = WinUtil::getTextWidth(tmp, m_hWnd);
	ctrlStatus.SetText(STATUS_SPEED, tmp.c_str());
	
	UpdateLayout(FALSE);
}

LRESULT DirectoryListingFrame::onSelChangedDirectories(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTREEVIEW* p = (NMTREEVIEW*) pnmh;
	
	if (p->itemNew.state & TVIS_SELECTED)
	{
		DirectoryListing::Directory* d = (DirectoryListing::Directory*)p->itemNew.lParam;
		changeDir(d);
		addHistory(dl->getPath(d));
	}
	return 0;
}

void DirectoryListingFrame::addHistory(const string& name)
{
	history.erase(history.begin() + historyIndex, history.end());
	while (history.size() > 25)
		history.pop_front();
	history.push_back(name);
	historyIndex = history.size();
}

// Метод определяет иконку для папки в зависимости от ее содержимого
FileImage::TypeDirectoryImages DirectoryListingFrame::GetTypeDirectory(const DirectoryListing::Directory* dir) const
{
	if (!dir->getComplete())
	{
		// Папка пустая
		return FileImage::DIR_MASKED;
	}
	
	// Проверяем все подпапки в папке
	for (auto i = dir->directories.cbegin(); i != dir->directories.cend(); ++i)
	{
		const string& nameSubDirectory = (*i)->getName();
		
		// Папка содержащая хотя бы одну папку BDMV, является Blu-ray папкой
		if (FileImage::isBdFolder(nameSubDirectory))
		{
			return FileImage::DIR_BD; // Папка Blu-ray
		}
		
		// Папка содержащая подпапки VIDEO_TS или AUDIO_TS, является DVD папкой
		if (FileImage::isDvdFolder(nameSubDirectory))
		{
			return FileImage::DIR_DVD; // Папка DVD
		}
	}
	
	// Проверяем все файлы в папке
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const string& nameFile = (*i)->getName();
		if (FileImage::isDvdFile(nameFile))
		{
			return FileImage::DIR_DVD; // Папка DVD
		}
	}
	
	return FileImage::DIR_ICON;
}

void DirectoryListingFrame::changeDir(DirectoryListing::Directory* p_dir)
{
	CWaitCursor l_cursor_wait; //-V808
	CLockRedraw<> l_lock_draw(ctrlList);
	{
		CFlyBusy l_busy(m_updating);
		// http://code.google.com/p/flylinkdc/issues/detail?id=1223
		auto& l_prev_selected_file = m_selected_file_history[m_prev_directory];
		string l_cur_selected_item_name = m_selected_file_history[p_dir];
		if (const auto& l_cur_item = ctrlList.getSelectedItem())
		{
			if (m_prev_directory)
			{
				l_prev_selected_file = Text::fromT(l_cur_item->getText(COLUMN_FILENAME));
			}
		}
		m_prev_directory = p_dir;
		ctrlList.DeleteAndCleanAllItemsNoLock();
		auto l_count = ctrlList.GetItemCount();
		ItemInfo* l_last_item = nullptr;
		for (auto i = p_dir->directories.cbegin(); i != p_dir->directories.cend(); ++i)
		{
			ItemInfo* ii = new ItemInfo(*i);
			const auto& l_name = (*i)->getName();
			if (!l_cur_selected_item_name.empty() && l_name == l_cur_selected_item_name)
			{
				l_last_item = ii;
			}
			ctrlList.insertItem(l_count++, ii, I_IMAGECALLBACK); // GetTypeDirectory(*i)
		}
		for (auto j = p_dir->files.cbegin(); j != p_dir->files.cend(); ++j)
		{
			ItemInfo* ii = new ItemInfo(*j);
			const auto& l_name = (*j)->getName();
			if (!l_cur_selected_item_name.empty() && l_name == l_cur_selected_item_name)
			{
				l_last_item = ii;
			}
			ctrlList.insertItem(l_count++, ii, I_IMAGECALLBACK);
		}
		ctrlList.resort();
		if (l_last_item)
		{
			const auto l_sel_item = ctrlList.findItem(l_last_item);
			ctrlList.SelectItem(l_sel_item); // Возвращаемся на запомненный файл.
		}
	}
	updateStatus();
	
	if (!p_dir->getComplete())
	{
		if (dl->getUser()->isOnline())
		{
			try
			{
				QueueManager::getInstance()->addList(dl->getHintedUser(), QueueItem::FLAG_PARTIAL_LIST, dl->getPath(p_dir));
				ctrlStatus.SetText(STATUS_TEXT, CTSTRING(DOWNLOADING_LIST));
			}
			catch (const QueueException& e)
			{
				ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
			}
		}
		else
		{
			ctrlStatus.SetText(STATUS_TEXT, CTSTRING(USER_OFFLINE));
		}
	}
}

void DirectoryListingFrame::up()
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t == NULL)
		return;
	t = ctrlTree.GetParentItem(t);
	if (t == NULL)
		return;
	ctrlTree.SelectItem(t);
}

void DirectoryListingFrame::back()
{
	if (history.size() > 1 && historyIndex > 1)
	{
		size_t n = min(historyIndex, history.size()) - 1;
		deque<string> tmp = history;
		selectItem(Text::toT(history[n - 1]));
		historyIndex = n;
		history = tmp;
	}
}

void DirectoryListingFrame::forward()
{
	if (history.size() > 1 && historyIndex < history.size())
	{
		size_t n = min(historyIndex, history.size() - 1);
		deque<string> tmp = history;
		selectItem(Text::toT(history[n]));
		historyIndex = n + 1;
		history = tmp;
	}
}

// !SMT!-UI
LRESULT DirectoryListingFrame::onOpenFile(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(i);
		// !necros!
		string rp = ShareManager::getInstance()->toRealPath(ii->file->getTTH());
		if (!rp.empty())
		{
			openFileFromList(Text::toT(rp));
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onOpenFolder(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	if ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		//[-] PVS-Studio V808 tstring l_param;
		if (const ItemInfo *ii = ctrlList.getItemData(i))
		{
			WinUtil::openFolder(Text::toT(ShareManager::getInstance()->toRealPath(ii->file->getTTH())));
		}
	}
	return 0;
}
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
bool DirectoryListingFrame::showFlyServerProperty(const ItemInfo* p_item_info)
{
	// Проверим медиаинфу и если есть - покажем
	CFlyServerDialogNavigator l_dlg;
	static const int l_mediainfo_array[] =
	{COLUMN_BITRATE , COLUMN_MEDIA_XY, COLUMN_MEDIA_VIDEO , COLUMN_MEDIA_AUDIO, COLUMN_DURATION, COLUMN_FLY_SERVER_RATING};
	for (int i = 0; i < _countof(l_mediainfo_array); ++i)
	{
		const auto l_MIItem = p_item_info->getText(l_mediainfo_array[i]);
		if (!l_MIItem.empty())
			l_dlg.m_MediaInfo.push_back(make_pair(CTSTRING_I(columnNames[l_mediainfo_array[i]]), l_MIItem));
	}
	static const int l_fileinfo_array[] =
	{COLUMN_FILENAME, COLUMN_TYPE, COLUMN_EXACTSIZE, COLUMN_SIZE, COLUMN_TTH, COLUMN_PATH, COLUMN_HIT, COLUMN_TS};
	for (int i = 0; i < _countof(l_fileinfo_array); ++i)
		l_dlg.m_FileInfo.push_back(make_pair(CTSTRING_I(columnNames[l_fileinfo_array[i]]), p_item_info->getText(l_fileinfo_array[i])));
		
	const string l_inform = CFlyServerInfo::getMediaInfoAsText(p_item_info->file->getTTH(), p_item_info->file->getSize());
	
	l_dlg.m_MIInform.push_back(make_pair(_T("General"), Text::toT(l_inform)));
	
	const auto l_result = l_dlg.DoModal(m_hWnd);
	return l_result == IDOK;
}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
LRESULT DirectoryListingFrame::onDoubleClickFiles(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	const NMITEMACTIVATE* item = (NMITEMACTIVATE*) pnmh;
	const HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t != NULL && item->iItem != -1)
	{
		const ItemInfo* ii = (ItemInfo*) ctrlList.GetItemData(item->iItem);
		if (ii->type == ItemInfo::FILE)
		{
			// !necros!
			const string rp = ShareManager::getInstance()->toRealPath(ii->file->getTTH());
			if (!rp.empty())
			{
				openFileFromList(Text::toT(rp));
			}
			else
				try
				{
					if (Util::isDclstFile(ii->file->getName()))
						dl->download(ii->file, Text::fromT(ii->getText(COLUMN_FILENAME)), true, WinUtil::isShift(), QueueItem::DEFAULT, true);
					else
						dl->download(ii->file, Text::fromT(ii->getText(COLUMN_FILENAME)), false, WinUtil::isShift(), QueueItem::DEFAULT);
						
				}
				catch (const Exception& e)
				{
					ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
				}
		}
		else
		{
			HTREEITEM ht = ctrlTree.GetChildItem(t);
			while (ht != NULL)
			{
				if ((DirectoryListing::Directory*)ctrlTree.GetItemData(ht) == ii->dir)
				{
					ctrlTree.SelectItem(ht);
					break;
				}
				ht = ctrlTree.GetNextSiblingItem(ht);
			}
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadDir(WORD , WORD , HWND , BOOL&)
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t != NULL)
	{
		DirectoryListing::Directory* dir = (DirectoryListing::Directory*)ctrlTree.GetItemData(t);
		try
		{
			dl->download(dir, SETTING(DOWNLOAD_DIRECTORY), WinUtil::isShift(), QueueItem::DEFAULT);
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadDirWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t != NULL)
	{
		DirectoryListing::Directory* dir = (DirectoryListing::Directory*)ctrlTree.GetItemData(t);
		
		QueueItem::Priority p;
		switch (wID - 90)
		{
			case IDC_PRIORITY_PAUSED:
				p = QueueItem::PAUSED;
				break;
			case IDC_PRIORITY_LOWEST:
				p = QueueItem::LOWEST;
				break;
			case IDC_PRIORITY_LOW:
				p = QueueItem::LOW;
				break;
			case IDC_PRIORITY_NORMAL:
				p = QueueItem::NORMAL;
				break;
			case IDC_PRIORITY_HIGH:
				p = QueueItem::HIGH;
				break;
			case IDC_PRIORITY_HIGHEST:
				p = QueueItem::HIGHEST;
				break;
			default:
				p = QueueItem::DEFAULT;
				break;
		}
		
		try
		{
			dl->download(dir, SETTING(DOWNLOAD_DIRECTORY), WinUtil::isShift(), p);
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadDirTo(WORD , WORD , HWND , BOOL&)
{
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t != NULL)
	{
		CWaitCursor l_cursor_wait; //-V808
		DirectoryListing::Directory* dir = (DirectoryListing::Directory*)ctrlTree.GetItemData(t);
		tstring target = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
		if (WinUtil::browseDirectory(target, m_hWnd))
		{
			LastDir::add(target);
			
			try
			{
				dl->download(dir, Text::fromT(target), WinUtil::isShift(), QueueItem::DEFAULT);
			}
			catch (const Exception& e)
			{
				ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
			}
		}
	}
	return 0;
}

void DirectoryListingFrame::downloadList(const tstring& aTarget, bool view /* = false */, QueueItem::Priority prio /* = QueueItem::Priority::DEFAULT */)
{
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(i);
		
		const tstring& target = aTarget.empty() ? Text::toT(FavoriteManager::getInstance()->getDownloadDirectory(Text::fromT(Util::getFileExt(ii->getText(COLUMN_FILENAME))))) : aTarget;
		
		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				if (view)
				{
					File::deleteFileT(target + Text::toT(Util::validateFileName(ii->file->getName())));
				}
				dl->download(ii->file, Text::fromT(target + ii->getText(COLUMN_FILENAME)), view, WinUtil::isShift() || view, prio);
			}
			else if (!view)
			{
				dl->download(ii->dir, Text::fromT(target), WinUtil::isShift(), prio);
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
}

LRESULT DirectoryListingFrame::onDownload(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	downloadList();
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadWithPrio(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	QueueItem::Priority p;
	
	switch (wID)
	{
		case IDC_PRIORITY_PAUSED:
			p = QueueItem::PAUSED;
			break;
		case IDC_PRIORITY_LOWEST:
			p = QueueItem::LOWEST;
			break;
		case IDC_PRIORITY_LOW:
			p = QueueItem::LOW;
			break;
		case IDC_PRIORITY_NORMAL:
			p = QueueItem::NORMAL;
			break;
		case IDC_PRIORITY_HIGH:
			p = QueueItem::HIGH;
			break;
		case IDC_PRIORITY_HIGHEST:
			p = QueueItem::HIGHEST;
			break;
		default:
			p = QueueItem::DEFAULT;
			break;
	}
	
	downloadList(false, p);
	
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadTo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlList.GetSelectedCount() == 1)
	{
		ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		
		try
		{
			if (ii->type == ItemInfo::FILE)
			{
				tstring target = Text::toT(SETTING(DOWNLOAD_DIRECTORY)) + ii->getText(COLUMN_FILENAME);
				if (WinUtil::browseFile(target, m_hWnd))
				{
					LastDir::add(Util::getFilePath(target));
					dl->download(ii->file, Text::fromT(target), false, WinUtil::isShift(), QueueItem::DEFAULT);
				}
			}
			else
			{
				tstring target = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
				if (WinUtil::browseDirectory(target, m_hWnd))
				{
					LastDir::add(target);
					dl->download(ii->dir, Text::fromT(target), WinUtil::isShift(), QueueItem::DEFAULT);
				}
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
	else
	{
		tstring target = Text::toT(SETTING(DOWNLOAD_DIRECTORY));
		if (WinUtil::browseDirectory(target, m_hWnd))
		{
			LastDir::add(target);
			downloadList(target);
		}
	}
	return 0;
}
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
LRESULT DirectoryListingFrame::onViewAsText(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	downloadList(Text::toT(Util::getTempPath()), true);
	return 0;
}
#endif
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
#ifdef _DEBUG
LRESULT DirectoryListingFrame::onGetTTHMediainfoServer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	CFlyServerKeyArray l_array;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* pItemInfo = reinterpret_cast<ItemInfo*>(ctrlList.GetItemData(i));
		if (pItemInfo->type == ItemInfo::FILE)
		{
			const DirectoryListing::File* pFile = pItemInfo->file;
			CFlyServerKey l_info(pFile->getTTH(), pFile->getSize()); // pFile->getName()
			l_array.push_back(l_info);
		}
		else
		{
			// Recursively add directory content?
		}
	}
	CFlyServerJSON::connect(l_array, false);
	return 0;
}
LRESULT DirectoryListingFrame::onSetTTHMediainfoServer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	CFlyServerKeyArray l_array;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* pItemInfo = reinterpret_cast<ItemInfo*>(ctrlList.GetItemData(i));
		if (pItemInfo->type == ItemInfo::FILE)
		{
			const DirectoryListing::File* pFile = pItemInfo->file;
			CFlyServerKey l_info(pFile->getTTH(), pFile->getSize()); // , pFile->getName()
			if (pFile->m_media)
			{
				l_info.m_media = *pFile->m_media;
			}
			l_info.m_hit = pFile->getHit();
			l_info.m_time_hash = pFile->getTS();
			CFlylinkDBManager::getInstance()->load_media_info(pFile->getTTH(), l_info.m_media, false);
			l_array.push_back(l_info);
		}
		else
		{
			// Recursively add directory content?
		}
	}
	CFlyServerJSON::connect(l_array, true);
	return 0;
}
#endif // _DEBUG
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER

LRESULT DirectoryListingFrame::onSearchByTTH(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ItemInfo* ii = ctrlList.getSelectedItem();
	if (ii && ii->type == ItemInfo::FILE)
	{
		WinUtil::searchHash(ii->file->getTTH());
	}
	return 0;
}

LRESULT DirectoryListingFrame::onAddToFavorites(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (const UserPtr& pUser = dl->getUser())
		FavoriteManager::getInstance()->addFavoriteUser(pUser);
	return 0;
}

LRESULT DirectoryListingFrame::onMarkAsDownloaded(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		ItemInfo* pItemInfo = reinterpret_cast<ItemInfo*>(ctrlList.GetItemData(i));
		if (pItemInfo->type == ItemInfo::FILE)
		{
			DirectoryListing::File* pFile = pItemInfo->file;
			CFlylinkDBManager::getInstance()->push_download_tth(pFile->getTTH());
			pFile->setFlag(DirectoryListing::FLAG_DOWNLOAD);
			pItemInfo->UpdatePathColumn(pFile);
			ctrlList.updateItem(pItemInfo);
		}
		else
		{
			// Recursively add directory content?
		}
	}
	
	return 0;
}

LRESULT DirectoryListingFrame::onPM(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const HintedUser& pUser = dl->getHintedUser();
	if (pUser.user)
	{
		PrivateFrame::openWindow(nullptr, pUser);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onMatchQueue(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	const int x = QueueManager::getInstance()->matchListing(*dl);
	const auto l_size = STRING(MATCHED_FILES).length() + 32;
	AutoArray<TCHAR> buf(l_size);
	snwprintf(buf.data(), l_size, CTSTRING(MATCHED_FILES), x);
	ctrlStatus.SetText(STATUS_TEXT, buf.data());
	return 0;
}

LRESULT DirectoryListingFrame::onListDiff(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring file;
	if (WinUtil::browseFile(file, m_hWnd, false, Text::toT(Util::getListPath()), FILE_LIST_TYPE_T))
	{
		DirectoryListing dirList(dl->getHintedUser());
		try
		{
			dirList.loadFile(Text::fromT(file));
			dl->getRoot()->filterList(dirList);
			m_loading = true;
			refreshTree(Util::emptyStringT);
			m_loading = false;
			initStatus();
			updateStatus();
		}
		catch (const Exception&)
		{
			/// @todo report to user?
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onGoToDirectory(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlList.GetSelectedCount() != 1)
		return 0;
		
	tstring fullPath;
	ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
	if (ii->type == ItemInfo::FILE)
	{
		if (!ii->file->getAdls())
			return 0;
		DirectoryListing::Directory* pd = ii->file->getParent();
		while (pd != NULL && pd != dl->getRoot())
		{
			fullPath = Text::toT(pd->getName()) + _T("\\") + fullPath;
			pd = pd->getParent();
		}
	}
	else if (ii->type == ItemInfo::DIRECTORY)
	{
		if (!(ii->dir->getAdls() && ii->dir->getParent() != dl->getRoot()))
			return 0;
		fullPath = Text::toT(((DirectoryListing::AdlDirectory*)ii->dir)->getFullPath());
	}
	
	selectItem(fullPath);
	
	return 0;
}

HTREEITEM DirectoryListingFrame::findItem(HTREEITEM ht, const tstring& name)
{
	string::size_type i = name.find('\\');
	if (i == string::npos)
		return ht;
		
	for (HTREEITEM child = ctrlTree.GetChildItem(ht); child != NULL; child = ctrlTree.GetNextSiblingItem(child))
	{
		DirectoryListing::Directory* d = (DirectoryListing::Directory*)ctrlTree.GetItemData(child);
		if (Text::toT(d->getName()) == name.substr(0, i))
		{
			return findItem(child, name.substr(i + 1));
		}
	}
	return NULL;
}

void DirectoryListingFrame::selectItem(const tstring& name)
{
	const HTREEITEM ht = findItem(treeRoot, name);
	if (ht != NULL)
	{
		ctrlTree.EnsureVisible(ht);
		ctrlTree.SelectItem(ht);
	}
}

#ifdef USE_OFFLINE_ICON_FOR_FILELIST
// [+] InfinitySky. Изменять иконку окна.
void DirectoryListingFrame::updateTitle()
{
	pair<tstring, bool> hubs = WinUtil::getHubNames(dl->getHintedUser());
	bool banIcon = FavoriteManager::getInstance()->isNoFavUserOrUserBanUpload(dl->getUser()); // !SMT!-UI
	if (hubs.second)
	{
		if (banIcon) // !SMT!-UI
		{
			setCustomIcon(WinUtil::g_banIconOnline);
		}
		else
		{
			unsetIconState();
		}
		setTabColor(RGB(0, 255, 255));
		isoffline = false;
	}
	else
	{
		if (banIcon) // !SMT!-UI
		{
			setCustomIcon(WinUtil::g_banIconOffline);
		}
		else
		{
			setIconState();
		}
		setTabColor(RGB(255, 0, 0));
		isoffline = true;
	}
	SetWindowText(dl->getUser()->getLastNickT() + _T(" - ") + hubs.first).c_str());
}
// [+] InfinitySky. END.
#endif // USE_OFFLINE_ICON_FOR_FILELIST

void DirectoryListingFrame::appendTargetMenu(OMenu& p_menu, const int p_id_menu)
{
	FavoriteManager::LockInstanceDirs lockedInstance;
	const auto& dirs = lockedInstance.getFavoriteDirsL();
	if (!dirs.empty())
	{
		int n = 0;
		for (auto i = dirs.cbegin(); i != dirs.cend(); ++i)
		{
			p_menu.AppendMenu(MF_STRING, p_id_menu + n, Text::toT(i->name).c_str());
			n++;
		}
		p_menu.AppendMenu(MF_SEPARATOR);
	}
}

LRESULT DirectoryListingFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	if (reinterpret_cast<HWND>(wParam) == ctrlList && ctrlList.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlList, pt);
		}
		
		ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		
		while (targetMenu.GetMenuItemCount() > 0)
		{
			targetMenu.DeleteMenu(0, MF_BYPOSITION);
		}
		
		OMenu fileMenu;
		fileMenu.CreatePopupMenu();
		clearPreviewMenu();
		
		fileMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD, CTSTRING(DOWNLOAD));
		fileMenu.AppendMenu(MF_STRING, IDC_OPEN_FILE, CTSTRING(OPEN)); // !SMT!-UI
		fileMenu.AppendMenu(MF_STRING, IDC_OPEN_FOLDER, CTSTRING(OPEN_FOLDER)); // [+] NightOrion
		fileMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)targetMenu, CTSTRING(DOWNLOAD_TO));
		fileMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(DOWNLOAD_WITH_PRIORITY));
#ifdef FLYLINKDC_USE_VIEW_AS_TEXT_OPTION
		fileMenu.AppendMenu(MF_STRING, IDC_VIEW_AS_TEXT, CTSTRING(VIEW_AS_TEXT));
#endif
		fileMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
		fileMenu.AppendMenu(MF_STRING, IDC_MARK_AS_DOWNLOADED, CTSTRING(MARK_AS_DOWNLOADED));
		fileMenu.AppendMenu(MF_SEPARATOR);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		fileMenu.AppendMenu(MF_STRING, IDC_VIEW_FLYSERVER_INFORM, CTSTRING(FLY_SERVER_INFORM_VIEW));
#endif
		appendPreviewItems(fileMenu);
		fileMenu.AppendMenu(MF_STRING, IDC_GENERATE_DCLST_FILE, CTSTRING(DCLS_GENERATE_LIST)); // [+] SSA
		fileMenu.AppendMenu(MF_SEPARATOR);
		fileMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY));
		fileMenu.AppendMenu(MF_SEPARATOR);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
# ifdef _DEBUG
		fileMenu.AppendMenu(MF_STRING, IDC_SET_TTH_MEDIAINFO_SERVER, _T("Mediainfo server - set-test!")); // TODO Убрать совсем
		fileMenu.AppendMenu(MF_STRING, IDC_GET_TTH_MEDIAINFO_SERVER, _T("Mediainfo server - get-test!")); // TODO Убрать совсем
		fileMenu.AppendMenu(MF_SEPARATOR);
# endif
#endif
		appendInternetSearchItems(fileMenu); // [!] IRainman fix.
		
		// [+] SCALOlaz: prepare for swap Item text https://code.google.com/p/flylinkdc/issues/detail?id=887
		const int l_ipos = WinUtil::GetMenuItemPosition(copyMenu, IDC_COPY_FILENAME);
		
		if (ctrlList.GetSelectedCount() == 1 && ii->type == ItemInfo::FILE)
		{
			fileMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, MF_BYCOMMAND | MFS_ENABLED);
			fileMenu.EnableMenuItem(IDC_SEARCH_FILE_IN_GOOGLE, MF_BYCOMMAND | MFS_ENABLED);
			fileMenu.EnableMenuItem(IDC_SEARCH_FILE_IN_YANDEX, MF_BYCOMMAND | MFS_ENABLED);
			fileMenu.EnableMenuItem(IDC_GENERATE_DCLST_FILE, MF_BYCOMMAND | MFS_DISABLED); // [+] SSA
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			fileMenu.EnableMenuItem(IDC_VIEW_FLYSERVER_INFORM, MF_BYCOMMAND | MFS_ENABLED);
#endif
			//Append Favorite download dirs.
			appendTargetMenu(targetMenu, IDC_DOWNLOAD_FAVORITE_DIRS);
			
			int n = 0;
			targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOADTO, CTSTRING(BROWSE));
			targets.clear();
			QueueManager::getInstance()->getTargets(ii->file->getTTH(), targets);
			
			if (!targets.empty())
			{
				targetMenu.AppendMenu(MF_SEPARATOR);
				for (auto i = targets.cbegin(); i != targets.cend(); ++i)
				{
					targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TARGET + (++n), Text::toT(*i).c_str());
				}
			}
			LastDir::appendItem(targetMenu, n);
			
			// !SMT!-UI
			const auto existingFile = !ShareManager::getInstance()->toRealPath(ii->file->getTTH()).empty();
			activatePreviewItems(fileMenu, existingFile);
			if (existingFile)
			{
				fileMenu.SetMenuDefaultItem(IDC_OPEN_FILE);
				fileMenu.EnableMenuItem(IDC_OPEN_FILE, MF_BYCOMMAND | MFS_ENABLED);
				fileMenu.EnableMenuItem(IDC_OPEN_FOLDER, MF_BYCOMMAND | MFS_ENABLED);
			}
			else
			{
				fileMenu.SetMenuDefaultItem(IDC_DOWNLOAD);
				fileMenu.EnableMenuItem(IDC_OPEN_FILE, MF_BYCOMMAND | MFS_DISABLED);
				fileMenu.EnableMenuItem(IDC_OPEN_FOLDER, MF_BYCOMMAND | MFS_DISABLED);
			}
			
			
			if (ii->file->getAdls())
			{
				fileMenu.AppendMenu(MF_STRING, IDC_GO_TO_DIRECTORY, CTSTRING(GO_TO_DIRECTORY));
			}
			// [+] SCALOlaz: swap Item text https://code.google.com/p/flylinkdc/issues/detail?id=887
			if (l_ipos != -1)
			{
				copyMenu.ModifyMenu(l_ipos, MF_BYPOSITION | MF_STRING, IDC_COPY_FILENAME, CTSTRING(FILENAME));
			}
			
			//fileMenu.EnableMenuItem((UINT)(HMENU)copyMenu, MF_BYCOMMAND | MFS_ENABLED);
			appendUcMenu(fileMenu, UserCommand::CONTEXT_FILELIST, ClientManager::getHubs(dl->getUser()->getCID(), dl->getHintedUser().hint));
#ifdef OLD_MENU_HEADER //[~]JhaoDa
			fileMenu.InsertSeparatorFirst(Text::CropStrLength(ii->file->getName()));
#endif
			fileMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
#ifdef OLD_MENU_HEADER //[~]JhaoDa
			fileMenu.RemoveFirstItem();
#endif
			cleanUcMenu(fileMenu);
		}
		else   // if(ctrlList.GetSelectedCount() == 1 && ii->type == ItemInfo::FILE) {
		{
			int iCount = ctrlList.GetSelectedCount();
			fileMenu.EnableMenuItem(IDC_SEARCH_ALTERNATES, MF_BYCOMMAND | MFS_DISABLED);
			fileMenu.EnableMenuItem(IDC_SEARCH_FILE_IN_GOOGLE, MF_BYCOMMAND | MFS_DISABLED);
			fileMenu.EnableMenuItem(IDC_SEARCH_FILE_IN_YANDEX, MF_BYCOMMAND | MFS_DISABLED);
			activatePreviewItems(fileMenu);
			//fileMenu.EnableMenuItem((UINT)(HMENU)copyMenu, MF_BYCOMMAND | MFS_DISABLED); // !SMT!-UI
			fileMenu.EnableMenuItem(IDC_GENERATE_DCLST_FILE, MF_BYCOMMAND | (iCount == 1 ? MFS_ENABLED : MFS_DISABLED));    // [+] SSA
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			fileMenu.EnableMenuItem(IDC_VIEW_FLYSERVER_INFORM, MF_BYCOMMAND | MFS_DISABLED);
#endif
			//Append Favorite download dirs
			appendTargetMenu(targetMenu, IDC_DOWNLOAD_FAVORITE_DIRS);
			int n = 0;
			targetMenu.AppendMenu(MF_STRING, IDC_DOWNLOADTO, CTSTRING(BROWSE));
			LastDir::appendItem(targetMenu, n);
			if (ii->type == ItemInfo::DIRECTORY &&
			        ii->dir->getAdls() && ii->dir->getParent() != dl->getRoot())
			{
				fileMenu.AppendMenu(MF_STRING, IDC_GO_TO_DIRECTORY, CTSTRING(GO_TO_DIRECTORY));
			}
			// [+] SCALOlaz: swap Item text https://code.google.com/p/flylinkdc/issues/detail?id=887
			if (l_ipos != -1)
			{
				copyMenu.ModifyMenu(l_ipos, MF_BYPOSITION | MF_STRING, IDC_COPY_FILENAME, CTSTRING(FOLDERNAME));
			}
			
			appendUcMenu(fileMenu, UserCommand::CONTEXT_FILELIST, ClientManager::getHubs(dl->getUser()->getCID(), dl->getHintedUser().hint));
#ifdef OLD_MENU_HEADER //[~]JhaoDa
			fileMenu.InsertSeparatorFirst(ctrlList.GetSelectedCount() == 1 ? (ii->type == ItemInfo::FILE ? Text::CropStrLength(ii->file->getName()) : Text::CropStrLength(ii->dir->getName())) : CTSTRING(FILES));
#endif
			fileMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
#ifdef OLD_MENU_HEADER //[~]JhaoDa          
			fileMenu.RemoveFirstItem();
#endif
			cleanUcMenu(fileMenu);
		}
		WinUtil::unlinkStaticMenus(fileMenu); // TODO - fix copy-paste
		
		return TRUE;
	}
	else if (reinterpret_cast<HWND>(wParam) == ctrlTree && ctrlTree.GetSelectedItem() != NULL)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlTree, pt);
		}
		else
		{
			ctrlTree.ScreenToClient(&pt);
			UINT a = 0;
			HTREEITEM ht = ctrlTree.HitTest(pt, &a);
			if (ht != NULL && ht != ctrlTree.GetSelectedItem())
				ctrlTree.SelectItem(ht);
			ctrlTree.ClientToScreen(&pt);
		}
		
		// Strange, windows doesn't change the selection on right-click... (!)
		
		while (targetDirMenu.GetMenuItemCount() > 0)
		{
			targetDirMenu.DeleteMenu(0, MF_BYPOSITION);
		}
		
		appendTargetMenu(targetDirMenu, IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS);
		int n = 0;
		targetDirMenu.AppendMenu(MF_STRING, IDC_DOWNLOADDIRTO, CTSTRING(BROWSE));
		if (!LastDir::get().empty())
		{
			targetDirMenu.AppendMenu(MF_SEPARATOR);
			for (auto i = LastDir::get().cbegin(); i != LastDir::get().cend(); ++i)
			{
				targetDirMenu.AppendMenu(MF_STRING, IDC_DOWNLOAD_TARGET_DIR + (++n), Text::toLabel(*i).c_str());
			}
		}
		
#ifdef OLD_MENU_HEADER //[~]JhaoDa
		directoryMenu.InsertSeparatorFirst(TSTRING(FOLDER));
#endif
		directoryMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
#ifdef OLD_MENU_HEADER //[~]JhaoDa
		directoryMenu.RemoveFirstItem();
#endif
		
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT DirectoryListingFrame::onXButtonUp(UINT /*uMsg*/, WPARAM wParam, LPARAM /* lParam */, BOOL& /* bHandled */)
{
	if (GET_XBUTTON_WPARAM(wParam) & XBUTTON1)
	{
		back();
		return TRUE;
	}
	else if (GET_XBUTTON_WPARAM(wParam) & XBUTTON2)
	{
		forward();
		return TRUE;
	}
	
	return FALSE;
}

LRESULT DirectoryListingFrame::onDownloadTarget(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_TARGET - 1;
	dcassert(newId >= 0);
	
	if (ctrlList.GetSelectedCount() == 1)
	{
		ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		
		if (ii->type == ItemInfo::FILE)
		{
			if (newId < (int)targets.size())
			{
				try
				{
					dl->download(ii->file, targets[newId], false, WinUtil::isShift(), QueueItem::DEFAULT);
				}
				catch (const Exception& e)
				{
					ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
				}
			}
			else
			{
				newId -= (int)targets.size();
				dcassert(newId < (int)LastDir::get().size());
				downloadList(LastDir::get()[newId]);
			}
		}
		else
		{
			dcassert(newId < (int)LastDir::get().size());
			downloadList(LastDir::get()[newId]);
		}
	}
	else if (ctrlList.GetSelectedCount() > 1)
	{
		dcassert(newId < (int)LastDir::get().size());
		downloadList(LastDir::get()[newId]);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadTargetDir(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_TARGET_DIR - 1;
	dcassert(newId >= 0);
	
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t != NULL)
	{
		DirectoryListing::Directory* dir = (DirectoryListing::Directory*)ctrlTree.GetItemData(t);
		//string target = SETTING(DOWNLOAD_DIRECTORY);
		try
		{
			dcassert(newId < (int)LastDir::get().size());
			dl->download(dir, Text::fromT(LastDir::get()[newId]), WinUtil::isShift(), QueueItem::DEFAULT);
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
	return 0;
}
LRESULT DirectoryListingFrame::onDownloadFavoriteDirs(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_FAVORITE_DIRS;
	dcassert(newId >= 0);
	FavoriteManager::LockInstanceDirs lockedInstance;
	const auto& spl = lockedInstance.getFavoriteDirsL();
	if (ctrlList.GetSelectedCount() == 1)
	{
		ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
		
		if (ii->type == ItemInfo::FILE)
		{
			if (newId < (int)targets.size())
			{
				try
				{
					dl->download(ii->file, targets[newId], false, WinUtil::isShift(), QueueItem::DEFAULT);
				}
				catch (const Exception& e)
				{
					ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
				}
			}
			else
			{
				newId -= (int)targets.size();
				downloadList(spl, newId);
			}
		}
		else
		{
			dcassert(newId < (int)spl.size());
			downloadList(spl, newId);
		}
	}
	else if (ctrlList.GetSelectedCount() > 1)
	{
		downloadList(spl, newId);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onDownloadWholeFavoriteDirs(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int newId = wID - IDC_DOWNLOAD_WHOLE_FAVORITE_DIRS;
	dcassert(newId >= 0);
	
	HTREEITEM t = ctrlTree.GetSelectedItem();
	if (t != NULL)
	{
		DirectoryListing::Directory* dir = (DirectoryListing::Directory*)ctrlTree.GetItemData(t);
		//string target = SETTING(DOWNLOAD_DIRECTORY);
		try
		{
			FavoriteManager::LockInstanceDirs lockedInstance;
			const auto& spl = lockedInstance.getFavoriteDirsL();
			dcassert(newId < (int)spl.size());
			dl->download(dir, spl[newId].dir, WinUtil::isShift(), QueueItem::DEFAULT);
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
	return 0;
}

LRESULT DirectoryListingFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*) pnmh;
	if (kd->wVKey == VK_BACK)
	{
		up();
	}
	else if (kd->wVKey == VK_TAB)
	{
		onTab();
	}
	else if (kd->wVKey == VK_LEFT && WinUtil::isAlt())
	{
		back();
	}
	else if (kd->wVKey == VK_RIGHT && WinUtil::isAlt())
	{
		forward();
	}
	else if (kd->wVKey == VK_RETURN)
	{
		if (ctrlList.GetSelectedCount() == 1)
		{
			ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(ctrlList.GetNextItem(-1, LVNI_SELECTED));
			if (ii->type == ItemInfo::DIRECTORY)
			{
				HTREEITEM ht = ctrlTree.GetChildItem(ctrlTree.GetSelectedItem());
				while (ht != NULL)
				{
					if ((DirectoryListing::Directory*)ctrlTree.GetItemData(ht) == ii->dir)
					{
						ctrlTree.SelectItem(ht);
						break;
					}
					ht = ctrlTree.GetNextSiblingItem(ht);
				}
			}
			else
			{
				downloadList(Text::toT(SETTING(DOWNLOAD_DIRECTORY)));
			}
		}
		else
		{
			downloadList(Text::toT(SETTING(DOWNLOAD_DIRECTORY)));
		}
	}
	return 0;
}

void DirectoryListingFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	RECT rect;
	GetClientRect(&rect);
	// position bars and offset their dimensions
	UpdateBarsPosition(rect, bResizeBars);
	
	if (ctrlStatus.IsWindow())
	{
		CRect sr;
		int w[STATUS_LAST] = {0};
		ctrlStatus.GetClientRect(sr);
		w[STATUS_DUMMY - 1] = sr.right - 16;
		for (int i = STATUS_DUMMY - 2; i >= 0; --i)
		{
			w[i] = max(w[i + 1] - statusSizes[i + 1], 0);
		}
		
		ctrlStatus.SetParts(STATUS_LAST, w);
		ctrlStatus.GetRect(0, sr);
		
		sr.left = w[STATUS_FILE_LIST_DIFF - 1];
		sr.right = w[STATUS_FILE_LIST_DIFF];
		ctrlListDiff.MoveWindow(sr);
		
		sr.left = w[STATUS_MATCH_QUEUE - 1];
		sr.right = w[STATUS_MATCH_QUEUE];
		ctrlMatchQueue.MoveWindow(sr);
		
		sr.left = w[STATUS_FIND - 1];
		sr.right = w[STATUS_FIND];
		ctrlFind.MoveWindow(sr);
		
		sr.left = w[STATUS_NEXT - 1];
		sr.right = w[STATUS_NEXT];
		ctrlFindNext.MoveWindow(sr);
	}
	
	SetSplitterRect(&rect);
}

HTREEITEM DirectoryListingFrame::findFile(const StringSearch& str, HTREEITEM root,
                                          int &foundFile, int &skipHits)
{
	// Check dir name for match
	DirectoryListing::Directory* dir = (DirectoryListing::Directory*)ctrlTree.GetItemData(root);
	if (str.match(dir->getName()))
	{
		if (skipHits == 0)
		{
			foundFile = -1;
			return root;
		}
		else
			skipHits--;
	}
	
	// Force list pane to contain files of current dir
	changeDir(dir);
	
	// Check file names in list pane
	for (int i = 0; i < ctrlList.GetItemCount(); i++)
	{
		const ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(i);
		if (ii->type == ItemInfo::FILE)
		{
			if (str.match(ii->file->getName()))
			{
				if (skipHits == 0)
				{
					foundFile = i;
					return root;
				}
				else
					skipHits--;
			}
		}
	}
	
	dcdebug("looking for directories...\n");
	// Check subdirs recursively
	HTREEITEM item = ctrlTree.GetChildItem(root);
	while (item != NULL)
	{
		HTREEITEM srch = findFile(str, item, foundFile, skipHits);
		if (srch)
			return srch;
		else
			item = ctrlTree.GetNextSiblingItem(item);
	}
	
	return 0;
}

void DirectoryListingFrame::findFile(bool findNext)
{
	if (!findNext)
	{
		// Prompt for substring to find
		LineDlg dlg;
		dlg.title = TSTRING(SEARCH_FOR_FILE);
		dlg.description = TSTRING(ENTER_SEARCH_STRING);
		
		if (dlg.DoModal() != IDOK)
			return;
			
		findStr = Text::fromT(dlg.line);
		m_skipHits = 0;
	}
	else
	{
		m_skipHits++;
	}
	
	if (findStr.empty())
		return;
		
	// Do a search
	int foundFile = -1, skipHitsTmp = m_skipHits;
	HTREEITEM const oldDir = ctrlTree.GetSelectedItem();
	HTREEITEM const foundDir = findFile(StringSearch(findStr), ctrlTree.GetRootItem(), foundFile, skipHitsTmp);
	ctrlTree.SetRedraw(TRUE);
	
	if (foundDir)
	{
		// Highlight the directory tree and list if the parent dir/a matched dir was found
		if (foundFile >= 0)
		{
			// SelectItem won't update the list if SetRedraw was set to FALSE and then
			// to TRUE and the item selected is the same as the last one... workaround:
			if (oldDir == foundDir)
				ctrlTree.SelectItem(NULL);
				
			ctrlTree.SelectItem(foundDir);
		}
		else
		{
			// Got a dir; select its parent directory in the tree if there is one
			HTREEITEM parentItem = ctrlTree.GetParentItem(foundDir);
			if (parentItem)
			{
				// Go to parent file list
				ctrlTree.SelectItem(parentItem);
				
				// Locate the dir in the file list
				DirectoryListing::Directory* dir = (DirectoryListing::Directory*)ctrlTree.GetItemData(foundDir);
				
				foundFile = ctrlList.findItem(Text::toT(dir->getName()), -1, false);
			}
			else
			{
				// If no parent exists, just the dir tree item and skip the list highlighting
				ctrlTree.SelectItem(foundDir);
			}
		}
		
		// Remove prev. selection from file list
		if (ctrlList.GetSelectedCount() > 0)
		{
			const int l_cnt = ctrlList.GetItemCount();
			for (int i = 0; i < l_cnt; i++)
				ctrlList.SetItemState(i, 0, LVIS_SELECTED);
		}
		
		// Highlight and focus the dir/file if possible
		if (foundFile >= 0)
		{
			ctrlList.SetFocus();
			ctrlList.EnsureVisible(foundFile, FALSE);
			ctrlList.SetItemState(foundFile, LVIS_SELECTED | LVIS_FOCUSED, (UINT) - 1);
		}
		else
		{
			ctrlTree.SetFocus();
		}
	}
	else
	{
		ctrlTree.SelectItem(oldDir);
		MessageBox(CTSTRING(NO_MATCHES), CTSTRING(SEARCH_FOR_FILE));
	}
}

void DirectoryListingFrame::runUserCommand(UserCommand& uc)
{
	if (!WinUtil::getUCParams(m_hWnd, uc, ucLineParams))
		return;
		
	StringMap ucParams = ucLineParams;
	
	std::unordered_set<UserPtr, User::Hash> l_nicks;
	
	int sel = -1;
	while ((sel = ctrlList.GetNextItem(sel, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = (ItemInfo*)ctrlList.getItemData(sel);
		if (uc.getType() == UserCommand::TYPE_RAW_ONCE)
		{
			if (l_nicks.find(dl->getUser()) != l_nicks.end())
				continue;
			l_nicks.insert(dl->getUser());
		}
		if (!dl->getUser()->isOnline())
			return;
		ucParams["fileTR"] = "NONE";
		if (ii->type == ItemInfo::FILE)
		{
			ucParams["type"] = "File";
			ucParams["fileFN"] = dl->getPath(ii->file) + ii->file->getName();
			ucParams["fileSI"] = Util::toString(ii->file->getSize());
			ucParams["fileSIshort"] = Util::formatBytes(ii->file->getSize());
			ucParams["fileTR"] = ii->file->getTTH().toBase32();
		}
		else
		{
			ucParams["type"] = "Directory";
			ucParams["fileFN"] = dl->getPath(ii->dir) + ii->dir->getName();
			ucParams["fileSI"] = Util::toString(ii->dir->getTotalSize());
			ucParams["fileSIshort"] = Util::formatBytes(ii->dir->getTotalSize());
		}
		
		// compatibility with 0.674 and earlier
		ucParams["file"] = ucParams["fileFN"];
		ucParams["filesize"] = ucParams["fileSI"];
		ucParams["filesizeshort"] = ucParams["fileSIshort"];
		ucParams["tth"] = ucParams["fileTR"];
		
		StringMap tmp = ucParams;
		const UserPtr tmpPtr = dl->getUser();
		ClientManager::getInstance()->userCommand(dl->getHintedUser(), uc, tmp, true);
	}
}

void DirectoryListingFrame::closeAll()
{
	dcdrun(const auto l_size_g_frames = g_frames.size());
	for (auto i = g_frames.cbegin(); i != g_frames.cend(); ++i)
	{
		i->second->PostMessage(WM_CLOSE, 0, 0);
	}
	dcassert(l_size_g_frames == g_frames.size());
}

LRESULT DirectoryListingFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	string data;
	// !SMT!-UI: copy several rows
	int i = -1;
	while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		const ItemInfo* ii = (ItemInfo*)ctrlList.getItemData(i);
		string sCopy;
		switch (wID)
		{
			case IDC_COPY_NICK:
				sCopy = dl->getUser()->getLastNick();
				break;
			case IDC_COPY_FILENAME:
				sCopy = Util::getFileName(ii->type == ItemInfo::FILE ? ii->file->getName() : ii->dir->getName()); // !SMT!-F
				break;
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
			case IDC_COPY_FLYSERVER_INFORM:
				if (ii->type == ItemInfo::FILE)
					sCopy = CFlyServerInfo::getMediaInfoAsText(ii->file->getTTH(), ii->file->getSize());
				break;
#endif
			case IDC_COPY_SIZE:
				sCopy = Util::formatBytes(ii->type == ItemInfo::FILE ? ii->file->getSize() : ii->dir->getTotalSize()); // !SMT!-F
				break;
			case IDC_COPY_LINK:
				if (ii->type == ItemInfo::FILE)
					sCopy = Util::getMagnet(ii->file->getTTH(), ii->file->getName(), ii->file->getSize());
				break;
			case IDC_COPY_TTH:
				if (ii->type == ItemInfo::FILE)
					sCopy = ii->file->getTTH().toBase32();
				break;
			case IDC_COPY_WMLINK://[+]necros
				if (ii->type == ItemInfo::FILE)
					sCopy = Util::getWebMagnet(ii->file->getTTH(), ii->file->getName(), ii->file->getSize());
				break;
			default:
				dcdebug("DIRECTORYLISTINGFRAME DON'T GO HERE\n");
				return 0;
		}
		if (!sCopy.empty())
		{
			if (data.empty())
				data = sCopy;
			else
				data = data + "\r\n" + sCopy;
		}
	}
	if (!data.empty())
	{
		WinUtil::setClipboard(data);
	}
	return 0;
}

LRESULT DirectoryListingFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	m_before_close = true;
	CWaitCursor l_cursor_wait; //-V808
	if (m_loading)
	{
		//tell the thread to abort and wait until we get a notification
		//that it's done.
		dl->setAbort(true);
		return 0;
	}
	if (!m_closed)
	{
		m_closed = true;
		safe_destroy_timer();
		SettingsManager::getInstance()->removeListener(this);
		g_frames.erase(m_hWnd);
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		waitForFlyServerStop();
#endif
		ctrlList.DeleteAndCleanAllItems();
		ctrlList.saveHeaderOrder(SettingsManager::DIRECTORYLISTINGFRAME_ORDER, SettingsManager::DIRECTORYLISTINGFRAME_WIDTHS, SettingsManager::DIRECTORYLISTINGFRAME_VISIBLE); // !SMT!-UI
		SET_SETTING(DIRLIST_COLUMNS_SORT, ctrlList.getSortColumn());
		SET_SETTING(DIRLIST_COLUMNS_SORT_ASC, ctrlList.isAscending());
		SET_SETTING(DIRECTORYLISTINGFRAME_SPLIT, m_nProportionalPos);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		bHandled = FALSE;
		return 0;
	}
}

LRESULT DirectoryListingFrame::onTabContextMenu(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	const POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };        // location of mouse click
	
	if (const UserPtr& user = dl->getUser())
	{
		OMenu tabMenu;
		tabMenu.CreatePopupMenu();
		clearUserMenu();
		
//#ifdef OLD_MENU_HEADER //[~]JhaoDa
		tabMenu.InsertSeparatorFirst(user->getLastNickT());
//#endif
		reinitUserMenu(user, Util::emptyString); // [!] TODO: add hub hint.
		appendAndActivateUserItems(tabMenu); // [+] IRainman https://code.google.com/p/flylinkdc/issues/detail?id=777
		tabMenu.AppendMenu(MF_SEPARATOR);
		
		tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_ALL_DIR_LIST, CTSTRING(MENU_CLOSE_ALL_DIR_LIST)); // [+] InfinitySky.
		tabMenu.AppendMenu(MF_STRING, IDC_CLOSE_WINDOW, CTSTRING(CLOSE_HOT));
		
		tabMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		WinUtil::unlinkStaticMenus(tabMenu); // TODO - fix copy-paste
	}
	return TRUE;
}

void DirectoryListingFrame::on(SettingsManagerListener::Save, SimpleXML& /*xml*/) noexcept
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

LRESULT DirectoryListingFrame::onSpeaker(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
#ifdef USE_OFFLINE_ICON_FOR_FILELIST
	updateTitle(); // [+] InfinitySky. Изменять заголовок окна (иконку).
#endif // USE_OFFLINE_ICON_FOR_FILELIST
	switch (wParam)
	{
		case FINISHED:
			m_loading = false;
			initStatus();
			ctrlStatus.SetFont(Fonts::g_systemFont);
			ctrlStatus.SetText(0, (TSTRING(PROCESSED_FILE_LIST) + _T(' ') + Util::toStringW((GET_TICK() - m_FL_LoadSec) / 1000) + _T(' ') + TSTRING(S)).c_str());
			ctrlTree.EnableWindow(TRUE);
			
			//notify the user that we've loaded the list
			setDirty();
			break;
		case ABORTED:
			m_loading = false;
			PostMessage(WM_CLOSE, 0, 0);
			break;
		default:
			dcassert(0);
			break;
	}
	return 0;
}

// !SMT!-UI
void DirectoryListingFrame::getItemColor(const Flags::MaskType flags, COLORREF &fg, COLORREF &bg)
{
	//[~]NSL
	if (flags & DirectoryListing::FLAG_SHARED)
		bg = SETTING(DUPE_COLOR);
	if (flags & DirectoryListing::FLAG_DOWNLOAD)
		bg = SETTING(DUPE_EX1_COLOR);
	if (flags & DirectoryListing::FLAG_DOWNLOAD_FOLDER)
	{
		DWORD l_color = SETTING(DUPE_EX1_COLOR);
		l_color = RGB(GetRValue(l_color) << 1, GetGValue(l_color) << 1, GetBValue(l_color) << 1);
		bg = l_color;
	}
	else if (flags & DirectoryListing::FLAG_OLD_TTH)
		bg =  SETTING(DUPE_EX2_COLOR) ;
		
}

// !fulDC!
LRESULT DirectoryListingFrame::onCustomDrawList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW plvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (plvcd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT:
		{
			Flags::MaskType flags = 0;
			ItemInfo *ii = reinterpret_cast<ItemInfo*>(plvcd->nmcd.lItemlParam);
			ii->calcImageIndex();
			if (ii->type == ItemInfo::FILE)
				flags = ii->file->getFlags();
			else if (ii->type == ItemInfo::DIRECTORY)
				flags = ii->dir->getFlags();
			getItemColor(flags, plvcd->clrText, plvcd->clrTextBk);
			if (!ii->columns[COLUMN_FLY_SERVER_RATING].empty())
				plvcd->clrTextBk  = OperaColors::brightenColor(plvcd->clrTextBk, -0.02f);
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
			Colors::alternationBkColor(plvcd); // [+] IRainman
#endif
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
			return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
#endif // SCALOLAZ_MEDIAVIDEO_ICO
		}
#ifdef SCALOLAZ_MEDIAVIDEO_ICO
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			ItemInfo *ii = reinterpret_cast<ItemInfo*>(plvcd->nmcd.lItemlParam);
			if (ctrlList.drawHDIcon(plvcd, ii->columns[COLUMN_MEDIA_XY], COLUMN_MEDIA_XY))
			{
				return CDRF_SKIPDEFAULT;
			}
		}
#endif  //SCALOLAZ_MEDIAVIDEO_ICO
	}
	return CDRF_DODEFAULT;
}
// !fulDC!
LRESULT DirectoryListingFrame::onCustomDrawTree(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	LPNMLVCUSTOMDRAW plvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pnmh);
	
	switch (plvcd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
			
		case CDDS_ITEMPREPAINT:
			if ((plvcd->nmcd.uItemState & CDIS_SELECTED) == 0)
			{
				DirectoryListing::Directory* dir = reinterpret_cast<DirectoryListing::Directory*>(plvcd->nmcd.lItemlParam);
				getItemColor(dir->getFlags(), plvcd->clrText, plvcd->clrTextBk);
			}
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
			else
				Colors::alternationBkColor(plvcd); // [+] IRainman
#endif
	}
	return CDRF_DODEFAULT;
}

DirectoryListingFrame::ItemInfo::ItemInfo(DirectoryListing::File* f) : type(FILE), file(f), m_icon_index(-1)
{
	columns[COLUMN_FILENAME] = Text::toT(f->getName()); // https://www.box.net/shared/972al1nj5hngajrcgvnt
	columns[COLUMN_TYPE] = Util::getFileExt(columns[COLUMN_FILENAME]);
	if (!columns[COLUMN_TYPE].empty() && columns[COLUMN_TYPE][0] == '.')
		columns[COLUMN_TYPE].erase(0, 1);
	columns[COLUMN_EXACTSIZE] = Util::formatExactSize(f->getSize());
	columns[COLUMN_SIZE] =  Util::formatBytesW(f->getSize());
	columns[COLUMN_TTH] = Text::toT(f->getTTH().toBase32());
	if (f->isSet(DirectoryListing::FLAG_SHARED_OWN)) // TODO убить FLAG_SHARED_OWN
		columns[COLUMN_PATH] = Text::toT(Util::getFilePath(ShareManager::getInstance()->toRealPath(f->getTTH())));
	else if (f->isSet(DirectoryListing::FLAG_SHARED) && f->getSize()) // [+] FlylinkDC++
		columns[COLUMN_PATH] = Text::toT(ShareManager::getInstance()->toRealPath(f->getTTH())); // !PPA!
		
	UpdatePathColumn(f);
	
	if (f->getHit())
		columns[COLUMN_HIT] = Util::toStringW(f->getHit());
	if (f->getTS())
		columns[COLUMN_TS] = Text::toT(Util::formatDigitalClock(f->getTS()));
	if (f->m_media)
	{
		if (f->m_media->m_bitrate)
			columns[COLUMN_BITRATE] = Util::toStringW(f->m_media->m_bitrate);
		columns[COLUMN_MEDIA_XY] = Text::toT(f->m_media->getXY());
		columns[COLUMN_MEDIA_VIDEO] = Text::toT(f->m_media->m_video);
		CFlyMediaInfo::translateDuration(f->m_media->m_audio, columns[COLUMN_MEDIA_AUDIO], columns[COLUMN_DURATION]);
	}
}
DirectoryListingFrame::ItemInfo::ItemInfo(DirectoryListing::Directory* d) : type(DIRECTORY), dir(d), m_icon_index(-1)
{
	columns[COLUMN_FILENAME]  = Text::toT(d->getName());
	columns[COLUMN_EXACTSIZE] = Util::formatExactSize(d->getTotalSize());
	columns[COLUMN_SIZE]      = Util::formatBytesW(d->getTotalSize());
	if (const int64_t l_Hit   = d->getTotalHit())
		columns[COLUMN_HIT]   = Util::toStringW(l_Hit);
	if (const int64_t l_ts   = d->getTotalTS())
		columns[COLUMN_TS]    = Text::toT(Util::formatDigitalClock(l_ts)); //-V106
	columns[COLUMN_BITRATE]   = d->getMinMaxBitrateDirAsString();
}

void DirectoryListingFrame::ItemInfo::UpdatePathColumn(const DirectoryListing::File* f)
{
	if (columns[COLUMN_PATH].empty())
	{
		// TODO Копипаста
		if (f->isSet(DirectoryListing::FLAG_DOWNLOAD))
			columns[COLUMN_PATH] = TSTRING(I_DOWNLOADED_THIS_FILE); //[!]NightOrion(translate)
		if (f->isSet(DirectoryListing::FLAG_OLD_TTH))
		{
			if (!columns[COLUMN_PATH].empty())
				columns[COLUMN_PATH] += _T(" + ");
			columns[COLUMN_PATH] += TSTRING(THIS_FILE_WAS_IN_MY_SHARE); //[!]NightOrion(translate)
		}
	}
}

LRESULT DirectoryListingFrame::onGenerateDCLST(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	DirectoryListing::Directory* dir = nullptr;
	
	if (wID == IDC_GENERATE_DCLST_FILE)  // Call from files panel
	{
		int i = -1;
		while ((i = ctrlList.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			ItemInfo* ii = (ItemInfo*)ctrlList.GetItemData(i);
			if (ii->type == ItemInfo::DIRECTORY)
			{
				dir = ii->dir;
				break;
			}
		}
	}
	else
	{
		// Получить выбранный каталог в XML
		HTREEITEM t = ctrlTree.GetSelectedItem();
		if (t != NULL)
		{
			dir = (DirectoryListing::Directory*)ctrlTree.GetItemData(t);
		}
	}
	if (dir != NULL)
	{
		try
		{
			if (dl->getUser())
			{
				DCLSTGenDlg dlg(dir, dl->getUser());
				dlg.DoModal(GetParent());
			}
		}
		catch (const Exception& e)
		{
			ctrlStatus.SetText(STATUS_TEXT, Text::toT(e.getError()).c_str());
		}
	}
	
	return 0;
}

// [+] SSA open file with dclst support
void DirectoryListingFrame::openFileFromList(const tstring& file)
{
	if (file.empty())
		return;
		
	if (Util::isDclstFile(file))
		DirectoryListingFrame::openWindow(file, Util::emptyStringT, HintedUser(UserPtr(), Util::emptyString), 0, true); // [!] IRainman fix: open all dclst file with empty user.
	else
		WinUtil::openFile(file);
		
}

LRESULT DirectoryListingFrame::onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	ItemInfo* li = ctrlList.getSelectedItem();
	if (li != NULL && li->type == ItemInfo::FILE)
	{
		startMediaPreview(wID, li->file->getTTH()
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		                  , li->file->getSize()
#endif
		                 );
	}
	return 0;
}

//===================================================================================================================================
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
LRESULT DirectoryListingFrame::onMergeFlyServerResult(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& bHandled)
{
	std::vector<int> l_update_index;
	{
		std::unique_ptr<Json::Value> l_root(reinterpret_cast<Json::Value*>(wParam));
		const Json::Value& l_arrays = (*l_root)["array"];
		const Json::Value::ArrayIndex l_count = l_arrays.size();
		Lock l(g_cs_fly_server);
		for (Json::Value::ArrayIndex i = 0; i < l_count; ++i)
		{
			const Json::Value& l_cur_item_in = l_arrays[i];
			const TTHValue l_tth = TTHValue(l_cur_item_in["tth"].asString());
			bool l_is_know_tth = false;
			const auto l_si_find = m_merge_item_map.find(l_tth);
			if (l_si_find != m_merge_item_map.end())
			{
				int l_cur_item = -1;
				dcassert(!isClosedOrShutdown());
				if (!isClosedOrShutdown())
					l_cur_item = ctrlList.findItem(l_si_find->second);
				else
					return 0;
				if (l_cur_item >= 0)
				{
					const Json::Value& l_result_counter = l_cur_item_in["info"];
					const Json::Value& l_result_base_media = l_cur_item_in["media"];
					const int l_count_media = Util::toInt(l_result_counter["count_media"].asString());
					//dcassert(l_count_media == 0 && l_result_counter["count_media"].asString() == "0")
					if (l_count_media > 0) // Медиаинфа на сервере уже лежит? - не пытаемся ее послать снова
					{
						l_is_know_tth |= true;  // TODO сделать проверку на полноту медиаинфы. т.к. на сервере может лежать частичные данные.
					}
					if (l_si_find->second->columns[COLUMN_BITRATE].empty())
						l_si_find->second->columns[COLUMN_BITRATE]  = Text::toT(l_result_base_media["fly_audio_br"].asString());
					if (l_si_find->second->columns[COLUMN_MEDIA_XY].empty())
						l_si_find->second->columns[COLUMN_MEDIA_XY] =  Text::toT(l_result_base_media["fly_xy"].asString());
					if (l_si_find->second->columns[COLUMN_MEDIA_VIDEO].empty())
						l_si_find->second->columns[COLUMN_MEDIA_VIDEO] =  Text::toT(l_result_base_media["fly_video"].asString());
					if (l_si_find->second->columns[COLUMN_MEDIA_AUDIO].empty())
					{
						const string l_fly_audio = l_result_base_media["fly_audio"].asString();
						CFlyMediaInfo::translateDuration(l_fly_audio, l_si_find->second->columns[COLUMN_MEDIA_AUDIO], l_si_find->second->columns[COLUMN_DURATION]);
					}
					const string l_count_query = l_result_counter["count_query"].asString();
					const string l_count_download = l_result_counter["count_download"].asString();
					if (!l_count_query.empty() || !l_count_download.empty())
					{
						l_update_index.push_back(l_cur_item);
						l_si_find->second->columns[COLUMN_FLY_SERVER_RATING] =  Text::toT(l_count_query);
						if (!l_count_download.empty())
						{
							if (l_si_find->second->columns[COLUMN_FLY_SERVER_RATING].empty())
								l_si_find->second->columns[COLUMN_FLY_SERVER_RATING] = Text::toT("0/" + l_count_download); // TODO fix copy-paste
							else
								l_si_find->second->columns[COLUMN_FLY_SERVER_RATING] = Text::toT(l_count_query + '/' + l_count_download);
						}
						if (l_count_query == "1")
							l_is_know_tth = false; // Файл на сервер первый раз появился.
					}
					CFlyServerCache l_cache;
					l_cache.m_ratio = Text::fromT(l_si_find->second->columns[COLUMN_FLY_SERVER_RATING]);
					l_cache.m_audio = l_result_base_media["fly_audio"].asString();
					l_cache.m_audio_br = l_result_base_media["fly_audio_br"].asString();
					l_cache.m_video = l_result_base_media["fly_video"].asString();
					l_cache.m_xy = l_result_base_media["fly_xy"].asString();
					g_fly_server_cache[l_tth] = std::make_pair(l_si_find->second, l_cache); // Сохраняем рейтинг и медиаинфу в кэше
				}
			}
			if (l_is_know_tth) // Если сервер расказал об этом TTH с медиаинфой, то не шлем ему ничего
			{
				m_tth_media_file_map.erase(l_tth);
			}
		}
	}
	prepare_mediainfo_to_fly_serverL(); // Соберем TTH, которые нужно отправить на флай-сервер в обмен на инфу.
	m_merge_item_map.clear();
	update_column_after_merge(l_update_index);
	return 0;
}
//===================================================================================================================================
void DirectoryListingFrame::update_column_after_merge(std::vector<int> p_update_index)
{
#if 0
	TODO - апдейты по колонкам не пашут иногда
http://code.google.com/p/flylinkdc/issues/detail?id=1113
	const static int l_array[] =
	{
		COLUMN_BITRATE , COLUMN_MEDIA_XY, COLUMN_MEDIA_VIDEO , COLUMN_MEDIA_AUDIO, COLUMN_DURATION, COLUMN_FLY_SERVER_RATING
	};
	const static std::vector<int> l_columns(l_array, l_array + _countof(l_array));
	dcassert(!isClosedOrShutdown());
	if (!isClosedOrShutdown())
	{
		ctrlList.update_columns(l_update_index, l_columns);
	}
	else
	{
		return;
	}
#else
	dcassert(!isClosedOrShutdown());
	if (!isClosedOrShutdown())
	{
		ctrlList.update_all_columns(p_update_index);
	}
#endif
}
//===================================================================================================================================
bool DirectoryListingFrame::scan_list_view_from_merge()
{
	if (BOOLSETTING(ENABLE_FLY_SERVER))
	{
		const int l_item_count = ctrlList.GetItemCount();
		if (l_item_count == 0)
			return 0;
		std::vector<int> l_update_index;
		const int l_top_index = ctrlList.GetTopIndex();
		const int l_count_per_page = ctrlList.GetCountPerPage();
		for (int j = l_top_index; j < l_item_count && j < l_top_index + l_count_per_page; ++j)
		{
			dcassert(!isClosedOrShutdown());
			ItemInfo* l_item_info = ctrlList.getItemData(j);
			if (l_item_info == nullptr || l_item_info->m_already_processed || l_item_info->type != ItemInfo::FILE) // Уже не первый раз или это не файл?
				continue;
			l_item_info->m_already_processed = true;
			const auto l_file_size = l_item_info->file->getSize();
			if (l_file_size)
			{
				const string l_file_ext = Text::toLower(Util::getFileExtWithoutDot(l_item_info->file->getName())); // TODO - расширение есть в Columns но в T-формате
				if (g_fly_server_config.isSupportFile(l_file_ext, l_file_size))
				{
					const TTHValue& l_tth = l_item_info->file->getTTH();
					Lock l(g_cs_fly_server);
					const auto l_find_ratio = g_fly_server_cache.find(l_tth);
					if (l_find_ratio == g_fly_server_cache.end()) // Если значение рейтинга есть в кэше то не запрашиваем о нем инфу с сервера
					{
						CFlyServerKey l_info(l_tth, l_file_size);
						m_merge_item_map.insert(make_pair(l_tth, l_item_info));
						l_info.m_only_counter  = !l_item_info->columns[COLUMN_MEDIA_AUDIO].empty(); // Колонка базовой медиаинфы уже заполенна - запросим с сервера только рейтинги
						if (l_info.m_only_counter) // TODO - определить точнее есть у нас инфа по файлу или нет?
						{
							m_tth_media_file_map[l_tth] = l_file_size; // Регистрируем кандидата на передачу информации
						}
						// TODO - обратиться к локальной базе вдруг у нас уже инфа есть?
						m_GetFlyServerArray.push_back(l_info);
					}
					else
					{
						l_update_index.push_back(j);
						const auto& l_cache = l_find_ratio->second.second;
						if (l_item_info->columns[COLUMN_FLY_SERVER_RATING].empty())
							l_item_info->columns[COLUMN_FLY_SERVER_RATING] = Text::toT(l_cache.m_ratio);
						if (l_item_info->columns[COLUMN_BITRATE].empty())
							l_item_info->columns[COLUMN_BITRATE]  = Text::toT(l_cache.m_audio_br);
						if (l_item_info->columns[COLUMN_MEDIA_XY].empty())
							l_item_info->columns[COLUMN_MEDIA_XY] =  Text::toT(l_cache.m_xy);
						if (l_item_info->columns[COLUMN_MEDIA_VIDEO].empty())
							l_item_info->columns[COLUMN_MEDIA_VIDEO] =  Text::toT(l_cache.m_video);
						if (l_item_info->columns[COLUMN_MEDIA_AUDIO].empty())
						{
							CFlyMediaInfo::translateDuration(l_cache.m_audio, l_item_info->columns[COLUMN_MEDIA_AUDIO], l_item_info->columns[COLUMN_DURATION]);
						}
					}
				}
			}
		}
		update_column_after_merge(l_update_index);
	}
	return m_GetFlyServerArray.size() || m_SetFlyServerArray.size();
}
//===================================================================================================================================
void DirectoryListingFrame::mergeFlyServerInfo()
{
	if (isClosedOrShutdown())
		return;
	CWaitCursor l_cursor_wait; //-V808
	dcassert(!isClosedOrShutdown());
	if (!isClosedOrShutdown())
	{
		post_message_for_update_mediainfo();
	}
}
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
//===================================================================================================================================
LRESULT DirectoryListingFrame::onTimer(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	if (!MainFrame::isAppMinimized() && WinUtil::g_tabCtrl->isActive(m_hWnd) && !isClosedOrShutdown()) // [+] IRainman opt.
	{
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
		if (scan_list_view_from_merge())
		{
			const auto l_tick = GET_TICK();
			addFlyServerTask(l_tick, false);
		}
#endif
		if (m_count_item_changed > 0)
		{
			updateStatus();
		}
	}
	return 0;
}
//===================================================================================================================================

/**
 * @file
 * $Id: DirectoryListingFrm.cpp,v 1.66 2006/09/23 19:24:39 bigmuscle Exp $
 */
