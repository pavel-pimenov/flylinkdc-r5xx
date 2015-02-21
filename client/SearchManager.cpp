/*
 * Copyright (C) 2001-2013 Jacek Sieka, arnetheduck on gmail point com
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
#include "SearchResult.h"
#include "QueueManager.h"
#include "StringTokenizer.h"
#include "FinishedManager.h"
#include "../FlyFeatures/flyServer.h"


uint16_t SearchManager::g_search_port = 0;

const char* SearchManager::getTypeStr(int type)
{
	static const char* g_types[Search::TYPE_LAST] =
	{
		CSTRING(ANY),
		CSTRING(AUDIO),
		CSTRING(COMPRESSED),
		CSTRING(DOCUMENT),
		CSTRING(EXECUTABLE),
		CSTRING(PICTURE),
		CSTRING(VIDEO_AND_SUBTITLES),
		CSTRING(DIRECTORY),
		"TTH"
	};
	return g_types[type];
}

SearchManager::SearchManager() :
	m_stop(false)
{

}

SearchManager::~SearchManager()
{
	if (socket.get())
	{
		m_stop = true;
		socket->disconnect();
#ifdef _WIN32
		join();
#endif
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
		
		start(64, "SearchManager");
	}
	catch (...)
	{
		socket.reset();
		throw;
	}
}

void SearchManager::disconnect()
{
	if (socket.get())
	{
		m_stop = true;
		m_queue_thread.shutdown();
		socket->disconnect();
		g_search_port = 0;
		
		join();
		
		socket.reset();
		
		m_stop = false;
	}
}

#define BUFSIZE 8192
int SearchManager::run()
{
	std::unique_ptr<uint8_t[]> buf(new uint8_t[BUFSIZE]);
	int len;
	sockaddr_in remoteAddr = { 0 };
	m_queue_thread.start(0);
	while (!m_stop)
	{
		try
		{
			while (!m_stop)
			{
				// @todo: remove this workaround for http://bugs.winehq.org/show_bug.cgi?id=22291
				// if that's fixed by reverting to simpler while (read(...) > 0) {...} code.
				if (socket->wait(400, Socket::WAIT_READ) != Socket::WAIT_READ)
				{
					continue; // [merge] https://github.com/eiskaltdcpp/eiskaltdcpp/commit/c8dcf444d17fffacb6797d14a57b102d653896d0
				}
				if (m_stop || (len = socket->read(&buf[0], BUFSIZE, remoteAddr)) <= 0)
					break;
				onData(&buf[0], len, inet_ntoa(remoteAddr.sin_addr));
			}
		}
		catch (const SocketException& e)
		{
			dcdebug("SearchManager::run Error: %s\n", e.getError().c_str());
		}
		
		bool failed = false;
		while (!m_stop)
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
				for (int i = 0; i < 60 && !m_stop; ++i)
				{
					sleep(1000);
				}
			}
		}
	}
	return 0;
}

int SearchManager::UdpQueue::run()
{
	string x;
	string remoteIp;
	stop = false;
	
	while (true)
	{
		m_s.wait();
		if (stop)
			break;
			
		{
			FastLock l(cs);
			if (resultList.empty()) continue;
			
			x = resultList.front().first;
			remoteIp = resultList.front().second;
			resultList.pop_front();
		}
		
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
			// Cчитать можно только до 2-х. значения больше игнорируются
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
			const string url = ClientManager::findHub(hubIpPort); // TODO - внутри линейный поиск. оптимизнуть
			// Иногда вместо IP приходит домен "$SR chen video\multfilm\Ну, погоди!\Ну, погоди! 2.avi33492992 5/5TTH:B4O5M74UPKZ7I23CH36NA3SZOUZTJLWNVEIJMTQ (dc.a-galaxy.com:411)|"
			// http://code.google.com/p/flylinkdc/issues/detail?id=1443
			// Это не обрабатывается в функции - поправить.
			// для dc.dly-server.ru - возвращается его IP-шник "31.186.103.125:411"
			// url оказывается пустым https://www.box.net/shared/ayirspvdjk2boix4oetr
			// падаем на dcassert в следующем вызове findHubEncoding.
			// [!] IRainman fix: не падаем!!!! Это диагностическое предупреждение!!!
			// [-] string encoding;
			// [-] if (!url.empty())
			const string l_encoding = ClientManager::findHubEncoding(url); // [!]
			// [~]
			nick = Text::toUtf8(nick, l_encoding);
			file = Text::toUtf8(file, l_encoding);
			const bool l_isTTH = isTTHBase64(l_hub_name_or_tth);
			if (!l_isTTH) // [+]FlylinkDC++ Team
				l_hub_name_or_tth = Text::toUtf8(l_hub_name_or_tth, l_encoding);
				
			UserPtr user = ClientManager::findUser(nick, url); // TODO оптимизнуть makeCID
			// не находим юзера "$SR snooper-06 Фильмы\Прошлой ночью в Нью-Йорке.avi1565253632 15/15TTH:LUWOOXBE2H77TUV4S4HNZQTVDXLPEYC757OUMLY (31.186.103.125:411)"
			// при пустом url - можно не звать ClientManager::findUser - не найдем.
			// сразу нужно переходить на ClientManager::findLegacyUser
			// url не педедается при коннекте к хабу через SOCKS5
			// TODO - если хаб только один - пытаться подставлять его?
			if (!user)
			{
				// LogManager::message("Error ClientManager::findUser nick = " + nick + " url = " + url);
				// Could happen if hub has multiple URLs / IPs
				user = ClientManager::findLegacyUser(nick
#ifndef IRAINMAN_USE_NICKS_IN_CM
				                                     , url
#endif
				                                    );
				if (!user)
				{
					//LogManager::message("Error ClientManager::findLegacyUser nick = " + nick + " url = " + url);
					continue;
				}
			}
			if (!remoteIp.empty())
			{
				user->setIP(remoteIp);
#ifdef _DEBUG
				//ClientManager::setIPUser(user, remoteIp); // TODO - может не нужно тут?
#endif
				// Тяжелая операция по мапе юзеров - только чтобы показать IP в списке ?
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
			const SearchResultPtr sr(new SearchResult(user, type, slots, freeSlots, size,
			                                          file, Util::emptyString, url, remoteIp, l_tth_value, -1 /*0 == auto*/));
			                                          
			SearchManager::getInstance()->fire(SearchManagerListener::SR(), sr);
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
		} // Тут нужен else?
		if (x.compare(1, 4, "PSR ", 4) == 0 && x[x.length() - 1] == 0x0a)
		{
			AdcCommand c(x.substr(0, x.length() - 1));
			if (c.getParameters().empty())
				continue;
			string cid = c.getParam(0);
			if (cid.size() != 39)
				continue;
				
			UserPtr user = ClientManager::findUser(CID(cid));
			// when user == NULL then it is probably NMDC user, check it later
			
			c.getParameters().erase(c.getParameters().begin());
			
			SearchManager::getInstance()->onPSR(c, user, remoteIp);
#ifdef FLYLINKDC_USE_COLLECT_STAT
			CFlylinkDBManager::getInstance()->push_event_statistic("SearchManager::UdpQueue::run()", "PSR", x, remoteIp, "", "", "");
#endif
		}
		else if (x.compare(0, 15, "$FLY-TEST-PORT ", 15) == 0)
		{
			//dcassert(SettingsManager::g_TestUDPSearchLevel <= 1);
			const auto l_magic = x.substr(15, 39);
			if (ClientManager::getMyCID().toBase32() == l_magic)
			{
				SettingsManager::g_TestUDPSearchLevel = true;
				auto l_ip = x.substr(15 + 39);
				if (l_ip.size() && l_ip[l_ip.size() - 1] == '|')
					l_ip = l_ip.substr(0, l_ip.size() - 1);
				SearchManager::getInstance()->fire(SearchManagerListener::UDPTest(), l_ip);
			}
			else
			{
				SettingsManager::g_TestUDPSearchLevel = false;
				LogManager::message("Error magic value = " + l_magic);
			}
		}
		/*else if(x.compare(1, 4, "SCH ",4) == 0 && x[x.length() - 1] == 0x0a) {
		    try {
		        respond(AdcCommand(x.substr(0, x.length()-1)));
		    } catch(const ParseException& ) {
		    }
		}*/ // Needs further DoS investigation
		
		
		sleep(10);
	}
	return 0;
}

