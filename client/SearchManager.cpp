/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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

#include "stdinc.h"
#include "UploadManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "StringTokenizer.h"
#include "FinishedManager.h"
#include "DebugManager.h"
#include "../FlyFeatures/flyServer.h"


uint16_t SearchManager::g_search_port = 0;

const char* SearchManager::getTypeStr(Search::TypeModes type)
{
	static const char* g_types[Search::TYPE_LAST_MODE] =
	{
		CSTRING(ANY),
		CSTRING(AUDIO),
		CSTRING(COMPRESSED),
		CSTRING(DOCUMENT),
		CSTRING(EXECUTABLE),
		CSTRING(PICTURE),
		CSTRING(VIDEO_AND_SUBTITLES),
		CSTRING(DIRECTORY),
		"TTH",
		CSTRING(CD_DVD_IMAGES),
		CSTRING(COMICS),
		CSTRING(BOOK),
	};
	return g_types[type];
}

SearchManager::SearchManager()
{

}

SearchManager::~SearchManager()
{
	if (socket.get())
	{
		stopThread();
		socket->disconnect();
#ifdef _WIN32
		join();
#endif
	}
}

void SearchManager::runTestUDPPort()
{
	extern bool g_DisableTestPort;
	if (g_DisableTestPort == false && boost::logic::indeterminate(SettingsManager::g_TestUDPSearchLevel))
	{
		string l_external_ip;
		std::vector<unsigned short> l_udp_port, l_tcp_port;
		l_udp_port.push_back(SETTING(UDP_PORT));
		bool l_is_udp_port_send = CFlyServerJSON::pushTestPort(l_udp_port, l_tcp_port, l_external_ip, 0, "UDP");
		if (l_is_udp_port_send)
		{
			SettingsManager::g_UDPTestExternalIP = l_external_ip;
		}
	}
}

void SearchManager::listen()
{
	disconnect();
	
	try
	{
		socket.reset(new Socket);
		socket->create(Socket::TYPE_UDP);
		socket->setBlocking(true);
		socket->setInBufSize();
		if (BOOLSETTING(AUTO_DETECT_CONNECTION))
		{
			g_search_port = socket->bind(0, Util::emptyString);
		}
		else
		{
			const auto l_ip = SETTING(BIND_ADDRESS);
			const auto l_port = SETTING(UDP_PORT);
			g_search_port = socket->bind(static_cast<uint16_t>(l_port), l_ip);
		}
		SET_SETTING(UDP_PORT, g_search_port);
#ifdef _DEBUG
		LogManager::message("Start search manager! UDP_PORT = " + Util::toString(g_search_port));
#endif
		runTestUDPPort();
		start(64, "SearchManager");
	}
	catch (...)
	{
		socket.reset();
		throw;
	}
}

void SearchManager::disconnect(bool p_is_stop /*=true */)
{
	if (socket.get())
	{
		stopThread();
		m_queue_thread.shutdown();
		socket->disconnect();
		g_search_port = 0;
		
		join();
		
		socket.reset();
		
		stopThread(p_is_stop);
	}
}

