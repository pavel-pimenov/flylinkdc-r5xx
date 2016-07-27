/*
 * Copyright (C) 2011-2016 FlylinkDC++ Team http://flylinkdc.com/
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

#ifdef IRAINMAN_INCLUDE_RSS

#include <wininet.h>
#include "RSSManager.h"
#include "../client/SimpleXML.h"
#include "../client/ResourceManager.h"
#include "../client/ClientManager.h"
#include "../XMLParser/xmlParser.h"
#include <boost/date_time/time_parsing.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/date_time/posix_time/ptime.hpp>


RSSManager::FeedList RSSManager::g_feeds;
RSSManager::NewsList RSSManager::g_newsList;
CriticalSection RSSManager::g_csFeed;
FastCriticalSection RSSManager::g_csNews;


RSSFeed::~RSSFeed()
{
	clearNewsList();
}

void
RSSFeed::clearNewsList()
{
	CFlyFastLock(csNews);
	for (auto i = m_newsList.cbegin(); i != m_newsList.cend(); ++i)
	{
		delete *i;
	}
	m_newsList.clear();
}

void
RSSFeed::removeCDATA(const string & str, string &tmp)
{
	if (str.empty())
		return;
	//[-] PVS-Studio V808 string tmpVal;
	string::size_type findFirst = str.find("<![CDATA[");
	string::size_type findLast = str.find_last_of("]]>");
	if (findFirst == 0 && findLast == str.size() - 1)
	{
		tmp = str.substr(9, str.size() - 3 - 9);
	}
	else
	{
		tmp = str;
	}
	/*
	    tmp.reserve(tmpVal.length());
	    string::const_iterator end = tmpVal.end();
	    for(string::const_iterator i = tmpVal.begin(); i != end; ++i) {
	        tmp += (*i);
	    }
	*/
}
bool
RSSFeed::UpdateFeedNewXML()
{
	time_t maxLastDate = 0;
	clearNewsList();
	string data;
	try
	{
		GetRSSData(feedURL, data);
		XMLParser::XMLResults xRes;
		XMLParser::XMLNode xRootNode = XMLParser::XMLNode::parseString(Text::toUtf8(data).c_str(), 0, &xRes);
		if (xRes.error == XMLParser::eXMLErrorNone)
		{
			string codeingType = codeing;
			if (codeingType.empty() && xRootNode.isAttributeSet("encoding"))
			{
				// "windows-1251" or "utf-8"
				codeingType = xRootNode.getAttributeOrDefault("encoding");
			}
			std::transform(codeingType.begin(), codeingType.end(), codeingType.begin(), ::tolower);
			bool isUtf8 = false;
			if (codeingType == "utf-8")
				isUtf8 = true;
				
			const RssFormat rssFormat = DetectRSSFormat(&xRootNode, XML_PARSER);
			
			switch (rssFormat)
			{
				case RSS_2_0:
					return ProcessRSS(&xRootNode, XML_PARSER, isUtf8);
				case RSS_ATOM:
					return ProcessAtom(&xRootNode, XML_PARSER, isUtf8);
			}
		}
		else
		{
			return UpdateFeedOldParser(data);
		}
		return false;
	}
	catch (const Exception& e)
	{
		LogManager::message(e.getError());
		return false;
	}
}

bool
RSSFeed::UpdateFeedOldParser(const string& data)
{
	clearNewsList();
	SimpleXML xml;
	try
	{
		xml.fromXML(data);
		xml.resetCurrentChild();
		const RssFormat rssFormat = DetectRSSFormat(&xml, XML_SIMPLE);
		switch (rssFormat)
		{
			case RSS_2_0:
				return ProcessRSS(&xml, XML_SIMPLE, true);
			case RSS_ATOM:
				return ProcessAtom(&xml, XML_SIMPLE, true);
		}
		
	}
	catch (SimpleXMLException& ex)
	{
		LogManager::message("Error RSSFeed::UpdateFeedOldParser " + ex.getError());
	}
	return false;
}

size_t
RSSFeed::GetRSSData(const string& url, string& data)
{
	return Util::getDataFromInet(true, url, data, 1000);
}