void SearchManager::onData(const uint8_t* buf, size_t aLen, const string& remoteIp)
{
	string x((char*)buf, aLen);
	m_queue_thread.addResult(x, remoteIp);
}

void SearchManager::onRES(const AdcCommand& cmd, const UserPtr& from, const string& remoteIp)
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
		const StringList& names = ClientManager::getHubNames(from->getCID(), Util::emptyString);
		const string& hubName = names.empty() ? STRING(OFFLINE) : Util::toString(names);
		const StringList& hubs = ClientManager::getHubs(from->getCID(), Util::emptyString);
		const string& hub = hubs.empty() ? STRING(OFFLINE) : Util::toString(hubs);
		
		const SearchResult::Types type = (file[file.length() - 1] == '\\' ? SearchResult::TYPE_DIRECTORY : SearchResult::TYPE_FILE);
		if (type == SearchResult::TYPE_FILE && tth.empty())
			return;
			
		const uint8_t slots = ClientManager::getInstance()->getSlots(from->getCID());
		const SearchResultPtr sr(new SearchResult(from, type, slots, (uint8_t)freeSlots, size,
		                                          file, hubName, hub, remoteIp, TTHValue(tth), l_token));
		fire(SearchManagerListener::SR(), sr);
	}
}

void SearchManager::onPSR(const AdcCommand& cmd, UserPtr from, const string& remoteIp)
{
	uint16_t udpPort = 0;
	uint32_t partialCount = 0;
	string tth;
	string hubIpPort;
	string nick;
	PartsInfo partialInfo;
	
	for (auto i = cmd.getParameters().cbegin(); i != cmd.getParameters().cend(); ++i)
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
		from = ClientManager::findUser(nick, url); // TODO оптмизнуть makeCID
		if (!from)
		{
			// Could happen if hub has multiple URLs / IPs
			from = ClientManager::findLegacyUser(nick
#ifndef IRAINMAN_USE_NICKS_IN_CM
			                                     , url
#endif
			                                    );
			if (!from)
			{
				dcdebug("Search result from unknown user");
				LogManager::message("Error SearchManager::onPSR & ClientManager::findLegacyUser nick = " + nick + " url = " + url);
				return;
			}
		}
	}
	
