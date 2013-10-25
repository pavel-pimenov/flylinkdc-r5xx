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

#ifndef DCPLUSPLUS_DCPP_QUEUE_ITEM_H
#define DCPLUSPLUS_DCPP_QUEUE_ITEM_H

#include "Segment.h"
#include "HintedUser.h"

static const string TEMP_EXTENSION = "dctmp";

class QueueManager;

#ifdef SSA_VIDEO_PREVIEW_FEATURE
class QueueItemDelegate
{
	public:
		virtual void addSegment(int64_t start, int64_t size) = 0;
		virtual size_t getDownloadItems(int64_t blockSize, vector<int64_t>& ItemsArray) = 0;
		virtual void setDownloadItem(int64_t pos, int64_t size) = 0;
};
#endif

class QueueItem : public Flags,
	public intrusive_ptr_base<QueueItem>
#ifdef _DEBUG
	, virtual NonDerivable<QueueItem>, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		typedef std::unordered_map<string*, QueueItemPtr, noCaseStringHash, noCaseStringEq> QIStringMap;
		
		enum Priority
		{
			DEFAULT = -1,
			PAUSED = 0,
			LOWEST,
			LOW,
			NORMAL,
			HIGH,
			HIGHEST,
			LAST
		};
		typedef vector<pair<string, QueueItem::Priority> > PriorityArray;
		
		enum FileFlags
		{
			/** Normal download, no flags set */
			FLAG_NORMAL             = 0x00,
			/** This is a user file listing download */
			FLAG_USER_LIST          = 0x01,
			/** The file list is downloaded to use for directory download (used with USER_LIST) */
			FLAG_DIRECTORY_DOWNLOAD = 0x02,
			/** The file is downloaded to be viewed in the gui */
			FLAG_CLIENT_VIEW        = 0x04, //-V112
			/** Flag to indicate that file should be viewed as a text file */
			FLAG_TEXT               = 0x08,
			/** Match the queue against this list */
			FLAG_MATCH_QUEUE        = 0x10,
			/** The file list downloaded was actually an .xml.bz2 list */
			FLAG_XML_BZLIST         = 0x20, //-V112
			/** Only download a part of the file list */
			FLAG_PARTIAL_LIST       = 0x40,
			/** Test user's file list for fake share */
			FLAG_USER_CHECK         = 0x80,
			/** Autodrop slow source is enabled for this file */
			FLAG_AUTODROP           = 0x100,
			/** [+] SSA - check User IP */
			FLAG_USER_GET_IP        = 0x200,
			/** [+] SSA - dclst support */
			FLAG_DCLST_LIST         = 0x400,
			/** [+] SSA - media preview */
			FLAG_MEDIA_VIEW         = 0x800,
			/** [+] IRainman - open file */
			FLAG_OPEN_FILE          = 0x1000,
		};
		
		/**
		 * Source parts info
		 * Meaningful only when Source::FLAG_PARTIAL is set
		 */
		class PartialSource : public intrusive_ptr_base<PartialSource>
		{
			public:
				PartialSource(const string& aMyNick, const string& aHubIpPort, const string& aIp, uint16_t udp) :
					myNick(aMyNick), hubIpPort(aHubIpPort), ip(aIp), udpPort(udp), nextQueryTime(0), pendingQueryCount(0) { }
					
				~PartialSource() { }
				
				typedef boost::intrusive_ptr<PartialSource> Ptr;
				bool isCandidate(const uint64_t p_now) const // [+] FlylinkDC++
				{
					return getPendingQueryCount() < 10 && getUdpPort() != 0 && getNextQueryTime() <= p_now;
				}
				
				GETSET(PartsInfo, partialInfo, PartialInfo);
				GETSET(string, myNick, MyNick);         // for NMDC support only
				GETSET(string, hubIpPort, HubIpPort);
				GETSET(string, ip, Ip);
				GETSET(uint64_t, nextQueryTime, NextQueryTime);
				GETSET(uint16_t, udpPort, UdpPort);
				GETSET(uint8_t, pendingQueryCount, PendingQueryCount);
		};
		
		class Source : public Flags
		{
			public:
				enum
				{
					FLAG_NONE               = 0x00,
					FLAG_FILE_NOT_AVAILABLE = 0x01,
					FLAG_PASSIVE            = 0x02,
					FLAG_REMOVED            = 0x04, //-V112
					FLAG_NO_TTHF            = 0x08,
					FLAG_BAD_TREE           = 0x10,
					FLAG_SLOW_SOURCE        = 0x20, //-V112
					FLAG_NO_TREE            = 0x40,
					FLAG_NO_NEED_PARTS      = 0x80,
					FLAG_PARTIAL            = 0x100,
					FLAG_TTH_INCONSISTENCY  = 0x200,
					FLAG_UNTRUSTED          = 0x400,
					FLAG_MASK               = FLAG_FILE_NOT_AVAILABLE
					| FLAG_PASSIVE | FLAG_REMOVED | FLAG_BAD_TREE | FLAG_SLOW_SOURCE
					| FLAG_NO_TREE | FLAG_TTH_INCONSISTENCY | FLAG_UNTRUSTED
				};
				
				Source(const UserPtr& aUser) : user(aUser), partialSource(nullptr) { }
				//Source(const Source& aSource) : Flags(aSource), hintedUser(aSource.hintedUser), partialSource(aSource.partialSource) { }
				
				bool operator==(const UserPtr& aUser) const
				{
					return user == aUser;
				}
				PartialSource::Ptr& getPartialSource()
				{
					return partialSource;
				}
				
				bool isCandidate(const bool isBadSourse) const // [+] FlylinkDC++
				{
					return isSet(FLAG_PARTIAL) && (isBadSourse || !isSet(FLAG_TTH_INCONSISTENCY));
				}
				
				GETSET(UserPtr, user, User);
				GETSET(PartialSource::Ptr, partialSource, PartialSource);
		};
		
		typedef deque<Source> SourceList; // [!] IRainman opt: change vector to deque
		typedef SourceList::iterator SourceIter;
		typedef SourceList::const_iterator SourceConstIter;
		typedef std::multimap<time_t, pair<SourceConstIter, const QueueItemPtr> > SourceListBuffer;
		
		// [+] fix ? http://code.google.com/p/flylinkdc/issues/detail?id=1236 .
		static void getPFSSourcesL(const QueueItemPtr& p_qi, SourceListBuffer& p_sourceList, uint64_t p_now);
		
		typedef std::set<Segment> SegmentSet;
		typedef SegmentSet::const_iterator SegmentConstIter;
		
		QueueItem(const string& aTarget, int64_t aSize, Priority aPriority, Flags::MaskType aFlag,
		          time_t aAdded, const TTHValue& tth);
		          
		~QueueItem();
		
		size_t countOnlineUsersL() const;
		bool countOnlineUsersGreatOrEqualThanL(const size_t maxValue) const; // [+] FlylinkDC++ opt.
		void getOnlineUsers(UserList& l) const;
		
		// [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		static SharedCriticalSection cs;
		// [~]
		
		SourceList& getSourcesL() // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return sources;
		}
		const SourceList& getSourcesL() const // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return sources;
		}
		SourceList& getBadSourcesL() // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return badSources;
		}
		const SourceList& getBadSourcesL() const // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return badSources;
		}
		string getTargetFileName() const
		{
			return Util::getFileName(getTarget());
		}
		SourceIter getSourceL(const UserPtr& aUser) // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return find(sources.begin(), sources.end(), aUser); // [!] IRainman fix done: [6] https://www.box.net/shared/898c1974fa8c47f9614b
		}
		SourceIter getBadSourceL(const UserPtr& aUser) // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return find(badSources.begin(), badSources.end(), aUser);
		}
		SourceConstIter getSourceL(const UserPtr& aUser) const // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return find(sources.begin(), sources.end(), aUser);
		}
		SourceConstIter getBadSourceL(const UserPtr& aUser) const // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return find(badSources.begin(), badSources.end(), aUser);
		}
		bool isSourceL(const UserPtr& aUser) const
		{
			return getSourceL(aUser) != sources.end();
		}
		bool isBadSourceL(const UserPtr& aUser) const
		{
			return getBadSourceL(aUser) != badSources.end();
		}
		bool isBadSourceExceptL(const UserPtr& aUser, Flags::MaskType exceptions) const
		{
			SharedLock l(cs); // [+] IRainman fix.
			const auto& i = getBadSourceL(aUser);
			if (i != badSources.end())
				return i->isAnySet((Flags::MaskType)(exceptions ^ Source::FLAG_MASK));
			return false;
		}
		void getChunksVisualisation(vector<pair<Segment, Segment>>& p_runnigChunksAndDownloadBytes, vector<Segment>& p_doneChunks) const; // [!] IRainman fix.
		bool isChunkDownloadedL(int64_t startPos, int64_t& len) const;
		void setOverlappedL(const Segment& p_segment, const bool p_isOverlapped); // [!] IRainman fix.
		/**
		 * Is specified parts needed by this download?
		 */
		bool isNeededPartL(const PartsInfo& partsInfo, int64_t blockSize);
		
		/**
		 * Get shared parts info, max 255 parts range pairs
		 */
		void getPartialInfoL(PartsInfo& p_partialInfo, int64_t p_blockSize) const;
		
		uint64_t getDownloadedBytes() const // [+] IRainman opt.
		{
			return m_downloadedBytes;
		}
		uint64_t calcAverageSpeedAndCalcAndGetDownloadedBytesL() const; // [!] IRainman opt.
		// TODO - попробовать переписать функцию на +/- убрав постоянные итерации по коллекции
		// на больших очередях будет тормозить?
		/* [-] IRainman no needs! Please lock fully consciously!
		uint64_t calcAverageSpeedAndDownloadedBytes() const
		{
		    SharedLock l(cs);
		    return calcAverageSpeedAndDownloadedBytesL();
		}
		[-] */
		bool isDirty() const
		{
			return m_dirty;
		}
		void setDirty(bool p_dirty = true)
		{
			m_dirty = p_dirty;
		}
		
		DownloadList& getDownloadsL() // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return m_downloads;
		}
		
		size_t getDownloadsSegmentCount() const
		{
			// SharedLock l(cs); [!] IRainman opt: no needs for dqueue.
			return m_downloads.size();
		}
		
		/** Next segment that is not done and not being downloaded, zero-sized segment returned if there is none is found */
		Segment getNextSegmentL(const int64_t blockSize, const int64_t wantedSize, const int64_t lastSpeed, const PartialSource::Ptr &partialSource) const; // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		
		void addSegmentL(const Segment& segment);
		void resetDownloaded()
		{
			UniqueLock l(cs); // [+] IRainman fix.
			done.clear();
		}
		
		bool isFinishedL() const
		{
			return done.size() == 1 && *done.begin() == Segment(0, getSize());
		}
		
		bool isRunningL() const
		{
			return !isWaitingL();
		}
		bool isWaitingL() const
		{
			// TODO https://code.google.com/p/flylinkdc/issues/detail?id=1081
			//SharedLock l(cs); // [+] IRainman fix.
			return m_downloads.empty();
		}
		
		string getListName() const;
		const string& getTempTarget();
		void setTempTarget(const string& aTempTarget)
		{
			m_dirty = true;
			tempTarget = aTempTarget;
		}
		