RSSFeed::RssFormat RSSFeed::DetectRSSFormat(void* data, RSSParser parser)
{
	if (parser == XML_PARSER)
	{
		XMLParser::XMLNode* rootNode = reinterpret_cast<XMLParser::XMLNode*>(data);
		if (rootNode != nullptr)
		{
			// Start Detection
			XMLParser::XMLNode rssNode = rootNode->getChildNode("rss");
			if (!rssNode.isEmpty())
			{
				return RSS_2_0;
			}
			XMLParser::XMLNode atomNode = rootNode->getChildNode("feed");
			if (!atomNode.isEmpty())
			{
				return RSS_ATOM;
			}
		}
	}
	else if (parser == XML_SIMPLE)
	{
		SimpleXML* xml = reinterpret_cast<SimpleXML*>(data);
		if (xml != nullptr)
		{
			if (xml->findChild("rss")) return RSS_2_0;
			if (xml->findChild("feed")) return RSS_ATOM;
		}
	}
	return RSS_2_0;
}

time_t RSSFeed::convertPubDate(const string& p_str_date) // move Util::
{
	time_t l_pubDate = {0};
	if (!p_str_date.empty())
	{
		SYSTEMTIME pTime = {0};
		
		if (!InternetTimeToSystemTimeA(p_str_date.c_str(), &pTime, 0) && !InternetTimeToSystemTimeA(("Mon, " + p_str_date).c_str(), &pTime, 0))
		{
			LogManager::message("Error InternetTimeToSystemTime p_str_date = " + p_str_date + " error = " + Util::translateError());
		}
		else
		{
			// RSS - http://news.yandex.ru/computers.rss
			// pDate - "18 Jun 2013 06:35:01 +0400"
			// pTime = {wYear=400 wMonth=6 wDayOfWeek=18 ...} BUG
			tm l_tm = {0};
			
			l_tm.tm_year = pTime.wYear - 1900;
			l_tm.tm_mon = pTime.wMonth - 1;
			l_tm.tm_mday = pTime.wDay;
			
			l_tm.tm_hour = pTime.wHour;
			l_tm.tm_min = pTime.wMinute;
			l_tm.tm_sec = pTime.wSecond;
			
			l_pubDate = mktime(&l_tm);
		}
	}
	return l_pubDate;
}