#define BUFSIZE 8192
int SearchManager::run()
{
	std::unique_ptr<uint8_t[]> buf(new uint8_t[BUFSIZE]);
	int len;
	sockaddr_in remoteAddr = { 0 };
	m_queue_thread.start(0);
	while (!isShutdown())
	{
		try
		{
			while (!isShutdown())
			{
				// @todo: remove this workaround for http://bugs.winehq.org/show_bug.cgi?id=22291
				// if that's fixed by reverting to simpler while (read(...) > 0) {...} code.
				//if (socket->wait(400, Socket::WAIT_READ) != Socket::WAIT_READ)
				//{
				//  continue; // [merge] https://github.com/eiskaltdcpp/eiskaltdcpp/commit/c8dcf444d17fffacb6797d14a57b102d653896d0
				//}
				if (isShutdown() || (len = socket->read(&buf[0], BUFSIZE, remoteAddr)) <= 0)
					break;
				const boost::asio::ip::address_v4 l_ip4(ntohl(remoteAddr.sin_addr.S_un.S_addr));
#ifdef _DEBUG
				const string l_ip1 = l_ip4.to_string();
				const string l_ip2 = inet_ntoa(remoteAddr.sin_addr);
				dcassert(l_ip1 == l_ip2);
#endif
				onData(&buf[0], len, l_ip4);
			}
		}
		catch (const SocketException& e)
		{
			dcdebug("SearchManager::run Error: %s\n", e.getError().c_str());
		}
		
		bool failed = false;
		while (!isShutdown())
		{
			try
			{
				socket->disconnect();
				socket->create(Socket::TYPE_UDP);
				socket->setBlocking(true);
				socket->setInBufSize();
				dcassert(g_search_port);
				socket->bind(g_search_port, SETTING(BIND_ADDRESS));
				if (failed)
				{
					LogManager::message(STRING(SEARCH_ENABLED));
					failed = false;
				}
				break;
			}
			catch (const SocketException& e)
			{
				dcdebug("SearchManager::run Stopped listening: %s\n", e.getError().c_str());
				
				if (!failed)
				{
					LogManager::message(STRING(SEARCH_DISALED) + ": " + e.getError());
					failed = true;
				}
				
				// Spin for 60 seconds
				for (int i = 0; i < 60 && !isShutdown(); ++i)
				{
					sleep(1000);
					LogManager::message("SearchManager::run() - sleep(1000)");
				}
			}
		}
	}
	return 0;
}

int SearchManager::UdpQueue::run()
{
	string x;
	boost::asio::ip::address_v4 remoteIp;
	m_is_stop = false;
	
	while (true)
	{
		m_search_semaphore.wait();
		if (m_is_stop)
			break;
			
		{
			CFlyFastLock(m_cs);
			if (m_resultList.empty()) continue;
			
			x = m_resultList.front().first;
			remoteIp = m_resultList.front().second;
			m_resultList.pop_front();
		}
		dcassert(x.length() > 4);
		if (x.length() <= 4)
		{
			dcassert(0);
			continue;
		}
		try {
			if (x.compare(0, 4, "$SR ", 4) == 0)
			{
				string::size_type i = 4;
				string::size_type j;
				// Directories: $SR <nick><0x20><directory><0x20><free slots>/<total slots><0x05><Hubname><0x20>(<Hubip:port>)
				// Files:       $SR <nick><0x20><filename><0x05><filesize><0x20><free slots>/<total slots><0x05><Hubname><0x20>(<Hubip:port>)
				if ((j = x.find(' ', i)) == string::npos)
				{
					continue;
				}
				string nick = x.substr(i, j - i);
				i = j + 1;
				
				// A file has 2 0x05, a directory only one
				// const size_t cnt = count(x.begin() + j, x.end(), 0x05);
				// C������ ����� ������ �� 2-�. �������� ������ ������������
				const auto l_find_05_first = x.find(0x05, j);
				dcassert(l_find_05_first != string::npos);
				if (l_find_05_first == string::npos)
					continue;
				const auto l_find_05_second = x.find(0x05, l_find_05_first + 1);
				SearchResult::Types type = SearchResult::TYPE_FILE;
				string file;
				int64_t size = 0;
				
				if (l_find_05_first != string::npos && l_find_05_second == string::npos) // cnt == 1
				{
					// We have a directory...find the first space beyond the first 0x05 from the back
					// (dirs might contain spaces as well...clever protocol, eh?)
					type = SearchResult::TYPE_DIRECTORY;
					// Get past the hubname that might contain spaces
					j = l_find_05_first;
					// Find the end of the directory info
					if ((j = x.rfind(' ', j - 1)) == string::npos)
					{
						continue;
					}
					if (j < i + 1)
					{
						continue;
					}
					file = x.substr(i, j - i) + '\\';
				}
				else if (l_find_05_first != string::npos && l_find_05_second != string::npos) // cnt == 2
				{
					j = l_find_05_first;
					file = x.substr(i, j - i);
					i = j + 1;
					if ((j = x.find(' ', i)) == string::npos)
					{
						continue;
					}
					size = Util::toInt64(x.substr(i, j - i));
				}
				i = j + 1;
				
				if ((j = x.find('/', i)) == string::npos)
				{
					continue;
				}
				uint8_t freeSlots = (uint8_t)Util::toInt(x.substr(i, j - i));
				i = j + 1;
				if ((j = x.find((char)5, i)) == string::npos)
				{
					continue;
				}
				uint8_t slots = (uint8_t)Util::toInt(x.substr(i, j - i));
				i = j + 1;
				if ((j = x.rfind(" (")) == string::npos)
				{
					continue;
				}
				string l_hub_name_or_tth = x.substr(i, j - i);
				i = j + 2;
				if ((j = x.rfind(')')) == string::npos)
				{
					continue;
				}
				
				const string hubIpPort = x.substr(i, j - i);
				const string url = ClientManager::findHub(hubIpPort);
				const string l_encoding = ClientManager::findHubEncoding(url);
				nick = Text::toUtf8(nick, l_encoding);
				file = Text::toUtf8(file, l_encoding);
				const bool l_isTTH = isTTHBase64(l_hub_name_or_tth);
				if (!l_isTTH)
					l_hub_name_or_tth = Text::toUtf8(l_hub_name_or_tth, l_encoding);
					
				UserPtr user = ClientManager::findUser(nick, url);
				if (!user)
				{
					user = ClientManager::findLegacyUser(nick, url);
					if (!user)
					{
						continue;
					}
				}
				if (!remoteIp.is_unspecified())
				{
					user->setIP(remoteIp, true);
#ifdef _DEBUG
					//ClientManager::setIPUser(user, remoteIp); // TODO - ����� �� ����� ���?
#endif
					// ������� �������� �� ���� ������ - ������ ����� �������� IP � ������ ?
				}
				string tth;
				if (l_isTTH)
				{
					tth = l_hub_name_or_tth.substr(4);
				}
				if (tth.empty() && type == SearchResult::TYPE_FILE)
				{
					dcassert(tth.empty() && type == SearchResult::TYPE_FILE);
					continue;
				}
				
				const TTHValue l_tth_value(tth);
				auto sr = std::make_unique<SearchResult>(user, type, slots, freeSlots, size, file, Util::emptyString, url, remoteIp, l_tth_value, -1 /*0 == auto*/);
				COMMAND_DEBUG("[Search-result] url = " + url + " remoteIp = " + remoteIp.to_string() + " file = " + file + " user = " + user->getLastNick(), DebugTask::CLIENT_IN, remoteIp.to_string());
				SearchManager::getInstance()->fly_fire1(SearchManagerListener::SR(), sr);
#ifdef FLYLINKDC_USE_COLLECT_STAT
				CFlylinkDBManager::getInstance()->push_event_statistic("SearchManager::UdpQueue::run()", "$SR", x, remoteIp, "", url, tth);
#endif
			}
			else if (x.compare(1, 4, "RES ", 4) == 0 && x[x.length() - 1] == 0x0a)
			{
				AdcCommand c(x.substr(0, x.length() - 1));
				if (c.getParameters().empty())
					continue;
				const string cid = c.getParam(0);
				if (cid.size() != 39)
				{
					dcassert(0);
					continue;
				}
				UserPtr user = ClientManager::findUser(CID(cid));
				if (!user)
					continue;
					
				// This should be handled by AdcCommand really...
				c.getParameters().erase(c.getParameters().begin());
				
				SearchManager::getInstance()->onRES(c, user, remoteIp);
#ifdef FLYLINKDC_USE_COLLECT_STAT
				CFlylinkDBManager::getInstance()->push_event_statistic("SearchManager::UdpQueue::run()", "RES", x, remoteIp, "", "", "");
#endif
			}
			else if (x.compare(1, 4, "PSR ", 4) == 0 && x[x.length() - 1] == 0x0a)
			{
				AdcCommand c(x.substr(0, x.length() - 1));
				if (c.getParameters().empty())
					continue;
				const string cid = c.getParam(0);
				if (cid.size() != 39)
					continue;
					
				const UserPtr user = ClientManager::findUser(CID(cid));
				// when user == NULL then it is probably NMDC user, check it later
				
				if (user)
				{
					c.getParameters().erase(c.getParameters().begin());
					SearchManager::getInstance()->onPSR(c, user, remoteIp);
#ifdef FLYLINKDC_USE_COLLECT_STAT
					CFlylinkDBManager::getInstance()->push_event_statistic("SearchManager::UdpQueue::run()", "PSR", x, remoteIp, "", "", "");
#endif
				}
			}
			else if (x.compare(0, 15, "$FLY-TEST-PORT ", 15) == 0)
			{
				//dcassert(SettingsManager::g_TestUDPSearchLevel <= 1);
				const auto l_magic = x.substr(15, 39);
				if (ClientManager::getMyCID().toBase32() == l_magic)
				{
					//LogManager::message("Test UDP port - OK!");
					SettingsManager::g_TestUDPSearchLevel = CFlyServerJSON::setTestPortOK(SETTING(UDP_PORT), "udp");
					auto l_ip = x.substr(15 + 39);
					if (l_ip.size() && l_ip[l_ip.size() - 1] == '|')
					{
						l_ip = l_ip.substr(0, l_ip.size() - 1);
					}
					SettingsManager::g_UDPTestExternalIP = l_ip;
				}
				else
				{
					SettingsManager::g_TestUDPSearchLevel = false;
					CFlyServerJSON::pushError(57, "UDP Error magic value = " + l_magic);
				}
			}
			else
			{
				// ADC commands must end with \n
				if (x[x.length() - 1] != 0x0a) {
					dcassert(0);
					dcdebug("Invalid UDP data received: %s (no newline)\n", x.c_str());
					CFlyServerJSON::pushError(88, "[UDP]Invalid UDP data received: %s (no newline): ip = " + remoteIp.to_string() + " x = [" + x + "]");
					continue;
				}
				
				if (!Text::validateUtf8(x)) {
					dcassert(0);
					dcdebug("UTF-8 validation failed for received UDP data: %s\n", x.c_str());
					CFlyServerJSON::pushError(87, "[UDP]UTF-8 validation failed for received UDP data: ip = " + remoteIp.to_string() + " x = [" + x + "]");
					continue;
				}
				// TODO  respond(AdcCommand(x.substr(0, x.length()-1)));
				
			}
		}
		catch (const ParseException& e)
		{
			dcassert(0);
			CFlyServerJSON::pushError(86, "[UDP][ParseException]:" + e.getError() + " ip = " + remoteIp.to_string() + " x = [" + x + "]");
		}
		
		sleep(2);
	}
	return 0;
}

