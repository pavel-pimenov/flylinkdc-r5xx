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
#include "Socket.h"
#include "TimerManager.h"
#include "IpGuard.h"
#include "LogManager.h"
#include "ResourceManager.h"
#include "CompatibilityManager.h"
#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi.lib")

/// @todo remove when MinGW has this
#ifdef __MINGW32__
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#endif
#endif

string Socket::udpServer;
uint16_t Socket::udpPort;

#ifdef _DEBUG

SocketException::SocketException(DWORD aError) noexcept
:
Exception("SocketException: " + errorToString(aError))
{
	m_error_code = aError;
	dcdebug("Thrown: %s\n", what()); //-V111
}

#else // _DEBUG

SocketException::SocketException(DWORD aError) noexcept :
Exception(errorToString(aError))
{
	m_error_code = aError;
}

#endif

Socket::Stats Socket::g_stats;

static const uint64_t SOCKS_TIMEOUT = 30000;

string SocketException::errorToString(int aError) noexcept
{
	string msg = Util::translateError(aError);
	if (msg.empty())
	{
		char tmp[64];
		tmp[0] = 0;
		snprintf(tmp, _countof(tmp), CSTRING(UNKNOWN_ERROR), aError);
		msg = tmp;
	}
	
	return msg;
}

void Socket::create(SocketType aType /* = TYPE_TCP */)
{
	if (m_sock != INVALID_SOCKET)
		disconnect();
		
	switch (aType)
	{
		case TYPE_TCP:
			m_sock = checksocket(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
			break;
		case TYPE_UDP:
			m_sock = checksocket(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
			break;
		default:
			dcassert(0);
	}
	m_type = aType;
	setBlocking(false);
}

uint16_t Socket::accept(const Socket& listeningSocket)
{
	if (m_sock != INVALID_SOCKET)
	{
		disconnect();
	}
	sockaddr_in sock_addr = { { 0 } }; // http://www.rsdn.ru/forum/cpp.applied/4054314.flat
	socklen_t sz = sizeof(sock_addr);
	
	do
	{
		m_sock = ::accept(listeningSocket.m_sock, (struct sockaddr*) & sock_addr, &sz);
	}
	while (m_sock == SOCKET_ERROR && getLastError() == EINTR);
	check(m_sock);
#ifdef PPA_INCLUDE_IPGUARD
	if (BOOLSETTING(ENABLE_IPGUARD))
		IpGuard::getInstance()->check(sock_addr.sin_addr.s_addr, this);
#endif
#ifdef _WIN32
	// Make sure we disable any inherited windows message things for this socket.
	::WSAAsyncSelect(m_sock, NULL, 0, 0);
#endif
	
	m_type = TYPE_TCP;
	
	// remote IP
	setIp(inet_ntoa(sock_addr.sin_addr));
	connected = true;
	setBlocking(false);
	
	// return the remote port
	if (sock_addr.sin_family == AF_INET)
	{
		return ntohs(sock_addr.sin_port);
	}
	/*
	if(sock_addr.sa.sa_family == AF_INET6)
	{
	    return ntohs(sock_addr.sai6.sin6_port);
	}*/
	
	return 0;
}


uint16_t Socket::bind(uint16_t aPort, const string& aIp /* = 0.0.0.0 */)
{
	sockaddr_in sock_addr = { { 0 } };
	
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(aPort);
	sock_addr.sin_addr.s_addr = inet_addr(aIp.c_str());
	if (::bind(m_sock, (sockaddr *)&sock_addr, sizeof(sock_addr)) == SOCKET_ERROR)
	{
		const string l_error = Util::translateError();
		dcdebug("Bind failed, retrying with INADDR_ANY: %s\n", l_error.c_str()); //-V111
		sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		//TODO - обработать ошибку с 10048 - занят порт
		// - отключил спам
		// LogManager::message("uint16_t Socket::bind Error! IP = " + aIp + " aPort=" + Util::toString(aPort) + " Error = " + l_error);
		check(::bind(m_sock, (sockaddr *)&sock_addr, sizeof(sock_addr)));
	}
	socklen_t size = sizeof(sock_addr);
	getsockname(m_sock, (struct sockaddr*)&sock_addr, (socklen_t*)&size);
	return ntohs(sock_addr.sin_port);
}

void Socket::listen()
{
	check(::listen(m_sock, 20));
	connected = true;
}

void Socket::connect(const string& aAddr, uint16_t aPort)
{
	sockaddr_in  serv_addr = { { 0 } };
	
	if (m_sock == INVALID_SOCKET)
	{
		create(TYPE_TCP);
	}
	
	string addr = resolve(aAddr);
	
	memzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_port = htons(aPort);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(addr.c_str());
#ifdef PPA_INCLUDE_IPGUARD
	if (BOOLSETTING(ENABLE_IPGUARD))
		IpGuard::getInstance()->check(serv_addr.sin_addr.s_addr);
#endif
	int result;
	do
	{
		result = ::connect(m_sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr));
	}
	while (result < 0 && getLastError() == EINTR);
	check(result, true);
	
	connected = true;
	setIp(addr);
	setPort(aPort);
}

static uint64_t timeLeft(uint64_t start, uint64_t timeout)
{
	if (timeout == 0)
	{
		return 0;
	}
	uint64_t now = GET_TICK();
	if (start + timeout < now)
		throw SocketException(STRING(CONNECTION_TIMEOUT));
	return start + timeout - now;
}

void Socket::socksConnect(const string& aAddr, uint16_t aPort, uint64_t timeout)
{
	if (SETTING(SOCKS_SERVER).empty() || SETTING(SOCKS_PORT) == 0)
	{
		throw SocketException(STRING(SOCKS_FAILED));
	}
#ifdef PPA_INCLUDE_IPGUARD
	if (BOOLSETTING(ENABLE_IPGUARD))
		IpGuard::getInstance()->check(inet_addr(resolve(aAddr).c_str()));
#endif
		
	uint64_t start = GET_TICK();
	
	connect(SETTING(SOCKS_SERVER), static_cast<uint16_t>(SETTING(SOCKS_PORT)));
	
	if (wait(timeLeft(start, timeout), WAIT_CONNECT) != WAIT_CONNECT)
	{
		throw SocketException(STRING(SOCKS_FAILED));
	}
	
	socksAuth(timeLeft(start, timeout));
	
	ByteVector connStr;
	
	// Authenticated, let's get on with it...
	connStr.push_back(5);           // SOCKSv5
	connStr.push_back(1);           // Connect
	connStr.push_back(0);           // Reserved
	
	if (BOOLSETTING(SOCKS_RESOLVE))
	{
		connStr.push_back(3);       // Address type: domain name
		connStr.push_back((uint8_t)aAddr.size());
		connStr.insert(connStr.end(), aAddr.begin(), aAddr.end());
	}
	else
	{
		connStr.push_back(1);       // Address type: IPv4;
		const unsigned long addr = inet_addr(resolve(aAddr).c_str());
		uint8_t* paddr = (uint8_t*) & addr;
		connStr.insert(connStr.end(), paddr, paddr + 4); //-V112
	}
	
	uint16_t l_port = htons(aPort);
	uint8_t* pport = (uint8_t*) & l_port;
	connStr.push_back(pport[0]);
	connStr.push_back(pport[1]);
	
	writeAll(&connStr[0], static_cast<int>(connStr.size()), timeLeft(start, timeout)); // [!] PVS V107 Implicit type conversion second argument 'connStr.size()' of function 'writeAll' to 32-bit type. socket.cpp 254
	
	// We assume we'll get a ipv4 address back...therefore, 10 bytes...
	/// @todo add support for ipv6
	if (readAll(&connStr[0], 10, timeLeft(start, timeout)) != 10)
	{
		throw SocketException(STRING(SOCKS_FAILED));
	}
	
	if (connStr[0] != 5 || connStr[1] != 0)
	{
		throw SocketException(STRING(SOCKS_FAILED));
	}
	
	in_addr sock_addr;
	
	memzero(&sock_addr, sizeof(sock_addr));
	sock_addr.s_addr = *((unsigned long*) & connStr[4]);
	setIp(inet_ntoa(sock_addr));
}

void Socket::socksAuth(uint64_t timeout)
{
	vector<uint8_t> connStr;
	
	uint64_t start = GET_TICK();
	
	if (SETTING(SOCKS_USER).empty() && SETTING(SOCKS_PASSWORD).empty())
	{
		// No username and pw, easier...=)
		connStr.push_back(5);           // SOCKSv5
		connStr.push_back(1);           // 1 method
		connStr.push_back(0);           // Method 0: No auth...
		
		writeAll(&connStr[0], 3, timeLeft(start, timeout));
		
		if (readAll(&connStr[0], 2, timeLeft(start, timeout)) != 2)
		{
			throw SocketException(STRING(SOCKS_FAILED));
		}
		
		if (connStr[1] != 0)
		{
			throw SocketException(STRING(SOCKS_NEEDS_AUTH));
		}
	}
	else
	{
		// We try the username and password auth type (no, we don't support gssapi)
		
		connStr.push_back(5);           // SOCKSv5
		connStr.push_back(1);           // 1 method
		connStr.push_back(2);           // Method 2: Name/Password...
		writeAll(&connStr[0], 3, timeLeft(start, timeout));
		
		if (readAll(&connStr[0], 2, timeLeft(start, timeout)) != 2)
		{
			throw SocketException(STRING(SOCKS_FAILED));
		}
		if (connStr[1] != 2)
		{
			throw SocketException(STRING(SOCKS_AUTH_UNSUPPORTED));
		}
		
		connStr.clear();
		// Now we send the username / pw...
		connStr.push_back(1);
		connStr.push_back((uint8_t)SETTING(SOCKS_USER).length());
		connStr.insert(connStr.end(), SETTING(SOCKS_USER).begin(), SETTING(SOCKS_USER).end());
		connStr.push_back((uint8_t)SETTING(SOCKS_PASSWORD).length());
		connStr.insert(connStr.end(), SETTING(SOCKS_PASSWORD).begin(), SETTING(SOCKS_PASSWORD).end());
		
		writeAll(&connStr[0], static_cast<int>(connStr.size()), timeLeft(start, timeout)); // [!] PVS V107 Implicit type conversion second argument 'connStr.size()' of function 'writeAll' to 32-bit type. socket.cpp 326
		
		if (readAll(&connStr[0], 2, timeLeft(start, timeout)) != 2)
		{
			throw SocketException(STRING(SOCKS_AUTH_FAILED));
		}
		
		if (connStr[1] != 0)
		{
			throw SocketException(STRING(SOCKS_AUTH_FAILED));
		}
	}
}

int Socket::getSocketOptInt(int p_option) const
{

#ifdef FLYLINKDC_HE
	int l_val;
#else
	int l_val = 0; //[!] 2012-04-23_22-28-18_L4N2H5DQSWJDZVGEWQRLCAQCSP3HVHJ3ZRWM73Q_05553A64_crash-stack-r501-build-9812.dmp
#endif
	socklen_t l_len = sizeof(l_val);
	check(::getsockopt(m_sock, SOL_SOCKET, p_option, (char*)&l_val, &l_len)); // [2] https://www.box.net/shared/3ad49dfa7f44028a7467
	/* [-] IRainman fix:
	Please read http://msdn.microsoft.com/en-us/library/windows/desktop/ms740532(v=vs.85).aspx
	and explain on what basis to audit https://code.google.com/p/flylinkdc/source/detail?r=9835 has been added to the magic check l_val <= 0,
	and the error in the log is sent to another condition l_val == 0.
	Just ask them to explain on what basis we do not trust the system,
	and why the system could restore us to waste in these places, but the api does not contain the test function of range.
	Once again I ask, please - if that's where you fell, you have to find real bugs in our code and not to mask them is not clear what the basis of checks.
	  [-] if (l_val <= 0)
	  [-]    throw SocketException("[Error] getSocketOptInt() <= 0");
	  [~] IRainman fix */
	return l_val;
}
#ifdef FLYLINKDC_SUPPORT_WIN_XP
void Socket::setInBufSize()
{
	if (!CompatibilityManager::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
	{
		const int l_sockInBuf = SETTING(SOCKET_IN_BUFFER);
		if (l_sockInBuf > 0)
			setSocketOpt(SO_RCVBUF, l_sockInBuf);
	}
}
void Socket::setOutBufSize()
{
	if (!CompatibilityManager::isOsVistaPlus()) // http://blogs.msdn.com/wndp/archive/2006/05/05/Winhec-blog-tcpip-2.aspx
	{
		const int l_sockOutBuf = SETTING(SOCKET_OUT_BUFFER);
		if (l_sockOutBuf > 0)
			setSocketOpt(SO_SNDBUF, l_sockOutBuf);
	}
}
#endif // FLYLINKDC_SUPPORT_WIN_XP
void Socket::setSocketOpt(int option, int val)
{
	dcassert(val > 0);
	int len = sizeof(val); // x64 - x86 int разный размер
	check(::setsockopt(m_sock, SOL_SOCKET, option, (char*)&val, len)); // [2] https://www.box.net/shared/57976d5de875f5b33516
}

int Socket::read(void* aBuffer, int aBufLen)
{
	int len = 0;
	
	dcassert(m_type == TYPE_TCP || m_type == TYPE_UDP);
	
	if (m_type == TYPE_TCP)
	{
		do
		{
			if (m_sock == INVALID_SOCKET)// [+]IRainman
				break;
				
			len = ::recv(m_sock, (char*)aBuffer, aBufLen, 0); // 2012-06-09_18-19-42_SQVQZUUAHG43VEDR2S7ZTUWUU4RK7JYLXQ3CQSY_EDA69E51_crash-stack-r501-x64-build-10294.dmp
			
#ifdef RIP_USE_LOG_PROTOCOL
			if (len > 0 && BOOLSETTING(LOG_PROTOCOL))
			{
				StringMap params;
				params["message"] = Util::toString(len) + " byte <- " + std::string((char*)aBuffer, len);
				LogManager::log(LogManager::PROTOCOL, params, true);
			}
#endif
		}
		while (len < 0 && getLastError() == EINTR);
	}
	else
	{
		do
		{
			if (m_sock == INVALID_SOCKET)// [+]IRainman
				break;
				
			len = ::recvfrom(m_sock, (char*)aBuffer, aBufLen, 0, NULL, NULL);
#ifdef RIP_USE_LOG_PROTOCOL
			if (len > 0 && BOOLSETTING(LOG_PROTOCOL))
			{
				StringMap params;
				params["message"] = Util::toString(len) + " UDP recvfrom byte <- " + std::string((char*)aBuffer, len);
				LogManager::log(LogManager::PROTOCOL, params, true);
			}
#endif
		}
		while (len < 0 && getLastError() == EINTR);
	}
	check(len, true);
	
	if (len > 0)
	{
		if (m_type == TYPE_UDP)
			g_stats.m_udp.totalDown += len;
		else
			g_stats.m_tcp.totalDown += len;
	}
	
	return len;
}

int Socket::read(void* aBuffer, int aBufLen, sockaddr_in &remote)
{
	dcassert(m_type == TYPE_UDP);
	
	sockaddr_in remote_addr = { 0 };
	socklen_t addr_length = sizeof(remote_addr);
	
	int len = 0;
	do
	{
		if (m_sock == INVALID_SOCKET)// [+]IRainman
			break;
			
		len = ::recvfrom(m_sock, (char*)aBuffer, aBufLen, 0, (struct sockaddr*) & remote_addr, &addr_length); // 2012-05-03_22-00-59_BXMHFQ4XUPHO3PGC3R7LOLCOCEBV57NUA63QOVA_AE6E2832_crash-stack-r502-beta24-build-9900.dmp
#ifdef RIP_USE_LOG_PROTOCOL
		if (len > 0 && BOOLSETTING(LOG_PROTOCOL))
		{
			StringMap params;
			params["message"] = Util::toString(len) + " UDP recvfrom byte <- " + std::string((char*)aBuffer, len);
			LogManager::log(LogManager::PROTOCOL, params, true);
		}
#endif
	}
	while (len < 0 && getLastError() == EINTR);
	
	check(len, true);
	if (len > 0)
	{
		if (m_type == TYPE_UDP)
			g_stats.m_udp.totalDown += len;
		else
			g_stats.m_tcp.totalDown += len;
	}
	remote = remote_addr;
	
	return len;
}

int Socket::readAll(void* aBuffer, int aBufLen, uint64_t timeout)
{
	uint8_t* buf = (uint8_t*)aBuffer;
	int i = 0;
	while (i < aBufLen)
	{
		const int j = read(buf + static_cast<size_t>(i), aBufLen - i); // [!] PVS V104 Implicit conversion of 'i' to memsize type in an arithmetic expression: buf + i socket.cpp 436
		if (j == 0)
		{
			return i;
		}
		else if (j == -1)
		{
			if (wait(timeout, WAIT_READ) != WAIT_READ)
			{
				return i;
			}
			continue;
		}
		dcassert(j > 0); // [+] IRainman fix.
		i += j;
	}
	return i;
}

int Socket::writeAll(const void* aBuffer, int aLen, uint64_t timeout)
{
	const uint8_t* buf = (const uint8_t*)aBuffer;
	int pos = 0;
	// No use sending more than this at a time...
#if 0 // fix http://code.google.com/p/flylinkdc/issues/detail?id=1333
	const int sendSize = getSocketOptInt(SO_SNDBUF);
#else
	const int sendSize = MAX_SOCKET_BUFFER_SIZE;
#endif
	
	while (pos < aLen)
	{
		const int i = write(buf + static_cast<size_t>(pos), (int)min(aLen - pos, sendSize)); // [!] PVS V104 Implicit conversion of 'pos' to memsize type in an arithmetic expression: buf + pos socket.cpp 464
		if (i == -1)
		{
			wait(timeout, WAIT_WRITE);
		}
		else
		{
			dcassert(i >= 0); // [+] IRainman fix.
			pos += i;
			// [-] IRainman fix: please see Socket::write
			// [-] g_stats.totalUp += i;
		}
	}
	return pos;
}

int Socket::write(const void* aBuffer, int aLen)
{
	int sent = 0;
	do
	{
		if (m_sock == INVALID_SOCKET)// [+]IRainman
			break;
			
#ifdef RIP_USE_LOG_PROTOCOL
		if (aLen > 0 && BOOLSETTING(LOG_PROTOCOL))
		{
			StringMap params;
			params["message"] = Util::toString(aLen) + " byte -> " + std::string((const char*)aBuffer, aLen);
			LogManager::log(LogManager::PROTOCOL, params, true);
		}
#endif
		sent = ::send(m_sock, (const char*)aBuffer, aLen, 0);
		// adguard.dll //[3] https://www.box.net/shared/cb7ec34c8cfac4b0b4a7
		// dng.dll
		// NetchartFilter.dll!100168ab() //[2] https://www.box.net/shared/007b54beb27139189267
		// 2012-04-27_18-43-09_UKBMIC5I554PHF57WL3PWXMD3XELARMMJ3JU3VA_FA08FF69_crash-stack-r502-beta22-build-9854.dmp
		// 2012-05-11_23-53-01_PIMG3OHBO7FMRNG7474ZB43CSELXW3U4A4G6LZI_2D14CD0B_crash-stack-r502-beta26-build-9946.dmp
	}
	while (sent < 0 && getLastError() == EINTR);
	
	check(sent, true);
	if (sent > 0)
	{
		if (m_type == TYPE_UDP)
			g_stats.m_udp.totalUp += sent;
		else
			g_stats.m_tcp.totalUp += sent;
	}
	return sent;
}

/**
* Sends data, will block until all data has been sent or an exception occurs
* @param aBuffer Buffer with data
* @param aLen Data length
* @throw SocketExcpetion Send failed.
*/
int Socket::writeTo(const string& aAddr, uint16_t aPort, const void* aBuffer, int aLen, bool proxy)
{
	if (aLen <= 0)
		return 0;
		
	uint8_t* buf = (uint8_t*)aBuffer;
	if (m_sock == INVALID_SOCKET)
	{
		create(TYPE_UDP);
		setSocketOpt(SO_SNDTIMEO, 250);
	}
	
	dcassert(m_type == TYPE_UDP);
	
	sockaddr_in serv_addr  = { { 0 } };
	
	if (aAddr.empty() || aPort == 0)
	{
		throw SocketException(EADDRNOTAVAIL);
	}
	
	memzero(&serv_addr, sizeof(serv_addr));
	
	int sent;
	if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5 && proxy)
	{
		if (udpServer.empty() || udpPort == 0)
		{
			throw SocketException(STRING(SOCKS_SETUP_ERROR));
		}
		
		serv_addr.sin_port = htons(udpPort);
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(udpServer.c_str());
		
		//[-] PVS-Studio V808 string s = BOOLSETTING(SOCKS_RESOLVE) ? resolve(ip) : ip;
		
		vector<uint8_t> connStr;
		
		connStr.reserve(20 + static_cast<size_t>(aLen)); // [!] PVS V106 Implicit type conversion first argument '20 + aLen' of function 'reserve' to memsize type. socket.cpp 570
		
		connStr.push_back(0);       // Reserved
		connStr.push_back(0);       // Reserved
		connStr.push_back(0);       // Fragment number, always 0 in our case...
		
		if (BOOLSETTING(SOCKS_RESOLVE))
		{
			connStr.push_back(3);
			connStr.push_back((uint8_t)aAddr.size()); //[+] aAddr SMT
			connStr.insert(connStr.end(), aAddr.begin(), aAddr.end());
		}
		else
		{
			connStr.push_back(1);       // Address type: IPv4;
			const unsigned long addr = inet_addr(resolve(aAddr).c_str());
			uint8_t* paddr = (uint8_t*) & addr;
			connStr.insert(connStr.end(), paddr, paddr + 4); //-V112
		}
		
		connStr.insert(connStr.end(), buf, buf + static_cast<size_t>(aLen)); // [!] PVS V104 Implicit conversion of 'aLen' to memsize type in an arithmetic expression: buf + aLen socket.cpp 590
		
		do
		{
			sent = ::sendto(m_sock, (const char*) & connStr[0], static_cast<int>(connStr.size()), 0, (struct sockaddr*) & serv_addr, sizeof(serv_addr)); // [!] PVS V107 Implicit type conversion third argument 'connStr.size()' of function 'sendto' to 32-bit type. socket.cpp 594
		}
		while (sent < 0 && getLastError() == EINTR);
	}
	else
	{
		serv_addr.sin_port = htons(aPort);
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(resolve(aAddr).c_str());
		do
		{
			sent = ::sendto(m_sock, (const char*)aBuffer, (int)aLen, 0, (struct sockaddr*) & serv_addr, sizeof(serv_addr));
		}
		while (sent < 0 && getLastError() == EINTR);
	}
	
	check(sent);
	if (m_type == TYPE_UDP)
		g_stats.m_udp.totalUp += sent;
	else
		g_stats.m_tcp.totalUp += sent;
	return sent;
}

/**
 * Blocks until timeout is reached one of the specified conditions have been fulfilled
 * @param millis Max milliseconds to block.
 * @param waitFor WAIT_*** flags that set what we're waiting for, set to the combination of flags that
 *                triggered the wait stop on return (==WAIT_NONE on timeout)
 * @return WAIT_*** ored together of the current state.
 * @throw SocketException Select or the connection attempt failed.
 */
int Socket::wait(uint64_t millis, int waitFor)
{
	timeval tv;
	fd_set rfd, wfd, efd;
	fd_set *rfdp = nullptr, *wfdp = nullptr;
	tv.tv_sec = static_cast<long>(millis / 1000);// [!] IRainman fix this fild in timeval is a long type (PVS TODO)
	tv.tv_usec = static_cast<long>((millis % 1000) * 1000);// [!] IRainman fix this fild in timeval is a long type (PVS TODO)
	
	if (waitFor & WAIT_CONNECT)
	{
		dcassert(!(waitFor & WAIT_READ) && !(waitFor & WAIT_WRITE));
		
		int result = -1;
		do
		{
			FD_ZERO(&wfd);
			FD_ZERO(&efd);
			
			FD_SET(m_sock, &wfd);
			FD_SET(m_sock, &efd);
			result = select((int)(m_sock + 1), 0, &wfd, &efd, &tv);
		}
		while (result < 0 && getLastError() == EINTR);
		check(result);
		
		if (FD_ISSET(m_sock, &wfd))
		{
			return WAIT_CONNECT;
		}
		
		if (FD_ISSET(m_sock, &efd))
		{
			int y = 0;
			socklen_t z = sizeof(y);
			check(getsockopt(m_sock, SOL_SOCKET, SO_ERROR, (char*)&y, &z));
			
			if (y != 0)
				throw SocketException(y);
			// No errors! We're connected (?)...
			return WAIT_CONNECT;
		}
		return 0;
	}
	
	int result = -1;
	do
	{
		if (waitFor & WAIT_READ)
		{
			dcassert(!(waitFor & WAIT_CONNECT));
			rfdp = &rfd;
			FD_ZERO(rfdp);
			FD_SET(m_sock, rfdp);
		}
		if (waitFor & WAIT_WRITE)
		{
			dcassert(!(waitFor & WAIT_CONNECT));
			wfdp = &wfd;
			FD_ZERO(wfdp);
			FD_SET(m_sock, wfdp);
		}
		
		result = select((int)(m_sock + 1), rfdp, wfdp, NULL, &tv); //[1] https://www.box.net/shared/03ae4d0b4586cea0a305
	}
	while (result < 0 && getLastError() == EINTR);
	check(result);
	
	waitFor = WAIT_NONE;
	
	dcassert(m_sock != INVALID_SOCKET); // https://github.com/eiskaltdcpp/eiskaltdcpp/commit/b031715
	if (m_sock != INVALID_SOCKET)
	{
		if (rfdp && FD_ISSET(m_sock, rfdp)) // https://www.box.net/shared/t3apqdurqxzicy4bg1h0
		{
			waitFor |= WAIT_READ;
		}
		if (wfdp && FD_ISSET(m_sock, wfdp))
		{
			waitFor |= WAIT_WRITE;
		}
	}
	return waitFor;
}

bool Socket::waitConnected(uint64_t millis)
{
	return wait(millis, Socket::WAIT_CONNECT) == WAIT_CONNECT;
}

bool Socket::waitAccepted(uint64_t /*millis*/)
{
	// Normal sockets are always connected after a call to accept
	return true;
}

string Socket::resolve(const string& aDns)
{
#ifdef _DEBUG
	static string g_last_resolve;
	if (!g_last_resolve.empty())
	{
		// TODO dcassert(g_last_resolve != aDns);
	}
	g_last_resolve = aDns;
#endif
#ifdef _WIN32
	sockaddr_in sock_addr  = { { 0 } };
	
	memzero(&sock_addr, sizeof(sock_addr));
	sock_addr.sin_port = 0;
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_addr.s_addr = inet_addr(aDns.c_str());
	
	if (sock_addr.sin_addr.s_addr == INADDR_NONE)     /* server address is a name or invalid */
	{
		hostent* host;
		host = gethostbyname(aDns.c_str());
		if (host == NULL)
		{
			return Util::emptyString;
		}
		sock_addr.sin_addr.s_addr = *((uint32_t*)host->h_addr);
		return inet_ntoa(sock_addr.sin_addr);
	}
	else
	{
		return aDns;
	}
#else
	// POSIX doesn't guarantee the gethostbyname to be thread safe. And it may (will) return a pointer to static data.
	string address;
	addrinfo hints = { 0 };
	addrinfo *result;
	hints.ai_family = AF_INET;
	
	if (getaddrinfo(aDns.c_str(), NULL, &hints, &result) == 0)
	{
		if (result->ai_addr != NULL)
			address = inet_ntoa(((sockaddr_in*)(result->ai_addr))->sin_addr);
	
		freeaddrinfo(result);
	}
	
	return address;
#endif
}

string Socket::getLocalIp() const noexcept
{
    dcassert(m_sock != INVALID_SOCKET);
    if (m_sock == INVALID_SOCKET)
{
return Util::emptyString;
}
sockaddr_in sock_addr  = { { 0 } };
socklen_t len = sizeof(sock_addr);
if (getsockname(m_sock, (struct sockaddr*)&sock_addr, &len) == 0)
{
return inet_ntoa(sock_addr.sin_addr);
}
return Util::emptyString;
}
//============================================================================
string Socket::getDefaultGateWay(bool& p_is_wifi_router)
{
	p_is_wifi_router = false;
	in_addr l_addr = {0};
	MIB_IPFORWARDROW ip_forward = {0};
	memset(&ip_forward, 0, sizeof(ip_forward));
	if (GetBestRoute(inet_addr("0.0.0.0"), 0, &ip_forward) == NO_ERROR)
	{
		l_addr = *(in_addr*)&ip_forward.dwForwardNextHop;
		const string l_ip_gateway = inet_ntoa(l_addr);
		if (l_ip_gateway == "192.168.1.1" || l_ip_gateway == "192.168.0.1")
		{
			p_is_wifi_router = true;
		}
		return l_ip_gateway;
	}
	return "";
	
#if 0
	// Get local host name
	char szHostName[128] = {0};
	
	if (gethostname(szHostName, sizeof(szHostName)))
	{
		// Error handling -> call 'WSAGetLastError()'
	}
	
	SOCKADDR_IN socketAddress;
	hostent *pHost        = 0;
	
	// Try to get the host ent
	pHost = gethostbyname(szHostName);
	if (!pHost)
	{
		// Error handling -> call 'WSAGetLastError()'
	}
	
	char ppszIPAddresses[10][16]; // maximum of ten IP addresses
	for (int iCnt = 0; (pHost->h_addr_list[iCnt]) && (iCnt < 10); ++iCnt)
	{
		memcpy(&socketAddress.sin_addr, pHost->h_addr_list[iCnt], pHost->h_length);
		strcpy(ppszIPAddresses[iCnt], inet_ntoa(socketAddress.sin_addr));
		
		//printf("Found interface address: %s\n", ppszIPAddresses[iCnt]);
	}
	return "";
#endif
}
//============================================================================
uint16_t Socket::getLocalPort() noexcept
{
	if (m_sock == INVALID_SOCKET)
		return 0;
		
	sockaddr_in sock_addr = { { 0 } };
	socklen_t len = sizeof(sock_addr);
	if (getsockname(m_sock, (struct sockaddr*)&sock_addr, &len) == 0)
	{
		return ntohs(sock_addr.sin_port);
	}
	return 0;
}

void Socket::socksUpdated()
{
	udpServer.clear();
	udpPort = 0;
	
	if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)
	{
		try
		{
			Socket s;
			s.setBlocking(false);
			s.connect(SETTING(SOCKS_SERVER), static_cast<uint16_t>(SETTING(SOCKS_PORT)));
			s.socksAuth(SOCKS_TIMEOUT);
			
			char connStr[10];
			connStr[0] = 5;         // SOCKSv5
			connStr[1] = 3;         // UDP Associate
			connStr[2] = 0;         // Reserved
			connStr[3] = 1;         // Address type: IPv4;
			*((unsigned long*)(&connStr[4])) = 0;  // No specific outgoing UDP address // [!] IRainman fix. this value unsigned!
			*((uint16_t*)(&connStr[8])) = 0;    // No specific port...
			
			s.writeAll(connStr, 10, SOCKS_TIMEOUT);
			
			// We assume we'll get a ipv4 address back...therefore, 10 bytes...if not, things
			// will break, but hey...noone's perfect (and I'm tired...)...
			if (s.readAll(connStr, 10, SOCKS_TIMEOUT) != 10)
			{
				return;
			}
			
			if (connStr[0] != 5 || connStr[1] != 0)
			{
				return;
			}
			
			udpPort = static_cast<uint16_t>(ntohs(*((uint16_t*)(&connStr[8]))));
			
			in_addr serv_addr;
			
			memzero(&serv_addr, sizeof(serv_addr));
			serv_addr.s_addr = *((unsigned long*)(&connStr[4])); // [!] IRainman fix. this value unsigned! (PVS TODO)
			udpServer = inet_ntoa(serv_addr);
		}
		catch (const SocketException&)
		{
			dcdebug("Socket: Failed to register with socks server\n");
		}
	}
}