#ifdef _DEBUG
	// ClientManager::setIPUser(from, remoteIp, udpPort);
#endif
	// TODO Ищем в OnlineUser а чуть выше ищем в UserPtr може тожно схлопнуть в один поиск для апдейта IP
	
	if (partialInfo.size() != partialCount)
	{
		// what to do now ? just ignore partial search result :-/
		return;
	}
	
	PartsInfo outPartialInfo;
	QueueItem::PartialSource ps(from->isNMDC() ? ClientManager::getInstance()->getMyNick(url) : Util::emptyString, hubIpPort, remoteIp, udpPort);
	ps.setPartialInfo(partialInfo);
	
	QueueManager::getInstance()->handlePartialResult(from, TTHValue(tth), ps, outPartialInfo);
	
	if (udpPort > 0 && !outPartialInfo.empty())
	{
		try
		{
			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			toPSR(cmd, false, ps.getMyNick(), hubIpPort, tth, outPartialInfo);
			ClientManager::getInstance()->send(cmd, from->getCID());
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

ClientManagerListener::SearchReply SearchManager::respond(const AdcCommand& adc, const CID& from, bool isUdpActive, const string& hubIpPort, StringSearch::List& reguest) // [!] IRainman-S
{
	// Filter own searches
	if (from == ClientManager::getMyCID()) // [!] IRainman fix.
		return ClientManagerListener::SEARCH_MISS; // [!] IRainman-S
		
	UserPtr p = ClientManager::findUser(from);
	if (!p)
		return ClientManagerListener::SEARCH_MISS; // [!] IRainman-S
		
	SearchResultList results;
	ShareManager::getInstance()->search(results, adc.getParameters(), isUdpActive ? 10 : 5, reguest); // [!] IRainman-S
	
	string token;
	
	adc.getParam("TO", 0, token);
	
	ClientManagerListener::SearchReply l_sr = ClientManagerListener::SEARCH_MISS; // [+] IRainman-S
	
	// TODO: don't send replies to passive users
	if (results.empty())
	{
		string tth;
		if (!adc.getParam("TR", 0, tth))
			return l_sr; // [!] IRainman-S
			
		PartsInfo partialInfo;
		if (QueueManager::getInstance()->handlePartialSearch(TTHValue(tth), partialInfo))
		{
			AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
			toPSR(cmd, true, Util::emptyString, hubIpPort, tth, partialInfo);
			ClientManager::getInstance()->send(cmd, from);
			l_sr = ClientManagerListener::SEARCH_PARTIAL_HIT; // [+] IRainman-S
			LogManager::psr_message(
			    "[SearchManager::respond] hubIpPort = " + hubIpPort +
			    " tth = " + tth +
			    " partialInfo.size() = " + Util::toString(partialInfo.size())
			);
		}
	}
	else
	{
		for (auto i = results.cbegin(); i != results.cend(); ++i)
		{
			AdcCommand cmd(AdcCommand::CMD_RES, AdcCommand::TYPE_UDP);
			(*i)->toRES(cmd, AdcCommand::TYPE_UDP);
			if (!token.empty())
				cmd.addParam("TO", token);
			ClientManager::getInstance()->send(cmd, from);
		}
		l_sr = ClientManagerListener::SEARCH_HIT; // [+] IRainman-S
	}
	return l_sr; // [+] IRainman-S
}
/*
string SearchManager::clean(const string& aSearchString)
{
    static const char* badChars = "$|.[]()-_+";
    string::size_type i = aSearchString.find_first_of(badChars);
    if (i == string::npos)
        return aSearchString;

    string tmp = aSearchString;
    // Remove all strange characters from the search string
    do
    {
        tmp[i] = ' ';
    }
    while ((i = tmp.find_first_of(badChars, i)) != string::npos);

    return tmp;
}
*/
string SearchManager::getPartsString(const PartsInfo& partsInfo) const
{
	string ret;
	
	for (auto i = partsInfo.cbegin(); i < partsInfo.cend(); i += 2)
	{
		ret += Util::toString(*i) + "," + Util::toString(*(i + 1)) + ",";
	}
	
	return ret.substr(0, ret.size() - 1);
}


void SearchManager::toPSR(AdcCommand& cmd, bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo) const
{
	cmd.getParameters().reserve(6);
	if (!myNick.empty())
		cmd.addParam("NI", Text::utf8ToAcp(myNick));
		
	cmd.addParam("HI", hubIpPort);
	cmd.addParam("U4", Util::toString(wantResponse ? getSearchPortUint() : 0)); // Сюда по ошибке подався не урл к хабу. && ClientManager::isActive(hubIpPort)
	cmd.addParam("TR", tth);
	cmd.addParam("PC", Util::toString(partialInfo.size() / 2));
	cmd.addParam("PI", getPartsString(partialInfo));
}

/**
 * @file
 * $Id: SearchManager.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */
