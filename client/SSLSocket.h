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

#ifndef DCPLUSPLUS_DCPP_SSLSOCKET_H
#define DCPLUSPLUS_DCPP_SSLSOCKET_H

#include "Socket.h"
#include "Singleton.h"
#include "ssl.h"


#ifndef SSL_SUCCESS
#define SSL_SUCCESS 1
#endif

class SSLSocketException : public SocketException
{
	public:
#ifdef _DEBUG
	SSLSocketException(const string& aError) noexcept :
		SocketException("SSLSocketException: " + aError) { }
#else //_DEBUG
	SSLSocketException(const string& aError) noexcept :
		SocketException(aError) { }
#endif // _DEBUG
	SSLSocketException(DWORD aError) noexcept :
		SocketException(aError) { }
};

class CryptoManager;

class SSLSocket : public Socket
{
	public:
		~SSLSocket()
		{
			//[-] disconnect(); PPA fix: called in ~Socket()!
		}
		
		uint16_t accept(const Socket& listeningSocket);
		void connect(const string& aIp, uint16_t aPort);
		int read(void* aBuffer, int aBufLen);
		int write(const void* aBuffer, int aLen);
		int wait(uint64_t millis, int waitFor);
		void shutdown() noexcept;
		void close() noexcept;
		
		bool isSecure() const noexcept
		{
		    return true;
		}
		bool isTrusted() const noexcept;
		string getCipherName() const noexcept;
		vector<uint8_t> getKeyprint() const noexcept;
		
		bool waitConnected(uint64_t millis);
		bool waitAccepted(uint64_t millis);
		
	private:
		friend class CryptoManager;
		
		SSLSocket(SSL_CTX* context);
		
		SSL_CTX* ctx;
		ssl::SSL ssl;
		
#ifndef HEADER_OPENSSLV_H
		bool finished;
#endif
		
		int checkSSL(int ret);
		bool waitWant(int ret, uint64_t millis);
};

#endif // SSLSOCKET_H
