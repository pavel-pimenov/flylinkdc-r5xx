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

#include "stdinc.h"

#if !defined(_WIN32) && !defined(PATH_MAX) // Extra PATH_MAX check for Mac OS X
#include <sys/syslimits.h>
#endif

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/for_each.hpp>
#include "QueueManager.h"
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "Download.h"
#include "UploadManager.h"
#include "Wildcards.h"
#include "MerkleCheckOutputStream.h"
#include "SearchResult.h"
#include "SharedFileStream.h"
#include "ADLSearch.h"
#include "../FlyFeatures/flyServer.h"


#ifdef STRONG_USE_DHT
#include "../dht/IndexManager.h"
#else
#include "ShareManager.h"
#endif


std::unique_ptr<webrtc::RWLockWrapper> QueueManager::FileQueue::g_cs_remove = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::vector<int64_t> QueueManager::FileQueue::g_remove_id_array;
#ifdef FLYLINKDC_USE_RWLOCK
std::unique_ptr<webrtc::RWLockWrapper> QueueManager::FileQueue::g_csFQ = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#else
std::unique_ptr<CriticalSection> QueueManager::FileQueue::g_csFQ = std::unique_ptr<CriticalSection>(new CriticalSection);
#endif
QueueItem::QIStringMap QueueManager::FileQueue::g_queue;
boost::unordered_map<TTHValue, int> QueueManager::FileQueue::g_queue_tth_map;

QueueManager::FileQueue QueueManager::g_fileQueue;
QueueManager::UserQueue QueueManager::g_userQueue;
bool QueueManager::g_dirty = false;
uint64_t QueueManager::g_lastSave = 0;
QueueManager::UserQueue::UserQueueMap QueueManager::UserQueue::g_userQueueMap[QueueItem::LAST];
QueueManager::UserQueue::RunningMap QueueManager::UserQueue::g_runningMap;
#ifdef FLYLINKDC_USE_USER_QUEUE_CS
Lock std::unique_ptr<webrtc::RWLockWrapper> QueueManager::UserQueue::g_userQueueMapCS = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#endif
#ifdef FLYLINKDC_USE_RUNNING_QUEUE_CS
std::unique_ptr<webrtc::RWLockWrapper> QueueManager::UserQueue::g_runningMapCS = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#endif


using boost::adaptors::map_values;
using boost::range::for_each;


class DirectoryItem
#ifdef _DEBUG
	: private boost::noncopyable
#endif
{
	public:
		//DirectoryItem() : priority(QueueItem::DEFAULT) { }
		DirectoryItem(const UserPtr& aUser, const string& aName, const string& aTarget,
		              QueueItem::Priority p) : m_name(aName), m_target(aTarget), m_priority(p), m_user(aUser) { }
		              
		const UserPtr& getUser() const
		{
			return m_user;
		}
		
		GETSET(string, m_name, Name);
		GETSET(string, m_target, Target);
		GETSET(QueueItem::Priority, m_priority, Priority);
	private:
		const UserPtr m_user;
};
QueueManager::FileQueue::FileQueue()
{
}

QueueManager::FileQueue::~FileQueue()
{
	dcassert(g_remove_id_array.empty());
	//dcassert(g_queue_tth_map.empty());
}

QueueItemPtr QueueManager::FileQueue::add(int64_t p_FlyQueueID,
                                          const string& aTarget,
                                          int64_t aSize,
                                          Flags::MaskType aFlags,
                                          QueueItem::Priority p,
                                          const string& aTempTarget,
                                          time_t aAdded,
                                          const TTHValue& root,
                                          uint8_t p_maxSegments)
{
	if (p == QueueItem::DEFAULT)
	{
		if (aSize <= SETTING(PRIO_HIGHEST_SIZE) * 1024)
		{
			p = QueueItem::HIGHEST;
		}
		else if (aSize <= SETTING(PRIO_HIGH_SIZE) * 1024)
		{
			p = QueueItem::HIGH;
		}
		else if (aSize <= SETTING(PRIO_NORMAL_SIZE) * 1024)
		{
			p = QueueItem::NORMAL;
		}
		else if (aSize <= SETTING(PRIO_LOW_SIZE) * 1024)
		{
			p = QueueItem::LOW;
		}
		else if (SETTING(PRIO_LOWEST))
		{
			p = QueueItem::LOWEST;
		}
	}
	
	QueueItemPtr qi(new QueueItem(aTarget, aSize, p, aFlags, aAdded, root, p_maxSegments, p_FlyQueueID, aTempTarget));
	
	if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP))
	{
		qi->setPriority(QueueItem::HIGHEST);
	}
	else
	{
		qi->setMaxSegments(getMaxSegments(qi->getSize()));
		
		if (p == QueueItem::DEFAULT)
		{
			if (BOOLSETTING(AUTO_PRIORITY_DEFAULT))
			{
				qi->setAutoPriority(true);
				qi->setPriority(QueueItem::LOW);
			}
			else
			{
				qi->setPriority(QueueItem::NORMAL);
			}
		}
	}
	
	if (!aTempTarget.empty())
	{
		if (!File::isExist(aTempTarget) && File::isExist(aTempTarget + ".antifrag"))
		{
			// load old antifrag file
			File::renameFile(aTempTarget + ".antifrag", qi->getTempTarget());
		}
	}
	dcassert(find_target(aTarget) == nullptr);
	if (find_target(aTarget) == nullptr)
	{
		add(qi);
	}
	else
	{
		qi.reset();
		LogManager::message("Skip duplicate target QueueManager::FileQueue::add file = " + aTarget);
	}
	return qi;
}

void QueueManager::FileQueue::add(const QueueItemPtr& qi) // [!] IRainman fix.
{
	WLock(*g_csFQ); // [+] IRainman fix.
	g_queue.insert(make_pair(qi->getTarget(), qi));
	auto l_count_tth = g_queue_tth_map.insert(make_pair(qi->getTTH(), 1));
	if (l_count_tth.second == false)
	{
		l_count_tth.first->second++;
	}
}
void QueueManager::FileQueue::remove_internal(const QueueItemPtr& qi)
{
	WLock(*g_csFQ); // [+] IRainman fix.
	g_queue.erase(qi->getTarget());
	auto l_count_tth = g_queue_tth_map.find(qi->getTTH());
	dcassert(l_count_tth != g_queue_tth_map.end());
	if (l_count_tth != g_queue_tth_map.end())
	{
		if (l_count_tth->second == 1)
		{
			g_queue_tth_map.erase(l_count_tth);
		}
		else
		{
			--l_count_tth->second;
		}
	}
}
void QueueManager::FileQueue::clearAll()
{
	WLock(*g_csFQ);
	g_queue_tth_map.clear();
	g_queue.clear();
}
void QueueManager::FileQueue::removeArray()
{
	std::vector<int64_t> l_remove_id_array;
	{
		CFlyReadLock(*g_cs_remove);
		l_remove_id_array.swap(g_remove_id_array);
	}
	CFlylinkDBManager::getInstance()->remove_queue_item_array(l_remove_id_array);
}
void QueueManager::FileQueue::removeDeferredDB(const QueueItemPtr& qi, bool p_is_batch_remove)
{
	remove_internal(qi);
	const auto l_id = qi->getFlyQueueID();
	if (l_id)
	{
		if (p_is_batch_remove)
		{
			CFlyWriteLock(*g_cs_remove);
			g_remove_id_array.push_back(l_id);
		}
		else
		{
			std::vector<int64_t> l_remove_id_array;
			l_remove_id_array.push_back(l_id);
			CFlylinkDBManager::getInstance()->remove_queue_item_array(l_remove_id_array);
		}
	}
}

int QueueManager::FileQueue::find_tth(QueueItemList& p_ql, const TTHValue& p_tth, int p_count_limit /*= 0 */)
{
	int l_count = 0;
	RLock(*g_csFQ); // [+] IRainman fix.
	if (g_queue_tth_map.find(p_tth) != g_queue_tth_map.end())
	{
		for (auto i = g_queue.cbegin(); i != g_queue.cend(); ++i)
		{
			const QueueItemPtr& qi = i->second;
			if (qi->getTTH() == p_tth)
			{
				p_ql.push_back(qi);
				if (p_count_limit == ++l_count)
					break;
			}
		}
	}
	return l_count;
}

QueueItemPtr QueueManager::FileQueue::findQueueItem(const TTHValue& p_tth)
{
	QueueItemList l_ql;
	///////////l_ql.reserve(1);
	if (find_tth(l_ql, p_tth, 1))
	{
		return l_ql.front();
	}
	return nullptr;
}

static QueueItemPtr findCandidateL(const QueueItem::QIStringMap::const_iterator& start, const QueueItem::QIStringMap::const_iterator& end, deque<string>& recent)
{
	QueueItemPtr cand = nullptr;
	
	for (auto i = start; i != end; ++i)
	{
		const QueueItemPtr& q = i->second;
		// We prefer to search for things that are not running...
		if (cand != nullptr && q->getNextSegmentL(0, 0, 0, nullptr).getSize() == 0)
			continue;
		// No finished files
		if (q->isFinishedL())
			continue;
		// No user lists
		if (q->isSet(QueueItem::FLAG_USER_LIST))
			continue;
		// No IP check
		if (q->isSet(QueueItem::FLAG_USER_GET_IP)) // [+] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=1092
			continue;
		// No paused downloads
		if (q->getPriority() == QueueItem::PAUSED)
			continue;
		// No files that already have more than AUTO_SEARCH_LIMIT online sources
		if (q->countOnlineUsersGreatOrEqualThanL(SETTING(AUTO_SEARCH_LIMIT)))
			continue;
		// Did we search for it recently?
		if (find(recent.begin(), recent.end(), q->getTarget()) != recent.end())
			continue;
			
		cand = q;
		
		if (cand->getNextSegmentL(0, 0, 0, nullptr).getSize() != 0)
			break;
	}
	
	//check this again, if the first item we pick is running and there are no
	//other suitable items this will be true
	if (cand && cand->getNextSegmentL(0, 0, 0, nullptr).getSize() == 0)
	{
		cand = nullptr;
	}
	
	return cand;
}

QueueItemPtr QueueManager::FileQueue::findAutoSearch(deque<string>& p_recent) const // [!] IRainman fix.
{
	WLock(*QueueItem::g_cs); // [+] IRainman fix.
	{
		RLock(*g_csFQ);
		dcassert(!g_queue.empty()); // https://crash-server.com/Problem.aspx?ProblemID=32091
		// http://code.google.com/p/flylinkdc/issues/detail?id=1121
		// We pick a start position at random, hoping that we will find something to search for...
		if (!g_queue.empty())
		{
			const auto start = (QueueItem::QIStringMap::size_type)Util::rand((uint32_t)g_queue.size());
			
			auto i = g_queue.cbegin();
			advance(i, start);
			
			QueueItemPtr cand = findCandidateL(i, g_queue.end(), p_recent);
			if (cand == nullptr)
			{
				cand = findCandidateL(g_queue.begin(), i, p_recent);
			}
			else if (cand->getNextSegmentL(0, 0, 0, nullptr).getSize() == 0)
			{
				QueueItemPtr cand2 = findCandidateL(g_queue.begin(), i, p_recent);
				if (cand2 != nullptr && cand2->getNextSegmentL(0, 0, 0, nullptr).getSize() != 0)
				{
					cand = cand2;
				}
			}
			return cand;
		}
		else
		{
			return QueueItemPtr();
		}
		
	}
}

void QueueManager::FileQueue::moveTarget(const QueueItemPtr& qi, const string& aTarget)
{
	remove_internal(qi);
	//[-]PPA qi->dec(); // [+] IRainman fix. // ??????
	qi->setTarget(aTarget);
	add(qi);
}

#ifndef IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
bool QueueManager::UserQueue::userIsDownloadedFiles(const UserPtr& aUser, QueueItemList& p_status_update_array)
{
	bool hasDown = false;
#ifdef FLYLINKDC_USE_USER_QUEUE_CS
	Lock CFlyReadLock(*g_userQueueMapCS);
#endif
	for (size_t i = 0; i < QueueItem::LAST && !ClientManager::isShutdown(); ++i)
	{
		const auto j = g_userQueueMap[i].find(aUser);
		if (j != g_userQueueMap[i].end())
		{
			p_status_update_array.insert(p_status_update_array.end(), j->second.cbegin(), j->second.cend()); // Без лока?
			if (i != QueueItem::PAUSED)
			{
				hasDown = true;
			}
		}
	}
	return hasDown;
}
#endif // IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED

void QueueManager::UserQueue::addL(const QueueItemPtr& qi) // [!] IRainman fix.
{
	for (auto i = qi->getSourcesL().cbegin(); i != qi->getSourcesL().cend(); ++i)
	{
		addL(qi, i->first, false);
	}
}
void QueueManager::UserQueue::addL(const QueueItemPtr& qi, const UserPtr& aUser, bool p_is_first_load) // [!] IRainman fix.
{
	dcassert(qi->getPriority() < QueueItem::LAST);
#ifdef FLYLINKDC_USE_USER_QUEUE_CS
	Lock CFlyWriteLock(*g_userQueueMapCS);
#endif
	auto& uq = g_userQueueMap[qi->getPriority()][aUser];
	
// При первой загрузки очереди из базы не зовем calcAverageSpeedAndCalcAndGetDownloadedBytesL
	if (p_is_first_load == false
	        && (
#ifdef IRAINMAN_INCLUDE_USER_CHECK
	            qi->isSet(QueueItem::FLAG_USER_CHECK) ||
#endif
	            qi->calcAverageSpeedAndCalcAndGetDownloadedBytesL() > 0))
	{
		uq.push_front(qi);
	}
	else
	{
		uq.push_back(qi);
	}
#ifdef _DEBUG
	if (((uq.size() + 1) % 100) == 0)
	{
		// LogManager::message("uq.size() % 100 = size = " + Util::toString(uq.size()));
	}
#endif
}

bool QueueManager::FileQueue::getTTH(const string& p_name, TTHValue& p_tth)
{
	RLock(*g_csFQ);
	auto i = g_queue.find(p_name);
	if (i != g_queue.cend())
	{
		p_tth = i->second->getTTH();
		return true;
	}
	return false;
}

