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

#include "DHT.h"
#include "ConnectionManager.h"
#include "IndexManager.h"
#include "SearchManager.h"
#include "TaskManager.h"
#include "Utils.h"

#include "../client/AdcSupports.h"
#include "../client/CID.h"
#include "../client/ClientManager.h"
#include "../client/CryptoManager.h"
#include "../client/LogManager.h"
#include "../client/SettingsManager.h"
#include "../client/ThrottleManager.h"//[+]IRainman SpeedLimiter
#include "../client/ShareManager.h"
#include "../client/UploadManager.h"
#include "../client/User.h"
#include "../FlyFeatures/flyServer.h"
namespace dht
{
string DHT::g_lastExternalIP;

DHT::DHT(void) : m_bucket(nullptr), m_lastPacket(0), requestFWCheck(true), m_firewalled(true)
{
	g_lastExternalIP = Util::getLocalOrBindIp(true); // hack
	m_type = ClientBase::DHT;
	
	IndexManager::newInstance();
}

DHT::~DHT(void)
{
	// when DHT is disabled, we shouldn't try to perform exit cleanup
	/* [-] IRainman fix: Please see DHT::stop(...)
	if (bucket == NULL)
	{
	    IndexManager::deleteInstance();
	    return;
	}
	stop(true); [-] IRainman fix: Please see shutdown function in DCPlusPlus module.
	*/
	
	IndexManager::deleteInstance();
}

/*
 * Starts DHT.
 */
void DHT::start()
{
	if (!BOOLSETTING(USE_DHT))
		return;
		
	// start with global firewalled status
	m_firewalled = false; //  !ClientManager::isActive(nullptr);
	requestFWCheck = true;
	//
	
	if (!m_bucket)
	{
		// if (BOOLSETTING(UPDATE_IP_DHT))
		//  SET_SETTING(EXTERNAL_IP, Util::emptyString); //fix https://code.google.com/p/flylinkdc/issues/detail?id=1264
		
		// [!] IRainman fix
		SearchManager::newInstance();
		TaskManager::newInstance();
		ConnectionManager::newInstance();
		
		m_bucket = new RoutingTable();
		
		loadData();
		
		TaskManager::getInstance()->start();
		
		m_dht_socket.listen();// [+] IRainman fix.
		if (SETTING(INCOMING_CONNECTIONS) != SettingsManager::INCOMING_FIREWALL_UPNP)
		{
			DHT::test_dht_port();
		}
	}
	// [-] IRainman fix. socket.listen();
}
void DHT::test_dht_port()
{
	// Запускаем тест порта DHT - TODO в отдельном потоке
	string l_external_ip;
	std::vector<unsigned short> l_udp_port, l_tcp_port;
	l_udp_port.push_back(SETTING(DHT_PORT));
	const bool l_is_udp_port_send = CFlyServerJSON::pushTestPort(l_udp_port, l_tcp_port, l_external_ip, 0, "DHT");
	BootstrapManager::clear_live_check();
	dcassert(l_is_udp_port_send);
}
void DHT::stop(bool exiting)
{
	if (!m_bucket)
		return;
		
	//socket.disconnect(); [-] IRainman fix.
	
	if (exiting || !BOOLSETTING(USE_DHT))
	{
		BootstrapManager::shutdown();
		CFlyLog l_TaskManagerLog("DHT::stop");
		m_dht_socket.disconnect(); // [+] IRainman fix.
		
		TaskManager::getInstance()->stop(); // [+] IRainman fix.
		
		saveData();
		m_lastPacket = 0;
		safe_delete(m_bucket);
		ConnectionManager::deleteInstance();
		dcassert(TaskManager::getInstance()->isDebugTimerExecute() == false);
		TaskManager::deleteInstance(); //[!] Разрушили хотя другой поток может еще выполняться
		// Поймал assert в TaskManager::on(TimerManagerListener::Second
		SearchManager::deleteInstance();
		RoutingTable::resetNodesCount(); // http://code.google.com/p/flylinkdc/issues/detail?id=1003
	}
}

/*
 * Process incoming command
 */
void DHT::dispatch(const string& aLine, const string& ip, uint16_t port, bool isUdpKeyValid)
{
	dcassert(!ClientManager::isShutdown());
	if (ClientManager::isShutdown())
		return;
	// check node's IP address
	if (!Utils::isGoodIPPort(ip, port))
	{
		LogManager::dht_message("DHT::dispatch] isGoodIPPort - error ip = " + ip + ":" + Util::toString(port));
		//socket.send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_BAD_IP, "Your client supplied invalid IP: " + ip, AdcCommand::TYPE_UDP), ip, port);
		return; // invalid ip/port supplied
	}
	
	try
	{
		AdcCommand cmd(aLine);
		const string cid = cmd.getParam(0);
		const CID l_CID(cid);
		if (cid.size() != 39)
		{
			LogManager::dht_message("DHT::dispatch] cid.size() != 39 - error ip = " + ip + ":" + Util::toString(port) + " cid = " + cmd.toString(l_CID));
			return;
		}
		
		// flood protection
		if (!Utils::checkFlood(ip, cmd))
		{
			LogManager::dht_message("DHT::dispatch] checkFlood - error ip = " + ip + ":" + Util::toString(port) + " cmd = " + cmd.toString(l_CID));
			return;
		}
		
		
		// ignore message from myself
		if (l_CID == ClientManager::getMyCID() || ip == g_lastExternalIP) // [!] IRainman fix.
		{
			LogManager::dht_message("DHT::dispatch] CID(cid) == ClientManager::getMyCID() || ip == g_lastExternalIP. error ip = " + ip +
			                        ":" + Util::toString(port) + " cid = " + cmd.toString(l_CID) + " g_lastExternalIP = " + g_lastExternalIP);
			return;
		}
		
		m_lastPacket = GET_TICK();
		
		// all communication to this node will be encrypted with this key
		UDPKey key;
		string udpKey;
		if (cmd.getParam("UK", 1, udpKey))
		{
			key.m_key = CID(udpKey);
			key.m_ip = DHT::getLastExternalIP();
		}
		Node::Ptr l_node = addDHTNode(CID(cid), ip, port, key, true, isUdpKeyValid);
		if (!l_node)
			return;
		// node is requiring FW check
		string internalUdpPort;
		if (cmd.getParam("FW", 1, internalUdpPort))
		{
			const bool l_firewalled = (Util::toInt(internalUdpPort) != port);
			if (l_firewalled)
			{
				l_node->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
				l_node->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE_BIT);
			}
			
			// send him his external ip and port
			AdcCommand l_cmd(AdcCommand::SEV_SUCCESS, AdcCommand::SUCCESS, !l_firewalled ? "UDP port opened" : "UDP port closed", AdcCommand::TYPE_UDP);
			l_cmd.addParam("FC", "FWCHECK");
			l_cmd.addParam("I4", ip);
			l_cmd.addParam("U4", Util::toString(port));
			send(l_cmd, ip, port, CID(cid), key);
		}
		
#define C(n) case AdcCommand::CMD_##n: handle(AdcCommand::n(), ip, port, key, cmd); break;
		switch (cmd.getCommand())
		{
			case AdcCommand::CMD_INF:
				handle(AdcCommand::INF(), ip, port, key, isUdpKeyValid, cmd);
				break;  // user's info
				C(SCH); // search request
				C(RES); // response to SCH
				C(PUB); // request to publish file
				C(CTM); // connection request
				C(RCM); // reverse connection request
				C(STA); // status message
				C(PSR); // partial file request
				C(MSG); // private message
				C(GET); // get some data
			case AdcCommand::CMD_SND:
				handle(AdcCommand::SND(), ip, port, key, isUdpKeyValid, cmd);
				break; // response to GET
				
			default:
				dcdebug("Unknown ADC command: %.50s\n", aLine.c_str());
				break;
#undef C
				
		}
	}
	catch (const ParseException&)
	{
		dcdebug("Invalid ADC command: %.50s\n", aLine.c_str());
	}
}

