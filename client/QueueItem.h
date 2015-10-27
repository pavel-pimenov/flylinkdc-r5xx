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
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "Download.h"

typedef std::unordered_map<UserPtr, DownloadPtr, User::Hash> DownloadMap;


extern const string g_dc_temp_extension;

class QueueManager;
typedef std::vector<uint16_t> PartsInfo;

#ifdef FLYLINKDC_USE_RWLOCK
#define RLock webrtc::ReadLockScoped
#define WLock webrtc::WriteLockScoped
#else
#define RLock Lock
#define WLock Lock
#endif


#ifdef SSA_VIDEO_PREVIEW_FEATURE
class QueueItemDelegate
{
	public:
		virtual void addSegment(int64_t start, int64_t size) = 0;
		virtual size_t getDownloadItems(int64_t blockSize, vector<int64_t>& ItemsArray) = 0;
		virtual void setDownloadItem(int64_t pos, int64_t size) = 0;
};
#endif
class QueueItem : public Flags
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		typedef boost::unordered_map<string, QueueItemPtr> QIStringMap;
		
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
			FLAG_NORMAL = 0x00,
			/** This is a user file listing download */
			FLAG_USER_LIST = 0x01,
			/** The file list is downloaded to use for directory download (used with USER_LIST) */
			FLAG_DIRECTORY_DOWNLOAD = 0x02,
			/** The file is downloaded to be viewed in the gui */
			FLAG_CLIENT_VIEW = 0x04, //-V112
			/** Flag to indicate that file should be viewed as a text file */
			FLAG_TEXT = 0x08,
			/** Match the queue against this list */
			FLAG_MATCH_QUEUE = 0x10,
			/** The file list downloaded was actually an .xml.bz2 list */
			FLAG_XML_BZLIST = 0x20, //-V112
			/** Only download a part of the file list */
			FLAG_PARTIAL_LIST = 0x40,
#ifdef IRAINMAN_INCLUDE_USER_CHECK
			/** Test user's file list for fake share */
			FLAG_USER_CHECK         = 0x80,
#endif
			/** Autodrop slow source is enabled for this file */
			FLAG_AUTODROP = 0x100,
			/** [+] SSA - check User IP */
			FLAG_USER_GET_IP = 0x200,
			/** [+] SSA - dclst support */
			FLAG_DCLST_LIST = 0x400,
			/** [+] SSA - media preview */
			FLAG_MEDIA_VIEW = 0x800,
			/** [+] IRainman - open file */
			FLAG_OPEN_FILE = 0x1000,
		};
		
		bool isUserList() const
		{
			return isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP);
		}
		/**
		 * Source parts info
		 * Meaningful only when Source::FLAG_PARTIAL is set
		 */
		class PartialSource
		{
			public:
				PartialSource(const string& aMyNick, const string& aHubIpPort, const boost::asio::ip::address_v4& aIp, uint16_t udp) :
					myNick(aMyNick), hubIpPort(aHubIpPort), ip(aIp), udpPort(udp), nextQueryTime(0), pendingQueryCount(0) { }
					
				~PartialSource() { }
				
				typedef std::shared_ptr<PartialSource> Ptr;
				bool isCandidate(const uint64_t p_now) const // [+] FlylinkDC++
				{
					return getPendingQueryCount() < 10 && getUdpPort() != 0 && getNextQueryTime() <= p_now;
				}
				
				GETSET(PartsInfo, partialInfo, PartialInfo);
				GETSET(string, myNick, MyNick);         // for NMDC support only
				GETSET(string, hubIpPort, HubIpPort);
				GETSET(boost::asio::ip::address_v4, ip, Ip);
				GETSET(uint64_t, nextQueryTime, NextQueryTime);
				GETSET(uint16_t, udpPort, UdpPort);
				GETSET(uint8_t, pendingQueryCount, PendingQueryCount);
		};
		
		class Source : public Flags
		{
			public:
				enum
				{
					FLAG_NONE = 0x00,
					FLAG_FILE_NOT_AVAILABLE = 0x01,
					FLAG_PASSIVE = 0x02,
					FLAG_REMOVED = 0x04, //-V112
					FLAG_NO_TTHF = 0x08,
					FLAG_BAD_TREE = 0x10,
					FLAG_SLOW_SOURCE = 0x20, //-V112
					FLAG_NO_TREE = 0x40,
					FLAG_NO_NEED_PARTS = 0x80,
					FLAG_PARTIAL = 0x100,
					FLAG_TTH_INCONSISTENCY = 0x200,
					FLAG_UNTRUSTED = 0x400,
					FLAG_MASK = FLAG_FILE_NOT_AVAILABLE
					| FLAG_PASSIVE | FLAG_REMOVED | FLAG_BAD_TREE | FLAG_SLOW_SOURCE
					| FLAG_NO_TREE | FLAG_TTH_INCONSISTENCY | FLAG_UNTRUSTED
				};
				
				Source()  {}
				
				bool isCandidate(const bool isBadSourse) const // [+] FlylinkDC++
				{
					return isSet(FLAG_PARTIAL) && (isBadSourse || !isSet(FLAG_TTH_INCONSISTENCY));
				}
				
				GETSET(PartialSource::Ptr, partialSource, PartialSource);
		};
		
		typedef std::unordered_map<UserPtr, Source, User::Hash> SourceMap;
		typedef SourceMap::iterator SourceIter;
		typedef SourceMap::const_iterator SourceConstIter;
		typedef std::multimap<time_t, pair<SourceConstIter, const QueueItemPtr> > SourceListBuffer;
		
		// [+] fix ? http://code.google.com/p/flylinkdc/issues/detail?id=1236 .
		static void getPFSSourcesL(const QueueItemPtr& p_qi, SourceListBuffer& p_sourceList, uint64_t p_now);
		
		typedef std::set<Segment> SegmentSet;
		
		QueueItem(const string& aTarget, int64_t aSize, Priority aPriority, Flags::MaskType aFlag,
		          time_t aAdded, const TTHValue& tth, uint8_t p_maxSegments, int64_t p_FlyQueueID, const string& aTempTarget);
		          
		~QueueItem();
		
		size_t countOnlineUsersL() const;
		bool countOnlineUsersGreatOrEqualThanL(const size_t maxValue) const; // [+] FlylinkDC++ opt.
		void getOnlineUsers(UserList& l) const;
		
		// [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
