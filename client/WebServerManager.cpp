/*
* Copyright (C) 2003 Twink, spm7@waikato.ac.nz
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of+
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Current version 2008-2013 IRainman a.rainman at gmail pt com
* Templation by Scalolaz zippozend at gmail pt com
* Last update 2013.5.22, fully compatible with StrongDC++
*/

#include "stdinc.h"

#include "WebServerManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "FinishedManager.h"
#include "UploadManager.h"
#include "StringTokenizer.h"
#include "SearchResult.h"
#include "Socket.h"
#include <boost/algorithm/string.hpp>

static const string NotFoundHeader = "HTTP/1.0 404 Not Found\r\n";

WebServerManager* Singleton<WebServerManager>::instance = nullptr;

WebServerManager::WebServerManager(void) : started(false), page404(nullptr), sended_search(false), m_search_token(0)
{
	SettingsManager::getInstance()->addListener(this);
}

WebServerManager::~WebServerManager(void)
{
	Stop();
}

void WebServerManager::Start() noexcept
{
	if (started)
		return;
	{
		CFlyFastLock(cs);
		if (started)
			return;
			
		started = true;
	}
	page404 = new WebPageInfo(PAGE_404, STRING(WEBSERVER_PAGE_NOT_FOUND));
	pages["/"] = new WebPageInfo(INDEX, "");
	pages["/index.htm"] = new WebPageInfo(INDEX, "");
	pages["/dlqueue.htm"] = new WebPageInfo(DOWNLOAD_QUEUE, STRING(DOWNLOAD_QUEUE));
	pages["/dlfinished.htm"] = new WebPageInfo(DOWNLOAD_FINISHED, STRING(FINISHED_DOWNLOAD));
	pages["/ulqueue.htm"] = new WebPageInfo(UPLOAD_QUEUE, STRING(SETTINGS_LOG_UPLOADS));
	pages["/ulfinished.htm"] = new WebPageInfo(UPLOAD_FINISHED, STRING(FINISHED_UPLOADS));
	pages["/weblog.htm"] = new WebPageInfo(LOG, STRING(SETTINGS_LOG_WEBSERVER));
	pages["/syslog.htm"] = new WebPageInfo(SYSLOG, STRING(SETTINGS_LOG_SYSTEM_MESSAGES));
	pages["/search.htm"] = new WebPageInfo(SEARCH, STRING(WEBSERVER_SEARCH_PAGE_NAME));
	pages["/settings.htm"] = new WebPageInfo(SETTINGS, STRING(SETTINGS));
	pages["/logs.htm"] = new WebPageInfo(LOGS, STRING(SETTINGS_LOGS));
	pages["/logout.htm"] = new WebPageInfo(LOGOUT, STRING(LOG_OUT));
#ifdef _DEBUG_WEB_SERVER_
	AllocConsole();
	freopen("con:", "w", stdout);
	printf("WebServer debug log:\n");
#endif
	try
	{
		socket.listen(static_cast<uint16_t>(SETTING(WEBSERVER_PORT)), SETTING(WEBSERVER_BIND_ADDRESS));
		socket.addListener(this);
		fly_fire(WebServerListener::Setup());
	}
	catch (const SocketException&) {} //-V565
}

void WebServerManager::Stop()
{
	if (!started)
		return;
	{
		CFlyFastLock(cs);
		if (!started)
			return;
			
		started = false;
	}
	try
	{
		socket.removeListener(this);
		socket.disconnect();
	}
	catch (const SocketException&) {} //-V565
	safe_delete(page404);
	for (auto p = pages.begin(); p != pages.end(); ++p)
	{
		safe_delete(p->second);
	}
	head.clear();
	foot.clear();
	search_delay.clear();
#ifdef _DEBUG_WEB_SERVER_
	FreeConsole();
#endif
}

void WebServerManager::on(ServerSocketListener::IncomingConnection) noexcept
{
	WebServerSocket *wss = new WebServerSocket();
	wss->accept(&socket);
	wss->start(64);
}

void WebServerManager::getLoginPage(string& p_out)
{
	const string& l_webserver = STRING(WEBSERVER);
	string pagehtml = GetTplFile("header.html");
	pagehtml += GetTplFile("login.html");
	pagehtml += GetTplFile("footer.html");
	TplSetParam(pagehtml, "TITLE", APPNAME " " + l_webserver + " - " + STRING(WEBSERVER_LOGIN_PAGE_NAME));
	TplSetParam(pagehtml, "THEME_PATH", "FlylinkDC");
	TplSetParam(pagehtml, "META", "");
	TplSetParam(pagehtml, "APPNAME", APPNAME " " + l_webserver);
	TplSetParam(pagehtml, "LANG_USERNAME", STRING(SETTINGS_SOCKS5_USERNAME));
	TplSetParam(pagehtml, "LANG_PASSWORD", STRING(PASSWORD));
	TplSetParam(pagehtml, "LANG_LOGIN", STRING(LOG_IN));
	
	p_out = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + Util::toString(pagehtml.length()) + "\r\n\r\n";
	p_out += pagehtml;
}

void WebServerManager::on(SettingsManagerListener::Repaint)
{
	dcassert(!ClientManager::isShutdown())
	if (!ClientManager::isShutdown())
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
}

