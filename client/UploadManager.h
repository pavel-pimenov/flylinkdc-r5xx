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

#ifndef DCPLUSPLUS_DCPP_UPLOAD_MANAGER_H
#define DCPLUSPLUS_DCPP_UPLOAD_MANAGER_H

#include <set>
#include "UploadManagerListener.h"
#include "ClientManagerListener.h"
#include "UserConnection.h"

typedef pair<UserPtr, unsigned int> CurrentConnectionPair;
typedef std::unordered_map<UserPtr, unsigned int, User::Hash> CurrentConnectionMap;
typedef std::vector<UploadPtr> UploadList;

class UploadQueueItem :
	public ColumnBase< 13 >,
	public UserInfoBase
{
	public:
		UploadQueueItem(const HintedUser& user, const string& file, int64_t pos, int64_t size) :
			m_hintedUser(user), m_file(file), m_pos(pos), m_size(size), m_time(GET_TIME())
		{
		}
		~UploadQueueItem()
		{
		}
		void update();
		const UserPtr& getUser() const
		{
			return m_hintedUser.user;
		}
		int getImageIndex() const
		{
			return 0; //g_fileImage.getIconIndex(getFile());
		}
		static int compareItems(const UploadQueueItem* a, const UploadQueueItem* b, uint8_t col);
		enum
		{
			COLUMN_FIRST, // �� ������ ��������� ColumnBase<13>
			COLUMN_FILE = COLUMN_FIRST,
			COLUMN_TYPE,
			COLUMN_PATH,
			COLUMN_NICK,
			COLUMN_HUB,
			COLUMN_TRANSFERRED,
			COLUMN_SIZE,
			COLUMN_ADDED,
			COLUMN_WAITING,
			COLUMN_LOCATION,
			COLUMN_IP,
#ifdef FLYLINKDC_USE_DNS
			COLUMN_DNS,
#endif
			COLUMN_SLOTS,
			COLUMN_SHARE,
			COLUMN_LAST // �� ������ ��������� ColumnBase<13>
		};
		
		GETC(HintedUser, m_hintedUser, HintedUser);
		GETC(string, m_file, File);
		GETSET(int64_t, m_pos, Pos);
		GETC(int64_t, m_size, Size);
		GETC(uint64_t, m_time, Time);
		Util::CustomNetworkIndex m_location;
};

class WaitingUser
#ifdef _DEBUG
//    : public boost::noncopyable
#endif
{
	public:
		WaitingUser(const HintedUser& p_hintedUser, const std::string& p_token, const UploadQueueItemPtr& p_uqi) : m_hintedUser(p_hintedUser), m_token(p_token)
		{
			m_waiting_files.push_back(p_uqi);
		}
		operator const UserPtr&() const
		{
			return m_hintedUser.user;
		}
		UserPtr getUser() const
		{
			return m_hintedUser.user;
		}
		std::vector<UploadQueueItemPtr> m_waiting_files;
		HintedUser m_hintedUser;
		GETSET(string, m_token, Token);
};

class UploadManager : private ClientManagerListener, private UserConnectionListener, public Speaker<UploadManagerListener>, private TimerManagerListener, public Singleton<UploadManager>
{
#ifdef FLYLINKDC_USE_DOS_GUARD
		typedef std::unordered_map<string, uint8_t> CFlyDoSCandidatMap;
		CFlyDoSCandidatMap m_dos_map;
		mutable FastCriticalSection csDos;
#endif
	public:
		static uint32_t g_count_WaitingUsersFrame;
		/** @return Number of uploads. */
		static size_t getUploadCount()
		{
			// CFlyReadLock(*g_csUploadsDelay);
			return g_uploads.size();
		}
		
		/**
		 * @remarks This is only used in the tray icons. Could be used in
		 * MainFrame too.
		 *
		 * @return Running average download speed in Bytes/s
		 */
		static int64_t getRunningAverage()
		{
			return g_runningAverage;
		}
		
		static int getSlots()
		{
			return (std::max(SETTING(SLOTS), std::max(SETTING(HUB_SLOTS), 0) * Client::getTotalCounts()));
		}
		
		/** @return Number of free slots. */
		static int getFreeSlots()
		{
			return std::max((getSlots() - g_running), 0);
		}
		
		/** @internal */
		int getFreeExtraSlots() const
		{
			return std::max(SETTING(EXTRA_SLOTS) - getExtra(), 0);
		}
		
		/** @param aUser Reserve an upload slot for this user and connect. */
		void reserveSlot(const HintedUser& hintedUser, uint64_t aTime);
		static void unreserveSlot(const HintedUser& hintedUser);
		void clearUserFilesL(const UserPtr&);
		void clearWaitingFilesL(const WaitingUser& p_wu);
		
		
		class LockInstanceQueue
		{
			public:
				LockInstanceQueue()
				{
					UploadManager::getInstance()->m_csQueue.lock();
				}
				~LockInstanceQueue()
				{
					UploadManager::getInstance()->m_csQueue.unlock();
				}
				UploadManager* operator->()
				{
					return UploadManager::getInstance();
				}
		};
		
		typedef std::list<WaitingUser> SlotQueue;
		const SlotQueue& getUploadQueueL() const
		{
			return m_slotQueue;
		}
		bool getIsFireballStatus() const
		{
			return isFireball;
		}
		bool getIsFileServerStatus() const
		{
			return isFileServer;
		}
		
		/** @internal */
		void addConnection(UserConnection* p_conn);
		static void removeDelayUpload(const UserPtr& aUser);
		static void abortUpload(const string& aFile, bool waiting = true);
		
		GETSET(int, extraPartial, ExtraPartial);
		GETSET(int, extra, Extra);
		GETSET(uint64_t, lastGrant, LastGrant);
		
		void load();
		static void save();
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		static bool isBanReply(const UserPtr& user);
#endif // IRAINMAN_ENABLE_AUTO_BAN
		
		static time_t getReservedSlotTime(const UserPtr& aUser);
		static void shutdown();
		
	private:
		bool isFireball;
		bool isFileServer;
		static int  g_running;
		static int64_t g_runningAverage;
		uint64_t m_fireballStartTick;
		
		static UploadList g_uploads;
		static UploadList g_delayUploads;
		static CurrentConnectionMap g_uploadsPerUser;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csUploadsDelay;
		
		void process_slot(UserConnection::SlotTypes p_slot_type, int p_delta);
		
		static void increaseUserConnectionAmountL(const UserPtr& p_user);
		static void decreaseUserConnectionAmountL(const UserPtr& p_user);
		static unsigned int getUserConnectionAmountL(const UserPtr& p_user);
		
		int m_lastFreeSlots; // amount of free slots at the previous minute
		
		typedef std::unordered_map<UserPtr, uint64_t, User::Hash> SlotMap;
		
		static SlotMap g_reservedSlots;
		static bool g_is_reservedSlotEmpty;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csReservedSlots;
		
		SlotMap m_notifiedUsers;
		SlotQueue m_slotQueue;
		mutable CriticalSection m_csQueue;
		
		size_t addFailedUpload(const UserConnection* aSource, const string& file, int64_t pos, int64_t size);
		void notifyQueuedUsers(int64_t p_tick);
		
		friend class Singleton<UploadManager>;
		UploadManager() noexcept;
		~UploadManager();
		
		bool getAutoSlot();
		void removeConnection(UserConnection* aConn, bool p_is_remove_listener = true);
		static void removeUpload(UploadPtr& aUpload, bool delay = false);
		void logUpload(const UploadPtr& u);
		
		static void testSlotTimeout(uint64_t aTick = GET_TICK());
		static void decodeBZ2(const uint8_t* is, size_t sz, string& os);
		// ClientManagerListener
		void on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept override;
		
		// TimerManagerListener
		void on(Second, uint64_t aTick) noexcept override;
		void on(Minute, uint64_t aTick) noexcept override;
		
		// UserConnectionListener
		// void on(UserBytesSent, UserConnection*, size_t p_Bytes, size_t p_Actual) noexcept override;
		void on(Failed, UserConnection*, const string&) noexcept override;
		void on(Get, UserConnection*, const string&, int64_t) noexcept override;
		void on(Send, UserConnection*) noexcept override;
		void on(TransmitDone, UserConnection*) noexcept override;
		void on(GetListLength, UserConnection*) noexcept override;
		
		void on(AdcCommand::GET, UserConnection*, const AdcCommand&) noexcept override;
		void on(AdcCommand::GFI, UserConnection*, const AdcCommand&) noexcept override;
		
		bool prepareFile(UserConnection* aSource, const string& aType, const string& aFile, int64_t aResume, int64_t& aBytes, bool listRecursive = false);
		bool hasUpload(const UserConnection* aSource, const string& p_source_file) const;
		
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		struct banmsg_t
		{
			uint32_t tick;
			int slots, share, limit, min_slots, max_slots, min_share, min_limit;
			bool same(const banmsg_t& a) const
			{
				return ((slots ^ a.slots) | (share ^ a.share) | (limit ^ a.limit) |
				        (min_slots ^ a.min_slots) |
				        (max_slots ^ a.max_slots) |
				        (min_share ^ a.min_share) | (min_limit ^ a.min_limit)) == 0;
			}
		};
		typedef std::unordered_map<string, banmsg_t> BanMap;
		bool handleBan(UserConnection* aSource/*, bool forceBan, bool noChecks*/);
		static bool hasAutoBan(const UserPtr& aUser);
		static BanMap g_lastBans;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csBans;
#endif // IRAINMAN_ENABLE_AUTO_BAN
};

#endif // !defined(UPLOAD_MANAGER_H)

/**
 * @file
 * $Id: UploadManager.h 578 2011-10-04 14:27:51Z bigmuscle $
 */