bool
RSSFeed::ProcessRSS(void* data, RSSParser parser, bool isUtf8)
{
	time_t maxLastDate = 0;
	if (parser == XML_PARSER)
	{
		XMLParser::XMLNode* rootNode = reinterpret_cast<XMLParser::XMLNode*>(data);
		if (rootNode != nullptr)
		{
			XMLParser::XMLNode rssNode = rootNode->getChildNode("rss");
			if (!rssNode.isEmpty())
			{
				XMLParser::XMLNode channelNode = rssNode.getChildNode("channel");
				if (!channelNode.isEmpty())
				{
					// Search items
					int i = 0;
					XMLParser::XMLNode itemNode = channelNode.getChildNode("item", &i);
					while (!itemNode.isEmpty())
					{
						string title;
						string url;
						string desc;
						time_t pubDate = 0;
						string author;
						string category;
						XMLParser::XMLNode titleNode = itemNode.getChildNode("title");
						if (!titleNode.isEmpty())
						{
							if (titleNode.nText())
								title = titleNode.getText();
							else if (titleNode.nClear())
								title = titleNode.getClear().lpszValue;
							if (isUtf8) title = Text::fromUtf8(title);
						}
						XMLParser::XMLNode linkNode = itemNode.getChildNode("link");
						if (!linkNode.isEmpty())
						{
							if (linkNode.nText())
								url = linkNode.getText();
							else if (linkNode.nClear())
								url = linkNode.getClear().lpszValue;
							// [!] SSA Can be problems
							if (isUtf8) url = Text::utf8ToAcp(Text::fromUtf8(url));
						}
						XMLParser::XMLNode descNode = itemNode.getChildNode("description");
						if (!descNode.isEmpty())
						{
							if (descNode.nText())
								desc = descNode.getText();
							else if (descNode.nClear())
								desc = descNode.getClear().lpszValue;
							if (isUtf8) desc = Text::fromUtf8(desc);
						}
						XMLParser::XMLNode pubDateNode = itemNode.getChildNode("pubDate");
						if (!pubDateNode.isEmpty())
						{
							string pDate;
							if (pubDateNode.nText())
								pDate = pubDateNode.getText();
							else if (pubDateNode.nClear())
								pDate = pubDateNode.getClear().lpszValue;
								
							pubDate = convertPubDate(pDate);
						}
						XMLParser::XMLNode authorNode =  itemNode.getChildNode("author");
						if (!authorNode.isEmpty())
						{
							if (authorNode.nText())
								author =  authorNode.getText();
							else if (authorNode.nClear())
								author = authorNode.getClear().lpszValue;
							if (isUtf8) author = Text::fromUtf8(author);
						}
						XMLParser::XMLNode categoryNode =  itemNode.getChildNode("category");
						if (!categoryNode.isEmpty())
						{
							if (categoryNode.nText())
								category = categoryNode.getText();
							else if (categoryNode.nClear())
								category = categoryNode.getClear().lpszValue;
							if (isUtf8) category = Text::fromUtf8(category);
						}
						
						if (pubDate == 0 || lastNewsDate < pubDate)
						{
							if (pubDate > maxLastDate)
								maxLastDate = pubDate;
								
							RSSItem* item = new RSSItem(Util::ConvertFromHTML(title), url, Util::ConvertFromHTML(desc), pubDate, author, category, source);
							CFlyFastLock(csNews);
							m_newsList.push_back(item);
						}
						
						itemNode = channelNode.getChildNode("item", &i);
					}
					if (maxLastDate > lastNewsDate)
						lastNewsDate = maxLastDate;
						
					CFlyFastLock(csNews);
					return !m_newsList.empty();
				}
			}
		}
	}
	else if (parser == XML_SIMPLE)
	{
		SimpleXML* xml = reinterpret_cast<SimpleXML*>(data);
		if (xml != nullptr)
		{
			xml->resetCurrentChild();
			
			if (xml->findChild("rss"))
			{
				xml->stepIn();
				if (xml->findChild("channel"))
				{
					xml->stepIn();
					while (xml->findChild("item"))
					{
						xml->stepIn();
						string title;
						string url;
						string desc;
						time_t pubDate = 0;
						string author;
						string category;
						if (xml->findChild("title"))
							removeCDATA(xml->getChildData(), title);
						xml->resetCurrentChild();
						if (xml->findChild("link"))
							removeCDATA(xml->getChildData(), url);
						xml->resetCurrentChild();
						if (xml->findChild("description"))
							removeCDATA(xml->getChildData(), desc);
						xml->resetCurrentChild();
						if (xml->findChild("pubDate"))
						{
							string pDate;
							removeCDATA(xml->getChildData(), pDate);
							pubDate = convertPubDate(pDate);
						}
						xml->resetCurrentChild();
						
						if (xml->findChild("author"))
							removeCDATA(xml->getChildData(), author);
						xml->resetCurrentChild();
						if (xml->findChild("category"))
							removeCDATA(xml->getChildData(), category);
						xml->resetCurrentChild();
						
						if (pubDate == 0 || lastNewsDate < pubDate)
						{
							if (pubDate > maxLastDate)
								maxLastDate = pubDate;
								
							RSSItem* item = new RSSItem(Util::ConvertFromHTML(title), url, desc, pubDate, author, category, source);
							CFlyFastLock(csNews);
							m_newsList.push_back(item);
						}
						xml->stepOut();
					}
				}
			}
			if (maxLastDate > lastNewsDate)
				lastNewsDate = maxLastDate;
				
			CFlyFastLock(csNews);
			return !m_newsList.empty();
		}
	}
	return false;
}


