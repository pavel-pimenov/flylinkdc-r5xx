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

#include "QueueFrame.h"
#include "PrivateFrame.h"
#include "../client/ShareManager.h"
#include "../client/DownloadManager.h"
#include "BarShader.h"
#include "MainFrm.h"
#include "libtorrent/hex.hpp"

int QueueFrame::columnIndexes[] = { COLUMN_TARGET, COLUMN_TYPE, COLUMN_STATUS, COLUMN_SEGMENTS, COLUMN_SIZE, COLUMN_PROGRESS, COLUMN_DOWNLOADED, COLUMN_PRIORITY,
                                    COLUMN_USERS, COLUMN_PATH,
                                    COLUMN_LOCAL_PATH,
                                    COLUMN_EXACT_SIZE, COLUMN_ERRORS, COLUMN_ADDED, COLUMN_TTH, COLUMN_SPEED
                                  };

int QueueFrame::columnSizes[] = { 200, 20, 300, 70, 75, 100, 120, 75, 200, 200, 200, 75, 200, 100, 125, 50 };

static ResourceManager::Strings columnNames[] = { ResourceManager::FILENAME, ResourceManager::TYPE, ResourceManager::STATUS, ResourceManager::SEGMENTS, ResourceManager::SIZE,
                                                  ResourceManager::DOWNLOADED_PARTS, ResourceManager::DOWNLOADED,
                                                  ResourceManager::PRIORITY, ResourceManager::USERS, ResourceManager::PATH,
                                                  ResourceManager::LOCAL_PATH,
                                                  ResourceManager::EXACT_SIZE, ResourceManager::ERRORS,
                                                  ResourceManager::ADDED, ResourceManager::TTH_ROOT,
                                                  ResourceManager::SPEED
                                                };

QueueFrame::QueueFrame() : CFlyTimerAdapter(m_hWnd), CFlyTaskAdapter(m_hWnd), menuItems(0), m_queueSize(0), m_queueItems(0), m_dirty(false),
	usingDirMenu(false), readdItems(0), m_fileLists(nullptr), showTree(true)
	, showTreeContainer(WC_BUTTON, this, SHOWTREE_MESSAGE_MAP),
	m_last_count(0), m_last_total(0), m_update_status(0)
{
	memzero(statusSizes, sizeof(statusSizes));
}

QueueFrame::~QueueFrame()
{
	// Clear up dynamicly allocated menu objects
	browseMenu.ClearMenu();
	removeMenu.ClearMenu();
	removeAllMenu.ClearMenu();
	pmMenu.ClearMenu();
	readdMenu.ClearMenu();
}

LRESULT QueueFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	showTree = BOOLSETTING(QUEUEFRAME_SHOW_TREE);
	
	CreateSimpleStatusBar(ATL_IDS_IDLEMESSAGE, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | SBARS_SIZEGRIP);
	ctrlStatus.Attach(m_hWndStatusBar);
	
	ctrlQueue.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
	                 WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS, WS_EX_CLIENTEDGE, IDC_QUEUE);
	SET_EXTENDENT_LIST_VIEW_STYLE(ctrlQueue);
	
	ctrlDirs.Create(m_hWnd, rcDefault, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
	                TVS_HASBUTTONS | TVS_LINESATROOT | TVS_HASLINES | TVS_SHOWSELALWAYS | TVS_DISABLEDRAGDROP,
	                WS_EX_CLIENTEDGE, IDC_DIRECTORIES);
	                
	WinUtil::SetWindowThemeExplorer(ctrlDirs.m_hWnd);
	
	ctrlDirs.SetImageList(g_fileImage.getIconList(), TVSIL_NORMAL);
	ctrlQueue.SetImageList(g_fileImage.getIconList(), LVSIL_SMALL);
	
	m_nProportionalPos = SETTING(QUEUEFRAME_SPLIT);
	SetSplitterPanes(ctrlDirs.m_hWnd, ctrlQueue.m_hWnd);
	
	// Create listview columns
	WinUtil::splitTokens(columnIndexes, SETTING(QUEUEFRAME_ORDER), COLUMN_LAST);
	WinUtil::splitTokensWidth(columnSizes, SETTING(QUEUEFRAME_WIDTHS), COLUMN_LAST);
	
	BOOST_STATIC_ASSERT(_countof(columnSizes) == COLUMN_LAST);
	BOOST_STATIC_ASSERT(_countof(columnNames) == COLUMN_LAST);
	
	for (uint8_t j = 0; j < COLUMN_LAST; j++)
	{
		const int fmt = (j == COLUMN_SIZE || j == COLUMN_DOWNLOADED || j == COLUMN_EXACT_SIZE || j == COLUMN_SEGMENTS) ? LVCFMT_RIGHT : LVCFMT_LEFT;
		ctrlQueue.InsertColumn(j, TSTRING_I(columnNames[j]), fmt, columnSizes[j], j);
	}
	
	ctrlQueue.setColumnOrderArray(COLUMN_LAST, columnIndexes);
	ctrlQueue.setVisible(SETTING(QUEUEFRAME_VISIBLE));
	
	// ctrlQueue.setSortColumn(COLUMN_TARGET);
	ctrlQueue.setSortColumn(SETTING(QUEUE_COLUMNS_SORT));
	ctrlQueue.setAscending(BOOLSETTING(QUEUE_COLUMNS_SORT_ASC));
	
	SET_LIST_COLOR(ctrlQueue);
	ctrlQueue.setFlickerFree(Colors::g_bgBrush);
	
	ctrlDirs.SetBkColor(Colors::g_bgColor);
	ctrlDirs.SetTextColor(Colors::g_textColor);
	
	ctrlShowTree.Create(ctrlStatus.m_hWnd, rcDefault, _T("+/-"), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	ctrlShowTree.SetButtonStyle(BS_AUTOCHECKBOX, false);
	ctrlShowTree.SetCheck(showTree);
	ctrlShowTree.SetFont(Fonts::g_systemFont);
	showTreeContainer.SubclassWindow(ctrlShowTree.m_hWnd);
	
	browseMenu.CreatePopupMenu();
	removeMenu.CreatePopupMenu();
	removeAllMenu.CreatePopupMenu();
	pmMenu.CreatePopupMenu();
	readdMenu.CreatePopupMenu();
	
	removeMenu.AppendMenu(MF_STRING, IDC_REMOVE_SOURCE, CTSTRING(ALL));
	removeMenu.AppendMenu(MF_SEPARATOR);
	
	readdMenu.AppendMenu(MF_STRING, IDC_READD, CTSTRING(ALL));
	readdMenu.AppendMenu(MF_SEPARATOR);
	
	addQueueList();
	QueueManager::getInstance()->addListener(this);
	DownloadManager::getInstance()->addListener(this);
	SettingsManager::getInstance()->addListener(this);
	
	memzero(statusSizes, sizeof(statusSizes));
	statusSizes[0] = 16;
	ctrlStatus.SetParts(6, statusSizes);
	m_update_status++;
	create_timer(1000);
	bHandled = FALSE;
	return 1;
}


void QueueFrame::QueueItemInfo::removeTarget(bool p_is_batch_remove)
{
	QueueManager::getInstance()->removeTarget(getTarget(), p_is_batch_remove);
}

int QueueFrame::QueueItemInfo::compareItems(const QueueItemInfo* a, const QueueItemInfo* b, int col)
{
	switch (col)
	{
		case COLUMN_SIZE:
		case COLUMN_EXACT_SIZE:
			return compare(a->getSize(), b->getSize());
		case COLUMN_PRIORITY:
			return compare((int)a->getPriority(), (int)b->getPriority());
		case COLUMN_DOWNLOADED:
			return compare(a->getDownloadedBytes(), b->getDownloadedBytes());
		case COLUMN_ADDED:
			return compare(a->getAdded(), b->getAdded());
		default:
			return lstrcmpi(a->getText(col).c_str(), b->getText(col).c_str());
	}
}

const tstring QueueFrame::QueueItemInfo::getText(int col) const
{
	switch (col)
	{
		case COLUMN_TARGET:
			return Text::toT(Util::getFileName(getTarget()));
		case COLUMN_TYPE:
			return Text::toT(Util::getFileExtWithoutDot(getTarget()));
		case COLUMN_STATUS:
		{
			if (isFinished())
			{
				return TSTRING(DOWNLOAD_FINISHED_IDLE);
			}
			if (!isTorrent())
			{
				const size_t l_online = m_qi->getLastOnlineCount();
				const size_t l_count_source = m_qi->getSourcesCount();
				if (isWaiting())
				{
					if (l_online > 0)
					{
						if (l_count_source == 1)
						{
							return TSTRING(WAITING_USER_ONLINE);
						}
						else
						{
							tstring buf;
							buf.resize(64);
							buf.resize(_snwprintf(&buf[0], buf.size(), CTSTRING(WAITING_USERS_ONLINE), l_online, l_count_source));
							return buf;
						}
					}
					else
					{
						switch (l_count_source)
						{
							case 0:
								return TSTRING(NO_USERS_TO_DOWNLOAD_FROM);
							case 1:
								return TSTRING(USER_OFFLINE);
							case 2:
								return TSTRING(BOTH_USERS_OFFLINE);
							case 3:
								return TSTRING(ALL_3_USERS_OFFLINE);
							case 4:
								return TSTRING(ALL_4_USERS_OFFLINE);
							default:
								tstring buf;
								buf.resize(64);
								buf.resize(_snwprintf(&buf[0], buf.size(), CTSTRING(ALL_USERS_OFFLINE), l_count_source));
								return buf;
						};
					}
				}
				else
				{
					if (l_count_source == 1)
					{
						return TSTRING(USER_ONLINE);
					}
					else
					{
						tstring buf;
						buf.resize(64);
						buf.resize(_snwprintf(&buf[0], buf.size(), CTSTRING(USERS_ONLINE), l_online, l_count_source));
						return buf;
					}
				}
			}
		}
		case COLUMN_SEGMENTS:
		{
			if (!isTorrent())
			{
				const QueueItemPtr qi = getQueueItem();
				return Util::toStringW(qi->getDownloadsSegmentCount()) + _T('/') + Util::toStringW(qi->getMaxSegments());
			}
			else
			{
				return _T("TODO - Segmets");
			}
		}
		case COLUMN_SIZE:
			return getSize() == -1 ? TSTRING(UNKNOWN) : Util::formatBytesW(getSize());
		case COLUMN_DOWNLOADED:
		{
			return getSize() > 0 ? Util::formatBytesW(getDownloadedBytes()) + _T(" (") + Util::toStringW((double)getDownloadedBytes() * 100.0 / (double)getSize()) + _T("%)") : Util::emptyStringT;
		}
		case COLUMN_PRIORITY:
		{
			tstring priority;
			switch (getPriority())
			{
				case QueueItem::PAUSED:
					priority = TSTRING(PAUSED);
					break;
				case QueueItem::LOWEST:
					priority = TSTRING(LOWEST);
					break;
				case QueueItem::LOW:
					priority = TSTRING(LOW);
					break;
				case QueueItem::NORMAL:
					priority = TSTRING(NORMAL);
					break;
				case QueueItem::HIGH:
					priority = TSTRING(HIGH);
					break;
				case QueueItem::HIGHEST:
					priority = TSTRING(HIGHEST);
					break;
				default:
					dcassert(0);
					break;
			}
			if (getAutoPriority())
			{
				priority += _T(" (") + TSTRING(AUTO) + _T(')');
			}
			return priority;
		}
		case COLUMN_USERS:
		{
			tstring tmp;
			if (!isTorrent())
			{
				RLock(*QueueItem::g_cs);
				const auto& sources = m_qi->getSourcesL();
				for (auto j = sources.cbegin(); j != sources.cend(); ++j)
				{
					if (!tmp.empty())
						tmp += _T(", ");
					tmp += j->first->getLastNickHubT();
				}
				return tmp.empty() ? TSTRING(NO_USERS) : tmp;
			}
			else
				return tmp;
		}
		case COLUMN_PATH:
		{
			return Text::toT(getPath());
		}
		case COLUMN_LOCAL_PATH:
		{
			if (!isTorrent())
			{
				tstring l_result;
				if (!m_qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_USER_GET_IP))
				{
					l_result = ShareManager::calc_status_file(getTTH());
				}
				return l_result;
			}
			else
			{
				return Text::toT(m_save_path);
			}
		}
		case COLUMN_EXACT_SIZE:
		{
			return getSize() == -1 ? TSTRING(UNKNOWN) : Util::formatExactSize(getSize());
		}
		case COLUMN_SPEED:
		{
			if (!isTorrent())
				return  Text::toT(Util::formatBytes(m_qi->getAverageSpeed()) + '/' + STRING(S));
			else
				return _T("TODO Speed"); // TODO
		}
		case COLUMN_ERRORS:
		{
			tstring tmp;
			if (!isTorrent())
			{
				RLock(*QueueItem::g_cs);
				const auto& badSources = m_qi->getBadSourcesL();
				for (auto j = badSources.cbegin(); j != badSources.cend(); ++j)
				{
					if (!j->second.isSet(QueueItem::Source::FLAG_REMOVED))
					{
						if (!tmp.empty())
							tmp += _T(", ");
						tmp += j->first->getLastNickHubT();
						
						tmp += _T(" (");
						if (j->second.isSet(QueueItem::Source::FLAG_FILE_NOT_AVAILABLE))
						{
							tmp += TSTRING(FILE_NOT_AVAILABLE);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_PASSIVE))
						{
							tmp += TSTRING(PASSIVE_USER);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_BAD_TREE))
						{
							tmp += TSTRING(INVALID_TREE);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_SLOW_SOURCE))
						{
							tmp += TSTRING(SLOW_USER);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_NO_TTHF))
						{
							tmp += TSTRING(SOURCE_TOO_OLD);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_NO_NEED_PARTS))
						{
							tmp += TSTRING(NO_NEEDED_PART);
						}
						else if (j->second.isSet(QueueItem::Source::FLAG_UNTRUSTED))
						{
							tmp += TSTRING(CERTIFICATE_NOT_TRUSTED);
						}
						tmp += _T(')');
					}
				}
			}
			return tmp.empty() ? TSTRING(NO_ERRORS) : tmp;
		}
		case COLUMN_ADDED:
		{
			return Text::toT(Util::formatDigitalClock(getAdded()));
		}
		case COLUMN_TTH:
		{
			if (!isTorrent())
				return m_qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_USER_GET_IP) ? Util::emptyStringT : Text::toT(getTTH().toBase32());
			else
				return Text::toT(libtorrent::aux::to_hex(m_sha1));
		}
		default:
		{
			return Util::emptyStringT;
		}
	}
}
void QueueFrame::on(DownloadManagerListener::RemoveTorrent, const libtorrent::sha1_hash& p_sha1) noexcept
{
#ifdef _DEBUG // ????
#ifdef FLYLINKDC_USE_TORRENT
	m_tasks.add(REMOVE_ITEM, new StringTask(DownloadManager::getInstance()->get_torrent_name(p_sha1)));
#endif
#endif
}

void QueueFrame::on(DownloadManagerListener::CompleteTorrentFile, const std::string& p_file_name) noexcept
{
}
void QueueFrame::on(DownloadManagerListener::AddedTorrent, const libtorrent::sha1_hash& p_sha1, const std::string& p_save_path) noexcept
{
#ifdef _DEBUG
	m_tasks.add(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(p_sha1, p_save_path)));
#endif
}
void QueueFrame::on(DownloadManagerListener::TorrentEvent, const DownloadArray& p_torrent_event) noexcept
{
#ifdef _DEBUG
	for (auto j = p_torrent_event.cbegin(); j != p_torrent_event.cend(); ++j)
	{
		m_tasks.add(UPDATE_ITEM, new UpdateTask(j->m_path, j->m_sha1));
	}
#endif
}

void QueueFrame::on(QueueManagerListener::AddedArray, const std::vector<QueueItemPtr>& p_qi_array) noexcept
{
	for (auto i = p_qi_array.cbegin(); i != p_qi_array.cend(); ++i)
	{
		m_tasks.add(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(*i)));
	}
}

void QueueFrame::on(QueueManagerListener::Added, const QueueItemPtr& aQI) noexcept
{
	m_tasks.add(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(aQI)));
}

void QueueFrame::addQueueItem(QueueItemInfo* ii, bool noSort)
{
	dcassert(m_closed == false);
	if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
	{
		dcassert(ii->getSize() >= 0);
		m_queueSize += ii->getSize();
	}
	m_queueItems++;
	m_dirty = true;
	
	const string& dir = ii->getPath();
	
	const auto i = m_directories.find(dir);
	const bool updateDir = (i == m_directories.end());
	m_directories.insert(DirectoryMapPair(dir, ii)); // TODO m_directories.insert(i,DirectoryMapPair(dir, ii));
	if (updateDir)
	{
		addDirectory(dir, ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST));
	}
	if (!showTree || isCurDir(dir))
	{
		if (noSort)
		{
			ctrlQueue.insertItem(ctrlQueue.GetItemCount(), ii, I_IMAGECALLBACK);
		}
		else
		{
			ctrlQueue.insertItem(ii, I_IMAGECALLBACK);
		}
	}
}

QueueFrame::QueueItemInfo* QueueFrame::getItemInfo(const string& p_target, const string& p_path) const
{
	dcassert(m_closed == false);
	const auto i = m_directories.equal_range(p_path);
	for (auto j = i.first; j != i.second; ++j)
	{
#ifdef _DEBUG
		//static int g_count = 0;
		//dcdebug("QueueFrame::getItemInfo  count = %d i->second->getTarget() = %s\n", ++g_count, i->second->getTarget().c_str());
#endif
		if (j->second->getTarget() == p_target)
		{
			return j->second;
		}
	}
	return nullptr;
}

void QueueFrame::addQueueList()
{
	CLockRedraw<> l_lock_draw(ctrlQueue);
	CLockRedraw<true> l_lock_draw_dir(ctrlDirs);
	{
		QueueManager::LockFileQueueShared l_fileQueue;
		const auto& li = l_fileQueue.getQueueL();
		for (auto j = li.cbegin(); j != li.cend(); ++j)
		{
			const QueueItemPtr& aQI = j->second;
			QueueItemInfo* ii = new QueueItemInfo(aQI);
			addQueueItem(ii, true);
		}
	}
	ctrlQueue.resort();
}

HTREEITEM QueueFrame::addDirectory(const string& dir, bool isFileList /* = false */, HTREEITEM startAt /* = NULL */)
{
	TVINSERTSTRUCT tvi = {0};
	tvi.hInsertAfter = TVI_SORT;
	tvi.item.mask = TVIF_IMAGE | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_TEXT;
	tvi.item.iImage = tvi.item.iSelectedImage = FileImage::DIR_ICON;
	
	if (isFileList)
	{
		// We assume we haven't added it yet, and that all filelists go to the same
		// directory...
		dcassert(m_fileLists == nullptr);
		tvi.hParent = NULL;
		tvi.item.pszText = _T("File Lists");
		tvi.item.lParam = (LPARAM) new string(dir);
		m_fileLists = ctrlDirs.InsertItem(&tvi);
		return m_fileLists;
	}
	
	// More complicated, we have to find the last available tree item and then see...
	string::size_type i = 0;
	string::size_type j;
	
	HTREEITEM next = NULL;
	HTREEITEM parent = NULL;
	
	if (startAt == NULL)
	{
		// First find the correct drive letter or netpath
		dcassert(dir.size() >= 3);
		dcassert((dir[1] == ':' && dir[2] == '\\') || (dir[0] == '\\' && dir[1] == '\\'));
		
		next = ctrlDirs.GetRootItem();
		
		while (next != nullptr)
		{
			if (next != m_fileLists)
			{
				const string* stmp = reinterpret_cast<string*>(ctrlDirs.GetItemData(next));
				if (stmp)
				{
					if (strnicmp(*stmp, dir, 3) == 0)
						break;
				}
			}
			next = ctrlDirs.GetNextSiblingItem(next);
		}
		
		if (next == NULL)
		{
			// First addition, set commonStart to the dir minus the last part...
			i = dir.rfind('\\', dir.length() - 2);
			if (i != string::npos)
			{
				tstring name = Text::toT(dir.substr(0, i));
				tvi.hParent = NULL;
				tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
				tvi.item.lParam = (LPARAM)new string(dir.substr(0, i + 1));
				next = ctrlDirs.InsertItem(&tvi);
			}
			else
			{
				dcassert((dir.length() == 3 && dir[1] == ':' && dir[2] == '\\') || (dir.length() == 2 && dir[0] == '\\' && dir[1] == '\\'));
				tstring name = Text::toT(dir);
				tvi.hParent = NULL;
				tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
				tvi.item.lParam = (LPARAM)new string(dir);
				next = ctrlDirs.InsertItem(&tvi);
			}
		}
		
		// Ok, next now points to x:\... find how much is common
		
		const string* rootStr = reinterpret_cast<string*>(ctrlDirs.GetItemData(next));
		
		i = 0;
		if (rootStr)
			for (;;)
			{
				j = dir.find('\\', i);
				if (j == string::npos)
					break;
				if (strnicmp(dir.c_str() + i, rootStr->c_str() + i, j - i + 1) != 0)
					break;
				i = j + 1;
			}
			
		if (rootStr && i < rootStr->length())
		{
			HTREEITEM oldRoot = next;
			
			// Create a new root
			tstring name = Text::toT(rootStr->substr(0, i - 1));
			tvi.hParent = NULL;
			tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
			tvi.item.lParam = (LPARAM)new string(rootStr->substr(0, i));
			HTREEITEM newRoot = ctrlDirs.InsertItem(&tvi);
			
			parent = addDirectory(*rootStr, false, newRoot);
			
			next = ctrlDirs.GetChildItem(oldRoot);
			while (next != NULL)
			{
				moveNode(next, parent);
				next = ctrlDirs.GetChildItem(oldRoot);
			}
			delete rootStr;
			ctrlDirs.DeleteItem(oldRoot);
			parent = newRoot;
		}
		else
		{
			// Use this root as parent
			parent = next;
			next = ctrlDirs.GetChildItem(parent);
		}
	}
	else
	{
		parent = startAt;
		next = ctrlDirs.GetChildItem(parent);
		i = getDir(parent).length();
		dcassert(strnicmp(getDir(parent), dir, getDir(parent).length()) == 0);
	}
	
	while (i < dir.length())
	{
		while (next != nullptr)
		{
			if (next != m_fileLists)
			{
				const string& n = getDir(next);
				if (!n.empty() && strnicmp(n.c_str() + i, dir.c_str() + i, n.length() - i) == 0) // i = 56 https://www.box.net/shared/3571b0c47c1a8360aec0  n = {npos=4294967295 } https://www.box.net/shared/487b71099375c9313d2a
				{
					// Found a part, we assume it's the best one we can find...
					i = n.length();
					
					parent = next;
					next = ctrlDirs.GetChildItem(next);
					break;
				}
			}
			next = ctrlDirs.GetNextSiblingItem(next);
		}
		
		if (next == NULL)
		{
			// We didn't find it, add...
			j = dir.find('\\', i);
			// dcassert(j != string::npos);
			tstring name;
			if (j != string::npos)
			{
				name = Text::toT(dir.substr(i, j - i));
			}
			else
			{
				name = Text::toT(dir.substr(i));
			}
			tvi.hParent = parent;
			tvi.item.pszText = const_cast<TCHAR*>(name.c_str());
			tvi.item.lParam = (LPARAM) new string(dir.substr(0, j + 1));
			
			parent = ctrlDirs.InsertItem(&tvi);
			
			i = j + 1;
		}
	}
	
	return parent;
}

void QueueFrame::removeDirectory(const string& dir, bool isFileList /* = false */)
{

	// First, find the last name available
	string::size_type i = 0;
	
	HTREEITEM next = ctrlDirs.GetRootItem();
	HTREEITEM parent = nullptr;
	
	if (isFileList)
	{
		dcassert(m_fileLists != nullptr);
		delete reinterpret_cast<string*>(ctrlDirs.GetItemData(m_fileLists));
		ctrlDirs.DeleteItem(m_fileLists);
		m_fileLists = nullptr;
		return;
	}
	else
	{
		while (i < dir.length())
		{
			while (next != nullptr)
			{
				if (next != m_fileLists)
				{
					const string& n = getDir(next);
					if (strnicmp(n.c_str() + i, dir.c_str() + i, n.length() - i) == 0)
					{
						// Match!
						parent = next;
						next = ctrlDirs.GetChildItem(next);
						i = n.length();
						break;
					}
				}
				next = ctrlDirs.GetNextSiblingItem(next);
			}
			if (next == NULL)
				break;
		}
	}
	
	next = parent;
	dcassert(m_closed == false);
	while (ctrlDirs.GetChildItem(next) == NULL && m_directories.find(getDir(next)) == m_directories.end())
	{
		delete reinterpret_cast<string*>(ctrlDirs.GetItemData(next));
		parent = ctrlDirs.GetParentItem(next);
		
		ctrlDirs.DeleteItem(next);
		if (parent == NULL)
			break;
		next = parent;
	}
}

void QueueFrame::removeDirectories(HTREEITEM ht)
{
	HTREEITEM next = ctrlDirs.GetChildItem(ht);
	while (next != NULL)
	{
		removeDirectories(next);
		next = ctrlDirs.GetNextSiblingItem(ht);
	}
	delete reinterpret_cast<string*>(ctrlDirs.GetItemData(ht));
	ctrlDirs.DeleteItem(ht);
}
void QueueFrame::on(QueueManagerListener::RemovedArray, const std::vector<string>& p_qi_array) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		m_tasks.add(REMOVE_ITEM_ARRAY, new StringArrayTask(p_qi_array));
	}
}

void QueueFrame::on(QueueManagerListener::Removed, const QueueItemPtr& aQI) noexcept
{
	if (!ClientManager::isBeforeShutdown())
	{
		m_tasks.add(REMOVE_ITEM, new StringTask(aQI->getTarget()));
	}
}

void QueueFrame::on(QueueManagerListener::Moved, const QueueItemPtr& aQI, const string& oldTarget) noexcept
{
	m_tasks.add(REMOVE_ITEM, new StringTask(oldTarget));
	m_tasks.add(ADD_ITEM, new QueueItemInfoTask(new QueueItemInfo(aQI)));
}

void QueueFrame::doTimerTask()
{
	if (!MainFrame::isAppMinimized(m_hWnd) && !isClosedOrShutdown() && m_update_status)
	{
		m_tasks.add(UPDATE_STATUSBAR, nullptr);
	}
	CFlyTaskAdapter::doTimerTask();
}

void QueueFrame::on(QueueManagerListener::Tick, const QueueItemList& p_list) noexcept
{
	if (!MainFrame::isAppMinimized(m_hWnd) && !isClosedOrShutdown() && !p_list.empty())
	{
		on(QueueManagerListener::StatusUpdatedList(), p_list);
	}
}

void QueueFrame::on(QueueManagerListener::TargetsUpdated, const StringList& p_targets) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		for (auto i = p_targets.cbegin(); i != p_targets.cend(); ++i)
		{
			m_tasks.add(UPDATE_ITEM, new UpdateTask(*i));
		}
	}
}
void QueueFrame::on(QueueManagerListener::StatusUpdated, const QueueItemPtr& aQI) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		m_tasks.add(UPDATE_ITEM, new UpdateTask(aQI->getTarget()));
	}
}

