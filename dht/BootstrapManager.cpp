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

#include "BootstrapManager.h"
#include "DHT.h"
#include "SearchManager.h"

#include "../client/AdcCommand.h"
#include "../client/ClientManager.h"
#include "../client/LogManager.h"
#include "../client/Util.h"
#include "../client/SimpleXML.h"
#include "../zlib/zlib.h"
#include "../FlyFeatures/flyServer.h"

namespace dht
{

BootstrapManager::BootstrapManager(void):m_count_dht_test_ok(0)
{
}

BootstrapManager::~BootstrapManager(void)
{
	dcassert(m_dht_bootstrap_count.empty());
	dcassert(m_count_dht_test_ok == 0);
}

void BootstrapManager::dht_live_check(const char* p_operation,const string& p_param)
{
    create_url_for_dht_server(); // TODO - потестить это внимательнее
	if(!m_dht_bootstrap_count.empty())
	{
		CFlyLog l_dht_log(p_operation);
		for(auto i=m_dht_bootstrap_count.cbegin();i!= m_dht_bootstrap_count.cend();++i)
		{
		 std::vector<byte> l_data;
		 const string l_url = i->first + p_param;
		 l_dht_log.step("Bootstrap count = " + Util::toString(i->second) + ", URL: " + l_url);
		 Util::getBinaryDataFromInet(Text::toT(m_user_agent).c_str(), 4096, l_url, l_data, 1000);
		}
	}
}
void BootstrapManager::flush_live_check()
{
	if(m_count_dht_test_ok)
	{
	  dht_live_check("[DHT live check]","&live=1");
	  m_count_dht_test_ok = 0;
	}
}
void BootstrapManager::shutdown()
{
	dht_live_check("[Shutdown DHT]","&stop=1");
    m_dht_bootstrap_count.clear();
	m_count_dht_test_ok = 0;
}
string BootstrapManager::create_url_for_dht_server()
{
	const DHTServer& l_server = CFlyServerConfig::getRandomDHTServer();
    m_user_agent = l_server.getAgent();
	string l_url = l_server.getUrl() + "?cid=" + ClientManager::getMyCID().toBase32() + "&encryption=1";  // [!] IRainman fix.
	// store only active nodes to database
	if (ClientManager::isActive(nullptr))
	{
		l_url += "&u4=" + Util::toString(DHT::getInstance()->getPort());
	}
	m_dht_bootstrap_count[l_url]++;
	return l_url;
}
bool BootstrapManager::bootstrap()
{
	if(m_user_agent.empty())
	{
		create_url_for_dht_server();
	}
	if (bootstrapNodes.empty())
	{
		CFlyLog l_dht_log("[DHT]");
		l_dht_log.step(STRING(DHT_BOOTSTRAPPING_STARTED));// [!]NightOrion(translate)
		
		const string l_url = create_url_for_dht_server();

		l_dht_log.step(STRING(DOWNLOAD) + ": " + l_url);
		if(m_user_agent.empty())
			m_user_agent = APPNAME " " A_VERSIONSTRING;
		std::vector<byte> l_data;
		const size_t l_size = Util::getBinaryDataFromInet(Text::toT(m_user_agent).c_str(), 4096, l_url, l_data, 2000);
		if (l_size == 0)
		{
			l_dht_log.step(STRING(ERROR_STRING));
			return false;
		}
		else
		{
		l_dht_log.step("compress size = " + Util::toString(l_size));
		try
		{
			uLongf destLen = 16384;
			std::unique_ptr<uint8_t[]> destBuf; // TODO перевести на вектор - и делать resize + обобщить с кодом из флай-сервера
			// decompress incoming packet
			int result;
			do
			{
				destLen *= 2;
				destBuf.reset(new uint8_t[destLen]);
				
				result = uncompress(&destBuf[0], &destLen, l_data.data(), l_size);
			}
			while (result == Z_BUF_ERROR);
			
			if (result != Z_OK)
			{
				// decompression error!!!
				l_dht_log.step("Decompress error. result = " + Util::toString(result));
				return false;
			}
			
		    l_dht_log.step("decompress Ok! size = " + Util::toString(destLen));
			SimpleXML remoteXml;
			remoteXml.fromXML(string((char*)destBuf.get(), destLen));
			remoteXml.stepIn();
			
			while (remoteXml.findChild("Node"))
			{
				const string l_cid_str = remoteXml.getChildAttrib("CID");
				dcassert(l_cid_str.length() == 39);
				if(l_cid_str.length() == 39)
				{
					const CID cid(l_cid_str);
					const string& i4   = remoteXml.getChildAttrib("I4");
					const string& u4   = remoteXml.getChildAttrib("U4");
					dcassert(Util::toInt(u4)); // http://ssa.in.ua/dcDHT.php?cid=00&encryption=0 U4 может вернуться нулем
					addBootstrapNode(i4, static_cast<uint16_t>(Util::toInt(u4)), cid, UDPKey());
				}
			}
			remoteXml.stepOut();
		}
		catch (const Exception& e)
		{
			l_dht_log.step(STRING(DHT_BOOTSTRAP_ERROR) + ' ' + e.getError());// [!]NightOrion(translate)
			return false;
		}
		}
	}
	return true;
}


void BootstrapManager::addBootstrapNode(const string& ip, uint16_t udpPort, const CID& targetCID, const UDPKey& udpKey)
{
	Lock l(m_cs);
	const BootstrapNode l_node = {ip, udpPort, targetCID, udpKey };
	bootstrapNodes.push_back(l_node);
}

bool BootstrapManager::process()
{
	Lock l(m_cs);
	flush_live_check();
	if (!bootstrapNodes.empty())
	{
		// send bootstrap request
		AdcCommand cmd(AdcCommand::CMD_GET, AdcCommand::TYPE_UDP);
		cmd.addParam("nodes");
		cmd.addParam("dht.xml");
		
		const BootstrapNode& node = bootstrapNodes.front();
		DHT::getInstance()->send(cmd, node.m_ip, node.m_udpPort, node.m_cid, node.m_udpKey);
		
		bootstrapNodes.pop_front();
		return true;
	}
	else
	{
		return bootstrap();
	}
}

}

#endif // STRONG_USE_DHT