bool
RSSFeed::ProcessAtom(void* data, RSSParser parser, bool isUtf8)
{
	time_t maxLastDate = 0;
	if (parser == XML_PARSER)
	{
		XMLParser::XMLNode* rootNode = reinterpret_cast<XMLParser::XMLNode*>(data);
		if (rootNode != nullptr)
		{
			XMLParser::XMLNode feedNode = rootNode->getChildNode("feed");
			if (!feedNode.isEmpty())
			{
				// Search items
				int i = 0;
				XMLParser::XMLNode itemNode = feedNode.getChildNode("entry", &i);
				while (!itemNode.isEmpty())
				{
					string title;
					string url;
					string desc;
					time_t pubDate = 0;
					string author;
					string category;
					XMLParser::XMLNode titleNode = itemNode.getChildNode("title");
					if (!titleNode.isEmpty())
					{
						if (titleNode.nText())
							title = titleNode.getText();
						else if (titleNode.nClear())
							title = titleNode.getClear().lpszValue;
						if (isUtf8) title = Text::fromUtf8(title);
					}
					int j = 0;
					XMLParser::XMLNode linkNode = itemNode.getChildNode("link", &j);
					while (!linkNode.isEmpty())
					{
						if (linkNode.nAttribute() && linkNode.isAttributeSet("rel") && linkNode.isAttributeSet("href"))
						{
							string relation = linkNode.getAttribute("rel");
							if (relation == "alternate")
							{
								url = linkNode.getAttribute("href");
								// [!] SSA Can be problems
								if (isUtf8) url = Text::utf8ToAcp(Text::fromUtf8(url));
								break;
							}
						}
						linkNode = itemNode.getChildNode("link", &j);
					}
					XMLParser::XMLNode descNode = itemNode.getChildNode("content");
					if (!descNode.isEmpty())
					{
						if (descNode.nText())
							desc = descNode.getText();
						else if (descNode.nClear())
							desc = descNode.getClear().lpszValue;
						if (isUtf8) desc = Text::fromUtf8(desc);
					}
					XMLParser::XMLNode pubDateNode = itemNode.getChildNode("published");
					if (!pubDateNode.isEmpty())
					{
						string pDate;
						if (pubDateNode.nText())
							pDate = pubDateNode.getText();
						else if (pubDateNode.nClear())
							pDate = pubDateNode.getClear().lpszValue;
							
						if (pDate.length() > 0)
						{
							/*
							SYSTEMTIME pTime = {0};
							InternetTimeToSystemTimeA(pDate.c_str(), &pTime, 0);
							*/
							// Using format in atom 2012-08-20T11:57:53.000Z
							
							std::string date_string, tod_string;
							boost::date_time::split(pDate, 'T', date_string, tod_string);
							tod_string = tod_string.substr(0, 8) + ".000";
							
							boost::posix_time::time_duration td = boost::date_time::parse_delimited_time_duration<boost::posix_time::time_duration>(tod_string);
							
							tm l_tm = {0};
							///
							l_tm.tm_year = atoi(date_string.substr(0, 4).c_str()) - 1900;
							l_tm.tm_mon = atoi(date_string.substr(5, 2).c_str())  - 1;
							l_tm.tm_mday = atoi(date_string.substr(8, 2).c_str());
							
							l_tm.tm_hour = td.hours();
							l_tm.tm_min = td.minutes();
							l_tm.tm_sec = td.seconds();
							
							pubDate = mktime(&l_tm);
							
						}
					}
					XMLParser::XMLNode authorNode =  itemNode.getChildNode("author");
					if (!authorNode.isEmpty())
					{
						XMLParser::XMLNode authorName = authorNode.getChildNode("name");
						if (!authorName.isEmpty())
						{
							if (authorName.nText())
								author =  authorName.getText();
							else if (authorName.nClear())
								author = authorName.getClear().lpszValue;
							if (isUtf8) author = Text::fromUtf8(author);
						}
					}
					int k = 0;
					XMLParser::XMLNode categoryNode =  itemNode.getChildNode("category", &k);
					while (!categoryNode.isEmpty())
					{
						if (categoryNode.nAttribute())
						{
							if (categoryNode.isAttributeSet("term"))
							{
								if (!category.empty())
									category += ",";
								category += categoryNode.getAttribute("term");
							}
						}
						categoryNode =  itemNode.getChildNode("category", &k);
					}
					if (isUtf8) category = Text::fromUtf8(category);
					
					if (pubDate == 0 || lastNewsDate < pubDate)
					{
						if (pubDate > maxLastDate)
							maxLastDate = pubDate;
							
						RSSItem* item = new RSSItem(Util::ConvertFromHTML(title), url, Util::ConvertFromHTML(desc), pubDate, author, category, source);
						CFlyFastLock(csNews);
						m_newsList.push_back(item);
					}
					
					itemNode = feedNode.getChildNode("entry", &i);
				}
				if (maxLastDate > lastNewsDate)
					lastNewsDate = maxLastDate;
					
				CFlyFastLock(csNews);
				return !m_newsList.empty();
			}
		}
	}
	else if (parser == XML_SIMPLE)
	{
		// [!] SSA - todo if needed?
		SimpleXML* xml = reinterpret_cast<SimpleXML*>(data);
		if (xml != nullptr)
		{
		}
	}
	return false;
}

