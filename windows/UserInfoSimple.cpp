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


#include "stdafx.h"
#include "../client/UploadManager.h"

#include "UserInfoSimple.h"
#include "PrivateFrame.h"
#include "LineDlg.h"

// !SMT!-UI
void UserInfoSimple::addSummaryMenu()
{
	// TODO: move obtain information about the user in the UserManager
	if (!getUser())
		return;
		
	CWaitCursor l_cursor_wait; //-V808
	
	UserInfoGuiTraits::userSummaryMenu.InsertSeparatorLast(getUser()->getLastNickT());
	
	ClientManager::UserParams l_params;
	if (ClientManager::getUserParams(getUser(), l_params))
	{
		tstring userInfo = TSTRING(SLOTS) + _T(": ") + Util::toStringW(l_params.m_slots) + _T(", ") + TSTRING(SHARED) + _T(": ") + Util::formatBytesW(l_params.m_bytesShared);
		
		if (l_params.m_limit)
		{
			userInfo += _T(", ") + TSTRING(SPEED_LIMIT) + _T(": ") + Util::formatBytesW(l_params.m_limit) + _T('/') + WSTRING(SEC);
		}
		
		UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, userInfo.c_str());
		
		const time_t slot = UploadManager::getReservedSlotTime(getUser());
		if (slot)
		{
			const tstring note = TSTRING(EXTRA_SLOT_TIMEOUT) + _T(": ") + Util::formatSecondsW((slot - GET_TICK()) / 1000);
			UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, note.c_str());
		}
		
		if (!l_params.m_ip.empty())
		{
			UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, l_params.getTagIP().c_str());
			
			const Util::CustomNetworkIndex l_location = Util::getIpCountry(l_params.m_ip, true); // Не обращаемся в базу данных
			const tstring loc = TSTRING(COUNTRY) + _T(": ") + l_location.getCountry() + _T(", ") + l_location.getDescription();
			UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, loc.c_str());
			
			HubFrame::addDupeUsersToSummaryMenu(l_params);
		}
		else
		{
			UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, l_params.getTagIP().c_str());
			HubFrame::addDupeUsersToSummaryMenu(l_params);
		}
	}
	
	//UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_SEPARATOR);
	
	bool caption = false;
	{
		UploadManager::LockInstanceQueue lockedInstance;
		const auto& users = lockedInstance->getUploadQueueL();
		for (auto uit = users.cbegin(); uit != users.cend(); ++uit)
		{
			if (uit->getUser() == getUser())
			{
				uint8_t l_count_menu = 0; // [+]PPA
				for (auto i = uit->m_waiting_files.cbegin(); i != uit->m_waiting_files.cend(); ++i)
				{
					if (!caption)
					{
						UserInfoGuiTraits::userSummaryMenu.InsertSeparatorLast(TSTRING(USER_WAIT_MENU));
						caption = true;
					}
					const tstring note =
					    _T('[') +
					    Util::toStringW((double)(*i)->getPos() * 100.0 / (double)(*i)->getSize()) +
					    _T("% ") +
					    Util::formatSecondsW(GET_TIME() - (*i)->getTime()) +
					    _T("]\t") +
					    Text::toT((*i)->getFile());
					UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, note.c_str());
					if (l_count_menu++ == 10) //[+]PPA
					{
						UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, _T("..."));
						break;
					}
				}
			}
		}
	}
	caption = false;
	{
		uint8_t l_count_menu = 0; // [+]PPA
		RLock(*QueueItem::g_cs);
		QueueManager::LockFileQueueShared l_fileQueue;
		const auto& downloads = l_fileQueue.getQueueL();
		for (auto j = downloads.cbegin(); j != downloads.cend(); ++j)
		{
			const QueueItemPtr& aQI = j->second;
			const bool src = aQI->isSourceL(getUser());
			bool badsrc = false;
			if (!src)
			{
				badsrc = aQI->isBadSourceL(getUser());
			}
			if (src || badsrc)
			{
				if (!caption)
				{
					UserInfoGuiTraits::userSummaryMenu.InsertSeparatorLast(TSTRING(NEED_USER_FILES_MENU));
					caption = true;
				}
				tstring note = Text::toT(aQI->getTarget());
				if (aQI->getSize() > 0)
				{
					note += tstring(_T("\t(")) + Util::toStringW((double)aQI->getDownloadedBytes() * 100.0 / (double)aQI->getSize()) + tstring(_T("%)"));
				}
				const UINT flags = MF_STRING | MF_DISABLED | (badsrc ? MF_GRAYED : 0);
				UserInfoGuiTraits::userSummaryMenu.AppendMenu(flags, IDC_NONE, note.c_str());
				if (l_count_menu++ == 10) //[+]PPA
				{
					UserInfoGuiTraits::userSummaryMenu.AppendMenu(MF_STRING | MF_DISABLED, IDC_NONE, _T("..."));
					break;
				}
			}
		}
	}
}
// !SMT!-S
tstring UserInfoSimple::getBroadcastPrivateMessage()
{
	tstring deftext;
	
	LineDlg dlg;
	dlg.description = TSTRING(PRIVATE_MESSAGE);
	dlg.title = TSTRING(SEND_TO_ALL_USERS);
	dlg.line = deftext;
	
	if (dlg.DoModal() == IDOK)
	{
		deftext = dlg.line;
		return deftext;
	}
	else
	{
		return Util::emptyStringT;
	}
}

