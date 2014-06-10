/*
 * Copyright (C) 2009-2011 Big Muscle, http://strongdc.sf.net
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

#ifndef _KBUCKET_H
#define _KBUCKET_H

#ifdef STRONG_USE_DHT

#include "Constants.h"
#include "DHTType.h"

#include "../client/Client.h"
#include "../client/SimpleXML.h"
#include "../client/User.h"

namespace dht
{

struct Node :
	public OnlineUser
{
		typedef boost::intrusive_ptr<Node> Ptr;
		typedef std::map<CID, Node::Ptr> Map; // map менять нельзя - контейнер должен быть сортирован
		
		Node(const UserPtr& u);
		~Node() noexcept { }
		
		uint8_t getType() const
		{
			return type;
		}
		bool isIpVerified() const
		{
			return ipVerified;
		}
		
		bool isOnline() const
		{
			return online;
		}
		void setOnline(bool _online)
		{
			online = _online;
		}
		
		void setAlive();
		void setIpVerified(bool verified)
		{
			ipVerified = verified;
		}
		void setTimeout(uint64_t now = GET_TICK());
		
		void setUdpKey(const UDPKey& _key)
		{
			key = _key;
		}
		UDPKey getUdpKey() const;
		
	private:
	
		friend class RoutingTable;
		
		UDPKey      key;
		
		uint64_t    created;
		uint64_t    expires;
		uint8_t     type;
		bool        ipVerified;
		bool        online; // getUser()->isOnline() returns true when node is online in any hub, we need info when he is online in DHT
};

class RoutingTable
{
	public:
		RoutingTable(int _level = 0, bool splitAllowed = true);
		~RoutingTable(void);
		
		typedef std::deque<Node::Ptr> NodeList;
		
		/** Creates new (or update existing) node which is NOT added to our routing table */
		Node::Ptr addOrUpdate(const UserPtr& u, const string& ip, uint16_t port, const UDPKey& udpKey, bool update, bool isUdpKeyValid);
		
		/** Returns nodes count in whole routing table */
		static size_t getNodesCount()
		{
			return g_nodesCount;
		}
		static void resetNodesCount()
		{
			g_nodesCount = 0;
		}
		
		/** Finds "max" closest nodes and stores them to the list */
		void getClosestNodes(const CID& cid, Node::Map& closest, unsigned int max, uint8_t maxType) const;
		
		/** Loads existing nodes from disk */
		void loadNodes(SimpleXML& xml);
		
		/** Save bootstrap nodes to disk */
		void saveNodes();
		
		void checkExpiration(uint64_t aTick);
		
	private:
	
		/** Left and right branches of the tree */
		RoutingTable* m_zones[2];
		
		/** The level of this zone */
		int  m_level;
		bool m_splitAllowed;
		
		/** List of nodes in this bucket */
		NodeList* m_bucket;
		
		/** List of known IPs in this bucket */
		static StringSet g_ipMap;
		
		static size_t g_nodesCount;
		
		void split();
		
		uint64_t m_lastRandomLookup;
};

}

#endif // STRONG_USE_DHT

#endif  // _KBUCKET_H