QueueItemPtr QueueManager::FileQueue::find_target(const string& p_target)
{
	RLock(*g_csFQ); // [+] IRainman fix.
	auto i = g_queue.find(p_target); // TODO - fix http://code.google.com/p/flylinkdc/issues/detail?id=1246
	if (i != g_queue.cend())
		return i->second;
	return nullptr;
}

QueueItemPtr QueueManager::UserQueue::getNextL(const UserPtr& aUser, QueueItem::Priority minPrio, int64_t wantedSize, int64_t lastSpeed, bool allowRemove) // [!] IRainman fix.
{
	int p = QueueItem::LAST - 1;
	m_lastError.clear();
	do
	{
#ifdef FLYLINKDC_USE_USER_QUEUE_CS
		CFlyReadLock(*g_userQueueMapCS);
#endif
		const auto i = g_userQueueMap[p].find(aUser);
		if (i != g_userQueueMap[p].cend())
		{
			dcassert(!i->second.empty());
			for (auto j = i->second.cbegin(); j != i->second.cend(); ++j)
			{
				const QueueItemPtr& qi = *j;
				const auto l_source = qi->findSourceL(aUser); // [!] IRainman fix done: [10] https://www.box.net/shared/6ea561f898012606519a
				if (l_source == qi->m_sources.end()) //[+]PPA
					continue;
				if (l_source->second.isSet(QueueItem::Source::FLAG_PARTIAL)) // TODO Crash
				{
					// check partial source
					const Segment segment = qi->getNextSegmentL(qi->get_block_size_sql(), wantedSize, lastSpeed, l_source->second.getPartialSource());
					if (allowRemove && segment.getStart() != -1 && segment.getSize() == 0)
					{
						// no other partial chunk from this user, remove him from queue
						removeUserL(qi, aUser, true);
						qi->removeSourceL(aUser, QueueItem::Source::FLAG_NO_NEED_PARTS); // https://drdump.com/Problem.aspx?ProblemID=129066
						m_lastError = STRING(NO_NEEDED_PART);
						return nullptr; // airDC++ http://code.google.com/p/flylinkdc/issues/detail?id=1365
					}
				}
				if (qi->isWaitingL()) // там внутри m_downloads.empty(); поэтому след можно сделать qi->getDownloadsL().front()
				{
					// check maximum simultaneous files setting
					const auto l_file_slot = (size_t)SETTING(FILE_SLOTS);
					if (l_file_slot == 0 ||
					        qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP) ||
					        QueueManager::getRunningFileCount(l_file_slot) < l_file_slot) // TODO - двойной лок
					{
						return qi;
					}
					else
					{
						m_lastError = STRING(ALL_FILE_SLOTS_TAKEN);
						continue;
					}
				}
				else if (qi->getDownloadsL().begin()->second->getType() == Transfer::TYPE_TREE) // No segmented downloading when getting the tree
				{
					continue;
				}
				if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
				{
					const auto blockSize = qi->get_block_size_sql();
					const Segment segment = qi->getNextSegmentL(blockSize, wantedSize, lastSpeed, l_source->second.getPartialSource());
					if (segment.getSize() == 0)
					{
						m_lastError = segment.getStart() == -1 ? STRING(ALL_DOWNLOAD_SLOTS_TAKEN) : STRING(NO_FREE_BLOCK);
						dcdebug("No segment for %s in %s, block " I64_FMT "\n", aUser->getCID().toBase32().c_str(), qi->getTarget().c_str(), blockSize);
						continue;
					}
				}
				return qi;
			}
		}
		p--;
	}
	while (p >= minPrio);
	
	return nullptr;
}

void QueueManager::UserQueue::addDownloadL(const QueueItemPtr& qi, const DownloadPtr& d) // [!] IRainman fix: this function needs external lock.
{
	qi->addDownloadL(d);
	// Only one download per user...
#ifdef FLYLINKDC_USE_RUNNING_QUEUE_CS
	CFlyWriteLock(*g_runningMapCS);
#endif
	dcassert(g_runningMap.find(d->getUser()) == g_runningMap.end());
	g_runningMap[d->getUser()] = qi;
}

size_t QueueManager::FileQueue::getRunningFileCount(const size_t p_stop_key)
{
	size_t l_cnt = 0;
	RLock(*g_csFQ); // [+] IRainman fix.
	for (auto i = g_queue.cbegin(); i != g_queue.cend(); ++i)
	{
		const QueueItemPtr& q = i->second;
		if (q->isRunningL()) //
		{
			++l_cnt;
			if (l_cnt > p_stop_key && p_stop_key != 0)
				break; // Выходим раньше и не бегаем по всей очереди.
		}
		// TODO - можно не бегать по очереди а считать в онлайне.
		// Алгоритм:
		// 1. при удалении из g_queue -1 (если m_queue.download заполнена)
		// 2  при добавлении к g_queue +1 (если m_queue.download заполнена)
		// 2. при добавлении в g_queue.download +1
		// 3. при опустошении g_queue.download -1
	}
	return l_cnt;
}

void QueueManager::FileQueue::calcPriorityAndGetRunningFilesL(QueueItem::PriorityArray& p_changedPriority, QueueItemList& p_runningFiles)
{
	for (auto i = g_queue.cbegin(); i != g_queue.cend(); ++i)
	{
		const QueueItemPtr& q = i->second;
		if (q->isRunningL())
		{
			p_runningFiles.push_back(q);
			// TODO calcAverageSpeedAndDownloadedBytes - тяжелая операция зачем зовем так часто?
			q->calcAverageSpeedAndCalcAndGetDownloadedBytesL(); // [+] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=992
			if (q->getAutoPriority())
			{
				const QueueItem::Priority p1 = q->getPriority();
				if (p1 != QueueItem::PAUSED)
				{
					const QueueItem::Priority p2 = q->calculateAutoPriority();
					if (p1 != p2)
					{
						p_changedPriority.push_back(make_pair(q->getTarget(), p2));
					}
				}
			}
		}
	}
}

bool QueueManager::UserQueue::removeDownloadL(const QueueItemPtr& qi, const UserPtr& user) // [!] IRainman fix: this function needs external lock.
{
	{
#ifdef FLYLINKDC_USE_RUNNING_QUEUE_CS
		CFlyWriteLock(*g_runningMapCS);
#endif
		g_runningMap.erase(user);
	}
	return qi->removeDownloadL(user);
}

void QueueManager::UserQueue::setQIPriority(const QueueItemPtr& qi, QueueItem::Priority p) // [!] IRainman fix.
{
	WLock(*QueueItem::g_cs); // [+] IRainamn fix.
	removeQueueItemL(qi, false);
	qi->setPriority(p); // TODO для установки приоритета - нужно удалить и снова добавить запись в контейнер? - это зовется очень часто
	addL(qi);
}

QueueItemPtr QueueManager::UserQueue::getRunningL(const UserPtr& aUser) // [!] IRainman fix.
{
#ifdef FLYLINKDC_USE_RUNNING_QUEUE_CS
	CFlyReadLock(*g_runningMapCS);
#endif
	const auto i = g_runningMap.find(aUser);
	return i == g_runningMap.cend() ? nullptr : i->second;
}

void QueueManager::UserQueue::removeQueueItemL(const QueueItemPtr& qi, bool p_is_remove_running) // [!] IRainman fix.
{
	const auto& s = qi->getSourcesL();
	for (auto i = s.cbegin(); i != s.cend(); ++i)
	{
		removeUserL(qi, i->first, p_is_remove_running,
		            false); // Не делаем лишний поиск в  qi->isSourceL(aUser)
	}
}

