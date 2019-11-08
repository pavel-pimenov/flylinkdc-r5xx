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


#ifndef DCPLUSPLUS_DCPP_FINISHED_MANAGER_H
#define DCPLUSPLUS_DCPP_FINISHED_MANAGER_H

#include "QueueManagerListener.h"
#include "UploadManagerListener.h"
#include "Singleton.h"
#include "FinishedManagerListener.h"
#include "User.h"
#include "ClientManager.h"


class FinishedItem
{
	public:
		enum
		{
			COLUMN_FIRST,
			COLUMN_FILE = COLUMN_FIRST,
			COLUMN_TYPE,
			COLUMN_DONE,
			COLUMN_PATH,
			COLUMN_TTH,
			COLUMN_NICK,
			COLUMN_HUB,
			COLUMN_SIZE,
			COLUMN_SPEED,
			COLUMN_IP,
			COLUMN_NETWORK_TRAFFIC,
			COLUMN_LAST
		};
		
		FinishedItem(const string& aTarget, const libtorrent::sha1_hash& p_sha1, int64_t aSize, int64_t aSpeed, time_t aTime, int64_t aActual, int64_t aID) :
			target(aTarget),
			size(aSize),
			avgSpeed(aSpeed),
			time(aTime),
			actual(aActual),
			id(aID),
			m_sha1(p_sha1)
		{
		}
		
		FinishedItem(const string& aTarget, const string& aNick, const string& aHubUrl, int64_t aSize, int64_t aSpeed,
		             const time_t aTime, const TTHValue& aTTH, const string& aIP, int64_t aID, int64_t aActual) :
			target(aTarget),
			hub(aHubUrl),
			hubs(aHubUrl),
			size(aSize),
			avgSpeed(aSpeed),
			time(aTime),
			tth(aTTH),
			ip(aIP),
			nick(aNick),
			id(aID),
			actual(aActual)
		{
		}
		FinishedItem(const string& aTarget, const HintedUser& aUser, int64_t aSize, int64_t aSpeed,
		             const time_t aTime, const TTHValue& aTTH, int64_t aActual, const string& aIP = Util::emptyString) :
			target(aTarget),
			cid(aUser.user->getCID()),
			hub(aUser.hint),
			hubs(aUser.user ? Util::toString(ClientManager::getHubNames(aUser.user->getCID(), Util::emptyString)) : Util::emptyString),
			size(aSize),
			avgSpeed(aSpeed),
			time(aTime),
			tth(aTTH),
			ip(aIP),
			nick(aUser.user->getLastNick()),
			id(0),
			actual(aActual)
		{
		}
		
		const tstring getText(int col) const
		{
			dcassert(col >= 0 && col < COLUMN_LAST);
			switch (col)
			{
				case COLUMN_FILE:
					return Text::toT(Util::getFileName(getTarget()));
				case COLUMN_TYPE:
					return Text::toT(Util::getFileExtWithoutDot(getTarget()));
				case COLUMN_DONE:
					return Text::toT(id ? Util::formatDigitalClockGMT(getTime()) : Util::formatDigitalClock(getTime()));
				case COLUMN_PATH:
					return Text::toT(Util::getFilePath(getTarget()));
				case COLUMN_NICK:
					return Text::toT(getNick());
				case COLUMN_HUB:
					return Text::toT(getHubs());
				case COLUMN_SIZE:
					return Util::formatBytesW(getSize());
				case COLUMN_NETWORK_TRAFFIC:
					if (getActual())
						return Util::formatBytesW(getActual());
					else
						return Util::emptyStringT;
				case COLUMN_SPEED:
					if (getAvgSpeed())
						return Util::formatBytesW(getAvgSpeed()) + _T('/') + WSTRING(S);
					else
						return Util::emptyStringT;
				case COLUMN_IP:
					return Text::toT(getIP());
				case COLUMN_TTH:
				{
					if (getTTH() != TTHValue())
						return Text::toT(getTTH().toBase32());
					else
						return Util::emptyStringT;
				}
				default:
					return Util::emptyStringT;
			}
		}
		
		static int compareItems(const FinishedItem* a, const FinishedItem* b, int col)
		{
			switch (col)
			{
				case COLUMN_SPEED:
					return compare(a->getAvgSpeed(), b->getAvgSpeed());
				case COLUMN_SIZE:
					return compare(a->getSize(), b->getSize());
				case COLUMN_NETWORK_TRAFFIC:
					return compare(a->getActual(), b->getActual());
				default:
					return lstrcmpi(a->getText(col).c_str(), b->getText(col).c_str());
			}
		}
		int getImageIndex() const;
		GETC(string, target, Target);
		GETC(TTHValue, tth, TTH);
		GETC(string, ip, IP);
		GETC(string, nick, Nick);
		GETC(string, hubs, Hubs);
		GETC(string, hub, Hub);
		GETC(CID, cid, CID);
		GETC(int64_t, size, Size);
		GETC(int64_t, avgSpeed, AvgSpeed);
		GETC(time_t, time, Time);
		GETC(int64_t, id, ID);
		GETC(int64_t, actual, Actual); // Socket Bytes!
		libtorrent::sha1_hash m_sha1;
		bool is_torrent() const
		{
			return !m_sha1.is_all_zeros();
		}
	private:
		friend class FinishedManager;
};

class FinishedManager : public Singleton<FinishedManager>,
	public Speaker<FinishedManagerListener>, private QueueManagerListener, private UploadManagerListener
{
	public:
		enum eType
		{
			e_Download = 0,
			e_Upload = 1
		};
		static const FinishedItemList& lockList(eType p_type)
		{
			g_cs[p_type]->AcquireLockShared();
			return g_finished[p_type];
		}
		static void unlockList(eType p_upload)
		{
			g_cs[p_upload]->ReleaseLockShared();
		}
		
		static void removeItem(const FinishedItemPtr& p_item, eType p_type);
		static void removeAll(eType p_type);
		void pushHistoryFinishedItem(const FinishedItemPtr& p_item, int p_type);
		void updateStatus()
		{
			fly_fire(FinishedManagerListener::UpdateStatus());
		}
		
	private:
		friend class Singleton<FinishedManager>;
		
		FinishedManager();
		~FinishedManager();
		
		void on(QueueManagerListener::Finished, const QueueItemPtr&, const string&, const DownloadPtr& aDownload) noexcept override;
		void on(UploadManagerListener::Complete, const UploadPtr& aUpload) noexcept override;
		
		string log(const CID& p_CID, const string& p_path, const string& p_message);
		void rotation_items(const FinishedItemPtr& p_item, eType p_type);
		
		static std::unique_ptr<webrtc::RWLockWrapper> g_cs[2]; // index = eType
		static FinishedItemList g_finished[2]; // index = eType
};

#endif // !defined(FINISHED_MANAGER_H)

/**
 * @file
 * $Id: FinishedManager.h 571 2011-07-27 19:18:04Z bigmuscle $
 */
