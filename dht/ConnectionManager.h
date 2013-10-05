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

#ifndef _DHT_CONNECTIONMANAGER_H
#define _DHT_CONNECTIONMANAGER_H

#ifdef STRONG_USE_DHT

#include "KBucket.h"

#include "../client/Singleton.h"
#include "../client/User.h"

namespace dht
{

class ConnectionManager :
	public Singleton<ConnectionManager>
{
	public:
		ConnectionManager(void);
		~ConnectionManager(void);
		
		/** Sends Connect To Me request to online node */
		void connect(const Node::Ptr& node, const string& token);
		void connect(const Node::Ptr& node, const string& token, bool secure);
		
		/** Creates connection to specified node */
		void connectToMe(const Node::Ptr& node, const AdcCommand& cmd);
		
		/** Sends request to create connection with me */
		void revConnectToMe(const Node::Ptr& node, const AdcCommand& cmd);
		
};

}

#endif // STRONG_USE_DHT

#endif  // _DHT_CONNECTIONMANAGER_H