void QueueManager::UserQueue::removeUserL(const QueueItemPtr& qi, const UserPtr& aUser, bool p_is_remove_running, bool p_is_find_sources /*= true */)  // [!] IRainman fix.
{
	if (p_is_remove_running
	        && qi == getRunningL(aUser) // Нужен ли поиск дополнительный перед удалением?
	   )
	{
		removeDownloadL(qi, aUser); // [!] IRainman fix.
	}
	bool l_isSource = !p_is_find_sources;
	if (!l_isSource)
	{
		l_isSource = qi->isSourceL(aUser); // crash https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=78346
		if (!l_isSource)
		{
			const string l_error = "Error QueueManager::UserQueue::removeUserL [dcassert(isSource)] aUser = " +
			                       (aUser ? aUser->getLastNick() : string("null"));
			CFlyServerJSON::pushError(55, l_error);
			dcassert(l_isSource);
			return;
		}
	}
	{
	
#ifdef FLYLINKDC_USE_USER_QUEUE_CS
		Lock CFlyWriteLock(*g_userQueueMapCS);
#endif
		auto& ulm = g_userQueueMap[qi->getPriority()];
		const auto& j = ulm.find(aUser);
		if (j == ulm.cend())
		{
			const string l_error = "Error QueueManager::UserQueue::removeUserL [dcassert(j != ulm.cend())] aUser = " +
			                       (aUser ? aUser->getLastNick() : string("null"));
			CFlyServerJSON::pushError(55, l_error);
			dcassert(j != ulm.cend());
			return;
		}
		
		auto& uq = j->second;
		const auto& i = find(uq.begin(), uq.end(), qi);
		// TODO - перевести на set const auto& i = uq.find(qi);
		if (i == uq.cend())
		{
			const string l_error = "Error QueueManager::UserQueue::removeUserL [dcassert(i != uq.cend());] aUser = " +
			                       (aUser ? aUser->getLastNick() : string("null"));
			CFlyServerJSON::pushError(55, l_error);
			dcassert(i != uq.cend());
			return;
		}
#ifdef _DEBUG
		if (uq.size() > 5)
		{
			// LogManager::message("void QueueManager::UserQueue::removeUserL User = " + aUser->getLastNick() +
			//                    " uq.size = " + Util::toString(uq.size()));
		}
#endif
		
		uq.erase(i);
		if (uq.empty())
		{
			ulm.erase(j);
		}
	}
}
void QueueManager::Rechecker::execute(const string& p_file) // [!] IRainman core.
{
	QueueItemPtr q;
	int64_t tempSize;
	TTHValue tth;
	
	{
		q = QueueManager::FileQueue::find_target(p_file);
		if (!q || q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			return;
			
		qm->fly_fire1(QueueManagerListener::RecheckStarted(), q->getTarget());
		dcdebug("Rechecking %s\n", p_file.c_str());
		
		tempSize = File::getSize(q->getTempTarget());
		
		if (tempSize == -1)
		{
			qm->fly_fire1(QueueManagerListener::RecheckNoFile(), q->getTarget());
			// [+] IRainman fix.
			q->resetDownloaded();
			qm->rechecked(q);
			// [~] IRainman fix.
			return;
		}
		
		if (tempSize < 64 * 1024)
		{
			qm->fly_fire1(QueueManagerListener::RecheckFileTooSmall(), q->getTarget());
			// [+] IRainman fix.
			q->resetDownloaded();
			qm->rechecked(q);
			// [~] IRainman fix.
			return;
		}
		
		if (tempSize != q->getSize())
		{
			try
			{
				File(q->getTempTarget(), File::WRITE, File::OPEN).setSize(q->getSize());
			}
			catch (FileException& p_e)
			{
				LogManager::message("[Error] setSize - " + q->getTempTarget() + " Error=" + p_e.getError());
			}
		}
		
		if (q->isRunningL())
		{
			qm->fly_fire1(QueueManagerListener::RecheckDownloadsRunning(), q->getTarget());
			return;
		}
		
		tth = q->getTTH();
	}
	
	TigerTree tt;
	string l_tempTarget;
	
	{
		// [-] CFlyLock(qm->cs); [-] IRainman fix.
		
		// get q again in case it has been (re)moved
		q = QueueManager::FileQueue::find_target(p_file);
		if (!q)
			return;
		__int64 l_block_size;
		if (!CFlylinkDBManager::getInstance()->get_tree(tth, tt, l_block_size))
		{
			qm->fly_fire1(QueueManagerListener::RecheckNoTree(), q->getTarget());
			return;
		}
		
		//Clear segments
		q->resetDownloaded();
		
		l_tempTarget = q->getTempTarget();
	}
	
	//Merklecheck
	int64_t startPos = 0;
	DummyOutputStream dummy;
	int64_t blockSize = tt.getBlockSize();
	bool hasBadBlocks = false;
	vector<uint8_t> buf((size_t)min((int64_t)1024 * 1024, blockSize));
	vector< pair<int64_t, int64_t> > l_sizes;
	
	try // [+] IRainman fix.
	{
		File inFile(l_tempTarget, File::READ, File::OPEN);
		while (startPos < tempSize)
		{
			try
			{
				MerkleCheckOutputStream<TigerTree, false> l_check(tt, &dummy, startPos);
				
				inFile.setPos(startPos);
				int64_t bytesLeft = min((tempSize - startPos), blockSize); //Take care of the last incomplete block
				int64_t segmentSize = bytesLeft;
				while (bytesLeft > 0)
				{
					size_t n = (size_t)min((int64_t)buf.size(), bytesLeft);
					size_t nr = inFile.read(&buf[0], n);
					l_check.write(&buf[0], nr);
					bytesLeft -= nr;
					if (bytesLeft > 0 && nr == 0)
					{
						// Huh??
						throw Exception();
					}
				}
				l_check.flush();
				
				l_sizes.push_back(make_pair(startPos, segmentSize));
			}
			catch (const Exception&)
			{
				hasBadBlocks = true;
				dcdebug("Found bad block at " I64_FMT "\n", startPos);
			}
			startPos += blockSize;
		}
	}
	catch (const FileException&)
	{
		return;
	}
	
	// [-] CFlyLock(qm->cs); [-] IRainman fix. //[4] https://www.box.net/shared/4c41b1400336247cce1c
	
	// get q again in case it has been (re)moved
	q = QueueManager::FileQueue::find_target(p_file);
	
	if (!q)
		return;
		
	//If no bad blocks then the file probably got stuck in the temp folder for some reason
	if (!hasBadBlocks)
	{
		qm->moveStuckFile(q);
		return;
	}
	
	{
		WLock(*QueueItem::g_cs); // [+] IRainman fix.
		for (auto i = l_sizes.cbegin(); i != l_sizes.cend(); ++i)
		{
			q->addSegmentL(Segment(i->first, i->second));
		}
	}
	
	qm->rechecked(q);
}

// [+] brain-ripper
// avoid "'this' : used in base member initializer list" warning
#pragma warning(push)
#pragma warning(disable:4355)
QueueManager::QueueManager() :
	rechecker(this),
	nextSearch(0),
	m_is_exists_queueFile(true)
{
	TimerManager::getInstance()->addListener(this);
	SearchManager::getInstance()->addListener(this);
	ClientManager::getInstance()->addListener(this);
	
	File::ensureDirectory(Util::getListPath());
}
#pragma warning(pop)

QueueManager::~QueueManager() noexcept
{
	dcassert(m_fire_src_array.empty());
	dcassert(m_remove_target_array.empty());
	SearchManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	// [+] IRainman core.
	m_listMatcher.waitShutdown();
	m_listQueue.waitShutdown();
	waiter.waitShutdown();
	dclstLoader.waitShutdown();
	m_mover.waitShutdown();
	rechecker.waitShutdown();
	// [~] IRainman core.
	saveQueue();
	
	if (!BOOLSETTING(KEEP_LISTS))
	{
		std::sort(protectedFileLists.begin(), protectedFileLists.end());
		StringList filelists = File::findFiles(Util::getListPath(), "*.xml.bz2");
		if (!filelists.empty())
		{
			std::sort(filelists.begin(), filelists.end());
			std::for_each(filelists.begin(),
			std::set_difference(filelists.begin(), filelists.end(), protectedFileLists.begin(), protectedFileLists.end(), filelists.begin()),
			File::deleteFile);
		}
	}
	SharedFileStream::check_before_destoy();
}
void QueueManager::shutdown() // [+] IRainman opt.
{
	m_listMatcher.forceStop();
	m_listQueue.forceStop();
	waiter.forceStop();
	dclstLoader.forceStop();
	m_mover.forceStop();
	rechecker.forceStop();
}

struct PartsInfoReqParam
{
	PartsInfo   parts;
	string      tth;
	string      myNick;
	string      hubIpPort;
	boost::asio::ip::address_v4      ip;
	uint16_t    udpPort;
};

void QueueManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept
{

	string searchString;
	vector<const PartsInfoReqParam*> params;
#ifdef STRONG_USE_DHT
	TTHValue* tthPub = nullptr;
#endif
	
	{
		PFSSourceList sl;
		RLock(*QueueItem::g_cs); // fix http://code.google.com/p/flylinkdc/issues/detail?id=1236
		//find max 10 pfs sources to exchange parts
		//the source basis interval is 5 minutes
		g_fileQueue.findPFSSourcesL(sl);
		
		for (auto i = sl.cbegin(); i != sl.cend(); ++i)
		{
			QueueItem::PartialSource::Ptr source = i->first->second.getPartialSource();
			const QueueItemPtr& qi = i->second;
			
			PartsInfoReqParam* param = new PartsInfoReqParam;
			
			qi->getPartialInfoL(param->parts, qi->get_block_size_sql());
			
			param->tth = qi->getTTH().toBase32();
			param->ip  = source->getIp();
			param->udpPort = source->getUdpPort();
			param->myNick = source->getMyNick();
			param->hubIpPort = source->getHubIpPort();
			
			params.push_back(param);
			
			source->setPendingQueryCount(source->getPendingQueryCount() + 1);
			source->setNextQueryTime(aTick + 300000);       // 5 minutes
		}
	}
	// [!] IRainman fix: code below, always starts only thread TimerManager, and no needs to lock.
	
#ifdef STRONG_USE_DHT
	if (BOOLSETTING(USE_DHT) && SETTING(INCOMING_CONNECTIONS) != SettingsManager::INCOMING_FIREWALL_PASSIVE)
		tthPub = g_fileQueue.findPFSPubTTH();
#endif
		
	if (g_fileQueue.getSize() > 0 && aTick >= nextSearch && BOOLSETTING(AUTO_SEARCH))
	{
		// We keep 30 recent searches to avoid duplicate searches
		while (m_recent.size() >= g_fileQueue.getSize() || m_recent.size() > 30)
		{
			m_recent.pop_front();
		}
		
		QueueItemPtr qi;
		while ((qi = g_fileQueue.findAutoSearch(m_recent)) == nullptr && !m_recent.empty()) // Местами не переставлять findAutoSearch меняет recent
		{
			m_recent.pop_front();
		}
		if (qi)
		{
			searchString = qi->getTTH().toBase32();
			m_recent.push_back(qi->getTarget());
			//dcassert(SETTING(AUTO_SEARCH_TIME) > 1)
			nextSearch = aTick + SETTING(AUTO_SEARCH_TIME) * 60000;
			if (BOOLSETTING(REPORT_ALTERNATES))
			{
				LogManager::message(STRING(ALTERNATES_SEND) + ' ' + Util::getFileName(qi->getTargetFileName()) + " TTH = " + qi->getTTH().toBase32());
			}
		}
	}
	// [~] IRainman fix.
	
	// Request parts info from partial file sharing sources
	for (auto i = params.cbegin(); i != params.cend(); ++i)
	{
		const PartsInfoReqParam* param = *i;
		dcassert(param->udpPort > 0);
		
		try
		{
			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			SearchManager::toPSR(cmd, true, param->myNick, param->hubIpPort, param->tth, param->parts);
			Socket l_udp;
			l_udp.writeTo(param->ip.to_string(), param->udpPort, cmd.toString(ClientManager::getMyCID()));
			COMMAND_DEBUG("[Partial-Search]" + cmd.toString(ClientManager::getMyCID()), DebugTask::CLIENT_OUT, param->ip.to_string() + ':' + Util::toString(param->udpPort));
			LogManager::psr_message(
			    "[PartsInfoReq] Send UDP IP = " + param->ip.to_string() +
			    " param->udpPort = " + Util::toString(param->udpPort) +
			    " cmd = " + cmd.toString(ClientManager::getMyCID())
			);
		}
		catch (Exception& e)
		{
			dcdebug("Partial search caught error\n");
			LogManager::psr_message(
			    "[Partial search caught error] Error = " + e.getError() +
			    " IP = " + param->ip.to_string() +
			    " param->udpPort = " + Util::toString(param->udpPort)
			);
		}
		
		delete param;
	}
#ifdef STRONG_USE_DHT
	// DHT PFS announce
	if (tthPub)
	{
		dht::IndexManager::publishPartialFile(*tthPub);
		delete tthPub;
	}
#endif
	
	if (!searchString.empty())
	{
		SearchManager::getInstance()->search_auto(searchString);
	}
}
// TODO HintedUser
void QueueManager::addList(const UserPtr& aUser, Flags::MaskType aFlags, const string& aInitialDir /* = Util::emptyString */)
{
	add(0, aInitialDir, -1, TTHValue(), aUser, (Flags::MaskType)(QueueItem::FLAG_USER_LIST | aFlags));
}

void QueueManager::DclstLoader::execute(const string& p_currentDclstFile) // [+] IRainman dclst support.
{
	const string l_dclstFilePath = Util::getFilePath(p_currentDclstFile);
	unique_ptr<DirectoryListing> dl(new DirectoryListing(HintedUser()));
	dl->loadFile(p_currentDclstFile);
	
	dl->download(dl->getRoot(), l_dclstFilePath, false, QueueItem::DEFAULT, 0);
	
	if (!dl->getIncludeSelf())
	{
		File::deleteFile(p_currentDclstFile);
	}
}

string QueueManager::getListPath(const UserPtr& user) const
{
	dcassert(user); // [!] IRainman fix: Unable to load the file list with an empty user!
	
	StringList nicks = ClientManager::getNicks(user->getCID(), Util::emptyString);
	string nick = nicks.empty() ? Util::emptyString : Util::cleanPathChars(nicks[0]) + ".";
	return checkTarget(Util::getListPath() + nick + user->getCID().toBase32(), -1);// [!] IRainman fix. FlylinkDC use Size on 2nd parametr!
}

void QueueManager::get_download_connection(const UserPtr& aUser)
{
	if (!ClientManager::isShutdown() && ConnectionManager::isValidInstance())
	{
		ConnectionManager::getInstance()->getDownloadConnection(aUser);
	}
}
void QueueManager::addFromWebServer(const string& aTarget, int64_t aSize, const TTHValue& aRoot)
{
	const auto l_old_value = SETTING(ON_DOWNLOAD_SETTING);
	try
	{
		SET_SETTING(ON_DOWNLOAD_SETTING, SettingsManager::ON_DOWNLOAD_RENAME);
		add(0, aTarget, aSize, aRoot, HintedUser());
		SET_SETTING(ON_DOWNLOAD_SETTING, l_old_value);
	}
	catch (Exception&)
	{
		SET_SETTING(ON_DOWNLOAD_SETTING, l_old_value);
		throw;
	}
}
void QueueManager::add(int64_t p_FlyQueueID, const string& aTarget, int64_t aSize, const TTHValue& aRoot, const UserPtr& aUser,
                       Flags::MaskType aFlags /* = 0 */, bool addBad /* = true */, bool p_first_file /*= true*/)
{
	// Check that we're not downloading from ourselves...
	if (aUser && // [+] https://www.crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=18543
	        ClientManager::isMe(aUser))
	{
		throw QueueException(STRING(NO_DOWNLOADS_FROM_SELF));
	}
	
	// Check if we're not downloading something already in our share
//[-]PPA
	/*
	    if (BOOLSETTING(DONT_DL_ALREADY_SHARED)){
	        if (ShareManager::isTTHShared(root)){
	            throw QueueException(STRING(TTH_ALREADY_SHARED));
	        }
	    }
	*/
	
	const bool l_userList = (aFlags & QueueItem::FLAG_USER_LIST) == QueueItem::FLAG_USER_LIST;
	const bool l_testIP = (aFlags & QueueItem::FLAG_USER_GET_IP) == QueueItem::FLAG_USER_GET_IP;
	const bool l_newItem = !(l_testIP || l_userList);
	
	string l_target;
	string l_tempTarget;
	
	if (l_userList)
	{
		dcassert(aUser); // [!] IRainman fix: You can not add to list download an empty user.
		l_target = getListPath(aUser);
		l_tempTarget = aTarget;
	}
	else if (l_testIP)
	{
		dcassert(aUser); // [!] IRainman fix: You can not add to check download an empty user.
		l_target = getListPath(aUser) + ".check";
		l_tempTarget = aTarget;
	}
	else
	{
		//+SMT, BugMaster: ability to use absolute file path
		if (File::isAbsolute(aTarget))
			l_target = aTarget;
		else
			l_target = FavoriteManager::getDownloadDirectory(Util::getFileExt(aTarget)) + aTarget;//[!] IRainman support download to specify extension dir.
		//target = SETTING(DOWNLOAD_DIRECTORY) + aTarget;
		//-SMT, BugMaster: ability to use absolute file path
		l_target = checkTarget(l_target, -1); // [!] IRainman fix. FlylinkDC use Size on 2nd parametr!
		// TODO - checkTarget тяжелая функция зовет
		// 1. string target = Util::validateFileName(aTarget); [!] IRainman - вряд ли это можно как то оптимизировать - проверять корректность символов в путях необходимо,  однако можно засунуть весь вызов одновременной загрузки кучи файлов в отдельный поток ;)
		// 2. int64_t sz = File::getSize(target); [!] IRainman - исправлено.
		// Если качаем каталог файлов из 10-70 тыс. на каждом выполняется такая проверка в моментм помещения в очередь.
		// Оптимизнуть к r502
	}
	
	// Check if it's a zero-byte file, if so, create and return...
	if (aSize == 0)
	{
		if (!BOOLSETTING(SKIP_ZERO_BYTE))
		{
			File::ensureDirectory(l_target);
			File f(l_target, File::WRITE, File::CREATE);
		}
		return;
	}
	
	bool l_wantConnection;
	
	{
		// [-] CFlyLock(cs); [-] IRainman fix.
		
		QueueItemPtr q = QueueManager::FileQueue::find_target(l_target);
		// По TTH искать нельзя
		// Проблема описана тут http://www.flylinkdc.ru/2014/04/flylinkdc-strongdc-tth.html
#if 0
		if (q == nullptr &&
		        l_newItem &&
		        (BOOLSETTING(ENABLE_MULTI_CHUNK) && aSize > SETTING(MIN_MULTI_CHUNK_SIZE) * 1024 * 1024) // [+] IRainman size in MB.
		   )
		{
			// q = QueueManager::FileQueue::findQueueItem(aRoot);
		}
#endif
		if (!q)
		{
			// [+] SSA - check file exist
			if (l_newItem)
			{
				/* FLylinkDC Team TODO: IRainman: давайте копировать имеющийся файл в папку назначения? будем трафик экономить! :)
				Необходима доработка диалога "что делать с уже имеющимся файлом", внутренности сам доделаю, в том числе и перепроверку имеющегося файла при замене. L.
				хорошо бы запилить к r502.
				string l_shareExistingPath;
				int64_t l_shareExistingSize;
				ShareManager::getRealPathAndSize(root, l_shareExistingPath, l_shareExistingSize);
				getTargetByRoot(...); - тоже можно использовать.
				*/
				int64_t l_existingFileSize;
				time_t l_eistingFileTime;
				bool l_is_link;
				if (File::isExist(l_target, l_existingFileSize, l_eistingFileTime, l_is_link))
				{
					m_curOnDownloadSettings = SETTING(ON_DOWNLOAD_SETTING);
					if (m_curOnDownloadSettings == SettingsManager::ON_DOWNLOAD_REPLACE && BOOLSETTING(KEEP_FINISHED_FILES_OPTION))
						m_curOnDownloadSettings = SettingsManager::ON_DOWNLOAD_ASK;
						
					if (m_curOnDownloadSettings == SettingsManager::ON_DOWNLOAD_ASK)
					{
						fly_fire5(QueueManagerListener::TryAdding(), l_target, aSize, l_existingFileSize, l_eistingFileTime, m_curOnDownloadSettings);
					}
					
					switch (m_curOnDownloadSettings)
					{
							/* FLylinkDC Team TODO: IRainman: давайте копировать имеющийся файл в папку назначения? будем трафик экономить! p.s: см. выше. :)
							case SettingsManager::ON_DOWNLOAD_EXIST_FILE_TO_NEW_DEST:
							    ...
							    return;
							*/
						case SettingsManager::ON_DOWNLOAD_REPLACE:
							File::deleteFile(l_target); // Delete old file.  FlylinkDC Team TODO: recheck existing file to save traffic and download time.
							break;
						case SettingsManager::ON_DOWNLOAD_RENAME:
							l_target = Util::getFilenameForRenaming(l_target); // for safest: call Util::getFilenameForRenaming twice from gui and core.
							break;
						case SettingsManager::ON_DOWNLOAD_SKIP:
							return;
					}
				}
			}
			// [~] SSA - check file exist
			q = g_fileQueue.add(p_FlyQueueID, l_target, aSize, aFlags, QueueItem::DEFAULT, l_tempTarget, GET_TIME(), aRoot, 1);
			if (q)
			{
				fly_fire1(QueueManagerListener::Added(), q);
			}
		}
		else
		{
			if (q->getSize() != aSize)
			{
				throw QueueException(STRING(FILE_WITH_DIFFERENT_SIZE)); // [!] IRainman fix done: [4] https://www.box.net/shared/0ac062dcc56424091537
			}
			if (!(aRoot == q->getTTH()))
			{
				throw QueueException(STRING(FILE_WITH_DIFFERENT_TTH));
			}
			
			// [!] IRainman fix.
			bool isFinished;
			{
				RLock(*QueueItem::g_cs); // [+]
				isFinished = q->isFinishedL();
			}
			if (isFinished)
			{
				throw QueueException(STRING(FILE_ALREADY_FINISHED));
			}
			// [~] IRainman fix.
			
			q->setFlag(aFlags);
		}
		if (aUser && q)
		{
			WLock(*QueueItem::g_cs); // [+] IRainman fix.
			l_wantConnection = addSourceL(q, aUser, (Flags::MaskType)(addBad ? QueueItem::Source::FLAG_MASK : 0));
		}
		else
		{
			l_wantConnection = false;
		}
		setDirty();
	}
	
	if (p_first_file)
	{
		//[!] FlylinkDC++ Team: выполняем вызов getDownloadConnection только на первом файле первого каталога при скачке каталога с одного юзера.
		//[!] FlylinkDC++ Team: исправлено зависание при скачивании каталогов с кол-вом файлов > 10-100 тыс.... + Может тормозить когда качается много каталогов.
		
		if (l_wantConnection && aUser->isOnline())
		{
			get_download_connection(aUser);
		}
		
		// auto search, prevent DEADLOCK
		if (l_newItem && //[+] FlylinkDC++ Team: Если файл-лист, или запрос IP - то его не нужно искать.
		        BOOLSETTING(AUTO_SEARCH))
		{
			SearchManager::getInstance()->search_auto(aRoot.toBase32());
		}
	}
}

void QueueManager::readdAll(const QueueItemPtr& q)
{
	QueueItem::SourceMap l_badSources;
	{
		WLock(*QueueItem::g_cs);
		l_badSources = q->getBadSourcesL(); // fix https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=62702
	}
	for (auto s = l_badSources.cbegin(); s != l_badSources.cend(); ++s)
	{
		readd(q->getTarget(), s->first);
	}
}

void QueueManager::readd(const string& p_target, const UserPtr& aUser)
{
	bool wantConnection = false;
	{
		const QueueItemPtr q = QueueManager::FileQueue::find_target(p_target);
		WLock(*QueueItem::g_cs); // [+] IRainman fix.
		if (q && q->isBadSourceL(aUser))
		{
			wantConnection = addSourceL(q, aUser, QueueItem::Source::FLAG_MASK);
		}
	}
	if (wantConnection && aUser->isOnline())
	{
		get_download_connection(aUser);
	}
}

void QueueManager::setDirty()
{
	if (!g_dirty)
	{
		g_dirty = true;
		g_lastSave = GET_TICK();
	}
}

string QueueManager::checkTarget(const string& aTarget, const int64_t aSize, bool p_is_validation_path /* = true*/)
{
#ifdef _WIN32
	if (aTarget.length() > FULL_MAX_PATH)
	{
		throw QueueException(STRING(TARGET_FILENAME_TOO_LONG));
	}
	// Check that target starts with a drive or is an UNC path
	if ((aTarget[1] != ':' || aTarget[2] != '\\') &&
	        (aTarget[0] != '\\' && aTarget[1] != '\\'))
	{
		throw QueueException(STRING(INVALID_TARGET_FILE));
	}
#else
	if (aTarget.length() > PATH_MAX)
	{
		throw QueueException(STRING(TARGET_FILENAME_TOO_LONG));
	}
	// Check that target contains at least one directory...we don't want headless files...
	if (aTarget[0] != '/')
	{
		throw QueueException(STRING(INVALID_TARGET_FILE));
	}
#endif
	
	const string l_target = p_is_validation_path ? Util::validateFileName(aTarget) : aTarget;
	
	if (aSize != -1) // [!] IRainman opt.
	{
		// Check that the file doesn't already exist...
		//[!] FlylinkDC always checking file size
		const int64_t sz = File::getSize(l_target);
		if (aSize <= sz) //[+] FlylinkDC && (aSize <= sz)
		{
			throw FileException(STRING(LARGER_TARGET_FILE_EXISTS));
		}
	}
	return l_target;
}

/** Add a source to an existing queue item */
bool QueueManager::addSourceL(const QueueItemPtr& qi, const UserPtr& aUser, Flags::MaskType addBad, bool p_is_first_load)
{
	dcassert(aUser); // [!] IRainman fix: Unable to add a source if the user is empty! Check your code!
	bool wantConnection;
	{
		// [-] WLock(*QueueItem::g_cs); // [-] IRainman fix.
		
		//dcassert(p_is_first_load == true && !userQueue.getRunningL(aUser) || p_is_first_load == false);
		if (p_is_first_load)
		{
			wantConnection = true;
		}
		else
		{
			wantConnection = (qi->getPriority() != QueueItem::PAUSED)
			                 && !g_userQueue.getRunningL(aUser);
		}
		
		// TODO - LOG dcassert(p_is_first_load == true && !qi->isSourceL(aUser) || p_is_first_load == false);
		if (qi->isSourceL(aUser))
		{
			if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			{
				return wantConnection;
			}
			throw QueueException(STRING(DUPLICATE_SOURCE) + ": " + Util::getFileName(qi->getTarget()));
		}
		dcassert(p_is_first_load == true && !qi->isBadSourceExceptL(aUser, addBad) || p_is_first_load == false);
		if (qi->isBadSourceExceptL(aUser, addBad))
		{
			throw QueueException(STRING(DUPLICATE_SOURCE) +
			                     " TTH = " + Util::getFileName(qi->getTarget()) +
			                     " Nick = " + aUser->getLastNick() +
			                     " id = " + Util::toString(qi->getFlyQueueID()));
		}
		
		qi->addSourceL(aUser, p_is_first_load);
		/*if(aUser.user->isSet(User::PASSIVE) && !ClientManager::isActive(aUser.hint)) {
		    qi->removeSource(aUser, QueueItem::Source::FLAG_PASSIVE);
		    wantConnection = false;
		} else */
		if (p_is_first_load == false && qi->isFinishedL())
		{
			wantConnection = false;
		}
		else
		{
			if (p_is_first_load == false)
			{
				if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP)) // http://code.google.com/p/flylinkdc/issues/detail?id=1088
				{
					PLAY_SOUND(SOUND_SOURCEFILE);
				}
			}
			g_userQueue.addL(qi, aUser, p_is_first_load);
		}
	}
	if (p_is_first_load == false)
	{
		QueueManager::getInstance()->fire_sources_updated(qi);
		setDirty();
	}
	return wantConnection;
}

