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

#include "stdafx.h"

#ifdef STRONG_USE_DHT

#include "Constants.h"
#include "DHT.h"
#include "TaskManager.h"
#include "Utils.h"

#include "../client/ClientManager.h"
#include "../client/CFlylinkDBManager.h"
#include "../client/LogManager.h"

namespace dht
{
StringSet RoutingTable::g_ipMap;
size_t RoutingTable::g_nodesCount = 0;

// Set all new nodes' type to 3 to avoid spreading dead nodes..
Node::Node(const UserPtr& u) :
	OnlineUser(u, *DHT::getInstance(), 0), created(GET_TICK()), type(3), expires(0), ipVerified(false), online(false)
{
}

UDPKey Node::getUdpKey() const
{
	// if our external IP changed from the last time, we can't encrypt packet with this key
	if (DHT::getInstance()->getLastExternalIP() == key.m_ip)
		return key;
	else
		return UDPKey();
}

void Node::setAlive()
{
	const uint64_t l_CurrentTick = GET_TICK(); // [+] IRainman optimize
	// long existing nodes will probably be there for another long time
	const uint64_t hours = (l_CurrentTick - created) / 1000 / 60 / 60;
	switch (hours)
	{
		case 0:
			type = 2;
			expires = l_CurrentTick + (NODE_EXPIRATION / 2);
			break;
		case 1:
			type = 1;
			expires = l_CurrentTick + (uint64_t)(NODE_EXPIRATION / 1.5);
			break;
		default:    // long existing nodes
			type = 0;
			expires = l_CurrentTick + NODE_EXPIRATION;
	}
}

void Node::setTimeout(uint64_t now)
{
	if (type == 4)
		return;
		
	type++;
	expires = now + NODE_RESPONSE_TIMEOUT;
}


RoutingTable::RoutingTable(int p_level, bool p_splitAllowed) :
	m_level(p_level), m_lastRandomLookup(GET_TICK()), m_splitAllowed(p_splitAllowed)
{
	m_bucket = new NodeList;
	
	m_zones[0] = nullptr;
	m_zones[1] = nullptr;
	
	TaskManager::getInstance()->addZone(this);
}

RoutingTable::~RoutingTable(void)
{
	delete m_zones[0];
	delete m_zones[1];
	
	if (m_bucket)
	{
		TaskManager::getInstance()->removeZone(this);
		
#ifdef FLYLINKDC_HE // [+] IRainman fix. https://code.google.com/p/flylinkdc/source/detail?r=15176
		if (ClientManager::isShutdown())
		{
			for (auto i = m_bucket->cbegin(); i != m_bucket->cend(); ++i)
			{
				if ((*i)->isOnline())
				{
					(*i)->dec();
				}
			}
		}
		else
#endif
		{
			for (auto i = m_bucket->cbegin(); i != m_bucket->cend(); ++i)
			{
				if ((*i)->isOnline())
				{
					ClientManager::getInstance()->putOffline(i->get());
					(*i)->setOnline(false);
					(*i)->dec();
				}
			}
		}
		delete m_bucket;
	}
}

inline bool getBit(const uint8_t* value, int number)
{
	uint8_t byte = value[number / 8];
	return (byte >> (7 - (number % 8))) & 1;
}

/*
 * Creates new (or update existing) node which is NOT added to our routing table
 */
Node::Ptr RoutingTable::addOrUpdate(const UserPtr& u, const string& ip, uint16_t port, const UDPKey& udpKey, bool update, bool isUdpKeyValid)
{
	if (m_bucket == nullptr)
	{
		// get distance from me
		const uint8_t* distance = Utils::getDistance(u->getCID(), ClientManager::getMyCID()).data(); // [!] IRainman fix.
		
		// iterate through tree structure
		return m_zones[getBit(distance, m_level)]->addOrUpdate(u, ip, port, udpKey, update, isUdpKeyValid);
	}
	else
	{
		Node::Ptr node = nullptr;
		for (auto it = m_bucket->cbegin(); it != m_bucket->cend(); ++it)
		{
			if ((*it)->getUser() == u)
			{
				node = *it;
				
				// put node at the end of the list
				m_bucket->erase(it);
				m_bucket->push_back(node);
				break;
			}
		}
#if 0		
		if (node == nullptr && u->isOnline())
		{
			// try to get node from ClientManager (user can be online but not in our routing table)
			// [-]PPA Отключил опасное преобразование типа OnlineUser нельзя расширить до наследника Node*
            // node = (Node*)ClientManager::findDHTNode(u->getCID()).get();
			// LogManager::getInstance()->message("RoutingTable::addOrUpdate error node == nullptr && u->isOnline() - send email ppa74@ya.ru");
			// dcassert(0);
		}
#endif		
		if (node != nullptr)
		{
			// fine, node found, update it and return it
			if (update)
			{
				if (!node->getUdpKey().isZero() && !(node->getUdpKey().m_key == udpKey.m_key))
				{
					// if we haven't changed our IP in the last time, we require that node's UDP key is same as key sent
					// with this packet to avoid spoofing
					return node;
				}
				
				auto &id = node->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'node->getIdentity()' expression repeatedly. kbucket.cpp 168
				const auto& oldIp = id.getIpAsString();
				const auto oldPort = id.getUdpPort();
				if (ip != oldIp || oldPort != port)
				{
					// don't allow update when new IP already exists for different node
					const string newIPPort = ip + ":" + Util::toString(port);
					auto i = g_ipMap.find(newIPPort);
					if (i != g_ipMap.end())
					{
						return node;
					}
					else
					{
						// erase old IP
						g_ipMap.erase(oldIp + ":" + Util::toString(oldPort));
						g_ipMap.insert(newIPPort);
					}
					// since IP changed, this flag becomes invalid
					node->setIpVerified(false);
				}
				
				if (!node->isIpVerified())
					node->setIpVerified(isUdpKeyValid);
					
				node->setAlive();
				id.setIp(ip);
				id.setUdpPort(port);
				node->setUdpKey(udpKey);
				
			}
		}
		else if (m_bucket->size() < DHT_K)
		{
			// bucket is not full, add node
			node = new Node(u);
			node->getIdentity().setIp(ip);
			node->getIdentity().setUdpPort(port);
			node->setIpVerified(isUdpKeyValid);
			node->setUdpKey(udpKey);
			
			m_bucket->push_back(node);
			
			++g_nodesCount;
		}
		else if (m_level < ID_BITS - 1 && m_splitAllowed)
		{
			// bucket is full, split it
			split();
			
			// iterate through tree structure
			const uint8_t* distance = Utils::getDistance(u->getCID(), ClientManager::getMyCID()).data(); // [!] IRainman fix.
			node = m_zones[getBit(distance, m_level)]->addOrUpdate(u, ip, port, udpKey, update, isUdpKeyValid);
		}
		else
		{
			node = new Node(u);
			node->getIdentity().setIp(ip);
			node->getIdentity().setUdpPort(port);
			node->setIpVerified(isUdpKeyValid);
		}
		
		u->setFlag(User::DHT0);
		return node;
	}
}

void RoutingTable::split()
{
	TaskManager::getInstance()->removeZone(this);
	
	// if we are in zero branch (very close nodes), we allow split always
	// branch with 1's is allowed to split only to level 4 and a few last buckets before routing table becomes full
	m_zones[0] = new RoutingTable(m_level + 1, m_splitAllowed);   // FIXME: only 000000000000 branches should be split and not 1110000000
	m_zones[1] = new RoutingTable(m_level + 1, (m_level < 4) || (m_level > ID_BITS - 5));
	
	for (auto it = m_bucket->begin(); it != m_bucket->end(); ++it)
	{
		// iterate through tree structure
		const uint8_t* l_distance = Utils::getDistance((*it)->getUser()->getCID(), ClientManager::getMyCID()).data(); // [!] IRainman fix.
		m_zones[getBit(l_distance, m_level)]->m_bucket->push_back(*it);
	}
	
	safe_delete(m_bucket);
}

/*
 * Finds "max" closest nodes and stores them to the list
 */
void RoutingTable::getClosestNodes(const CID& cid, Node::Map& closest, unsigned int max, uint8_t maxType) const
{
	if (m_bucket != nullptr)
	{
		for (auto it = m_bucket->begin(); it != m_bucket->end(); ++it)
		{
			Node::Ptr node = *it;
			if (node->getType() <= maxType && node->isIpVerified() &&
				node->getUser()->isSet(User::TCP4) // [!] IRainamn fix.
				)
			{
				CID distance = Utils::getDistance(cid, node->getUser()->getCID());
				
				if (closest.size() < max)
				{
					// just insert
					dcassert(node != NULL);
					closest.insert(std::make_pair(distance, node));
				}
				else
				{
					// not enough room, so insert only closer nodes
					if (distance < closest.rbegin()->first) // "closest" is sorted map, so just compare with last node
					{
						dcassert(node != nullptr);
						
						closest.erase(closest.rbegin()->first);
						closest.insert(std::make_pair(distance, node));
					}
				}
			}
		}
	}
	else
	{
		const uint8_t* distance = Utils::getDistance(cid, ClientManager::getMyCID()).data(); // [!] IRainman fix.
		const bool bit = getBit(distance, m_level);
		m_zones[bit]->getClosestNodes(cid, closest, max, maxType);
		
		if (closest.size() < max)   // if not enough nodes found, try other buckets
			m_zones[!bit]->getClosestNodes(cid, closest, max, maxType);
	}
}

/*
 * Loads existing nodes from disk
 */
void RoutingTable::loadNodes(SimpleXML& xml)
{
	std::vector<dht::BootstrapNode> l_dht_nodes; 
	if(CFlylinkDBManager::getInstance()->load_dht_nodes(l_dht_nodes))
	{
		for (auto k = l_dht_nodes.cbegin(); k != l_dht_nodes.cend(); ++k)
		{
			const BootstrapNode& l_node = *k;
			if (Utils::isGoodIPPort(l_node.m_ip,l_node.m_udpPort))
			{
				if (!l_node.m_udpKey.m_ip.empty() && !l_node.m_udpKey.m_key.isZero() )
				{
					if (DHT::getInstance()->getLastExternalIP() == "0.0.0.0")
					 DHT::getInstance()->setLastExternalIP(l_node.m_udpKey.m_ip);
				}
				BootstrapManager::getInstance()->addBootstrapNode(l_node.m_ip, l_node.m_udpPort, l_node.m_cid, l_node.m_udpKey);
			}
		}
	}
	else
	{
		xml.resetCurrentChild();
		if (xml.findChild("Nodes"))
		{
			xml.stepIn();
			while (xml.findChild("Node"))
			{
				const CID cid(xml.getChildAttrib("CID"));
				const string& i4   = xml.getChildAttrib("I4");
				const uint16_t u4 = static_cast<uint16_t>(xml.getIntChildAttrib("U4"));
				dcassert(u4);
			
				if (Utils::isGoodIPPort(i4, u4))
				{
					UDPKey udpKey;
					const string& key      = xml.getChildAttrib("key");
					const string& keyIp    = xml.getChildAttrib("keyIP");
				
					if (!key.empty() && !keyIp.empty())
					{
						udpKey.m_key = CID(key);
						udpKey.m_ip = keyIp;
					
						// since we don't know our IP yet, we can use stored one
						if (DHT::getInstance()->getLastExternalIP() == "0.0.0.0")
							DHT::getInstance()->setLastExternalIP(keyIp);
					}
				
					//UserPtr u = ClientManager::getUser(cid);
					//addOrUpdate(u, i4, u4, udpKey, false, true);
				
					BootstrapManager::getInstance()->addBootstrapNode(i4, u4, cid, udpKey);
				}
			}
			xml.stepOut();
		}
	}
}

/*
 * Save bootstrap nodes to disk
 */
void RoutingTable::saveNodes()
{
	// get 500 random nodes to bootstrap from them next time
	Node::Map closestToRandom;
	getClosestNodes(CID::generate(), closestToRandom, 500, 3); // [~] IRainman 200 -> 500
	// [!] IRainman opt: dqueue -> vector.
	std::vector<dht::BootstrapNode> l_dht_nodes;
	l_dht_nodes.reserve(closestToRandom.size());
	// [~] IRainman opt.
	for (auto k = closestToRandom.cbegin(); k != closestToRandom.cend(); ++k)
	{
		const Node::Ptr& node = k->second;
		BootstrapNode l_node;
		l_node.m_cid = node->getUser()->getCID();
		l_node.m_ip = node->getIdentity().getIpAsString();
		l_node.m_udpPort = node->getIdentity().getUdpPort();
		const UDPKey l_key = node->getUdpKey();
		if (!l_key.isZero())
		{
			l_node.m_udpKey = l_key;
		}
		l_dht_nodes.push_back(l_node);
	}
	if(!l_dht_nodes.empty())
	  CFlylinkDBManager::getInstance()->save_dht_nodes(l_dht_nodes);
}

void RoutingTable::checkExpiration(uint64_t aTick)
{
	if (m_bucket == nullptr)
		return;
		
	// first, remove dead nodes
	auto i = m_bucket->begin();
	while (i != m_bucket->end())
	{
		Node::Ptr& node = *i;
		
		if (node->getType() == 4 && node->expires > 0 && node->expires <= aTick)
		{
			if (node->isOnline())
			{
				node->setOnline(false);
				
				// FIXME: what if we're still downloading/uploading from/to this node?
				// then it is weird why it does not responded to our INF
				ClientManager::getInstance()->putOffline(node.get());
				node->dec();
			}
			
			if (node->unique()) // is the only reference in this bucket?
			{
				// node is dead, remove it
				const auto& ip   = node->getIdentity().getIpAsString();
				const auto port = node->getIdentity().getUdpPort();
				g_ipMap.erase(ip + ':' + Util::toString(port));
				
				i = m_bucket->erase(i);
				
				--g_nodesCount;
			}
			else
			{
				++i;
			}
			
			continue;
		}
		
		if (node->expires == 0)
			node->expires = aTick;
		++i;
	}
	
	// lookup for new random nodes
	if (aTick - m_lastRandomLookup >= RANDOM_LOOKUP)
	{
		if (m_bucket->size() <= 2)
		{
		
		}
		m_lastRandomLookup = aTick;
	}
	
	if (m_bucket->empty())
		return;
		
	// select the oldest expired node
	Node::Ptr node = m_bucket->front();
	if (node->getType() < 4 && node->expires <= aTick)
	{
		// ping the oldest (expired) node
		node->setTimeout(aTick);
		DHT::getInstance()->info(node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(), DHT::PING, node->getUser()->getCID(), node->getUdpKey());
	}
	else
	{
		m_bucket->pop_front();
		m_bucket->push_back(node);
	}
	/*
	#ifdef _DEBUG
	        int verified = 0; int types[5] = { 0 };
	        for(auto j = bucket->begin(); j != bucket->end(); ++j)
	        {
	            Node::Ptr n = *j;
	            if(n->isIpVerified()) verified++;
	
	            dcassert(n->getType() >= 0 && n->getType() <= 4);
	            types[n->getType()]++;
	        }
	
	        dcdebug("DHT Zone Level %d, Nodes: %d (%d verified), Types: %d/%d/%d/%d/%d\n", level, bucket->size(), verified, types[0], types[1], types[2], types[3], types[4]);
	#endif*/
}

}

#endif // STRONG_USE_DHT