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
#include "ConnectionManager.h"
#include "DHT.h"

#include "../client/AdcCommand.h"
#include "../client/ConnectionManager.h"
#include "../client/CryptoManager.h"

namespace dht
{

ConnectionManager::ConnectionManager(void)
{
}

ConnectionManager::~ConnectionManager(void)
{
}

/*
 * Sends Connect To Me request to online node
 */
void ConnectionManager::connect(const Node::Ptr& node, const string& token)
{
	connect(node, token, CryptoManager::TLSOk() && node->getUser()->isSet(User::ADCS));
}

void ConnectionManager::connect(const Node::Ptr& node, const string& token, bool secure)
{
	// don't allow connection if we didn't proceed a handshake
	if (!node->isOnline())
	{
		// do handshake at first
		DHT::getInstance()->info(node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
		                         DHT::PING | DHT::CONNECTION, node->getUser()->getCID(), node->getUdpKey());
		return;
	}
	
	const bool active = ClientManager::isActive(nullptr);
	
	// if I am not active, send reverse connect to me request
	AdcCommand cmd(active ? AdcCommand::CMD_CTM : AdcCommand::CMD_RCM, AdcCommand::TYPE_UDP);
	cmd.addParam(secure ? AdcSupports::SECURE_CLIENT_PROTOCOL_TEST : AdcSupports::CLIENT_PROTOCOL);
	
	if (active)
	{
		uint16_t port = secure ? ::ConnectionManager::getInstance()->getSecurePort() : ::ConnectionManager::getInstance()->getPort();
		cmd.addParam(Util::toString(port));
	}
	
	cmd.addParam(token);
	
	DHT::getInstance()->send(cmd, node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
	                         node->getUser()->getCID(), node->getUdpKey());
}

/*
 * Creates connection to specified node
 */
void ConnectionManager::connectToMe(const Node::Ptr& p_node, const AdcCommand& p_cmd)
{
	const string& protocol = p_cmd.getParam(1);
	const string& port = p_cmd.getParam(2);
	const string& token = p_cmd.getParam(3);
	
	bool secure = false;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		// Nothing special
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::TLSOk())
	{
		secure = true;
	}
	else
	{
		AdcCommand l_cmd(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_UNSUPPORTED, "Protocol unknown", AdcCommand::TYPE_UDP);
		l_cmd.addParam("PR", protocol);
		l_cmd.addParam("TO", token);
		
		DHT::getInstance()->send(l_cmd, p_node->getIdentity().getIpAsString(), p_node->getIdentity().getUdpPort(),
		                         p_node->getUser()->getCID(), p_node->getUdpKey());
		return;
	}
	
	if (!p_node->getIdentity().isTcpActive())
	{
		AdcCommand err(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "IP unknown", AdcCommand::TYPE_UDP);
		DHT::getInstance()->send(err, p_node->getIdentity().getIpAsString(), p_node->getIdentity().getUdpPort(),
		                         p_node->getUser()->getCID(), p_node->getUdpKey());
		return;
	}
	
	::ConnectionManager::getInstance()->adcConnect(*p_node, static_cast<uint16_t>(Util::toInt(port)), token, secure);
}

/*
 * Sends request to create connection with me
 */
void ConnectionManager::revConnectToMe(const Node::Ptr& node, const AdcCommand& cmd)
{
	// this is valid for active-passive connections only
	if (!ClientManager::isActive(nullptr))
		return;
		
	const string& protocol = cmd.getParam(1);
	const string& token = cmd.getParam(2);
	
	bool secure;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		secure = false;
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::TLSOk())
	{
		secure = true;
	}
	else
	{
		AdcCommand sta(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_UNSUPPORTED, "Protocol unknown", AdcCommand::TYPE_UDP);
		sta.addParam("PR", protocol);
		sta.addParam("TO", token);
		
		DHT::getInstance()->send(sta, node->getIdentity().getIpAsString(), node->getIdentity().getUdpPort(),
		                         node->getUser()->getCID(), node->getUdpKey());
		return;
	}
	
	connect(node, token, secure);
}

}

#endif // STRONG_USE_DHT