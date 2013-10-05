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

#ifndef DCPLUSPLUS_DCPP_HTTP_CONNECTION_H
#define DCPLUSPLUS_DCPP_HTTP_CONNECTION_H

#include "BufferedSocket.h"

// FlylinkDC++ Team TODO: http://code.google.com/p/flylinkdc/issues/detail?id=632 https://code.google.com/p/flylinkdc/issues/detail?id=900

class HttpConnection;

class HttpConnectionListener
{
	public:
		virtual ~HttpConnectionListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Data;
		typedef X<1> Failed;
		typedef X<2> Complete;
		typedef X<3> Redirected;
		typedef X<4> TypeNormal;
		typedef X<5> TypeBZ2;
#ifdef RIP_USE_CORAL
		typedef X<6> Retried;
#endif
		virtual void on(Data, HttpConnection*, const uint8_t*, size_t) noexcept = 0;
		virtual void on(Failed, HttpConnection*, const string&) noexcept { }
		virtual void on(Complete, HttpConnection*, const string&
#ifdef RIP_USE_CORAL
		                , bool
#endif
		               ) noexcept { }
		virtual void on(Redirected, HttpConnection*, const string&) noexcept { }
		virtual void on(TypeNormal, HttpConnection*) noexcept { }
		virtual void on(TypeBZ2, HttpConnection*) noexcept { }
#ifdef RIP_USE_CORAL
		virtual void on(Retried, HttpConnection*, const bool) noexcept { }
#endif
};

class HttpConnection : BufferedSocketListener, public Speaker<HttpConnectionListener>
#ifdef _DEBUG
	, boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		void downloadFile(const string& aUrl, const string& aUserAgent = Util::emptyString);
		HttpConnection() : ok(false), port(80), size(-1), moved302(false),
#ifdef RIP_USE_CORAL
			coralizeState(CST_DEFAULT),
#endif
			m_http_socket(nullptr) { }
		~HttpConnection() noexcept
		{
			socket_cleanup(true);
		}
#ifdef RIP_USE_CORAL
		enum CoralizeStates {CST_DEFAULT, CST_CONNECTED, CST_NOCORALIZE};
		void setCoralizeState(CoralizeStates _cor)
		{
			coralizeState = _cor;
		}
#endif
	private:
		void socket_cleanup(bool p_delete)
		{
			if (m_http_socket)
			{
				m_http_socket->removeListeners();
				m_http_socket->disconnect();
				BufferedSocket::putSocket(m_http_socket, p_delete);
			}
		}
		
		string currentUrl;
		string file;
		string server;
		string m_userAgent;
		bool ok;
		uint16_t port;
		int64_t size;
		bool moved302;
#ifdef RIP_USE_CORAL
		CoralizeStates coralizeState;
#endif
		BufferedSocket* m_http_socket;
		
		// BufferedSocketListener
		void on(Connected) noexcept;
		void on(Line, const string&) noexcept;
		void on(Data, uint8_t*, size_t) noexcept;
		void on(ModeChange) noexcept;
		void on(Failed, const string&) noexcept;
		
		void onConnected();
		void onLine(const string& aLine);
		
};

#endif // !defined(HTTP_CONNECTION_H)

/**
 * @file
 * $Id: HttpConnection.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
