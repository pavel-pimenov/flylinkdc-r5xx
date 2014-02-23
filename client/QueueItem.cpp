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
#include "QueueItem.h"
#include "LogManager.h"
#include "HashManager.h"
#include "Download.h"
#include "File.h"
#include <boost/atomic.hpp>

#ifdef FLYLINKDC_USE_RWLOCK
std::unique_ptr<webrtc::RWLockWrapper> QueueItem::g_cs = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
#else
std::unique_ptr<CriticalSection> QueueItem::g_cs = std::unique_ptr<CriticalSection>(new CriticalSection);
#endif

QueueItem::QueueItem(const string& aTarget, int64_t aSize, Priority aPriority, Flags::MaskType aFlag,
                     time_t aAdded, const TTHValue& p_tth) :
	target(aTarget), maxSegments(1), fileBegin(0),
	size(aSize), priority(aPriority), added(aAdded),
	autoPriority(false), nextPublishingTime(0), flyQueueID(0), flyCountSourceInSQL(0),
	m_dirty(true),
	m_block_size(0),
	m_tthRoot(p_tth),
	m_downloadedBytes(0),
	m_averageSpeed(0)
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	, m_delegater(nullptr)
#endif
{
#ifdef _DEBUG
	LogManager::getInstance()->message("QueueItem::QueueItem aTarget = " + aTarget + " this = " + Util::toString(__int64(this)));
#endif
	inc();
	setFlags(aFlag);
#ifdef PPA_INCLUDE_DROP_SLOW
	if (BOOLSETTING(DISCONNECTING_ENABLE))
	{
		setFlag(FLAG_AUTODROP);
	}
#endif
}
QueueItem::~QueueItem()
{
#ifdef _DEBUG
	LogManager::getInstance()->message("[~~~~] QueueItem::~QueueItem aTarget = " + target + " this = " + Util::toString(__int64(this)));
#endif
}
//==========================================================================================
int16_t QueueItem::calcTransferFlag(bool& partial, bool& trusted, bool& untrusted, bool& tthcheck, bool& zdownload, bool& chunked, double& ratio) const
{
	int16_t segs = 0;
	RLock l(*QueueItem::g_cs);
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		const Download *d = i->second;
		if (d->getStart() > 0) // crash http://code.google.com/p/flylinkdc/issues/detail?id=1361
		{
			segs++;
			
			if (d->isSet(Download::FLAG_PARTIAL))
			{
				partial = true;
			}
			if (d->m_isSecure)
			{
				if (d->m_isTrusted)
				{
					trusted = true;
				}
				else
				{
					untrusted = true;
				}
			}
			if (d->isSet(Download::FLAG_TTH_CHECK))
			{
				tthcheck = true;
			}
			if (d->isSet(Download::FLAG_ZDOWNLOAD))
			{
				zdownload = true;
			}
			if (d->isSet(Download::FLAG_CHUNKED))
			{
				chunked = true;
			}
			ratio += d->getPos() > 0 ? static_cast<double>(d->getActual()) / static_cast<double>(d->getPos()) : 1.00;
		}
	}
	return segs;
}
//==========================================================================================
QueueItem::Priority QueueItem::calculateAutoPriority() const
{
	if (autoPriority)
	{
		QueueItem::Priority p;
		const int percent = static_cast<int>(getDownloadedBytes() * 10 / size);
		switch (percent)
		{
			case 0:
			case 1:
			case 2:
				p = QueueItem::LOW;
				break;
			case 3:
			case 4:
			case 5:
			default:
				p = QueueItem::NORMAL;
				break;
			case 6:
			case 7:
			case 8:
				p = QueueItem::HIGH;
				break;
			case 9:
			case 10:
				p = QueueItem::HIGHEST;
				break;
		}
		return p;
	}
	return priority;
}
//==========================================================================================
static string getTempName(const string& aFileName, const TTHValue& aRoot)
{
	string tmp = aFileName + '.' + aRoot.toBase32() + '.' + TEMP_EXTENSION;
	return tmp;
}
//==========================================================================================
void QueueItem::calcBlockSize()
{
	m_block_size = CFlylinkDBManager::getInstance()->getBlockSizeSQL(getTTH(), getSize());
	dcassert(m_block_size);
	
#ifdef _DEBUG
	static boost::atomic_uint g_count(0);
	dcdebug("QueueItem::getBlockSize() TTH = %s [count = %d]\n", getTTH().toBase32().c_str(), int(++g_count));
#endif
}

size_t QueueItem::countOnlineUsersL() const
{
	size_t l_count = 0;
	for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
	{
		if (i->first->isOnline()) // [!] IRainman fix done [3] https://www.box.net/shared/7b99196ed232f2aaa28c  https://www.box.net/shared/0006cf0ff4dcec643530
			l_count++;
	}
	return l_count;
}

bool QueueItem::countOnlineUsersGreatOrEqualThanL(const size_t maxValue) const // [+] FlylinkDC++ opt.
{
	if (m_sources.size() < maxValue)
	{
		return false;
	}
	size_t count = 0;
	for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
	{
		if (i->first->isOnline())
		{
			if (++count == maxValue)
			{
				return true;
			}
		}
		/* TODO: needs?
		else if (maxValue - count > static_cast<size_t>(sources.cend() - i))
		{
		    return false;
		}
		/*/
	}
	return false;
}

void QueueItem::getOnlineUsers(UserList& list) const
{
	RLock l(*QueueItem::g_cs); // [+] IRainman fix.
	for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
	{
		if (i->first->isOnline())
		{
			list.push_back(i->first);
		}
	}
}

void QueueItem::addSourceL(const UserPtr& aUser)
{
	dcassert(!isSourceL(aUser));
	SourceIter i = getBadSourceL(aUser);
	if (i != m_badSources.end())
	{
		m_sources.insert(*i);
		m_badSources.erase(i->first);
	}
	else
	{
		m_sources.insert(std::make_pair(aUser, Source()));
	}
}
// [+] fix ? http://code.google.com/p/flylinkdc/issues/detail?id=1236 .
void QueueItem::getPFSSourcesL(const QueueItemPtr& p_qi, SourceListBuffer& p_sourceList, uint64_t p_now)
{
	auto addToList = [&](const bool isBadSourses) -> void
	{
		const auto& sources = isBadSourses ? p_qi->getBadSourcesL() : p_qi->getSourcesL();
		for (auto j = sources.cbegin(); j != sources.cend(); ++j)
		{
			const auto &l_ps = j->second.getPartialSource();
			if (j->second.isCandidate(isBadSourses) && l_ps->isCandidate(p_now))
			{
				p_sourceList.insert(make_pair(l_ps->getNextQueryTime(), make_pair(j, p_qi)));
			}
		}
	};
	
	addToList(false);
	addToList(true);
}
// [~] fix.

bool QueueItem::isChunkDownloadedL(int64_t startPos, int64_t& len) const
{
	if (len <= 0)
		return false;
		
	for (auto i = done.cbegin(); i != done.cend(); ++i)
	{
		const int64_t& start = i->getStart();
		const int64_t& end   = i->getEnd();
		if (start <= startPos && startPos < end)
		{
			len = min(len, end - startPos);
			return true;
		}
	}
	return false;
}

void QueueItem::removeSourceL(const UserPtr& aUser, Flags::MaskType reason)
{
	SourceIter i = getSourceL(aUser); // crash - https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=42877 && http://www.flickr.com/photos/96019675@N02/10488126423/
	dcassert(i != m_sources.end());
	// [-] IRainman fix: is not possible in normal state! Please don't problem maskerate.
//	if (i != m_sources.end()) //[+]PPA
//	{
	i->second.setFlag(reason);
	m_badSources.insert(*i);
	m_sources.erase(i);
//	}
//	else
//	{
//		LogManager::getInstance()->message("Error QueueItem::removeSourceL [i != m_sources.end()] aUser = [" +
//		                                   aUser->getLastNick() + "] Please send a text or a screenshot of the error to developers ppa74@ya.ru");
//	}
}
string QueueItem::getListName() const
{
	dcassert(isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST));
	if (isSet(QueueItem::FLAG_XML_BZLIST))
	{
		return getTarget() + ".xml.bz2";
	}
	else if (isSet(QueueItem::FLAG_DCLST_LIST))
	{
		return getTarget();
	}
	else
	{
		return getTarget() + ".xml";
	}
}

const string& QueueItem::getTempTarget()
{
	if (!isSet(QueueItem::FLAG_USER_LIST) && m_tempTarget.empty())
	{
		if (!SETTING(TEMP_DOWNLOAD_DIRECTORY).empty() && (File::getSize(getTarget()) == -1))
		{
			::StringMap sm;
			if (target.length() >= 3 && target[1] == ':' && target[2] == '\\')
				sm["targetdrive"] = target.substr(0, 3);
			else
				sm["targetdrive"] = Util::getLocalPath().substr(0, 3);
				
			setTempTarget(Util::formatParams(SETTING(TEMP_DOWNLOAD_DIRECTORY), sm, false) + getTempName(getTargetFileName(), getTTH()));
		}
		if (SETTING(TEMP_DOWNLOAD_DIRECTORY).empty())
			setTempTarget(target.substr(0, target.length() - getTargetFileName().length()) + getTempName(getTargetFileName(), getTTH()));
	}
	return m_tempTarget;
}
#ifdef _DEBUG
bool QueueItem::isSourceValid(const QueueItem::Source* p_source_ptr)
{
	RLock l(*g_cs);
	for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
	{
		if (p_source_ptr == &i->second)
			return true;
	}
	return false;
}
#endif
void QueueItem::addDownloadL(Download* p_download)
{
	dcassert(p_download->getUser());
	dcassert(m_downloads.find(p_download->getUser()) == m_downloads.end());
	m_downloads.insert(std::make_pair(p_download->getUser(), p_download));
}

