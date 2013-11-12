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

const char* SearchManager::getTypeStr(int type)
{
	static const char* types[TYPE_LAST] =
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
	return types[type];
}

SearchManager::SearchManager() :
	port(0),
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

string SearchManager::normalizeWhitespace(const string& aString)
{
	string::size_type found = 0;
	string normalized = aString;
	while ((found = normalized.find_first_of("\t\n\r", found)) != string::npos)
	{
		normalized[found] = ' ';
		found++;
	}
	return normalized;
}

void SearchManager::search(const string& aName, int64_t aSize, TypeModes aTypeMode /* = TYPE_ANY */, Search::SizeModes aSizeMode /* = SIZE_ATLEAST */, const string& aToken /* = Util::emptyString */, void* aOwner /* = NULL */)
{
	ClientManager::getInstance()->search(aSizeMode, aSize, aTypeMode, normalizeWhitespace(aName), aToken, aOwner);
}

uint64_t SearchManager::search(const StringList& who, const string& aName, int64_t aSize /* = 0 */, TypeModes aTypeMode /* = TYPE_ANY */, Search::SizeModes aSizeMode /* = SIZE_ATLEAST */, const string& aToken /* = Util::emptyString */, const StringList& aExtList, void* aOwner /* = NULL */)
{
	return ClientManager::getInstance()->search(who, aSizeMode, aSize, aTypeMode, normalizeWhitespace(aName), aToken, aExtList, aOwner);
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
			port = socket->bind(0, Util::emptyString);
		}
		else
		{
			port = socket->bind(static_cast<uint16_t>(SETTING(UDP_PORT)), SETTING(BIND_ADDRESS));
		}
		
		start(64);
	}
	catch (...)
	{
		socket.reset();
		throw;
	}
}