void SearchManager::onData(const std::string& p_line)
{
	m_queue_thread.addResult(p_line, boost::asio::ip::address_v4());
}

void SearchManager::onData(const uint8_t* buf, size_t aLen, const boost::asio::ip::address_v4& remoteIp)
{
	if (aLen > 4)
	{
		const string x((char*)buf, aLen);
		m_queue_thread.addResult(x, remoteIp);
	}
	else
	{
		dcassert(0);
	}
}
void SearchManager::search_auto(const string& p_tth)
{
	SearchParamOwner l_search_param;
	l_search_param.m_token = 0; /*"auto"*/
	l_search_param.m_size_mode = Search::SIZE_DONTCARE;
	l_search_param.m_file_type = Search::TYPE_TTH;
	l_search_param.m_size = 0;
	l_search_param.m_filter = p_tth;
	// ��� TTH �� ����� �����. l_search_param.normalize_whitespace();
	l_search_param.m_owner = nullptr;
	l_search_param.m_is_force_passive_searh = false;
	
	dcassert(p_tth.size() == 39);
	
	//search(l_search_param);
	ClientManager::search(l_search_param);
}

void SearchManager::onRES(const AdcCommand& cmd, const UserPtr& from, const boost::asio::ip::address_v4& p_remoteIp)
{
	int freeSlots = -1;
	int64_t size = -1;
	string file;
	string tth;
	uint32_t l_token = -1; // 0 == auto
	
	for (auto i = cmd.getParameters().cbegin(); i != cmd.getParameters().cend(); ++i)
	{
		const string& str = *i;
		if (str.compare(0, 2, "FN", 2) == 0)
		{
			file = Util::toNmdcFile(str.substr(2));
		}
		else if (str.compare(0, 2, "SL", 2) == 0)
		{
			freeSlots = Util::toInt(str.substr(2));
		}
		else if (str.compare(0, 2, "SI", 2) == 0)
		{
			size = Util::toInt64(str.substr(2));
		}
		else if (str.compare(0, 2, "TR", 2) == 0)
		{
			tth = str.substr(2);
		}
		else if (str.compare(0, 2, "TO", 2) == 0)
		{
			l_token = Util::toUInt32(str.substr(2));
			// dcassert(l_token);
		}
	}
	
	if (!file.empty() && freeSlots != -1 && size != -1)
	{
	
		/// @todo get the hub this was sent from, to be passed as a hint? (eg by using the token?)
		const StringList names = ClientManager::getHubNames(from->getCID(), Util::emptyString);
		const string hubName = names.empty() ? STRING(OFFLINE) : Util::toString(names);
		const StringList hubs = ClientManager::getHubs(from->getCID(), Util::emptyString);
		const string hub = hubs.empty() ? STRING(OFFLINE) : Util::toString(hubs);
		
		const SearchResult::Types type = (file[file.length() - 1] == '\\' ? SearchResult::TYPE_DIRECTORY : SearchResult::TYPE_FILE);
		if (type == SearchResult::TYPE_FILE && tth.empty())
			return;
			
		const uint8_t slots = ClientManager::getSlots(from->getCID());
		auto sr = std::make_unique<SearchResult>(from, type, slots, (uint8_t)freeSlots, size, file, hubName, hub, p_remoteIp, TTHValue(tth), l_token);
		fly_fire1(SearchManagerListener::SR(), sr);
	}
}

