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
#include "HttpConnection.h"
#include "LogManager.h"
#include "SettingsManager.h"


#ifdef RIP_USE_CORAL
static const std::string CORAL_SUFFIX = ".nyud.net";
#endif

/**
 * Downloads a file and returns it as a string
 * @todo Report exceptions
 * @todo Abort download
 * @param aUrl Full URL of file
 * @return A string with the content, or empty if download failed
 */
void HttpConnection::downloadFile(const string& aUrl, const string& aUserAgent)
{
	dcassert(!m_http_socket);
	if (m_http_socket) // [+] PPA fix
	{
		LogManager::getInstance()->message("HttpConnection::downloadFile \"" + aUrl + "\"- recursive call! Downloads cancelling...");
		dcassert(0);
		return;
	}
	dcassert(Util::isHttpLink(aUrl) || Util::isHttpsLink(aUrl));
	currentUrl = aUrl;
	m_userAgent = aUserAgent;
	// Trim spaces
	/* [-] IRainman: please check user input data. Don't use this cruth.
	while (currentUrl[0] == ' ')
	    currentUrl.erase(0, 1);
	while (currentUrl[currentUrl.length() - 1] == ' ')
	{
	    currentUrl.erase(currentUrl.length() - 1);
	}
	[!] IRainman: please check user input data. Don't use this cruth.*/
	// reset all settings (as in constructor), moved here from onLine(302) because ok was not reset properly
	moved302 = false;
	ok = false;
	size = -1;
	// set download type
	if (stricmp(currentUrl.substr(currentUrl.size() - 4).c_str(), ".bz2") == 0)
	{
		fire(HttpConnectionListener::TypeBZ2(), this);
	}
	else
	{
		fire(HttpConnectionListener::TypeNormal(), this);
	}
	
	bool isSecure = false;
	string proto, query, fragment;
	const string& l_httpProxyAdress = SETTING(HTTP_PROXY); // [+] IRainman opt.
	if (l_httpProxyAdress.empty())
	{
		Util::decodeUrl(currentUrl, proto, server, port, file, isSecure, query, fragment);
		if (file.empty())
			file = '/';
	}
	else
	{
		Util::decodeUrl(l_httpProxyAdress, proto, server, port, file, isSecure, query, fragment);
		file = currentUrl;
	}
	
	if (!query.empty())
		file += '?' + query;
		
#ifdef RIP_USE_CORAL
	if (BOOLSETTING(CORAL) && coralizeState != CST_NOCORALIZE)
	{
		{
			if (server.length() > CORAL_SUFFIX.length() && server.compare(server.length() - CORAL_SUFFIX.length(), CORAL_SUFFIX.length(), CORAL_SUFFIX) != 0)
			{
				server += CORAL_SUFFIX;
			}
			else
			{
				coralizeState = CST_NOCORALIZE;
			}
			
		}
	}
#endif
	
	if (port == 0)
		port = 80;
		
	if (!m_http_socket)
	{
		m_http_socket = BufferedSocket::getBufferedSocket(0x0a);
	}
	m_http_socket->addListener(this);
	try
	{
		m_http_socket->connect(server, port, isSecure, true, false);
	}
	catch (const Exception& e)
	{
		fire(HttpConnectionListener::Failed(), this, e.getError() + " (" + currentUrl + ")");
	}
}

void HttpConnection::on(BufferedSocketListener::Connected) noexcept
{
	dcassert(m_http_socket);
	if (!m_http_socket)
		return;
		
	m_http_socket->write("GET " + file + " HTTP/1.0\r\n"); // [!] FlylinkDC++ used HTTP 1.0, details here: https://code.google.com/p/flylinkdc/issues/detail?id=900
	// [!] FlylinkDC DHT hack.
	static const string g_flyUserAgent = "User-Agent: FlylinkDC++ " A_VERSIONSTRING
#ifdef FLYLINKDC_HE
	"HE"
#endif
	"\r\n";
	
	if (!m_userAgent.empty())
		m_http_socket->write("User-Agent:" + m_userAgent + " \r\n");
	else
		m_http_socket->write(g_flyUserAgent); // [!] Don't use APPNAME here. Needs for stats.
	// [~] FlylinkDC DHT hack.
	
	string sRemoteServer = server;
	if (!SETTING(HTTP_PROXY).empty())
	{
		string tfile, proto, query, fragment;
		uint16_t tport;
		Util::decodeUrl(file, proto, sRemoteServer, tport, tfile, query, fragment);
	}
	m_http_socket->write("Host: " + sRemoteServer + "\r\n");
	m_http_socket->write("Connection: close\r\n"); // we'll only be doing one request
	m_http_socket->write("Cache-Control: no-cache\r\n\r\n");
	
#ifdef RIP_USE_CORAL
	if (coralizeState == CST_DEFAULT) coralizeState = CST_CONNECTED;
#endif
}

void HttpConnection::on(BufferedSocketListener::Line, const string & aLine) noexcept
{
	if (!ok)
	{
		dcdebug("%s\n", aLine.c_str());
		if (aLine.find("200") == string::npos)
		{
			if (aLine.find("301") != string::npos || aLine.find("302") != string::npos)
			{
				moved302 = true;
			}
			else
			{
				socket_cleanup(false);
#ifdef RIP_USE_CORAL
				if (SETTING(CORAL) && coralizeState != CST_NOCORALIZE)
				{
					fire(HttpConnectionListener::Retried(), this, coralizeState == CST_CONNECTED);
					coralizeState = CST_NOCORALIZE;
					dcdebug("HTTP error with Coral, retrying : %s\n", currentUrl.c_str());
					downloadFile(currentUrl);
					return;
				}
#endif
				fire(HttpConnectionListener::Failed(), this, aLine + " (" + currentUrl + ")");
#ifdef RIP_USE_CORAL
				coralizeState = CST_DEFAULT;
#endif
				return;
			}
		}
		ok = true;
	}
	else if (moved302 && Util::findSubString(aLine, "Location") != string::npos)
	{
		dcassert(m_http_socket);
		socket_cleanup(false);
		string location302 = aLine.substr(10, aLine.length() - 11);
		// make sure we can also handle redirects with relative paths
		// if (Util::isHttpLink(location302)) [-] IRainman fix: support for part-time redirect URL.
		{
			if (location302[0] == '/') // [!] IRainman fix
			{
				string proto, query, fragment;
				Util::decodeUrl(currentUrl, proto, server, port, file, query, fragment);
				string tmp = "http://" + server;
				if (port != 80)
					tmp += ':' + Util::toString(port);
				location302 = tmp + location302;
			}
			else
			{
				string::size_type i = currentUrl.rfind('/');
				dcassert(i != string::npos);
				location302 = currentUrl.substr(0, i + 1) + location302;
			}
		}
		
		if (location302 == currentUrl)
		{
			fire(HttpConnectionListener::Failed(), this, "Endless redirection loop: " + currentUrl);
			return;
		}
		
		fire(HttpConnectionListener::Redirected(), this, location302);
		
#ifdef RIP_USE_CORAL
		coralizeState = CST_DEFAULT;
#endif
		downloadFile(location302);
		
	}
	else if (aLine == "\x0d")
	{
		m_http_socket->setDataMode(size);
	}
	else if (Util::findSubString(aLine, "Content-Length") != string::npos)
	{
		size = Util::toInt(aLine.substr(16, aLine.length() - 17));
	}
	else if (Util::findSubString(aLine, "Content-Encoding") != string::npos)
	{
		if (aLine.substr(18, aLine.length() - 19) == "x-bzip2")
			fire(HttpConnectionListener::TypeBZ2(), this);
	}
}

void HttpConnection::on(BufferedSocketListener::Failed, const string & aLine) noexcept
{
	socket_cleanup(false);
#ifdef RIP_USE_CORAL
	if (SETTING(CORAL) && coralizeState == CST_DEFAULT)
	{
		fire(HttpConnectionListener::Retried(), this, coralizeState == CST_CONNECTED);
		coralizeState = CST_NOCORALIZE;
		dcdebug("Coralized address failed, retrying : %s\n", currentUrl.c_str());
		downloadFile(currentUrl);
		return;
	}
	coralizeState = CST_DEFAULT;
#endif
	fire(HttpConnectionListener::Failed(), this, aLine + " (" + currentUrl + ")");
}

void HttpConnection::on(BufferedSocketListener::ModeChange) noexcept
{
	socket_cleanup(false);
	fire(HttpConnectionListener::Complete(), this, currentUrl
#ifdef RIP_USE_CORAL
	, BOOLSETTING(CORAL) && coralizeState != CST_NOCORALIZE
#endif
	    );
#ifdef RIP_USE_CORAL
	coralizeState = CST_DEFAULT;
#endif
}
void HttpConnection::on(BufferedSocketListener::Data, uint8_t * aBuf, size_t aLen) noexcept
{
	fire(HttpConnectionListener::Data(), this, aBuf, aLen);
}

/**
 * @file
 * $Id: HttpConnection.cpp 569 2011-07-25 19:48:51Z bigmuscle $
 */
