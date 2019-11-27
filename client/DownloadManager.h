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

#ifndef DCPLUSPLUS_DCPP_DOWNLOAD_MANAGER_H
#define DCPLUSPLUS_DCPP_DOWNLOAD_MANAGER_H

#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/torrent_handle.hpp"
#define FLYLKINKDC_USE_TORRENT_AGENTS_CONC_TIMER

#ifdef FLYLKINKDC_USE_TORRENT_AGENTS_CONC_TIMER
#include <agents.h>
#endif

#include "DownloadManagerListener.h"
#include "TimerManager.h"
#include "UserConnection.h"
#include "ZUtils.h"
#include "FilteredFile.h"

#ifndef FLYLINKDC_USE_TORRENT
#define FLYLINKDC_USE_TORRENT
#endif

#endif


/**
 * Singleton. Use its listener interface to update the download list
 * in the user interface.
 */
typedef std::vector<UserConnection*> UserConnectionList;

class DownloadManager : public Speaker<DownloadManagerListener>,
	private UserConnectionListener, private TimerManagerListener,
	public Singleton<DownloadManager>
{
#ifdef FLYLINKDC_USE_TORRENT
		std::unique_ptr<libtorrent::session> m_torrent_session;
		libtorrent::port_mapping_t m_maping_index[3];
		void select_files(const libtorrent::torrent_handle& p_torrent_handle);
		std::atomic<int> m_torrent_resume_count = { 0 };
		std::atomic<int> m_torrent_rename_count = { 0 };
		std::unordered_set<libtorrent::torrent_handle> m_torrents;
	public:
		void init_torrent(bool p_is_force = false);
		void shutdown_torrent();
		bool add_torrent_file(const tstring& p_torrent_path, const tstring& p_torrent_url);
		bool remove_torrent_file(const libtorrent::sha1_hash& p_sha1, libtorrent::remove_flags_t p_options);
		bool pause_torrent_file(const libtorrent::sha1_hash& p_sha1, bool p_is_resume);
		bool set_file_priority(const libtorrent::sha1_hash& p_sha1, const CFlyTorrentFileArray& p_files,
		                       const std::vector<libtorrent::download_priority_t>& p_file_priority, const std::string& p_save_path);
		int listen_torrent_port();
		int ssl_listen_torrent_port();
		void fire_added_torrent(const libtorrent::sha1_hash& p_sha1);
		std::string get_torrent_name(const libtorrent::sha1_hash& p_sha1);
		std::string get_torrent_magnet(const libtorrent::sha1_hash& p_sha1);
#endif
		
	public:
		/** @internal */
		void addConnection(UserConnection* p_conn);
		static void checkIdle(const UserPtr& user);
		
		/** @internal */
		static void abortDownload(const std::string& aTarget);
		
		/** @return Running average download speed in Bytes/s */
		static int64_t getRunningAverage()
		{
			return g_runningAverage;
		}
		
		static size_t getDownloadCount();
		
		static bool isStartDownload(QueueItem::Priority prio);
		static bool checkFileDownload(const UserPtr& aUser);
		void fireData(UserConnection*, const uint8_t*, size_t) noexcept;
	private:
		void onTorrentAlertNotify();
		
		static std::unique_ptr<webrtc::RWLockWrapper> g_csDownload;
		static DownloadList g_download_map;
		static UserConnectionList g_idlers;
		static void remove_idlers(UserConnection* aSource);
		
		static int64_t g_runningAverage;
		
		void removeConnection(UserConnection* p_conn, bool p_is_remove_listener = true);
		static void removeDownload(const DownloadPtr& aDownload);
		void fileNotAvailable(UserConnection* aSource);
		void noSlots(UserConnection* aSource, const string& param = Util::emptyString);
		
		int64_t getResumePos(const string& file, const TigerTree& tt, int64_t startPos);
		
		void failDownload(UserConnection* aSource, const string& reason);
		
		friend class Singleton<DownloadManager>;
		
		DownloadManager();
		~DownloadManager();
		
		void checkDownloads(UserConnection* aConn);
		void startData(UserConnection* aSource, int64_t start, int64_t newSize, bool z);
		void endData(UserConnection* aSource);
		
		void onFailed(UserConnection* aSource, const string& aError);
		
		// UserConnectionListener
		void on(Data, UserConnection*, const uint8_t*, size_t) noexcept override;
		void on(Failed, UserConnection* aSource, const string& aError) noexcept override
		{
			onFailed(aSource, aError);
		}
		void on(ProtocolError, UserConnection* aSource, const string& aError) noexcept override
		{
			onFailed(aSource, aError);
		}
		void on(MaxedOut, UserConnection*, const string& param) noexcept override;
		void on(FileNotAvailable, UserConnection*) noexcept override;
		void on(ListLength, UserConnection* aSource, const string& aListLength) noexcept override;
		void on(Updated, UserConnection*) noexcept override;
		
		void on(AdcCommand::SND, UserConnection*, const AdcCommand&) noexcept override;
		void on(AdcCommand::STA, UserConnection*, const AdcCommand&) noexcept override;
		
		// TimerManagerListener
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept override;
		void on(CheckUserIP, UserConnection*) noexcept override;
	public:
		bool alert_handler();
		void post_torrent_info();
	private:
#ifdef FLYLKINKDC_USE_TORRENT_AGENTS_CONC_TIMER
		concurrency::call<void*> alert_caller_;
		concurrency::timer<void*> alert_timer_;
		CriticalSection m_cs_alert;
#endif
		bool m_is_torrent_alert_active = false;
		
		void start_alert_handler();
		void stop_alert_handler();
		
};

#endif // !defined(DOWNLOAD_MANAGER_H)

/**
 * @file
 * $Id: DownloadManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
