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


#pragma once

#ifndef DCPLUSPLUS_DCPP_QUEUE_ITEM_H
#define DCPLUSPLUS_DCPP_QUEUE_ITEM_H

#include "Segment.h"
#include "HintedUser.h"
#include "Download.h"

using std::pair;

#ifdef FLYLINKDC_USE_LOG_QUEUE_ITEM_DIRTY
#include "LogManager.h"
#endif

typedef std::vector<DownloadPtr> DownloadList;

extern const string g_dc_temp_extension;

class QueueManager;
typedef std::vector<uint16_t> PartsInfo;

#ifdef FLYLINKDC_USE_RWLOCK
#define RLock CFlyReadLock
#define WLock CFlyWriteLock
#else
#define RLock CFlyLock
#define WLock CFlyLock
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
	, boost::noncopyable
#endif
{
	public:
		typedef std::unordered_map<string, QueueItemPtr> QIStringMap;
		
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
		typedef std::vector<std::pair<std::string, QueueItem::Priority> > PriorityArray;
		
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
			FLAG_USER_GET_IP = 0x200,
			FLAG_DCLST_LIST = 0x400,
			FLAG_MEDIA_VIEW = 0x800,
			FLAG_OPEN_FILE = 0x1000,
			FLAG_TORRENT_FILE = 0x2000
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
				
				bool isCandidate(const bool isBadSourse) const
				{
					return isSet(FLAG_PARTIAL) && (isBadSourse || !isSet(FLAG_TTH_INCONSISTENCY));
				}
				
				GETSET(PartialSource::Ptr, partialSource, PartialSource);
		};
		
		typedef std::unordered_map<UserPtr, Source, User::Hash> SourceMap;
		typedef SourceMap::iterator SourceIter;
		typedef SourceMap::const_iterator SourceConstIter;
		typedef std::multimap<time_t, std::pair<SourceConstIter, const QueueItemPtr> > SourceListBuffer;
		
		static void getPFSSourcesL(const QueueItemPtr& p_qi, SourceListBuffer& p_sourceList, uint64_t p_now);
		
		typedef std::set<Segment> SegmentSet;
		
		QueueItem(const string& aTarget, int64_t aSize, Priority aPriority, bool aAutoPriority, Flags::MaskType aFlag,
		          time_t aAdded, const TTHValue& tth, uint8_t p_maxSegments, int64_t p_FlyQueueID, const string& aTempTarget);
		          
		~QueueItem();
		
		bool countOnlineUsersGreatOrEqualThanL(const size_t maxValue) const;
		void getOnlineUsers(UserList& p_users) const;
		
#ifdef FLYLINKDC_USE_RWLOCK
		static std::unique_ptr<webrtc::RWLockWrapper> g_cs;
#else
		static std::unique_ptr<CriticalSection> g_cs;
#endif
		
		const SourceMap& getSourcesL()
		{
			return m_sources;
		}
		const SourceMap& getBadSourcesL()
		{
			return m_badSources;
		}
#ifdef _DEBUG
		bool isSourceValid(const QueueItem::Source* p_source_ptr);
#endif
		size_t getSourcesCount() const
		{
			return m_sources.size();
		}
		string getTargetFileName() const
		{
			return Util::getFileName(getTarget());
		}
		SourceIter findSourceL(const UserPtr& aUser)
		{
			return m_sources.find(aUser);
		}
		SourceIter findBadSourceL(const UserPtr& aUser)
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
		bool isBadSourceExceptL(const UserPtr& aUser, Flags::MaskType exceptions) const;
		void getChunksVisualisation(vector<pair<Segment, Segment>>& p_runnigChunksAndDownloadBytes, vector<Segment>& p_doneChunks) const;
		bool isChunkDownloaded(int64_t startPos, int64_t& len) const;
		void setOverlapped(const Segment& p_segment, const bool p_isOverlapped);
		/**
		 * Is specified parts needed by this download?
		 */
		bool isNeededPart(const PartsInfo& partsInfo, int64_t p_blockSize) const;
		
		/**
		 * Get shared parts info, max 255 parts range pairs
		 */
		void getPartialInfo(PartsInfo& p_partialInfo, uint64_t p_blockSize) const;
		
		uint64_t getDownloadedBytes() const
		{
			return m_downloadedBytes;
		}
		uint64_t calcAverageSpeedAndCalcAndGetDownloadedBytesL() const;
		void calcDownloadedBytes() const;
		bool isDirtyBase() const
		{
			return m_dirty_base;
		}
		bool isDirtyAll() const
		{
			return m_dirty_base || m_dirty_segment || m_dirty_source;
		}
		void setDirty(bool p_dirty)
		{
#ifdef _DEBUG
			if (p_dirty)
			{
				m_dirty_base = p_dirty;
			}
#endif
#ifdef FLYLINKDC_USE_LOG_QUEUE_ITEM_DIRTY
			LogManager::message(__FUNCTION__ " p_dirty = " + Util::toString(p_dirty));
#endif
			m_dirty_base = p_dirty;
		}
		void resetDirtyAll()
		{
			setDirty(false);
			setDirtySource(false);
			setDirtySegment(false);
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
		mutable FastCriticalSection m_fcs_download;
		mutable FastCriticalSection m_fcs_segment;
		void addDownload(const DownloadPtr& p_download);
		bool removeDownload(const UserPtr& p_user);
		size_t getDownloadsSegmentCount() const
		{
			return m_downloads.size();
		}
		bool disconectedSlow(const DownloadPtr& d);
		void disconectedAllPosible(const DownloadPtr& d);
		uint8_t calcActiveSegments();
		void getAllDownloadUser(UserList& p_users);
		bool isDownloadTree();
		UserPtr getFirstUser();
		void getAllDownloadsUsers(UserList& p_users);
		/** Next segment that is not done and not being downloaded, zero-sized segment returned if there is none is found */
		Segment getNextSegmentL(const int64_t blockSize, const int64_t wantedSize, const int64_t lastSpeed, const PartialSource::Ptr &partialSource) const;
		
		void addSegment(const Segment& segment, bool p_is_first_load = false);
		void addSegmentL(const Segment& segment, bool p_is_first_load = false);
		void resetDownloaded();
		
		bool isFinished() const;
		
		bool isRunning() const
		{
			return !isWaiting();
		}
		bool isWaiting() const
		{
			// fix lock - не включать! CFlyFastLock(m_fcs_download);
			return m_downloads.empty();
		}
		string getListName() const;
		const string& getTempTarget();
		const string& getTempTargetConst() const
		{
			return m_tempTarget;
		}
		
		
		/*
		#define GETSET_DIRTY(type, name, name2) \
		private: type name; \
		public: TypeTraits<type>::ParameterType get##name2() const { return name; } \
		    void set##name2(TypeTraits<type>::ParameterType a##name2) \
		    { \
		        if(name != a##name2) \
		        { \
		            LogManager::message("GETSET_DIRTY name = " __FUNCTION__ " old = " + Util::toString(get##name2()) + " new = " + Util::toString(a##name2)); \
		            setDirty(true); \
		            name = a##name2; \
		        } \
		        else \
		        { \
		        LogManager::message("[!]GETSET_DIRTY name = " __FUNCTION__ " old = new = " + Util::toString(a##name2)); \
		        }\
		    }
		*/
	private:
		const TTHValue m_tthRoot;
		bool m_dirty_base;
		bool m_dirty_source;
		bool m_dirty_segment;
		uint64_t m_block_size;
		void calcBlockSize();
	public:
		bool m_is_file_not_exist;
		
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
		
		DownloadList m_downloads;
		
		SegmentSet m_done_segment;
		string getSectionString() const;
#ifdef SSA_VIDEO_PREVIEW_FEATURE
		SegmentSet getDone() const
		{
			CFlyFastLock(m_fcs_segment);
			return m_done_segment;
		}
#endif
		GETSET(uint64_t, timeFileBegin, TimeFileBegin);
		GETSET(uint64_t, nextPublishingTime, NextPublishingTime);
		GETSET(int64_t, lastsize, LastSize);
		GETSET(time_t, added, Added);
		GETSET(int64_t, flyQueueID, FlyQueueID);
		
	private:
		Priority m_priority;
		string m_Target;
		int64_t m_Size;
		uint8_t m_maxSegments;
		bool m_AutoPriority;
	public:
		bool getAutoPriority() const
		{
			return m_AutoPriority;
		}
		void setAutoPriority(bool p_value)
		{
			if (m_AutoPriority != p_value)
			{
				setDirty(true);
#ifdef FLYLINKDC_USE_LOG_QUEUE_ITEM_DIRTY
				LogManager::message(__FUNCTION__ " new = " + Util::toString(p_value));
#endif
				m_AutoPriority = p_value;
			}
		}
		
		uint8_t getMaxSegments() const
		{
			return m_maxSegments;
		}
		void setMaxSegments(uint8_t p_value)
		{
			if (m_maxSegments != p_value)
			{
				setDirty(true);
#ifdef FLYLINKDC_USE_LOG_QUEUE_ITEM_DIRTY
				LogManager::message(__FUNCTION__ " new = " + Util::toString(p_value));
#endif
				m_maxSegments = p_value;
			}
		}
		const int64_t& getSize() const
		{
			return m_Size;
		}
		void setSize(const int64_t& p_value)
		{
			if (m_Size != p_value)
			{
				setDirty(true);
#ifdef FLYLINKDC_USE_LOG_QUEUE_ITEM_DIRTY
				LogManager::message(__FUNCTION__ " new = " + Util::toString(p_value));
#endif
				m_Size = p_value;
			}
		}
		
		const string& getTarget() const
		{
			return m_Target;
		}
		void setTarget(const string& p_value)
		{
			if (m_Target != p_value)
			{
				setDirty(true);
#ifdef FLYLINKDC_USE_LOG_QUEUE_ITEM_DIRTY
				LogManager::message(__FUNCTION__ " new = " + Util::toString(p_value));
#endif
				m_Target = p_value;
			}
		}
		
		Priority getPriority() const
		{
			return m_priority;
		}
		void setPriority(Priority p_value)
		{
			if (m_priority != p_value)
			{
				setDirtySegment(true);
#ifdef FLYLINKDC_USE_LOG_QUEUE_ITEM_DIRTY
				LogManager::message(__FUNCTION__ " new = " + Util::toString(p_value));
#endif
				m_priority = p_value;
			}
		}
		void setTempTarget(const string& p_value)
		{
			if (m_tempTarget != p_value)
			{
				setDirty(true);
#ifdef FLYLINKDC_USE_LOG_QUEUE_ITEM_DIRTY
				LogManager::message(__FUNCTION__ " new = " + Util::toString(p_value));
#endif
				m_tempTarget = p_value;
			}
		}
		
		int16_t calcTransferFlag(bool& partial, bool& trusted, bool& untrusted, bool& tthcheck, bool& zdownload, bool& chunked, double& ratio) const;
		QueueItem::Priority calculateAutoPriority() const;
		
		bool isAutoDrop() const
		{
			return isSet(FLAG_AUTODROP);
		}
		
		void changeAutoDrop()
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
			return m_averageSpeed;
		}
		void setSectionString(const string& p_section, bool p_is_first_load);
		size_t getLastOnlineCount();
	private:
		mutable uint64_t m_averageSpeed;
		mutable uint64_t m_downloadedBytes;
		friend class QueueManager;
		unsigned m_diry_sources;
		size_t m_last_count_online_sources;
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
		CFlySegment() : m_id(0), m_priority(QueueItem::NORMAL)
		{
		}
		explicit CFlySegment(const QueueItemPtr& p_QueueItem)
		{
			m_priority = int(p_QueueItem->getPriority());
			m_segment = p_QueueItem->getSectionString();
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
