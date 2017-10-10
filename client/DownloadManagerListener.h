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


#ifndef DCPLUSPLUS_DCPP_DOWNLOADMANAGERLISTENER_H_
#define DCPLUSPLUS_DCPP_DOWNLOADMANAGERLISTENER_H_

#include "typedefs.h"
#include "User.h"
#include "TransferData.h"
#include "Download.h"

#include "libtorrent\torrent_info.hpp"
/**
 * Use this listener interface to get progress information for downloads.
 *
 * @remarks All methods are sending a pointer to a Download but the receiver
 * (TransferView) is not using any of the methods in Download, only methods
 * from its super class, Transfer. The listener functions should send Transfer
 * objects instead.
 *
 * Changing this will will cause a problem with Download::List which is used
 * in the on Tick function. One solution is reimplement on Tick to call once
 * for every Downloads, sending one Download at a time. But maybe updating the
 * GUI is not DownloadManagers problem at all???
 */

struct CFlyTorrentFile
{
	std::string m_file_path;
	int64_t m_size = 0;
};
typedef std::vector<CFlyTorrentFile> CFlyTorrentFileArray;
class DownloadManagerListener
{
	public:
		virtual ~DownloadManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Complete;
		typedef X<1> Failed;
#ifdef FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE
		typedef X<2> Starting;
#endif
		typedef X<3> Tick;
		typedef X<4> Requesting;
		typedef X<5> Status;
		typedef X<6> RemoveToken;
		typedef X<7> TorrentEvent;
		typedef X<8> RemoveTorrent;
		typedef X<9> SelectTorrent;
		typedef X<10> AddedTorrent;
		typedef X<11> CompleteTorrentFile;
		
		/**
		 * This is the first message sent before a download starts.
		 * No other messages will be sent before this.
		 */
		virtual void on(Requesting, const DownloadPtr& aDownload) noexcept { }
		virtual void on(RemoveToken, const std::string& p_token) noexcept { }
		virtual void on(CompleteTorrentFile, const std::string& p_name) noexcept { }
		
		virtual void on(RemoveTorrent, const libtorrent::sha1_hash& p_sha1) noexcept { }
		virtual void on(SelectTorrent, const libtorrent::sha1_hash& p_sha1, CFlyTorrentFileArray& p_files,
		                std::shared_ptr<const libtorrent::torrent_info> p_torrent_info) noexcept { }
		virtual void on(AddedTorrent, const libtorrent::sha1_hash& p_sha1, const std::string& p_save_path) noexcept { }
		
#ifdef FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE
		/**
		 * This is the first message sent before a download starts.
		 */
		virtual void on(Starting, const DownloadPtr& aDownload) noexcept { }
#endif
		
		/**
		 * Sent once a second if something has actually been downloaded.
		 */
		virtual void on(Tick, const DownloadArray&) noexcept { }
		
		virtual void on(TorrentEvent, const DownloadArray&) noexcept { }
		
		/**
		 * This is the last message sent before a download is deleted.
		 * No more messages will be sent after it.
		 */
		virtual void on(Complete, const DownloadPtr& aDownload) noexcept { }
		
		/**
		 * This indicates some sort of failure with a particular download.
		 * No more messages will be sent after it.
		 *
		 * @remarks Should send an error code instead of a string and let the GUI
		 * display an error string.
		 */
		virtual void on(Failed, const DownloadPtr& aDownload, const string&) noexcept { }
		
		virtual void on(Status, const UserConnection*, const string&) noexcept { }
};

#endif /*DOWNLOADMANAGERLISTENER_H_*/
