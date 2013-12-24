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
			COLUMN_DONE,
			COLUMN_PATH,
			COLUMN_NICK,
			COLUMN_HUB,
			COLUMN_SIZE,
			COLUMN_SPEED,
			COLUMN_IP, //[+] PPA
#ifdef IRAINMAN_AV_CHECK
			COLUMN_ANTIVIR_CHECK,
#endif
			COLUMN_LAST
		};
		
		FinishedItem(const string& aTarget, const HintedUser& aUser, int64_t aSize, int64_t aSpeed, time_t aTime, const string& aTTH = Util::emptyString, const string& aIP = Util::emptyString) :
			target(aTarget),
			cid(aUser.user->getCID()),
			hub(aUser.hint),
			hubs(Util::toString(ClientManager::getHubNames(aUser.user->getCID(), Util::emptyString))),
			size(aSize), avgSpeed(aSpeed), time(aTime), tth(aTTH), ip(aIP), nick(aUser.user->getLastNick())
		{
#ifdef IRAINMAN_AV_CHECK
			if (SETTINGS(USE_ANTIVIR) && SETTINGS(ANTIVIR_AUTO_CHECK))
				setAVchecked(::ShellExecute(NULL, NULL, Text::toT(SETTINGS(ANTIVIR_PATH)).c_str(), Text::toT('\"' + target + '\"').c_str(), NULL, SW_HIDE) != -1);
			else
				setAVchecked(false);
#endif // IRAINMAN_AV_CHECK
		}
		
		const tstring getText(int col) const
		{
			dcassert(col >= 0 && col < COLUMN_LAST);
			switch (col)
			{
				case COLUMN_FILE:
					return Text::toT(Util::getFileName(getTarget()));
				case COLUMN_DONE:
					return Text::toT(Util::formatDigitalClock(getTime()));
				case COLUMN_PATH:
					return Text::toT(Util::getFilePath(getTarget()));
				case COLUMN_NICK:
					return Text::toT(getNick());
				case COLUMN_HUB:
					return Text::toT(getHubs());
				case COLUMN_SIZE:
					return Util::formatBytesW(getSize());
				case COLUMN_SPEED:
					return Util::formatBytesW(getAvgSpeed()) + _T('/') + WSTRING(S);
				case COLUMN_IP:
					return Text::toT(getIP()); //[+]PPA
#ifdef IRAINMAN_AV_CHECK
				case COLUMN_ANTIVIR_CHECK:
					return getAVchecked() ? TSTRING(CHECKED) : TSTRING(NOT_CHECKED);
#endif
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
				default:
					return lstrcmpi(a->getText(col).c_str(), b->getText(col).c_str());
			}
		}
		int getImageIndex() const;
		
		GETC(string, target, Target);
		GETC(string, tth, Tth);
		GETC(string, ip, IP); // [+] PPA
		GETC(string, nick, Nick);
		GETC(string, hubs, Hubs); // [+] http://code.google.com/p/flylinkdc/issues/detail?id=981
		GETC(string, hub, Hub);
		GETC(CID, cid, CID);
		GETC(int64_t, size, Size);
		GETC(int64_t, avgSpeed, AvgSpeed);
		GETC(time_t, time, Time);
#ifdef IRAINMAN_AV_CHECK
		GETSET(bool, avChecked, AVchecked);
#endif
		
	private:
		friend class FinishedManager;
		
};

class FinishedManager : public Singleton<FinishedManager>,
	public Speaker<FinishedManagerListener>, private QueueManagerListener, private UploadManagerListener
{
	public:
		const FinishedItemList& lockList(const bool upload)
		{
			if (upload)
			{
				csU.lockShared();
				return m_uploads;
			}
			else
			{
				csD.lockShared();
				return m_downloads;
			}
		}
		void unlockList(const bool upload)
		{
			if (upload)
			{
				csU.unlockShared();
			}
			else
			{
				csD.unlockShared();
			}
		}
		
		void remove(FinishedItemPtr item, bool upload = false);
		void removeAll(bool upload = false);
		
	private:
		friend class Singleton<FinishedManager>;
		
		FinishedManager();
		~FinishedManager();
		
		void on(QueueManagerListener::Finished, const QueueItemPtr&, const string&, const Download*) noexcept;
		void on(UploadManagerListener::Complete, const Upload*) noexcept;
		
#ifdef IRAINMAN_USE_SEPARATE_CS_IN_FINISHED_MANAGER
		FastSharedCriticalSection csD, csU; // [!] IRainman opt: add separate cs.
#else
		FastSharedCriticalSection csD;
		FastSharedCriticalSection& csU;
#endif
		FinishedItemList m_downloads;
		FinishedItemList m_uploads;
};

#endif // !defined(FINISHED_MANAGER_H)

/**
 * @file
 * $Id: FinishedManager.h 571 2011-07-27 19:18:04Z bigmuscle $
 */