#define GETSET_DIRTY(type, name, name2) \
private: type name; \
public: TypeTraits<type>::ParameterType get##name2() const { return name; } \
	void set##name2(TypeTraits<type>::ParameterType a##name2) {m_dirty = true; name = a##name2; }
	private:
		const TTHValue m_tthRoot;
		bool m_dirty;
		uint64_t m_block_size; // TODO: please fix the architect error, if this possible, see details here: http://code.google.com/p/flylinkdc/source/detail?r=12761
		void calcBlockSize();
	public:
		const TTHValue& getTTH() const
		{
			return m_tthRoot;
		}
		uint64_t getBlockSize()
		{
			if (m_block_size == 0)
			{
				calcBlockSize();
			}
			return m_block_size;
		}
		
		DownloadList m_downloads;
		
		GETSET_DIRTY(SegmentSet, done, Done);
		GETSET_DIRTY(string, target, Target);
		GETSET_DIRTY(uint64_t, fileBegin, FileBegin);
		GETSET_DIRTY(uint64_t, nextPublishingTime, NextPublishingTime);
		GETSET_DIRTY(int64_t, size, Size);
		GETSET_DIRTY(time_t, added, Added);
		GETSET_DIRTY(Priority, priority, Priority);
		GETSET_DIRTY(uint8_t, maxSegments, MaxSegments);
		GETSET_DIRTY(bool, autoPriority, AutoPriority);
		GETSET_DIRTY(int64_t, flyQueueID, FlyQueueID);
		GETSET_DIRTY(int, flyCountSourceInSQL, FlyCountSourceInSQL);
		
		int16_t calcTransferFlag(bool& partial, bool& trusted, bool& untrusted, bool& tthcheck, bool& zdownload, bool& chunked, double& ratio) const;
		QueueItem::Priority calculateAutoPriority() const;
		
		bool isAutoDrop() const // [+] IRainman fix.
		{
			return isSet(FLAG_AUTODROP);
		}
		
		void changeAutoDrop() // [+] IRainman fix.
		{
			if (isAutoDrop())
			{
				unsetFlag(QueueItem::FLAG_AUTODROP);
			}
			else
			{
				setFlag(QueueItem::FLAG_AUTODROP);
			}
		}
		
		uint64_t getAverageSpeed() const
		{
			return m_averageSpeed; // [+] IRainman opt.
		}
	private:
		mutable uint64_t m_averageSpeed; // [+] IRainman opt.
		mutable uint64_t m_downloadedBytes; // [+] IRainman opt.
		friend class QueueManager;
		SourceList sources;
		SourceList badSources;
		string tempTarget;
		
		void addSourceL(const UserPtr& aUser);
		void removeSourceL(const UserPtr& aUser, Flags::MaskType reason);
		
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	public:
		void setDelegate(QueueItemDelegate* p_delegater)
		{
			m_delegater = p_delegater;
		}
	private:
		QueueItemDelegate* m_delegater;
#endif
};

#endif // !defined(QUEUE_ITEM_H)

/**
* @file
* $Id: QueueItem.h 568 2011-07-24 18:28:43Z bigmuscle $
*/
