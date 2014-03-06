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

#ifndef DCPLUSPLUS_DCPP_UPLOAD_MANAGER_H
#define DCPLUSPLUS_DCPP_UPLOAD_MANAGER_H

#include <set>
#include "UserConnectionListener.h"
#include "Singleton.h"
#include "UploadManagerListener.h"
#include "ClientManager.h"
#include "ClientManagerListener.h"

class UploadQueueItem : public intrusive_ptr_base<UploadQueueItem>, // [!] IRainman fix: cleanup.
	public ColumnBase< 12 >, // [+] PPA. TODO fix me: use COLUMN_LAST.
	public UserInfoBase //[+] PPA
{
	public:
		UploadQueueItem(const HintedUser& user, const string& file, int64_t pos, int64_t size) :
			m_hintedUser(user), m_file(file), m_pos(pos), m_size(size), m_time(GET_TIME())
		{
#ifdef _DEBUG
			++g_upload_queue_item_count;
#endif
			inc();
		}
		~UploadQueueItem()
		{
#ifdef _DEBUG
			--g_upload_queue_item_count;
#endif
			//dec();
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
		static int compareItems(const UploadQueueItem* a, const UploadQueueItem* b, uint8_t col)
		{
			//+BugMaster: small optimization; fix; correct IP sorting
			switch (col)
			{
				case COLUMN_FILE:
				case COLUMN_PATH:
				case COLUMN_NICK:
				case COLUMN_HUB:
					return stricmp(a->getText(col), b->getText(col));
				case COLUMN_TRANSFERRED:
					return compare(a->m_pos, b->m_pos);
				case COLUMN_SIZE:
					return compare(a->m_size, b->m_size);
				case COLUMN_ADDED:
				case COLUMN_WAITING:
					return compare(a->m_time, b->m_time);
				case COLUMN_SLOTS:
					return compare(a->getUser()->getSlots(), b->getUser()->getSlots()); // !SMT!-UI
				case COLUMN_SHARE:
					return compare(a->getUser()->getBytesShared(), b->getUser()->getBytesShared()); // !SMT!-UI
				case COLUMN_IP:
					return compare(Socket::convertIP4(Text::fromT(a->getText(col))), Socket::convertIP4(Text::fromT(b->getText(col))));
			}
			return stricmp(a->getText(col), b->getText(col));
			//-BugMaster: small optimization; fix; correct IP sorting
			//return 0; [-] IRainman.
		}
		enum
		{
			COLUMN_FIRST,
			COLUMN_FILE = COLUMN_FIRST,
			COLUMN_PATH,
			COLUMN_NICK,
			COLUMN_HUB,
			COLUMN_TRANSFERRED,
			COLUMN_SIZE,
			COLUMN_ADDED,
			COLUMN_WAITING,
			COLUMN_LOCATION, // !SMT!-IP
			COLUMN_IP, // !SMT!-IP
#ifdef PPA_INCLUDE_DNS
			COLUMN_DNS, // !SMT!-IP
#endif
			COLUMN_SLOTS, // !SMT!-UI
			COLUMN_SHARE, // !SMT!-UI
			COLUMN_LAST
		};
		
		GETC(HintedUser, m_hintedUser, HintedUser);
		GETC(string, m_file, File);
		GETSET(int64_t, m_pos, Pos);
		GETC(int64_t, m_size, Size);
		GETC(uint64_t, m_time, Time);
		Util::CustomNetworkIndex m_location;
#ifdef _DEBUG
		static boost::atomic_int g_upload_queue_item_count;
#endif
};

class WaitingUser
#ifdef _DEBUG
// TODO : public boost::noncopyable
#endif
{
	public:
		WaitingUser(const UserPtr& user, const std::string& token, UploadQueueItem* p_uqi) : m_user(user), m_token(token)
		{
			m_files.insert(p_uqi);
		}
		operator const UserPtr&() const
		{
			return m_user;
		}
		std::set<UploadQueueItem*> m_files; // Опасно торчит указатель
		GETC(UserPtr, m_user, User);
		GETSET(string, m_token, Token);
};

class UploadManager : private ClientManagerListener, private UserConnectionListener, public Speaker<UploadManagerListener>, private TimerManagerListener, public Singleton<UploadManager>
{
#ifdef PPA_INCLUDE_DOS_GUARD
		typedef boost::unordered_map<string, uint8_t> CFlyDoSCandidatMap;
		CFlyDoSCandidatMap m_dos_map;
		mutable FastCriticalSection csDos; // [+] IRainman opt.
#endif
	public:
		/** @return Number of uploads. */
		size_t getUploadCount()
		{
			// [-] Lock l(m_csUploads); [-] IRainman opt.
			return m_uploads.size();
		}
		
		/**
		 * @remarks This is only used in the tray icons. Could be used in
		 * MainFrame too.
		 *
		 * @return Running average download speed in Bytes/s
		 */
		int64_t getRunningAverage()
		{
			return runningAverage;//[+] IRainman refactoring transfer mechanism
		}
		
		int getSlots() const
		{
			return (max(SETTING(SLOTS), max(SETTING(HUB_SLOTS), 0) * Client::getTotalCounts()));
		}
		
		/** @return Number of free slots. */
		int getFreeSlots() const
		{
			return max((getSlots() - running), 0);
		}
		
		/** @internal */
		int getFreeExtraSlots() const
		{
			return max(SETTING(EXTRA_SLOTS) - getExtra(), 0);
		}
		
		/** @param aUser Reserve an upload slot for this user and connect. */
		void reserveSlot(const HintedUser& hintedUser, uint64_t aTime);
		void unreserveSlot(const HintedUser& hintedUser);
		void clearUserFilesL(const UserPtr&);
		
		class LockInstanceQueue // [+] IRainman opt.
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
		
		typedef list<WaitingUser> SlotQueue; // [!] IRainman opt: change container to list from vector.
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
		void removeDelayUpload(const UserPtr& aUser);
		void abortUpload(const string& aFile, bool waiting = true);
		
		GETSET(int, extraPartial, ExtraPartial);
		GETSET(int, extra, Extra);
		GETSET(uint64_t, lastGrant, LastGrant);
		
		void load(); // !SMT!-S
		void save() const; // !SMT!-S
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		bool isBanReply(const UserPtr& user) const; // !SMT!-S
#endif // IRAINMAN_ENABLE_AUTO_BAN
		
		time_t getReservedSlotTime(const UserPtr& aUser) const;
	private:
		bool isFireball;
		bool isFileServer;
		int running;
		
		int64_t runningAverage;//[+] IRainman refactoring transfer mechanism
		
		uint64_t fireballStartTick;
		
		UploadList m_uploads;
		UploadList m_delayUploads;
		mutable CriticalSection m_csUploads; // [!] IRainman opt.
		
		// [+] IRainman SpeedLimiter
		typedef pair<UserPtr, unsigned int> CurrentConnectionPair;
		typedef std::unordered_map<UserPtr, unsigned int, User::Hash> CurrentConnectionMap;
		CurrentConnectionMap m_uploadsPerUser;
		
		void increaseUserConnectionAmountL(const UserPtr& p_user)
		{
			const auto i = m_uploadsPerUser.find(p_user);
			if (i != m_uploadsPerUser.end())
				i->second++;
			else
				m_uploadsPerUser.insert(CurrentConnectionPair(p_user, 1));
		}
		void decreaseUserConnectionAmountL(const UserPtr& p_user)
		{
			const auto i = m_uploadsPerUser.find(p_user);
			dcassert(i != m_uploadsPerUser.end());
			if (i != m_uploadsPerUser.end())
			{
				i->second--;
				if (i->second == 0)
					m_uploadsPerUser.erase(p_user);
			}
		}
		unsigned int getUserConnectionAmountL(const UserPtr& p_user) const
		{
			const auto i = m_uploadsPerUser.find(p_user);
			if (i != m_uploadsPerUser.end())
				return i->second;
				
			dcassert(0);
			return 1;
		}
		// [~] IRainman SpeedLimiter
		
		int lastFreeSlots; /// amount of free slots at the previous minute
		
		typedef std::unordered_map<UserPtr, uint64_t, User::Hash> SlotMap;
		
		SlotMap m_reservedSlots;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csReservedSlots;
		
		SlotMap m_notifiedUsers;
		SlotQueue m_slotQueue;
		mutable CriticalSection m_csQueue; // [+] IRainman opt.
		
		size_t addFailedUpload(const UserConnection& source, const string& file, int64_t pos, int64_t size);
		void notifyQueuedUsersL(int64_t tick);//[!]IRainman refactoring transfer mechanism add int64_t tick
		
		friend class Singleton<UploadManager>;
		UploadManager() noexcept;
		~UploadManager();
		
		bool getAutoSlot();
		void removeConnection(UserConnection* aConn);
		void removeUpload(Upload* aUpload, bool delay = false);
		void logUpload(const Upload* u);
		
		void testSlotTimeout(uint64_t aTick = GET_TICK()); // !SMT!-S
		
		// ClientManagerListener
		void on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept;
		
		// TimerManagerListener
		void on(Second, uint64_t aTick) noexcept;
		void on(Minute, uint64_t aTick) noexcept;
		
		// UserConnectionListener
		void on(BytesSent, UserConnection*, size_t p_Bytes, size_t p_Actual) noexcept;
		void on(Failed, UserConnection*, const string&) noexcept;
		void on(Get, UserConnection*, const string&, int64_t) noexcept;
		void on(Send, UserConnection*) noexcept;
		void on(TransmitDone, UserConnection*) noexcept;
		
		void on(AdcCommand::GET, UserConnection*, const AdcCommand&) noexcept;
		void on(AdcCommand::GFI, UserConnection*, const AdcCommand&) noexcept;
		
		bool prepareFile(UserConnection& aSource, const string& aType, const string& aFile, int64_t aResume, int64_t& aBytes, bool listRecursive = false);
		bool hasUpload(UserConnection& aSource, const string& p_source_file) const; // [+] FlylinkDC++
		// !SMT!-S
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		struct banmsg_t
		{
			uint32_t tick;
			int slots, share, limit, min_slots, max_slots, min_share, min_limit;
			bool same(const banmsg_t &a) const
			{
				return ((slots ^ a.slots) | (share ^ a.share) | (limit ^ a.limit) |
				        (min_slots ^ a.min_slots) |
				        (max_slots ^ a.max_slots) |
				        (min_share ^ a.min_share) | (min_limit ^ a.min_limit)) == 0;
			}
		};
		typedef boost::unordered_map<string, banmsg_t> BanMap;
		bool handleBan(UserConnection& aSource/*, bool forceBan, bool noChecks*/);
		static bool hasAutoBan(const UserPtr& aUser);
		BanMap m_lastBans;
		static std::unique_ptr<webrtc::RWLockWrapper> g_csBans;
#endif // IRAINMAN_ENABLE_AUTO_BAN
};

#endif // !defined(UPLOAD_MANAGER_H)

/**
 * @file
 * $Id: UploadManager.h 578 2011-10-04 14:27:51Z bigmuscle $
 */