/*
 * Sends command to ip and port
 */
bool DHT::send(AdcCommand& cmd, const string& ip, uint16_t port, const CID& targetCID, const UDPKey& udpKey)
{
	dcassert(BOOLSETTING(USE_DHT));
	if (1) // SettingsManager::g_TestUDPDHTLevel == true)
	{
		{
			// FW check
			CFlyFastLock(fwCheckCs);
			if (requestFWCheck/* && (firewalledWanted.size() + firewalledChecks.size() < FW_RESPONSES)*/)
			{
				if (firewalledWanted.find(ip) == firewalledWanted.end()) // only when not requested from this node yet // [!] IRainman opt.
				{
					firewalledWanted.insert(ip);
					cmd.addParam("FW", Util::toString(getPort()));
				}
			}
		}
		m_dht_socket.send(cmd, ip, port, targetCID, udpKey);
		return true;
	}
	else
	{
		//dcassert(0);
#ifdef FLYLINKDC_BETA
		LogManager::dht_message("Skip DHT::send - error test DHT port!");
#endif
	}
	return false;
}

/*
 * Creates new (or update existing) node which is NOT added to our routing table
 */
/*
Node::Ptr DHT::createNode(const CID& cid, const string& ip, uint16_t port, bool update, bool isUdpKeyValid)
    {
        // create user as offline (only TCP connected users will be online)
        UserPtr u = ClientManager::getInstance()->getUser(cid,true);

        CFlyFastLock(cs);
        return m_bucket->createNode(u, ip, port, update, isUdpKeyValid);
    }
*/

