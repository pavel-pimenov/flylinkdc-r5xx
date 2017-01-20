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
#include "CFlylinkDBManager.h"
#include "ClientManager.h"
#include "UserConnection.h"
#include "../FlyFeatures/flyServer.h"

#ifdef FLYLINKDC_USE_RWLOCK
std::unique_ptr<webrtc::RWLockWrapper> QueueItem::g_cs = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
#else
std::unique_ptr<CriticalSection> QueueItem::g_cs = std::unique_ptr<CriticalSection>(new CriticalSection);
#endif

const string g_dc_temp_extension = "dctmp";

QueueItem::QueueItem(const string& aTarget, int64_t aSize, Priority aPriority, bool aAutoPriority, Flags::MaskType aFlag,
                     time_t aAdded, const TTHValue& p_tth, uint8_t p_maxSegments, int64_t p_FlyQueueID, const string& aTempTarget) :
	m_Target(aTarget),
	m_tempTarget(aTempTarget),
	m_maxSegments(std::max(uint8_t(1), p_maxSegments)),
	timeFileBegin(0),
	m_Size(aSize),
	m_priority(aPriority),
	added(aAdded),
	m_AutoPriority(aAutoPriority),
	nextPublishingTime(0),
	flyQueueID(p_FlyQueueID),
	m_dirty_base(false),
	m_dirty_source(false),
	m_dirty_segment(false),