void QueueManager::addDirectory(const string& aDir, const UserPtr& aUser, const string& aTarget, QueueItem::Priority p /* = QueueItem::DEFAULT */) noexcept
{
	bool needList;
	{
		CFlyFastLock(csDirectories);
		
		const auto dp = m_directories.equal_range(aUser);
		
		for (auto i = dp.first; i != dp.second; ++i)
		{
			if (stricmp(aTarget.c_str(), i->second->getName().c_str()) == 0)
				return;
		}
		
		// Unique directory, fine...
		m_directories.insert(make_pair(aUser, new DirectoryItem(aUser, aDir, aTarget, p)));
		needList = (dp.first == dp.second);
	}
	
	setDirty();
	
	if (needList)
	{
		try
		{
			addList(aUser, QueueItem::FLAG_DIRECTORY_DOWNLOAD | QueueItem::FLAG_PARTIAL_LIST, aDir);
		}
		catch (const Exception&)
		{
			// Ignore, we don't really care...
		}
	}
}

QueueItem::Priority QueueManager::hasDownload(const UserPtr& aUser)
{
	RLock(*QueueItem::g_cs);
	QueueItemPtr qi = g_userQueue.getNextL(aUser, QueueItem::LOWEST);
	if (!qi)
	{
		return QueueItem::PAUSED;
	}
	return qi->getPriority();
}
// *** WARNING ***
// Lock(cs) makes sure that there's only one thread accessing this
// [-] static TTHMap tthMap; [-] IRainman fix.

void QueueManager::buildMap(const DirectoryListing::Directory* dir, TTHMap& tthMap) noexcept // [!] IRainman fix.
{
	for (auto j = dir->directories.cbegin(); j != dir->directories.cend(); ++j)
	{
		if (!(*j)->getAdls()) // [1] https://www.box.net/shared/d511d114cb87f7fa5b8d
		{
			buildMap(*j, tthMap); // [!] IRainman fix.
		}
	}
	
	for (auto i = dir->files.cbegin(); i != dir->files.cend(); ++i)
	{
		const DirectoryListing::File* df = *i;
		tthMap.insert(make_pair(df->getTTH(), df));
	}
}

int QueueManager::matchListing(const DirectoryListing& dl) noexcept
{
	dcassert(dl.getUser()); // [!] IRainman fix: It makes no sense to check the file list on the presence of a file from our download queue if the user in the file list is empty!
	
	int matches = 0;
	
	// [!] IRainman fix.
	if (!g_fileQueue.empty()) // [!] opt
	{
		// [-] CFlyLock(cs); [-]
		TTHMap l_tthMap;// tthMap.clear(); [!]
		buildMap(dl.getRoot(), l_tthMap); // [!]
		dcassert(!l_tthMap.empty());
		{
			WLock(*QueueItem::g_cs);
			{
				RLock(*FileQueue::g_csFQ);
				// [~] IRainman fix.
				for (auto i = g_fileQueue.getQueueL().cbegin(); i != g_fileQueue.getQueueL().cend(); ++i)
				{
					const QueueItemPtr& qi = i->second;
					if (qi->isFinishedL())
						continue;
					if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
						continue;
					const auto j = l_tthMap.find(qi->getTTH());
					if (j != l_tthMap.end() && j->second->getSize() == qi->getSize()) // [!] IRainman fix.
					{
						try
						{
							addSourceL(qi, dl.getUser(), QueueItem::Source::FLAG_FILE_NOT_AVAILABLE);
							matches++;
						}
						catch (const Exception&)
						{
							// Ignore...
						}
					}
				}
			}
		}
		if (matches > 0)
		{
			get_download_connection(dl.getUser());
		}
	}
	return matches;
}

void QueueManager::FileListQueue::execute(const DirectoryListInfoPtr& list) // [+] IRainman fix: moved form MainFrame to core.
{
	if (File::isExist(list->file))
	{
		unique_ptr<DirectoryListing> dl(new DirectoryListing(list->m_hintedUser));
		try
		{
			dl->loadFile(list->file);
			ADLSearchManager::getInstance()->matchListing(*dl);
			ClientManager::checkCheating(list->m_hintedUser, dl.get());
		}
		catch (const Exception& e)
		{
			LogManager::message(e.getError());
		}
	}
	delete list;
}

void QueueManager::ListMatcher::execute(const StringList& list) // [+] IRainman fix: moved form MainFrame to core.
{
	for (auto i = list.cbegin(); i != list.cend(); ++i)
	{
		UserPtr u = DirectoryListing::getUserFromFilename(*i);
		if (!u)
			continue;
			
		DirectoryListing dl(HintedUser(u, Util::emptyString));
		try
		{
			dl.loadFile(*i);
			dl.logMatchedFiles(u, QueueManager::getInstance()->matchListing(dl));
		}
		catch (const Exception&)
		{
			//-V565
		}
	}
}

void QueueManager::QueueManagerWaiter::execute(const WaiterFile& p_currentFile) // [+] IRainman: auto pausing running downloads before moving.
{
	const string& l_source = p_currentFile.getSource();
	const string& l_target = p_currentFile.getTarget();
	const QueueItem::Priority l_priority = p_currentFile.getPriority();
	
	auto qm = QueueManager::getInstance();
	qm->setAutoPriority(l_source, false); // [+] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1030
	qm->setPriority(l_source, QueueItem::PAUSED);
	const QueueItemPtr qi = QueueManager::FileQueue::find_target(l_source);
	dcassert(qi->getPriority() == QueueItem::PAUSED);
	while (qi)
	{
		if (qi->isRunningL())
		{
			sleep(1000);
		}
		else
		{
			qm->move(l_source, l_target);
			qm->setPriority(l_target, l_priority);
		}
	}
}

