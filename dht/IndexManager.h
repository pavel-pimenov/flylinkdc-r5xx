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

#ifndef _INDEXMANAGER_H
#define _INDEXMANAGER_H

#ifdef STRONG_USE_DHT

#include "Constants.h"
#include "KBucket.h"

#include "../client/ShareManager.h"
#include "../client/Singleton.h"

#include <boost/atomic.hpp>

namespace dht
{


class IndexManager :
	public Singleton<IndexManager>
{
	public:
		IndexManager();
		~IndexManager();
		
		
		/** Finds TTH in known indexes and returns it */
		bool findResult(const TTHValue& tth, SourceList& sources) const;
		
		/** Try to publish next file in queue */
		void publishNextFile();
		
		/** Loads existing indexes from disk */
		void loadIndexes(SimpleXML& xml);
		
		/** How many files is currently being published */
		static void incPublishing()
		{
			Thread::safeInc(g_publishing); // [!] IRainman opt.
		}
		static void decPublishing()
		{
			Thread::safeDec(g_publishing); // [!] IRainman opt.
		}
		
		/** Is publishing allowed? */
		static void setPublish(bool p_publish)
		{
			g_is_publish = p_publish;
		}
		static bool isPublish()
		{
			return g_is_publish;
		}
		
		/** Processes incoming request to publish file */
		void processPublishSourceRequest(const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& cmd);
		
		/** Removes old sources */
		void checkExpiration(uint64_t aTick);
		
		/** Publishes shared file */
		static void publishFile(const TTHValue& tth, int64_t size);
		
		/** Publishes partially downloaded file */
		static void publishPartialFile(const TTHValue& tth);
		
		/** Set time when our sharelist should be republished */
		static void setNextPublishing()
		{
			g_nextRepublishTime = GET_TICK() + REPUBLISH_TIME;
		}
		
		/** Is time when we should republish our sharelist? */
		static bool isTimeForPublishing()
		{
			return g_isRepublishTime;
		}
		static void setTimeForPublishing()
		{
			g_isRepublishTime = GET_TICK() >= g_nextRepublishTime;
		}
		
	private:
	
		/** Contains known hashes in the network and their sources */
		
		/** Queue of files prepared for publishing */
		static std::deque<File> g_publishQueue;
		
		/** Is publishing allowed? */
		static bool g_is_publish;
		
		/** How many files is currently being published */
		static volatile long g_publishing;
		
		/** Time when our sharelist should be republished */
		static uint64_t g_nextRepublishTime;
		
		static bool g_isRepublishTime;
		
		/** Synchronizes access to tthList */
		static FastCriticalSection g_cs; // [!] IRainman opt: use spin lock here.
		
		/** Add new source to tth list */
		void addSource(const TTHValue& tth, const CID& cid, const string& ip, uint16_t port, uint64_t size, bool partial);
		
		int m_hour_count;
};

} // namespace dht

#endif // STRONG_USE_DHT

#endif  // _INDEXMANAGER_H