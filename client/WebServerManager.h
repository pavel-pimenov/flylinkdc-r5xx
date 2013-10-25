/*
 * Copyright (C) 2003 Twink, spm7@waikato.ac.nz
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
 *
 *
 * Current version 2008-2013 (c)IRainman a.rainman@gmail.com
 */

#pragma once
#include "DCPlusPlus.h"
#include "ServerSocket.h"
#include "SearchManager.h"

#ifdef _DEBUG
//#define _DEBUG_WEB_SERVER_
#endif;

#if defined(NIGHT_BUILD)
#define IRAINMAN_NG_WEB_SERVER_CORE
#endif

static const uint64_t WEB_SERVER_USER_SESSION_TIMEOUT = 300;

class WebServerListener
{
	public:
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Setup;
		typedef X<1> ShutdownPC;
		
		virtual void on(Setup) noexcept = 0;
		virtual void on(ShutdownPC, int action) noexcept = 0;
};

class WebServerManager : public Singleton<WebServerManager>, public ServerSocketListener, public Speaker<WebServerListener>, private SettingsManagerListener, private SearchManagerListener
{
	private:
		friend Singleton<WebServerManager>;
		
		WebServerManager(void);
		~WebServerManager(void);
		
		void Stop();
		
		enum PageID
		{
			INDEX,
			DOWNLOAD_QUEUE,
			SEARCH,
			SETTINGS,
			LOGS,
			LOG,
			SYSLOG,
			DOWNLOAD_FINISHED,
			UPLOAD_QUEUE,
			UPLOAD_FINISHED,
			LOGOUT,
			PAGE_404
		};
		
		struct WebPageInfo
		{
			WebPageInfo(PageID n, const string& t) : id(n), title(t) {}
			const PageID id;
			const string title;
		};
		
		WebPageInfo *page404;
		
		typedef boost::unordered_map<string, WebPageInfo*> WebPages;
		
		WebPages pages;
		
		string getDLQueue();
		string getULQueue();
		
		string getIndexPage();
		//getErrorPage();
		
		string getFinished(bool);
		string getSearch();
		string getSettings();
		
		string getLogsPage();
		string getLogs(bool Enable, const string& LogFile, const string& Caption, const string& MessageNotEnable);
		string getWebLogs();
		string getSysLogs();
		
		string search_delay;
		string results;
		string head;
		string foot;
		size_t PageIndex;
		bool sended_search;
		//bool search_started; // TODO
		
		std::map<size_t, string> SearchPages;
		
		bool started;
		FastCriticalSection cs;
		// ServerSocketListener
		void on(ServerSocketListener::IncomingConnection) noexcept;
		
		ServerSocket socket;
		
		struct user_login
		{
			uint64_t time;
			bool power;
		};
		
		boost::unordered_map<string, user_login> LoggedIn;
		
		size_t row;
		
		string token;
		
	public:
		typedef struct
		{
			UserPtr User;
			string HubURL;
		} searchresult;
	private:
	
		std::map<size_t, searchresult> SearchResultList;
		
	public:
		ServerSocket& getServerSocket()
		{
			return socket;
		}
		
		struct UserStatus
		{
			private:
				bool isLogged, isPower;
			public:
				UserStatus(bool islogged = false, bool ispower = false) : isLogged(islogged), isPower(ispower)
				{
				
				}
				bool isloggedin() const
				{
					return isLogged;
				}
				bool ispower() const
				{
					return isPower;
				}
		};
		
		void getPage(string& p_InOut, const string& IP, UserStatus CurrentUser);
		void getLoginPage(string& p_out);
		
		// SettingsManagerListener
		void on(SettingsManagerListener::Save, SimpleXML&) noexcept
		{
			if (BOOLSETTING(WEBSERVER))
			{
				Restart();
			}
			else
			{
				Stop();
			}
		}
		// SearchManagerListener
		void on(SearchManagerListener::SR, const SearchResultPtr& aResult) noexcept;
		
		void Start() noexcept;
		void Restart()
		{
			Stop();
			Start();
		}
		
		uint16_t getPort() const
		{
			return BOOLSETTING(WEBSERVER) && BOOLSETTING(WEBSERVER_ALLOW_UPNP) ? SETTING(WEBSERVER_PORT) : 0;
		}
		
		bool noPage(const string& for_find);
		
