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

#ifdef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
#ifndef IRAINMAN_USE_SHARED_SPIN_LOCK
#error You can not include a directive IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER without the inclusion directive IRAINMAN_USE_SHARED_SPIN_LOCK because it will lead to deadlocks!
#endif
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


#ifdef STRONG_USE_DHT
#include "../dht/IndexManager.h"
#else
#include "ShareManager.h"
#endif


using boost::adaptors::map_values;
using boost::range::for_each;

class DirectoryItem
{
	public:
		DirectoryItem() : priority(QueueItem::DEFAULT) { }
		DirectoryItem(const UserPtr& aUser, const string& aName, const string& aTarget,
		              QueueItem::Priority p) : name(aName), target(aTarget), priority(p), user(aUser) { }
		~DirectoryItem() { }
		
		UserPtr& getUser()
		{
			return user;
		}
		void setUser(const UserPtr& aUser)
		{
			user = aUser;
		}
		
		GETSET(string, name, Name);
		GETSET(string, target, Target);
		GETSET(QueueItem::Priority, priority, Priority);
	private:
		UserPtr user;
};


QueueManager::FileQueue::FileQueue()
#ifndef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
	: csFQ(QueueItem::cs)
#endif
{
	SettingsManager::getInstance()->addListener(this);
}

QueueManager::FileQueue::~FileQueue()
{
	SettingsManager::getInstance()->removeListener(this);
	
	UniqueLock l(csFQ); // [+] IRainman fix. http://code.google.com/p/flylinkdc/issues/detail?id=1121
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
		i->second->dec();
}

QueueItemPtr QueueManager::FileQueue::add(const string& aTarget, int64_t aSize,
                                          Flags::MaskType aFlags, QueueItem::Priority p, const string& aTempTarget,
                                          time_t aAdded, const TTHValue& root) throw(QueueException, FileException) // [!] IRainman fix.
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
// [+] FlylinkDC
	// These override any other priority settings
	getUserSettingsPriority(aTarget, p);
// [~] FlylinkDC

	QueueItem* qi = new QueueItem(aTarget, aSize, p, aFlags, aAdded, root);
	
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
	
	qi->setTempTarget(aTempTarget);
	if (!aTempTarget.empty())
	{
		if (!File::isExist(aTempTarget) && File::isExist(aTempTarget + ".antifrag"))
		{
			// load old antifrag file
			File::renameFile(aTempTarget + ".antifrag", qi->getTempTarget());
		}
	}
	dcassert(find(aTarget) == NULL);
	add(qi);
	return qi;
}

void QueueManager::FileQueue::add(const QueueItemPtr& qi) // [!] IRainman fix.
{
	UniqueLock l(csFQ); // [+] IRainman fix.
	m_queue.insert(make_pair(const_cast<string*>(&qi->getTarget()), qi));
}

void QueueManager::FileQueue::remove(const QueueItemPtr& qi) // [!] IRainman fix.
{
	{
		UniqueLock l(csFQ); // [+] IRainman fix.
		m_queue.erase(const_cast<string*>(&qi->getTarget()));
	}
	qi->dec();
	const auto l_id = qi->getFlyQueueID();
	if (l_id) // [!] IRainman fix: FlyQueueID is not set for filelists.
		CFlylinkDBManager::getInstance()->remove_queue_item(l_id);
}

void QueueManager::FileQueue::find(QueueItemList& sl, int64_t aSize, const string& suffix) const
{
	SharedLock l(csFQ); // [+] IRainman fix.
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
	{
		if (i->second->getSize() == aSize)
		{
			const string& t = i->second->getTarget();
			if (suffix.empty() || (suffix.length() < t.length() &&
			                       stricmp(suffix.c_str(), t.c_str() + (t.length() - suffix.length())) == 0))
				sl.push_back(i->second);
		}
	}
}

void QueueManager::FileQueue::find(QueueItemList& ql, const TTHValue& tth) const
{
	SharedLock l(csFQ); // [+] IRainman fix.
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
	{
		const QueueItemPtr& qi = i->second;
		if (qi->getTTH() == tth)
		{
			ql.push_back(qi);
		}
	}
}

QueueItemPtr QueueManager::FileQueue::find(const TTHValue& tth) const // [+] IRainman opt.
{
	SharedLock l(csFQ);
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
	{
		const QueueItemPtr& qi = i->second;
		if (qi->getTTH() == tth)
		{
			return qi;
		}
	}
	return nullptr;
}

