/*
 * Copyright (C) 2011-2015 FlylinkDC++ Team http://flylinkdc.com/
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

#ifdef IRAINMAN_INCLUDE_RSS

#if !defined(RSS_MANAGER_H)
#define RSS_MANAGER_H

#include "../client/Util.h"
#include "../client/Singleton.h"
#include "../client/Speaker.h"
#include "../client/Semaphore.h"
#include "../client/TimerManager.h"
#include "../client/SettingsManager.h"
#include "../client/LogManager.h"

class RSSItem
{
		typedef RSSItem* Ptr;
	public:
		RSSItem(const string& aTitle, const string& aUrl, const string& aDesc, time_t aPublishDate,
		        const string& aAuthor, const string& aCategory, const string& aSource) :
			title(aTitle), url(aUrl), desc(aDesc), publishDate(aPublishDate), author(aAuthor), category(aCategory), source(aSource)
		{
		}
		
		RSSItem(const RSSItem* src):
			title(src->getTitle()), url(src->getUrl()), desc(src->getDesc()), publishDate(src->getPublishDate()), author(src->getAuthor()), category(src->getCategory()), source(src->getSource())
		{
		
		}
		
		GETSET(string, title, Title);
		GETSET(string, url, Url);
		GETSET(string, desc, Desc);
		GETSET(time_t, publishDate, PublishDate);
		GETSET(string, author, Author);
		GETSET(string, category, Category);
		GETSET(string, source, Source);
		
};

class RSSFeed
{
	public:
		RSSFeed(const string& afeedURL, const string& aSource, const string& codeingType)
			: feedURL(afeedURL), source(aSource), lastNewsDate(0), codeing(codeingType)
		{}
		virtual ~RSSFeed();
		
		typedef vector<RSSItem*> RSSItemList;
//		typedef RSSItemList::const_iterator RSSItemIter;
		
		bool UpdateFeedNewXML();
		
		GETSET(time_t, lastNewsDate, LastNewsDate);
		GETSET(string, feedURL, FeedURL);
		GETSET(string, source, Source);
		GETSET(string, codeing, Codeing);
		
		const RSSItemList& getNewsList() const// [+] IRainman fix.
		{
			return m_newsList;
		}
	public:
		enum RssFormat {
			RSS_2_0,
			RSS_ATOM,
			RSS_UNKNOWN
		};

		enum RSSParser {
			XML_PARSER,
			XML_SIMPLE
		};

	private:
		size_t GetRSSData(const string &url, string &data);
		void clearNewsList();
		void removeCDATA(const string &str, string &tmp);
		bool UpdateFeedOldParser(const string &data);
		
		RssFormat DetectRSSFormat(void* data, RSSParser parser);
		bool ProcessRSS(void* data, RSSParser parser, bool isUtf8);
		bool ProcessAtom(void* data, RSSParser parser, bool isUtf8);
		time_t convertPubDate(const string& p_str_date); // [+]PPA

		FastCriticalSection csNews;
		RSSItemList m_newsList;
};

class RSSListener
{
	public:
		virtual ~RSSListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Added;
		typedef X<1> NewRSS;
		typedef X<2> Failed;
		
		virtual void on(Added, const RSSItem*) noexcept { }
		virtual void on(Failed, const string&) noexcept { }
		virtual void on(NewRSS, const unsigned int) noexcept { }
};

enum RSSManagerTasks
{
	CHECK_NEWS,
};

class RSSManager :
	public Singleton<RSSManager>, public Speaker<RSSListener>, public BackgroundTaskExecuter<RSSManagerTasks>, private SettingsManagerListener, private TimerManagerListener
{
	public:
	
		typedef vector<const RSSItem*> NewsList;
		
		typedef vector<RSSFeed*> FeedList;
		
		void updateFeeds()
		{
			{
				Lock l(csFeed);
				if (m_feeds.empty()) // [+] IRainman fix.
					return;
			}
			addTask(CHECK_NEWS); // [!] IRainman fix done [2] https://www.box.net/shared/be613d6f54c533c0e1ff
		}
		
		const FeedList& lockFeedList()
		{
			csFeed.lock();
			return m_feeds;
		}
		void unlockFeedList()
		{
			csFeed.unlock();
		}
		
		const NewsList& lockNewsList()
		{
			csNews.lock();
			return m_newsList;
		}
		void unlockNewsList()
		{
			csNews.unlock();
		}
		 
		bool hasRSSFeed(const string& url, const string& name);
		
		RSSFeed* addNewFeed(const string &url, const string & name, const string& codeing, bool bUpdateFeeds = false);
		bool removeFeedAt(size_t pos);
		
		typedef StringList CodeingList;
		
		const string& getCodeing(const size_t i)
		{
			if (i < m_codeingList.size())
				return m_codeingList[i];

			return m_codeingList[0];
		}
		
		size_t GetCodeingByString(const string& codeing);
		
	private:
	
		CodeingList m_codeingList;
		
		friend class Singleton<RSSManager>;
		
		RSSManager();
		
		virtual ~RSSManager() ;
		
		void clearLists();
		
		FeedList m_feeds;
		
		NewsList m_newsList;
		
		CriticalSection csFeed;
		FastCriticalSection csNews;
		
		void fail(const string& aError);
		void execute(const RSSManagerTasks& p_task);
		
		void updateAllFeeds();
		
		bool canAdd(const RSSItem* p_item);
		// SettingsManagerListener
		virtual void on(SettingsManagerListener::Save, SimpleXML& xml) override
		{
			save(xml);
		}
		virtual void on(SettingsManagerListener::Load, SimpleXML& xml) override
		{
			load(xml);
		}
		
		// TimerManagerListener
		virtual void on(TimerManagerListener::Minute, uint64_t aTick) noexcept override;
		void load(SimpleXML& aXml);
		void save(SimpleXML& aXml);
		unsigned int minuteCounter;
		
		// Show ballon
		// void ShowBallonNews(); TODO?
};
#endif // !defined(RSS_MANAGER_H)

#endif // IRAINMAN_INCLUDE_RSS