void WebServerManager::getPage(string& p_InOut, const string& IP, UserStatus CurrentUser)
{
#ifdef _DEBUG_WEB_SERVER_
	printf("requested: '%s'\n", p_InOut.c_str());
#endif
	string header = "HTTP/1.0 200 OK\r\n";
	
	WebPageInfo *page = page404;
	WebPages::const_iterator f = pages.find(p_InOut);
	if (f != pages.end())
		page = f->second;
		
	string pagehtml = GetTplFile("header.html");
	const string& l_webserver = STRING(WEBSERVER);
	if ((page->id == SEARCH) && sended_search)
	{
		TplSetParam(pagehtml, "META", "<meta http-equiv=\"Refresh\" content=\"" + search_delay + ";URL=search.htm?stop=true\"/>");
	}
	
	TplSetParam(pagehtml, "TITLE", APPNAME " " + l_webserver + " - " + STRING(WEBSERVER_LOGIN_PAGE_NAME));
	TplSetParam(pagehtml, "META", "");
	string str_tpl = GetTplFile("pages.html");
	TplSetParam(str_tpl, "APPNAME", APPNAME " " + l_webserver);
	TplSetParam(str_tpl, "MENU_INDEX", "<a href='index.htm'>" + STRING(SETTINGS_GENERAL) + "</a>");
	TplSetParam(str_tpl, "MENU_SEARCH", "<a href='search.htm'>" + STRING(WEBSERVER_SEARCH_PAGE_NAME) + "</a>");
	TplSetParam(str_tpl, "MENU_LOGOUT", "<a href='logout.htm'>" + STRING(LOG_OUT) + "</a>");
	TplSetParam(str_tpl, "USERIP", IP);
	if (CurrentUser.ispower())
	{
		TplSetParam(str_tpl, "USERSTATE", STRING(WEBSERVER_FULL_USER));
	}
	else
	{
		TplSetParam(str_tpl, "USERSTATE", STRING(WEBSERVER_USER));
	}
	
	// Only admin view this functions
	if (CurrentUser.ispower())
	{
		// system  managment
		static const string str_tpl_admin = "<div id='icons'>\n"
		                                    "<a href='switch.htm' id=switch></a>\n"
		                                    "<a href='logoff.htm' id=logoff></a>\n"
		                                    "<a href='suspend.htm' id=suspend></a>\n"
		                                    "<a href='reboot.htm' id=reboot></a>\n"
		                                    "<a href='shutdown.htm' id=shutdown></a>\n"
		                                    "</div>\n";
		TplSetParam(str_tpl, "CONTENT_ADMIN_BUTTONS", str_tpl_admin);
		TplSetParam(str_tpl, "MENU_SETTINGS", "<a href='settings.htm'>" + STRING(SETTINGS) + "</a>");
	}
	else
	{
		TplSetParam(str_tpl, "CONTENT_ADMIN_BUTTONS", "");
		TplSetParam(str_tpl, "MENU_SETTINGS", "");
	}
	
	TplSetParam(str_tpl, "MENU_WEBLOG", "<a href='weblog.htm'>" + STRING(SETTINGS_LOG_WEBSERVER) + "</a>");
	TplSetParam(str_tpl, "MENU_SYSLOG", "<a href='syslog.htm'>" + STRING(SETTINGS_LOG_SYSTEM_MESSAGES) + "</a>");
	TplSetParam(str_tpl, "MENU_UPLOADS_FINISHED", "<a href='ulfinished.htm'>" + STRING(FINISHED_UPLOADS) + "</a>");
	TplSetParam(str_tpl, "MENU_UPLOADS_QUEUE", "<a href='ulqueue.htm'>" + STRING(WAITING_USERS) + "</a>");
	TplSetParam(str_tpl, "MENU_DOWNLOADS_QUEUE", "<a href='dlqueue.htm'>" + STRING(DOWNLOAD_QUEUE) + "</a>");
	TplSetParam(str_tpl, "MENU_DOWNLOADS_FINISHED", "<a href='dlfinished.htm'>" + STRING(FINISHED_DOWNLOADS) + "</a>");
	if ((page->id == SEARCH))
	{
		string tmp_pagehtml = "<script type=\"text/javascript\" src=\"";
		tmp_pagehtml += "search4608.js\"></script>\n";
		if (sended_search)
		{
			tmp_pagehtml += "<script type=\"text/javascript\">";
			tmp_pagehtml += "/*<![CDATA[*/";
			tmp_pagehtml += "var limit=" + search_delay + ";";
			tmp_pagehtml += "processTimer();";
			//tmp_pagehtml += "GetSearchRezult();"; // TODO
			tmp_pagehtml += "/*]]>*/";
			tmp_pagehtml += "</script>";
		}
	}
	
	switch (page->id)
	{
		case INDEX:
			TplSetParam(str_tpl, "CONTENT", getIndexPage());
			break;
		case DOWNLOAD_QUEUE:
			TplSetParam(str_tpl, "CONTENT", getDLQueue());
			break;
		case SEARCH:
			TplSetParam(str_tpl, "CONTENT", getSearch());
			break;
		case LOG:
			TplSetParam(str_tpl, "CONTENT", getWebLogs());
			break;
		case SYSLOG:
			TplSetParam(str_tpl, "CONTENT", getSysLogs());
			break;
		case DOWNLOAD_FINISHED:
			TplSetParam(str_tpl, "CONTENT", getFinished(false));
			break;
		case UPLOAD_QUEUE:
			TplSetParam(str_tpl, "CONTENT", getULQueue());
			break;
		case UPLOAD_FINISHED:
			TplSetParam(str_tpl, "CONTENT", getFinished(true));
			break;
		case SETTINGS:
			TplSetParam(str_tpl, "CONTENT", getSettings());
			break;
		case LOGOUT:
		{
			const auto i = LoggedIn.find(IP);
			if (i != LoggedIn.cend()) // [1] https://www.box.net/shared/75d5cd705f7609438910
				LoggedIn.erase(i);
				
			getLoginPage(p_InOut);
			return;
		}
		case PAGE_404:
		default:
			int action = -1; // system  managment
			if (p_InOut == "/shutdown.htm") action = 0;
			else if (p_InOut == "/reboot.htm") action = 2;
			else if (p_InOut == "/suspend.htm") action = 3;
			else if (p_InOut == "/logoff.htm") action = 1;
			else if (p_InOut == "/switch.htm") action = 5;
			else    // system  managment
			{
				header = NotFoundHeader;
				pagehtml += STRING(WEBSERVER_PAGE_NOT_FOUND);
				break;
			} // system  managment
			if (CurrentUser.ispower() && action >= 0)
			{
				fly_fire1(WebServerListener::ShutdownPC(), action);
				pagehtml += "&nbsp;" + STRING(WEBSERVER_REGUEST_SENT_OK);
				//LRESULT MainFrame::OnClose(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
			}
			else
			{
				pagehtml += "&nbsp;" + STRING(WEBSERVER_ONLY_POWER_USER_MESSAGE);
			}
			break;
			
	}
	
	pagehtml += str_tpl;
	pagehtml += TplFooterTime();    // Generated Time
	pagehtml += GetTplFile("footer.html");
	
	// PushAllParam into template
	TplSetParam(pagehtml, "THEME_PATH", "FlylinkDC");
	TplSetParam(pagehtml, "I_SEARCH_DELAY", search_delay);
	
	header += "Content-Type: text/html\r\nContent-Length: " + Util::toString(pagehtml.length()) + "\r\n\r\n";
	
#ifdef _DEBUG_WEB_SERVER_
	printf("sending: %s\n", (header + pagehtml).c_str());
#endif
	p_InOut = header + pagehtml;
}