/*
 * Adds node to routing table
 */
Node::Ptr DHT::addDHTNode(const CID& cid, const string& ip, uint16_t port, const UDPKey& udpKey, bool update, bool isUdpKeyValid)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		// create user as offline (only TCP connected users will be online)
		// https://drdump.com/DumpGroup.aspx?DumpGroupID=239463&Login=guest
		UserPtr u = ClientManager::getUser(cid, true); // TODO - утекает. если долго работать тут появляется много юзеров.
		// а когда их удаляем?
		CFlyFastLock(cs);
		return m_bucket->addOrUpdate(u, ip, port, udpKey, update, isUdpKeyValid);
	}
	else
	{
		return Node::Ptr();
	}
}

/*
 * Finds "max" closest nodes and stores them to the list
 */
void DHT::getClosestNodes(const CID& cid, std::map<CID, Node::Ptr>& closest, unsigned int max, uint8_t maxType)
{
	CFlyFastLock(cs);
	m_bucket->getClosestNodes(cid, closest, max, maxType);
}

/*
 * Removes dead nodes
 */
void DHT::checkExpiration(uint64_t aTick)
{
	CFlyFastLock(fwCheckCs);
	firewalledWanted.clear();
}

/*
 * Finds the file in the network
 */
void DHT::findFile(const string& tth, uint32_t p_token)
{
	if (isConnected())
		SearchManager::getInstance()->findFile(tth, p_token);
}

/*
 * Sends our info to specified ip:port
 */
