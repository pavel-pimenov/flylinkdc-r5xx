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

#include "stdinc.h"
#include "SearchResult.h"
#include "UploadManager.h"
#include "Text.h"
#include "User.h"
#include "ClientManager.h"
#include "Client.h"
#include "CFlylinkDBManager.h"
#include "ShareManager.h"


SearchResultBaseTTH::SearchResultBaseTTH(Types aType, int64_t aSize, const string& aFile, const TTHValue& aTTH, uint8_t aSlots /* = 0 */, uint8_t aFreeSlots /* = 0 */):
	m_file(aFile),
	m_size(aSize),
	m_tth(aTTH),
	m_slots(aSlots),
	m_freeSlots(aFreeSlots),
	m_type(aType)
{
}

void SearchResultBaseTTH::initSlot()
{
	m_slots     = UploadManager::getSlots();
	m_freeSlots = UploadManager::getFreeSlots();
}

string SearchResultBaseTTH::toSR(const Client& c) const
{
	// File:        "$SR %s %s%c%s %d/%d%c%s (%s)|"
	// Directory:   "$SR %s %s %d/%d%c%s (%s)|"
	string tmp;
	tmp.reserve(128 + getFile().size());
	tmp.append("$SR ", 4);
//#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
//	tmp.append(c.getMyNick());
//#else
	tmp.append(Text::fromUtf8(c.getMyNick(), c.getEncoding()));
//#endif
	tmp.append(1, ' ');
//#ifdef IRAINMAN_USE_UNICODE_IN_NMDC
//	const string& acpFile = file;
//#else
	const string acpFile = Text::fromUtf8(getFile(), c.getEncoding());
//#endif
	if (m_type == TYPE_FILE)
	{
		tmp.append(acpFile);
		tmp.append(1, '\x05');
		tmp.append(Util::toString(getSize()));
	}
	else
	{
		tmp.append(acpFile, 0, acpFile.length() - 1);
	}
	//dcassert(getFreeSlots() != 0 && getSlots() != 0);
	tmp.append(1, ' ');
	tmp.append(Util::toString(getFreeSlots()));
	tmp.append(1, '/');
	tmp.append(Util::toString(getSlots()));
	tmp.append(1, '\x05');
	tmp.append(g_tth + getTTH().toBase32()); // [!] IRainman opt.
	tmp.append(" (", 2);
	tmp.append(c.getIpPort());
	tmp.append(")|", 2);
	return tmp;
}
void SearchResultBaseTTH::toRES(AdcCommand& cmd, char p_type) const
{
	cmd.addParam("SI", Util::toString(getSize()));
	cmd.addParam("SL", Util::toString(getFreeSlots()));
	cmd.addParam("FN", Util::toAdcFile(getFile()));
	cmd.addParam("TR", getTTH().toBase32());
}

SearchResult::SearchResult(Types aType, int64_t aSize, const string& aFile, const TTHValue& aTTH, uint32_t aToken) :
	SearchResultCore(aType, aSize, aFile, aTTH),
	m_user(ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks()),
	m_is_tth_remembrance(false),
	m_is_tth_download(false),
	m_is_virus(false),
	m_is_tth_check(false),
	m_token(aToken),
	m_is_p2p_guard_calc(false),
	m_virus_level(0)
{
	initSlot();
	m_is_tth_share = aType == TYPE_FILE; // Constructor for ShareManager
}

SearchResult::SearchResult(const UserPtr& aUser, Types aType, uint8_t aSlots, uint8_t aFreeSlots,
                           int64_t aSize, const string& aFile, const string& aHubName,
                           const string& aHubURL, const boost::asio::ip::address_v4& aIP4, const TTHValue& aTTH, uint32_t aToken) :
	SearchResultCore(aType, aSize, aFile, aTTH, aSlots, aFreeSlots),
	m_hubName(aHubName),
	m_hubURL(aHubURL),
	m_user(aUser),
	m_search_ip4(aIP4),
	m_token(aToken),
	m_is_tth_share(false),
	m_is_tth_remembrance(false),
	m_is_tth_download(false),
	m_is_virus(false),
	m_is_tth_check(false),
	m_is_p2p_guard_calc(false),
	m_virus_level(0)
{
}

void SearchResult::calcP2PGuard()
{
	if (m_is_p2p_guard_calc == false)
	{
		if (m_search_ip4.to_ulong())
		{
			m_p2p_guard_text = CFlylinkDBManager::getInstance()->is_p2p_guard(m_search_ip4.to_ulong());
			m_is_p2p_guard_calc = true;
		}
	}
}

void SearchResult::calcHubName()
{
	if (m_hubName.empty())
	{
		const StringList names = ClientManager::getHubNames(getUser()->getCID(), Util::emptyString);
		m_hubName = names.empty() ? STRING(OFFLINE) : Util::toString(names);
	}
}
void SearchResult::checkTTH()
{
	if (m_is_tth_check == false)
	{
		if (m_type == TYPE_FILE)
		{
			m_is_tth_share = ShareManager::isTTHShared(getTTH());
			if (m_is_tth_share == false)
			{
				const auto l_status_file = CFlylinkDBManager::getInstance()->get_status_file(getTTH());
				if (l_status_file & CFlylinkDBManager::PREVIOUSLY_DOWNLOADED)
					m_is_tth_download = true;
				if (l_status_file & CFlylinkDBManager::VIRUS_FILE_KNOWN)
					m_is_virus = true;
				if (l_status_file & CFlylinkDBManager::PREVIOUSLY_BEEN_IN_SHARE)
					m_is_tth_remembrance = true;
			}
			else
				m_is_tth_remembrance = true;
		}
		m_is_tth_check = true;
	}
}

string SearchResult::getFilePath() const
{
	if (getType() == TYPE_FILE)
		return Util::getFilePath(getFile());
	else
		return Util::emptyString;
}

string SearchResult::getFileName() const
{
	if (getType() == TYPE_FILE)
		return Util::getFileName(getFile());
		
	if (getFile().size() < 2)
		return getFile();
		
	const string::size_type i = getFile().rfind('\\', getFile().length() - 2);
	if (i == string::npos)
		return getFile();
		
	return getFile().substr(i + 1);
}