uint64_t UserInfoSimple::inputSlotTime()
{
	static tstring deftext = _T("00:30");
	
	LineDlg dlg;
	dlg.description = TSTRING(EXTRA_SLOT_TIME_FORMAT);
	dlg.title = TSTRING(EXTRA_SLOT_TIMEOUT);
	dlg.line = deftext;
	
	if (dlg.DoModal() == IDOK)
	{
		deftext = dlg.line;
		unsigned int n = 0;
		for (size_t i = 0; i < deftext.length(); i++) // TODO: cleanup.
		{
			if (deftext[i] == L':') n++;
		}
		unsigned int d, h, m;
		switch (n)
		{
			case 1:
				if (swscanf(deftext.c_str(), L"%u:%u", &h, &m) == 2)
					return (h * 3600 + m * 60);
					
				break;
			case 2:
				if (swscanf(deftext.c_str(), L"%u:%u:%u", &d, &h, &m) == 3)
					return (d * 3600 * 24 + h * 3600 + m * 60);
					
				break;
		}
		::MessageBox(GetForegroundWindow(), CTSTRING(INVALID_TIME_FORMAT), CTSTRING(ERRORS), MB_OK | MB_ICONERROR);
	}
	return 0;
}

// [+] FlylinkDC++ Team

#if 0
int UploadQueueItemInfo::getImageIndex() const
{
	return g_fileImage.getIconIndex(getQi()->getFile());
}


void UploadQueueItemInfo::update()
{
	setText(COLUMN_FILE, Text::toT(Util::getFileName(getQi()->getFile())));
	setText(COLUMN_PATH, Text::toT(Util::getFilePath(getQi()->getFile())));
	setText(COLUMN_NICK, getQi()->getUser()->getLastNickT()); // [1] https://www.box.net/shared/plriwg50qendcr3kbjp5
	setText(COLUMN_HUB, WinUtil::getHubNames(getQi()->getHintedUser()).first); // [!] IRainman fix: hintedUser
	setText(COLUMN_TRANSFERRED, Util::formatBytesW(getQi()->getPos()) + _T(" (") + Util::toStringW((double)getQi()->getPos() * 100.0 / (double)getQi()->getSize()) + _T("%)"));
	setText(COLUMN_SIZE, Util::formatBytesW(getQi()->getSize()));
	setText(COLUMN_ADDED, Text::toT(Util::formatDigitalClock(getQi()->getTime())));
	setText(COLUMN_WAITING, Util::formatSecondsW(GET_TIME() - getQi()->getTime()));
	setText(COLUMN_SHARE, Util::formatBytesW(getQi()->getUser()->getBytesShared())); //[+]PPA
	setText(COLUMN_SLOTS, Util::toStringW(getQi()->getUser()->getSlots())); //[+]PPA
	// !SMT!-IP
	if (!m_location.isSet() && !m_ip.empty()) // [!] IRainman opt: Prevent multiple repeated requests to the database if the location has not been found!
	{
		m_location = Util::getIpCountry(m_ip);
		setText(COLUMN_IP, Text::toT(m_ip));
	}
	if (m_location.isKnown())
	{
		setText(COLUMN_LOCATION, m_location.getDescription());
	}
#ifdef FLYLINKDC_USE_DNS
	// [!] IRainman opt.
	if (m_dns.empty())
	{
		m_dns = Socket::nslookup(m_ip);
		setText(COLUMN_DNS, Text::toT(m_dns)); // todo: paint later if not resolved yet
	}
	// [~] IRainman opt.
#endif
}
#endif

/* [-] IRainman opt.
const tstring UploadQueueItemInfo::getText(uint8_t col) const
{
    switch (col)
    {
        case COLUMN_FILE:
            return Text::toT(Util::getFileName(getQi()->getFile()));
        case COLUMN_PATH:
            return Text::toT(Util::getFilePath(getQi()->getFile()));
        case COLUMN_NICK:
            return WinUtil::getNicks(getQi()->getHintedUser());
        case COLUMN_HUB:
            return WinUtil::getHubNames(getQi()->getHintedUser()).first;
        case COLUMN_SIZE:
            return Util::formatBytesW(getQi()->getSize());
        case COLUMN_ADDED:
            return Text::toT(Util::formatDigitalClock(getQi()->getTime()));
        case COLUMN_TRANSFERRED:
            return Util::formatBytesW(getQi()->getPos()) + _T(" (") + Util::toStringW((double)getQi()->getPos() * 100.0 / (double)getQi()->getSize()) + _T("%)");
        case COLUMN_WAITING:
            return Util::formatSecondsW(GET_TIME() - getQi()->getTime());
        case COLUMN_SHARE:
            return Util::formatBytesW(getQi()->getUser()->getBytesShared());
        case COLUMN_SLOTS:
            return Util::toStringW(getQi()->getUser()->getSlots());
        case COLUMN_IP:
            return Text::toT(m_ip);
        case COLUMN_LOCATION:
        {
            // !SMT!-IP
            if (!m_location.isSet() && !m_ip.empty()) // [!] IRainman opt: Prevent multiple repeated requests to the database if the location has not been found!
            {
                m_location = Util::getIpCountry(m_ip);
            }
            return m_location.isKnown() ? m_location.getDescription() : Util::emptyStringT;
        }
#ifdef FLYLINKDC_USE_DNS
        case COLUMN_DNS:
        {
            if (m_dns.empty())
            {
                m_dns = Socket::nslookup(m_ip);
            }
            return Text::toT(m_dns);
        }
#endif
    }
    return ColumnBase::getText(col);
}
[~] IRainman opt. */

// [~] FlylinkDC++ Team