void DHT::info(const string& ip, uint16_t port, uint32_t type, const CID& targetCID, const UDPKey& udpKey)
{
	dcassert(port);
	// TODO: what info is needed?
	AdcCommand cmd(AdcCommand::CMD_INF, AdcCommand::TYPE_UDP);
	
#ifdef SVNVERSION
#define VER VERSIONSTRING SVNVERSION
#else
#define VER VERSIONSTRING
#endif
	
	cmd.addParam("TY", Util::toString(type));
	cmd.addParam("AP", "FlylinkDC++"
#ifdef FLYLINKDC_HE
	             "HE"
#endif
	            );
	            
	cmd.addParam("VE", A_VERSIONSTRING);
	cmd.addParam("NI", SETTING(NICK));
	cmd.addParam("SL", Util::toString(UploadManager::getSlots()));
	
	if (BOOLSETTING(THROTTLE_ENABLE) && ThrottleManager::getInstance()->getUploadLimitInBytes() != 0)
	{
		cmd.addParam("US", Util::toString(ThrottleManager::getInstance()->getUploadLimitInBytes()));
	}
	else
	{
		cmd.addParam("US", Util::toString((long)(Util::toDouble(SETTING(UPLOAD_SPEED)) * 1024 * 1024 / 8)));
	}
	
	string su;
	if (CryptoManager::TLSOk())
		su += AdcSupports::ADCS_FEATURE + ',';
		
	// TCP status according to global status
	if (ClientManager::isActive(nullptr))
		su += AdcSupports::TCP4_FEATURE + ',';
		
	// UDP status according to UDP status check
	if (!isFirewalled())
		su += AdcSupports::UDP4_FEATURE + ',';
		
	if (!su.empty())
	{
		su.resize(su.size() - 1);
	}
	cmd.addParam("SU", su);
	
	send(cmd, ip, port, targetCID, udpKey);
}

/*
 * Sends Connect To Me request to online node
 */
void DHT::connect(const OnlineUser& ou, const string& p_token, bool p_is_force_passive)
{
	// this is DHT's node, so we can cast ou to Node
	ConnectionManager::getInstance()->connect((Node*)&ou, p_token, false);
}

/*
 * Sends private message to online node
 */
void DHT::privateMessage(const OnlineUserPtr& ou, const string& aMessage, bool thirdPerson /* = false */)
{
	AdcCommand cmd(AdcCommand::CMD_MSG, AdcCommand::TYPE_UDP);
	cmd.addParam(aMessage);
	if (thirdPerson)
		cmd.addParam("ME", "1");
		
	auto key = UDPKey();
	key.m_ip = ou->getIdentity().getIpAsString();
	key.m_key = ou->getUser()->getCID();
	
	send(cmd, ou->getIdentity().getIpAsString(), ou->getIdentity().getUdpPort(), ou->getUser()->getCID(), key);
}

/*
 * Loads network information from XML file
 */
void DHT::loadData()
{
	try
	{
		SimpleXML xml;
		const string l_dht_xml_file = Util::getConfigPath() + DHT_FILE;
		if (::File::isExist(l_dht_xml_file))
		{
			CFlyLog l_log("[dht]");
			try
			{
				::File l_xml_file(l_dht_xml_file, ::File::READ, ::File::OPEN);
				xml.fromXML(l_xml_file.read());
				l_log.step("SimpleXML.load(dht.xml)");
				xml.stepIn();
			}
			catch (const FileException& e)
			{
				LogManager::message("[dht][DHT::loadData] FileException [" + string(e.what()) + "]");
			}
			m_bucket->loadNodes(xml);
			l_log.step("loadNodes(dht.xml)");
			IndexManager::getInstance()->loadIndexes(xml);
			l_log.step("loadIndexes(dht.xml)");
			xml.stepOut();
			::File::deleteFile(l_dht_xml_file);
		}
		else
		{
			// Загружаем ноды из базы
			CFlyLog l_log("[dht] loadNodes(sqlite)");
			m_bucket->loadNodes(xml);
		}
	}
	catch (const Exception& e)
	{
		LogManager::message("[dht][DHT::loadData][Error] Exception [" + string(e.what()) + "]");
		dcdebug("%s\n", e.what());
	}
}

/*
 * Finds "max" closest nodes and stores them to the list
 */
void DHT::saveData()
{
	CFlyFastLock(cs);
	m_bucket->saveNodes();
}

/*
 * Message processing
 */