static const string checked_checked = "checked=checked";
static inline const string& toChecked(const bool val)
{
	return val ? checked_checked : Util::emptyString;
}

string WebServerManager::getSettings()
{
	string ret = GetTplFile("page_settings.html");
	TplSetParam(ret, "PAGENAME", STRING(SETTINGS));
	TplSetParam(ret, "LANG_KBPS", STRING(KBPS));
	TplSetParam(ret, "LANG_OK", STRING(OK));
	TplSetParam(ret, "LANG_CANCEL", STRING(CANCEL));
	TplSetParam(ret, "LANG_SHARE", STRING(SHARED));
	TplSetParam(ret, "LANG_WEBSERVER", STRING(WEBSERVER));
	TplSetParam(ret, "LANG_TRANSFER_LIMITS", STRING(SETCZDC_TRANSFER_LIMITING));
	TplSetParam(ret, "LANG_BUTTON_SHARE_REFRESH", STRING(CMD_SHARE_REFRESH));
	TplSetParam(ret, "LANG_BUTTON_FILELIST_REFRESH", STRING(MENU_REFRESH_FILE_LIST_PURGE));
	
	TplSetParam(ret, "LANG_WEBSERVER_SEARCHSIZE", STRING(WEBSERVER_SEARCHSIZE));
	TplSetParam(ret, "S_WEBSERVER_SEARCHSIZE", Util::toString(SETTING(WEBSERVER_SEARCHSIZE)));
	TplSetParam(ret, "LANG_WEBSERVER_SEARCHPAGESIZE", STRING(WEBSERVER_SEARCHPAGESIZE));
	TplSetParam(ret, "S_WEBSERVER_SEARCHPAGESIZE", Util::toString(SETTING(WEBSERVER_SEARCHPAGESIZE)));
	
	TplSetParam(ret, "S_CHECKED_WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR", toChecked(BOOLSETTING(WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR)));
	
	TplSetParam(ret, "LANG_WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR", STRING(WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR));
	TplSetParam(ret, "LANG_WEBSERVER_ONLY_POWER_USER_MESSAGE", STRING(WEBSERVER_ONLY_POWER_USER_MESSAGE));
	
	TplSetParam(ret, "S_CHECKED_THROTTLE_ENABLE", toChecked(BOOLSETTING(THROTTLE_ENABLE)));
	
	TplSetParam(ret, "LANG_SETCZDC_ENABLE_LIMITING", STRING(SETCZDC_ENABLE_LIMITING));
	
	TplSetParam(ret, "LANG_SETCZDC_DOWNLOAD", STRING(DOWNLOAD));
	TplSetParam(ret, "S_SETCZDC_DOWNLOAD", Util::toString(SETTING(MAX_DOWNLOAD_SPEED_LIMIT_NORMAL)));
	TplSetParam(ret, "LANG_SETCZDC_UPLOAD", STRING(UPLOAD));
	TplSetParam(ret, "S_SETCZDC_UPLOAD", Util::toString(SETTING(MAX_UPLOAD_SPEED_LIMIT_NORMAL)));
	
	TplSetParam(ret, "S_CHECKED_TIME_DEPENDENT_THROTTLE", toChecked(BOOLSETTING(TIME_DEPENDENT_THROTTLE)));
	
	TplSetParam(ret, "LANG_SETCZDC_ALTERNATE_LIMITING", STRING(SETCZDC_ALTERNATE_LIMITING));
	TplSetParam(ret, "S_SETCZDC_DOWNLOAD_T", Util::toString(SETTING(MAX_DOWNLOAD_SPEED_LIMIT_TIME)));
	TplSetParam(ret, "S_SETCZDC_UPLOAD_T", Util::toString(SETTING(MAX_UPLOAD_SPEED_LIMIT_TIME)));
	
	TplSetParam(ret, "S_SETCZDC_ALTERNATE_LIMITING", Util::toString(SETTING(BANDWIDTH_LIMIT_START)));
	TplSetParam(ret, "LANG_SETCZDC_TO", STRING(SETCZDC_TO));
	TplSetParam(ret, "S_SETCZDC_TO", Util::toString(SETTING(BANDWIDTH_LIMIT_END)));
	
	//  ret = TplSetParam( ret, "  ",  );
	return ret;
}

string WebServerManager::getLogsPage()
{
	string ret = "<h1>" + STRING(SETTINGS_LOGS) + "</h1>\n";
	ret += "<div class=menu><br />\n";
	ret += "<a href='weblog.htm'>" + STRING(SETTINGS_LOG_WEBSERVER) + "</a>\n";
	ret += "<a href='syslog.htm'>" + STRING(SETTINGS_LOG_SYSTEM_MESSAGES) + "</a>\n";
	ret += "<a href='ulfinished.htm'>" + STRING(FINISHED_UPLOADS) + "</a>\n";
	ret += "<a href='ulqueue.htm'>" + STRING(SETTINGS_LOG_UPLOADS) + "</a>\n";
	ret += "<a href='dlfinished.htm'>" + STRING(FINISHED_DOWNLOADS) + "</a><br /><br />\n";
	ret += "</div>";
	return ret;
}

string WebServerManager::getLogs(bool Enable, const string& LogFile, const string& Caption, const string& MessageNotEnable)
{
	string ret = "<h3>" + Caption + "</h3>\n";
	const string path = SETTING(LOG_DIRECTORY) + LogFile;
	const size_t datasize = 32768;
	if (Enable)
	{
		try
		{
			File f(path, File::READ, File::OPEN);
			const size_t size = f.getSize();
			if (size > datasize)
			{
				f.setPos(size - datasize);
			}
			const StringTokenizer<string> l_tok(f.read(datasize), "\r\n");
			const StringList& lines = l_tok.getTokens();
			const size_t linesCount = lines.size();
			string tmp;
			for (size_t i = linesCount > 100 ? linesCount - 100 : linesCount; i < (linesCount - 1); i++)
			{
				tmp += "&nbsp;&raquo;&nbsp;" + lines[i] + "<br />";
			}
			boost::replace_all(tmp, "\n", "<br />");
			ret += tmp;
		}
		catch (const FileException & e)
		{
			ret += "&nbsp;<div class=content><strong>" + STRING(ERRORS) + ":</strong> " + e.getError() + "</div>\n";
		}
	}
	else
	{
		ret += "&nbsp;<div class=content>" + MessageNotEnable + "</div>\n";
	}
	return ret;
}