void SearchManager::onPSR(const AdcCommand& p_cmd, UserPtr from, const boost::asio::ip::address_v4& remoteIp)
{
	uint16_t udpPort = 0;
	uint32_t partialCount = 0;
	string tth;
	string hubIpPort;
	string nick;
	PartsInfo partialInfo;
	
	for (auto i = p_cmd.getParameters().cbegin(); i != p_cmd.getParameters().cend(); ++i)
	{
		const string& str = *i;
		if (str.compare(0, 2, "U4", 2) == 0)
		{
			udpPort = static_cast<uint16_t>(Util::toInt(str.substr(2)));
		}
		else if (str.compare(0, 2, "NI", 2) == 0)
		{
			nick = str.substr(2);
		}
		else if (str.compare(0, 2, "HI", 2) == 0)
		{
			hubIpPort = str.substr(2);
		}
		else if (str.compare(0, 2, "TR", 2) == 0)
		{
			tth = str.substr(2);
		}
		else if (str.compare(0, 2, "PC", 2) == 0)
		{
			partialCount = Util::toUInt32(str.substr(2)) * 2;
		}
		else if (str.compare(0, 2, "PI", 2) == 0)
		{
			const StringTokenizer<string> tok(str.substr(2), ',');
			for (auto j = tok.getTokens().cbegin(); j != tok.getTokens().cend(); ++j)
			{
				partialInfo.push_back((uint16_t)Util::toInt(*j));
			}
		}
	}
	
	
	const string url = ClientManager::findHub(hubIpPort);
	if (!from || ClientManager::isMe(from))
	{
		// for NMDC support
		
		if (nick.empty() || hubIpPort.empty())
		{
			return;
		}
		from = ClientManager::findUser(nick, url); // TODO ���������� makeCID
		if (!from)
		{
			// Could happen if hub has multiple URLs / IPs
			from = ClientManager::findLegacyUser(nick, url);
			if (!from)
			{
				dcdebug("Search result from unknown user");
				LogManager::psr_message("Error SearchManager::onPSR & ClientManager::findLegacyUser nick = " + nick + " url = " + url);
				return;
			}
			else
			{
				dcdebug("Search result from valid user");
				LogManager::psr_message("OK SearchManager::onPSR & ClientManager::findLegacyUser nick = " + nick + " url = " + url);
			}
		}
	}
	
#ifdef _DEBUG
	// ClientManager::setIPUser(from, remoteIp, udpPort);
#endif
	// TODO ���� � OnlineUser � ���� ���� ���� � UserPtr ���� ����� ��������� � ���� ����� ��� ������� IP
	
	if (partialInfo.size() != partialCount)
	{
		// what to do now ? just ignore partial search result :-/
		return;
	}
	if (!from)
	{
		return;
	}
	PartsInfo outPartialInfo;
	QueueItem::PartialSource ps(from->isNMDC() ? ClientManager::findMyNick(url) : Util::emptyString, hubIpPort, remoteIp, udpPort);
	ps.setPartialInfo(partialInfo);
	
	QueueManager::getInstance()->handlePartialResult(from, TTHValue(tth), ps, outPartialInfo);
	
	if (udpPort > 0 && !outPartialInfo.empty())
	{
		try
		{
			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			toPSR(cmd, false, ps.getMyNick(), hubIpPort, tth, outPartialInfo);
			ClientManager::send(cmd, from->getCID());
			LogManager::psr_message(
			    "[SearchManager::respond] hubIpPort = " + hubIpPort +
			    " ps.getMyNick() = " + ps.getMyNick() +
			    " tth = " + tth +
			    " outPartialInfo.size() = " + Util::toString(outPartialInfo.size())
			);
			
		}
		catch (const Exception& e)
		{
			dcdebug("Partial search caught error\n");
			LogManager::psr_message("Partial search caught error = " + e.getError());
		}
	}
	
}