// user's info
void DHT::handle(AdcCommand::INF, const string& ip, uint16_t port, const UDPKey& udpKey, bool isUdpKeyValid, const AdcCommand& c) noexcept
{
	dcassert(!ClientManager::isShutdown());
	const CID cid = CID(c.getParam(0));
	// add node to our routing table and put him online
	const Node::Ptr node = addDHTNode(cid, ip, port, udpKey, true, isUdpKeyValid);
	if (!node)
		return;
	auto& id = node->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'node->getIdentity()' expression repeatedly. dht.cpp 440
	InfType it = NONE;
	for (auto i = c.getParameters().cbegin() + 1; i != c.getParameters().cend(); ++i)
	{
		if (i->length() < 2)
			continue;
			
		// [!] IRainman fix.
		switch (*(short*)i->c_str())
		{
			case TAG('T', 'Y'):
			{
				it = (InfType)Util::toInt(i->c_str() + 2);
				break;
			}
			case TAG('S', 'L'):
			{
				id.setSlots(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('F', 'S'):
			{
				id.setFreeSlots(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('U', 'S'):
			{
				id.setLimit(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('S', 'U'):
			{
				AdcSupports::setSupports(id, i->substr(2));
				break;
			}
			case TAG('N', 'I'):
			{
				id.setNick(i->substr(2));
				break;
			}
			case TAG('I', '6'):
			case TAG('I', '4'):
			case TAG('U', '4'):
			case TAG('U', 'K'):
			{
				// avoid IP+port spoofing + don't store key into map
				break;
			}
			default:
			{
				id.setStringParam(i->c_str(), i->substr(2));
			}
		}
		// [~] IRainman fix.
	}
	/* [-] IRainman - see AdcSupports::setSupports.
	if (id.isSupports(ADCS_FEATURE))
	{
	    node->getUser()->setFlag(User::TLS);
	}
	  [-] */
	/* [-] IRainman - deprecated.
	if (id.getLimit())
	{
	    id.setConnection(Util::formatBytes(uint64_t(id.getLimit())) + "/s");
	}
	  [-] */
	if (((it & CONNECTION) == CONNECTION) && !node->isOnline()) // only when connection is required
	{
		// put him online so we can make a connection with him
		node->inc();
		node->setOnline(true);
		ClientManager::getInstance()->putOnline(node.get(), true);
		
		// FIXME: if node has not been added into the routing table (for whatever reason), we should take some action
		// to avoid having him online forever (bringing memory leak for such nodes)
	}
	
	// do we wait for any search results from this user?
	SearchManager::getInstance()->processSearchResults(node->getUser(), node->getIdentity().getSlots());
	
	if ((it & PING) == PING)
	{
		// remove ping flag to avoid ping-pong-ping-pong-ping...
		info(ip, port, it & ~PING, cid, udpKey);
	}
}

// incoming search request
void DHT::handle(AdcCommand::SCH, const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& c) noexcept
{
	if (!ClientManager::isShutdown())
	{
		SearchManager::getInstance()->processSearchRequest(ip, port, udpKey, c);
	}
}

// incoming search result
void DHT::handle(AdcCommand::RES, const string& /*ip*/, uint16_t /*port*/, const UDPKey& /*udpKey*/, const AdcCommand& c) noexcept
{
	SearchManager::getInstance()->processSearchResult(c);
}

// incoming publish request
void DHT::handle(AdcCommand::PUB, const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& c) noexcept
{
	if (!isFirewalled()) // we should index this entry only if our UDP port is opened
		IndexManager::getInstance()->processPublishSourceRequest(ip, port, udpKey, c);
}

// connection request
void DHT::handle(AdcCommand::CTM, const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& c) noexcept
{
	CID cid = CID(c.getParam(0));
	
	// connection allowed with online nodes only, so try to get them directly from ClientManager
	OnlineUserPtr node = ClientManager::findDHTNode(cid);
	if (node != NULL)
		ConnectionManager::getInstance()->connectToMe((Node*)node.get(), c);
	else
	{
		// node is not online
		// this can happen if we restarted our client (we are online for him, but he is offline for us)
		DHT::getInstance()->info(ip, port, DHT::PING | DHT::CONNECTION, cid, udpKey);
	}
}

// reverse connection request
void DHT::handle(AdcCommand::RCM, const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& c) noexcept
{
	CID cid = CID(c.getParam(0));
	
	// connection allowed with online nodes only, so try to get them directly from ClientManager
	OnlineUserPtr node = ClientManager::findDHTNode(cid);
	if (node != NULL)
		ConnectionManager::getInstance()->revConnectToMe((Node*)node.get(), c);
	else
	{
		// node is not online
		// this can happen if we restarted our client (we are online for him, but he is offline for us)
		DHT::getInstance()->info(ip, port, DHT::PING | DHT::CONNECTION, cid, udpKey);
	}
}

// status message
void DHT::handle(AdcCommand::STA, const string& fromIP, uint16_t port, const UDPKey& /*udpKey*/, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 3)
	{
		LogManager::dht_message("DHT::handle c.getParameters().size() < 3 - fromIP = " + fromIP + ":" + Util::toString(port) + " c = " + c.toString(CID()));
		return;
	}
	
	int code = Util::toInt(c.getParam(1).substr(1));
	
	if (code == 0)
	{
		string resTo;
		if (!c.getParam("FC", 2, resTo))
		{
			LogManager::dht_message("DHT::handle !c.getParam(FC, 2, resTo) - fromIP = " + fromIP + ":" + Util::toString(port) + " c = " + c.toString(CID()) + " FC = " + resTo);
			return;
		}
		
		if (resTo == "PUB")
		{
			LogManager::dht_message("DHT::handle resTo == PUB - empty code!  fromIP = " + fromIP + ":" + Util::toString(port));
			/*#ifdef _DEBUG
			            // don't do anything
			            string tth;
			            if (!c.getParam("TR", 1, tth))
			                return;
			
			            try
			            {
			                string fileName = Util::getFileName(ShareManager::getInstance()->toVirtual(TTHValue(tth)));
			                LogManager::message("DHT (" + fromIP + "): File published: " + fileName);//TODO translate
			            }
			            catch (const ShareException&)
			            {
			                // published non-shared file??? Maybe partial file
			                LogManager::message("DHT (" + fromIP + "): Partial file published: " + tth);//TODO translate
			
			            }
			#endif*/
		}
		else if (resTo == "FWCHECK")
		{
			CFlyFastLock(fwCheckCs);
			// [!] IRainman opt.
			const auto j = firewalledWanted.find(fromIP); // [1] https://www.box.net/shared/4b2e554c75f77c3f9054
			if (j == firewalledWanted.end())
			{
				LogManager::dht_message("DHT::handle i == firewalledWanted.end() - fromIP = " + fromIP + ":" + Util::toString(port));
				return; // we didn't requested firewall check from this node
			}
			
			firewalledWanted.erase(j);
			if (firewalledChecks.find(fromIP) != firewalledChecks.end())
			{
				LogManager::dht_message("DHT::handle i == firewalledChecks.find(fromIP) != firewalledChecks.end() - fromIP = " + fromIP + ":" + Util::toString(port));
				return; // already received firewall check from this node
			}
			// [~] IRainman opt.
			
			string externalIP;
			string externalUdpPort;
			if (!c.getParam("I4", 1, externalIP) || !c.getParam("U4", 1, externalUdpPort))
			{
				LogManager::dht_message("DHT::handle !c.getParam('I4', 1, externalIP) || !c.getParam('U4', 1, externalUdpPort) - fromIP = " + fromIP + ":" + Util::toString(port));
				return; // no IP and port in response
			}
			const auto l_udp_port = static_cast<uint16_t>(Util::toInt(externalUdpPort));
			firewalledChecks.insert(std::make_pair(fromIP, std::make_pair(externalIP, l_udp_port)));
			if (firewalledChecks.size() >= FW_RESPONSES)
			{
				// when we received more firewalled statuses, we will be firewalled
				int fw = 0;
				string lastIP;
				for (auto i = firewalledChecks.cbegin(); i != firewalledChecks.cend(); ++i)
				{
					string ip = i->second.first;
					uint16_t udpPort = i->second.second;
					
					if (udpPort != getPort())
						fw++;
					else
						fw--;
						
					if (lastIP.empty())
					{
						externalIP = ip;
						lastIP = ip;
					}
					
					//If the last check matches this one, reset our current IP.
					//If the last check does not match, wait for our next incoming IP.
					//This happens for one reason.. a client responsed with a bad IP.
					if (ip == lastIP)
						externalIP = ip;
					else
						lastIP = ip;
				}
				
				if (fw >= 0)
				{
					// we are probably firewalled, so our internal UDP port is unaccessible
					if (externalIP != g_lastExternalIP || !m_firewalled)
					{
						LogManager::message("DHT: " + STRING(DHT_FIREWALLED_UDP) + " (IP: " + externalIP + ") port:" + externalUdpPort);
					}
					m_firewalled = true;
				}
				else
				{
					if (externalIP != g_lastExternalIP || m_firewalled)
					{
						LogManager::message("DHT: " + STRING(DHT_OUR_UPD_PORT_OPEND) + " (IP: " + externalIP + ") port:" + externalUdpPort);
					}
					
					m_firewalled = false;
				}
				
				dcassert(!externalIP.empty())
				if (BOOLSETTING(UPDATE_IP_DHT) && !externalIP.empty())
				{
					SET_SETTING(EXTERNAL_IP, externalIP);
				}
				
				firewalledChecks.clear();
				firewalledWanted.clear();
				
				g_lastExternalIP = externalIP;
				requestFWCheck = false;
			}
		}
		return;
	}
	
	// display message in all other cases
	//string msg = c.getParam(2);
	//if(!msg.empty())
	//  LogManager::message("DHT (" + fromIP + "): " + msg);
}

// partial file request
void DHT::handle(AdcCommand::PSR, const string& p_ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& c) noexcept
{
	const CID cid = CID(c.getParam(0));
	dcassert(c.getParam(0).size() == 39);
	// !!!!!!!!!!!!!!!!!!!!!// !!!!!!!!!!!!!!!!!!!!!
	// !!!!!!!!!!!!!!!!!!!!! c.getParameters().erase(c.getParameters().begin());  // remove CID from UDP command
	// Не нужно удалять?
	
	// connection allowed with online nodes only, so try to get them directly from ClientManager
	const OnlineUserPtr node = ClientManager::findDHTNode(cid);
	if (node != NULL)
	{
		boost::system::error_code l_ec;
		const auto l_ip4 = boost::asio::ip::address_v4::from_string(p_ip, l_ec);
		dcassert(!l_ec);
		if (!l_ec)
		{
			::SearchManager::getInstance()->onPSR(c, node->getUser(), l_ip4);
		}
		else
		{
#ifdef FLYLINKDC_BETA
			const string l_message = "Identity::setIP Error IP = " + p_ip;
			LogManager::message(l_message);
			CFlyServerJSON::pushError(27, l_message);
#endif
		}
	}
	else
	{
		// node is not online
		// this can happen if we restarted our client (we are online for him, but he is offline for us)
		DHT::getInstance()->info(p_ip, port, DHT::PING | DHT::CONNECTION, cid, udpKey);
	}
}

// private message
void DHT::handle(AdcCommand::MSG, const string& /*ip*/, uint16_t /*port*/, const UDPKey& /*udpKey*/, const AdcCommand& /*c*/) noexcept
{
	// not implemented yet
	//fly_fire1(ClientListener::PrivateMessage(), this, *node, to, node, c.getParam(0), c.hasFlag("ME", 1));
	
	//privateMessage(*node, "Sorry, private messages aren't supported yet!", false);
}

void DHT::handle(AdcCommand::GET, const string& ip, uint16_t port, const UDPKey& udpKey, const AdcCommand& c) noexcept
{
	if (c.getParam(1) == "nodes" && c.getParam(2) == "dht.xml")
	{
		AdcCommand cmd(AdcCommand::CMD_SND, AdcCommand::TYPE_UDP);
		cmd.addParam(c.getParam(1));
		cmd.addParam(c.getParam(2));
		
		SimpleXML xml;
		xml.addTag("Nodes");
		xml.stepIn();
		
		// get 20 random contacts
		Node::Map nodes;
		DHT::getInstance()->getClosestNodes(CID::generate(), nodes, 20, 2);
		
		// add nodelist in XML format
		for (auto i = nodes.cbegin(); i != nodes.cend(); ++i)
		{
			xml.addTag("Node");
			xml.addChildAttrib("CID", i->second->getUser()->getCID().toBase32());
			xml.addChildAttrib("I4", i->second->getIdentity().getIpAsString());
			xml.addChildAttrib("U4", i->second->getIdentity().getUdpPort());
		}
		
		xml.stepOut();
		
		string nodesXML;
		StringOutputStream sos(nodesXML);
		//sos.write(SimpleXML::utf8Header);
		xml.toXML(&sos);
		
		cmd.addParam(Utils::compressXML(nodesXML));
		
		send(cmd, ip, port, CID(c.getParam(0)), udpKey);
	}
}

void DHT::handle(AdcCommand::SND, const string& ip, uint16_t port, const UDPKey& udpKey, bool isUdpKeyValid, const AdcCommand& c) noexcept
{
	if (c.getParam(1) == "nodes" && c.getParam(2) == "dht.xml")
	{
		// add node to our routing table
		if (isUdpKeyValid)
		{
			const auto l_node = addDHTNode(CID(c.getParam(0)), ip, port, udpKey, false, true);
			if (!l_node)
				return;
		}
		
		try
		{
			SimpleXML xml;
			xml.fromXML(c.getParam(3));
			xml.stepIn();
			
			// extract bootstrap nodes
			unsigned int n = 20;
			while (xml.findChild("Node") && n-- > 0)
			{
				const CID cid(xml.getChildAttrib("CID"));
				
				if (cid.isZero())
				{
					LogManager::dht_message("DHT::handle(AdcCommand::SND cid.isZero()! IP = " + ip + ":" + Util::toString(port));
					continue;
				}
				
				// don't bother with myself
				if (ClientManager::getMyCID() == cid) // [!] IRainman fix.
				{
					LogManager::dht_message("DHT::handle(AdcCommand::SND ClientManager::getMyCID() == cid - IP = " + ip + ":" + Util::toString(port) + " CID = " + cid.toBase32());
					continue;
				}
				
				const string& i4    = xml.getChildAttrib("I4");
				const uint16_t u4         = static_cast<uint16_t>(xml.getIntChildAttrib("U4"));
				
				// don't bother with private IPs
				if (!Utils::isGoodIPPort(i4, u4))
				{
					LogManager::dht_message("DHT::handle(AdcCommand::SND !Utils::isGoodIPPort(i4, u4) - IP = " + ip + ":" + Util::toString(port));
					continue;
				}
				
				// create verified node, it's not big risk here and allows faster bootstrapping
				// if this node already exists in our routing table, don't update its ip/port for security reasons
				if (!addDHTNode(cid, i4, u4, UDPKey(), false, true))
					return;
			}
#ifdef _DEBUG
			int l_tail_count_node = 0;
			while (xml.findChild("Node") && l_tail_count_node++)
			{
			}
			if (l_tail_count_node)
			{
				LogManager::message("DHT::handle(AdcCommand::SND l_tail_count_node = " + Util::toString(l_tail_count_node));
			}
#endif
			xml.stepOut();
		}
		catch (const SimpleXMLException& e)
		{
			LogManager::message("DHT::handle(AdcCommand::SND malformed node list = " + e.getError());
		}
	}
}

}

#endif // STRONG_USE_DHT