bool QueueItem::removeDownloadL(const UserPtr& p_user)
{
	//dcassert(m_downloads.find(p_user) != m_downloads.end());
	const auto l_size_before = m_downloads.size();
	m_downloads.erase(p_user);
	dcassert(l_size_before != m_downloads.size());
	return l_size_before != m_downloads.size();
}
Segment QueueItem::getNextSegmentL(const int64_t  blockSize, const int64_t wantedSize, const int64_t lastSpeed, const PartialSource::Ptr &partialSource) const
{
	if (getSize() == -1 || blockSize == 0)
	{
		return Segment(0, -1);
	}
	
	if (!BOOLSETTING(MULTI_CHUNK))
	{
		if (!m_downloads.empty())
		{
			return Segment(-1, 0);
		}
		
		int64_t start = 0;
		int64_t end = getSize();
		
		if (!done.empty())
		{
			const Segment& first = *done.begin();
			
			if (first.getStart() > 0)
			{
				end = Util::roundUp(first.getStart(), blockSize);
			}
			else
			{
				start = Util::roundDown(first.getEnd(), blockSize);
				
				if (done.size() > 1)
				{
					const Segment& second = *(++done.begin());
					end = Util::roundUp(second.getStart(), blockSize);
				}
			}
		}
		
		return Segment(start, std::min(getSize(), end) - start);
	}
	
	if (m_downloads.size() >= maxSegments ||
	        (BOOLSETTING(DONT_BEGIN_SEGMENT) && static_cast<size_t>(SETTING(DONT_BEGIN_SEGMENT_SPEED) * 1024) < getAverageSpeed()))
	{
		// no other segments if we have reached the speed or segment limit
		return Segment(-1, 0);
	}
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	if (m_delegater != nullptr)
	{
		vector<int64_t> delegatesItemsAll;
		const size_t countItems = m_delegater->getDownloadItems(blockSize, delegatesItemsAll);
		if (countItems > 0)
		{
			size_t currentCheckItem = 0;
			bool overlaps = true;
			while (overlaps && currentCheckItem < countItems)
			{
				// Check overlapped
				int64_t startPreviewPosition = delegatesItemsAll[currentCheckItem];
				int64_t endPreviewPosition = startPreviewPosition + blockSize;
				Segment block(startPreviewPosition, blockSize);
				overlaps = false;
				for (auto i = done.cbegin(); !overlaps && i != done.cend(); ++i)
				{
					int64_t dstart = i->getStart();
					int64_t dend = i->getEnd();
					// We accept partial overlaps, only consider the block done if it is fully consumed by the done block
					if (dstart <= startPreviewPosition && dend >= endPreviewPosition)
					{
						overlaps = true;
					}
				}
				if (!overlaps)
				{
					for (auto i = m_downloads.cbegin(); !overlaps && i != m_downloads.cend(); ++i)
					{
						overlaps = block.overlaps(i->second->getSegment());
					}
					
				}
				if (!overlaps)
					break;
					
				currentCheckItem++;
			}
			if (!overlaps)
			{
				const int64_t startPreviewPosition = delegatesItemsAll[currentCheckItem];
				m_delegater->setDownloadItem(delegatesItemsAll[currentCheckItem], blockSize);
				return Segment(startPreviewPosition, blockSize);
			}
		}
		
	}
#endif // SSA_VIDEO_PREVIEW_FEATURE
	
	/* added for PFS */
	vector<int64_t> posArray;
	vector<Segment> neededParts;
	
	if (partialSource)
	{
		posArray.reserve(partialSource->getPartialInfo().size());
		
		// Convert block index to file position
		for (auto i = partialSource->getPartialInfo().cbegin(); i != partialSource->getPartialInfo().cend(); ++i)
			posArray.push_back(min(getSize(), (int64_t)(*i) * blockSize));
	}
	
	/***************************/
	
	const double donePart = static_cast<double>(calcAverageSpeedAndCalcAndGetDownloadedBytesL()) / getSize();
	
	// We want smaller blocks at the end of the transfer, squaring gives a nice curve...
	int64_t targetSize = static_cast<int64_t>(static_cast<double>(wantedSize) * std::max(0.25, (1. - (donePart * donePart))));
	
	if (targetSize > blockSize)
	{
		// Round off to nearest block size
		targetSize = Util::roundDown(targetSize, blockSize);
	}
	else
	{
		targetSize = blockSize;
	}
	
	int64_t start = 0;
	int64_t curSize = targetSize;
	
	while (start < getSize())
	{
		int64_t end = std::min(getSize(), start + curSize);
		Segment block(start, end - start);
		bool overlaps = false;
		for (auto i = done.cbegin(); !overlaps && i != done.cend(); ++i)
		{
			if (curSize <= blockSize)
			{
				int64_t dstart = i->getStart();
				int64_t dend = i->getEnd();
				// We accept partial overlaps, only consider the block done if it is fully consumed by the done block
				if (dstart <= start && dend >= end)
				{
					overlaps = true;
				}
			}
			else
			{
				overlaps = block.overlaps(*i);
			}
		}
		
		for (auto i = m_downloads.cbegin(); !overlaps && i != m_downloads.cend(); ++i)
		{
			overlaps = block.overlaps(i->second->getSegment());
		}
		
		if (!overlaps)
		{
			if (partialSource)
			{
				// store all chunks we could need
				for (auto j = posArray.cbegin(); j < posArray.cend(); j += 2)
				{
					if ((*j <= start && start < * (j + 1)) || (start <= *j && *j < end))
					{
						int64_t b = max(start, *j);
						int64_t e = min(end, *(j + 1));
						
						// segment must be blockSize aligned
						dcassert(b % blockSize == 0);
						dcassert(e % blockSize == 0 || e == getSize());
						
						neededParts.push_back(Segment(b, e - b));
					}
				}
			}
			else
			{
				return block;
			}
		}
		
		if (overlaps && (curSize > blockSize))
		{
			curSize -= blockSize;
		}
		else
		{
			start = end;
			curSize = targetSize;
		}
	}
	
	if (!neededParts.empty())
	{
		// select random chunk for download
		dcdebug("Found partial chunks: %d\n", neededParts.size());
		
		Segment& selected = neededParts[Util::rand(0, static_cast<uint32_t>(neededParts.size()))];
		selected.setSize(std::min(selected.getSize(), targetSize)); // request only wanted size
		
		return selected;
	}
	
	if (partialSource == NULL && BOOLSETTING(OVERLAP_CHUNKS) && lastSpeed > 10 * 1024)
	{
		// overlap slow running chunk
		
		const uint64_t l_CurrentTick = GET_TICK();//[+]IRainman refactoring transfer mechanism
		for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
		{
			const Download* d = i->second;
			
			// current chunk mustn't be already overlapped
			if (d->getOverlapped())
				continue;
				
			// current chunk must be running at least for 2 seconds
			if (d->getStart() == 0 || l_CurrentTick - d->getStart() < 2000)//[!]IRainman refactoring transfer mechanism
				continue;
				
			// current chunk mustn't be finished in next 10 seconds
			if (d->getSecondsLeft() < 10)
				continue;
				
			// overlap current chunk at last block boundary
			int64_t pos = d->getPos() - (d->getPos() % blockSize);
			int64_t size = d->getSize() - pos;
			
			// new user should finish this chunk more than 2x faster
			int64_t newChunkLeft = size / lastSpeed;
			if (2 * newChunkLeft < d->getSecondsLeft())
			{
				dcdebug("Overlapping... old user: %I64d s, new user: %I64d s\n", d->getSecondsLeft(), newChunkLeft);
				return Segment(d->getStartPos() + pos, size, true);
			}
		}
	}
	
	return Segment(0, 0);
}

void QueueItem::setOverlappedL(const Segment& p_segment, const bool p_isOverlapped) // [!] IRainman fix.
{
	// set overlapped flag to original segment
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		Download* d = i->second;
		if (d->getSegment().contains(p_segment))
		{
			d->setOverlapped(p_isOverlapped);
			break;
		}
	}
}

uint64_t QueueItem::calcAverageSpeedAndCalcAndGetDownloadedBytesL() const // [!] IRainman opt.
{
	uint64_t l_totalDownloaded = 0;
	uint64_t l_totalSpeed = 0; // Скорость 64 битная нужна?
	// count done segments
	for (auto i = done.cbegin(); i != done.cend(); ++i)
	{
		l_totalDownloaded += i->getSize();
	}
	// count running segments
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		const Download* d = i->second;
		l_totalDownloaded += d->getPos(); // [!] IRainman fix done: [6] https://www.box.net/shared/bcc1e978be39a1e0cbf6
		l_totalSpeed += d->getRunningAverage();
	}
	/*
	#ifdef _DEBUG
	static boost::atomic_uint l_count(0);
	dcdebug("QueueItem::calcAverageSpeedAndDownloadedBytes() total_download = %I64u, totalSpeed = %I64u [count = %d] [done.size() = %u] [downloads.size() = %u]\n",
	        l_totalDownloaded, l_totalSpeed,
	        int(++l_count), done.size(), m_downloads.size());
	#endif
	*/
	m_averageSpeed    = l_totalSpeed;
	m_downloadedBytes = l_totalDownloaded;
	return m_downloadedBytes;
}

void QueueItem::addSegmentL(const Segment& segment)
{
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	if (m_delegater != nullptr)
		m_delegater->addSegment(segment.getStart(), segment.getSize());
#endif
	dcassert(segment.getOverlapped() == false);
	done.insert(segment);
#ifdef _DEBUG
	LogManager::getInstance()->message("QueueItem::addSegmentL, setDirty = true! id = " +
	                                   Util::toString(this->getFlyQueueID()) + " target = " + this->getTarget()
	                                   + " TempTarget = " + this->getTempTarget()
	                                   + " segment.getSize() = " + Util::toString(segment.getSize())
	                                   + " segment.getEnd() = " + Util::toString(segment.getEnd())
	                                  );
#endif
	// Consolidate segments
	if (done.size() == 1)
		return;
	setDirty();
	
	for (auto i = ++done.cbegin() ; i != done.cend();)
	{
		SegmentSet::iterator prev = i;
		--prev;
		if (prev->getEnd() >= i->getStart())
		{
			Segment big(prev->getStart(), i->getEnd() - prev->getStart());
			done.erase(prev);
			done.erase(i++);
			done.insert(big);
		}
		else
		{
			++i;
		}
	}
}

bool QueueItem::isNeededPartL(const PartsInfo& partsInfo, int64_t p_blockSize)
{
	dcassert(partsInfo.size() % 2 == 0);
	
	auto i  = done.begin();
	for (auto j = partsInfo.cbegin(); j != partsInfo.cend(); j += 2)
	{
		while (i != done.end() && (*i).getEnd() <= (*j) * p_blockSize)
			++i;
			
		if (i == done.end() || !((*i).getStart() <= (*j) * p_blockSize && (*i).getEnd() >= (*(j + 1)) * p_blockSize))
			return true;
	}
	
	return false;
}

void QueueItem::getPartialInfoL(PartsInfo& p_partialInfo, uint64_t p_blockSize) const
{
	dcassert(p_blockSize);
	if (p_blockSize == 0) // https://crash-server.com/DumpGroup.aspx?ClientID=ppa&DumpGroupID=31115
		return;
		
	const size_t maxSize = min(done.size() * 2, (size_t)510);
	p_partialInfo.reserve(maxSize);
	
	for (auto i = done.cbegin(); i != done.cend() && p_partialInfo.size() < maxSize; ++i)
	{
	
		uint16_t s = (uint16_t)(i->getStart() / p_blockSize);
		uint16_t e = (uint16_t)((i->getEnd() - 1) / p_blockSize + 1);
		
		p_partialInfo.push_back(s);
		p_partialInfo.push_back(e);
	}
}
/* [-] IRainman fix.
    void QueueItem::getChunksVisualisation(const int type, vector<Segment>& p_segments) const   // type: 0 - downloaded bytes, 1 - running chunks, 2 - done chunks
    {
        p_segments.clear();
        switch (type)
        {
            case 0:
                if (downloads.size())
                    p_segments.reserve(downloads.size()); // 2012-05-03_22-05-14_U4TZSIYHFC32XEVTFRRT6A5T746QRL3VBVRZ6OY_6B53F172_crash-stack-r502-beta24-x64-build-9900.dmp
                for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
                    // 2012-04-23_22-36-14_X3Q2BSS4NNE6QDKD5RNWCORZL2KFQOXQL4VXCRI_049FD0A1_crash-stack-r501-x64-build-9812.dmp
                    // 2012-05-03_22-00-59_NAYCMEFHSDE42EDMGZCWHXJC4A4OGLBHWPGV2PQ_BB76317B_crash-stack-r502-beta24-build-9900.dmp
                    // _Xlength_error 2012-04-29_13-46-19_NIYJPY3EGH5A52BBBL7EXAFJSHKQMHNCLAGJXFI_3B603F6D_crash-stack-r501-x64-build-9869.dmp
                    // no_mem  2012-04-29_13-46-19_DSYQLU4LP7CX6GDDAJ2UFRPXE3X3PHVA4OOCMGQ_F85B2675_crash-stack-r501-x64-build-9869.dmp
                    // 2012-05-11_23-57-17_U4TZSIYHFC32XEVTFRRT6A5T746QRL3VBVRZ6OY_07BE60FA_crash-stack-r502-beta26-x64-build-9946.dmp
                    // 2012-05-11_23-53-01_PODAVNOJYU3ELXDKZYXRGXDKKYFAE5Z6JB4FQMY_A49C5274_crash-stack-r502-beta26-build-9946.dmp
                    // 2012-06-09_18-19-42_WXKSUZGUF5NOB2HT6HGGWK5OJNXJNY77A4Q2KHY_27F77588_crash-stack-r501-x64-build-10294.dmp
                {
                    p_segments.push_back((*i)->getSegment());
                }
                break;
            case 1:
                p_segments.reserve(downloads.size());
                for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
                {
                    p_segments.push_back(Segment((*i)->getStartPos(), (*i)->getPos()));
                }
                break;
            case 2:
                p_segments.reserve(done.size());
                for (auto i = done.cbegin(); i != done.cend(); ++i)
                {
                    p_segments.push_back(*i);
                }
                break;
        }
    }
*/
// [+] IRainman fix.
void QueueItem::getChunksVisualisation(vector<pair<Segment, Segment>>& p_runnigChunksAndDownloadBytes, vector<Segment>& p_doneChunks) const
{
	RLock l(*QueueItem::g_cs); // [+] IRainman fix.
	
	p_runnigChunksAndDownloadBytes.reserve(m_downloads.size()); // [!] IRainman fix done: [9] https://www.box.net/shared/9ccc91535264c1609a1e
	// m_downloads.size() для list - дорого!
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		p_runnigChunksAndDownloadBytes.push_back(make_pair(i->second->getSegment(), Segment(i->second->getStartPos(), i->second->getPos()))); // https://www.box.net/shared/1004787fe85503e7d4d9
	}
	p_doneChunks.reserve(done.size());
	for (auto i = done.cbegin(); i != done.cend(); ++i)
	{
		p_doneChunks.push_back(*i);
	}
}

// [~] IRainman fix.