ClientManagerListener::SearchReply SearchManager::respond(const AdcCommand& adc, const CID& from, bool isUdpActive, const string& hubIpPort, StringSearch::List& reguest)
{
	// Filter own searches
	if (from == ClientManager::getMyCID())
	{
		return ClientManagerListener::SEARCH_MISS;
	}
	
	const UserPtr p = ClientManager::findUser(from);
	if (!p)
	{
		return ClientManagerListener::SEARCH_MISS;
	}
	
	SearchResultList l_search_results;
	ShareManager::search_max_result(l_search_results, adc.getParameters(), isUdpActive ? 10 : 5, reguest);
	
	ClientManagerListener::SearchReply l_sr = ClientManagerListener::SEARCH_MISS;
	
	// TODO: don't send replies to passive users
	if (l_search_results.empty())
	{
		string tth;
		if (!adc.getParam("TR", 0, tth))
			return l_sr;
			
		PartsInfo partialInfo;
		if (QueueManager::handlePartialSearch(TTHValue(tth), partialInfo))
		{
			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			toPSR(cmd, true, Util::emptyString, hubIpPort, tth, partialInfo);
			ClientManager::send(cmd, from);
			l_sr = ClientManagerListener::SEARCH_PARTIAL_HIT;
			LogManager::psr_message(
			    "[SearchManager::respond] hubIpPort = " + hubIpPort +
			    " tth = " + tth +
			    " partialInfo.size() = " + Util::toString(partialInfo.size())
			);
		}
	}
	else
	{
		string l_token;
		adc.getParam("TO", 0, l_token);
		for (auto i = l_search_results.cbegin(); i != l_search_results.cend(); ++i)
		{
			AdcCommand cmd(AdcCommand::CMD_RES, AdcCommand::TYPE_UDP);
			i->toRES(cmd, AdcCommand::TYPE_UDP);
			if (!l_token.empty())
			{
				cmd.addParam("TO", l_token);
			}
			ClientManager::send(cmd, from);
		}
		l_sr = ClientManagerListener::SEARCH_HIT;
	}
	return l_sr;
}

string SearchManager::getPartsString(const PartsInfo& partsInfo)
{
	string ret;
	
	for (auto i = partsInfo.cbegin(); i < partsInfo.cend(); i += 2)
	{
		ret += Util::toString(*i) + "," + Util::toString(*(i + 1)) + ",";
	}
	
	return ret.substr(0, ret.size() - 1);
}

void SearchManager::toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo)
{
	cmd.getParameters().reserve(6);
	if (!myNick.empty())
	{
		cmd.addParam("NI", Text::utf8ToAcp(myNick));
	}
	
	cmd.addParam("HI", hubIpPort);
	cmd.addParam("U4", Util::toString(wantResponse ? getSearchPortUint() : 0)); // ���� �� ������ ������� �� ��� � ����. && ClientManager::isActive(hubIpPort)
	cmd.addParam("TR", tth);
	cmd.addParam("PC", Util::toString(partialInfo.size() / 2));
	cmd.addParam("PI", getPartsString(partialInfo));
}

/**
 * @file
 * $Id: SearchManager.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */
