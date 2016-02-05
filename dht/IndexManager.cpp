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
uint64_t IndexManager::g_nextRepublishTime = 0;
std::deque<File> IndexManager::g_publishQueue;
FastCriticalSection IndexManager::g_cs;
bool IndexManager::g_isRepublishTime = false;
bool IndexManager::g_is_publish = false;
volatile long IndexManager::g_publishing = 0;

IndexManager::IndexManager() :
	m_hour_count(0)
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
	dcassert(BOOLSETTING(USE_DHT));
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
	    CFlyLock(cs);
	
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
	dcassert(BOOLSETTING(USE_DHT));
#ifdef _DEBUG
	static FastCriticalSection cs_tth_dup;
	static boost::unordered_map<string, int> g_tth_dup;
	
	CFlyFastLock(cs_tth_dup);
	if (g_tth_dup[p_tth.toBase32()]++ > 1)
	{
		LogManager::message("[dht] IndexManager::findResult dup = " + p_tth.toBase32() +
		                    " count = " + Util::toString(g_tth_dup[p_tth.toBase32()]) + " size_map = " + Util::toString(g_tth_dup.size()));
	}
#endif
	return CFlylinkDBManager::getInstance()->find_dht_files(p_tth, p_sources) > 0;
	/*
	// TODO: does file exist in my own sharelist?
	    CFlyFastLock(cs);
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
	dcassert(BOOLSETTING(USE_DHT));
	dcassert(!ClientManager::isShutdown());
	File f;
	{
		CFlyFastLock(g_cs);
		
		if (g_publishQueue.empty() || g_publishing >= MAX_PUBLISHES_AT_TIME)
			return;
			
		incPublishing();
		
		f = g_publishQueue.front(); // get the first file in queue
		g_publishQueue.pop_front(); // and remove it from queue
	}
	SearchManager::getInstance()->findStore(f.tth.toBase32(), f.size, f.partial);
}

/*
 * Loads existing indexes from disk
 */
void IndexManager::loadIndexes(SimpleXML& xml)
{
	dcassert(BOOLSETTING(USE_DHT));
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
	if (!l_tthList.empty())
	{
		CFlylinkDBManager::getInstance()->save_dht_files(l_tthList);
	}
}

/*
 * Processes incoming request to publish file
 */
void IndexManager::processPublishSourceRequest(const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& cmd)
{
	dcassert(BOOLSETTING(USE_DHT));
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
	dcassert(BOOLSETTING(USE_DHT));
	if (++m_hour_count > 60)
	{
		CFlylinkDBManager::getInstance()->check_expiration_dht_files(p_Tick);
		m_hour_count = 0;
	}
	/*
	    bool dirty = false;
	    CFlyFastLock(cs);
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
	dcassert(BOOLSETTING(USE_DHT));
	dcassert(!ClientManager::isShutdown());
	if (size > MIN_PUBLISH_FILESIZE)
	{
		CFlyFastLock(g_cs);
		g_publishQueue.push_back(File(tth, size, false));
	}
}

/*
 * Publishes partially downloaded file
 */
void IndexManager::publishPartialFile(const TTHValue& tth)
{
	dcassert(BOOLSETTING(USE_DHT));
	dcassert(!ClientManager::isShutdown());
	CFlyFastLock(g_cs);
	g_publishQueue.push_front(File(tth, 0, true));
}


} // namespace dht

#endif // STRONG_USE_DHT