string WebServerManager::getWebLogs()
{
	return getLogs(BOOLSETTING(LOG_WEBSERVER), SETTING(LOG_FILE_WEBSERVER),
	               STRING(SETTINGS_LOG_WEBSERVER), STRING(LOGGING_NOT_ENABLED_WEBSERVER));
}

string WebServerManager::getSysLogs()
{
	return getLogs(BOOLSETTING(LOG_WEBSERVER), SETTING(LOG_FILE_SYSTEM),
	               STRING(SETTINGS_LOG_SYSTEM_MESSAGES), STRING(LOGGING_NOT_ENABLED_SYSTEM));
}

string WebServerManager::getSearch()
{
	string ret = GetTplFile("page_search.html");
	TplSetParam(ret, "PAGENAME", STRING(WEBSERVER_SEARCH_PAGE_NAME));
	TplSetParam(ret, "THEME_PATH", "FlylinkDC");
	if (sended_search)  // Processed... Please Wait
	{
		TplSetParam(ret, "SEARCH_SUITE", GetTplFile("page_search_proceed.html"));
		TplSetParam(ret, "LANG_BUTTON_STOP", STRING(STOP));
		TplSetParam(ret, "LANG_SEARCH_FOR", STRING(SEARCH_FOR));
		TplSetParam(ret, "LANG_PLEASE_WAIT", STRING(PLEASE_WAIT));
		sended_search = false;// search_started; TODO
	}
	else    // Select or get magnet-link
	{
		TplSetParam(ret, "SEARCH_SUITE", GetTplFile("page_search_select.html"));
		if (SETTING(WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR))   // If Enabled Change Download Dir
		{
			TplSetParam(ret, "SEARCH_DOWNLOAD_DIR", GetTplFile("page_search_dir_select.html"));
			TplSetParam(ret, "LANG_ADLS_DESTINATION", STRING(ADLS_DESTINATION));
			TplSetParam(ret, "S_ADLS_DESTINATION", SETTING(DOWNLOAD_DIRECTORY));
		}
		else
		{
			TplSetParam(ret, "SEARCH_DOWNLOAD_DIR", "");
		}
		TplSetParam(ret, "LANG_ADD2", STRING(ADD2));
		TplSetParam(ret, "LANG_SEARCH", STRING(SEARCH));
		TplSetParam(ret, "LANG_WEBSERVER_ADD_MAGNET_LINK", STRING(WEBSERVER_ADD_MAGNET_LINK));
		TplSetParam(ret, "LANG_WEBSERVER_MAGNET_LINK", STRING(WEBSERVER_MAGNET_LINK));
		TplSetParam(ret, "LANG_ENTER_SEARCH_STRING", STRING(ENTER_SEARCH_STRING));
		// ... Select List - Types of Search Content
		TplSetParam(ret, "LANG_OPT_ANY", STRING(ANY));
		TplSetParam(ret, "LANG_OPT_AUDIO", STRING(AUDIO));
		TplSetParam(ret, "LANG_OPT_COMPRESSED", STRING(COMPRESSED));
		TplSetParam(ret, "LANG_OPT_DOCUMENT", STRING(DOCUMENT));
		TplSetParam(ret, "LANG_OPT_EXECUTABLE", STRING(EXECUTABLE));
		TplSetParam(ret, "LANG_OPT_PICTURE", STRING(PICTURE));
		TplSetParam(ret, "LANG_OPT_VIDEO_AND_SUBTITLES", STRING(VIDEO_AND_SUBTITLES));
		TplSetParam(ret, "LANG_OPT_DIRECTORY", STRING(DIRECTORY));
	}
	// Pagination ...
	if (PageIndex && (PageIndex <= SearchPageCount()))
	{
		string ret_pages;
		if (PageIndex > 1)
		{
			ret_pages = GetTplFile("page_search_pagination.html");
			TplSetParam(ret_pages, "S_PAGE", Util::toString(PageIndex - 1));
			TplSetParam(ret_pages, "LANG_PAGE_BUTTON", STRING(BACK));
			TplSetParam(ret, "SEARCH_PAGINATION_BACK", ret_pages);
		}
		else
		{
			TplSetParam(ret, "SEARCH_PAGINATION_BACK", "");
		}
		
		//  ret_pages += GetTplFile("page_search_pagenumber.html");
		//  ret_pages = TplSetParam( ret_pages, "S_PAGE", GetSearchPage(PageIndex) );
		
		if ((PageIndex > 0) && (PageIndex < SearchPageCount()))
		{
			ret_pages = GetTplFile("page_search_pagination.html");
			TplSetParam(ret_pages, "S_PAGE", Util::toString(PageIndex + 1));
			TplSetParam(ret_pages, "LANG_PAGE_BUTTON", STRING(NEXT));
			TplSetParam(ret, "SEARCH_PAGINATION_NEXT", ret_pages);
		}
		else
		{
			TplSetParam(ret, "SEARCH_PAGINATION_NEXT", "");
		}
	}
	else
	{
		TplSetParam(ret, "SEARCH_PAGINATION_BACK", "");
		TplSetParam(ret, "SEARCH_PAGINATION_NEXT", "");
	}
	TplSetParam(ret, "SEARCH_LIST", GetTplFile("page_search_list.html"));
	TplSetParam(ret, "S_LIST", GetSearchPage(PageIndex));
	return ret;
}

string WebServerManager::getFinished(bool uploads)
{
	string ret = GetTplFile("page_upldnl_finished.html");
	TplSetParam(ret, "PAGENAME", (uploads ? STRING(FINISHED_UPLOADS) : STRING(FINISHED_DOWNLOADS)));
	TplSetParam(ret, "LANG_TIME", STRING(TIME));
	TplSetParam(ret, "LANG_FILENAME", STRING(FILENAME));
	TplSetParam(ret, "LANG_SIZE", STRING(SIZE));
	
	string ret_fin_list;
	const FinishedItemList& fl = FinishedManager::lockList(uploads ? FinishedManager::e_Upload : FinishedManager::e_Download);
	for (auto i = fl.cbegin(); (i != fl.cend()); ++i)
	{
		ret_fin_list += "<tr>\n";
		ret_fin_list += "<td>" + Util::formatDigitalClock((*i)->getTime()) + "</td>\n";
		ret_fin_list += "<td>" + Util::getFileName((*i)->getTarget()) + "</td>\n";
		ret_fin_list += "<td>" + Util::formatBytes((*i)->getSize()) + "</td>\n";
		ret_fin_list += "</tr>\n";
	}
	FinishedManager::unlockList(uploads ? FinishedManager::e_Upload : FinishedManager::e_Download);
	TplSetParam(ret, "FINISHED_LIST", ret_fin_list);
	return ret;
}

string WebServerManager::getDLQueue()
{
	string ret = GetTplFile("page_download_queue.html");
	TplSetParam(ret, "PAGENAME", STRING(DOWNLOAD_QUEUE));
	TplSetParam(ret, "LANG_FILENAME", STRING(FILENAME));
	TplSetParam(ret, "LANG_SIZE", STRING(SIZE));
	TplSetParam(ret, "LANG_DOWNLOADED", STRING(DOWNLOADED));
	TplSetParam(ret, "LANG_SPEED", STRING(SPEED));
	TplSetParam(ret, "LANG_SEGMENTS", STRING(SEGMENTS));
	TplSetParam(ret, "LANG_MANAGE", STRING(MANAGE));
	
	static const string ManagementElements = \
	                                         "<option value='" + STRING(AUTO) +    "'>" + STRING(AUTO) +    "</option>\n" + \
	                                         "<option value='" + STRING(PAUSED) +  "'>" + STRING(PAUSED) +  "</option>\n" + \
	                                         "<option value='" + STRING(LOWEST) +  "'>" + STRING(LOWEST) +  "</option>\n" + \
	                                         "<option value='" + STRING(LOW) +     "'>" + STRING(LOW) +     "</option>\n" + \
	                                         "<option value='" + STRING(NORMAL) +  "'>" + STRING(NORMAL) +  "</option>\n" + \
	                                         "<option value='" + STRING(HIGH) +    "'>" + STRING(HIGH) +    "</option>\n" + \
	                                         "<option value='" + STRING(HIGHEST) + "'>" + STRING(HIGHEST) + "</option>\n" + \
	                                         "</select>\n" + \
	                                         "<input class=button type=submit name=dqueue value='" + STRING(SET_PRIORITY) + "' />\n" + \
	                                         "<input class=button type=submit name=dqueue value='" + STRING(REMOVE2) + "' onclick=\"return confirm('" + STRING(REALLY_REMOVE) + "')\" /></form></td>\n" + \
	                                         "</tr>\n";
	                                         
	// [!] IRainman fix. TODO.
	string ret_select;
	RLock(*QueueItem::g_cs);
	QueueManager::LockFileQueueShared l_fileQueue;
	const auto& li = l_fileQueue.getQueueL();
	for (auto j = li.cbegin(); j != li.cend(); ++j)
	{
		const QueueItemPtr& qi = j->second;
		const int64_t size = qi->getSize();
		/*
		aQI->getTarget();
		aQI->getTargetFileName();
		aQI->getTempTarget();
		aQI->setTarget();
		aQI->setTempTarget();
		aQI->setMaxSegments();
		aQI->getMaxSegments();
		*/
		
		// TODO aQI->getChunksVisualisation();
		
		ret_select += "<tr>\n";
		ret_select += "<td>" + qi->getTargetFileName() + "</td>\n";
		
		const int64_t downloaded = qi->getDownloadedBytes();
		string percent;
		if (size > 0)
		{
			percent = "(" + Util::toString(double(downloaded * 100. / size)) + "%)";
			ret_select += "<td>" + Util::formatBytes(size) + "</td>\n";
		}
		else if (size == 0)
		{
			percent = "0";
		}
		else if (size < 0)
		{
			if (qi->isSet(QueueItem::FLAG_USER_LIST))
			{
				percent = "(" + STRING(FILE_LIST) + ")";
			}
			else if (qi->isSet(QueueItem::FLAG_DCLST_LIST))
			{
				percent = "(" + STRING(DCLSTGEN_NAMEBORDER) + ")";
			}
			else if (qi->isSet(QueueItem::FLAG_USER_GET_IP))
			{
				percent = "(" + STRING(GET_IP) + ")";
			}
			else
			{
				percent = "(" + STRING(DIRECTORY) + ")";
				ret_select += "<input type=hidden name=dqueuedir value=1 />\n";
			}
			ret_select += "<td> - </td>\n";
		}
		
		ret_select += "<td>" + Util::formatBytes(downloaded) + ' ' + percent + "</td>\n";
		ret_select += "<td>" + (qi->isRunningL() ? Util::formatBytes(qi->getAverageSpeed()) + '/' + STRING(S) : (qi->isWaitingL() ? STRING(WAITING) : "Not runing")) + "</td>\n"; // [!]TODO translate
		ret_select += "<td>" + Util::toString((int)qi->countOnlineUsersL()) + '/' + Util::toString(qi->getMaxSegments()) + "</td>\n";
#ifdef _DEBUG_WEB_SERVER_
		ret_select += "<td><form method=get onsubmit=\"if(confirm('п»їРўРѕС‡РЅРѕ РўРѕС‡РЅРѕ?? CRAZY MS OS ^^')){this.submit}else{return false;}\" action='dlqueue.htm'>";
#else
		ret_select += "<td><form method=get action='dlqueue.htm'>";
#endif
		ret_select += "\n<input type=hidden name=qfile value=\"" + qi->getTarget() + "\" />\n";
		ret_select += "<select class=sel name=dp>\n";
		string current_prio;
		switch (qi->getPriority())
		{
			case QueueItem::PAUSED:
				current_prio = STRING(PAUSED);
				break;
			case QueueItem::LOWEST:
				current_prio = STRING(LOWEST);
				break;
			case QueueItem::LOW:
				current_prio = STRING(LOW);
				break;
			case QueueItem::NORMAL:
				current_prio = STRING(NORMAL);
				break;
			case QueueItem::HIGH:
				current_prio = STRING(HIGH);
				break;
			case QueueItem::HIGHEST:
				current_prio = STRING(HIGHEST);
				break;
			default:
				current_prio = STRING(AUTO);
				break;
		}
		ret_select += "<option value='" + current_prio + "' selected=selected>" + current_prio + "</option>\n";
		ret_select += ManagementElements;
	}
	TplSetParam(ret, "DOWNLOAD_QUEUE_LIST", ret_select);
	return ret;
}