bool QueueManager::FileQueue::isQueueItem(const TTHValue& tth) const // [+] IRainman opt.
{
	SharedLock l(csFQ);
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
	{
		const QueueItemPtr& qi = i->second;
		if (qi->getTTH() == tth)
		{
			return true;
		}
	}
	return false;
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

QueueItemPtr QueueManager::FileQueue::findAutoSearch(deque<string>& recent) const // [!] IRainman fix.
{
	UniqueLock l(QueueItem::cs); // [+] IRainman fix.
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
	SharedLock ll(csFQ); // [+] IRainman fix.
#endif
	
	dcassert(!m_queue.empty()); // https://crash-server.com/Problem.aspx?ProblemID=32091
	// http://code.google.com/p/flylinkdc/issues/detail?id=1121
	// We pick a start position at random, hoping that we will find something to search for...
	const auto start = (QueueItem::QIStringMap::size_type)Util::rand((uint32_t)m_queue.size());
	
	auto i = m_queue.cbegin();
	advance(i, start);
	
	QueueItemPtr cand = findCandidateL(i, m_queue.end(), recent);
	if (cand == nullptr)
	{
		cand = findCandidateL(m_queue.begin(), i, recent);
	}
	else if (cand->getNextSegmentL(0, 0, 0, nullptr).getSize() == 0)
	{
		QueueItemPtr cand2 = findCandidateL(m_queue.begin(), i, recent);
		if (cand2 != nullptr && cand2->getNextSegmentL(0, 0, 0, nullptr).getSize() != 0)
		{
			cand = cand2;
		}
	}
	return cand;
}

void QueueManager::FileQueue::move(const QueueItemPtr& qi, const string& aTarget)
{
	{
		UniqueLock l(csFQ); // [+] IRainman fix.
		m_queue.erase(const_cast<string*>(&qi->getTarget()));
	}
	qi->dec(); // [+] IRainman fix.
	qi->setTarget(aTarget);
	add(qi);
}

void QueueManager::UserQueue::add(const QueueItemPtr& qi) // [!] IRainman fix.
{
	UniqueLock lqi(QueueItem::cs); // [+] IRainman fix.
	for (auto i = qi->getSourcesL().cbegin(); i != qi->getSourcesL().cend(); ++i)
	{
		addL(qi, i->getUser());
	}
}

void QueueManager::UserQueue::addL(const QueueItemPtr& qi, const UserPtr& aUser) // [!] IRainman fix.
{
	auto& uq = userQueue[qi->getPriority()][aUser];
	
	if (qi->isSet(QueueItem::FLAG_USER_CHECK) || qi->calcAverageSpeedAndCalcAndGetDownloadedBytesL() > 0)
	{
		uq.push_front(qi);
	}
	else
	{
		uq.push_back(qi);
	}
}

bool QueueManager::FileQueue::getTTH(const string& p_name, TTHValue& p_tth) const
{
	SharedLock l(csFQ);
	auto i = m_queue.find(const_cast<string*>(&p_name));
	if (i != m_queue.cend())
	{
		p_tth = i->second->getTTH();
		return true;
	}
	return false;
}
QueueItemPtr QueueManager::FileQueue::find(const string& p_target) const // [!] IRainman fix.
{
	SharedLock l(csFQ); // [+] IRainman fix.
	auto i = m_queue.find(const_cast<string*>(&p_target)); // TODO - fix http://code.google.com/p/flylinkdc/issues/detail?id=1246
	if (i != m_queue.cend())
		return i->second;
	return nullptr;
}

void QueueManager::FileQueue::on(SettingsManagerListener::QueueChanges) noexcept
{
	auto highPrioFiles = SPLIT_SETTING(HIGH_PRIO_FILES);
	auto lowPrioFiles = SPLIT_SETTING(LOW_PRIO_FILES);
	
	FastLock l(m_csPriorities);
	swap(m_highPrioFiles, highPrioFiles);
	swap(m_lowPrioFiles, lowPrioFiles);
}

QueueItemPtr QueueManager::UserQueue::getNextL(const UserPtr& aUser, QueueItem::Priority minPrio, int64_t wantedSize, int64_t lastSpeed, bool allowRemove) // [!] IRainman fix.
{
	int p = QueueItem::LAST - 1;
	lastError.clear();
	do
	{
		const auto i = userQueue[p].find(aUser);
		if (i != userQueue[p].cend())
		{
			dcassert(!i->second.empty());
			for (auto j = i->second.cbegin(); j != i->second.cend(); ++j)
			{
				const QueueItemPtr& qi = *j;
				QueueItem::SourceConstIter l_source = qi->getSourceL(aUser); // [!] IRainman fix done: [10] https://www.box.net/shared/6ea561f898012606519a
				if (l_source == qi->sources.end()) //[+]PPA
					continue;
				if (l_source->isSet(QueueItem::Source::FLAG_PARTIAL)) // TODO Crash
				{
					// check partial source
					const int64_t blockSize = qi->getBlockSize();
					dcassert(blockSize);
					const Segment segment = qi->getNextSegmentL(blockSize, wantedSize, lastSpeed, l_source->getPartialSource());
					if (allowRemove && segment.getStart() != -1 && segment.getSize() == 0)
					{
						// no other partial chunk from this user, remove him from queue
						removeL(qi, aUser);
						qi->removeSourceL(aUser, QueueItem::Source::FLAG_NO_NEED_PARTS);
						lastError = STRING(NO_NEEDED_PART);
						p++;
						break;
					}
				}
				if (qi->isWaitingL()) // там внутри m_downloads.empty(); поэтому след можно сделать qi->getDownloadsL().front()
				{
					// check maximum simultaneous files setting
					if (SETTING(FILE_SLOTS) == 0 ||
					        qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP) ||
					        QueueManager::getInstance()->getRunningFileCount() < (size_t)SETTING(FILE_SLOTS))
					{
						return qi;
					}
					else
					{
						lastError = STRING(ALL_FILE_SLOTS_TAKEN);
						continue;
					}
				}
				else if (qi->getDownloadsL().front()->getType() == Transfer::TYPE_TREE) // No segmented downloading when getting the tree
				{
					continue;
				}
				if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
				{
					const int64_t blockSize = qi->getBlockSize();
					Segment segment = qi->getNextSegmentL(blockSize, wantedSize, lastSpeed, l_source->getPartialSource());
					if (segment.getSize() == 0)
					{
						lastError = segment.getStart() == -1 ? STRING(ALL_DOWNLOAD_SLOTS_TAKEN) : STRING(NO_FREE_BLOCK);
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

void QueueManager::UserQueue::addDownloadL(const QueueItemPtr& qi, Download* d) // [!] IRainman fix: this function needs external lock.
{
	qi->getDownloadsL().push_back(d);
	// Only one download per user...
	dcassert(running.find(d->getUser()) == running.end());
	running[d->getUser()] = qi;
}
void QueueManager::FileQueue::getUserSettingsPriority(const string& target, QueueItem::Priority& p_prio) const
{
	if (!m_highPrioFiles.empty() || !m_lowPrioFiles.empty())
	{
		const auto lowerName = Text::toLower(Util::getFileName(target));
		
		FastLock l(m_csPriorities);
		if (Wildcard::patternMatchLowerCase(lowerName, m_highPrioFiles))
		{
			p_prio = QueueItem::HIGHEST;
		}
		else if (Wildcard::patternMatchLowerCase(lowerName, m_lowPrioFiles))
		{
			p_prio = QueueItem::LOWEST;
		}
	}
}

size_t QueueManager::FileQueue::getRunningFileCount() const
{
	size_t l_cnt = 0;
	SharedLock l(csFQ); // [+] IRainman fix.
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
	{
		const QueueItemPtr& q = i->second;
		if (q->isRunningL()) //
		{
			++l_cnt;
		}
		// TODO - можно не бегать по очереди а считать в онлайне.
		// Алгоритм:
		// 1. при удалении из m_queue -1 (если m_queue.download заполнена)
		// 2  при добавлении к m_queue +1 (если m_queue.download заполнена)
		// 2. при добавлении в m_queue.download +1
		// 3. при опустошении m_queue.download -1
		
	}
	return l_cnt;
}

void QueueManager::FileQueue::calcPriorityAndGetRunningFiles(QueueItem::PriorityArray& p_changedPriority, QueueItemList& p_runningFiles)
{
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
	SharedLock l(QueueItem::cs);
#endif
	SharedLock ll(csFQ); // [+] IRainman fix.
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
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
						p_changedPriority.push_back(make_pair(q->getTarget(), p2));
				}
			}
		}
	}
}

void QueueManager::UserQueue::removeDownloadL(const QueueItemPtr& qi, const UserPtr& user) // [!] IRainman fix: this function needs external lock.
{
	running.erase(user);
	
	for (auto i = qi->getDownloadsL().cbegin(); i != qi->getDownloadsL().cend(); ++i)
	{
		if ((*i)->getUser() == user) // bug! http://code.google.com/p/flylinkdc/issues/detail?id=1269
		{
			qi->getDownloadsL().erase(i);
			break;
		}
	}
}

void QueueManager::UserQueue::setPriority(const QueueItemPtr& qi, QueueItem::Priority p) // [!] IRainman fix.
{
	UniqueLock l(QueueItem::cs); // [+] IRainamn fix.
	removeL(qi, false);
	qi->setPriority(p);
	add(qi);
}

QueueItemPtr QueueManager::UserQueue::getRunningL(const UserPtr& aUser) // [!] IRainman fix.
{
	const auto i = running.find(aUser);
	return (i == running.cend()) ? nullptr : i->second;
}

void QueueManager::UserQueue::removeL(const QueueItemPtr& qi, bool removeRunning) // [!] IRainman fix.
{
	const auto& s = qi->getSourcesL();
	for (auto i = s.cbegin(); i != s.cend(); ++i)
	{
		removeL(qi, i->getUser(), removeRunning);
	}
}

void QueueManager::UserQueue::removeL(const QueueItemPtr& qi, const UserPtr& aUser, bool removeRunning) // [!] IRainman fix.
{
	if (removeRunning && qi == getRunningL(aUser))
	{
		removeDownloadL(qi, aUser); // [!] IRainman fix.
	}
	const bool isSource = qi->isSourceL(aUser);
	if (!isSource)
	{
		dcassert(isSource);
		return;
	}
	
	auto& ulm = userQueue[qi->getPriority()];
	const auto j = ulm.find(aUser);
	if (j == ulm.cend())
	{
		dcassert(j != ulm.cend());
		return;
	}
	
	auto& uq = j->second;
	const auto i = find(uq.begin(), uq.end(), qi);
	if (i == uq.cend())
	{
		dcassert(i != uq.cend());
		return;
	}
	
	uq.erase(i);
	if (uq.empty())
	{
		ulm.erase(j);
	}
}

void QueueManager::Rechecker::execute(const string && p_file) // [!] IRainman core.
{
	QueueItemPtr q;
	int64_t tempSize;
	TTHValue tth;
	
	{
		// [-] Lock l(qm->cs); [-] IRainman fix.
		
		q = qm->fileQueue.find(p_file);
		if (!q || q->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			return;
			
		qm->fire(QueueManagerListener::RecheckStarted(), q->getTarget());
		dcdebug("Rechecking %s\n", p_file.c_str());
		
		tempSize = File::getSize(q->getTempTarget());
		
		if (tempSize == -1)
		{
			qm->fire(QueueManagerListener::RecheckNoFile(), q->getTarget());
			// [+] IRainman fix.
			q->resetDownloaded();
			qm->rechecked(q);
			// [~] IRainman fix.
			return;
		}
		
		if (tempSize < 64 * 1024)
		{
			qm->fire(QueueManagerListener::RecheckFileTooSmall(), q->getTarget());
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
				LogManager::getInstance()->message("[Error] setSize - " + q->getTempTarget() + " Error=" + p_e.getError());
			}
		}
		
		if (q->isRunningL())
		{
			qm->fire(QueueManagerListener::RecheckDownloadsRunning(), q->getTarget());
			return;
		}
		
		tth = q->getTTH();
	}
	
	TigerTree tt;
	string tempTarget;
	
	{
		// [-] Lock l(qm->cs); [-] IRainman fix.
		
		// get q again in case it has been (re)moved
		q = qm->fileQueue.find(p_file);
		if (!q)
			return;
			
		if (!CFlylinkDBManager::getInstance()->getTree(tth, tt))
		{
			qm->fire(QueueManagerListener::RecheckNoTree(), q->getTarget());
			return;
		}
		
		//Clear segments
		q->resetDownloaded();
		
		tempTarget = q->getTempTarget();
	}
	
	//Merklecheck
	int64_t startPos = 0;
	DummyOutputStream dummy;
	int64_t blockSize = tt.getBlockSize();
	bool hasBadBlocks = false;
	
	vector<uint8_t> buf((size_t)min((int64_t)1024 * 1024, blockSize));
	
	typedef pair<int64_t, int64_t> SizePair;
	typedef vector<SizePair> Sizes;
	Sizes sizes;
	
	try // [+] IRainman fix.
	{
		File inFile(tempTarget, File::READ, File::OPEN);
		// [!] IRainman fix done: FileException! 2012-05-03_22-00-59_ASFM4NEIHGU4QDY7Y47A4QA54OSD25MOF4732FQ_4FBD35E8_crash-stack-r502-beta24-build-9900.dmp
		// 2012-05-11_23-53-01_L6P3Z3TKLYYLIFJTSF2JIMBT67EAZEI6RYDTRSI_6363943D_crash-stack-r502-beta26-build-9946.dmp
		
		while (startPos < tempSize)
		{
			try
			{
				MerkleCheckOutputStream<TigerTree, false> check(tt, &dummy, startPos);
				
				inFile.setPos(startPos);
				int64_t bytesLeft = min((tempSize - startPos), blockSize); //Take care of the last incomplete block
				int64_t segmentSize = bytesLeft;
				while (bytesLeft > 0)
				{
					size_t n = (size_t)min((int64_t)buf.size(), bytesLeft);
					size_t nr = inFile.read(&buf[0], n);
					check.write(&buf[0], nr);
					bytesLeft -= nr;
					if (bytesLeft > 0 && nr == 0)
					{
						// Huh??
						throw Exception();
					}
				}
				check.flush();
				
				sizes.push_back(make_pair(startPos, segmentSize));
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
	
	// [-] Lock l(qm->cs); [-] IRainman fix. //[4] https://www.box.net/shared/4c41b1400336247cce1c
	
	// get q again in case it has been (re)moved
	q = qm->fileQueue.find(p_file);
	
	if (!q)
		return;
		
	//If no bad blocks then the file probably got stuck in the temp folder for some reason
	if (!hasBadBlocks)
	{
		qm->moveStuckFile(q);
		return;
	}
	
	{
		UniqueLock l(QueueItem::cs); // [+] IRainman fix.
		for (auto i = sizes.cbegin(); i != sizes.cend(); ++i)
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
	lastSave(0),
	rechecker(this),
	dirty(true),
	nextSearch(0),
	exists_queueFile(true)
{
	TimerManager::getInstance()->addListener(this);
	SearchManager::getInstance()->addListener(this);
	ClientManager::getInstance()->addListener(this);
	
	File::ensureDirectory(Util::getListPath());
}
#pragma warning(pop)

QueueManager::~QueueManager() noexcept
{
	SearchManager::getInstance()->removeListener(this);
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	// [+] IRainman core.
	m_listMatcher.waitShutdown();
	m_listQueue.waitShutdown();
	waiter.waitShutdown();
	dclstLoader.waitShutdown();
	mover.waitShutdown();
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
}

struct PartsInfoReqParam
{
	PartsInfo   parts;
	string      tth;
	string      myNick;
	string      hubIpPort;
	string      ip;
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
		SharedLock l(QueueItem::cs); // fix http://code.google.com/p/flylinkdc/issues/detail?id=1236
		//find max 10 pfs sources to exchange parts
		//the source basis interval is 5 minutes
		fileQueue.findPFSSourcesL(sl);
		
		for (auto i = sl.cbegin(); i != sl.cend(); ++i)
		{
			QueueItem::PartialSource::Ptr source = i->first->getPartialSource();
			const QueueItemPtr& qi = i->second;
			
			PartsInfoReqParam* param = new PartsInfoReqParam;
			
			const int64_t blockSize = qi->getBlockSize();
			dcassert(blockSize);
			qi->getPartialInfoL(param->parts, blockSize);
			
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
		tthPub = fileQueue.findPFSPubTTH();
#endif
		
	if (fileQueue.getSize() > 0 && aTick >= nextSearch && BOOLSETTING(AUTO_SEARCH))
	{
		// We keep 30 recent searches to avoid duplicate searches
		while (recent.size() >= fileQueue.getSize() || recent.size() > 30)
		{
			recent.pop_front();
		}
		
		QueueItemPtr qi = nullptr;
		while ((qi = fileQueue.findAutoSearch(recent)) == nullptr && !recent.empty()) // Местами не переставлять findAutoSearch меняет recent
		{
			recent.pop_front();
		}
		if (qi)
		{
			searchString = qi->getTTH().toBase32();
			recent.push_back(qi->getTarget());
			nextSearch = aTick + (SETTING(AUTO_SEARCH_TIME) * 60000);
			if (BOOLSETTING(REPORT_ALTERNATES))
				LogManager::getInstance()->message(STRING(ALTERNATES_SEND) + ' ' + Util::getFileName(qi->getTargetFileName()));
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
			AdcCommand cmd = SearchManager::getInstance()->toPSR(true, param->myNick, param->hubIpPort, param->tth, param->parts);
			Socket s;
			s.writeTo(param->ip, param->udpPort, cmd.toString(ClientManager::getMyCID()));
		}
		catch (...)
		{
			dcdebug("Partial search caught error\n");
		}
		
		delete param;
	}
#ifdef STRONG_USE_DHT
	// DHT PFS announce
	if (tthPub)
	{
		dht::IndexManager::getInstance()->publishPartialFile(*tthPub);
		delete tthPub;
	}
#endif
	
	if (!searchString.empty())
	{
		SearchManager::getInstance()->search(searchString, 0, SearchManager::TYPE_TTH, Search::SIZE_DONTCARE, "auto");
	}
}

void QueueManager::addList(const UserPtr& aUser, Flags::MaskType aFlags, const string& aInitialDir /* = Util::emptyString */) throw(QueueException, FileException)
{
	add(aInitialDir, -1, TTHValue(), aUser, (Flags::MaskType)(QueueItem::FLAG_USER_LIST | aFlags));
}

void QueueManager::DclstLoader::execute(const string && p_currentDclstFile) // [+] IRainman dclst support.
{
	const string l_dclstFilePath = Util::getFilePath(p_currentDclstFile);
	unique_ptr<DirectoryListing> dl(new DirectoryListing(HintedUser(UserPtr(), Util::emptyString)));
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
	
	StringList nicks = ClientManager::getInstance()->getNicks(user->getCID(), Util::emptyString);
	string nick = nicks.empty() ? Util::emptyString : Util::cleanPathChars(nicks[0]) + ".";
	return checkTarget(Util::getListPath() + nick + user->getCID().toBase32());// [!] IRainman fix. FlylinkDC use Size on 2nd parametr!
}

void QueueManager::add(const string& aTarget, int64_t aSize, const TTHValue& root, const UserPtr& aUser,
                       Flags::MaskType aFlags /* = 0 */, bool addBad /* = true */, bool p_first_file /*= true*/) throw(QueueException, FileException)
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
	        if (ShareManager::getInstance()->isTTHShared(root)){
	            throw QueueException(STRING(TTH_ALREADY_SHARED));
	        }
	    }
	*/
	
	const bool l_userList = (aFlags & QueueItem::FLAG_USER_LIST) == QueueItem::FLAG_USER_LIST;
	const bool l_testIP = (aFlags & QueueItem::FLAG_USER_GET_IP) == QueueItem::FLAG_USER_GET_IP;
	const bool l_newItem = !(l_testIP || l_userList);
	
	string target;
	string tempTarget;
	
	if (l_userList)
	{
		dcassert(aUser); // [!] IRainman fix: You can not add to list download an empty user.
		target = getListPath(aUser);
		tempTarget = aTarget;
	}
	else if (l_testIP)
	{
		dcassert(aUser); // [!] IRainman fix: You can not add to check download an empty user.
		target = getListPath(aUser) + ".check";
		tempTarget = aTarget;
	}
	else
	{
		//+SMT, BugMaster: ability to use absolute file path
		if (File::isAbsolute(aTarget))
			target = aTarget;
		else
			target = FavoriteManager::getInstance()->getDownloadDirectory(Util::getFileExt(aTarget)) + aTarget;//[!] IRainman support download to specify extension dir.
		//target = SETTING(DOWNLOAD_DIRECTORY) + aTarget;
		//-SMT, BugMaster: ability to use absolute file path
		target = checkTarget(target); // [!] IRainman fix. FlylinkDC use Size on 2nd parametr!
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
			File::ensureDirectory(target);
			File f(target, File::WRITE, File::CREATE);
		}
		return;
	}
	
	bool l_wantConnection;
	
	{
		// [-] Lock l(cs); [-] IRainman fix.
		
		QueueItemPtr q = fileQueue.find(target);
		if (q == nullptr && l_newItem
		        && (BOOLSETTING(MULTI_CHUNK) && aSize > SETTING(MIN_MULTI_CHUNK_SIZE) * 1024 * 1024) // [+] IRainman size in MB.
		   )
		{
#ifdef IRAINMAN_FASTS_QUEUE_MANAGER
			q = fileQueue.find(root);
#else
			QueueItemList ql;
			fileQueue.find(ql, root);
			if (!ql.empty())
			{
				dcassert(ql.size() == 1);
				q = ql.front();
			}
#endif // IRAINMAN_FASTS_QUEUE_MANAGER
		}
		
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
				ShareManager::getInstance()->getRealPathAndSize(root, l_shareExistingPath, l_shareExistingSize);
				getTargetByRoot(...); - тоже можно использовать.
				*/
				int64_t l_existingFileSize;
				time_t l_eistingFileTime;
				if (File::isExist(target, l_existingFileSize, l_eistingFileTime))
				{
					m_curOnDownloadSettings = SETTING(ON_DOWNLOAD_SETTING);
					if (m_curOnDownloadSettings == SettingsManager::ON_DOWNLOAD_REPLACE && BOOLSETTING(KEEP_FINISHED_FILES_OPTION))
						m_curOnDownloadSettings = SettingsManager::ON_DOWNLOAD_ASK;
						
					if (m_curOnDownloadSettings == SettingsManager::ON_DOWNLOAD_ASK)
						fire(QueueManagerListener::TryAdding(), target, aSize, l_existingFileSize, l_eistingFileTime, m_curOnDownloadSettings);
						
					switch (m_curOnDownloadSettings)
					{
							/* FLylinkDC Team TODO: IRainman: давайте копировать имеющийся файл в папку назначения? будем трафик экономить! p.s: см. выше. :)
							case SettingsManager::ON_DOWNLOAD_EXIST_FILE_TO_NEW_DEST:
							    ...
							    return;
							*/
						case SettingsManager::ON_DOWNLOAD_REPLACE:
							File::deleteFile(target); // Delete old file.  FlylinkDC Team TODO: recheck existing file to save traffic and download time.
							break;
						case SettingsManager::ON_DOWNLOAD_RENAME:
							target = Util::getFilenameForRenaming(target); // for safest: call Util::getFilenameForRenaming twice from gui and core.
							break;
						case SettingsManager::ON_DOWNLOAD_SKIP:
							return;
					}
				}
			}
			// [~] SSA - check file exist
			q = fileQueue.add(target, aSize, aFlags, QueueItem::DEFAULT, tempTarget, GET_TIME(), root);
			fire(QueueManagerListener::Added(), q);
		}
		else
		{
			if (q->getSize() != aSize)
			{
				throw QueueException(STRING(FILE_WITH_DIFFERENT_SIZE)); // [!] IRainman fix done: [4] https://www.box.net/shared/0ac062dcc56424091537
			}
			if (!(root == q->getTTH()))
			{
				throw QueueException(STRING(FILE_WITH_DIFFERENT_TTH));
			}
			
			// [!] IRainman fix.
			bool isFinished;
			{
				SharedLock l(QueueItem::cs); // [+]
				isFinished = q->isFinishedL();
			}
			if (isFinished)
			{
				throw QueueException(STRING(FILE_ALREADY_FINISHED));
			}
			// [~] IRainman fix.
			
			q->setFlag(aFlags);
		}
		if (aUser)
		{
			UniqueLock l(QueueItem::cs); // [+] IRainman fix.
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
			ConnectionManager::getInstance()->getDownloadConnection(aUser);
			
		// auto search, prevent DEADLOCK
		if (l_newItem && //[+] FlylinkDC++ Team: Если файл-лист, или запрос IP - то его не нужно искать.
		        BOOLSETTING(AUTO_SEARCH))
		{
			SearchManager::getInstance()->search(root.toBase32(), 0, SearchManager::TYPE_TTH, Search::SIZE_DONTCARE, "auto");
		}
	}
}

void QueueManager::readdAll(const QueueItemPtr& q) throw(QueueException) // [+] IRainman opt.
{
	UniqueLock l(QueueItem::cs);
	const auto badSources = q->getBadSourcesL();
	for (auto s = badSources.cbegin(); s != badSources.cend(); ++s)
	{
		const auto u = s->getUser();
		if (addSourceL(q, u, QueueItem::Source::FLAG_MASK))
			ConnectionManager::getInstance()->getDownloadConnection(u);
	}
}

void QueueManager::readd(const string& target, const UserPtr& aUser) throw(QueueException)
{
	bool wantConnection;
	{
		// [-] Lock l(cs); [-] IRainman fix.
		QueueItemPtr q = fileQueue.find(target);
		UniqueLock l(QueueItem::cs); // [+] IRainman fix.
		if (q && q->isBadSourceL(aUser))
		{
			wantConnection = addSourceL(q, aUser, QueueItem::Source::FLAG_MASK);
		}
		else
		{
			wantConnection = false;
		}
	}
	if (wantConnection && aUser->isOnline())
		ConnectionManager::getInstance()->getDownloadConnection(aUser);
}

void QueueManager::setDirty()
{
	if (!dirty)
	{
		dirty = true;
		lastSave = GET_TICK();
	}
}

string QueueManager::checkTarget(const string& aTarget, const int64_t aSize) throw(QueueException, FileException) //[!] FlylinkDC always checking file size: bool checkExistence changed to int64_t aSize
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
	
	const string target = Util::validateFileName(aTarget);
	
	if (aSize != -1) // [!] IRainman opt.
	{
		// Check that the file doesn't already exist...
		//[!] FlylinkDC always checking file size
		const int64_t sz = File::getSize(target);
		if (aSize <= sz) //[+] FlylinkDC && (aSize <= sz)
		{
			throw FileException(STRING(LARGER_TARGET_FILE_EXISTS));
		}
	}
	return target;
}

/** Add a source to an existing queue item */
bool QueueManager::addSourceL(const QueueItemPtr& qi, const UserPtr& aUser, Flags::MaskType addBad) throw(QueueException, FileException) // [!] IRainman fix.
{
	dcassert(aUser); // [!] IRainman fix: Unable to add a source if the user is empty! Check your code!
	/* [-] IRainman fix.
	if (!aUser) //[+]PPA http://flylinkdc.blogspot.com/2009/11/crash-strong-r3154.html
	    throw QueueException("[C++Bug] aUser = NULL! Target:" + Util::getFileName(qi->getTarget()));
	[-] */
	
	bool wantConnection;
	{
		// [-] UniqueLock l(QueueItem::cs); // [-] IRainman fix.
		
		wantConnection = (qi->getPriority() != QueueItem::PAUSED) && !userQueue.getRunningL(aUser);
		
		if (qi->isSourceL(aUser))
		{
			if (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
			{
				return wantConnection;
			}
			throw QueueException(STRING(DUPLICATE_SOURCE) + ": " + Util::getFileName(qi->getTarget()));
		}
		
		if (qi->isBadSourceExceptL(aUser, addBad))
		{
			throw QueueException(STRING(DUPLICATE_SOURCE) + ": " + Util::getFileName(qi->getTarget()));
		}
		
		qi->addSourceL(aUser);
		/*if(aUser.user->isSet(User::PASSIVE) && !ClientManager::getInstance()->isActive(aUser.hint)) {
		    qi->removeSource(aUser, QueueItem::Source::FLAG_PASSIVE);
		    wantConnection = false;
		} else */
		if (qi->isFinishedL())
		{
			wantConnection = false;
		}
		else
		{
			if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP)) // http://code.google.com/p/flylinkdc/issues/detail?id=1088
			{
				PLAY_SOUND(SOUND_SOURCEFILE);
			}
			userQueue.addL(qi, aUser);
		}
	}
	fire(QueueManagerListener::SourcesUpdated(), qi);
	setDirty();
	
	return wantConnection;
}

void QueueManager::addDirectory(const string& aDir, const UserPtr& aUser, const string& aTarget, QueueItem::Priority p /* = QueueItem::DEFAULT */) noexcept
{
	bool needList;
	{
		FastLock l(csDirectories);
		
		const auto dp = directories.equal_range(aUser);
		
		for (auto i = dp.first; i != dp.second; ++i)
		{
			if (stricmp(aTarget.c_str(), i->second->getName().c_str()) == 0)
				return;
		}
		
		// Unique directory, fine...
		directories.insert(make_pair(aUser, new DirectoryItem(aUser, aDir, aTarget, p)));
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

QueueItem::Priority QueueManager::hasDownload(const UserPtr& aUser) noexcept
{
	UniqueLock l(QueueItem::cs); // [!] IRainman fix.
	QueueItemPtr qi = userQueue.getNextL(aUser, QueueItem::LOWEST);
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
	if (!fileQueue.empty()) // [!] opt
	{
		// [-] Lock l(cs); [-]
		TTHMap l_tthMap;// tthMap.clear(); [!]
		buildMap(dl.getRoot(), l_tthMap); // [!]
		dcassert(!l_tthMap.empty());
		{
			UniqueLock l(QueueItem::cs);
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
			SharedLock ll(fileQueue.csFQ);
#endif
			// [~] IRainman fix.
			for (auto i = fileQueue.getQueueL().cbegin(); i != fileQueue.getQueueL().cend(); ++i)
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
		if (matches > 0)
		{
			ConnectionManager::getInstance()->getDownloadConnection(dl.getUser());
		}
	}
	return matches;
}

void QueueManager::FileListQueue::execute(const DirectoryListInfoPtr && list) // [+] IRainman fix: moved form MainFrame to core.
{
	if (File::isExist(list->file))
	{
		unique_ptr<DirectoryListing> dl(new DirectoryListing(list->m_hintedUser));
		try
		{
			dl->loadFile(list->file);
			ADLSearchManager::getInstance()->matchListing(*dl);
			ClientManager::getInstance()->checkCheating(list->m_hintedUser, dl.get());
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message(e.getError());
		}
	}
	delete list;
}

void QueueManager::ListMatcher::execute(const StringList && list) // [+] IRainman fix: moved form MainFrame to core.
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

void QueueManager::QueueManagerWaiter::execute(const WaiterFile && p_currentFile) // [+] IRainman: auto pausing running downloads before moving.
{
	const string& l_source = p_currentFile.getSource();
	const string& l_target = p_currentFile.getTarget();
	const QueueItem::Priority l_priority = p_currentFile.getPriority();
	
	auto qm = QueueManager::getInstance();
	qm->setAutoPriority(l_source, false); // [+] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1030
	qm->setPriority(l_source, QueueItem::PAUSED);
	QueueItemPtr qi = qm->fileQueue.find(l_source);
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
	const string target = Util::validateFileName(aTarget);
	if (aSource == target)
		return;
		
	// [+] IRainman: auto pausing running downloads before moving.
	QueueItemPtr qs = fileQueue.find(aSource);
	if (!qs)
		return;
		
	// Don't move file lists
	if (qs->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
		return;
		
	// Don't move running downloads
	if (qs->isRunningL())
	{
		waiter.move(aSource, target, qs->getPriority());
		return;
	}
	// [~] IRainman: auto pausing running downloads before moving.
	
	// [-] IRainman fix.
	// [-] bool delSource = false;
	
	// [-] Lock l(cs);
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
		QueueItemPtr qt = fileQueue.find(target);
		if (!qt || stricmp(aSource, target) == 0)
		{
			// Good, update the target and move in the queue...
			fire(QueueManagerListener::Moved(), qs, aSource);
			fileQueue.move(qs, target);
			//fire(QueueManagerListener::Added(), qs);// [-]IRainman
			setDirty();
		}
		else
		{
			// Don't move to target of different size
			if (qs->getSize() != qt->getSize() || qs->getTTH() != qt->getTTH())
				return; // TODO спросить юзера!
				
			{
				UniqueLock l(QueueItem::cs); // [+] IRainman fix.
				for (auto i = qs->getSourcesL().cbegin(); i != qs->getSourcesL().cend(); ++i)
				{
					try
					{
						addSourceL(qt, i->getUser(), QueueItem::Source::FLAG_MASK);
					}
					catch (const Exception&)
					{
					}
				}
			}
			// [-] delSource = true;
			remove(aSource); // [+]
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
	UniqueLock l(QueueItem::cs); // [!] IRainman fix.
	QueueItemPtr qi = userQueue.getNextL(aUser);
	if (!qi)
		return false;
		
	aTarget = qi->getTarget();
	aSize = qi->getSize();
	aFlags = qi->getFlags();
	
	return true;
}

uint8_t QueueManager::FileQueue::getMaxSegments(const uint64_t filesize)
{
	// [!] IRainman fix.
#ifdef _DEBUG
	return 255; // [!] IRainman stress test :)
#else
	if (BOOLSETTING(SEGMENTS_MANUAL))
		return min((uint8_t)SETTING(NUMBER_OF_SEGMENTS), (uint8_t)200);
	else
		return min(filesize / (50 * MIN_BLOCK_SIZE) + 2, 200Ui64);
#endif
	// [~] IRainman fix.
}

void QueueManager::getTargets(const TTHValue& tth, StringList& sl)
{
	// [-] Lock l(cs); [-] IRainman fix.
	QueueItemList ql;
	fileQueue.find(ql, tth);
	for (auto i = ql.cbegin(); i != ql.cend(); ++i)
	{
		sl.push_back((*i)->getTarget());
	}
}

Download* QueueManager::getDownload(UserConnection& aSource, string& aMessage) noexcept
{
	// [-] Lock l(cs); [-] IRainman fix.
	
	const UserPtr& u = aSource.getUser();
	dcdebug("Getting download for %s...", u->getCID().toBase32().c_str());
	Download* d;
	QueueItemPtr q;
	{
		UniqueLock l(QueueItem::cs); // [+] IRainman fix. // TOOD Dead lock [3] https://code.google.com/p/flylinkdc/issues/detail?id=1028
		
		q = userQueue.getNextL(u, QueueItem::LOWEST, aSource.getChunkSize(), aSource.getSpeed(), true);
		
		if (!q)
		{
			userQueue.getLastError(aMessage);
			dcdebug("none\n");
			return nullptr;
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
		
		d = new Download(aSource, q.get()); // [!] IRainman fix.
		
		userQueue.addDownloadL(q, d);
	}
	
	fire(QueueManagerListener::SourcesUpdated(), q);
	dcdebug("found %s\n", q->getTarget().c_str());
	return d;
}

namespace
{
class TreeOutputStream : public OutputStream
#ifdef _DEBUG
	, virtual NonDerivable<TreeOutputStream> // [+] IRainman fix.
#endif
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

void QueueManager::setFile(Download* d)
{
	if (d->getType() == Transfer::TYPE_FILE)
	{
		// [-] ReadLock lfq(fileQueue.csFQ); // [-] IRainman fix.
		
		QueueItemPtr qi = fileQueue.find(d->getPath());
		if (!qi)
		{
			throw QueueException(STRING(TARGET_REMOVED));
		}
		
		if (d->getOverlapped())
		{
			d->setOverlapped(false);
			
			bool found = false;
			// ok, we got a fast slot, so it's possible to disconnect original user now
			SharedLock l(QueueItem::cs); // [+] IRainman fix.
			for (auto i = qi->getDownloadsL().cbegin(); i != qi->getDownloadsL().cend(); ++i)
			{
				if ((*i) != d && (*i)->getSegment().contains(d->getSegment()))
				{
				
					// overlapping has no sense if segment is going to finish
					if ((*i)->getSecondsLeft() < 10)
						break;
						
					found = true;
					
					// disconnect slow chunk
					(*i)->getUserConnection().disconnect();
					break;
				}
			}
			
			if (!found)
			{
				// slow chunk already finished ???
				throw QueueException(STRING(DOWNLOAD_FINISHED_IDLE));
			}
		}
		
		const string& target = d->getDownloadTarget(); // [!] IRainman opt:  use const link here.
		
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
			File::ensureDirectory(target);
		}
		
		// open stream for both writing and reading, because UploadManager can request reading from it
		auto f = new SharedFileStream(target, File::RW, File::OPEN | File::CREATE | File::SHARED | File::NO_CACHE_HINT);
		
		// Only use antifrag if we don't have a previous non-antifrag part
		if (BOOLSETTING(ANTI_FRAG) && f->getSize() != qi->getSize())
		{
			f->setSize(d->getTigerTree().getFileSize());
		}
		
		f->setPos(d->getSegment().getStart());
		d->setFile(f);
	}
	else if (d->getType() == Transfer::TYPE_FULL_LIST)
	{
		{
			// [-] Lock l(cs); [-] IRainman fix.
			
			QueueItemPtr qi = fileQueue.find(d->getPath());
			if (!qi)
			{
				throw QueueException(STRING(TARGET_REMOVED));
			}
			
			// set filelist's size
			qi->setSize(d->getSize());
		}
		
		string target = d->getPath();
		File::ensureDirectory(target);
		
		if (d->isSet(Download::FLAG_XML_BZ_LIST))
		{
			target += ".xml.bz2";
		}
		else
		{
			target += ".xml";
		}
		d->setFile(new File(target, File::WRITE, File::OPEN | File::TRUNCATE | File::CREATE));
	}
	else if (d->getType() == Transfer::TYPE_PARTIAL_LIST)
	{
		// [!] IRainman fix. TODO
		d->setFile(new StringOutputStream(d->getPFS()));
		// d->setFile(new File(d->getPFS(), File::WRITE, File::OPEN | File::TRUNCATE | File::CREATE));
		// [~] IRainman fix
	}
	else if (d->getType() == Transfer::TYPE_TREE)
	{
		d->setFile(new TreeOutputStream(d->getTigerTree()));
	}
}

void QueueManager::moveFile(const string& source, const string& target)
{
	File::ensureDirectory(target);
	if (File::getSize(source) > MOVER_LIMIT)
	{
		mover.moveFile(source, target);
	}
	else
	{
		internal_moveFile(source, target);
	}
}

void QueueManager::internal_moveFile(const string& source, const string& target)
{
	try
	{
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		getInstance()->fire(QueueManagerListener::TryFileMoving(), target);
#endif
		File::renameFile(source, target);
		getInstance()->fire(QueueManagerListener::FileMoved(), target);
	}
	catch (const FileException& /*e1*/)
	{
		// Try to just rename it to the correct name at least
		const string newTarget = Util::getFilePath(source) + Util::getFileName(target);
		try
		{
			File::renameFile(source, newTarget);
			LogManager::getInstance()->message(source + ' ' + STRING(RENAMED_TO) + ' ' + newTarget);
		}
		catch (const FileException& e2)
		{
			LogManager::getInstance()->message(STRING(UNABLE_TO_RENAME) + ' ' + source + " -> NewTarget: " + newTarget + " Error = " + e2.getError());
		}
	}
}

void QueueManager::moveStuckFile(const QueueItemPtr& qi)
{
	moveFile(qi->getTempTarget(), qi->getTarget());
	
	{
		UniqueLock l(QueueItem::cs); // [+] IRainman fix.
		if (qi->isFinishedL())
		{
			userQueue.removeL(qi);
		}
	}
	
	const string target = qi->getTarget();
	
	if (!BOOLSETTING(KEEP_FINISHED_FILES_OPTION))
	{
		fire(QueueManagerListener::Removed(), qi);
		fileQueue.remove(qi);
	}
	else
	{
		{
			UniqueLock l(QueueItem::cs); // [+] IRainman fix.
			qi->addSegmentL(Segment(0, qi->getSize()));
		}
		fire(QueueManagerListener::StatusUpdated(), qi);
	}
	
	fire(QueueManagerListener::RecheckAlreadyFinished(), target);
}

void QueueManager::rechecked(const QueueItemPtr& qi)
{
	fire(QueueManagerListener::RecheckDone(), qi->getTarget());
	fire(QueueManagerListener::StatusUpdated(), qi);
	
	setDirty();
}

void QueueManager::putDownload(Download* aDownload, bool finished, bool reportFinish) noexcept
{
	UserList getConn;
	string fileName;
	const HintedUser& hintedUser = aDownload->getHintedUser();
	const UserPtr& user = aDownload->getUser();
	
	dcassert(user); // [!] IRainman fix: putDownload call with empty by the user can not because you can not even attempt to download with an empty user!
	
	Flags::MaskType flags = 0;
	bool downloadList = false;
	
	{
		// [-] UniqueLock l(QueueItem::cs); // [-] IRainman fix.
		
		OutputStream* l_file = aDownload->getFile();
		if (l_file != nullptr) // [+] IRainman fix.
		{
			delete l_file;
			aDownload->setFile(nullptr);
		}
		
		if (aDownload->getType() == Transfer::TYPE_PARTIAL_LIST)
		{
			QueueItemPtr q = fileQueue.find(aDownload->getPath());
			if (q)
			{
				if (!aDownload->getPFS().empty())
				{
					// [!] IRainman fix.
					bool fulldir = q->isSet(QueueItem::FLAG_MATCH_QUEUE);
					if (!fulldir)
					{
						fulldir = q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD);
						if (fulldir)
						{
							FastLock l(csDirectories);
							fulldir &= directories.find(aDownload->getUser()) != directories.end();
						}
					}
					if (fulldir)
						// [~] IRainman fix.
					{
						dcassert(finished);
						
						fileName = aDownload->getPFS();
						flags = (q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD) ? (QueueItem::FLAG_DIRECTORY_DOWNLOAD) : 0)
						        | (q->isSet(QueueItem::FLAG_MATCH_QUEUE) ? QueueItem::FLAG_MATCH_QUEUE : 0)
						        | QueueItem::FLAG_TEXT;
					}
					else
					{
						fire(QueueManagerListener::PartialList(), hintedUser, aDownload->getPFS());
					}
				}
				else
				{
					// partial filelist probably failed, redownload full list
					dcassert(!finished);
					
					downloadList = true;
					flags = q->getFlags() & ~QueueItem::FLAG_PARTIAL_LIST;
				}
				
				fire(QueueManagerListener::Removed(), q);
				
				{
					UniqueLock l(QueueItem::cs); // [+] IRainman fix.
					userQueue.removeL(q);
				}
				
				fileQueue.remove(q);
			}
		}
		else
		{
			QueueItemPtr q = fileQueue.find(aDownload->getPath());
			// 2012-04-29_13-38-26_IJU6HPQFFXSXSTA5V57FOJBYESKF4PIK4QY2FGA_E73C122F_crash-stack-r501-build-9869.dmp
			
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
				
				if (finished)
				{
					if (aDownload->getType() == Transfer::TYPE_TREE)
					{
						// Got a full tree, now add it to the HashManager
						dcassert(aDownload->getTreeValid());
						HashManager::getInstance()->addTree(aDownload->getTigerTree());
						
						{
							UniqueLock l(QueueItem::cs); // [+] IRainman fix.
							userQueue.removeDownloadL(q, aDownload->getUser()); // [!] IRainman fix.
						}
						
						fire(QueueManagerListener::StatusUpdated(), q);
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
								UniqueLock l(QueueItem::cs); // [+] IRainman fix.
								q->addSegmentL(Segment(0, q->getSize()));
							}
						}
						else if (aDownload->getType() == Transfer::TYPE_FILE)
						{
							aDownload->setOverlapped(false);
							
							{
								UniqueLock l(QueueItem::cs); // [+] IRainman fix.
								q->addSegmentL(aDownload->getSegment());
							}
						}
						
						const bool isFile = aDownload->getType() == Transfer::TYPE_FILE;
						bool isFinishedFile;
						if (isFile)
						{
							SharedLock l(QueueItem::cs); // [+] IRainman fix.
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
								UploadManager::getInstance()->abortUpload(q->getTempTarget());
								
								{
									UniqueLock l(QueueItem::cs); // [+] IRainman fix.
									// Disconnect all possible overlapped downloads
									for (auto i = q->getDownloadsL().cbegin(); i != q->getDownloadsL().cend(); ++i)
									{
										if (*i != aDownload)
											(*i)->getUserConnection().disconnect();
									}
								}
							}
							
							// Check if we need to move the file
							if (aDownload->getType() == Transfer::TYPE_FILE && !aDownload->getTempTarget().empty() && (stricmp(aDownload->getPath().c_str(), aDownload->getTempTarget().c_str()) != 0))
							{
								moveFile(aDownload->getTempTarget(), aDownload->getPath());
							}
							
							if (BOOLSETTING(LOG_DOWNLOADS) && (BOOLSETTING(LOG_FILELIST_TRANSFERS) || aDownload->getType() == Transfer::TYPE_FILE))
							{
								StringMap params;
								aDownload->getParams(aDownload->getUserConnection(), params);
								LOG(DOWNLOAD, params);
							}
							//[+]PPA
							if (!q->isSet(Download::FLAG_XML_BZ_LIST) && !q->isSet(Download::FLAG_USER_GET_IP) && !q->isSet(Download::FLAG_PARTIAL))
							{
								CFlylinkDBManager::getInstance()->push_download_tth(q->getTTH());
							}
							//[~]PPA
							
							fire(QueueManagerListener::Finished(), q, dir, aDownload);
							
							{
								UniqueLock l(QueueItem::cs); // [+] IRainman fix.
								if (q->isSet(QueueItem::FLAG_USER_LIST)) // [<-] IRainman fix: moved form MainFrame to core.
								{
									// [!] IRainman fix: please always match listing without hint!
									m_listQueue.addTask(new DirectoryListInfo(HintedUser(q->getDownloadsL().front()->getUser(), Util::emptyString), q->getListName(), dir, aDownload->getRunningAverage()));
								}
								userQueue.removeL(q);
							}
							// [+] IRainman dclst support
							if (q->isSet(QueueItem::FLAG_DCLST_LIST))
							{
								addDclst(q->getTarget());
							}
							// [~] IRainman dclst support
							
							if (!BOOLSETTING(KEEP_FINISHED_FILES_OPTION) || aDownload->getType() == Transfer::TYPE_FULL_LIST)
							{
								fire(QueueManagerListener::Removed(), q);
								fileQueue.remove(q);
							}
							else
							{
								fire(QueueManagerListener::StatusUpdated(), q);
							}
						}
						else
						{
							{
								UniqueLock l(QueueItem::cs); // [+] IRainman fix.
								userQueue.removeDownloadL(q, aDownload->getUser()); // [!] IRainman fix.
							}
							
							if (aDownload->getType() != Transfer::TYPE_FILE || (reportFinish && q->isWaitingL()))
							{
								fire(QueueManagerListener::StatusUpdated(), q);
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
							SharedLock l(QueueItem::cs); // [+] IRainman fix.
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
									UniqueLock l(QueueItem::cs); // [+] IRainman fix.
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
						UniqueLock l(QueueItem::cs); // [+] IRainman fix.
						userQueue.removeDownloadL(q, aDownload->getUser()); // [!] IRainman fix.
					}
					
					fire(QueueManagerListener::StatusUpdated(), q);
					
					if (aDownload->isSet(Download::FLAG_OVERLAP))
					{
						// overlapping segment disconnected, unoverlap original segment
						UniqueLock l(QueueItem::cs); // [+] IRainman fix.
						q->setOverlappedL(aDownload->getSegment(), false); // [!] IRainman fix.
					}
				}
			}
			else if (aDownload->getType() != Transfer::TYPE_TREE)
			{
				if (!aDownload->getTempTarget().empty() && (aDownload->getType() == Transfer::TYPE_FULL_LIST || aDownload->getTempTarget() != aDownload->getPath()))
				{
					File::deleteFile(aDownload->getTempTarget());
				}
			}
		}
		delete aDownload;
	}
	
	for (auto i = getConn.cbegin(); i != getConn.cend(); ++i)
	{
		ConnectionManager::getInstance()->getDownloadConnection(*i);
	}
	
	if (!fileName.empty())
	{
		processList(fileName, hintedUser, flags);
	}
	
	// partial file list failed, redownload full list
	if (user->isOnline() && downloadList)
	{
		try
		{
			addList(user, flags);
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
		LogManager::getInstance()->message(STRING(UNABLE_TO_OPEN_FILELIST) + ' ' + name);
		return;
	}
	// [+] IRainman fix.
	dcassert(dirList.getTotalFileCount());
	if (!dirList.getTotalFileCount())
	{
		LogManager::getInstance()->message(STRING(UNABLE_TO_OPEN_FILELIST) + " (dirList.getTotalFileCount() == 0) " + name);
		return;
	}
	// [~] IRainman fix.
	if (flags & QueueItem::FLAG_DIRECTORY_DOWNLOAD)
	{
		vector<DirectoryItemPtr> dl;
		{
			FastLock l(csDirectories);
			const auto dp = directories.equal_range(hintedUser.user) | map_values;
			dl.assign(boost::begin(dp), boost::end(dp));
			directories.erase(hintedUser.user);
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

void QueueManager::remove(const string& aTarget) noexcept
{
	UserList x;
	{
		// [-] Lock l(cs); [-] IRainman fix.
		
		QueueItemPtr q = fileQueue.find(aTarget);
		if (!q)
			return;
			
		if (q->isSet(QueueItem::FLAG_DIRECTORY_DOWNLOAD))
		{
			SharedLock ll(QueueItem::cs); // [+] IRainman fix.
			dcassert(q->getSourcesL().size() == 1);
			FastLock l(csDirectories); // [+] IRainman fix.
			for_each(directories.equal_range(q->getSourcesL()[0].getUser()) | map_values, DeleteFunction());
			directories.erase(q->getSourcesL()[0].getUser());
		}
		
		// For partial-share
		UploadManager::getInstance()->abortUpload(q->getTempTarget());
		
		if (q->isRunningL())
		{
			SharedLock l(QueueItem::cs); // [+] IRainman fix.
			auto d = q->getDownloadsL();
			for (auto i = d.cbegin(); i != d.cend(); ++i)
			{
				x.push_back((*i)->getUser());
			}
		}
		else if (!q->getTempTarget().empty() && q->getTempTarget() != q->getTarget())
		{
			File::deleteFile(q->getTempTarget());
		}
		
		fire(QueueManagerListener::Removed(), q);
		
		{
			UniqueLock l(QueueItem::cs); // [+] IRainman fix.
			if (!q->isFinishedL())
			{
				userQueue.removeL(q);
			}
		}
		fileQueue.remove(q);
		
		setDirty();
	}
	
	for (auto i = x.cbegin(); i != x.cend(); ++i)
	{
		ConnectionManager::getInstance()->disconnect(*i, true);
	}
}

void QueueManager::removeSource(const string& aTarget, const UserPtr& aUser, Flags::MaskType reason, bool removeConn /* = true */) noexcept
{
	bool isRunning = false;
	bool removeCompletely = false;
	do
	{
		QueueItemPtr q = fileQueue.find(aTarget);
		if (!q)
			return;
			
		UniqueLock l(QueueItem::cs); // [!] IRainman fix.
		
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
			q->getSourceL(aUser)->setFlag(reason);
			return;
		}
		
		if (q->isRunningL() && userQueue.getRunningL(aUser) == q)
		{
			isRunning = true;
			userQueue.removeDownloadL(q, aUser); // [!] IRainman fix.
			fire(QueueManagerListener::StatusUpdated(), q);
		}
		if (!q->isFinishedL())
		{
			userQueue.removeL(q, aUser);
		}
		q->removeSourceL(aUser, reason);
		
		fire(QueueManagerListener::SourcesUpdated(), q);
		setDirty();
	}
	while (false);
	if (isRunning && removeConn)
	{
		ConnectionManager::getInstance()->disconnect(aUser, true);
	}
	if (removeCompletely)
	{
		remove(aTarget);
	}
}

void QueueManager::removeSource(const UserPtr& aUser, Flags::MaskType reason) noexcept
{
	// @todo remove from finished items
	bool isRunning = false;
	string removeRunning;
	{
		// [!] IRainman fix.
		// [-] Lock l(cs);
		QueueItemPtr qi;
		UniqueLock l(QueueItem::cs);
		// [~] IRainman fix.
		while ((qi = userQueue.getNextL(aUser, QueueItem::PAUSED)) != nullptr)
		{
			if (qi->isSet(QueueItem::FLAG_USER_LIST))
			{
				remove(qi->getTarget());
			}
			else
			{
				userQueue.removeL(qi, aUser);
				qi->removeSourceL(aUser, reason);
				fire(QueueManagerListener::SourcesUpdated(), qi);
				setDirty();
			}
		}
		
		qi = userQueue.getRunningL(aUser);
		if (qi)
		{
			if (qi->isSet(QueueItem::FLAG_USER_LIST))
			{
				removeRunning = qi->getTarget();
			}
			else
			{
				userQueue.removeDownloadL(qi, aUser); // [!] IRainman fix.
				userQueue.removeL(qi, aUser);
				isRunning = true;
				qi->removeSourceL(aUser, reason);
				fire(QueueManagerListener::StatusUpdated(), qi);
				fire(QueueManagerListener::SourcesUpdated(), qi);
				setDirty();
			}
		}
	}
	
	if (isRunning)
	{
		ConnectionManager::getInstance()->disconnect(aUser, true);
	}
	if (!removeRunning.empty())
	{
		remove(removeRunning);
	}
}

void QueueManager::setPriority(const string& aTarget, QueueItem::Priority p) noexcept
{
	UserList getConn;
	bool running = false;
	
	{
		// [-] Lock l(cs); [-] IRainman fix.
		
		QueueItemPtr q = fileQueue.find(aTarget);
		if (q != nullptr && q->getPriority() != p)
		{
			bool isFinished;
			{
				SharedLock l(QueueItem::cs); // [+] IRainman fix.
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
				userQueue.setPriority(q, p);
				setDirty();
				fire(QueueManagerListener::StatusUpdated(), q);
			}
		}
	}
	
	if (p == QueueItem::PAUSED)
	{
		if (running)
			DownloadManager::getInstance()->abortDownload(aTarget);
	}
	else
	{
		for (auto i = getConn.cbegin(); i != getConn.cend(); ++i)
		{
			ConnectionManager::getInstance()->getDownloadConnection(*i);
		}
	}
}

void QueueManager::setAutoPriority(const string& aTarget, bool ap) noexcept
{
	vector<pair<string, QueueItem::Priority>> priorities;
	
	{
		// [-] Lock l(cs); [-] IRainman fix.
		
		QueueItemPtr q = fileQueue.find(aTarget);
		if (q && q->getAutoPriority() != ap)
		{
			q->setAutoPriority(ap);
			if (ap)
			{
				priorities.push_back(make_pair(q->getTarget(), q->calculateAutoPriority()));
			}
			setDirty();
			fire(QueueManagerListener::StatusUpdated(), q);
		}
	}
	
	for (auto p = priorities.cbegin(); p != priorities.cend(); ++p)
	{
		setPriority((*p).first, (*p).second);
	}
}

void QueueManager::saveQueue(bool force) noexcept
{
	if (!dirty && !force)
		return;
		
	//CFlyLog l_log("[Save queue to DB]");
	//l_log.step("save data to DB");
	// [-] Lock l(cs); [-] IRainman fix.
	{
		SharedLock lcs(QueueItem::cs); // TODO после исправления дедлока - убрать данную блокировку. https://code.google.com/p/flylinkdc/issues/detail?id=1028
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
		SharedLock lfq(fileQueue.csFQ); // [+] IRainman fix.
#endif
		for (auto i = fileQueue.getQueueL().begin(); i != fileQueue.getQueueL().end(); ++i)
		{
			auto& qi  = i->second;
			if (qi->isDirty())
			{
				if (!qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_USER_GET_IP))
				{
#ifdef _DEBUG
					const auto& l_first = i->first;
					LogManager::getInstance()->message("merge_queue_item(qi) getFlyQueueID = " + Util::toString(qi->getFlyQueueID()) + " *l_first = " + *l_first);
#endif
					if (CFlylinkDBManager::getInstance()->merge_queue_item(qi))
					{
						qi->setDirty(false);
					}
				}
			}
		}
	}
	// [-] dirty = false; [-] IRainman fix.
	if (exists_queueFile)
	{
		const auto l_queueFile = getQueueFile();
		File::deleteFile(l_queueFile);
		File::deleteFile(l_queueFile + ".bak");
		exists_queueFile = false;
	}
	// Put this here to avoid very many saves tries when disk is full...
	lastSave = GET_TICK();
	dirty = false; // [+] IRainman fix.
}

class QueueLoader : public SimpleXMLReader::CallBack
{
	public:
		QueueLoader() : cur(nullptr), inDownloads(false) { }
		~QueueLoader() { }
		void startTag(const string& name, StringPairList& attribs, bool simple);
		void endTag(const string& name, const string& data);
	private:
		string target;
		
		QueueItemPtr cur;
		bool inDownloads;
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
		exists_queueFile = false;
	}
	CFlylinkDBManager::getInstance()->load_queue();
	dirty = false;
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
	if (!inDownloads && name == "Downloads")
	{
		inDownloads = true;
	}
	else if (inDownloads)
	{
		if (cur == nullptr && name == sDownload)
		{
			int64_t size = Util::toInt64(getAttrib(attribs, sSize, 1));
			if (size == 0)
				return;
			try
			{
				const string& tgt = getAttrib(attribs, sTarget, 0);
				// @todo do something better about existing files
				target = QueueManager::checkTarget(tgt,  /*checkExistence*/ -1);// [!] IRainman fix. FlylinkDC use Size on 2nd parametr!
				if (target.empty())
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
				
			const string& tempTarget = getAttrib(attribs, sTempTarget, 5);
			uint8_t maxSegments = (uint8_t)Util::toInt(getAttrib(attribs, sMaxSegments, 5));
			int64_t downloaded = Util::toInt64(getAttrib(attribs, sDownloaded, 5));
			if (downloaded > size || downloaded < 0)
				downloaded = 0;
				
			if (added == 0)
				added = GET_TIME();
				
			QueueItemPtr qi = qm->fileQueue.find(target);
			
			if (qi == NULL)
			{
				qi = qm->fileQueue.add(target, size, 0, p, tempTarget, added, TTHValue(tthRoot));
				if (downloaded > 0)
				{
					{
						UniqueLock l(QueueItem::cs); // [+] IRainman fix.
						qi->addSegmentL(Segment(0, downloaded));
					}
					qi->setPriority(qi->calculateAutoPriority());
				}
				
				bool ap = Util::toInt(getAttrib(attribs, sAutoPriority, 6)) == 1;
				qi->setAutoPriority(ap);
				qi->setMaxSegments(max((uint8_t)1, maxSegments));
				
				qm->fire(QueueManagerListener::Added(), qi);
			}
			if (!simple)
				cur = qi;
		}
		else if (cur && name == sSegment)
		{
			int64_t start = Util::toInt64(getAttrib(attribs, sStart, 0));
			int64_t size = Util::toInt64(getAttrib(attribs, sSize, 1));
			
			if (size > 0 && start >= 0 && (start + size) <= cur->getSize())
			{
				{
					UniqueLock l(QueueItem::cs); // [+] IRainman fix.
					cur->addSegmentL(Segment(start, size));
				}
				cur->setPriority(cur->calculateAutoPriority());
			}
		}
		else if (cur && name == sSource)
		{
			const string& cid = getAttrib(attribs, sCID, 0);
			UserPtr user;
			if (cid.length() != 39)
			{
				user = ClientManager::getInstance()->getUser(getAttrib(attribs, sNick, 1), getAttrib(attribs, sHubHint, 1)
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
				                                             , 0
#endif
				                                            );
			}
			else
			{
				user = ClientManager::getInstance()->getUser(CID(cid));
			}
			
			bool wantConnection;
			try
			{
				UniqueLock l(QueueItem::cs); // [+] IRainman fix.
				wantConnection = qm->addSourceL(cur, user, 0) && user->isOnline();
			}
			catch (const Exception&)
			{
				return;
			}
			if (wantConnection)
			{
				ConnectionManager::getInstance()->getDownloadConnection(user);
			}
		}
	}
}

void QueueLoader::endTag(const string& name, const string&)
{
	if (inDownloads)
	{
		if (name == sDownload)
		{
			cur = nullptr;
		}
		else if (name == "Downloads")
		{
			inDownloads = false;
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
void QueueManager::on(SearchManagerListener::SR, const SearchResultPtr& sr) noexcept
{
	bool added = false;
	bool wantConnection = false;
	bool needsAutoMatch = false;
	
	{
		// Lock l(cs); [-] IRainman fix.
		QueueItemList matches;
		
		fileQueue.find(matches, sr->getTTH());
		
		if (!matches.empty()) // [+] IRainman opt.
		{
			UniqueLock l(QueueItem::cs); // [!] http://code.google.com/p/flylinkdc/issues/detail?id=1082
			for (auto i = matches.cbegin(); i != matches.cend(); ++i)
			{
				const QueueItemPtr& qi = *i;
				// Size compare to avoid popular spoof
				if (qi->getSize() == sr->getSize())
				{
					if (!qi->isSourceL(sr->getUser())) // [!] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=1227
					{
						if (qi->isFinishedL())
							break;  // don't add sources to already finished files
							
						try
						{
							needsAutoMatch = !qi->countOnlineUsersGreatOrEqualThanL(SETTING(MAX_AUTO_MATCH_SOURCES));
							
							// [-] if (!BOOLSETTING(AUTO_SEARCH_AUTO_MATCH) || (l_count_online_users >= (size_t)SETTING(MAX_AUTO_MATCH_SOURCES))) [-] IRainman fix.
							wantConnection = addSourceL(qi, HintedUser(sr->getUser(), sr->getHubURL()), 0);
							
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
				const string path = Util::getFilePath(sr->getFile());
				// [!] IRainman fix: please always match listing without hint! old code: sr->getHubURL().
				addList(sr->getUser(), QueueItem::FLAG_MATCH_QUEUE | (path.empty() ? 0 : QueueItem::FLAG_PARTIAL_LIST), path);
			}
			catch (const Exception&)
			{
				// ...
			}
		}
		if (wantConnection && sr->getUser()->isOnline())
		{
			ConnectionManager::getInstance()->getDownloadConnection(sr->getUser());
		}
	}
}

// ClientManagerListener
void QueueManager::on(ClientManagerListener::UserConnected, const UserPtr& aUser) noexcept
{
#ifdef IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
	bool hasDown = false;
	{
		SharedLock l(QueueItem::cs); // [!] IRainman fix.
		for (size_t i = 0; i < QueueItem::LAST; ++i)
		{
			const auto j = userQueue.getListL(i).find(aUser);
			if (j != userQueue.getListL(i).end())
			{
				fire(QueueManagerListener::StatusUpdatedList(), j->second); // [!] IRainman opt.
				if (i != QueueItem::PAUSED)
					hasDown = true;
			}
		}
	}
#else
	QueueItemList l_status_update_array;
	const bool hasDown = userQueue.userIsDownloadedFiles(aUser, l_status_update_array);
	fire(QueueManagerListener::StatusUpdatedList(), l_status_update_array); // [!] IRainman opt.
#endif // IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
	
	if (hasDown)
	{
		// the user just came on, so there's only 1 possible hub, no need for a hint
		ConnectionManager::getInstance()->getDownloadConnection(aUser);
	}
}

void QueueManager::on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept
{
#ifdef IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
	SharedLock l(QueueItem::cs); // [!] IRainman fix.
	for (size_t i = 0; i < QueueItem::LAST; ++i)
	{
		const auto j = userQueue.getListL(i).find(aUser);
		if (j != userQueue.getListL(i).end())
		{
			fire(QueueManagerListener::StatusUpdatedList(), j->second); // [!] IRainman opt.
		}
	}
#else
	QueueItemList l_status_update_array;
	userQueue.userIsDownloadedFiles(aUser, l_status_update_array);
	fire(QueueManagerListener::StatusUpdatedList(), l_status_update_array); // [!] IRainman opt.
#endif // IRAINMAN_NON_COPYABLE_USER_QUEUE_ON_USER_CONNECTED_OR_DISCONECTED
}

void QueueManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (dirty && ((lastSave + 30000) < aTick))
	{
#ifdef _DEBUG
		LogManager::getInstance()->message("[!-> [Start] saveQueue lastSave = " + Util::toString(lastSave) + " aTick = " + Util::toString(aTick));
#endif
		saveQueue();
	}
	
	QueueItem::PriorityArray l_priorities;
	
	{
		QueueItemList l_runningItems;
		// [-] Lock l(cs); [-] IRainman fix.
		calcPriorityAndGetRunningFiles(l_priorities, l_runningItems);
		if (!l_runningItems.empty())
		{
			// TODO - QueueItemList - fire with no lock. Probably needs lock csFQ here.
			fire(QueueManagerListener::Tick(), l_runningItems); // [!] IRainman opt.
		}
	}
	
	for (auto p = l_priorities.cbegin(); p != l_priorities.cend(); ++p)
	{
		setPriority(p->first, p->second);
	}
}

#ifdef PPA_INCLUDE_DROP_SLOW
bool QueueManager::dropSource(Download* d)
{
	uint8_t activeSegments = 0;
	bool   allowDropSource;
	uint64_t overallSpeed;
	{
		SharedLock l(QueueItem::cs); // [!] IRainman fix.
		
		QueueItemPtr q = userQueue.getRunningL(d->getUser());
		
		dcassert(q); // [+] IRainman fix.
		if (!q)
			return false;
			
		dcassert(q->isSourceL(d->getUser()));
		
		if (!q->isAutoDrop()) // [!] IRainman fix.
			return false;
			
		const auto& d = q->getDownloadsL();
		for (auto i = d.cbegin(); i != d.cend(); ++i)
		{
			if ((*i)->getStart() > 0)
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
				d->setFlag(Download::FLAG_SLOWUSER);
				
				if (d->getRunningAverage() < SETTING(REMOVE_SPEED) * 1024)
				{
					return true;
				}
				else
				{
					d->getUserConnection().disconnect();
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
		// [-] Lock l(cs); [-] IRainman fix.
		
		// Locate target QueueItem in download queue
#ifdef IRAINMAN_FASTS_QUEUE_MANAGER
		QueueItemPtr qi = fileQueue.find(tth);
		if (!qi)
			return false;
#else
		QueueItemList ql;
		fileQueue.find(ql, tth);
			
		if (ql.empty())
		{
			dcdebug("Not found in download queue\n");
			return false;
		}
			
		dcassert(ql.size() == 1); // [+] IRainman fix.
			
		QueueItemPtr qi = ql.front();
#endif // IRAINMAN_FASTS_QUEUE_MANAGER
			
		UniqueLock l(QueueItem::cs); // [+] IRainman fix.
		
		// don't add sources to finished files
		// this could happen when "Keep finished files in queue" is enabled
		if (qi->isFinishedL())
			return false;
			
		// Check min size
		if (qi->getSize() < PARTIAL_SHARE_MIN_SIZE)
		{
			dcassert(0);
			return false;
		}
		
		// Get my parts info
		const int64_t blockSize = qi->getBlockSize();
		dcassert(blockSize);
		// [-] IRainman fix.
		// [-] if (blockSize == 0)
		// [-]  blockSize = qi->getSize();
		
		qi->getPartialInfoL(outPartialInfo, blockSize);
		
		// Any parts for me?
		wantConnection = qi->isNeededPartL(partialSource.getPartialInfo(), blockSize);
		
		// If this user isn't a source and has no parts needed, ignore it
		QueueItem::SourceIter si = qi->getSourceL(aUser);
		if (si == qi->getSourcesL().end())
		{
			si = qi->getBadSourceL(aUser);
			
			if (si != qi->getBadSourcesL().end() && si->isSet(QueueItem::Source::FLAG_TTH_INCONSISTENCY))
				return false;
				
			if (!wantConnection)
			{
				if (si == qi->getBadSourcesL().end())
					return false;
			}
			else
			{
				// add this user as partial file sharing source
				qi->addSourceL(aUser);
				si = qi->getSourceL(aUser);
				si->setFlag(QueueItem::Source::FLAG_PARTIAL);
				
				QueueItem::PartialSource* ps = new QueueItem::PartialSource(partialSource.getMyNick(),
				                                                            partialSource.getHubIpPort(), partialSource.getIp(), partialSource.getUdpPort());
				si->setPartialSource(ps);
				
				userQueue.addL(qi, aUser);
				dcassert(si != qi->getSourcesL().end());
				fire(QueueManagerListener::SourcesUpdated(), qi);
			}
		}
		
		// Update source's parts info
		if (si->getPartialSource())
		{
			si->getPartialSource()->setPartialInfo(partialSource.getPartialInfo());
		}
	}
	
	// Connect to this user
	if (wantConnection)
		ConnectionManager::getInstance()->getDownloadConnection(aUser);
		
	return true;
}

bool QueueManager::handlePartialSearch(const TTHValue& tth, PartsInfo& _outPartsInfo)
{
	{
		// [-] Lock l(cs); [-] IRainman fix.
		
		// Locate target QueueItem in download queue
#ifdef IRAINMAN_FASTS_QUEUE_MANAGER
		QueueItemPtr qi = fileQueue.find(tth);
		if (!qi)
			return false;
#else
		QueueItemList ql;
		fileQueue.find(ql, tth);
			
		if (ql.empty())
		{
			return false;
		}
			
		QueueItemPtr qi = ql.front();
#endif // IRAINMAN_FASTS_QUEUE_MANAGER
		if (qi->getSize() < PARTIAL_SHARE_MIN_SIZE)
		{
			return false;
		}
		
		SharedLock l(QueueItem::cs); // [+] IRainman fix.
		
		// don't share when file does not exist
		if (!File::isExist(qi->isFinishedL() ? qi->getTarget() : qi->getTempTarget()))
			return false;
			
		const int64_t blockSize = qi->getBlockSize();
		dcassert(blockSize);
		// [-] IRainman fix.
		// [-] if (blockSize == 0)
		// [-]  blockSize = qi->getSize();
		
		qi->getPartialInfoL(_outPartsInfo, blockSize);
	}
	
	return !_outPartsInfo.empty();
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
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
	SharedLock l(csFQ); // [+] IRainman fix.
#endif
	
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
	{
		const auto& q = i->second;
		
		if (q->getSize() < PARTIAL_SHARE_MIN_SIZE) continue;
		
		// don't share when file does not exist
		if (!File::isExist(q->isFinishedL() ? q->getTarget() : q->getTempTarget()))
			continue;
			
		QueueItem::getPFSSourcesL(q, buffer, now);
//////////////////
#ifdef _DEBUG
		// После отладки убрать сарый вариант наполнения
		const QueueItem::SourceList& sources = q->getSourcesL();
		const QueueItem::SourceList& badSources = q->getBadSourcesL();
		for (auto j = sources.cbegin(); j != sources.cend(); ++j)
		{
			const auto &l_getPartialSource = j->getPartialSource(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the '(* j).getPartialSource()' expression repeatedly. queuemanager.cpp 2900
			if (j->isSet(QueueItem::Source::FLAG_PARTIAL) &&
			        l_getPartialSource->getNextQueryTime() <= now &&
			        l_getPartialSource->getPendingQueryCount() < 10 && l_getPartialSource->getUdpPort() > 0)
			{
				debug_buffer.insert(make_pair(l_getPartialSource->getNextQueryTime(), make_pair(j, q)));
			}
			
		}
		for (auto j = badSources.cbegin(); j != badSources.cend(); ++j)
		{
			const auto &l_getPartialSource = j->getPartialSource(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the '(* j).getPartialSource()' expression repeatedly. queuemanager.cpp 2900
			if ((*j).isSet(QueueItem::Source::FLAG_TTH_INCONSISTENCY) == false && j->isSet(QueueItem::Source::FLAG_PARTIAL) &&
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
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_QUEUE_MANAGER
	SharedLock ll(QueueItem::cs);
#endif
	SharedLock l(csFQ); // [+] IRainman fix.
	
	for (auto i = m_queue.cbegin(); i != m_queue.cend(); ++i)
	{
		const auto& qi = i->second;
		// [-] IRainman fix: not possible in normal state.
		// if (qi == nullptr)  // PVS-Studio V595 The pointer was utilized before it was verified against nullptr.
		// continue;
		// [~]
		
		if (!File::isExist(qi->isFinishedL() ? qi->getTarget() : qi->getTempTarget())) // don't share when file does not exist
			continue;
			
		if (qi->getPriority() > QueueItem::PAUSED && qi->getSize() >= PARTIAL_SHARE_MIN_SIZE && now >= qi->getNextPublishingTime())
		{
			if (cand == nullptr || cand->getNextPublishingTime() > qi->getNextPublishingTime() || (cand->getNextPublishingTime() == qi->getNextPublishingTime() && cand->getPriority() < qi->getPriority()))
			{
				if (qi->getDownloadedBytes() > qi->getBlockSize())
					cand = qi;
			}
		}
	}
	
	if (cand)
	{
		cand->setNextPublishingTime(now + PFS_REPUBLISH_TIME);      // one hour
		return new TTHValue(cand->getTTH());
	}
	
	return nullptr;
}
#endif

/**
 * @file
 * $Id: QueueManager.cpp 583 2011-12-18 13:06:31Z bigmuscle $
 */
