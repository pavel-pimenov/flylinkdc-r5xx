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
#include "ServerSocket.h"
#include "SettingsManager.h"

void ServerSocket::listen(uint16_t aPort, const string& aIp = SETTING(BIND_ADDRESS))
{
	if (socket.m_sock != INVALID_SOCKET)
	{
		socket.disconnect();
	}
	socket.create(Socket::TYPE_TCP);
	// Set reuse address option...
	socket.setSocketOpt(SO_REUSEADDR, 1);
	socket.bind(aPort, aIp);
	socket.listen();
}

/**
 * @file
 * $Id: ServerSocket.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */
