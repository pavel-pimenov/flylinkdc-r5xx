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

#pragma once


#if !defined(SERVER_SOCKET_H)
#define SERVER_SOCKET_H

#include "Socket.h"
#include "Speaker.h"

class ServerSocketListener
{
	public:
		virtual ~ServerSocketListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> IncomingConnection;
		virtual void on(IncomingConnection) noexcept = 0;
};

class ServerSocket : public Speaker<ServerSocketListener>
#ifdef _DEBUG
	, boost::noncopyable
#endif
{
	public:
		ServerSocket() noexcept { }
		
		void listen(uint16_t port, const string& aIp);
		void disconnect()
		{
			socket.disconnect();
		}
		
		/** This is called by windows whenever an "FD_ACCEPT" is sent...doesn't work with unix... */
		void incoming()
		{
			fly_fire(ServerSocketListener::IncomingConnection());
		}
		
		socket_t getSock() const
		{
			return socket.m_sock;
		}
		operator const Socket&() const
		{
			return socket;
		}
	private:
	
		friend class Socket;
		friend class WebServerSocket;
		
		Socket socket;
};

#endif // !defined(SERVER_SOCKET_H)

/**
 * @file
 * $Id: ServerSocket.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