//	m_is_failed(false),
	m_block_size(0),
	m_tthRoot(p_tth),
	m_downloadedBytes(0),
	lastsize(0),
	m_averageSpeed(0),
	m_diry_sources(0),
	m_last_count_online_sources(0)
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	, m_delegater(nullptr)
#endif
{
	if (p_FlyQueueID == 0)
	{
		m_dirty_base = true;
	}
#ifdef _DEBUG
	//LogManager::message("QueueItem::QueueItem aTarget = " + aTarget + " this = " + Util::toString(__int64(this)));
#endif
	setFlags(aFlag);
#ifdef FLYLINKDC_USE_DROP_SLOW
	if (BOOLSETTING(DISCONNECTING_ENABLE))
	{
		setFlag(FLAG_AUTODROP);
	}
#endif
}
QueueItem::~QueueItem()
{
#ifdef _DEBUG
	//LogManager::message("[~~~~] QueueItem::~QueueItem aTarget = " + target + " this = " + Util::toString(__int64(this)));
#endif
}
//==========================================================================================
int16_t QueueItem::calcTransferFlag(bool& partial, bool& trusted, bool& untrusted, bool& tthcheck, bool& zdownload, bool& chunked, double& ratio) const
{
	int16_t segs = 0;
	CFlyFastLock(m_fcs_download);
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		const auto d = i->second;
		if (d->getStart() > 0)
		{
			segs++;
			
			if (d->isSet(Download::FLAG_DOWNLOAD_PARTIAL))
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
	if (getAutoPriority())
	{
		QueueItem::Priority p;
		const int percent = static_cast<int>(getDownloadedBytes() * 10 / getSize());
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
	return getPriority();
}
//==========================================================================================
static string getDCTempName(const string& aFileName, const TTHValue& aRoot)
{
	string l_temp_name = aFileName;
	Util::fixFileNameMaxPathLimit(l_temp_name);
	l_temp_name = l_temp_name + '.' + aRoot.toBase32() + '.' + g_dc_temp_extension;
	return l_temp_name;
}
//==========================================================================================
void QueueItem::calcBlockSize()
{
	m_block_size = CFlylinkDBManager::getInstance()->get_block_size_sql(getTTH(), getSize());
	dcassert(m_block_size);
	
#ifdef _DEBUG
	static boost::atomic_uint g_count(0);
	dcdebug("QueueItem::getBlockSize() TTH = %s [count = %d]\n", getTTH().toBase32().c_str(), int(++g_count));
#endif
}
//==========================================================================================
size_t QueueItem::getLastOnlineCount()
{
	if (m_diry_sources)
	{
		RLock(*QueueItem::g_cs);
		m_last_count_online_sources = 0;
		for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
		{
			if (i->first->isOnline())
				++m_last_count_online_sources;
		}
		m_diry_sources = 0;
	}
	return m_last_count_online_sources;
}
//==========================================================================================
bool QueueItem::isBadSourceExceptL(const UserPtr& aUser, Flags::MaskType exceptions) const
{
	const auto& i = m_badSources.find(aUser);
	if (i != m_badSources.end())
	{
		return i->second.isAnySet((Flags::MaskType)(exceptions ^ Source::FLAG_MASK));
	}
	return false;
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
	if (!ClientManager::isBeforeShutdown())
	{
		RLock(*QueueItem::g_cs);
		for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
		{
			if (i->first->isOnline())
			{
				list.push_back(i->first);
			}
		}
	}
}

void QueueItem::setSectionString(const string& p_section, bool p_is_first_load)
{
	if (p_section.empty())
		return;
		
	const StringTokenizer<string> SectionTokens(p_section, ' ');
	const StringList &Sections = SectionTokens.getTokens();
	
	if (!Sections.empty()) // TODO - парсинг секций отложить
	{
		// must be multiply of 2
		dcassert((Sections.size() & 1) == 0);
		
		if ((Sections.size() & 1) == 0)
		{
			CFlyFastLock(m_fcs_segment);
			for (auto i = Sections.cbegin(); i < Sections.cend(); i += 2)
			{
				int64_t l_start = Util::toInt64(i->c_str());
				int64_t l_size = Util::toInt64((i + 1)->c_str());
				
				addSegmentL(Segment(l_start, l_size), p_is_first_load); // TODO вынести лок выше наружу
			}
		}
	}
}

void QueueItem::addSourceL(const UserPtr& aUser, bool p_is_first_load)
{
	if (p_is_first_load == true)
	{
		m_sources.insert(std::make_pair(aUser, Source()));
	}
	else
	{
		dcassert(!isSourceL(aUser));
		SourceIter i = findBadSourceL(aUser);
		if (i != m_badSources.end())
		{
			m_sources.insert(*i);
			m_badSources.erase(i->first);
		}
		else
		{
			m_sources.insert(std::make_pair(aUser, Source()));  // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=139307
		}
		setDirtySource(true);
	}
	m_diry_sources++;
}
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
void QueueItem::resetDownloaded()
{
	bool l_is_dirty = true;
	{
		CFlyFastLock(m_fcs_segment);
		l_is_dirty = !m_done_segment.empty();
		m_done_segment.clear();
	}
	setDirtySegment(l_is_dirty);
}

bool QueueItem::isFinished() const
{
	if (m_done_segment.size() == 1)
	{
		CFlyFastLock(m_fcs_segment);
		return m_done_segment.size() == 1 && m_done_segment.begin()->isSizeEqu(getSize());
	}
	return false;
}

bool QueueItem::isChunkDownloaded(int64_t startPos, int64_t& len) const
{
	if (len <= 0)
		return false;
	CFlyFastLock(m_fcs_segment);
	for (auto i = m_done_segment.cbegin(); i != m_done_segment.cend(); ++i)
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
	SourceIter i = findSourceL(aUser); // crash - https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=42877 && http://www.flickr.com/photos/96019675@N02/10488126423/
	dcassert(i != m_sources.end());
	if (i != m_sources.end()) // https://drdump.com/Problem.aspx?ProblemID=129066
	{
		i->second.setFlag(reason);
		m_badSources.insert(*i);
		m_sources.erase(i);
		m_diry_sources++;
		setDirtySource(true);
	}
	else
	{
		const string l_error = "Error QueueItem::removeSourceL [i != m_sources.end()] aUser = [" +
		                       aUser->getLastNick() + "] Please send a text or a screenshot of the error to developers ppa74@ya.ru";
		LogManager::message(l_error);
		CFlyServerJSON::pushError(31, l_error);
	}
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
		const auto l_dc_temp_name = getDCTempName(getTargetFileName(), getTTH());
		if (!SETTING(TEMP_DOWNLOAD_DIRECTORY).empty() && File::getSize(getTarget()) == -1)
		{
			::StringMap sm;
			if (m_Target.length() >= 3 && m_Target[1] == ':' && m_Target[2] == '\\')
				sm["targetdrive"] = m_Target.substr(0, 3);
			else
				sm["targetdrive"] = Util::getLocalPath().substr(0, 3);
				
			setTempTarget(Util::formatParams(SETTING(TEMP_DOWNLOAD_DIRECTORY), sm, false) + l_dc_temp_name);
			
			{
				static bool g_is_first_check = false;
				if (!g_is_first_check && !m_tempTarget.empty())
				{
				
					g_is_first_check = true;
					File::ensureDirectory(SETTING(TEMP_DOWNLOAD_DIRECTORY));
					const tstring l_temp_targetT = Text::toT(m_tempTarget);
#ifndef _DEBUG
					const auto l_marker_file = Util::getFilePath(l_temp_targetT) + _T(".flylinkdc-test-readonly-") + Util::toStringW(GET_TIME()) + _T(".tmp");
#else
					const auto l_marker_file = Util::getFilePath(l_temp_targetT) + _T(".flylinkdc-test-readonly.tmp");
#endif
					try
					{
						{
							File l_f_ro_test(l_marker_file, File::WRITE, File::CREATE | File::TRUNCATE);
						}
						File::deleteFileT(l_marker_file);
					}
					catch (const Exception&)
					{
						dcassert(0);
						//const DWORD l_error = GetLastError();
						//if (l_error == 5) TODO - позже включить
						{
							SET_SETTING(TEMP_DOWNLOAD_DIRECTORY, "");
							const string l_log = "Error create/write + " + Text::fromT(l_marker_file) + " Error =" + Util::translateError();
							CFlyServerJSON::pushError(42, l_log);
						}
					}
				}
			}
		}
		if (SETTING(TEMP_DOWNLOAD_DIRECTORY).empty())
		{
			setTempTarget(m_Target.substr(0, m_Target.length() - getTargetFileName().length()) + l_dc_temp_name);
		}
	}
	return m_tempTarget;
}
#ifdef _DEBUG
bool QueueItem::isSourceValid(const QueueItem::Source* p_source_ptr)
{
	RLock(*g_cs);
	for (auto i = m_sources.cbegin(); i != m_sources.cend(); ++i)
	{
		if (p_source_ptr == &i->second)
			return true;
	}
	return false;
}
#endif
void QueueItem::addDownload(const DownloadPtr& p_download)
{
	CFlyFastLock(m_fcs_download);
	dcassert(p_download->getUser());
	dcassert(m_downloads.find(p_download->getUser()) == m_downloads.end());
	m_downloads.insert(std::make_pair(p_download->getUser(), p_download));
}

bool QueueItem::removeDownload(const UserPtr& p_user)
{
	CFlyFastLock(m_fcs_download);
	const auto l_size_before = m_downloads.size();
	if (l_size_before)
	{
		m_downloads.erase(p_user);
		//dcassert(l_size_before != m_downloads.size());
	}
	return l_size_before != m_downloads.size();
}
Segment QueueItem::getNextSegmentL(const int64_t  blockSize, const int64_t wantedSize, const int64_t lastSpeed, const PartialSource::Ptr &partialSource) const
{
	if (getSize() == -1 || blockSize == 0)
	{
		return Segment(0, -1);
	}
	
	if (!BOOLSETTING(ENABLE_MULTI_CHUNK))
	{
		{
			CFlyFastLock(m_fcs_download);
			if (!m_downloads.empty())
			{
				return Segment(-1, 0);
			}
		}
		
		int64_t start = 0;
		int64_t end = getSize();
		
		if (!m_done_segment.empty())
		{
			CFlyFastLock(m_fcs_segment);
			const Segment& first = *m_done_segment.begin();
			
			if (first.getStart() > 0)
			{
				end = Util::roundUp(first.getStart(), blockSize);
			}
			else
			{
				start = Util::roundDown(first.getEnd(), blockSize);
				{
					CFlyFastLock(m_fcs_segment);
					if (m_done_segment.size() > 1)
					{
						const Segment& second = *(++m_done_segment.begin());
						end = Util::roundUp(second.getStart(), blockSize);
					}
				}
			}
		}
		
		return Segment(start, std::min(getSize(), end) - start);
	}
	{
		CFlyFastLock(m_fcs_download);
		if (m_downloads.size() >= getMaxSegments() ||
		        (BOOLSETTING(DONT_BEGIN_SEGMENT) && static_cast<size_t>(SETTING(DONT_BEGIN_SEGMENT_SPEED) * 1024) < getAverageSpeed()))
		{
			// no other segments if we have reached the speed or segment limit
			return Segment(-1, 0);
		}
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
				{
					CFlyFastLock(m_fcs_segment);
					for (auto i = m_done_segment.cbegin(); !overlaps && i != m_done_segment.cend(); ++i)
					{
						const int64_t dstart = i->getStart();
						const int64_t dend = i->getEnd();
						// We accept partial overlaps, only consider the block done if it is fully consumed by the done block
						if (dstart <= startPreviewPosition && dend >= endPreviewPosition)
						{
							overlaps = true;
						}
					}
				}
				if (!overlaps)
				{
					CFlyFastLock(m_fcs_download);
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
		{
			posArray.push_back(min(getSize(), (int64_t)(*i) * blockSize));
		}
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
	{
		CFlyFastLock(m_fcs_download);
		{
			CFlyFastLock(m_fcs_segment);
			while (start < getSize())
			{
				int64_t end = std::min(getSize(), start + curSize);
				Segment block(start, end - start);
				bool overlaps = false;
				{
					for (auto i = m_done_segment.cbegin(); !overlaps && i != m_done_segment.cend(); ++i)
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
				}
				if (!overlaps)
				{
					for (auto i = m_downloads.cbegin(); !overlaps && i != m_downloads.cend(); ++i)
					{
						overlaps = block.overlaps(i->second->getSegment());
					}
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
		}
	} // end lock
	
	if (!neededParts.empty())
	{
		// select random chunk for download
		dcdebug("Found partial chunks: %d\n", int(neededParts.size()));
		
		Segment& selected = neededParts[Util::rand(0, static_cast<uint32_t>(neededParts.size()))];
		selected.setSize(std::min(selected.getSize(), targetSize)); // request only wanted size
		
		return selected;
	}
	
	if (partialSource == NULL && BOOLSETTING(OVERLAP_CHUNKS) && lastSpeed > 10 * 1024)
	{
		// overlap slow running chunk
		
		const uint64_t l_CurrentTick = GET_TICK();//[+]IRainman refactoring transfer mechanism
		CFlyFastLock(m_fcs_download);
		for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
		{
			const auto d = i->second;
			
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
			int64_t l_pos = d->getPos() - (d->getPos() % blockSize);
			int64_t l_size = d->getSize() - l_pos;
			
			// new user should finish this chunk more than 2x faster
			int64_t newChunkLeft = l_size / lastSpeed;
			if (2 * newChunkLeft < d->getSecondsLeft())
			{
				dcdebug("Overlapping... old user: %I64d s, new user: %I64d s\n", d->getSecondsLeft(), newChunkLeft);
				return Segment(d->getStartPos() + l_pos, l_size, true);
			}
		}
	}
	
	return Segment(0, 0);
}

void QueueItem::setOverlapped(const Segment& p_segment, const bool p_isOverlapped)
{
	// set overlapped flag to original segment
	CFlyFastLock(m_fcs_download);
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		auto d = i->second;
		if (d->getSegment().contains(p_segment))
		{
			d->setOverlapped(p_isOverlapped);
			break;
		}
	}
}

string QueueItem::getSectionString() const
{
	string l_strSections;
	{
		CFlyFastLock(m_fcs_segment);
		l_strSections.reserve(m_done_segment.size() * 10);
		for (auto i = m_done_segment.cbegin(); i != m_done_segment.cend(); ++i)
		{
			char buf[48];
			buf[0] = 0;
			_snprintf(buf, _countof(buf), "%I64d %I64d ", i->getStart(), i->getSize());
			l_strSections += buf;
		}
	}
	if (!l_strSections.empty())
	{
		l_strSections.resize(l_strSections.size() - 1);
	}
	return l_strSections;
}
void QueueItem::calcDownloadedBytes() const
{
	CFlyFastLock(m_fcs_segment);
	uint64_t l_totalDownloaded = 0;
	for (auto i = m_done_segment.cbegin(); i != m_done_segment.cend(); ++i)
	{
		l_totalDownloaded += i->getSize();
	}
	m_downloadedBytes = l_totalDownloaded;
}

uint64_t QueueItem::calcAverageSpeedAndCalcAndGetDownloadedBytesL() const // [!] IRainman opt.
{
	uint64_t l_totalSpeed = 0; // Скорость 64 битная нужна?
	calcDownloadedBytes();
	// count running segments
	CFlyFastLock(m_fcs_download);
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		const auto d = i->second;
		m_downloadedBytes += d->getPos();
		l_totalSpeed += d->getRunningAverage();
	}
	/*
	#ifdef _DEBUG
	static boost::atomic_uint l_count = 0;
	dcdebug("QueueItem::calcAverageSpeedAndDownloadedBytes() total_download = %I64u, totalSpeed = %I64u [count = %d] [done.size() = %u] [downloads.size() = %u]\n",
	        l_totalDownloaded, l_totalSpeed,
	        int(++l_count), done.size(), m_downloads.size());
	#endif
	*/
	m_averageSpeed    = l_totalSpeed;
	return m_downloadedBytes;
}
void QueueItem::addSegment(const Segment& p_segment, bool p_is_first_load /*= false */)
{
	CFlyFastLock(m_fcs_segment);
	addSegmentL(p_segment, p_is_first_load);
}
void QueueItem::addSegmentL(const Segment& p_segment, bool p_is_first_load)
{
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	if (m_delegater != nullptr && p_segment.getSize() > 0)
	{
		m_delegater->addSegment(p_segment.getStart(), p_segment.getSize());
	}
#endif
	dcassert(p_segment.getOverlapped() == false);
	m_done_segment.insert(p_segment);
#ifdef _DEBUG
//  LogManager::message("QueueItem::addSegment, setDirty = true! id = " +
//                      Util::toString(this->getFlyQueueID()) + " target = " + this->getTarget()
//	                    + " TempTarget = " + this->getTempTarget()
//	                    + " segment.getSize() = " + Util::toString(segment.getSize())
//	                    + " segment.getEnd() = " + Util::toString(segment.getEnd())
//	                   );
#endif
	// Consolidate segments
	if (m_done_segment.size() == 1)
		return;
	if (p_is_first_load == false)
	{
		setDirtySegment(true);
	}
	for (auto i = ++m_done_segment.cbegin(); i != m_done_segment.cend();)
	{
		SegmentSet::iterator prev = i;
		--prev;
		if (prev->getEnd() >= i->getStart())
		{
			const Segment big(prev->getStart(), i->getEnd() - prev->getStart());
			m_done_segment.erase(prev);
			m_done_segment.erase(i++);
			m_done_segment.insert(big);
			if (p_is_first_load == false)
			{
				setDirtySegment(true);
			}
		}
		else
		{
			++i;
		}
	}
}

bool QueueItem::isNeededPart(const PartsInfo& partsInfo, int64_t p_blockSize) const
{
	dcassert(partsInfo.size() % 2 == 0);
	CFlyFastLock(m_fcs_segment);
	auto i = m_done_segment.begin();
	for (auto j = partsInfo.cbegin(); j != partsInfo.cend(); j += 2)
	{
		while (i != m_done_segment.end() && (*i).getEnd() <= (*j) * p_blockSize)
			++i;
			
		if (i == m_done_segment.end() || !((*i).getStart() <= (*j) * p_blockSize && (*i).getEnd() >= (*(j + 1)) * p_blockSize))
			return true;
	}
	
	return false;
}

void QueueItem::getPartialInfo(PartsInfo& p_partialInfo, uint64_t p_blockSize) const
{
	dcassert(p_blockSize);
	if (p_blockSize == 0) // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=31115
		return;
		
	CFlyFastLock(m_fcs_segment);
	const size_t maxSize = min(m_done_segment.size() * 2, (size_t)510);
	p_partialInfo.reserve(maxSize);
	
	for (auto i = m_done_segment.cbegin(); i != m_done_segment.cend() && p_partialInfo.size() < maxSize; ++i)
	{
	
		uint16_t s = (uint16_t)(i->getStart() / p_blockSize);
		uint16_t e = (uint16_t)((i->getEnd() - 1) / p_blockSize + 1);
		
		p_partialInfo.push_back(s);
		p_partialInfo.push_back(e);
	}
}
void QueueItem::getChunksVisualisation(vector<pair<Segment, Segment>>& p_runnigChunksAndDownloadBytes, vector<Segment>& p_doneChunks) const
{
	try
	{
		{
			CFlyFastLock(m_fcs_download);
			p_runnigChunksAndDownloadBytes.reserve(m_downloads.size());
			for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
			{
				p_runnigChunksAndDownloadBytes.push_back(make_pair(i->second->getSegment(), Segment(i->second->getStartPos(), i->second->getPos())));
			}
		}
		p_doneChunks.reserve(m_done_segment.size());
		{
			CFlyFastLock(m_fcs_segment);
			for (auto i = m_done_segment.cbegin(); i != m_done_segment.cend(); ++i)
			{
				p_doneChunks.push_back(*i);
			}
		}
	}
	catch (const std::length_error&) // fix https://drdump.com/DumpGroup.aspx?DumpGroupID=672685
	{
		p_runnigChunksAndDownloadBytes.clear();
		p_doneChunks.clear();
	}
}
uint8_t QueueItem::calcActiveSegments()
{
	uint8_t activeSegments = 0;
	CFlyFastLock(m_fcs_download);
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		if (i->second->getStart() > 0)
		{
			activeSegments++;
		}
		// more segments won't change anything
		if (activeSegments > 1)
			break;
	}
	return activeSegments;
}
void QueueItem::getAllDownloadsUsers(UserList& p_users)
{
	p_users.clear();
	CFlyFastLock(m_fcs_download);
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		p_users.push_back(i->first);
	}
}
UserPtr QueueItem::getFirstUser()
{
	CFlyFastLock(m_fcs_download);
	if (!m_downloads.empty())
		return m_downloads.begin()->first;
	else
		return UserPtr();
}
bool QueueItem::isDownloadTree()
{
	CFlyFastLock(m_fcs_download);
	if (!m_downloads.empty())
		return m_downloads.begin()->second->getType() == Transfer::TYPE_TREE;
	else
		return false;
}
void QueueItem::getAllDownloadUser(UserList& p_users)
{
	CFlyFastLock(m_fcs_download);
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		p_users.push_back(i->first);
	}
}
void QueueItem::disconectedAllPosible(const DownloadPtr& aDownload)
{
	CFlyFastLock(m_fcs_download);
	// Disconnect all possible overlapped downloads
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		if (i->second != aDownload)
		{
			i->second->getUserConnection()->disconnect();
		}
	}
}
bool QueueItem::disconectedSlow(const DownloadPtr& aDownload)
{
	bool found = false;
	// ok, we got a fast slot, so it's possible to disconnect original user now
	CFlyFastLock(m_fcs_download);
	for (auto i = m_downloads.cbegin(); i != m_downloads.cend(); ++i)
	{
		const auto& j = i->second;
		if (j != aDownload && j->getSegment().contains(aDownload->getSegment()))
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
	return found;
}
