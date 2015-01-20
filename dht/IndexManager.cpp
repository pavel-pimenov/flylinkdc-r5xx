/*
 * Copyright (C) 2008-2011 Big Muscle, http://strongdc.sf.net
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

#ifdef STRONG_USE_DHT

#include "DHT.h"
#include "IndexManager.h"
#include "SearchManager.h"

#include "../client/LogManager.h"
#include "../client/ShareManager.h"
#include "../client/TimerManager.h"
#include "../client/CFlylinkDBManager.h"

namespace dht
{

IndexManager::IndexManager() :
	nextRepublishTime(GET_TICK()), publishing(0), publish(false), m_hour_count(0)
{
}

IndexManager::~IndexManager()
{
}

/*
 * Add new source to tth list
 */
void IndexManager::addSource(const TTHValue& p_tth, const CID& p_cid, const string& p_ip, uint16_t p_port, uint64_t p_size, bool p_partial)
{
	SourceTTH l_source;
	l_source.setTTH(p_tth);
	l_source.setCID(p_cid);
	l_source.setIp(p_ip);
	l_source.setUdpPort(p_port);
	l_source.setSize(p_size);
//	l_source.setExpires(GET_TICK_CURRENT() + (p_partial ? PFS_REPUBLISH_TIME : REPUBLISH_TIME));
	l_source.setPartial(p_partial);
	dht::TTHArray l_DHTFile;
	l_DHTFile.reserve(1);
	l_DHTFile.push_back(l_source);
	CFlylinkDBManager::getInstance()->save_dht_files(l_DHTFile);
/*
	Lock l(cs);
	
	TTHMap::iterator i = tthList.find(p_tth);
	if (i != tthList.end())
	{
		// no user duplicites
		SourceList& sources = i->second;
		for (auto s = sources.cbegin(); s != sources.cend(); ++s)
		{
			if (p_cid == *s->getCID())
			{
				// delete old item
				sources.erase(s);
				break;
			}
		}
		
		// old items in front, new items in back
		sources.push_back(source);
		
		// if maximum sources reached, remove the oldest one
		if (sources.size() > MAX_SEARCH_RESULTS)
			sources.pop_front();
	}
	else
	{
		// new file
		tthList.insert(std::make_pair(p_tth, SourceList(1, source)));
	}
*/	
}

/*
 * Finds TTH in known indexes and returns it
 */
bool IndexManager::findResult(const TTHValue& p_tth, SourceList& p_sources) const
{
#ifdef _DEBUG
	static FastCriticalSection cs_tth_dup;
	static boost::unordered_map<string,int> g_tth_dup;

	FastLock l(cs_tth_dup);
	if(g_tth_dup[p_tth.toBase32()]++ > 1)
	{
		LogManager::message("[dht] IndexManager::findResult dup = " + p_tth.toBase32() + 
			" count = " + Util::toString(g_tth_dup[p_tth.toBase32()]) + " size_map = " + Util::toString(g_tth_dup.size()));
	}
#endif
	return CFlylinkDBManager::getInstance()->find_dht_files(p_tth,p_sources) > 0;
/*
// TODO: does file exist in my own sharelist?
	FastLock l(cs);
	TTHMap::const_iterator i = tthList.find(tth);
	if (i != tthList.end())
	{
		sources = i->second;
		return true;
	}
	
	return false;
*/
}

/*
 * Try to publish next file in queue
 */
void IndexManager::publishNextFile()
{
	File f;
	{
		FastLock l(cs);
		
		if (publishQueue.empty() || publishing >= MAX_PUBLISHES_AT_TIME)
			return;
			
		incPublishing();
		
		f = publishQueue.front(); // get the first file in queue
		publishQueue.pop_front(); // and remove it from queue
	}
	SearchManager::getInstance()->findStore(f.tth.toBase32(), f.size, f.partial);
}

/*
 * Loads existing indexes from disk
 */
void IndexManager::loadIndexes(SimpleXML& xml)
{
	TTHArray l_tthList;
	xml.resetCurrentChild();
	if (xml.findChild("Files"))
	{
		xml.stepIn();
		while (xml.findChild("File"))
		{
			const TTHValue l_tth = TTHValue(xml.getChildAttrib("TTH"));
			xml.stepIn();
			while (xml.findChild("Source"))
			{
				SourceTTH l_source_item;
				l_source_item.setTTH(l_tth);
				l_source_item.setCID(CID(xml.getChildAttrib("CID")));
				l_source_item.setIp(xml.getChildAttrib("I4"));
				l_source_item.setUdpPort(static_cast<uint16_t>(xml.getIntChildAttrib("U4")));
				l_source_item.setSize(xml.getLongLongChildAttrib("SI"));
//				l_source_item.setExpires(xml.getLongLongChildAttrib("EX"));
				l_source_item.setPartial(false);			
				l_tthList.push_back(l_source_item);
			}			
			xml.stepOut();
		}
		xml.stepOut();
	 }
	if (l_tthList.size())
 	 CFlylinkDBManager::getInstance()->save_dht_files(l_tthList);
}