StringList RSSManager::g_codeingList;

// Class RSSManager
RSSManager::RSSManager(void)
	: m_minuteCounter(0)
{
	g_codeingList.push_back(Util::emptyString);
	g_codeingList.push_back("utf-8");
	g_codeingList.push_back("windows-1251");
	TimerManager::getInstance()->addListener(this);
}

RSSManager::~RSSManager(void)
{
	TimerManager::getInstance()->removeListener(this);
	waitShutdown();
	{
		CFlyFastLock(g_csNews);
		for (auto j = g_newsList.cbegin(); j != g_newsList.cend(); ++j)
		{
			delete *j;
		}
	}
	{
		CFlyLock(g_csFeed);
		for (auto i = g_feeds.cbegin(); i != g_feeds.cend(); ++i)
		{
			delete *i;
		}
	}
}

void
RSSManager::updateAllFeeds()
{
	unsigned int iNewNews = 0;
	NewsList l_fire_added_array;
	{
		CFlyLock(g_csFeed); // [+] IRainman fix.
		for (auto i = g_feeds.cbegin(); i != g_feeds.cend(); ++i)
		{
			if ((*i)->UpdateFeedNewXML())
			{
				const RSSFeed::RSSItemList& list = (*i)->getNewsList();
				for (auto j = list.cbegin(); j != list.cend(); ++j)
				{
					const RSSItem *l_item = new RSSItem(*j);
					CFlyFastLock(g_csNews);
					if (canAdd(l_item))
					{
						g_newsList.push_back(l_item);
						l_fire_added_array.push_back(l_item);
					}
					else
					{
						safe_delete(l_item);
					}
				}
				iNewNews += list.size();
			}
		}
	}
	for (auto i = l_fire_added_array.begin(); i !=  l_fire_added_array.end(); ++i)
	{
		fly_fire1(RSSListener::Added(), *i);
	}
	
	if (iNewNews)
	{
		fly_fire1(RSSListener::NewRSS(), iNewNews);
	}
}

bool
RSSManager::canAdd(const RSSItem* p_item)
{
	// [!] IRainman: this function call needs to lock externals.
	if (g_newsList.empty())
		return true;
		
	for (size_t i = 0; i < g_newsList.size(); i++)
	{
		if (const RSSItem *l_iteminList = g_newsList[i])
		{
			if (l_iteminList->getUrl().compare(p_item->getUrl()) == 0)
				return false;
		}
	}
	
	return true;
}
void
RSSManager::fail(const string& p_error)
{
	dcdebug("RSSManager: New command when already failed: %s\n", p_error.c_str());
	//LogManager::message("RSSManager: " + p_error);
	fly_fire1(RSSListener::Failed(), p_error);
}

void
RSSManager::execute(const RSSManagerTasks& p_task)
{
	dcassert(p_task == CHECK_NEWS);
	updateAllFeeds();
}

bool RSSManager::hasRSSFeed(const string & url, const string & name)
{
	// [!] IRainman: this function call needs to lock externals.
	for (auto i = g_feeds.cbegin(); i != g_feeds.cend(); ++i)
	{
		if (!stricmp((*i)->getFeedURL(), url) || !stricmp((*i)->getSource(), name))
			return true;
	}
	return false;
}