#ifdef FLYLINKDC_USE_RWLOCK
		static std::unique_ptr<webrtc::RWLockWrapper> g_cs;
#else
		static std::unique_ptr<CriticalSection> g_cs;
#endif
		// [~]
		
		const SourceMap& getSourcesL() // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return m_sources;
		}
		const SourceMap& getBadSourcesL() // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return m_badSources;
		}
#ifdef _DEBUG
		bool isSourceValid(const QueueItem::Source* p_source_ptr);
#endif
		size_t getSourcesCountL() const
		{
			return m_sources.size();
		}
		string getTargetFileName() const
		{
			return Util::getFileName(getTarget());
		}
		SourceIter findSourceL(const UserPtr& aUser) // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return m_sources.find(aUser); // [!] IRainman fix done: [6] https://www.box.net/shared/898c1974fa8c47f9614b
		}
		SourceIter findBadSourceL(const UserPtr& aUser) // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return m_badSources.find(aUser);
		}
		bool isSourceL(const UserPtr& aUser) const
		{
			return m_sources.find(aUser) != m_sources.end();
		}
		bool isBadSourceL(const UserPtr& aUser) const
		{
			return m_badSources.find(aUser) != m_badSources.end();
		}
		bool isBadSourceExceptL(const UserPtr& aUser, Flags::MaskType exceptions) const
		{
			const auto& i = m_badSources.find(aUser);
			if (i != m_badSources.end())
				return i->second.isAnySet((Flags::MaskType)(exceptions ^ Source::FLAG_MASK));
			return false;
		}
		void getChunksVisualisation(vector<pair<Segment, Segment>>& p_runnigChunksAndDownloadBytes, vector<Segment>& p_doneChunks) const; // [!] IRainman fix.
		bool isChunkDownloadedL(int64_t startPos, int64_t& len) const;
		void setOverlappedL(const Segment& p_segment, const bool p_isOverlapped); // [!] IRainman fix.
		/**
		 * Is specified parts needed by this download?
		 */
		bool isNeededPartL(const PartsInfo& partsInfo, int64_t p_blockSize) const;
		
		/**
		 * Get shared parts info, max 255 parts range pairs
		 */
		void getPartialInfoL(PartsInfo& p_partialInfo, uint64_t p_blockSize) const;
		
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
		RLock l(*QueueItem::g_cs);
		return calcAverageSpeedAndDownloadedBytesL();
		}
		[-] */
		bool isDirtyBase() const
		{
			return m_dirty;
		}
		bool isDirtyAll() const
		{
			return m_dirty || m_dirty_segment || m_dirty_source;
		}
		void setDirty(bool p_dirty)
		{
#ifdef _DEBUG
			if (p_dirty)
			{
				m_dirty = p_dirty;
			}
#endif
			m_dirty = p_dirty;
		}
		void setDirtyAll(bool p_dirty)
		{
			setDirty(p_dirty);
			setDirtySource(p_dirty);
			setDirtySegment(p_dirty);
		}
		
		bool isDirtySource() const
		{
			return m_dirty_source;
		}
		bool isDirtySegment() const
		{
			return m_dirty_segment;
		}
		void setDirtySource(bool p_dirty)
		{
#ifdef _DEBUG
			if (p_dirty)
			{
				m_dirty_source = p_dirty;
			}
#endif
			m_dirty_source = p_dirty;
		}
		void setDirtySegment(bool p_dirty)
		{
#ifdef _DEBUG
			if (p_dirty)
			{
				m_dirty_segment = p_dirty;
			}
#endif
			m_dirty_segment = p_dirty;
		}
		const DownloadMap& getDownloadsL() // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		{
			return m_downloads;
		}
		void addDownloadL(const DownloadPtr& p_download);
		bool removeDownloadL(const UserPtr& p_user);
		size_t getDownloadsSegmentCount() const
		{
			// RLock l(*QueueItem::g_cs);
			return m_downloads.size();
		}
		
		/** Next segment that is not done and not being downloaded, zero-sized segment returned if there is none is found */
		Segment getNextSegmentL(const int64_t blockSize, const int64_t wantedSize, const int64_t lastSpeed, const PartialSource::Ptr &partialSource) const; // [!] IRainman fix: Please lock access to functions with postfix L with an external lock critical section in QueueItem, ie in this class.
		
		void addSegmentL(const Segment& segment, bool p_is_first_load = false);
		void resetDownloaded()
		{
			bool l_is_dirty = true;
			{
				WLock l(*g_cs);
				l_is_dirty = !m_done_segment.empty();
				m_done_segment.clear();
			}
			setDirtySegment(l_is_dirty);
		}
		
		bool isFinishedL() const
		{
			return m_done_segment.size() == 1 && *m_done_segment.begin() == Segment(0, getSize());
		}
		
		bool isRunningL() const
		{
			return !isWaitingL();
		}
		bool isWaitingL() const
		{
			// TODO https://code.google.com/p/flylinkdc/issues/detail?id=1081
			return m_downloads.empty();
		}
		
		string getListName() const;
		const string& getTempTarget();
		const string& getTempTargetConst() const;
		void setTempTarget(const string& p_TempTarget);
		