void Socket::shutdown() noexcept
{
	dcassert(m_sock != INVALID_SOCKET); // Поймаем под отладкой двойной shutdown? L: нет не поймаем, INVALID_SOCKET это тоже нормальное состояние сокета, который мог стать не валидным в процессе работы в результате какой либо сетевой ошибки.
	if (m_sock != INVALID_SOCKET)
	{
		::shutdown(m_sock, SD_BOTH); // !DC++!
	}
}

void Socket::close() noexcept
{
	dcassert(m_sock != INVALID_SOCKET); // Поймаем под отладкой двойное закрытие? L: нет не поймаем, INVALID_SOCKET это тоже нормальное состояние сокета, который мог стать не валидным в процессе работы в результате какой либо сетевой ошибки.
	if (m_sock != INVALID_SOCKET)
	{
#ifdef _WIN32
		::closesocket(m_sock);
#else
		::close(sock);
#endif
		connected = false;
		m_sock = INVALID_SOCKET;
	}
}

void Socket::disconnect()
{
	shutdown();
	close();
}

string Socket::getRemoteHost(const string& aIp)
{
	dcassert(!aIp.empty());
	if (aIp.empty())
		return Util::emptyString;
		
	const unsigned long addr = inet_addr(aIp.c_str());
	
	hostent *h = gethostbyaddr(reinterpret_cast<const char *>(&addr), 4, AF_INET); //-V112
	dcassert(h);
	if (h == nullptr)
	{
		return Util::emptyString;
	}
	else
	{
		return h->h_name;
	}
}

/**
 * @file
 * $Id: Socket.cpp 581 2011-11-02 18:59:46Z bigmuscle $
 */
