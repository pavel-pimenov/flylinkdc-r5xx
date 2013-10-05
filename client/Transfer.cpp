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
#include "ClientManager.h"
#include "Upload.h"

const string Transfer::g_type_names[] =
{
	"file", "file", "list", "tthl"
};

const string Transfer::g_user_list_name = "files.xml";
const string Transfer::g_user_list_name_bz = "files.xml.bz2";

Transfer::Transfer(UserConnection& p_conn, const string& p_path, const TTHValue& p_tth) :
	m_type(TYPE_FILE), m_runningAverage(0), // [!] IRainman opt.
	m_path(p_path), m_tth(p_tth), m_actual(0), m_pos(0), m_userConnection(p_conn), m_startPos(0),
	m_isSecure(p_conn.isSecure()), m_isTrusted(p_conn.isTrusted()),
	m_start(0), m_lastTick(GET_TICK()) // [!] IRainman fix.
{
	// [!] IRainman refactoring transfer mechanism
	m_samples.push_back(Sample(m_lastTick, 0));
}
/*[-]IRainman refactoring transfer mechanism
 inline void Transfer::updateRunningAverage(uint64_t tick)
 {
    //[!]IRainman refactoring transfer mechanism
    runningAverage = getAverageSpeed();
    lastTick = tick;
 }
*/
void Transfer::tick(uint64_t p_CurrentTick)
{
	//[!]IRainman refactoring transfer mechanism
	FastLock l(m_cs);
	setLastTick(p_CurrentTick);
	
	//if (!samples.empty()) [!]IRainman: do not touch it
	{
		if (m_actual && m_samples.back().second == m_actual)
			m_samples.back().first = m_lastTick; // Position hasn't changed, just update the time
		else
			m_samples.push_back(Sample(m_lastTick, m_actual));
			
		const uint64_t ticks = m_lastTick - m_samples.front().first;
		const int64_t bytes = m_actual - m_samples.front().second;
		m_runningAverage = bytes * 1000I64 / (ticks ? ticks : 1I64);
		
		if (m_samples.size() > SPEED_APPROXIMATION_INTERVAL_S && ticks > SPEED_APPROXIMATION_INTERVAL_S * 1000)
			m_samples.pop_front();
	}
	
	// [!] IRainman fix: complite.
	// 2012-04-18_11-27-14_KURJ2LHEQPW6GCTWTYBSY7X3VJS3GEJHOK7RCGY_9A5426AF_crash-stack-r502-beta18-x64-build-9768.dmp
	// 2012-04-23_22-36-14_MQRSOVHEE6WA3YCANG7D36TKPR52TDPSSND7AUI_47AD139A_crash-stack-r501-x64-build-9812.dmp
	// 2012-04-23_22-36-14_J7AE3TLFTSHPJDYLFJGR5S3Z6QEAQSSTMIUVJ2A_EB7F1FF7_crash-stack-r501-x64-build-9812.dmp
	// 2012-04-27_18-43-09_ZVZTEP5ULRFDTRIK6S7DEZFKNC7NJOCOPRJUD2Y_90F7E6FC_crash-stack-r502-beta22-build-9854.dmp
	// 2012-05-03_22-00-59_2T3FXPPUYEB6HQLNPNPQINIDDHHU4EUUB2VNBGA_1F949D6A_crash-stack-r502-beta24-build-9900.dmp
	// 2012-06-07_20-46-18_RTU7QYLE2AQZRI2MHIPEGVCPI2VTOOBIWLAMWQA_08251EBA_crash-stack-r502-beta30-build-10270.dmp
	// 2012-06-17_22-40-29_3X4MWYTLOXY7HXS3KAILFKVJVC6MYX5EEMWQC6A_4B216112_crash-stack-r502-beta36-x64-build-10378.dmp
}

int64_t Transfer::getSecondsLeft(const bool wholeFile) const
{
	//[!]IRainman refactoring transfer mechanism
	const int64_t avg = getRunningAverage();
	const int64_t bytesLeft = (wholeFile ? ((Upload*)this)->getFileSize() : getSize()) - getPos();
	return bytesLeft / ((avg > 0) ? avg : 1);
}

void Transfer::getParams(const UserConnection& aSource, StringMap& params) const
{
	const auto& hint = aSource.getHintedUser().hint;
	const auto& user = aSource.getUser();
	
	params["userCID"] = user->getCID().toBase32();
	params["userNI"] = !user->getLastNick().empty() ? user->getLastNick() : Util::toString(ClientManager::getInstance()->getNicks(user->getCID(), Util::emptyString));
	params["userI4"] = aSource.getRemoteIp();
	
	StringList hubNames = ClientManager::getInstance()->getHubNames(user->getCID(), hint);
	if (hubNames.empty())
	{
		hubNames.push_back(STRING(OFFLINE));
	}
	params["hub"] = Util::toString(hubNames);
	
	StringList hubs = ClientManager::getInstance()->getHubs(user->getCID(), hint);
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
	params["time"] = getStart() == 0 ? "-" : Util::formatSeconds((getLastTick() - getStart()) / 1000); // [!] IRainman refactoring transfer mechanism
	params["fileTR"] = getTTH().toBase32();
}
