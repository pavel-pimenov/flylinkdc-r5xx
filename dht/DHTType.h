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

#ifndef _DHT_TYPE_H
#define _DHT_TYPE_H

#ifdef STRONG_USE_DHT

#include <deque>
#include <unordered_map>

#include "CID.h"
#include "Util.h"

namespace dht
{
struct File
{
	File() : size(0), partial(false) { }
	File(const TTHValue& _tth, int64_t _size, bool _partial) :
		tth(_tth), size(_size), partial(_partial) { }
		
	/** File hash */
	TTHValue tth;
	
	/** File size in bytes */
	int64_t size;
	
	/** Is it partially downloaded file? */
	bool partial;
};

struct Source
{
	GETSET(CID, cid, CID);
	GETSET(string, ip, Ip);
//[-]	GETSET(uint64_t, expires, Expires);
	GETSET(uint64_t, size, Size);
	GETSET(uint16_t, udpPort, UdpPort);
	GETSET(bool, partial, Partial);
};
struct SourceTTH : public Source
{
	GETSET(TTHValue, tth, TTH);
};
typedef std::vector<SourceTTH> TTHArray;
typedef std::deque<Source> SourceList;
//typedef std::unordered_map<TTHValue, SourceList> TTHMap;

struct UDPKey
{
	string  m_ip;
	CID     m_key;
	
	bool isZero() const
	{
		return m_ip.empty() || m_key.isZero() || m_ip == "0.0.0.0";
	}
};

struct BootstrapNode
{
	// int			m_id;
	string      m_ip;
	uint16_t    m_udpPort;
	CID         m_cid;
	UDPKey      m_udpKey;
};

}

#endif // STRONG_USE_DHT

#endif  // _DHT_TYPE_H