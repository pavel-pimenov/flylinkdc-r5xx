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

#ifndef _DHT_UTILS_H
#define _DHT_UTILS_H

#ifdef STRONG_USE_DHT

#include "../client/AdcCommand.h"
#include "../client/CID.h"
#include "../client/MerkleTree.h"

namespace dht
{

// all members must be static!
class Utils
{
	public:
		/** Returns distance between two nodes */
		static CID getDistance(const CID& cid1, const CID& cid2);
		
		/** Returns distance between node and file */
		static TTHValue getDistance(const CID& cid, const TTHValue& tth)
		{
			return TTHValue(const_cast<uint8_t*>(getDistance(cid, CID(tth.data)).data()));
		}
		
		/** Detect whether it is correct to use IP:port in DHT network */
		static bool isGoodIPPort(const string& ip, uint16_t port);
		
		/** General flooding protection */
		static bool checkFlood(const string& ip, const AdcCommand& cmd);
		
		/** Removes tracked packets. Called once a minute. */
		static void cleanFlood();
		
		/** Stores outgoing request to avoid receiving invalid responses */
		static void trackOutgoingPacket(const string& ip, const AdcCommand& cmd);
		
		/** Generates UDP key for specified IP address */
		static CID getUdpKey(const string& targetIp);
		
		static const string& compressXML(string& xml);
		
	private:
		Utils(void) { }
		~Utils(void) { }
		
		struct OutPacket
		{
			OutPacket(const string& _ip, uint64_t _time, uint32_t _cmd) : ip(_ip), time(_time), cmd(_cmd) {}
			const string      ip;
			const uint64_t    time;
			const uint32_t    cmd;
		};
		
		static FastCriticalSection cs; // [!] IRainman opt: use spin lock here.
		static std::unordered_map<string, std::unordered_multiset<uint32_t>> receivedPackets;
		static std::list<const OutPacket> sentPackets;
};

} // namespace dht

#endif // STRONG_USE_DHT

#endif  // _DHT_UTILS_H