string WebServerManager::getIndexPage()
{
	string ret = GetTplFile("page_index.html");
	TplSetParam(ret, "PAGENAME", STRING(WELCOME));
	TplSetParam(ret, "LANG_WELCOME", STRING(WELCOME));
	TplSetParam(ret, "LANG_TO", STRING(TO));
	TplSetParam(ret, "APPNAME", APPNAME);
	TplSetParam(ret, "LANG_WEBSERVER", STRING(WEBSERVER));
	return ret;
}
/*
string WebServerManager::getErrorPage()
{
const string& l_webserver = STRING(WEBSERVER);
#ifdef SCALOLAZ_NEW_WEB_SERVER
    string ret = GetTplFile("page_index.html");
        ret = TplSetParam( ret, "PAGENAME", STRING(ERROR) );
        ret = TplSetParam( ret, "LANG_WEBSERVER", l_webserver );
#else
    string ret = "<h2>" + STRING(ERROR) + "</h2>\n";
        ret += "<div class=content><br />&nbsp;" + STRING(WELCOME) + ' ' + STRING(TO) + " " APPNAME " " + l_webserver + "</div>\n";
#endif
    return ret;
}
*/
string WebServerManager::getULQueue()
{
	string ret = GetTplFile("page_upload_queue.html");
	TplSetParam(ret, "PAGENAME", STRING(WAITING_USERS));
	TplSetParam(ret, "LANG_USER", STRING(USER));
	TplSetParam(ret, "LANG_FILENAME", STRING(FILENAME));
	string ret_queue_list;
	{
		UploadManager::LockInstanceQueue lockedInstance;
		const auto& users = lockedInstance->getUploadQueueL();
		for (auto ii = users.cbegin(); ii != users.cend(); ++ii)
		{
			for (auto i = ii->m_waiting_files.cbegin(); i != ii->m_waiting_files.cend(); ++i)
			{
				ret_queue_list += "<tr>\n<td>" + Util::toString(ClientManager::getNicks(ii->getUser()->getCID(), Util::emptyString, false)) + "</td>\n";
				ret_queue_list += "<td>" + Util::getFileName((*i)->getFile()) + "</td>\n</tr>\n";
			}
		}
	}
	TplSetParam(ret, "UPLOAD_QUEUE_LIST", ret_queue_list);
	return ret;
}

StringMap getArgs(const string& arguments)
{
	StringMap args;
	string::size_type i = 0;
	string::size_type j = 0;
	while ((i = arguments.find('=', j)) != string::npos)
	{
		const string key = arguments.substr(j, i - j);
		string value = arguments.substr(i + 1);
		
		j = i + 1;
		if ((i = arguments.find('&', j)) != string::npos)
		{
			value = arguments.substr(j, i - j);
			j = i + 1;
		}
		
		args[key] = value;
	}
	return args;
}

static bool getFile(string& p_InOutData)
{
	ReplaceAllUriSeparatorToPathSeparator(p_InOutData);
	try
	{
		p_InOutData = File(Util::getWebServerPath() + p_InOutData, File::READ, File::OPEN).read();
	}
	catch (const FileException& e)
	{
		p_InOutData = STRING(WEBSERVER_PAGE_NOT_FOUND) + e.getError();
		return false;
	}
	return true;
}

bool WebServerManager::noPage(const string& for_find)
{
	if (pages.find(for_find) != pages.end() || for_find == "/shutdown.htm" || for_find == "/reboot.htm" || for_find == "/suspend.htm" || for_find == "/logoff.htm" || for_find == "/switch.htm")
	{
#ifdef _DEBUG_WEB_SERVER_
		dcdebug("This web server page not found, get file:%s\n", for_find.c_str());
#endif
		return false;
	}
	return true;
}

int WebServerSocket::run()
{
	const size_t g_size_buf = 8192;
	string header(g_size_buf, 0);
	int test = 0;
	while (++test < 10000)
	{
		const int size = recv(m_www_sock, &header[0], g_size_buf, 0);
		if (size == -1)
			continue;
			
		header.resize(static_cast<size_t>(size));
		
		const string IP = inet_ntoa(m_from.sin_addr);
		
		dcdebug("Webserver incoming: %s from IP %s\n", header.c_str(), IP.c_str()); //-V111
		
		string::size_type start = 0, end = 0;
		
		if (((start = header.find("GET ")) != string::npos) && (end = header.substr(start + 4).find(' ')) != string::npos) //-V112
		{
			if (BOOLSETTING(LOG_WEBSERVER) && (header.find("?user") == string::npos))
			{
				StringMap params;
				const string file = header.substr(start + 4, end); //-V112
				params["file"] = file;
				params["ip"] = IP;
				if (file != "/robots.txt")
					LOG(WEBSERVER, params);
			}
			string headerF = header.substr(start + 5, end - 1);
			const string::size_type endF = header.find('?');
			const string::size_type bs = header.substr(start + 5).find(' ');
			if ((endF != string::npos) && (endF < bs))
			{
				headerF = header.substr(start + 5, endF - 5);
			}
			if (WebServerManager::getInstance()->noPage('/' + headerF))
			{
#ifdef _DEBUG_WEB_SERVER_
				printf("requested: '%s'\n", headerF.c_str());
#endif
				if (!getFile(headerF))
				{
					const string& l_pageNotFoundStr = STRING(WEBSERVER_PAGE_NOT_FOUND);
					string l_header = NotFoundHeader;
					l_header += "Content-Type: text/html\r\nContent-Length: " + Util::toString(l_pageNotFoundStr.length()) + "\r\n\r\n";
					headerF = l_header + l_pageNotFoundStr;
				}
#ifdef _DEBUG_WEB_SERVER_
				printf("sending: %s\n", (headerF).c_str());
#endif
				::send(m_www_sock, headerF.c_str(), static_cast<int>(headerF.size()), 0);
				break;
			}
			
			header = header.substr(start + 4, end); //-V112
#ifdef _DEBUG_WEB_SERVER_
			dcdebug("%s\n", header.c_str());
#endif
			WebServerManager::UserStatus CurrentUser = WebServerManager::getInstance()->GetUserStatus(IP);
			
			if ((start = header.find('?')) != string::npos)
			{
				const string arguments(header.substr(start + 1));
				header = header.substr(0, start);
				StringMap m = getArgs(arguments);
				if (!m["user"].empty())
				{
					if (m["user"] == SETTING(WEBSERVER_USER) && m["pass"] == SETTING(WEBSERVER_PASS))
					{
						WebServerManager::getInstance()->login(IP);
						CurrentUser = WebServerManager::getInstance()->GetUserStatus(IP);
					}
					else if (m["user"] == SETTING(WEBSERVER_POWER_USER) && m["pass"] == SETTING(WEBSERVER_POWER_PASS))
					{
						WebServerManager::getInstance()->login(IP, true);
						CurrentUser = WebServerManager::getInstance()->GetUserStatus(IP);
					}
				}
				
				if (CurrentUser.isloggedin())
				{
					if (!m["search"].empty())
					{
						WebServerManager::getInstance()->search(Util::encodeURI(m["search"], true), Search::TypeModes(Util::toInt(m["type"])));
					} /*else {
                        WebServerManager::getInstance()->searchstarted(m["search_started"].empty()); // TODO
                    }*/
					if (!m["stop"].empty())
					{
						WebServerManager::getInstance()->reset();
					}
					if (!m["pagenum"].empty())
					{
						WebServerManager::getInstance()->ChangePage(Util::toInt(m["pagenum"]));
					}
					
					string dir;
					if (SETTING(WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR))
					{
						if (!m["dir"].empty())
						{
							dir = Util::encodeURI(m["dir"], true);
						}
					}
					
					if (!m["name"].empty())
					{
						const WebServerManager::searchresult toAdd = WebServerManager::getInstance()->GetSearchResult(Util::toInt(m["number"]));
						switch (Util::toInt(m["type"]))
						{
							case SearchResult::TYPE_DIRECTORY:
								QueueManager::getInstance()->addDirectory(Util::encodeURI(m["file"], true), HintedUser(toAdd.User, toAdd.HubURL), dir);
								break;
							case SearchResult::TYPE_FILE:
								const string name = Util::encodeURI(m["name"], true);
								const string DownloadName = !dir.empty() ? SETTING(DOWNLOAD_DIRECTORY) + name : name;
								QueueManager::getInstance()->add(0, DownloadName, Util::toInt64(m["size"]), TTHValue(m["tth"]), HintedUser(toAdd.User, toAdd.HubURL));
								break;
						}
					}
					if (!m["link"].empty())
					{
						string tmpL = Util::encodeURI(m["link"], true);
						if ((start = tmpL.find('?')) != string::npos)
						{
							// IRainman TODO: please refactoring me after rewrite magnet parsing mechanism.
							string arg = tmpL.substr(start + 1);
							tmpL = tmpL.substr(0, start);
							StringMap Link = getArgs(arg);
							if (Link["xt"].length() == 54)
							{
								TTHValue tth = TTHValue(Link["xt"].substr(15));
								
								string DownloadName = Link["dn"];
								if (!DownloadName.empty())
								{
									DownloadName = Util::encodeURI(DownloadName, true); // fix https://code.google.com/p/flylinkdc/issues/detail?id=1496
									if (!dir.empty())
									{
										File::addTrailingSlash(dir);
										File::ensureDirectory(dir);
										DownloadName = dir + DownloadName;
									}
									QueueManager::getInstance()->addFromWebServer(DownloadName, Util::toInt64(Link["xl"]), tth);
								}
							}
						}
					}
					if (!m["dqueue"].empty())
					{
						const string dqueue = Util::encodeURI(m["dqueue"], true);
						const string qfile = Util::encodeURI(m["qfile"], true);
						if (dqueue == STRING(REMOVE2))
						{
							QueueManager::getInstance()->removeTarget(qfile, false);
						}
						if (dqueue == STRING(SET_PRIORITY))
						{
							const string dp = Util::encodeURI(m["dp"], true);
							if (dp == STRING(AUTO))
								QueueManager::getInstance()->setAutoPriority(qfile, true);
							else if (dp == STRING(PAUSED))
								QueueManager::getInstance()->setPriority(qfile, QueueItem::PAUSED);
							else if (dp == STRING(LOWEST))
								QueueManager::getInstance()->setPriority(qfile, QueueItem::LOWEST);
							else if (dp == STRING(LOW))
								QueueManager::getInstance()->setPriority(qfile, QueueItem::LOW);
							else if (dp == STRING(NORMAL))
								QueueManager::getInstance()->setPriority(qfile, QueueItem::NORMAL);
							else if (dp == STRING(HIGH))
								QueueManager::getInstance()->setPriority(qfile, QueueItem::HIGH);
							else if (dp == STRING(HIGHEST))
								QueueManager::getInstance()->setPriority(qfile, QueueItem::HIGHEST);
						}
					}
					if (!m["refresh"].empty())
					{
						ShareManager::getInstance()->setDirty();
						ShareManager::getInstance()->refresh_share(true);
					}
					if (!m["purgetth"].empty())
					{
						ShareManager::getInstance()->setDirty();
						ShareManager::getInstance()->setPurgeTTH();
						ShareManager::getInstance()->refresh_share(true);
						LogManager::message(STRING(PURGE_TTH_DATABASE)); //[!]NightOrion(translate)
					}
					if (!m["webss"].empty() && !m["websps"].empty() &&
					        !m["upspeed"].empty() && !m["downspeed"].empty() &&
					        !m["upspeedt"].empty() && !m["downspeedt"].empty())
					{
						// [!] IRainman SpeedLimiter: to work correctly, you must first set the upload speed, and only then download speed!
						SET_SETTING(MAX_UPLOAD_SPEED_LIMIT_NORMAL, Util::toInt(m["upspeed"]));
						SET_SETTING(MAX_UPLOAD_SPEED_LIMIT_TIME, Util::toInt(m["upspeedt"]));
						SET_SETTING(MAX_DOWNLOAD_SPEED_LIMIT_NORMAL, Util::toInt(m["downspeed"]));
						SET_SETTING(MAX_DOWNLOAD_SPEED_LIMIT_TIME, Util::toInt(m["downspeedt"]));
						SET_SETTING(THROTTLE_ENABLE, !(m["speedlimit"].empty()));
						SET_SETTING(TIME_DEPENDENT_THROTTLE, !(m["speedlimitalt"].empty()));
						if (CurrentUser.ispower())
						{
							SET_SETTING(WEBSERVER_ALLOW_CHANGE_DOWNLOAD_DIR, !(m["alch_d_d"].empty()));
						}
						SET_SETTING(BANDWIDTH_LIMIT_START, Util::toInt(m["altspeedtimestart"]));
						SET_SETTING(BANDWIDTH_LIMIT_END, Util::toInt(m["altspeedtimestop"]));
						SET_SETTING(WEBSERVER_SEARCHSIZE, Util::toInt(m["webss"]));
						SET_SETTING(WEBSERVER_SEARCHPAGESIZE, Util::toInt(m["websps"]));
					}
				}
			}
			if (CurrentUser.isloggedin())
			{
				WebServerManager::getInstance()->getPage(header, IP, CurrentUser);
			}
			else
			{
				WebServerManager::getInstance()->getLoginPage(header);
			}
			::send(m_www_sock, header.c_str(), static_cast<int>(header.size()), 0);
			break;
		}
	}
	::closesocket(m_www_sock); // TOOD узнать зачем в веб сервере используется сырой сокет а не С++ обертка? и где ::shutdown(m_www_sock, SD_BOTH);?
	m_www_sock = INVALID_SOCKET;
	delete this;// Cleanup the thread object
	return 0;
}