void QueueManager::move(const string& aSource, const string& aTarget) noexcept
{
	const string l_target = Util::validateFileName(aTarget);
	if (aSource == l_target)
		return;
		
	// [+] IRainman: auto pausing running downloads before moving.
	const QueueItemPtr qs = QueueManager::FileQueue::find_target(aSource);
	if (!qs)
		return;
		
	// Don't move file lists
	if (qs->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
		return;
		
	// Don't move running downloads
	if (qs->isRunningL())
	{
		waiter.move(aSource, l_target, qs->getPriority());
		return;
	}
	// [~] IRainman: auto pausing running downloads before moving.
	
	// [-] IRainman fix.
	// [-] bool delSource = false;
	
	// [-] CFlyLock(cs);
	// [-] QueueItemPtr qs = fileQueue.find(aSource);
	// [-] if (qs)
	{
		// Don't move running downloads
		// [-] if (qs->isRunning())
		// [-] {
		// [-]  return;
		// [-] }
		// Don't move file lists
		// [-] if (qs->isSet(QueueItem::FLAG_USER_LIST))
		// [-]  return;
		
		// Let's see if the target exists...then things get complicated...
		const QueueItemPtr qt = QueueManager::FileQueue::find_target(l_target);
		if (!qt || stricmp(aSource, l_target) == 0)
		{
			// Good, update the target and move in the queue...
			fly_fire2(QueueManagerListener::Moved(), qs, aSource);
			g_fileQueue.moveTarget(qs, l_target);
			//fly_fire1(QueueManagerListener::Added(), qs);// [-]IRainman
			setDirty();
		}
		else
		{
			// Don't move to target of different size
			if (qs->getSize() != qt->getSize() || qs->getTTH() != qt->getTTH())
				return; // TODO спросить юзера!
				
			{
				WLock(*QueueItem::g_cs); // [+] IRainman fix.
				for (auto i = qs->getSourcesL().cbegin(); i != qs->getSourcesL().cend(); ++i)
				{
					try
					{
						addSourceL(qt, i->first, QueueItem::Source::FLAG_MASK);
					}
					catch (const Exception&)
					{
					}
				}
			}
			// [-] delSource = true;
			removeTarget(aSource, false);
			// [~]
		}
	}
	
	// [-] if (delSource)
	// [-] {
	// [-]  remove(aSource);
	// [-] }
}

bool QueueManager::getQueueInfo(const UserPtr& aUser, string& aTarget, int64_t& aSize, int& aFlags) noexcept
{
	WLock(*QueueItem::g_cs); // [!] IRainman fix.
	const QueueItemPtr qi = g_userQueue.getNextL(aUser);
	if (!qi)
		return false;
		
	aTarget = qi->getTarget();
	aSize = qi->getSize();
	aFlags = qi->getFlags();
	
	return true;
}

uint8_t QueueManager::FileQueue::getMaxSegments(const uint64_t filesize)
{
	if (BOOLSETTING(SEGMENTS_MANUAL))
		return min((uint8_t)SETTING(NUMBER_OF_SEGMENTS), (uint8_t)200);
	else
		return min(filesize / (50 * MIN_BLOCK_SIZE) + 2, 200Ui64);
}

void QueueManager::getTargets(const TTHValue& tth, StringList& sl)
{
	// [-] CFlyLock(cs); [-] IRainman fix.
	QueueItemList l_ql;
	g_fileQueue.find_tth(l_ql, tth);
	for (auto i = l_ql.cbegin(); i != l_ql.cend(); ++i)
	{
		sl.push_back((*i)->getTarget());
	}
}

DownloadPtr QueueManager::getDownload(UserConnection* aSource, string& aMessage) noexcept
{
	const auto l_ip = aSource->getRemoteIp();
	const auto l_chiper_name = aSource->getCipherName();
	const UserPtr u = aSource->getUser();
	dcdebug("Getting download for %s...", u->getCID().toBase32().c_str());
	QueueItemPtr q;
	DownloadPtr d;
	{
		WLock(*QueueItem::g_cs);
		
		q = g_userQueue.getNextL(u, QueueItem::LOWEST, aSource->getChunkSize(), aSource->getSpeed(), true);
		
		if (!q)
		{
			aMessage = g_userQueue.getLastError();
			dcdebug("none\n");
			return d;
		}
		
		// Check that the file we will be downloading to exists
		if (q->calcAverageSpeedAndCalcAndGetDownloadedBytesL() > 0)
		{
			if (!File::isExist(q->getTempTarget()))
			{
				// Temp target gone?
				q->resetDownloaded();
			}
		}
	}
	
	// Нельзя звать new Download под локом QueueItem::g_cs
	d = DownloadPtr(new Download(aSource, q, l_ip, l_chiper_name));
	aSource->setDownload(d);
	
	{
		WLock(*QueueItem::g_cs);
		g_userQueue.addDownloadL(q, d);
	}
	
	fire_sources_updated(q);
	dcdebug("found %s\n", q->getTarget().c_str());
	return d;
}

namespace
{
class TreeOutputStream : public OutputStream
{
	public:
		explicit TreeOutputStream(TigerTree& aTree) : tree(aTree), bufPos(0)
		{
		}
		
		size_t write(const void* xbuf, size_t len)
		{
			size_t pos = 0;
			uint8_t* b = (uint8_t*)xbuf;
			while (pos < len)
			{
				size_t left = len - pos;
				if (bufPos == 0 && left >= TigerTree::BYTES)
				{
					tree.getLeaves().push_back(TTHValue(b + pos));
					pos += TigerTree::BYTES;
				}
				else
				{
					size_t bytes = min(TigerTree::BYTES - bufPos, left);
					memcpy(buf + bufPos, b + pos, bytes);
					bufPos += bytes;
					pos += bytes;
					if (bufPos == TigerTree::BYTES)
					{
						tree.getLeaves().push_back(TTHValue(buf));
						bufPos = 0;
					}
				}
			}
			return len;
		}
		
		size_t flush()
		{
			return 0;
		}
	private:
		TigerTree& tree;
		uint8_t buf[TigerTree::BYTES];
		size_t bufPos;
};

}

void QueueManager::setFile(const DownloadPtr& d)
{
	if (d->getType() == Transfer::TYPE_FILE)
	{
		const QueueItemPtr qi = QueueManager::FileQueue::find_target(d->getPath());
		if (!qi)
		{
			throw QueueException(STRING(TARGET_REMOVED));
		}
		
		if (d->getOverlapped())
		{
			d->setOverlapped(false);
			
			bool found = false;
			// ok, we got a fast slot, so it's possible to disconnect original user now
			RLock(*QueueItem::g_cs); // [+] IRainman fix.
			for (auto i = qi->getDownloadsL().cbegin(); i != qi->getDownloadsL().cend(); ++i)
			{
				const auto& j = i->second;
				if (j != d && j->getSegment().contains(d->getSegment()))
				{
				
					// overlapping has no sense if segment is going to finish
					if (j->getSecondsLeft() < 10)
						break;
						
					found = true;
					
					// disconnect slow chunk
					j->getUserConnection()->disconnect();
					break;
				}
			}
			
			if (!found)
			{
				// slow chunk already finished ???
				throw QueueException(STRING(DOWNLOAD_FINISHED_IDLE));
			}
		}
		
		const string l_target = d->getDownloadTarget();
		
		if (qi->getDownloadedBytes() > 0)
		{
			if (!File::isExist(qi->getTempTarget()))
			{
				// When trying the download the next time, the resume pos will be reset
				throw QueueException(STRING(TARGET_REMOVED));
			}
		}
		else
		{
			File::ensureDirectory(l_target);
		}
		
		// open stream for both writing and reading, because UploadManager can request reading from it
		auto f = new SharedFileStream(l_target, File::RW, File::OPEN | File::CREATE | File::SHARED | File::NO_CACHE_HINT);
		
		// Only use antifrag if we don't have a previous non-antifrag part
		if (BOOLSETTING(ANTI_FRAG) && f->getSize() != qi->getSize())
		{
			f->setSize(d->getTigerTree().getFileSize());
		}
		
		f->setPos(d->getSegment().getStart());
		d->setDownloadFile(f);
	}
	else if (d->getType() == Transfer::TYPE_FULL_LIST)
	{
		{
			// [-] CFlyLock(cs); [-] IRainman fix.
			
			QueueItemPtr qi = QueueManager::FileQueue::find_target(d->getPath());
			if (!qi)
			{
				throw QueueException(STRING(TARGET_REMOVED));
			}
			
			// set filelist's size
			qi->setSize(d->getSize());
		}
		
		string l_target = d->getPath();
		File::ensureDirectory(l_target);
		
		if (d->isSet(Download::FLAG_XML_BZ_LIST))
		{
			l_target += ".xml.bz2";
		}
		else
		{
			l_target += ".xml";
		}
		d->setDownloadFile(new File(l_target, File::WRITE, File::OPEN | File::TRUNCATE | File::CREATE));
	}
	else if (d->getType() == Transfer::TYPE_PARTIAL_LIST)
	{
		// [!] IRainman fix. TODO
		d->setDownloadFile(new StringOutputStream(d->getPFS()));
		// d->setFile(new File(d->getPFS(), File::WRITE, File::OPEN | File::TRUNCATE | File::CREATE));
		// [~] IRainman fix
	}
	else if (d->getType() == Transfer::TYPE_TREE)
	{
		d->setDownloadFile(new TreeOutputStream(d->getTigerTree()));
	}
}

void QueueManager::moveFile(const string& source, const string& p_target)
{
	File::ensureDirectory(p_target);
	if (File::getSize(source) > MOVER_LIMIT)
	{
		m_mover.moveFile(source, p_target);
	}
	else
	{
		internalMoveFile(source, p_target);
	}
}

void QueueManager::internalMoveFile(const string& source, const string& p_target)
{
	try
	{
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		getInstance()->fly_fire1(QueueManagerListener::TryFileMoving(), p_target);
#endif
		File::renameFile(source, p_target);
		getInstance()->fly_fire1(QueueManagerListener::FileMoved(), p_target);
	}
	catch (const FileException& /*e1*/)
	{
		// Try to just rename it to the correct name at least
		const string newTarget = Util::getFilePath(source) + Util::getFileName(p_target);
		try
		{
			File::renameFile(source, newTarget);
			LogManager::message(source + ' ' + STRING(RENAMED_TO) + ' ' + newTarget);
		}
		catch (const FileException& e2)
		{
			LogManager::message(STRING(UNABLE_TO_RENAME) + ' ' + source + " -> NewTarget: " + newTarget + " Error = " + e2.getError());
		}
	}
}

void QueueManager::moveStuckFile(const QueueItemPtr& qi)
{
	moveFile(qi->getTempTarget(), qi->getTarget());
	
	{
		WLock(*QueueItem::g_cs); // [+] IRainman fix.
		if (qi->isFinishedL())
		{
			g_userQueue.removeQueueItemL(qi);
		}
	}
	
	const string l_target = qi->getTarget();
	
	if (!BOOLSETTING(KEEP_FINISHED_FILES_OPTION))
	{
		fire_remove_internal(qi, false, false, false);
	}
	else
	{
		{
			WLock(*QueueItem::g_cs); // [+] IRainman fix.
			qi->addSegmentL(Segment(0, qi->getSize()));
		}
		fire_status_updated(qi);
	}
	if (!ClientManager::isShutdown())
	{
		fly_fire1(QueueManagerListener::RecheckAlreadyFinished(), l_target);
	}
}
void QueueManager::fire_sources_updated(const QueueItemPtr& qi)
{
	//dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		CFlyLock(m_cs_fire_src);
		m_fire_src_array.insert(qi->getTarget());
		// TODO - посчитать сколько позиций экономим
	}
}
void QueueManager::fire_removed_array(const StringList& p_target_array)
{
	if (!ClientManager::isShutdown())
	{
		fly_fire1(QueueManagerListener::RemovedArray(), p_target_array);
	}
}
void QueueManager::fire_removed(const QueueItemPtr& qi)
{
	if (!ClientManager::isShutdown())
	{
		fly_fire1(QueueManagerListener::Removed(), qi);
	}
}
void QueueManager::fire_status_updated(const QueueItemPtr& qi)
{
	//dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		fly_fire1(QueueManagerListener::StatusUpdated(), qi);
	}
}
void QueueManager::rechecked(const QueueItemPtr& qi)
{
	fly_fire1(QueueManagerListener::RecheckDone(), qi->getTarget());
	fire_status_updated(qi);
	
	setDirty();
}

void QueueManager::putDownload(const string& p_path, DownloadPtr aDownload, bool p_is_finished, bool p_is_report_finish) noexcept
{
#if 0
	dcassert(!ClientManager::isShutdown());
	// fix https://drdump.com/Problem.aspx?ProblemID=112136
	if (!ClientManager::isShutdown())
	{
		// TODO - check and delete aDownload?
		// return;
	}
#endif
	UserList getConn;
	string fileName;
	const HintedUser l_hintedUser = aDownload->getHintedUser(); // crash https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=155631
	UserPtr l_user = aDownload->getUser();
	
	dcassert(l_user); // [!] IRainman fix: putDownload call with empty by the user can not because you can not even attempt to download with an empty user!
	
	Flags::MaskType flags = 0;
	bool downloadList = false;
	
	{
		aDownload->reset_download_file();  // https://drdump.com/Problem.aspx?ProblemID=130529
		
		if (aDownload->getType() == Transfer::TYPE_PARTIAL_LIST)
		{
			QueueItemPtr q = QueueManager::FileQueue::find_target(p_path);
			if (q)
			{
				//q->setFailed(!aDownload->m_reason.empty());
				
				if (!aDownload->getPFS().empty())
				{
					// [!] IRainman fix.
					bool fulldir = q->isSet(QueueItem::FLAG_MATCH_QUEUE);
					if (!fulldir)
					{
						fulldir = q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD);
						if (fulldir)
						{
							CFlyFastLock(csDirectories);
							fulldir &= m_directories.find(aDownload->getUser()) != m_directories.end();
						}
					}
					if (fulldir)
						// [~] IRainman fix.
					{
						dcassert(p_is_finished);
						fileName = aDownload->getPFS();
						flags = (q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD) ? QueueItem::FLAG_DIRECTORY_DOWNLOAD : 0)
						        | (q->isSet(QueueItem::FLAG_MATCH_QUEUE) ? QueueItem::FLAG_MATCH_QUEUE : 0)
						        | QueueItem::FLAG_TEXT;
					}
					else
					{
						fly_fire2(QueueManagerListener::PartialList(), l_hintedUser, aDownload->getPFS());
					}
				}
				else
				{
					// partial filelist probably failed, redownload full list
					dcassert(!p_is_finished);
					
					downloadList = true;
					flags = q->getFlags() & ~QueueItem::FLAG_PARTIAL_LIST;
				}
				
				fire_remove_internal(q, true, true, false);
			}
		}
		else
		{
			QueueItemPtr q = QueueManager::FileQueue::find_target(p_path);
			if (q)
			{
				if (aDownload->getType() == Transfer::TYPE_FULL_LIST)
				{
					if (aDownload->isSet(Download::FLAG_XML_BZ_LIST))
					{
						q->setFlag(QueueItem::FLAG_XML_BZLIST);
					}
					else
					{
						q->unsetFlag(QueueItem::FLAG_XML_BZLIST);
					}
				}
				
				if (p_is_finished)
				{
					if (aDownload->getType() == Transfer::TYPE_TREE)
					{
						// Got a full tree, now add it to the HashManager
						dcassert(aDownload->getTreeValid());
						HashManager::addTree(aDownload->getTigerTree());
						
						{
							WLock(*QueueItem::g_cs); // [+] IRainman fix.
							g_userQueue.removeDownloadL(q, aDownload->getUser()); // [!] IRainman fix.
						}
						
						fire_status_updated(q);
					}
					else
					{
						// Now, let's see if this was a directory download filelist...
						dcassert(!q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD));
						if (q->isSet(QueueItem::FLAG_MATCH_QUEUE))
						{
							fileName = q->getListName();
							flags = q->isSet(QueueItem::FLAG_MATCH_QUEUE) ? QueueItem::FLAG_MATCH_QUEUE : 0;
						}
						
						string dir;
						if (aDownload->getType() == Transfer::TYPE_FULL_LIST)
						{
							dir = q->getTempTarget();
							{
								WLock(*QueueItem::g_cs); // [+] IRainman fix.
								q->addSegmentL(Segment(0, q->getSize()));
							}
						}
						else if (aDownload->getType() == Transfer::TYPE_FILE)
						{
							aDownload->setOverlapped(false);
							
							{
								WLock(*QueueItem::g_cs); // [+] IRainman fix.
								q->addSegmentL(aDownload->getSegment());
							}
						}
						
						const bool isFile = aDownload->getType() == Transfer::TYPE_FILE;
						bool isFinishedFile;
						if (isFile)
						{
							RLock(*QueueItem::g_cs); // [+] IRainman fix.
							isFinishedFile = q->isFinishedL();
						}
						else
						{
							isFinishedFile = false;
						}
						if (!isFile || isFinishedFile)
						{
							if (isFile)
							{
								// For partial-share, abort upload first to move file correctly
								UploadManager::abortUpload(q->getTempTarget());
								
								{
									WLock(*QueueItem::g_cs); // [+] IRainman fix.
									// Disconnect all possible overlapped downloads
									for (auto i = q->getDownloadsL().cbegin(); i != q->getDownloadsL().cend(); ++i)
									{
										if (i->second != aDownload)
											i->second->getUserConnection()->disconnect();
									}
								}
							}
							
							// Check if we need to move the file
							if (aDownload->getType() == Transfer::TYPE_FILE && !aDownload->getTempTarget().empty() && (stricmp(p_path.c_str(), aDownload->getTempTarget().c_str()) != 0))
							{
								if (!q->isSet(Download::FLAG_USER_GET_IP)) // fix  https://code.google.com/p/flylinkdc/issues/detail?id=1480
									// TODO !q->isSet(Download::FLAG_USER_CHECK)
								{
									moveFile(aDownload->getTempTarget(), p_path);
								}
							}
							SharedFileStream::cleanup();
							if (BOOLSETTING(LOG_DOWNLOADS) && (BOOLSETTING(LOG_FILELIST_TRANSFERS) || aDownload->getType() == Transfer::TYPE_FILE))
							{
								StringMap params;
								aDownload->getParams(params);
								LOG(DOWNLOAD, params);
							}
							//[+]PPA
							if (!q->isSet(Download::FLAG_XML_BZ_LIST) && !q->isSet(Download::FLAG_USER_GET_IP) && !q->isSet(Download::FLAG_DOWNLOAD_PARTIAL))
							{
								CFlylinkDBManager::getInstance()->push_download_tth(q->getTTH());
								if (BOOLSETTING(ENABLE_FLY_SERVER))
								{
									const string l_file_ext = Text::toLower(Util::getFileExtWithoutDot(p_path));
									if (CFlyServerConfig::isMediainfoExt(l_file_ext))
									{
#ifdef FLYLINKDC_USE_MEDIAINFO_SERVER
										CFlyTTHKey l_file(q->getTTH(), q->getSize());
										CFlyServerJSON::addDownloadCounter(l_file);
#endif // FLYLINKDC_USE_MEDIAINFO_SERVER
#ifdef _DEBUG
										//CFlyServerJSON::sendDownloadCounter(false);
#endif
									}
								}
							}
							//[~]PPA
							
							if (!ClientManager::isShutdown())
							{
								fly_fire3(QueueManagerListener::Finished(), q, dir, aDownload);
							}
							
							{
								WLock(*QueueItem::g_cs); // [+] IRainman fix.
								if (q->isSet(QueueItem::FLAG_USER_LIST)) // [<-] IRainman fix: moved form MainFrame to core.
								{
									// [!] IRainman fix: please always match listing without hint!
									m_listQueue.addTask(new DirectoryListInfo(HintedUser(q->getDownloadsL().begin()->first, Util::emptyString), q->getListName(), dir, aDownload->getRunningAverage()));
								}
								g_userQueue.removeQueueItemL(q);
							}
							// [+] IRainman dclst support
							if (q->isSet(QueueItem::FLAG_DCLST_LIST))
							{
								addDclst(q->getTarget());
							}
							// [~] IRainman dclst support
							
							if (!BOOLSETTING(KEEP_FINISHED_FILES_OPTION) || aDownload->getType() == Transfer::TYPE_FULL_LIST)
							{
								fire_remove_internal(q, false, false, false);
							}
							else
							{
								fire_status_updated(q);
							}
						}
						else
						{
							{
								WLock(*QueueItem::g_cs); // [+] IRainman fix.
								g_userQueue.removeDownloadL(q, aDownload->getUser()); // [!] IRainman fix.
							}
							
							if (aDownload->getType() != Transfer::TYPE_FILE || (p_is_report_finish && q->isWaitingL()))
							{
								fire_status_updated(q);
							}
						}
						setDirty();
					}
				}
				else
				{
					if (aDownload->getType() != Transfer::TYPE_TREE)
					{
						bool isEmpty;
						{
							RLock(*QueueItem::g_cs); // [+] IRainman fix.
							isEmpty = q->calcAverageSpeedAndCalcAndGetDownloadedBytesL() == 0;
						}
						if (isEmpty)
						{
							q->setTempTarget(Util::emptyString);
						}
						if (q->isSet(QueueItem::FLAG_USER_LIST))
						{
							// Blah...no use keeping an unfinished file list...
							File::deleteFile(q->getListName());
						}
						if (aDownload->getType() == Transfer::TYPE_FILE)
						{
							// mark partially downloaded chunk, but align it to block size
							int64_t downloaded = aDownload->getPos();
							downloaded -= downloaded % aDownload->getTigerTree().getBlockSize();
							
							if (downloaded > 0)
							{
								// since download is not finished, it should never happen that downloaded size is same as segment size
								dcassert(downloaded < aDownload->getSize());
								
								{
									WLock(*QueueItem::g_cs); // [+] IRainman fix.
									q->addSegmentL(Segment(aDownload->getStartPos(), downloaded));
								}
								
								setDirty();
							}
						}
					}
					
					if (q->getPriority() != QueueItem::PAUSED)
					{
						q->getOnlineUsers(getConn);
					}
					
					{
						WLock(*QueueItem::g_cs); // [+] IRainman fix.
						g_userQueue.removeDownloadL(q, aDownload->getUser()); // [!] IRainman fix.
					}
					
					fire_status_updated(q);
					
					if (aDownload->isSet(Download::FLAG_OVERLAP))
					{
						// overlapping segment disconnected, unoverlap original segment
						WLock(*QueueItem::g_cs); // [+] IRainman fix.
						q->setOverlappedL(aDownload->getSegment(), false); // [!] IRainman fix.
					}
				}
			}
			else if (aDownload->getType() != Transfer::TYPE_TREE)
			{
				const string l_tmp_target = aDownload->getTempTarget();
				if (!l_tmp_target.empty() && (aDownload->getType() == Transfer::TYPE_FULL_LIST || l_tmp_target != p_path))
				{
					File::deleteFile(l_tmp_target);
				}
			}
		}
		aDownload.reset();
	}
	
	for (auto i = getConn.cbegin(); i != getConn.cend(); ++i)
	{
		get_download_connection(*i);
	}
	
	if (!fileName.empty())
	{
		processList(fileName, l_hintedUser, flags);
	}
	
	// partial file list failed, redownload full list
	if (downloadList && l_user->isOnline())
	{
		try
		{
			addList(l_user, flags);
		}
		catch (const Exception&) {}
	}
}

void QueueManager::processList(const string& name, const HintedUser& hintedUser, int flags)
{
	dcassert(hintedUser.user); // [!] IRainman fix: It makes no sense to check the file list on the presence of a file from our download queue if the user in the file list is empty!
	
	DirectoryListing dirList(hintedUser);
	try
	{
		if (flags & QueueItem::FLAG_TEXT)
		{
			MemoryInputStream mis(name);
			dirList.loadXML(mis, true, false);
		}
		else
		{
			dirList.loadFile(name);
		}
	}
	catch (const Exception&)
	{
		LogManager::message(STRING(UNABLE_TO_OPEN_FILELIST) + ' ' + name);
		return;
	}
	// [+] IRainman fix.
	dcassert(dirList.getTotalFileCount());
	if (!dirList.getTotalFileCount())
	{
		LogManager::message(STRING(UNABLE_TO_OPEN_FILELIST) + " (dirList.getTotalFileCount() == 0) " + name);
		return;
	}
	// [~] IRainman fix.
	if (flags & QueueItem::FLAG_DIRECTORY_DOWNLOAD)
	{
		vector<DirectoryItemPtr> dl;
		{
			CFlyFastLock(csDirectories);
			const auto dp = m_directories.equal_range(hintedUser.user) | map_values;
			dl.assign(boost::begin(dp), boost::end(dp));
			m_directories.erase(hintedUser.user);
		}
		
		for (auto i = dl.cbegin(); i != dl.cend(); ++i)
		{
			DirectoryItem* di = *i;
			dirList.download(di->getName(), di->getTarget(), false);
			delete di;
		}
	}
	if (flags & QueueItem::FLAG_MATCH_QUEUE)
	{
		dirList.logMatchedFiles(hintedUser.user, matchListing(dirList));
	}
}

void QueueManager::recheck(const string& aTarget)
{
	rechecker.add(aTarget);
}

void QueueManager::removeAll()
{
	QueueManager::FileQueue::clearAll();
	CFlylinkDBManager::getInstance()->remove_queue_all_items();
}
void QueueManager::fire_remove_batch()
{
	StringList l_targets;
	{
		CFlyLock(m_cs_target_array);
		l_targets.swap(m_remove_target_array);
	}
	fire_removed_array(l_targets);
}
void QueueManager::fire_remove_internal(const QueueItemPtr& p_qi, bool p_is_remove_item, bool p_is_force_remove_item, bool p_is_batch_remove)
{
	if (p_is_batch_remove)
	{
		CFlyLock(m_cs_target_array);
		m_remove_target_array.push_back(p_qi->getTarget());
	}
	else
		fire_removed(p_qi);
		
	if (p_is_remove_item)
	{
		WLock(*QueueItem::g_cs);
		if (p_is_force_remove_item || p_is_remove_item && !p_qi->isFinishedL())
		{
			g_userQueue.removeQueueItemL(p_qi);
		}
	}
	g_fileQueue.removeDeferredDB(p_qi, p_is_batch_remove);
}
bool QueueManager::removeTarget(const string& aTarget, bool p_is_batch_remove)
{
	UserList x;
	{
		QueueItemPtr q = FileQueue::find_target(aTarget);
		if (!q)
			return false;
			
		if (q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD))
		{
			RLock(*QueueItem::g_cs); // [+] IRainman fix.
			dcassert(q->getSourcesL().size() == 1);
			{
				CFlyFastLock(csDirectories); // [+] IRainman fix.
				for_each(m_directories.equal_range(q->getSourcesL().begin()->first) | map_values, DeleteFunction()); // Мутное место
				m_directories.erase(q->getSourcesL().begin()->first);
			}
		}
		
		const auto l_temp_target = q->getTempTargetConst();
		if (!l_temp_target.empty())
		{
			// For partial-share
			UploadManager::abortUpload(l_temp_target);
		}
		
		if (q->isRunningL())
		{
			RLock(*QueueItem::g_cs); // [+] IRainman fix.
			const auto& d = q->getDownloadsL();
			for (auto i = d.cbegin(); i != d.cend(); ++i)
			{
				x.push_back(i->first);
			}
		}
		else if (!l_temp_target.empty() && l_temp_target != q->getTarget())
		{
			File::deleteFile(l_temp_target);
		}
		
		fire_remove_internal(q, true, false, p_is_batch_remove);
		setDirty();
	}
	
	for (auto i = x.cbegin(); i != x.cend(); ++i)
	{
		ConnectionManager::disconnect(*i, true);
	}
	return true;
}

void QueueManager::removeSource(const string& aTarget, const UserPtr& aUser, Flags::MaskType reason, bool removeConn /* = true */) noexcept
{
	bool isRunning = false;
	bool removeCompletely = false;
	do
	{
		WLock(*QueueItem::g_cs); // [!] IRainman fix.
		QueueItemPtr q = QueueManager::FileQueue::find_target(aTarget);
		if (!q)
			return;
			
		if (!q->isSourceL(aUser))
			return;
			
		if (q->isAnySet(QueueItem::FLAG_USER_LIST
		| QueueItem::FLAG_USER_GET_IP // [+] IRainman fix: auto-remove.
		               ))
		{
			removeCompletely = true;
			break;
		}
		
		if (reason == QueueItem::Source::FLAG_NO_TREE)
		{
			const auto& l_source = q->findSourceL(aUser);
			if (l_source != q->m_sources.end())
			{
				l_source->second.setFlag(reason);
			}
			return;
		}
		
		if (q->isRunningL() && g_userQueue.getRunningL(aUser) == q)
		{
			isRunning = true;
			g_userQueue.removeDownloadL(q, aUser); // [!] IRainman fix.
			fire_status_updated(q);
		}
		if (!q->isFinishedL())
		{
			g_userQueue.removeUserL(q, aUser, true);
		}
		q->removeSourceL(aUser, reason);
		
		fire_sources_updated(q);
		setDirty();
	}
	while (false);
	if (isRunning && removeConn)
	{
		ConnectionManager::disconnect(aUser, true);
	}
	if (removeCompletely)
	{
		removeTarget(aTarget, false);
	}
}

