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

#include "stdinc.h"
#include "UserConnection.h"

const string Transfer::g_type_names[] =
{
	"file", "file", "list", "tthl"
};

const string Transfer::g_user_list_name = "files.xml";
const string Transfer::g_user_list_name_bz = "files.xml.bz2";

Transfer::Transfer(UserConnection* p_conn, const string& p_path, const TTHValue& p_tth, const string& p_ip, const string& p_chiper_name) :
	m_type(TYPE_FILE), m_runningAverage(0),
	m_path(p_path), m_tth(p_tth), m_actual(0), m_pos(0), m_userConnection(p_conn), m_hinted_user(p_conn->getHintedUser()), m_startPos(0),
	m_isSecure(p_conn->isSecure()), m_isTrusted(p_conn->isTrusted()),
	m_start(0), m_lastTick(GET_TICK()),
	m_chiper_name(p_chiper_name),
	m_ip(p_ip), // TODO - перевести на boost? падаем
	m_fileSize(-1)
	// https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=62265
{
	m_samples.push_back(Sample(m_lastTick, 0));
}
string Transfer::getConnectionQueueToken() const
{
	if (getUserConnection())
	{
		return getUserConnection()->getConnectionQueueToken();
	}
	else
	{
		return BaseUtil::emptyString;
	}
}

void Transfer::tick(uint64_t p_CurrentTick)
{
	if (!ClientManager::isBeforeShutdown())
	{
	
		CFlyFastLock(m_cs);
		setLastTick(p_CurrentTick);
		dcassert(!m_samples.empty());
		if (!m_samples.empty()) // https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=57070
		{
			if (m_actual && m_samples.back().second == m_actual)
				m_samples.back().first = m_lastTick; // Position hasn't changed, just update the time
			else
				m_samples.emplace_back(Sample(m_lastTick, m_actual));
				
			const uint64_t ticks = m_lastTick - m_samples.front().first;
			const int64_t bytes = m_actual - m_samples.front().second;
			m_runningAverage = bytes * 1000I64 / (ticks ? ticks : 1I64);
			
			if (ticks > SPEED_APPROXIMATION_INTERVAL_S * 1000 && m_samples.size() > SPEED_APPROXIMATION_INTERVAL_S)
			{
				m_samples.pop_front();
			}
		}
	}
}

int64_t Transfer::getSecondsLeft(const bool wholeFile) const
{
	const int64_t avg = getRunningAverage();
	const int64_t bytesLeft = (wholeFile ? getFileSize() : getSize()) - getPos();
	if (bytesLeft > 0)
		return bytesLeft / ((avg > 0) ? avg : 1);
	else
	{
		//dcassert(0);
		return 0;
	}
}

void Transfer::getParams(const UserConnection* aSource, StringMap& params) const
{
	const auto hint = aSource->getHintedUser().hint;
	const auto user = aSource->getUser();
	dcassert(user);
	if (user)
	{
		params["userCID"] = user->getCID().toBase32();
		params["userNI"] = !user->getLastNick().empty() ? user->getLastNick() : Util::toString(ClientManager::getNicks(user->getCID(), BaseUtil::emptyString, false));
		params["userI4"] = aSource->getRemoteIp();
		
		StringList hubNames = ClientManager::getHubNames(user->getCID(), hint);
		if (hubNames.empty())
		{
			hubNames.push_back(STRING(OFFLINE));
		}
		params["hub"] = Util::toString(hubNames);
		
		StringList hubs = ClientManager::getHubs(user->getCID(), hint);
		if (hubs.empty())
		{
			hubs.push_back(STRING(OFFLINE));
		}
		params["hubURL"] = Util::toString(hubs);
		
		params["fileSI"] = Util::toString(getSize());
		params["fileSIshort"] = Util::formatBytes(getSize());
		params["fileSIchunk"] = Util::toString(getPos());
		params["fileSIchunkshort"] = Util::formatBytes(getPos());
		params["fileSIactual"] = Util::toString(getActual());
		params["fileSIactualshort"] = Util::formatBytes(getActual());
		params["speed"] = Util::formatBytes(getRunningAverage()) + '/' + STRING(S);
		params["time"] = getStart() == 0 ? "-" : Util::formatSeconds((getLastTick() - getStart()) / 1000);
		params["fileTR"] = getTTH().toBase32();
	}
}
void Transfer::setStart(uint64_t tick)
{
	m_start = tick;
	CFlyFastLock(m_cs);
	setLastTick(tick);
	m_samples.emplace_back(Sample(m_start, 0));
}
const uint64_t Transfer::getLastActivity()
{
	return getUserConnection()->getLastActivity(false);
}
//string Transfer::getUserConnectionToken() const
//{
//	return getUserConnection()->getUserConnectionToken();
//}
//string Transfer::getConnectionToken() const
//{
//	return getUserConnection()->getConnectionToken();
//}