#define GETSET_DIRTY(type, name, name2) \
private: type name; \
public: TypeTraits<type>::ParameterType get##name2() const { return name; } \
	void set##name2(TypeTraits<type>::ParameterType a##name2) { if(name != a##name2) {setDirty(true); name = a##name2;} }
	private:
		const TTHValue m_tthRoot;
		bool m_dirty;
		bool m_dirty_source;
		bool m_dirty_segment;
		uint64_t m_block_size;
		void calcBlockSize();
	public:
		const TTHValue& getTTH() const
		{
			return m_tthRoot;
		}
		void setBlockSize(uint64_t p_block_size)
		{
			m_block_size = p_block_size;
		}
		uint64_t get_block_size_sql()
		{
			if (m_block_size == 0)
			{
				calcBlockSize();
			}
			dcassert(m_block_size);
			return m_block_size;
		}
		
		DownloadMap m_downloads;
		
		SegmentSet m_done_segment;
		string getSectionStringL();
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		const SegmentSet& getDoneL() const
		{
			return m_done_segment;
		}
#endif
		GETSET_DIRTY(string, target, Target);
		GETSET_DIRTY(uint64_t, fileBegin, FileBegin);
		GETSET(uint64_t, nextPublishingTime, NextPublishingTime);
		GETSET_DIRTY(int64_t, size, Size);
		GETSET(time_t, added, Added);
		GETSET_DIRTY(uint8_t, maxSegments, MaxSegments);
		GETSET_DIRTY(bool, autoPriority, AutoPriority);
		GETSET_DIRTY(int64_t, flyQueueID, FlyQueueID);
		
	private:
		Priority m_priority;
	public:
		Priority getPriority() const
		{
			return m_priority;
		}
		void setPriority(Priority p_priority);
		int16_t calcTransferFlagL(bool& partial, bool& trusted, bool& untrusted, bool& tthcheck, bool& zdownload, bool& chunked, double& ratio) const;
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
		void setSectionString(const string& p_section, bool p_is_first_load);
	private:
		mutable uint64_t m_averageSpeed; // [+] IRainman opt.
		mutable uint64_t m_downloadedBytes; // [+] IRainman opt.
		friend class QueueManager;
		SourceMap m_sources;
		SourceMap m_badSources;
		string m_tempTarget;
		
		void addSourceL(const UserPtr& aUser, bool p_is_first_load);
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

class CFlySegment
{
	public:
		int m_id;
		int m_priority;
		string m_segment;
		CFlySegment()
		{
		}
		explicit CFlySegment(const QueueItemPtr& p_QueueItem)
		{
			m_priority = int(p_QueueItem->getPriority());
			m_segment = p_QueueItem->getSectionStringL();
			m_id = p_QueueItem->getFlyQueueID();
			dcassert(m_id);
		}
};
typedef vector<CFlySegment> CFlySegmentArray;

#endif // !defined(QUEUE_ITEM_H)

/**
* @file
* $Id: QueueItem.h 568 2011-07-24 18:28:43Z bigmuscle $
*/