void RSSManager::load(SimpleXML& aXml)
{
	{
		CFlyLock(g_csFeed);
		aXml.resetCurrentChild();
		if (aXml.findChild("Rss"))
		{
			aXml.stepIn();
			while (aXml.findChild("Feed"))
			{
				const string& realURL = aXml.getChildData();
				if (realURL.empty())
				{
					continue;
				}
				const string& sourceName = aXml.getChildAttrib("Name");
				const string& codeingType = aXml.getChildAttrib("Codeing");
				
				const size_t iCodeingType = Util::toInt(codeingType);
				
				// add only unique RSS feeds
				addNewFeed(realURL, sourceName, getCodeing(iCodeingType));
			}
			aXml.stepOut();
		}
	}
	updateFeeds();
}

RSSFeed*
RSSManager::addNewFeed(const string& url, const string& name, const string& codeing, bool bUpdateFeeds/* = false */)
{
	CFlyLock(g_csFeed); // [+] IRainman fix.
	if (!hasRSSFeed(url, name))
	{
		RSSFeed* rFeed = new RSSFeed(url, name, codeing);
		g_feeds.push_back(rFeed);
		if (bUpdateFeeds)
		{
			updateFeeds();
		}
		
		return rFeed;
	}
	return nullptr;
}
bool
RSSManager::removeFeedAt(size_t pos)
{
	bool bRes = false;
	CFlyLock(g_csFeed); // [+] IRainman fix.
	if (pos < g_feeds.size())
	{
		RSSFeed* feed = g_feeds.at(pos);
		if (feed)
		{
			auto i = g_feeds.cbegin();
			for (; i != g_feeds.cend(); ++i)
			{
				if (*i == feed)
				{
					break;
				}
			}
			if (i != g_feeds.cend())
			{
				g_feeds.erase(i);
				bRes = true;
			}
		}
	}
	return bRes;
}


void RSSManager::save(SimpleXML& aXml)
{
	CFlyLock(g_csFeed);
	aXml.addTag("Rss");
	aXml.stepIn();
	for (auto i = g_feeds.cbegin(); i != g_feeds.cend(); ++i)
	{
		aXml.addTag("Feed", (*i)->getFeedURL());
		aXml.addChildAttrib("Name", (*i)->getSource());
		const size_t codeingT = GetCodeingByString((*i)->getCodeing());
		aXml.addChildAttrib("Codeing", Util::toString(codeingT));
	}
	aXml.stepOut();
}
void RSSManager::on(TimerManagerListener::Minute, uint64_t tick) noexcept
{
	if (SETTING(RSS_AUTO_REFRESH_TIME) > 0)
	{
		if (++m_minuteCounter >= (unsigned int) SETTING(RSS_AUTO_REFRESH_TIME))
		{
			updateFeeds(); // [!] IRainman fix done [2] https://www.box.net/shared/be613d6f54c533c0e1ff
			m_minuteCounter = 0;
		}
	}
}
const string RSSManager::getCodeing(const size_t i)
{
	if (i < g_codeingList.size())
	{
		return g_codeingList[i];
	}
	if (!g_codeingList.empty())
		return g_codeingList[0];
	else
		return Util::emptyString;
}

size_t RSSManager::GetCodeingByString(const string& codeing)
{
	string codeingType = codeing;
	std::transform(codeingType.begin(), codeingType.end(), codeingType.begin(), ::tolower);
	for (size_t i = 0; i < g_codeingList.size(); i++)
	{
		if (codeingType == g_codeingList[i])
			return i;
	}
	return 0;
}
void RSSManager::updateFeeds()
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		{
			CFlyLock(g_csFeed);
			if (g_feeds.empty()) // [+] IRainman fix.
				return;
		}
		if (RSSManager::isValidInstance())
		{
			RSSManager::getInstance()->addTask(CHECK_NEWS);
		}
	}
}

#endif // IRAINMAN_INCLUDE_RSS