void  QueueFrame::on(QueueManagerListener::StatusUpdatedList, const QueueItemList& p_list) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		for (auto i = p_list.cbegin(); i != p_list.cend(); ++i)
		{
			on(QueueManagerListener::StatusUpdated(), *i);
		}
	}
}
void QueueFrame::removeItem(const string& p_target)
{
	const auto l_path = Util::getFilePath(p_target);
	const QueueItemInfo* ii = getItemInfo(p_target, l_path);
	if (!ii)
	{
		// Item already delete.
		return;
	}
	
	if (!showTree || isCurDir(ii->getPath()))
	{
		dcassert(ctrlQueue.findItem(ii) != -1);
		ctrlQueue.deleteItem(ii);
	}
	
	if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
	{
		m_queueSize -= ii->getSize();
		dcassert(m_queueSize >= 0);
	}
	m_queueItems--;
	dcassert(m_queueItems >= 0);
	
	dcassert(m_closed == false);
	const auto i = m_directories.equal_range(ii->getPath());
	QueueDirectoryIterC j;
	for (j = i.first; j != i.second; ++j)
	{
		if (j->second == ii)
			break;
	}
	dcassert(j != i.second);
	m_directories.erase(j);
	if (m_directories.find(ii->getPath()) == m_directories.end())
	{
		removeDirectory(ii->getPath(), ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST));
		if (isCurDir(ii->getPath()))
			curDir.clear();
	}
	m_dirty = true;
	delete ii;
	
	if (!m_queueItems)
	{
		m_update_status++;
	}
}
LRESULT QueueFrame::onSpeaker(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	TaskQueue::List t;
	m_tasks.get(t);
	if (t.empty())
		return 0;
	CFlyBusyBool l_busy(m_spoken);
	
	dcassert(m_closed == false);
	for (auto ti = t.cbegin(); ti != t.cend(); ++ti)
	{
		switch (ti->first)
		{
			case ADD_ITEM:
			{
				const auto& iit = static_cast<QueueItemInfoTask&>(*ti->second);
				dcassert(ctrlQueue.findItem(iit.m_ii) == -1);
				addQueueItem(iit.m_ii, false);
				m_dirty = true;
			}
			break;
			case REMOVE_ITEM_ARRAY:
			{
				const auto& l_target_array = static_cast<StringArrayTask&>(*ti->second);
				CLockRedraw<> l_lock_draw(ctrlQueue);
				for (auto i = l_target_array.m_str_array.cbegin(); i != l_target_array.m_str_array.cend(); ++i)
				{
					removeItem(*i);
				}
			}
			break;
			case REMOVE_ITEM:
			{
				const auto& l_target = static_cast<StringTask&>(*ti->second);
				removeItem(l_target.m_str);
			}
			break;
			case UPDATE_ITEM:
			{
				auto &ui = static_cast<UpdateTask&>(*ti->second);
				const auto l_path = Util::getFilePath(ui.getTarget());
				const bool l_is_cur_dir = isCurDir(l_path);
				if (showTree && l_is_cur_dir == false)
				{
					// TODO addQueueItem(ui.m_ii, false);
					//m_dirty = true;
				}
				if (!showTree || l_is_cur_dir)
				{
					const QueueItemInfo* ii = getItemInfo(ui.getTarget(), l_path);
					if (!ii)
					{
						//dcassert(0);
#ifdef _DEBUG
						//LogManager::message("Error find ui.getTarget() +" + ui.getTarget());
#endif
						break;
					}
					
					if (!showTree || l_is_cur_dir)
					{
						const int pos = ctrlQueue.findItem(ii);
						if (pos != -1)
						{
							const int l_top_index = ctrlQueue.GetTopIndex();
							if (pos >= l_top_index && pos <= l_top_index + ctrlQueue.GetCountPerPage())
							{
								ctrlQueue.updateItem(pos, COLUMN_SEGMENTS);
								ctrlQueue.updateItem(pos, COLUMN_PROGRESS);
								ctrlQueue.updateItem(pos, COLUMN_PRIORITY);
								ctrlQueue.updateItem(pos, COLUMN_USERS);
								ctrlQueue.updateItem(pos, COLUMN_ERRORS);
								ctrlQueue.updateItem(pos, COLUMN_STATUS);
								ctrlQueue.updateItem(pos, COLUMN_DOWNLOADED);
								ctrlQueue.updateItem(pos, COLUMN_SPEED);
							}
						}
					}
				}
			}
			break;
			case UPDATE_STATUS:
			{
				auto& status = static_cast<StringTask&>(*ti->second);
				ctrlStatus.SetText(1, Text::toT(status.m_str).c_str());
			}
			break;
			case UPDATE_STATUSBAR:
			{
				updateQueueStatus();
				setCountMessages(ctrlQueue.GetItemCount());
				m_update_status = 0;
			}
			break;
			default:
				dcassert(0);
				break;
		}
		delete ti->second;
	}
	
	return 0;
}

void QueueFrame::removeSelected()
{
	UINT checkState = BOOLSETTING(CONFIRM_DELETE) ? BST_UNCHECKED : BST_CHECKED;
	if (checkState == BST_CHECKED || ::MessageBox(m_hWnd, CTSTRING(REALLY_REMOVE), getFlylinkDCAppCaptionWithVersionT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES)
	{
		CWaitCursor l_cursor_wait;
		try
		{
			ctrlQueue.forEachSelected(&QueueItemInfo::removeBatch);
		}
		catch (const std::bad_alloc&)
		{
			ShareManager::tryFixBadAlloc();
			// fix  https://drdump.com/Problem.aspx?ProblemID=286889
		}
		QueueManager::FileQueue::removeArray();
		QueueManager::getInstance()->fire_remove_batch();
		// Let's update the setting unchecked box means we bug user again...
		SET_SETTING(CONFIRM_DELETE, checkState != BST_CHECKED);
		
	}
}
void QueueFrame::removeAllDir()
{
	if (ctrlDirs.GetSelectedItem())
	{
		UINT checkState = BST_CHECKED;
		if (::MessageBox(m_hWnd, CTSTRING(REALLY_REMOVE), getFlylinkDCAppCaptionWithVersionT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES)
		{
			QueueManager::getInstance()->removeAll();
			ctrlDirs.DeleteAllItems();
			ctrlQueue.DeleteAllItems();
		}
	}
}
void QueueFrame::removeSelectedDir()
{
	if (ctrlDirs.GetSelectedItem())
	{
		UINT checkState = BOOLSETTING(CONFIRM_DELETE) ? BST_UNCHECKED : BST_CHECKED;
		if (checkState == BST_CHECKED || ::MessageBox(m_hWnd, CTSTRING(REALLY_REMOVE), getFlylinkDCAppCaptionWithVersionT().c_str(), CTSTRING(DONT_ASK_AGAIN), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON1, checkState) == IDYES)
		{
			CWaitCursor l_cursor_wait;
			CLockRedraw<> l_lock_draw(ctrlQueue);
			m_tmp_target_to_delete.clear();
			removeDir(ctrlDirs.GetSelectedItem());
			for (auto i = m_tmp_target_to_delete.cbegin(); i != m_tmp_target_to_delete.cend(); ++i)
			{
				QueueManager::getInstance()->removeTarget(*i, true);
			}
			m_tmp_target_to_delete.clear();
			QueueManager::FileQueue::removeArray();
			QueueManager::getInstance()->fire_remove_batch();
			
			
			// Let's update the setting unchecked box means we bug user again...
			SET_SETTING(CONFIRM_DELETE, checkState != BST_CHECKED);
		}
	}
}
void QueueFrame::moveSelected()
{
	tstring name;
	if (showTree)
	{
		name = Text::toT(curDir);
	}
	
	if (WinUtil::browseDirectory(name, m_hWnd))
	{
		vector<const QueueItemInfo*> l_movingItems;
		int j = -1;
		while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
		{
			const QueueItemInfo* ii = ctrlQueue.getItemData(j);
			l_movingItems.push_back(ii);
		}
		const string l_toDir = Text::fromT(name);
		for (auto i = l_movingItems.cbegin(); i != l_movingItems.end(); ++i)
		{
			const QueueItemInfo* ii = *i;
			const auto l_target = ii->getTarget();
			QueueManager::getInstance()->move(l_target, l_toDir + Util::getFileName(l_target));
		}
		
	}
}

void QueueFrame::moveTempArray()
{
	for (auto i = m_move_temp_array.cbegin(); i != m_move_temp_array.cend(); ++i)
	{
		QueueManager::getInstance()->move((*i).first->getTarget(), (*i).second + Util::getFileName((*i).first->getTarget()));
	}
	m_move_temp_array.clear();
}

void QueueFrame::moveSelectedDir()
{
	if (ctrlDirs.GetSelectedItem() == NULL)
		return;
		
	dcassert(!curDir.empty());
	tstring name = Text::toT(curDir);
	
	if (WinUtil::browseDirectory(name, m_hWnd))
	{
		m_move_temp_array.clear();
		moveDir(ctrlDirs.GetSelectedItem(), Text::fromT(name) + Util::getLastDir(getDir(ctrlDirs.GetSelectedItem())) + PATH_SEPARATOR);
		moveTempArray();
	}
}

void QueueFrame::moveDir(HTREEITEM ht, const string& target)
{
	dcassert(m_closed == false);
	HTREEITEM next = ctrlDirs.GetChildItem(ht);
	while (next != NULL)
	{
		// must add path separator since getLastDir only give us the name
		moveDir(next, target + Util::getLastDir(getDir(next)) + PATH_SEPARATOR);
		next = ctrlDirs.GetNextSiblingItem(next);
	}
	
	const string* s = reinterpret_cast<string*>(ctrlDirs.GetItemData(ht));
	
	if (s)
	{
		const auto p = m_directories.equal_range(*s);
		
		for (auto i = p.first; i != p.second; ++i)
		{
			m_move_temp_array.push_back(TempMovePair(i->second, target));
		}
	}
	else
	{
		dcassert(0);
	}
}

void QueueFrame::renameSelected()
{
	// Single file, get the full filename and move...
	const QueueItemInfo* ii = getSelectedQueueItem();
	const tstring target = Text::toT(ii->getTarget());
	const tstring filename = Util::getFileName(target);
	
	LineDlg dlg;
	dlg.title = TSTRING(RENAME);
	dlg.description = TSTRING(FILENAME);
	dlg.line = filename;
	if (dlg.DoModal(m_hWnd) == IDOK)
		QueueManager::getInstance()->move(ii->getTarget(), Text::fromT(target.substr(0, target.length() - filename.length()) + dlg.line));
}

void QueueFrame::renameSelectedDir()
{
	if (ctrlDirs.GetSelectedItem() == NULL)
		return;
		
	dcassert(!curDir.empty());
	const string lname = Util::getLastDir(getDir(ctrlDirs.GetSelectedItem()));
	LineDlg dlg;
	dlg.description = TSTRING(DIRECTORY);
	dlg.title = TSTRING(RENAME);
	dlg.line = Text::toT(lname);
	if (dlg.DoModal(m_hWnd) == IDOK)
	{
		const string name = curDir.substr(0, curDir.length() - (lname.length() + 2)) + PATH_SEPARATOR + Text::fromT(dlg.line) + PATH_SEPARATOR;
		m_move_temp_array.clear();
		moveDir(ctrlDirs.GetSelectedItem(), name);
		moveTempArray();
	}
}

LRESULT QueueFrame::onContextMenu(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	OMenu priorityMenu;
	priorityMenu.CreatePopupMenu();
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_PAUSED, CTSTRING(PAUSED));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_LOWEST, CTSTRING(LOWEST));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_LOW, CTSTRING(LOW));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_NORMAL, CTSTRING(NORMAL));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_HIGH, CTSTRING(HIGH));
	priorityMenu.AppendMenu(MF_STRING, IDC_PRIORITY_HIGHEST, CTSTRING(HIGHEST));
	priorityMenu.AppendMenu(MF_STRING, IDC_AUTOPRIORITY, CTSTRING(AUTO));
	
	if (reinterpret_cast<HWND>(wParam) == ctrlQueue && ctrlQueue.GetSelectedCount() > 0)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlQueue, pt);
		}
		
		OMenu segmentsMenu;
		segmentsMenu.CreatePopupMenu();
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTONE, (_T("1 ") + TSTRING(SEGMENT)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTTWO, (_T("2 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTTHREE, (_T("3 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTFOUR, (_T("4 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTFIVE, (_T("5 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTSIX, (_T("6 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTSEVEN, (_T("7 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTEIGHT, (_T("8 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTNINE, (_T("9 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTTEN, (_T("10 ") + TSTRING(SEGMENTS)).c_str());
		
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTFIFTY, (_T("50 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTHUNDRED, (_T("100 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTHUNDRED_FIFTY, (_T("150 ") + TSTRING(SEGMENTS)).c_str());
		segmentsMenu.AppendMenu(MF_STRING, IDC_SEGMENTTWO_HUNDRED, (_T("200 ") + TSTRING(SEGMENTS)).c_str());
		
		if (ctrlQueue.GetSelectedCount() > 0)
		{
			usingDirMenu = false;
			CMenuItemInfo mi;
			
			while (browseMenu.GetMenuItemCount() > 0)
			{
				browseMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (removeMenu.GetMenuItemCount() > 2)
			{
				removeMenu.RemoveMenu(2, MF_BYPOSITION);
			}
			while (removeAllMenu.GetMenuItemCount() > 0)
			{
				removeAllMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (pmMenu.GetMenuItemCount() > 0)
			{
				pmMenu.RemoveMenu(0, MF_BYPOSITION);
			}
			while (readdMenu.GetMenuItemCount() > 2)
			{
				readdMenu.RemoveMenu(2, MF_BYPOSITION);
			}
			clearPreviewMenu();
			
			if (ctrlQueue.GetSelectedCount() == 1)
			{
				OMenu copyMenu;
				copyMenu.CreatePopupMenu();
				copyMenu.AppendMenu(MF_STRING, IDC_COPY_LINK, CTSTRING(COPY_MAGNET_LINK));
				for (int i = 0; i < COLUMN_LAST; ++i)
					copyMenu.AppendMenu(MF_STRING, IDC_COPY + i, CTSTRING_I(columnNames[i]));
					
				OMenu singleMenu;
				singleMenu.CreatePopupMenu();
				singleMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
				appendPreviewItems(singleMenu);
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)segmentsMenu, CTSTRING(MAX_SEGMENTS_NUMBER));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)browseMenu, CTSTRING(GET_FILE_LIST));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)pmMenu, CTSTRING(SEND_PRIVATE_MESSAGE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)readdMenu, CTSTRING(READD_SOURCE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)copyMenu, CTSTRING(COPY));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
				singleMenu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)removeMenu, CTSTRING(REMOVE_SOURCE));
				singleMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)removeAllMenu, CTSTRING(REMOVE_FROM_ALL));
				singleMenu.AppendMenu(MF_STRING, IDC_REMOVE_OFFLINE, CTSTRING(REMOVE_OFFLINE));
				singleMenu.AppendMenu(MF_SEPARATOR);
				singleMenu.AppendMenu(MF_STRING, IDC_RECHECK, CTSTRING(RECHECK_FILE));
				singleMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
				singleMenu.SetMenuDefaultItem(IDC_SEARCH_ALTERNATES);
				
				const QueueItemInfo* ii = getSelectedQueueItem();
				if (ii && !ii->isTorrent())
				{
					{
						const QueueItemPtr qi = ii->getQueueItem();
						segmentsMenu.CheckMenuItem(IDC_SEGMENTONE - 1 + qi->getMaxSegments(), MF_CHECKED);
					}
					if (!ii->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_PARTIAL_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
					{
						setupPreviewMenu(ii->getTarget());
						activatePreviewItems(singleMenu);
					}
					else
					{
						singleMenu.EnableMenuItem((UINT_PTR)(HMENU)segmentsMenu, MFS_DISABLED);
					}
				}
				menuItems = 0;
				int pmItems = 0;
				if (ii)
					if (!ii->isTorrent())
					{
						RLock(*QueueItem::g_cs);
						const QueueItemPtr qi = ii->getQueueItem();
						if (qi)
						{
							const auto& sources = ii->getQueueItem()->getSourcesL(); // ������ ����� ������
							// ���� ��������� ����� ���������
							for (auto i = sources.cbegin(); i != sources.cend(); ++i)
							{
								const auto user = i->first;
								string l_hub_name;
								if (user->getHubID())
								{
									l_hub_name = CFlylinkDBManager::getInstance()->get_hub_name(user->getHubID());
								}
								tstring nick = WinUtil::escapeMenu(user->getLastNickT() + _T(" (") + Text::toT(l_hub_name) + _T(")"));
								
								// tstring nick = WinUtil::escapeMenu(WinUtil::getNicks(user, Util::emptyString));
								// add hub hint to menu
								//const auto& hubs = ClientManager::getHubNames(user->getCID(), Util::emptyString);
								//if (!hubs.empty())
								//  nick += _T(" (") + Text::toT(hubs[0]) + _T(")");
								
								mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
								mi.fType = MFT_STRING;
								mi.dwTypeData = (LPTSTR)nick.c_str();
								mi.dwItemData = (ULONG_PTR)& i->first;
								mi.wID = IDC_BROWSELIST + menuItems;
								browseMenu.InsertMenuItem(menuItems, TRUE, &mi);
								mi.wID = IDC_REMOVE_SOURCE + 1 + menuItems; // "All" is before sources
								removeMenu.InsertMenuItem(menuItems + 2, TRUE, &mi); // "All" and separator come first
								mi.wID = IDC_REMOVE_SOURCES + menuItems;
								removeAllMenu.InsertMenuItem(menuItems, TRUE, &mi);
								if (user->isOnline())
								{
									mi.wID = IDC_PM + menuItems;
									pmMenu.InsertMenuItem(menuItems, TRUE, &mi);
									pmItems++;
								}
								menuItems++;
							}
							readdItems = 0;
							const auto& badSources = ii->getQueueItem()->getBadSourcesL(); // ������ ����� ������
							// ���� ��������� ����� ���������
							for (auto i = badSources.cbegin(); i != badSources.cend(); ++i)
							{
								const auto& user = i->first;
								tstring nick = WinUtil::getNicks(user, Util::emptyString);
								if (i->second.isSet(QueueItem::Source::FLAG_FILE_NOT_AVAILABLE))
								{
									nick += _T(" (") + TSTRING(FILE_NOT_AVAILABLE) + _T(")");
								}
								else if (i->second.isSet(QueueItem::Source::FLAG_PASSIVE))
								{
									nick += _T(" (") + TSTRING(PASSIVE_USER) + _T(")");
								}
								else if (i->second.isSet(QueueItem::Source::FLAG_BAD_TREE))
								{
									nick += _T(" (") + TSTRING(INVALID_TREE) + _T(")");
								}
								else if (i->second.isSet(QueueItem::Source::FLAG_NO_NEED_PARTS))
								{
									nick += _T(" (") + TSTRING(NO_NEEDED_PART) + _T(")");
								}
								else if (i->second.isSet(QueueItem::Source::FLAG_NO_TTHF))
								{
									nick += _T(" (") + TSTRING(SOURCE_TOO_OLD) + _T(")");
								}
								else if (i->second.isSet(QueueItem::Source::FLAG_SLOW_SOURCE))
								{
									nick += _T(" (") + TSTRING(SLOW_USER) + _T(")");
								}
								else if (i->second.isSet(QueueItem::Source::FLAG_UNTRUSTED))
								{
									nick += _T(" (") + TSTRING(CERTIFICATE_NOT_TRUSTED) + _T(")");
								}
								// add hub hint to menu
								const auto& hubs = ClientManager::getHubNames(user->getCID(), Util::emptyString);
								if (!hubs.empty())
									nick += _T(" (") + Text::toT(hubs[0]) + _T(")");
									
								mi.fMask = MIIM_ID | MIIM_TYPE | MIIM_DATA;
								mi.fType = MFT_STRING;
								mi.dwTypeData = (LPTSTR)nick.c_str();
								mi.dwItemData = (ULONG_PTR) & (*i);
								mi.wID = IDC_READD + 1 + readdItems;  // "All" is before sources
								readdMenu.InsertMenuItem(readdItems + 2, TRUE, &mi);  // "All" and separator come first
								readdItems++;
							}
						}
					}
					
				if (menuItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)browseMenu, MFS_DISABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeMenu, MFS_DISABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeAllMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)browseMenu, MFS_ENABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeMenu, MFS_ENABLED);
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)removeAllMenu, MFS_ENABLED);
				}
				
				if (pmItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)pmMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)pmMenu, MFS_ENABLED);
				}
				
				if (readdItems == 0)
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)readdMenu, MFS_DISABLED);
				}
				else
				{
					singleMenu.EnableMenuItem((UINT_PTR)(HMENU)readdMenu, MFS_ENABLED);
				}
				
				priorityMenu.CheckMenuItem(ii->getPriority(), MF_BYPOSITION | MF_CHECKED);
				if (ii->getAutoPriority())
					priorityMenu.CheckMenuItem(7, MF_BYPOSITION | MF_CHECKED);
					
				singleMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
				
			}
			else
			{
				OMenu multiMenu;
				multiMenu.CreatePopupMenu();
				multiMenu.AppendMenu(MF_STRING, IDC_SEARCH_ALTERNATES, CTSTRING(SEARCH_FOR_ALTERNATES));
				multiMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)segmentsMenu, CTSTRING(MAX_SEGMENTS_NUMBER));
				multiMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
				multiMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
				multiMenu.AppendMenu(MF_SEPARATOR);
				multiMenu.AppendMenu(MF_STRING, IDC_REMOVE_OFFLINE, CTSTRING(REMOVE_OFFLINE));
				multiMenu.AppendMenu(MF_STRING, IDC_RECHECK, CTSTRING(RECHECK_FILE));
				multiMenu.AppendMenu(MF_SEPARATOR);
				multiMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
				multiMenu.SetMenuDefaultItem(IDC_SEARCH_ALTERNATES);
				multiMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
			}
			
			return TRUE;
		}
	}
	else if (reinterpret_cast<HWND>(wParam) == ctrlDirs && ctrlDirs.GetSelectedItem() != NULL)
	{
		POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		
		if (pt.x == -1 && pt.y == -1)
		{
			WinUtil::getContextMenuPos(ctrlDirs, pt);
		}
		else
		{
			// Strange, windows doesn't change the selection on right-click... (!)
			UINT a = 0;
			ctrlDirs.ScreenToClient(&pt);
			HTREEITEM ht = ctrlDirs.HitTest(pt, &a);
			if (ht != NULL && ht != ctrlDirs.GetSelectedItem())
				ctrlDirs.SelectItem(ht);
			ctrlDirs.ClientToScreen(&pt);
		}
		usingDirMenu = true;
		
		OMenu dirMenu;
		dirMenu.CreatePopupMenu();
		dirMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)priorityMenu, CTSTRING(SET_PRIORITY));
		dirMenu.AppendMenu(MF_STRING, IDC_MOVE, CTSTRING(MOVE));
		dirMenu.AppendMenu(MF_STRING, IDC_RENAME, CTSTRING(RENAME));
		dirMenu.AppendMenu(MF_SEPARATOR);
		OMenu l_deleteAllMenu;
		l_deleteAllMenu.CreatePopupMenu();
		l_deleteAllMenu.AppendMenu(MF_STRING, IDC_REMOVE_ALL, CTSTRING(REMOVE_ALL_QUEUE));
		dirMenu.AppendMenu(MF_POPUP, (UINT_PTR)(HMENU)l_deleteAllMenu, CTSTRING(REMOVE_ALL));
		dirMenu.AppendMenu(MF_SEPARATOR);
		dirMenu.AppendMenu(MF_STRING, IDC_REMOVE, CTSTRING(REMOVE));
		dirMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, m_hWnd);
		
		return TRUE;
	}
	
	bHandled = FALSE;
	return FALSE;
}

LRESULT QueueFrame::onRecheck(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	StringList tmp;
	int j = -1;
	while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(j);
		tmp.push_back(ii->getTarget());
	}
	for (auto i = begin(tmp); i != end(tmp); ++i)
	{
		QueueManager::getInstance()->recheck(*i);
	}
	return 0;
}

LRESULT QueueFrame::onSearchAlternates(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		WinUtil::searchHash(ctrlQueue.getItemData(i)->getTTH());
	return 0;
}

LRESULT QueueFrame::onCopyMagnet(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		WinUtil::copyMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize());
	}
	return 0;
}

LRESULT QueueFrame::onBrowseList(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		browseMenu.GetMenuItemInfo(wID, FALSE, &mi);
		OMenuItem* omi = (OMenuItem*)mi.dwItemData;
		if (omi)
		{
			const UserPtr* s   = (UserPtr*)omi->m_data;
			try
			{
				const auto& hubs = ClientManager::getHubNames((*s)->getCID(), Util::emptyString);
				QueueManager::getInstance()->addList(HintedUser(*s, !hubs.empty() ? hubs[0] : Util::emptyString), QueueItem::FLAG_CLIENT_VIEW);
			}
			catch (const Exception&)
			{
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onReadd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		if (ii->isTorrent())
			return 0;
			
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		
		readdMenu.GetMenuItemInfo(wID, FALSE, &mi);
		if (wID == IDC_READD)
		{
			try
			{
				const auto& l_item = ii->getQueueItem();
				QueueManager::getInstance()->readdAll(l_item);
			}
			catch (const QueueException& e)
			{
				ctrlStatus.SetText(1, Text::toT(e.getError()).c_str());
			}
		}
		else
		{
			OMenuItem* omi = (OMenuItem*)mi.dwItemData;
			if (omi)
			{
				const UserPtr s = *(UserPtr*)omi->m_data; // TODO - https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=62702
				// ������� ����
				try
				{
					QueueManager::getInstance()->readd(ii->getTarget(), s);
				}
				catch (const QueueException& e)
				{
					ctrlStatus.SetText(1, Text::toT(e.getError()).c_str());
				}
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onRemoveSource(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		m_remove_source_array.clear();
		int i = ctrlQueue.GetNextItem(-1, LVNI_SELECTED);
		const QueueItemInfo* ii = ctrlQueue.getItemData(i);
		if (ii->isTorrent())
			return 0;
		if (wID == IDC_REMOVE_SOURCE)
		{
			RLock(*QueueItem::g_cs);
			const auto& sources = ii->getQueueItem()->getSourcesL();
			for (auto si = sources.cbegin(); si != sources.cend(); ++si)
			{
				m_remove_source_array.push_back(std::make_pair(ii->getTarget(), si->first));
			}
		}
		else
		{
			CMenuItemInfo mi;
			mi.fMask = MIIM_DATA;
			removeMenu.GetMenuItemInfo(wID, FALSE, &mi);
			OMenuItem* omi = (OMenuItem*)mi.dwItemData;
			if (omi)
			{
				UserPtr* s = (UserPtr*)omi->m_data; // https://drdump.com/Problem.aspx?ProblemID=241344
				m_remove_source_array.push_back(std::make_pair(ii->getTarget(), *s));
			}
		}
		removeSources();
	}
	return 0;
}

LRESULT QueueFrame::onRemoveSources(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CMenuItemInfo mi;
	mi.fMask = MIIM_DATA;
	removeAllMenu.GetMenuItemInfo(wID, FALSE, &mi);
	OMenuItem* omi = (OMenuItem*)mi.dwItemData;
	if (omi)
	{
		if (UserPtr* s = (UserPtr*)omi->m_data)
		{
			QueueManager::getInstance()->removeSource(*s, QueueItem::Source::FLAG_REMOVED);
		}
	}
	return 0;
}

LRESULT QueueFrame::onPM(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		CMenuItemInfo mi;
		mi.fMask = MIIM_DATA;
		
		pmMenu.GetMenuItemInfo(wID, FALSE, &mi);
		OMenuItem* omi = (OMenuItem*)mi.dwItemData;
		if (omi)
		{
			if (UserPtr* s = (UserPtr*)omi->m_data)
			{
				const auto hubs = ClientManager::getHubs((*s)->getCID(), Util::emptyString);
				PrivateFrame::openWindow(nullptr, HintedUser(*s, !hubs.empty() ? hubs[0] : Util::emptyString));
			}
		}
	}
	return 0;
}

LRESULT QueueFrame::onAutoPriority(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{

	if (usingDirMenu)
	{
		setAutoPriority(ctrlDirs.GetSelectedItem(), true);
	}
	else
	{
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			QueueManager::getInstance()->setAutoPriority(ctrlQueue.getItemData(i)->getTarget(), !ctrlQueue.getItemData(i)->getAutoPriority());
		}
	}
	return 0;
}

LRESULT QueueFrame::onSegments(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItemInfo* ii = ctrlQueue.getItemData(i);
		if (!ii->isTorrent())
		{
			{
				const QueueItemPtr& qi = ii->getQueueItem();
				qi->setMaxSegments(std::max((uint8_t)1, (uint8_t)(wID - IDC_SEGMENTONE + 1))); // !BUGMASTER!-S
			}
			ctrlQueue.updateItem(ctrlQueue.findItem(ii), COLUMN_SEGMENTS);
		}
	}
	
	return 0;
}

LRESULT QueueFrame::onPriority(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
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
	
	if (usingDirMenu)
	{
		setPriority(ctrlDirs.GetSelectedItem(), p);
	}
	else
	{
		int i = -1;
		while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
		{
			// TODO - ������� ��������� � ��������� - ������� ������
			const auto l_target = ctrlQueue.getItemData(i)->getTarget();
			QueueManager::getInstance()->setAutoPriority(l_target, false);
			QueueManager::getInstance()->setPriority(l_target, p);
		}
	}
	
	return 0;
}

void QueueFrame::removeDir(HTREEITEM ht)
{
	dcassert(m_closed == false);
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child != NULL)
	{
		removeDir(child);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	const auto dp = m_directories.equal_range(name);
	for (auto i = dp.first; i != dp.second; ++i)
	{
		m_tmp_target_to_delete.push_back(i->second->getTarget());
	}
}

/*
 * @param inc True = increase, False = decrease
 */
void QueueFrame::changePriority(bool inc)
{
	int i = -1;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItem::Priority p = ctrlQueue.getItemData(i)->getPriority();
		
		if ((inc && p == QueueItem::HIGHEST) || (!inc && p == QueueItem::PAUSED))
		{
			// Trying to go higher than HIGHEST or lower than PAUSED
			// so do nothing
			continue;
		}
		
		switch (p)
		{
			case QueueItem::HIGHEST:
				p = QueueItem::HIGH;
				break;
			case QueueItem::HIGH:
				p = inc ? QueueItem::HIGHEST : QueueItem::NORMAL;
				break;
			case QueueItem::NORMAL:
				p = inc ? QueueItem::HIGH    : QueueItem::LOW;
				break;
			case QueueItem::LOW:
				p = inc ? QueueItem::NORMAL  : QueueItem::LOWEST;
				break;
			case QueueItem::LOWEST:
				p = inc ? QueueItem::LOW     : QueueItem::PAUSED;
				break;
			case QueueItem::PAUSED:
				p = QueueItem::LOWEST;
				break;
			default:
				dcassert(false);
				break;
		}
		// TODO - ������� ��������� � ��������� - ������� ������
		const auto l_target = ctrlQueue.getItemData(i)->getTarget();
		QueueManager::getInstance()->setAutoPriority(l_target, false);
		QueueManager::getInstance()->setPriority(l_target, p);
	}
}

void QueueFrame::setPriority(HTREEITEM ht, const QueueItem::Priority& p)
{
	dcassert(m_closed == false);
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child)
	{
		setPriority(child, p);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	const auto dp = m_directories.equal_range(name);
	for (auto i = dp.first; i != dp.second; ++i)
	{
		// TODO - ������� ��������� � ��������� - ������� ������
		// + ������ ��� ����
		QueueManager::getInstance()->setAutoPriority(i->second->getTarget(), false);
		QueueManager::getInstance()->setPriority(i->second->getTarget(), p);
	}
}

void QueueFrame::setAutoPriority(HTREEITEM ht, const bool& ap)
{
	dcassert(m_closed == false);
	if (ht == NULL)
		return;
	HTREEITEM child = ctrlDirs.GetChildItem(ht);
	while (child)
	{
		setAutoPriority(child, ap);
		child = ctrlDirs.GetNextSiblingItem(child);
	}
	const string& name = getDir(ht);
	const auto dp = m_directories.equal_range(name);
	for (auto i = dp.first; i != dp.second; ++i)
	{
		QueueManager::getInstance()->setAutoPriority(i->second->getTarget(), ap);
	}
}

void QueueFrame::updateQueueStatus()
{
	if (m_closed == false && ctrlStatus.IsWindow())
	{
		int64_t total = 0;
		unsigned cnt = ctrlQueue.GetSelectedCount();
		if (cnt == 0)
		{
			cnt = ctrlQueue.GetItemCount();
			
			if (showTree)
			{
				if (m_last_count != cnt)
				{
					int i = -1;
					while (m_closed == false && (i = ctrlQueue.GetNextItem(i, LVNI_ALL)) != -1)
					{
						const QueueItemInfo* ii = ctrlQueue.getItemData(i);
						if (ii)
						{
							const int64_t l_size = ii->getSize(); // https://drdump.com/Problem.aspx?ProblemID=131118 + https://drdump.com/Problem.aspx?ProblemID=143789
							total += l_size;
						}
					}
					m_last_count = cnt;
					m_last_total = total;
				}
				else
				{
					total = m_last_total;
				}
			}
			else
			{
				total = m_queueSize;
			}
		}
		else
		{
			if (m_last_count != cnt)
			{
				int i = -1;
				while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
				{
					const QueueItemInfo* ii = ctrlQueue.getItemData(i);
					total += ii->getSize();
				}
				m_last_count = cnt;
				m_last_total = total;
			}
			else
			{
				total = m_last_total;
			}
		}
		
		tstring tmp1 = TSTRING(ITEMS) + _T(": ") + Util::toStringW(cnt);
		tstring tmp2 = TSTRING(SIZE) + _T(": ") + Util::formatBytesW(total);
		bool u = false;
		
		int w = WinUtil::getTextWidth(tmp1, ctrlStatus.m_hWnd);
		if (statusSizes[1] < w)
		{
			statusSizes[1] = w;
			u = true;
		}
		ctrlStatus.SetText(2, tmp1.c_str());
		w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
		if (statusSizes[2] < w)
		{
			statusSizes[2] = w;
			u = true;
		}
		ctrlStatus.SetText(3, tmp2.c_str());
		
		if (m_dirty)
		{
			tmp1 = TSTRING(FILES) + _T(": ") + Util::toStringW(m_queueItems);
			tmp2 = TSTRING(SIZE) + _T(": ") + Util::formatBytesW(m_queueSize);
			
			w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
			if (statusSizes[3] < w)
			{
				statusSizes[3] = w;
				u = true;
			}
			ctrlStatus.SetText(4, tmp1.c_str());
			
			w = WinUtil::getTextWidth(tmp2, ctrlStatus.m_hWnd);
			if (statusSizes[4] < w)
			{
				statusSizes[4] = w;
				u = true;
			}
			ctrlStatus.SetText(5, tmp2.c_str());
			
			m_dirty = false;
		}
		
		if (u)
		{
			UpdateLayout(TRUE);
		}
	}
}

void QueueFrame::UpdateLayout(BOOL bResizeBars /* = TRUE */)
{
	if (isClosedOrShutdown())
		return;
	{
		RECT rect;
		GetClientRect(&rect);
		// position bars and offset their dimensions
		UpdateBarsPosition(rect, bResizeBars);
		
		if (ctrlStatus.IsWindow())
		{
			CRect sr;
			int w[6];
			ctrlStatus.GetClientRect(sr);
			w[5] = sr.right - 16;
#define setw(x) w[x] = std::max(w[x+1] - statusSizes[x], 0)
			setw(4);
			setw(3);
			setw(2);
			setw(1);
			
			w[0] = 36;
			
			ctrlStatus.SetParts(6, w);
			
			ctrlStatus.GetRect(0, sr);
			if (ctrlShowTree.IsWindow())
				ctrlShowTree.MoveWindow(sr);
		}
		
		if (showTree)
		{
			if (GetSinglePaneMode() != SPLIT_PANE_NONE)
			{
				SetSinglePaneMode(SPLIT_PANE_NONE);
				updateQueue();
			}
		}
		else
		{
			if (GetSinglePaneMode() != SPLIT_PANE_RIGHT)
			{
				SetSinglePaneMode(SPLIT_PANE_RIGHT);
				updateQueue();
			}
		}
		
		CRect rc = rect;
		SetSplitterRect(rc);
	}
}

LRESULT QueueFrame::onClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
	if (!m_closed)
	{
		m_closed = true;
		safe_destroy_timer();
		clear_and_destroy_task();
		SettingsManager::getInstance()->removeListener(this);
		DownloadManager::getInstance()->removeListener(this);
		QueueManager::getInstance()->removeListener(this);
		
		WinUtil::setButtonPressed(IDC_QUEUE, false);
		PostMessage(WM_CLOSE);
		return 0;
	}
	else
	{
		HTREEITEM ht = ctrlDirs.GetRootItem();
		while (ht != NULL)
		{
			clearTree(ht);
			ht = ctrlDirs.GetNextSiblingItem(ht);
		}
		
		SET_SETTING(QUEUEFRAME_SHOW_TREE, ctrlShowTree.GetCheck() == BST_CHECKED);
		ctrlQueue.DeleteAllItems();
		// try fix https://www.crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=101839
		// https://www.crash-server.com/Problem.aspx?ProblemID=43187
		// https://www.crash-server.com/Problem.aspx?ClientID=guest&ProblemID=30936
		for (auto i = m_directories.cbegin(); i != m_directories.cend(); ++i)
		{
			delete i->second;
		}
		m_directories.clear();
		
		ctrlQueue.saveHeaderOrder(SettingsManager::QUEUEFRAME_ORDER,
		                          SettingsManager::QUEUEFRAME_WIDTHS, SettingsManager::QUEUEFRAME_VISIBLE);
		                          
		SET_SETTING(QUEUE_COLUMNS_SORT, ctrlQueue.getSortColumn());
		SET_SETTING(QUEUE_COLUMNS_SORT_ASC, ctrlQueue.isAscending());
		SET_SETTING(QUEUEFRAME_SPLIT, m_nProportionalPos);
		bHandled = FALSE;
		return 0;
	}
}

LRESULT QueueFrame::onItemChanged(int /*idCtrl*/, LPNMHDR /* pnmh */, BOOL& /*bHandled*/)
{
	updateQueue();
	return 0;
}

void QueueFrame::onTab()
{
	if (showTree)
	{
		HWND focus = ::GetFocus();
		if (focus == ctrlDirs.m_hWnd)
		{
			ctrlQueue.SetFocus();
		}
		else if (focus == ctrlQueue.m_hWnd)
		{
			ctrlDirs.SetFocus();
		}
	}
}

void QueueFrame::updateQueue()
{
	dcassert(m_closed == false);
	CWaitCursor l_cursor_wait; //-V808
	ctrlQueue.DeleteAllItems();
	QueueDirectoryPairC i;
	if (showTree)
	{
		i = m_directories.equal_range(getSelectedDir());
	}
	else
	{
		i.first = m_directories.begin();
		i.second = m_directories.end();
	}
	
	CLockRedraw<> l_lock_draw(ctrlQueue);
	auto l_count = ctrlQueue.GetItemCount();
	for (auto j = i.first; j != i.second; ++j)
	{
		QueueItemInfo* ii = j->second;
		ctrlQueue.insertItem(l_count++, ii, I_IMAGECALLBACK);
	}
	ctrlQueue.resort();
	curDir = getSelectedDir();
}

void QueueFrame::clearTree(HTREEITEM item)
{
	dcassert(ctrlDirs.IsWindow());
	if (ctrlDirs.IsWindow())
	{
		HTREEITEM next = ctrlDirs.GetChildItem(item);
		while (next != NULL)
		{
			clearTree(next);
			next = ctrlDirs.GetNextSiblingItem(next);
		}
		dcassert(ctrlDirs.IsWindow());
		if (ctrlDirs.IsWindow())
		{
			delete reinterpret_cast<string*>(ctrlDirs.GetItemData(item));
			ctrlDirs.SetItemData(item, NULL); // fix https://drdump.com/Problem.aspx?ProblemID=191000
		}
	}
}

// Put it here to avoid a copy for each recursion...
void QueueFrame::moveNode(HTREEITEM item, HTREEITEM parent)
{
	static TCHAR g_tmpBuf[1024];
	g_tmpBuf[0] = 0;
	TVINSERTSTRUCT tvis = {0};
	tvis.itemex.hItem = item;
	tvis.itemex.mask = TVIF_CHILDREN | TVIF_HANDLE | TVIF_IMAGE | TVIF_INTEGRAL | TVIF_PARAM |
	                   TVIF_SELECTEDIMAGE | TVIF_STATE | TVIF_TEXT;
	tvis.itemex.pszText = g_tmpBuf;
	tvis.itemex.cchTextMax = _countof(g_tmpBuf);
	ctrlDirs.GetItem((TVITEM*)&tvis.itemex);
	tvis.hInsertAfter = TVI_SORT;
	tvis.hParent = parent;
	tvis.item.mask &= ~TVIF_HANDLE;
	HTREEITEM ht = ctrlDirs.InsertItem(&tvis);
	HTREEITEM next = ctrlDirs.GetChildItem(item);
	while (next != NULL)
	{
		moveNode(next, ht);
		next = ctrlDirs.GetChildItem(item);
	}
	ctrlDirs.DeleteItem(item);
}

LRESULT QueueFrame::onCustomDraw(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLVCUSTOMDRAW* cd = (NMLVCUSTOMDRAW*)pnmh;
	
	const QueueItemInfo *ii = (const QueueItemInfo*)cd->nmcd.lItemlParam;
	switch (cd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;
		case CDDS_ITEMPREPAINT:
		{
#if 0
			if (ii &&
			        !ii->isTorrent() &&
			        ii->getQueueItem() &&  // https://drdump.com/Problem.aspx?ProblemID=259662
			        !ii->getQueueItem()->getBadSourcesL().empty()) // https://www.crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=117848
			{
				cd->clrText = SETTING(ERROR_COLOR);
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
				Colors::alternationBkColor(cd);
#endif
				return CDRF_NEWFONT | CDRF_NOTIFYSUBITEMDRAW;
			}
#ifdef FLYLINKDC_USE_LIST_VIEW_MATTRESS
			Colors::alternationBkColor(cd);
#endif
			return CDRF_NOTIFYSUBITEMDRAW;
#else
			return CDRF_NOTIFYSUBITEMDRAW;
#endif
		}
		case CDDS_SUBITEM | CDDS_ITEMPREPAINT:
		{
			if (ctrlQueue.findColumn(cd->iSubItem) == COLUMN_PROGRESS)
			{
				if (!BOOLSETTING(SHOW_PROGRESS_BARS))
				{
					bHandled = FALSE;
					return 0;
				}
				if (ii->getSize() == -1) return CDRF_DODEFAULT;
				
				// draw something nice...
				CRect rc;
				ctrlQueue.GetSubItemRect((int)cd->nmcd.dwItemSpec, cd->iSubItem, LVIR_BOUNDS, rc);
				CBarShader statusBar(rc.Height(), rc.Width(), SETTING(PROGRESS_BACK_COLOR), ii->getSize());
				if (!ii->isTorrent())
				{
					try
					{
						vector<pair<Segment, Segment>> l_runnigChunksAndDownloadBytes;
						vector<Segment> l_doneChunks;
						QueueManager::getChunksVisualisation(ii->getQueueItem(), l_runnigChunksAndDownloadBytes, l_doneChunks);
						for (auto i = l_runnigChunksAndDownloadBytes.cbegin(); i < l_runnigChunksAndDownloadBytes.cend(); ++i)
						{
							statusBar.FillRange((*i).first.getStart(), (*i).first.getEnd(), SETTING(COLOR_RUNNING));
							statusBar.FillRange((*i).second.getStart(), (*i).second.getEnd(), SETTING(COLOR_DOWNLOADED));
						}
						for (auto i = l_doneChunks.cbegin(); i < l_doneChunks.cend(); ++i)
						{
							statusBar.FillRange((*i).getStart(), (*i).getEnd(), SETTING(COLOR_DOWNLOADED));
						}
					}
					catch (std::bad_alloc&)
					{
						ShareManager::tryFixBadAlloc(); // https://drdump.com/Problem.aspx?ProblemID=238815
					}
				}
				CDC cdc;
				cdc.CreateCompatibleDC(cd->nmcd.hdc);
				HBITMAP pOldBmp = cdc.SelectBitmap(CreateCompatibleBitmap(cd->nmcd.hdc,  rc.Width(),  rc.Height()));
				
				statusBar.Draw(cdc, 0, 0, SETTING(PROGRESS_3DDEPTH));
				BitBlt(cd->nmcd.hdc, rc.left, rc.top, rc.Width(), rc.Height(), cdc.m_hDC, 0, 0, SRCCOPY);
				DeleteObject(cdc.SelectBitmap(pOldBmp));
				
				return CDRF_SKIPDEFAULT;
			}
		}
		default:
			return CDRF_DODEFAULT;
	}
}

LRESULT QueueFrame::onCopy(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	tstring data;
	int i = -1, columnId = wID - IDC_COPY;
	while ((i = ctrlQueue.GetNextItem(i, LVNI_SELECTED)) != -1)
	{
		QueueItemInfo* ii = ctrlQueue.getItemData(i);
		tstring sdata;
		
		if (wID == IDC_COPY_LINK)
			sdata = Text::toT(Util::getMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize()));
		else if (wID == IDC_COPY_WMLINK)
			sdata = Text::toT(Util::getWebMagnet(ii->getTTH(), Util::getFileName(ii->getTarget()), ii->getSize()));
		else
			sdata = ii->getText(columnId);
			
		if (data.empty())
			data = sdata;
		else
			data = data + L"\r\n" + sdata;
	}
	WinUtil::setClipboard(data);
	return 0;
}

LRESULT QueueFrame::onPreviewCommand(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (ctrlQueue.GetSelectedCount() == 1)
	{
		const QueueItemInfo* ii = getSelectedQueueItem();
		if (!ii->isTorrent())
		{
			const auto& qi = ii->getQueueItem();
			startMediaPreview(wID, qi);
		}
	}
	return 0;
}

LRESULT QueueFrame::onRemoveOffline(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	int j = -1;
	m_remove_source_array.clear();
	while ((j = ctrlQueue.GetNextItem(j, LVNI_SELECTED)) != -1)
	{
		const QueueItemInfo* ii = ctrlQueue.getItemData(j);
		if (!ii->isTorrent())
		{
			RLock(*QueueItem::g_cs);
			const auto& sources = ii->getQueueItem()->getSourcesL();
			for (auto i = sources.cbegin(); i != sources.cend(); ++i)  // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=111640
			{
				if (!i->first->isOnline())
				{
					m_remove_source_array.push_back(std::make_pair(ii->getTarget(), i->first));
				}
			}
		}
	}
	removeSources();
	return 0;
}
void QueueFrame::removeSources()
{
	for (auto j = m_remove_source_array.cbegin(); j != m_remove_source_array.cend(); ++j)
	{
		QueueManager::getInstance()->removeSource(j->first, j->second, QueueItem::Source::FLAG_REMOVED);
	}
}
void QueueFrame::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
	{
		if (ctrlQueue.isRedraw())
		{
			ctrlQueue.setFlickerFree(Colors::g_bgBrush);
			ctrlDirs.SetBkColor(Colors::g_bgColor);
			ctrlDirs.SetTextColor(Colors::g_textColor);
			RedrawWindow(NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
		}
	}
}

void QueueFrame::onRechecked(const string& target, const string& message)
{
	if (!ClientManager::isBeforeShutdown())
	{
		string buf;
		buf.resize(STRING(INTEGRITY_CHECK).length() + message.length() + target.length() + 16);
		sprintf(&buf[0], CSTRING(INTEGRITY_CHECK), message.c_str(), target.c_str());
		m_tasks.add(UPDATE_STATUS, new StringTask(buf));
	}
}

void QueueFrame::on(QueueManagerListener::RecheckStarted, const string& target) noexcept
{
	onRechecked(target, STRING(STARTED));
}

void QueueFrame::on(QueueManagerListener::RecheckNoFile, const string& target) noexcept
{
	onRechecked(target, STRING(UNFINISHED_FILE_NOT_FOUND));
}

void QueueFrame::on(QueueManagerListener::RecheckFileTooSmall, const string& target) noexcept
{
	onRechecked(target, STRING(UNFINISHED_FILE_TOO_SMALL));
}

void QueueFrame::on(QueueManagerListener::RecheckDownloadsRunning, const string& target) noexcept
{
	onRechecked(target, STRING(DOWNLOADS_RUNNING));
}

void QueueFrame::on(QueueManagerListener::RecheckNoTree, const string& target) noexcept
{
	onRechecked(target, STRING(NO_FULL_TREE));
}

void QueueFrame::on(QueueManagerListener::RecheckAlreadyFinished, const string& target) noexcept
{
	onRechecked(target, STRING(FILE_ALREADY_FINISHED));
}

void QueueFrame::on(QueueManagerListener::RecheckDone, const string& target) noexcept
{
	onRechecked(target, STRING(DONE));
}

LRESULT QueueFrame::onKeyDown(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMLVKEYDOWN* kd = (NMLVKEYDOWN*)pnmh;
	if (kd->wVKey == VK_DELETE)
	{
		if (ctrlQueue.getSelectedCount())
		{
			removeSelected();
		}
	}
	else if (kd->wVKey == VK_ADD)
	{
		// Increase Item priority
		changePriority(true);
	}
	else if (kd->wVKey == VK_SUBTRACT)
	{
		// Decrease item priority
		changePriority(false);
	}
	else if (kd->wVKey == VK_TAB)
	{
		onTab();
	}
	return 0;
}

LRESULT QueueFrame::onKeyDownDirs(int /*idCtrl*/, LPNMHDR pnmh, BOOL& /*bHandled*/)
{
	NMTVKEYDOWN* kd = (NMTVKEYDOWN*)pnmh;
	if (kd->wVKey == VK_DELETE)
	{
		removeSelectedDir();
	}
	else if (kd->wVKey == VK_TAB)
	{
		onTab();
	}
	return 0;
}
string QueueFrame::getSelectedDir() const
{
	HTREEITEM ht = ctrlDirs.GetSelectedItem();
	return ht == NULL ? Util::emptyString : getDir(ctrlDirs.GetSelectedItem());
}

string QueueFrame::getDir(HTREEITEM ht) const
{
	dcassert(ht != NULL);
	if (ht)
	{
		const auto l_str = reinterpret_cast<string*>(ctrlDirs.GetItemData(ht));
		if (l_str)
			return *l_str;
		else
		{
			dcassert(0);
			return Util::emptyString;
		}
	}
	else
		return Util::emptyString;
}

/**
 * @file
 * $Id: QueueFrame.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
