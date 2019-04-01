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


#ifndef DCPLUSPLUS_DCPP_SOCKET_H
#define DCPLUSPLUS_DCPP_SOCKET_H

#ifdef _WIN32

#include <ws2tcpip.h>
typedef int socklen_t;
typedef SOCKET socket_t;

#else

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

typedef int socket_t;
const int INVALID_SOCKET = -1;
#define SOCKET_ERROR -1
#endif

#include "SettingsManager.h"

class SocketException : public Exception
{
	public:
#ifdef _DEBUG
		explicit SocketException(const string& aError) noexcept :
			Exception("SocketException: " + aError), m_error_code(0) { }
#else //_DEBUG
		explicit SocketException(const string& aError) noexcept :
			Exception(aError), m_error_code(0) { }
#endif // _DEBUG
		explicit SocketException(DWORD aError) noexcept;
		DWORD getErrorCode() const
		{
			return m_error_code;
		}
	private:
		static string errorToString(int aError) noexcept;
		DWORD m_error_code;
};

class ServerSocket;

class Socket
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		enum
		{
			WAIT_NONE = 0x00,
			WAIT_CONNECT = 0x01,
			WAIT_READ = 0x02,
			WAIT_WRITE = 0x04 //-V112
		};
		
		enum SocketType
		{
			TYPE_TCP = 0, // IPPROTO_TCP
			TYPE_UDP = 1  // IPPROTO_UDP
		};

		enum Protocol
		{
			PROTO_DEFAULT = 0,
			PROTO_NMDC = 1,
			PROTO_ADC = 2
		};
		
		Socket() : m_sock(INVALID_SOCKET), connected(false)
			, m_maxSpeed(0), m_currentBucket(0) //[+] IRainman SpeedLimiter
			, m_type(TYPE_TCP), port(0)
			, m_proto(PROTO_DEFAULT)
		{
		}
		Socket(const string& aIp, uint16_t aPort) : m_sock(INVALID_SOCKET), connected(false)
			, m_maxSpeed(0), m_currentBucket(0) //[+] IRainman SpeedLimiter
			, m_type(TYPE_TCP)
			, m_proto(PROTO_DEFAULT)
		{
			connect(aIp, aPort);
		}
		virtual ~Socket()
		{
#ifdef _DEBUG
			if (m_sock != INVALID_SOCKET) // [+] PPA test
#endif
			{
				disconnect();
				m_sock = INVALID_SOCKET;
			}
		}
		
		/**
		 * Connects a socket to an address/ip, closing any other connections made with
		 * this instance.
		 * @param aAddr Server address, in dns or xxx.xxx.xxx.xxx format.
		 * @param aPort Server port.
		 * @throw SocketException If any connection error occurs.
		 */
		virtual void connect(const string& aIp, uint16_t aPort);
		void connect(const string& aIp, const string& aPort)
		{
			connect(aIp, static_cast<uint16_t>(Util::toInt(aPort)));
		}
		/**
		 * Same as connect(), but through the SOCKS5 server
		 */
		//[!]IRainman change uint32_t -> uint64_t
		// was the only time value in 32-bit format, all other values in Socket, BufferedSocket, SSLSocket the 64 bit
		void socksConnect(const string& aIp, uint16_t aPort, uint64_t timeout = 0);
		
		/**
		 * Sends data, will block until all data has been sent or an exception occurs
		 * @param aBuffer Buffer with data
		 * @param aLen Data length
		 * @throw SocketExcpetion Send failed.
		 */
		int writeAll(const void* aBuffer, int aLen, uint64_t timeout = 0);
		virtual int write(const void* aBuffer, int aLen);
		int write(const string& aData)
		{
			return write(aData.data(), (int)aData.length());
		}
		virtual int writeTo(const string& aIp, uint16_t aPort, const void* aBuffer, int aLen, bool proxy = true);
		int writeTo(const string& aIp, uint16_t aPort, const string& aData)
		{
			dcassert(aData.length());
			return writeTo(aIp, aPort, aData.data(), (int)aData.length());
		}
		virtual void shutdown() noexcept;
		virtual void close() noexcept;
		void disconnect() noexcept;
		
		virtual bool waitConnected(uint64_t millis);
		virtual bool waitAccepted(uint64_t millis);
		
		/**
		 * Reads zero to aBufLen characters from this socket,
		 * @param aBuffer A buffer to store the data in.
		 * @param aBufLen Size of the buffer.
		 * @return Number of bytes read, 0 if disconnected and -1 if the call would block.
		 * @throw SocketException On any failure.
		 */
		virtual int read(void* aBuffer, int aBufLen);
		/**
		 * Reads zero to aBufLen characters from this socket,
		 * @param aBuffer A buffer to store the data in.
		 * @param aBufLen Size of the buffer.
		 * @param aIP Remote IP address
		 * @return Number of bytes read, 0 if disconnected and -1 if the call would block.
		 * @throw SocketException On any failure.
		 */
		virtual int read(void* aBuffer, int aBufLen, sockaddr_in& remote);
		/**
		 * Reads data until aBufLen bytes have been read or an error occurs.
		 * If the socket is closed, or the timeout is reached, the number of bytes read
		 * actually read is returned.
		 * On exception, an unspecified amount of bytes might have already been read.
		 */
		int readAll(void* aBuffer, int aBufLen, uint64_t timeout = 0);
		
		virtual int wait(uint64_t millis, int waitFor);
		bool isConnected()
		{
			return connected;
		}
		
		static string resolve(const string& aDns);
		static uint32_t convertIP4(const string& p_ip)
		{
			UINT32 l_IP = inet_addr(p_ip.c_str());
			if (l_IP != INADDR_NONE)
				return ntohl(l_IP);
			return l_IP;
		}
		static string convertIP4(uint32_t p_ip)
		{
			uint32_t l_tmpIp = htonl(p_ip);
			return inet_ntoa(*(in_addr*)&l_tmpIp);
		}
		
#ifdef _WIN32
		void setBlocking(bool block) noexcept
		{
			u_long b = block ? 0 : 1;
			ioctlsocket(m_sock, FIONBIO, &b);
		}
#else
		void setBlocking(bool block) noexcept
		{
			int flags = fcntl(sock, F_GETFL, 0);
			if (block)
			{
				fcntl(sock, F_SETFL, flags & (~O_NONBLOCK));
			}
			else
			{
				fcntl(sock, F_SETFL, flags | O_NONBLOCK);
			}
		}
#endif
		
#ifdef FLYLINKDC_USE_DEAD_CODE
		string getLocalIp() const;
#endif
		static string getDefaultGateWay(boost::logic::tribool& p_is_wifi_router);
		uint16_t getLocalPort() const;
		
		// Low level interface
		virtual void create(SocketType aType = TYPE_TCP);
		
		/** Binds a socket to a certain local port and possibly IP. */
		virtual uint16_t bind(uint16_t aPort = 0, const string& aIp = "0.0.0.0");
		virtual void listen();
		/** Accept a socket.
		@return remote port */
		virtual uint16_t accept(const Socket& listeningSocket);
		
		int getSocketOptInt(int option) const;
		void setSocketOpt(int option, int value);
		void setInBufSize();
		void setOutBufSize();
		virtual bool isSecure() const noexcept
		{
			return false;
		}
		virtual bool isTrusted()
		{
			return false;
		}
		virtual string getCipherName() const noexcept
		{
			return Util::emptyString;
		}
		virtual vector<uint8_t> getKeyprint() const noexcept
		{
			return Util::emptyByteVector; // [!] IRainman opt.
		}
		virtual bool verifyKeyprint(const string&, bool /*allowUntrusted*/) noexcept {
			return true;
		}
		
		virtual std::string getEncryptionInfo() const noexcept
		{
			return Util::emptyString;
		}
		
		/** When socks settings are updated, this has to be called... */
		static void socksUpdated();
		static string getRemoteHost(const string& aIp);
		
		GETSET(string, ip, Ip);
		GETSET(uint16_t, port, Port);
		
		socket_t m_sock;
		Protocol m_proto;
		
		//[+] IRainman SpeedLimiter
		GETSET(int64_t, m_maxSpeed, MaxSpeed);
		GETSET(int64_t, m_currentBucket, CurrentBucket);
		void updateSocketBucket(unsigned int p_numberOfUserConnection)
		{
			m_currentBucket = getMaxSpeed() / p_numberOfUserConnection;
		}
		//[~] IRainman SpeedLimiter
		
	protected:
		socket_t getSock() const;
		
		SocketType m_type;
		bool connected;
		
		struct StatsItem
		{
			uint64_t totalDown;
			uint64_t totalUp;
			StatsItem() : totalDown(0), totalUp(0)
			{
			}
		};
		struct Stats
		{
			StatsItem m_tcp;
			StatsItem m_udp;
			StatsItem m_ssl;
		};
		
		static string g_udpServer;
		static uint16_t g_udpPort;
	public:
		static Stats g_stats;
	protected:
		static int getLastError()
		{
			return ::WSAGetLastError();
		}
	private:
		void socksAuth(uint64_t timeout);
		bool getLocalIPPort(uint16_t& p_port, string& p_ip, bool p_is_calc_ip) const;
		static socket_t checksocket(socket_t ret)
		{
			if (ret == SOCKET_ERROR)
			{
				throw SocketException(getLastError());
			}
			return ret;
		}
		static socket_t check(socket_t ret, bool blockOk = false)
		{
			if (ret == SOCKET_ERROR)
			{
				const int error = getLastError();
				if (blockOk && error == WSAEWOULDBLOCK)
				{
					return INVALID_SOCKET;
				}
				else
				{
					throw SocketException(error);
				}
			}
			return ret; // [12] Wizard https://www.box.net/shared/d8203c21d9e943b71cf5
		}
		static void check(int ret, bool blockOk = false) // [+] IRainman fix: Implementation of the function Socket::check - versatile, and allows for such use.
		{
			// [!] PVS V106 (Implicit type conversion first argument 'xxx' of function 'check' to memsize type).
			check(static_cast<socket_t>(ret), blockOk);
		}
		
};

#endif // DCPLUSPLUS_DCPP_SOCKET_H

/**
 * @file
 * $Id: Socket.h 573 2011-08-04 22:33:45Z bigmuscle $
 */