void SearchManager::disconnect() noexcept
{
	if (socket.get())
	{
		m_stop = true;
		m_queue.shutdown();
		socket->disconnect();
		port = 0;
		
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
	m_queue.start(0);
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
				socket->bind(port, SETTING(BIND_ADDRESS));
				if (failed)
				{
					LogManager::getInstance()->message(STRING(SEARCH_ENABLED));
					failed = false;
				}
				break;
			}
			catch (const SocketException& e)
			{
				dcdebug("SearchManager::run Stopped listening: %s\n", e.getError().c_str());
				
				if (!failed)
				{
					LogManager::getInstance()->message(STRING(SEARCH_DISALED) + ": " + e.getError());
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
			// Cчитать можно только до 2-х. значени€ больше игнорируютс€
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
			const string url = ClientManager::getInstance()->findHub(hubIpPort); // TODO - внутри линейный поиск. оптимизнуть
			// url оказываетс€ пустым https://www.box.net/shared/ayirspvdjk2boix4oetr
			// падаем на dcassert в следующем вызове findHubEncoding.
			// [!] IRainman fix: не падаем!!!! Ёто диагностическое предупреждение!!!
			// [-] string encoding;
			// [-] if (!url.empty())
			const string encoding = ClientManager::getInstance()->findHubEncoding(url); // [!]
			// [~]
			nick = Text::toUtf8(nick, encoding);
			file = Text::toUtf8(file, encoding);
			const bool l_isTTH = isTTHBase64(l_hub_name_or_tth);
			if (!l_isTTH) // [+]FlylinkDC++ Team
				l_hub_name_or_tth = Text::toUtf8(l_hub_name_or_tth, encoding);
				
			UserPtr user = ClientManager::getInstance()->findUser(nick, url); // TODO оптмизнуть makeCID
			if (!user)
			{
				// Could happen if hub has multiple URLs / IPs
				user = ClientManager::getInstance()->findLegacyUser(nick
#ifndef IRAINMAN_USE_NICKS_IN_CM
				                                                    , url
#endif
				                                                   );
				if (!user)
				
					continue;
			}
			if (!remoteIp.empty())
				user->setIP(remoteIp);
				
			ClientManager::getInstance()->setIPUser(user, remoteIp);
			
			string tth;
			if (l_isTTH)
			{
				tth = l_hub_name_or_tth.substr(4);
				StringList names = ClientManager::getInstance()->getHubNames(user->getCID(), Util::emptyString);
				l_hub_name_or_tth = names.empty() ? STRING(OFFLINE) : Util::toString(names);
			}
			
			if (tth.empty() && type == SearchResult::TYPE_FILE)
			{
				continue;
			}
			
			const TTHValue l_tth_value(tth);
			const SearchResultPtr sr(new SearchResult(user, type, slots, freeSlots, size,
			                                          file, l_hub_name_or_tth, url, remoteIp, l_tth_value, Util::emptyString));
			                                          
			SearchManager::getInstance()->fire(SearchManagerListener::SR(), sr);
			
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
			
			UserPtr user = ClientManager::getInstance()->findUser(CID(cid));
			if (!user)
				continue;
				
			// This should be handled by AdcCommand really...
			c.getParameters().erase(c.getParameters().begin());
			
			SearchManager::getInstance()->onRES(c, user, remoteIp);
			
		}
		if (x.compare(1, 4, "PSR ", 4) == 0 && x[x.length() - 1] == 0x0a)
		{
			AdcCommand c(x.substr(0, x.length() - 1));
			if (c.getParameters().empty())
				continue;
			string cid = c.getParam(0);
			if (cid.size() != 39)
				continue;
				
			UserPtr user = ClientManager::getInstance()->findUser(CID(cid));
			// when user == NULL then it is probably NMDC user, check it later
			
			c.getParameters().erase(c.getParameters().begin());
			
			SearchManager::getInstance()->onPSR(c, user, remoteIp);
			
		} /*else if(x.compare(1, 4, "SCH ",4) == 0 && x[x.length() - 1] == 0x0a) {
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
	m_queue.addResult(x, remoteIp);
}

void SearchManager::onRES(const AdcCommand& cmd, const UserPtr& from, const string& remoteIp)
{
	int freeSlots = -1;
	int64_t size = -1;
	string file;
	string tth;
	string token;
	
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
			token = str.substr(2);
		}
	}
	
	if (!file.empty() && freeSlots != -1 && size != -1)
	{
	
		/// @todo get the hub this was sent from, to be passed as a hint? (eg by using the token?)
		const StringList& names = ClientManager::getInstance()->getHubNames(from->getCID(), Util::emptyString);
		const string& hubName = names.empty() ? STRING(OFFLINE) : Util::toString(names);
		const StringList& hubs = ClientManager::getInstance()->getHubs(from->getCID(), Util::emptyString);
		const string& hub = hubs.empty() ? STRING(OFFLINE) : Util::toString(hubs);
		
		const SearchResult::Types type = (file[file.length() - 1] == '\\' ? SearchResult::TYPE_DIRECTORY : SearchResult::TYPE_FILE);
		if (type == SearchResult::TYPE_FILE && tth.empty())
			return;
			
		const uint8_t slots = ClientManager::getInstance()->getSlots(from->getCID());
		const SearchResultPtr sr(new SearchResult(from, type, slots, (uint8_t)freeSlots, size,
		                                          file, hubName, hub, remoteIp, TTHValue(tth), token));
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
	
	
	const string url = ClientManager::getInstance()->findHub(hubIpPort);
	if (!from || ClientManager::isMe(from))
	{
		// for NMDC support
		
		if (nick.empty() || hubIpPort.empty())
		{
			return;
		}
		from = ClientManager::getInstance()->findUser(nick, url); // TODO оптмизнуть makeCID
		if (!from)
		{
			// Could happen if hub has multiple URLs / IPs
			from = ClientManager::getInstance()->findLegacyUser(nick
#ifndef IRAINMAN_USE_NICKS_IN_CM
			                                                    , url
#endif
			                                                   );
			if (!from)
			{
				dcdebug("Search result from unknown user");
				return;
			}
		}
	}
	
	ClientManager::getInstance()->setIPUser(from, remoteIp, udpPort);
	// TODO »щем в OnlineUser а чуть выше ищем в UserPtr може тожно схлопнуть в один поиск дл€ апдейта IP
	
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
			AdcCommand l_cmd = SearchManager::getInstance()->toPSR(false, ps.getMyNick(), hubIpPort, tth, outPartialInfo);
			ClientManager::getInstance()->send(l_cmd, from->getCID());
		}
		catch (...)
		{
			dcdebug("Partial search caught error\n");
			// TODO log
		}
	}
	
}

ClientManagerListener::SearchReply SearchManager::respond(const AdcCommand& adc, const CID& from, bool isUdpActive, const string& hubIpPort, StringSearch::List& reguest) // [!] IRainman-S
{
	// Filter own searches
	if (from == ClientManager::getMyCID()) // [!] IRainman fix.
		return ClientManagerListener::SEARCH_MISS; // [!] IRainman-S
		
	UserPtr p = ClientManager::getInstance()->findUser(from);
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
			AdcCommand cmd = toPSR(true, Util::emptyString, hubIpPort, tth, partialInfo);
			ClientManager::getInstance()->send(cmd, from);
			l_sr = ClientManagerListener::SEARCH_PARTIAL_HIT; // [+] IRainman-S
		}
	}
	else
	{
		for (auto i = results.cbegin(); i != results.cend(); ++i)
		{
			AdcCommand cmd = (*i)->toRES(AdcCommand::TYPE_UDP);
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


AdcCommand SearchManager::toPSR(bool wantResponse, const string& myNick, const string& hubIpPort, const string& tth, const vector<uint16_t>& partialInfo) const
{
	AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
	
	if (!myNick.empty())
		cmd.addParam("NI", Text::utf8ToAcp(myNick));
		
	cmd.addParam("HI", hubIpPort);
	cmd.addParam("U4", Util::toString(wantResponse ? getPort() : 0)); // —юда по ошибке подавс€ не урл к хабу. && ClientManager::getInstance()->isActive(hubIpPort)
	cmd.addParam("TR", tth);
	cmd.addParam("PC", Util::toString(partialInfo.size() / 2));
	cmd.addParam("PI", getPartsString(partialInfo));
	
	return cmd;
}

/**
 * @file
 * $Id: SearchManager.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */
