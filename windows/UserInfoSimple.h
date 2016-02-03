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

#ifndef USERINFOSIMPLE_H
#define USERINFOSIMPLE_H

#pragma once


#include "../client/UserInfo.h"
#include "../client/UploadManager.h"

class UserInfoSimple: public UserInfoBase
{
		// [->] IRainman moved from client/UserInfoBase.
	public:
		explicit UserInfoSimple(const HintedUser& p_hinted_user) : m_hintedUser(p_hinted_user)
		{
		}
		explicit UserInfoSimple(const UserPtr& p_user, const string& p_hub_hint) : m_hintedUser(HintedUser(p_user, p_hub_hint))
		{
		}
		explicit UserInfoSimple(const OnlineUserPtr& p_user, const string& p_hub_hint) : m_hintedUser(HintedUser(p_user->getUser(), p_hub_hint)) // [+] IRainman fix.
		{
			m_hintedUser.user->setLastNick(p_user->getIdentity().getNick());
		}
		
		void addSummaryMenu(); // !SMT!-UI
		static const tstring& getBroadcastPrivateMessage(); // !SMT!-S
		static uint64_t inputSlotTime(); // !SMT!-UI
		
		const UserPtr& getUser() const
		{
			return m_hintedUser.user;
		}
		
		const HintedUser m_hintedUser;
};

#if 0 // http://code.google.com/p/flylinkdc/issues/detail?id=1413
class UploadQueueItemInfo : public UserInfoBase // [<-] IRainman fix: moved from kernel and cleanup.
	, public ColumnBase< 13 >
{
	public:
		UploadQueueItemInfo(UploadQueueItemPtr queueItem) : m_queueItem(queueItem)
		{
			m_queueItem->inc();
			{
				int dummy_param_limit;
				if (ClientManager::getUserParams(getQi()->getHintedUser(), m_share, m_slots, dummy_param_limit, m_ip)) // [!] IRainman opt.
				{
#ifdef PPA_INCLUDE_DNS
					// [-] dns = Socket::nslookup(ip); [-] IRainman opt.
#endif
					// [-] location = Util::getIpCountry(ip); [-] IRainman opt.
				}
				else
				{
					m_share = 0;
					m_slots = 0;
					// [-] ip = Util::emptyString; [-] IRainman opt.
#ifdef PPA_INCLUDE_DNS
					// [-] dns = Util::emptyString; [-] IRainman opt.
#endif
				}
			}
		}
		
		~UploadQueueItemInfo()
		{
			m_queueItem->dec();
		}
		
		bool operator==(const UploadQueueItemPtr& item) const
		{
			return getQi() == item;
		}
		bool operator==(const UploadQueueItemInfo* item) const
		{
			return getQi() == item->getQi();
		}
		
		void update();
		
		static int compareItems(const UploadQueueItemInfo* a, const UploadQueueItemInfo* b, uint8_t col)
		{
			//+BugMaster: small optimization; fix; correct IP sorting
			switch (col)
			{
				case COLUMN_TRANSFERRED:
					return compare(a->getQi()->getPos(), b->getQi()->getPos());
				case COLUMN_SIZE:
					return compare(a->getQi()->getSize(), b->getQi()->getSize());
				case COLUMN_ADDED:
				case COLUMN_WAITING:
					return compare(a->getQi()->getTime(), b->getQi()->getTime());
				case COLUMN_SLOTS:
					return compare(a->getSlots(), b->getSlots()); // !SMT!-UI
				case COLUMN_SHARE:
					return compare(a->getShare(), b->getShare()); // !SMT!-UI
				case COLUMN_IP:
					return compare(Socket::convertIP4(Text::fromT(a->getText(col))), Socket::convertIP4(Text::fromT(b->getText(col))));
			}
			return stricmp(a->getText(col), b->getText(col));
			//-BugMaster: small optimization; fix; correct IP sorting
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
		
		const UserPtr& getUser() const
		{
			return getQi()->getUser();
		}
		
		const tstring getText(uint8_t col) const // [!] IRainman opt.
		{
			return ColumnBase::getText(col);
		}
		
		int getImageIndex() const;
		
		mutable Util::CustomNetworkIndex m_location; // [!] IRainman opt !SMT!-IP
		GETM(string, m_ip, Ip);
#ifdef PPA_INCLUDE_DNS
		GETM(string, m_dns, Dns);
#endif
		GETM(UploadQueueItemPtr, m_queueItem, Qi);
		GETM(uint64_t, m_share, Share); // !SMT!-UI
		GETM(int, m_slots, Slots); // !SMT!-UI
};

#endif
#endif