void QueueManager::removeSource(const UserPtr& aUser, Flags::MaskType reason) noexcept
{
	// @todo remove from finished items
	bool isRunning = false;
	string removeRunning;
	{
		QueueItemPtr qi;
		WLock(*QueueItem::g_cs);
		while ((qi = g_userQueue.getNextL(aUser, QueueItem::PAUSED)) != nullptr)
		{
			if (qi->isSet(QueueItem::FLAG_USER_LIST))
			{
				bool l_is_found = removeTarget(qi->getTarget(), false);
				if (l_is_found == false)
				{
					break;
					/*
					Читаем например с с юзера список файлов. В трансфере (внизу, окно передач) висит -1Б ( у меня гавноинторенты, но можно повторить на БОЛЬШОМ файллисте наверное),
					Идём в Очередь Скачивания. Слева в дереве - в ПКМ на File Lists выбираем Удалить всё / Удалить... бла-бла-бла
					Внизу в трансфере на строке с -1Б давим ПКМ, выбираем Удалить пользователя из очереди
					Флай виснет намертво.
					*/
				}
			}
			else
			{
				g_userQueue.removeUserL(qi, aUser, true);
				qi->removeSourceL(aUser, reason);
				fire_sources_updated(qi);
				setDirty();
			}
		}
		
		qi = g_userQueue.getRunningL(aUser);
		if (qi)
		{
			if (qi->isSet(QueueItem::FLAG_USER_LIST))
			{
				removeRunning = qi->getTarget();
			}
			else
			{
				g_userQueue.removeDownloadL(qi, aUser); // [!] IRainman fix.
				g_userQueue.removeUserL(qi, aUser, true);
				isRunning = true;
				qi->removeSourceL(aUser, reason);
				fire_status_updated(qi);
				fire_sources_updated(qi);
				setDirty();
			}
		}
	}
	
	if (isRunning)
	{
		ConnectionManager::disconnect(aUser, true);
	}
	if (!removeRunning.empty())
	{
		removeTarget(removeRunning, false);
	}
}

void QueueManager::setPriority(const string& aTarget, QueueItem::Priority p) noexcept
{
	UserList getConn;
	bool running = false;
	
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		QueueItemPtr q = QueueManager::FileQueue::find_target(aTarget);
		if (q != nullptr && q->getPriority() != p)
		{
			bool isFinished;
			{
				RLock(*QueueItem::g_cs); // [+] IRainman fix.
				isFinished = q->isFinishedL();
			}
			if (!isFinished)
			{
				running = q->isRunningL();
				
				if (q->getPriority() == QueueItem::PAUSED || p == QueueItem::HIGHEST)
				{
					// Problem, we have to request connections to all these users...
					q->getOnlineUsers(getConn);
				}
				g_userQueue.setQIPriority(q, p); // !!!!!!!!!!!!!!!!!! Удаляет и вставляет в массив каждую секунду
#ifdef _DEBUG
				LogManager::message("QueueManager g_userQueue.setQIPriority q->getTarget = " + q->getTarget());
#endif
				setDirty();
				fire_status_updated(q);
			}
		}
	}
	
	if (p == QueueItem::PAUSED)
	{
		if (running)
			DownloadManager::abortDownload(aTarget);
	}
	else
	{
		for (auto i = getConn.cbegin(); i != getConn.cend(); ++i)
		{
			get_download_connection(*i);
		}
	}
}

void QueueManager::setAutoPriority(const string& aTarget, bool ap) noexcept
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		vector<pair<string, QueueItem::Priority>> priorities;
		QueueItemPtr q = QueueManager::FileQueue::find_target(aTarget);
		if (q && q->getAutoPriority() != ap)
		{
			q->setAutoPriority(ap);
			if (ap)
			{
				priorities.push_back(make_pair(q->getTarget(), q->calculateAutoPriority()));
			}
			setDirty();
			fire_status_updated(q);
		}
		
		for (auto p = priorities.cbegin(); p != priorities.cend(); ++p)
		{
			setPriority((*p).first, (*p).second);
		}
	}
}

void QueueManager::saveQueue(bool force) noexcept
{
	if (!g_dirty && !force)
		return;
		
	CFlySegmentArray l_segment_array;
	std::vector<QueueItemPtr> l_items;
	{
		RLock(*QueueItem::g_cs);
		{
			{
				RLock(*FileQueue::g_csFQ);
				for (auto i = g_fileQueue.getQueueL().begin(); i != g_fileQueue.getQueueL().end(); ++i)
				{
					auto& qi = i->second;
					if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
					{
						if (qi->getFlyQueueID() &&
						qi->isDirtySegment() == true &&
						qi->isDirtyBase() == false &&
						qi->isDirtySource() == false)
						{
						
							const CFlySegment l_QueueSegment(qi);
							l_segment_array.push_back(l_QueueSegment);
							qi->setDirtySegment(false); // Считаем что обновление сегментов пройдет без ошибок.
						}
						else if (qi->isDirtyAll())
						{
							l_items.push_back(qi);
						}
					}
				}
			}
		}
		if (!l_items.empty())
		{
			if (l_items.size() > 50)
			{
				CFlyLog l_log("[Save queue to SQLite]");
				l_log.log("Store: " + Util::toString(l_items.size()) + " items...");
				CFlylinkDBManager::getInstance()->merge_queue_all_items(l_items);
			}
			else
			{
#ifdef _DEBUG
				CFlyLog l_log("[Save small! queue to SQLite]");
				l_log.log("Store small: " + Util::toString(l_items.size()) + " items...");
#endif
				CFlylinkDBManager::getInstance()->merge_queue_all_items(l_items);
			}
		}
	}
	// Если изменились только сегменты + приоритеты - можно обновить базу без блокировки менеджера очередей
	if (!l_segment_array.empty())
	{
		if (l_segment_array.size() > 10)
		{
			CFlyLog l_log("[Update queue segments]");
			l_log.log("Store: " + Util::toString(l_segment_array.size()) + " items...");
			CFlylinkDBManager::getInstance()->merge_queue_all_segments(l_segment_array);
		}
		else
		{
#ifdef _DEBUG
			CFlyLog l_log("[Update small queue segments]");
			l_log.log("Store small: " + Util::toString(l_segment_array.size()) + " items...");
#endif
			CFlylinkDBManager::getInstance()->merge_queue_all_segments(l_segment_array);
		}
	}
	if (m_is_exists_queueFile)
	{
		const auto l_queueFile = getQueueFile();
		File::deleteFile(l_queueFile);
		File::deleteFile(l_queueFile + ".bak");
		m_is_exists_queueFile = false;
	}
	// Put this here to avoid very many saves tries when disk is full...
	g_lastSave = GET_TICK();
	g_dirty = false; // [+] IRainman fix.
}

class QueueLoader : public SimpleXMLReader::CallBack
{
	public:
		QueueLoader() : m_cur(nullptr), m_isInDownloads(false) { }
		~QueueLoader() { }
		void startTag(const string& name, StringPairList& attribs, bool simple);
		void endTag(const string& name, const string& data);
	private:
		string m_target;
		
		QueueItemPtr m_cur;
		bool m_isInDownloads;
};

void QueueManager::loadQueue() noexcept
{
	try
	{
		QueueLoader l;
		File f(getQueueFile(), File::READ, File::OPEN);
		SimpleXMLReader(&l).parse(f);
	}
	catch (const Exception&)
	{
		m_is_exists_queueFile = false;
	}
	CFlylinkDBManager::getInstance()->load_queue();
	g_dirty = false;
}

static const string sDownload = "Download";
static const string sTempTarget = "TempTarget";
static const string sTarget = "Target";
static const string sSize = "Size";
static const string sDownloaded = "Downloaded";
static const string sPriority = "Priority";
static const string sSource = "Source";
static const string sNick = "Nick";
static const string sDirectory = "Directory";
static const string sAdded = "Added";
static const string sTTH = "TTH";
static const string sCID = "CID";
static const string sHubHint = "HubHint";
static const string sSegment = "Segment";
static const string sStart = "Start";
static const string sAutoPriority = "AutoPriority";
static const string sMaxSegments = "MaxSegments";

void QueueLoader::startTag(const string& name, StringPairList& attribs, bool simple)
{
	QueueManager* qm = QueueManager::getInstance();
	if (!m_isInDownloads && name == "Downloads")
	{
		m_isInDownloads = true;
	}
	else if (m_isInDownloads)
	{
		if (m_cur == nullptr && name == sDownload)
		{
			int64_t size = Util::toInt64(getAttrib(attribs, sSize, 1));
			if (size == 0)
				return;
			try
			{
				const string& tgt = getAttrib(attribs, sTarget, 0);
				// @todo do something better about existing files
				m_target = QueueManager::checkTarget(tgt,  /*checkExistence*/ -1);// [!] IRainman fix. FlylinkDC use Size on 2nd parametr!
				if (m_target.empty())
					return;
			}
			catch (const Exception&)
			{
				return;
			}
			QueueItem::Priority p = (QueueItem::Priority)Util::toInt(getAttrib(attribs, sPriority, 3));
			time_t added = static_cast<time_t>(Util::toInt(getAttrib(attribs, sAdded, 4)));
			const string& tthRoot = getAttrib(attribs, sTTH, 5);
			if (tthRoot.empty())
				return;
				
			const string& l_tempTarget = getAttrib(attribs, sTempTarget, 5);
			const uint8_t maxSegments = (uint8_t)Util::toInt(getAttrib(attribs, sMaxSegments, 5));
			int64_t downloaded = Util::toInt64(getAttrib(attribs, sDownloaded, 5));
			if (downloaded > size || downloaded < 0)
				downloaded = 0;
				
			if (added == 0)
				added = GET_TIME();
				
			QueueItemPtr qi = QueueManager::FileQueue::find_target(m_target);
			
			if (qi == NULL)
			{
				qi = QueueManager::g_fileQueue.add(0, m_target, size, 0, p, l_tempTarget, added, TTHValue(tthRoot), max((uint8_t)1, maxSegments));
				if (downloaded > 0)
				{
					{
						WLock(*QueueItem::g_cs); // [+] IRainman fix.
						qi->addSegmentL(Segment(0, downloaded));
					}
					qi->setPriority(qi->calculateAutoPriority());
				}
				
				const bool ap = Util::toInt(getAttrib(attribs, sAutoPriority, 6)) == 1;
				qi->setAutoPriority(ap);
				
				qm->fly_fire1(QueueManagerListener::Added(), qi);
			}
			if (!simple)
				m_cur = qi;
		}
		else if (m_cur && name == sSegment)
		{
			int64_t start = Util::toInt64(getAttrib(attribs, sStart, 0));
			int64_t size = Util::toInt64(getAttrib(attribs, sSize, 1));
			
			if (size > 0 && start >= 0 && (start + size) <= m_cur->getSize())
			{
				{
					WLock(*QueueItem::g_cs); // [+] IRainman fix.
					m_cur->addSegmentL(Segment(start, size));
				}
				m_cur->setPriority(m_cur->calculateAutoPriority());
			}
		}
		else if (m_cur && name == sSource)
		{
			const string& cid = getAttrib(attribs, sCID, 0);
			UserPtr user;
			if (cid.length() != 39)
			{
				user = ClientManager::getUser(getAttrib(attribs, sNick, 1), getAttrib(attribs, sHubHint, 1)
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
				                              , 0
#endif
				                              , false
				                             );
			}
			else
			{
				user = ClientManager::getUser(CID(cid), true);
			}
			
			bool wantConnection;
			try
			{
				WLock(*QueueItem::g_cs); // [+] IRainman fix.
				wantConnection = QueueManager::addSourceL(m_cur, user, 0) && user->isOnline();
			}
			catch (const Exception&)
			{
				return;
			}
			if (wantConnection)
			{
				QueueManager::get_download_connection(user);
			}
		}
	}
}

void QueueLoader::endTag(const string& name, const string&)
{
	if (m_isInDownloads)
	{
		if (name == sDownload)
		{
			m_cur = nullptr;
		}
		else if (name == "Downloads")
		{
			m_isInDownloads = false;
		}
	}
}

void QueueManager::noDeleteFileList(const string& path)
{
	if (!BOOLSETTING(KEEP_LISTS))
	{
		protectedFileLists.push_back(path);
	}
}

// SearchManagerListener
void QueueManager::on(SearchManagerListener::SR, const SearchResult& sr) noexcept
{
	bool added = false;
	bool wantConnection = false;
	bool needsAutoMatch = false;
	
	{
		// CFlyLock(cs); [-] IRainman fix.
		QueueItemList l_matches;
		
		g_fileQueue.find_tth(l_matches, sr.getTTH());
		
		if (!l_matches.empty()) // [+] IRainman opt.
		{
			WLock(*QueueItem::g_cs); // [!] http://code.google.com/p/flylinkdc/issues/detail?id=1082
			for (auto i = l_matches.cbegin(); i != l_matches.cend(); ++i)
			{
				const QueueItemPtr& qi = *i;
				// Size compare to avoid popular spoof
				if (qi->getSize() == sr.getSize())
				{
					if (!qi->isSourceL(sr.getUser())) // [!] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=1227
					{
						if (qi->isFinishedL())
							break;  // don't add sources to already finished files
							
						try
						{
							needsAutoMatch = !qi->countOnlineUsersGreatOrEqualThanL(SETTING(MAX_AUTO_MATCH_SOURCES));
							
							// [-] if (!BOOLSETTING(AUTO_SEARCH_AUTO_MATCH) || (l_count_online_users >= (size_t)SETTING(MAX_AUTO_MATCH_SOURCES))) [-] IRainman fix.
							wantConnection = addSourceL(qi, HintedUser(sr.getUser(), sr.getHubUrl()), 0);
							
							added = true;
						}
						catch (const Exception&)
						{
							// ...
						}
						break;
					}
				}
			}
		}
	}
	
	if (added)
	{
		if (needsAutoMatch && BOOLSETTING(AUTO_SEARCH_AUTO_MATCH))
		{
			try
			{
				const string path = Util::getFilePath(sr.getFile());
				// [!] IRainman fix: please always match listing without hint! old code: sr->getHubUrl().
				addList(sr.getUser(), QueueItem::FLAG_MATCH_QUEUE | (path.empty() ? 0 : QueueItem::FLAG_PARTIAL_LIST), path);
			}
			catch (const Exception&)
			{
				// ...
			}
		}
		if (wantConnection && sr.getUser()->isOnline())
		{
			get_download_connection(sr.getUser());
		}
	}
}