/*
 * Processes incoming request to publish file
 */
void IndexManager::processPublishSourceRequest(const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& cmd)
{
	const CID cid = CID(cmd.getParam(0));
	
	string tth;
	if (!cmd.getParam("TR", 1, tth))
		return; // nothing to identify a file?
		
	string size;
	if (!cmd.getParam("SI", 1, size))
		return; // no file size?
		
	string partial;
	cmd.getParam("PF", 1, partial);
	
	addSource(TTHValue(tth), cid, ip, port, Util::toInt64(size), partial == "1");
	
	// send response
	AdcCommand res(AdcCommand::SEV_SUCCESS, AdcCommand::SUCCESS, "File published", AdcCommand::TYPE_UDP);
	res.addParam("FC", "PUB");
	res.addParam("TR", tth);
	DHT::getInstance()->send(res, ip, port, cid, udpKey);
}

/*
 * Removes old sources
 */
void IndexManager::checkExpiration(uint64_t p_Tick)
{
	if (++m_hour_count > 60)
	{
		CFlylinkDBManager::getInstance()->check_expiration_dht_files(p_Tick);
		m_hour_count = 0;
	}
/*	
	bool dirty = false;
	FastLock l(cs);	
	auto i = tthList.begin();
	while (i != tthList.end())
	{
		auto j = i->second.begin();
		while (j != i->second.end())
		{
			const Source& source = *j; // https://www.box.net/shared/109810fe782a6f5300e7
			if (source.getExpires() <= aTick)
			{
				dirty = true;
				j = i->second.erase(j);
			}
			else
				break;  // list is sorted, so finding first non-expired can stop iteration
				
		}
		
		if (i->second.empty())
			tthList.erase(i++);
		else
			++i;
	}
	
	if (dirty)
		DHT::getInstance()->setDirty();
*/
}

/** Publishes shared file */
void IndexManager::publishFile(const TTHValue& tth, int64_t size)
{
	if (size > MIN_PUBLISH_FILESIZE)
	{
		FastLock l(cs);
		publishQueue.push_back(File(tth, size, false)); // TODO 2012-04-23_22-28-18_5X2A4DH3DN4RBHJBW7A6UCJNXV3HW6UQSGTDS4I_40B6FDE8_crash-stack-r501-build-9812.dmp
		// 2012-04-23_22-28-18_A24UAZGII2D4JQ5B2GQO7BSOJZGHSOZLL6QR4RA_DE100545_crash-stack-r501-build-9812.dmp
		// 2012-04-29_13-38-26_GGKPIT76OJFJ4NI7NQ6B7QRB66WKJMIDUIIVFEI_AF901814_crash-stack-r501-build-9869.dmp
		// 2012-05-03_22-00-59_AZWODHUBAHNNZWNBFTNWRY7AGXVT46UH22UHYWI_DC008D1E_crash-stack-r502-beta24-build-9900.dmp
		// 2012-05-03_22-05-14_U77FRCHNRJDNVF325OSOPZU3K6DGTORPWW4TIOY_BA6785FB_crash-stack-r502-beta24-x64-build-9900.dmp
		// 2012-05-03_22-05-14_YZKXKY3PNHETPRYT6NABRMBYPRA5AOURK7EE3IA_A1F7CCFE_crash-stack-r502-beta24-x64-build-9900.dmp
		// 2012-04-29_13-38-26_PR6EDIVLYDASGBBREFIKGXB5WW36ZY6MSLK2CQQ_0873ADD2_crash-stack-r501-build-9869.dmp
		// 2012-04-29_13-38-26_V3HKSEV52WSC3PHSO5DOTEIPESQ5JZWRZRH3PIQ_ED6821C8_crash-stack-r501-build-9869.dmp
		// 2012-05-11_23-57-17_U77FRCHNRJDNVF325OSOPZU3K6DGTORPWW4TIOY_D7B8E691_crash-stack-r502-beta26-x64-build-9946.dmp
		// 2012-06-09_18-15-11_MRLAHWANAPWOZ3AKPZY5DHT56OP6SWGLVRHAIRY_30E3C1A5_crash-stack-r501-build-10294.dmp
	}
}

/*
 * Publishes partially downloaded file
 */
void IndexManager::publishPartialFile(const TTHValue& tth)
{
	FastLock l(cs);
	publishQueue.push_front(File(tth, 0, true));
}


} // namespace dht

#endif // STRONG_USE_DHT