void WebServerManager::search(string p_search_str, Search::TypeModes p_search_type)
{
	if (sended_search == false)
	{
		string::size_type i = 0;
		while ((i = p_search_str.find('+', i)) != string::npos)
		{
			p_search_str.replace(i++, 1, " ");
		}
		if (p_search_type == Search::TYPE_TTH)
		{
			p_search_str = g_tth + p_search_str; // [!] IRainman opt.
		}
		
		m_search_token = Util::rand();
		
		SearchManager::getInstance()->addListener(this);
		// TODO: Get ADC searchtype extensions if any is selected
		SearchParamTokenMultiClient l_search_param;
		l_search_param.m_filter = p_search_str;
		l_search_param.m_size_mode = Search::SIZE_DONTCARE;
		l_search_param.m_token = m_search_token;
		l_search_param.m_file_type = p_search_type;
		l_search_param.m_size = 0;
		l_search_param.m_owner = this;
		l_search_param.m_is_force_passive_searh = false;
		
		const uint64_t l_searchInterval = ClientManager::getInstance()->multi_search(l_search_param);
		search_delay = Util::toString(l_searchInterval / 1000 + 15);
		results.clear();
		SearchPages.clear();
		PageIndex = 0;
		row = 0;
		sended_search = true;
	}
}

void WebServerManager::on(SearchManagerListener::SR, const SearchResult& aResult) noexcept
{
	{
		CFlyFastLock(cs);
		if (!aResult.getToken() && m_search_token != aResult.getToken())
			return;
			
		if (row < static_cast<size_t>(SETTING(WEBSERVER_SEARCHSIZE)))
		{
			const string Row = Util::toString(row);
			const string User = aResult.getUser()->getLastNick();
//          string User = ClientManager::getNick(aResult->getUser()->getCID());
//          string User = ClientManager::getNicks(HintedUser(aResult->getUser(), aResult->getHubUrl()))[0];
			const string& File = aResult.getFile();
			const string FileName = aResult.getFileName();
			results += "<form method=get name='form" + Row + "' action='search.htm'>\n";
			results += "<tr class=search onclick='set_download_dir(\"form" + Row + "\")'>\n";
			switch (aResult.getType())
			{
				case SearchResult::TYPE_FILE:
				{
					const string TTH = aResult.getTTH().toBase32();
					const int64_t Size = aResult.getSize();
#ifdef _DEBUG_WEB_SERVER_
					results += "<td>" + Row + "</td>\n";
#endif
					results += "<td>" + User + "</td>\n<td>" + FileName + "</td>\n<td>" + Util::formatBytes(Size) + "</td>\n<td>" + TTH + "</td>\n";
					results += "<td><a href=\"magnet:?xt=urn:tree:tiger:" + TTH + "&xl" + Util::toString(Size) + "&dn" + FileName + "\" target=\"_blank\">" + STRING(OPEN) + "</a></td>\n</tr>\n";
					results += "<input type=hidden name=size value='" + Util::toString(Size) + "'/>";
					results += "<input type=hidden name=tth value='" + TTH + "'/>";
					break;
				}
				case SearchResult::TYPE_DIRECTORY:
				{
#ifdef _DEBUG_WEB_SERVER_
					results += "<td>" + Row + "</td>\n";
#endif
					results += "<td>" + User + "</td>\n<td>" + FileName + "</td>\n<td>" + STRING(DIRECTORY) + "</td>\n<td>" + File + "</td>\n</tr>\n";
					results += "<input type=hidden name=number value='" + Row + "'/>\n";
					AddSearchResult(row, aResult.getUser(), aResult.getHubUrl());
					break;
				}
			}
			results += "<input type=hidden name=dir/>";
			results += "<input type=hidden name=file value='" + File + "'/>";
			results += "<input type=hidden name=name value='" + FileName + "'/>";
			results += "<input type=hidden name=type value='" + Util::toString(aResult.getType()) + "'/>\n";
			results += "</form>\n";
			
			PageIndex = 1;
			WebServerManager::AddSearchPage(results, row / SETTING(WEBSERVER_SEARCHPAGESIZE) + 1);
			row++;
			if ((row % SETTING(WEBSERVER_SEARCHPAGESIZE)) == 0)
				results.clear();
		}
	}
}