		void login(const string& ip, bool PowerUser = false)
		{
			user_login tmp;
			tmp.power = PowerUser;
			tmp.time = GET_TICK();
			LoggedIn[ip] = tmp;
		}
		
		void search(string search_str, int search_type);
		
		/*void searchstarted(bool s) { // TODO
		    search_started = s;
		}*/
		
		void reset()
		{
			row = 0; /* Counter to permit FireFox correct item clicks */
			//search_started = false; // TODO
			SearchManager::getInstance()->removeListener(this);
		}
		
		UserStatus GetUserStatus(const string& ip)
		{
			auto i = LoggedIn.find(ip);
			if (i != LoggedIn.cend())
			{
				const uint64_t elapsed = (GET_TICK() - i->second.time) / 1000;
				if (elapsed > WEB_SERVER_USER_SESSION_TIMEOUT)
				{
					LoggedIn.erase(i);
					return UserStatus();
				}
				else
					LoggedIn[ip].time = GET_TICK();
					
				return UserStatus(true, i->second.power);
			}
			
			return UserStatus();
		}
		
		//SearchResultList
		void AddSearchResult(size_t number, const UserPtr& User, const string& HubURL)
		{
			searchresult toAdd;
			toAdd.User = User;
			toAdd.HubURL = HubURL;
			SearchResultList[number] = toAdd;
		}
		
		searchresult GetSearchResult(size_t number)
		{
			searchresult tmp;
			if (SearchResultList.find(number) != SearchResultList.cend())
			{
				tmp = SearchResultList[number];
			}
			return tmp;
		}
		//SearchPages
		void AddSearchPage(const string& rez, size_t num)
		{
			SearchPages[num] = rez;
		}
		
		const string& GetSearchPage(size_t num)
		{
			if (SearchPages.find(num) != SearchPages.cend())
			{
				return SearchPages[num];
			}
			return Util::emptyString;
		}
		
		size_t SearchPageCount() const
		{
			return SearchPages.size();
		}
		
		void ChangePage(size_t num)
		{
			PageIndex = num;
		}
		
		static const string& getTemplatePath()
		{
			static const string g = "FlylinkDC" PATH_SEPARATOR_STR "template" PATH_SEPARATOR_STR; // TODO: add settings
			return g;
		}
		
		// HTML Templates injection. SCALOlaz dynamically. I don't understand what i'm write...
		// Load HTML to string
		string GetTplFile(const char* tpl_name) const
		{
			string templ;
			try
			{
				templ = File(Util::getWebServerPath() + getTemplatePath() + tpl_name, File::READ, File::OPEN).read();
			}
			catch (const FileException& e)
			{
				templ = e.getError();
			}
			return templ;
		}
		// Set result to Template tags
		// [!] IRainman fix.
#define TplSetParam(str, tag, val) { static const string fullTag = "{" tag "}"; internal_tplSetParam(str, fullTag, val); }
		
		void internal_tplSetParam(string& text_in_out, const string& tag, const string& value) const
		{
			size_t start = 0;
			while (true)
			{
				start = text_in_out.find(tag, start);
				if (start == string::npos)
					break;
					
				text_in_out.replace(start, tag.size(), value);
				start = start + value.size();
			}
		}
		// [~] IRainman fix.
		
		string TplFooterTime()
		{
			string str_tpl = GetTplFile("footer_data.html");
			TplSetParam(str_tpl, "LANG_WEBSERVER_GENERATED", STRING(WEBSERVER_GENERATED_IN_MESSAGE));
			TplSetParam(str_tpl, "TIME", Util::getShortTimeString());
			return str_tpl;
		}
		// Теперь надо обработать include - подгрузить ещё один файл в тело переменной
		// END HTML Templates injection
};

class WebServerSocket : public BASE_THREAD
{
	public:
		WebServerSocket() : sock(NULL)
		{
			memzero(&from, sizeof(from));
		}
		void accept(ServerSocket *s)
		{
			int fromlen = sizeof(from);
#ifdef _DEBUG_WEB_SERVER_
			printf("accepting\n");
#endif
			sock = ::accept(s->getSock(), (struct sockaddr*) & from, &fromlen); // http://msdn.microsoft.com/en-us/library/windows/desktop/ms737526(v=vs.85).aspx
			u_long b = 1;
			ioctlsocket(sock, FIONBIO, &b);
		}
		
		int run();
		
	private:
		sockaddr_in from;
		SOCKET sock;
};