// ClientManagerListener
void QueueManager::on(ClientManagerListener::UserConnected, const UserPtr& aUser) noexcept
{
#ifdef IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
	bool hasDown = false;
	{
		RLock(*QueueItem::g_cs); // [!] IRainman fix.
		for (size_t i = 0; i < QueueItem::LAST; ++i)
		{
			const auto j = g_userQueue.getListL(i).find(aUser);
			if (j != g_userQueue.getListL(i).end())
			{
				fly_fire1(QueueManagerListener::StatusUpdatedList(), j->second); // [!] IRainman opt.
				if (i != QueueItem::PAUSED)
					hasDown = true;
			}
		}
	}
#else
	QueueItemList l_status_update_array;
	const bool hasDown = g_userQueue.userIsDownloadedFiles(aUser, l_status_update_array);
	if (!l_status_update_array.empty())
	{
		fly_fire1(QueueManagerListener::StatusUpdatedList(), l_status_update_array); // [!] IRainman opt.
	}
#endif // IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
	
	if (hasDown)
	{
		// the user just came on, so there's only 1 possible hub, no need for a hint
		get_download_connection(aUser);
	}
}

void QueueManager::on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept
{
#ifdef IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
	RLock(*QueueItem::g_cs); // [!] IRainman fix.
	for (size_t i = 0; i < QueueItem::LAST; ++i)
	{
		const auto j = g_userQueue.getListL(i).find(aUser);
		if (j != g_userQueue.getListL(i).end())
		{
			fly_fire1(QueueManagerListener::StatusUpdatedList(), j->second); // [!] IRainman opt.
		}
	}
#else
	QueueItemList l_status_update_array;
	g_userQueue.userIsDownloadedFiles(aUser, l_status_update_array);
	if (!l_status_update_array.empty())
	{
		if (!ClientManager::isShutdown())
		{
			fly_fire1(QueueManagerListener::StatusUpdatedList(), l_status_update_array); // [!] IRainman opt.
		}
	}
#endif // IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
}
#if 0
bool QueueManager::getTargetByRoot(const TTHValue& tth, string& p_target, string& p_tempTarget)
{
	// [-] CFlyLock(cs); [-] IRainman fix.
	QueueItemPtr qi = QueueManager::FileQueue::findQueueItem(tth);
	if (!qi)
		return false;
	p_target = qi->getTarget();
	p_tempTarget = qi->getTempTarget();
	return true;
}
#endif

bool QueueManager::isChunkDownloaded(const TTHValue& tth, int64_t startPos, int64_t& bytes, string& p_target)
{
	// [-] CFlyLock(cs); [-] IRainman fix.
	QueueItemPtr qi = QueueManager::FileQueue::findQueueItem(tth);
	if (!qi)
		return false;
	RLock(*QueueItem::g_cs); // TODO - унести это ниже!
	p_target = qi->isFinishedL() ? qi->getTarget() : qi->getTempTarget();
	
	return qi->isChunkDownloadedL(startPos, bytes);
}

void QueueManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (g_dirty && ((g_lastSave + 30000) < aTick))
	{
#ifdef _DEBUG
		LogManager::message("[!-> [Start] saveQueue lastSave = " + Util::toString(g_lastSave) + " aTick = " + Util::toString(aTick));
#endif
		saveQueue();
	}
	
	QueueItem::PriorityArray l_priorities;
	
	if (!ClientManager::isShutdown())
	{
		QueueItemList l_runningItems;
		{
			RLock(*QueueItem::g_cs);
			{
				RLock(*FileQueue::g_csFQ); // TODO - этот лок возможно тут не нужен
				calcPriorityAndGetRunningFilesL(l_priorities, l_runningItems);
			}
		}
		if (!l_runningItems.empty())
		{
			fly_fire1(QueueManagerListener::Tick(), l_runningItems); // Нельзя звать под локом
		}
		{
			StringList l_fire_src_array;
			{
				CFlyLock(m_cs_fire_src);
				l_fire_src_array.reserve(m_fire_src_array.size());
				for (auto i = m_fire_src_array.cbegin(); i != m_fire_src_array.cend(); ++i)
				{
					l_fire_src_array.push_back(*i);
				}
				m_fire_src_array.clear();
			}
			if (!l_fire_src_array.empty())
			{
				fly_fire1(QueueManagerListener::TargetsUpdated(), l_fire_src_array);
			}
		}
	}
	
	for (auto p = l_priorities.cbegin(); p != l_priorities.cend(); ++p)
	{
		setPriority(p->first, p->second);
	}
}

#ifdef PPA_INCLUDE_DROP_SLOW
bool QueueManager::dropSource(const DownloadPtr& aDownload)
{
	uint8_t activeSegments = 0;
	bool   allowDropSource;
	uint64_t overallSpeed;
	{
		RLock(*QueueItem::g_cs); // [!] IRainman fix.
		
		const QueueItemPtr q = g_userQueue.getRunningL(aDownload->getUser());
		
		dcassert(q); // [+] IRainman fix.
		if (!q)
			return false;
			
		dcassert(q->isSourceL(aDownload->getUser()));
		
		if (!q->isAutoDrop()) // [!] IRainman fix.
			return false;
			
		const auto& l_d = q->getDownloadsL();
		for (auto i = l_d.cbegin(); i != l_d.cend(); ++i)
		{
			if (i->second->getStart() > 0)
			{
				activeSegments++;
			}
			// more segments won't change anything
			if (activeSegments > 1)
				break;
		}
		
		allowDropSource = q->countOnlineUsersGreatOrEqualThanL(2);
		overallSpeed = q->getAverageSpeed();
	}
	
	if (!SETTING(DROP_MULTISOURCE_ONLY) || (activeSegments > 1))
	{
		if (allowDropSource)
		{
			const size_t iHighSpeed = SETTING(DISCONNECT_FILE_SPEED);
			
			if ((iHighSpeed == 0 || overallSpeed > iHighSpeed * 1024))
			{
				aDownload->setFlag(Download::FLAG_SLOWUSER);
				
				if (aDownload->getRunningAverage() < SETTING(REMOVE_SPEED) * 1024)
				{
					return true;
				}
				else
				{
					aDownload->getUserConnection()->disconnect();
				}
			}
		}
	}
	
	return false;
}
#endif

bool QueueManager::handlePartialResult(const UserPtr& aUser, const TTHValue& tth, const QueueItem::PartialSource& partialSource, PartsInfo& outPartialInfo)
{
	bool wantConnection = false;
	dcassert(outPartialInfo.empty());
	
	{
		// [-] CFlyLock(cs); [-] IRainman fix.
		
		// Locate target QueueItem in download queue
		QueueItemPtr qi = QueueManager::FileQueue::findQueueItem(tth);
		if (!qi)
			return false;
		LogManager::psr_message("[QueueManager::handlePartialResult] findQueueItem - OK TTH = " + tth.toBase32());
		
		WLock(*QueueItem::g_cs); // [+] IRainman fix.
		
		// don't add sources to finished files
		// this could happen when "Keep finished files in queue" is enabled
		if (qi->isFinishedL())
			return false;
			
		// Check min size
		if (qi->getSize() < PARTIAL_SHARE_MIN_SIZE)
		{
			dcassert(0);
			LogManager::psr_message(
			    "[QueueManager::handlePartialResult] qi->getSize() < PARTIAL_SHARE_MIN_SIZE. qi->getSize() = " + Util::toString(qi->getSize()));
			return false;
		}
		
		// Get my parts info
		const auto blockSize = qi->get_block_size_sql();
		qi->getPartialInfoL(outPartialInfo, qi->get_block_size_sql());
		
		// Any parts for me?
		wantConnection = qi->isNeededPartL(partialSource.getPartialInfo(), blockSize);
		
		// If this user isn't a source and has no parts needed, ignore it
		auto si = qi->findSourceL(aUser);
		if (si == qi->getSourcesL().end())
		{
			si = qi->findBadSourceL(aUser);
			
			if (si != qi->getBadSourcesL().end() && si->second.isSet(QueueItem::Source::FLAG_TTH_INCONSISTENCY))
				return false;
				
			if (!wantConnection)
			{
				if (si == qi->getBadSourcesL().end())
					return false;
			}
			else
			{
				// add this user as partial file sharing source
				qi->addSourceL(aUser, false);
				si = qi->findSourceL(aUser);
				si->second.setFlag(QueueItem::Source::FLAG_PARTIAL);
				
				const auto ps = std::make_shared<QueueItem::PartialSource>(partialSource.getMyNick(),
				                                                           partialSource.getHubIpPort(), partialSource.getIp(), partialSource.getUdpPort());
				si->second.setPartialSource(ps);
				
				g_userQueue.addL(qi, aUser, false);
				dcassert(si != qi->getSourcesL().end());
				fire_sources_updated(qi);
				LogManager::psr_message(
				    "[QueueManager::handlePartialResult] new QueueItem::PartialSource nick = " + partialSource.getMyNick() +
				    " HubIpPort = " + partialSource.getHubIpPort() +
				    " IP = " + partialSource.getIp().to_string() +
				    " UDP port = " + Util::toString(partialSource.getUdpPort())
				);
			}
		}
		
		// Update source's parts info
		if (si->second.getPartialSource())
		{
			si->second.getPartialSource()->setPartialInfo(partialSource.getPartialInfo());
		}
	}
	
	// Connect to this user
	if (wantConnection)
	{
		get_download_connection(aUser);
	}
	
	return true;
}

bool QueueManager::handlePartialSearch(const TTHValue& tth, PartsInfo& p_outPartsInfo)
{
	// Locate target QueueItem in download queue
	const QueueItemPtr qi = QueueManager::FileQueue::findQueueItem(tth);
	if (!qi)
		return false;
	if (qi->getSize() < PARTIAL_SHARE_MIN_SIZE)
	{
		return false;
	}
	
	RLock(*QueueItem::g_cs); // [+] IRainman fix.
	
	// don't share when file does not exist
	if (!File::isExist(qi->isFinishedL() ? qi->getTarget() : qi->getTempTargetConst()))
		return false;
		
	qi->getPartialInfoL(p_outPartsInfo, qi->get_block_size_sql());
	return !p_outPartsInfo.empty();
}

// compare nextQueryTime, get the oldest ones
void QueueManager::FileQueue::findPFSSourcesL(PFSSourceList& sl)
{
	QueueItem::SourceListBuffer buffer;
#ifdef _DEBUG
	QueueItem::SourceListBuffer debug_buffer;
#endif
	const uint64_t now = GET_TICK();
	// http://code.google.com/p/flylinkdc/issues/detail?id=1121
	RLock(*g_csFQ); // [+] IRainman fix.
	for (auto i = g_queue.cbegin(); i != g_queue.cend(); ++i)
	{
		const auto& q = i->second;
		
		if (q->getSize() < PARTIAL_SHARE_MIN_SIZE) continue;
		
		// don't share when file does not exist
		if (!File::isExist(q->isFinishedL() ? q->getTarget() : q->getTempTargetConst())) // Обязательно Const
			continue;
			
		QueueItem::getPFSSourcesL(q, buffer, now);
//////////////////
#ifdef _DEBUG
		// После отладки убрать сарый вариант наполнения
		const auto& sources = q->getSourcesL();
		const auto& badSources = q->getBadSourcesL();
		for (auto j = sources.cbegin(); j != sources.cend(); ++j)
		{
			const auto &l_getPartialSource = j->second.getPartialSource(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the '(* j).getPartialSource()' expression repeatedly. queuemanager.cpp 2900
			if (j->second.isSet(QueueItem::Source::FLAG_PARTIAL) &&
			        l_getPartialSource->getNextQueryTime() <= now &&
			        l_getPartialSource->getPendingQueryCount() < 10 && l_getPartialSource->getUdpPort() > 0)
			{
				debug_buffer.insert(make_pair(l_getPartialSource->getNextQueryTime(), make_pair(j, q)));
			}
			
		}
		for (auto j = badSources.cbegin(); j != badSources.cend(); ++j)
		{
			const auto &l_getPartialSource = j->second.getPartialSource(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the '(* j).getPartialSource()' expression repeatedly. queuemanager.cpp 2900
			if (j->second.isSet(QueueItem::Source::FLAG_TTH_INCONSISTENCY) == false && j->second.isSet(QueueItem::Source::FLAG_PARTIAL) &&
			        l_getPartialSource->getNextQueryTime() <= now && l_getPartialSource->getPendingQueryCount() < 10 &&
			        l_getPartialSource->getUdpPort() > 0)
			{
				debug_buffer.insert(make_pair(l_getPartialSource->getNextQueryTime(), make_pair(j, q)));
			}
		}
#endif
//////////////////
	}
	// TODO: opt this function.
	// copy to results
	dcassert(debug_buffer == buffer);
	dcassert(sl.empty());
	if (!buffer.empty())
	{
		const size_t maxElements = 10;
		sl.reserve(std::min(buffer.size(), maxElements));
		for (auto i = buffer.cbegin(); i != buffer.cend() && sl.size() < maxElements; ++i)
		{
			sl.push_back(i->second);
		}
	}
}

#ifdef STRONG_USE_DHT
TTHValue* QueueManager::FileQueue::findPFSPubTTH()
{
	const uint64_t now = GET_TICK();
	QueueItemPtr cand;
	RLock(*QueueItem::g_cs);
	{
		RLock(*g_csFQ); // [+] IRainman fix.
		
		for (auto i = g_queue.cbegin(); i != g_queue.cend(); ++i)
		{
			const auto& qi = i->second;
			// [-] IRainman fix: not possible in normal state.
			// if (qi == nullptr)  // PVS-Studio V595 The pointer was utilized before it was verified against nullptr.
			// continue;
			// [~]
			
			if (!File::isExist(qi->isFinishedL() ? qi->getTarget() : qi->getTempTargetConst())) // don't share when file does not exist
				continue;
				
			if (qi->getPriority() > QueueItem::PAUSED && qi->getSize() >= PARTIAL_SHARE_MIN_SIZE && now >= qi->getNextPublishingTime())
			{
				if (cand == nullptr || cand->getNextPublishingTime() > qi->getNextPublishingTime() || (cand->getNextPublishingTime() == qi->getNextPublishingTime() && cand->getPriority() < qi->getPriority()))
				{
					if (qi->getDownloadedBytes() > qi->get_block_size_sql())
						cand = qi;
				}
			}
		}
		
		if (cand)
		{
			cand->setNextPublishingTime(now + PFS_REPUBLISH_TIME);      // one hour
			return new TTHValue(cand->getTTH());
		}
	}
	return nullptr;
}
#endif

/**
 * @file
 * $Id: QueueManager.cpp 583 2011-12-18 13:06:31Z bigmuscle $
 */
