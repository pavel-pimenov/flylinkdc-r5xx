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
#include "Socket.h"
#include "TimerManager.h"
#include "IpGuard.h"
#include "ResourceManager.h"
#include "CompatibilityManager.h"
#include <iphlpapi.h>

#include "../FlyFeatures/flyServer.h"

#pragma comment(lib, "iphlpapi.lib")


template<typename F> static auto airdc_check(F f, bool blockOk = false) -> decltype(f()) {
	for (;;) {
		auto ret = f();
		if (ret != static_cast<decltype(ret)>(SOCKET_ERROR)) {
			return ret;
		}
		
		auto error = ::WSAGetLastError();
		if (blockOk && error == WSAEWOULDBLOCK) {
			return static_cast<decltype(ret)>(-1);
		}
		
		if (error != EINTR) {
			throw SocketException(error);
		}
	}
}

inline int getSocketOptInt2(socket_t sock, int option) {
	int val;
	socklen_t len = sizeof(val);
	airdc_check([&] { return ::getsockopt(sock, SOL_SOCKET, option, (char*)&val, &len); });
	return val;
}

/// @todo remove when MinGW has this
#ifdef __MINGW32__
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL WSAEADDRNOTAVAIL
#endif
#endif

string Socket::g_udpServer;
uint16_t Socket::g_udpPort;

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

const uint64_t SOCKS_TIMEOUT = 30000;

string SocketException::errorToString(int aError) noexcept
{
	string msg = Util::translateError(aError);
	if (msg.empty())
	{
		char tmp[64];
		tmp[0] = 0;
		_snprintf(tmp, _countof(tmp), CSTRING(UNKNOWN_ERROR), aError);
		msg = tmp;
	}
	
	return msg;
}
socket_t Socket::getSock() const
{
	return m_sock;
}

void Socket::create(SocketType aType /* = TYPE_TCP */)
{
	//LogManager::message("[Create]");
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
	//LogManager::message("[accept]");
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
	const string l_remote_ip = inet_ntoa(sock_addr.sin_addr);
	IpGuard::check_ip_str(l_remote_ip, this);
	// Make sure we disable any inherited windows message things for this socket.
	::WSAAsyncSelect(m_sock, NULL, 0, 0);
	
	m_type = TYPE_TCP;
	
	// remote IP
	setIp(l_remote_ip);
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
	//LogManager::message("[bind] aAddr = " + aIp + " port = " + Util::toString(aPort));
	sockaddr_in sock_addr = { { 0 } };
	
	sock_addr.sin_family = AF_INET;
	sock_addr.sin_port = htons(aPort);
	sock_addr.sin_addr.s_addr = inet_addr(aIp.c_str());
	if (::bind(m_sock, (sockaddr *)&sock_addr, sizeof(sock_addr)) == SOCKET_ERROR)
	{
#ifdef _DEBUG
		const string l_error = Util::translateError();
		dcdebug("Bind failed, retrying with INADDR_ANY: %s\n", l_error.c_str()); //-V111
#endif
		sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		//TODO - обработать ошибку с 10048 - занят порт
		// - отключил спам
		// LogManager::message("uint16_t Socket::bind Error! IP = " + aIp + " aPort=" + Util::toString(aPort) + " Error = " + l_error);
		check(::bind(m_sock, (sockaddr *)&sock_addr, sizeof(sock_addr)));
	}
	const auto l_port = getLocalPort();
	return l_port;
}

void Socket::listen()
{
	//LogManager::message("[listen]");
	check(::listen(m_sock, 20));
	connected = true;
}

void Socket::connect(const string& aAddr, uint16_t aPort)
{
	//LogManager::message("[Connect] aAddr = " + aAddr + " port = " + Util::toString(aPort));
	sockaddr_in  serv_addr = { { 0 } };
	
	if (m_sock == INVALID_SOCKET)
	{
		create(TYPE_TCP);
	}
	
	const string l_addr = resolve(aAddr);
	
	memzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_port = htons(aPort);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(l_addr.c_str());
	string l_reason;
	if (IpGuard::check_ip_str(l_addr, l_reason))
	{
		throw SocketException(STRING(IPGUARD_BLOCK_LIST) + ": (" + aAddr + ") :" + l_reason);
	}
	int result;
	do
	{
		result = ::connect(m_sock, (struct sockaddr*) & serv_addr, sizeof(serv_addr));
	}
	while (result < 0 && getLastError() == EINTR);
	check(result, true);
	
	connected = true;
	setIp(l_addr);
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
	string l_reason;
	if (IpGuard::check_ip_str(resolve(aAddr), l_reason))
	{
		throw SocketException(STRING(IPGUARD_BLOCK_LIST) + ": (" + aAddr + ") :" + l_reason);
	}
	
	uint64_t start = GET_TICK();
	
	connect(SETTING(SOCKS_SERVER), static_cast<uint16_t>(SETTING(SOCKS_PORT)));
	
	if (!waitConnected(timeLeft(start, timeout)))
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
	int l_val = 0;
	socklen_t l_len = sizeof(l_val);
	check(::getsockopt(m_sock, SOL_SOCKET, p_option, (char*)&l_val, &l_len));
	return l_val;
}
void Socket::setSocketOpt(int option, int val)
{
	dcassert(val > 0);
	int len = sizeof(val); // x64 - x86 int разный размер
	check(::setsockopt(m_sock, SOL_SOCKET, option, (char*)&val, len));
}

int Socket::read(void* aBuffer, int aBufLen)
{
	int len = 0;
	
	dcassert(m_type == TYPE_TCP || m_type == TYPE_UDP);
	
	if (m_type == TYPE_TCP)
	{
		do
		{
			if (m_sock == INVALID_SOCKET)
				break;
				
			len = ::recv(m_sock, (char*)aBuffer, aBufLen, 0);
			
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
			if (m_sock == INVALID_SOCKET)
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
		if (m_sock == INVALID_SOCKET)
			break;
			
		len = ::recvfrom(m_sock, (char*)aBuffer, aBufLen, 0, (struct sockaddr*) & remote_addr, &addr_length);
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
			if (!wait(timeout, true, false).first)
			{
				return i;
			}
			continue;
		}
		dcassert(j > 0);
		i += j;
	}
	return i;
}

int Socket::writeAll(const void* aBuffer, int aLen, uint64_t timeout)
{
	const uint8_t* buf = (const uint8_t*)aBuffer;
	int pos = 0;
	// No use sending more than this at a time...
#if 0
	const int sendSize = getSocketOptInt(SO_SNDBUF);
#else
	const int sendSize = MAX_SOCKET_BUFFER_SIZE;
#endif
	
	while (pos < aLen)
	{
		const int i = write(buf + static_cast<size_t>(pos), (int)std::min(aLen - pos, sendSize)); // [!] PVS V104 Implicit conversion of 'pos' to memsize type in an arithmetic expression: buf + pos socket.cpp 464
		if (i == -1)
		{
			wait(timeout, false, true);
		}
		else
		{
			dcassert(i >= 0);
			pos += i;
		}
	}
	return pos;
}

int Socket::write(const void* aBuffer, int aLen)
{
	int sent = 0;
	do
	{
		if (m_sock == INVALID_SOCKET)
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
		// adguard.dll
		// dng.dll
		// NetchartFilter.dll!100168ab()
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
#ifdef _DEBUG
#endif
	if (aLen <= 0)
	{
		dcassert(0);
		return 0;
	}
	
	uint8_t* buf = (uint8_t*)aBuffer;
	if (m_sock == INVALID_SOCKET)
	{
		create(TYPE_UDP);
		setSocketOpt(SO_SNDTIMEO, 250);
#ifdef _DEBUG
		//LogManager::message("Create UDP socket! aAddr = " + aAddr + " aPort = " + Util::toString(aPort) + " aLen = " + Util::toString(aLen) + " Value:" + string((const char*)aBuffer, aLen));
#endif
	}
#ifdef _DEBUG
	else
	{
		if (m_type == TYPE_UDP)
		{
			//LogManager::message("Reuse UDP socket! aAddr = " + aAddr + " aPort = " + Util::toString(aPort) + " aLen = " + Util::toString(aLen) + " Value:" + string((const char*)aBuffer, aLen));
		}
	}
#endif
	dcassert(m_type == TYPE_UDP);
	
	sockaddr_in serv_addr  = { { 0 } };
	
	if (aAddr.empty() || aPort == 0)
	{
		//dcassert(0);
		throw SocketException(EADDRNOTAVAIL);
	}
	
	memzero(&serv_addr, sizeof(serv_addr));
	
	int sent;
	if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5 && proxy)
	{
		if (g_udpServer.empty() || g_udpPort == 0)
		{
			static bool g_is_first = false;
			if (!g_is_first)
			{
				// TODO g_is_first = true;
				Socket::socksUpdated();
			}
		}
		if (g_udpServer.empty() || g_udpPort == 0)
		{
			throw SocketException(STRING(SOCKS_SETUP_ERROR));
		}
		
		serv_addr.sin_port = htons(g_udpPort);
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = inet_addr(g_udpServer.c_str());
		
		
		vector<uint8_t> connStr;
		unsigned long addr;
		connStr.reserve(24 + static_cast<size_t>(aLen)); // [!] PVS V106 Implicit type conversion first argument '20 + aLen' of function 'reserve' to memsize type. socket.cpp 570
		
		connStr.push_back(0);       // Reserved
		connStr.push_back(0);       // Reserved
		connStr.push_back(0);       // Fragment number, always 0 in our case...
		
		if (BOOLSETTING(SOCKS_RESOLVE))
		{
			connStr.push_back(3);
			connStr.push_back((uint8_t)aAddr.size());
			connStr.insert(connStr.end(), aAddr.begin(), aAddr.end());
		}
		else
		{
			connStr.push_back(1);       // Address type: IPv4;
			addr = inet_addr(resolve(aAddr).c_str());
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
/**
 * Blocks until timeout is reached one of the specified conditions have been fulfilled
 * @param millis Max milliseconds to block.
 * @param checkRead Check for reading
 * @param checkWrite Check for writing
 * @return pair with read/write state respectively
 * @throw SocketException Select or the connection attempt failed.
 */
std::pair<bool, bool> Socket::wait(uint64_t millis, bool checkRead, bool checkWrite) {
	timeval tv;
	tv.tv_sec = static_cast<long>(millis / 1000);
	tv.tv_usec = (millis % 1000) * 1000;
	fd_set rfd, wfd;
	fd_set *rfdp = NULL, *wfdp = NULL;
	
	int nfds = -1;
	
	if (checkRead) {
		rfdp = &rfd;
		FD_ZERO(rfdp);
		if (m_sock != INVALID_SOCKET)
		{
			FD_SET(m_sock, &rfd);
			nfds = std::max((int)m_sock, nfds);
		}
		
//        if (sock6.valid()) {
//            FD_SET(sock6, &rfd);
//            nfds = std::max((int)sock6, nfds);
//        }
	}
	
	if (checkWrite) {
		wfdp = &wfd;
		FD_ZERO(wfdp);
		if (m_sock != INVALID_SOCKET) {
			FD_SET(m_sock, &wfd);
			nfds = std::max((int)m_sock, nfds);
		}
		
//        if (sock6.valid()) {
//            FD_SET(sock6, &wfd);
//            nfds = std::max((int)sock6, nfds);
//        }
	}
	
	airdc_check([&] { return ::select(nfds + 1, rfdp, wfdp, NULL, &tv); });
	
	
	return std::make_pair(
	           rfdp && ((m_sock != INVALID_SOCKET && FD_ISSET(m_sock, rfdp))),
	           wfdp && ((m_sock != INVALID_SOCKET && FD_ISSET(m_sock, wfdp))));
//    return std::make_pair(
//        rfdp && ((sock4.valid() && FD_ISSET(sock4, rfdp)) || (sock6.valid() && FD_ISSET(sock6, rfdp))),
//        wfdp && ((sock4.valid() && FD_ISSET(sock4, wfdp)) || (sock6.valid() && FD_ISSET(sock6, wfdp))));
}

bool Socket::waitConnected(uint64_t millis) {
	timeval tv;
	tv.tv_sec = static_cast<long>(millis / 1000);
	tv.tv_usec = (millis % 1000) * 1000;
	fd_set fd;
	FD_ZERO(&fd);
	
	int nfds = -1;
	if (m_sock != INVALID_SOCKET) {
		FD_SET(m_sock, &fd);
		nfds = static_cast<int>(m_sock);
	}
	
//    if (sock6.valid()) {
//        FD_SET(sock6, &fd);
//        nfds = std::max(static_cast<int>(sock6), nfds);
//    }

	airdc_check([&] { return ::select(nfds + 1, NULL, &fd, NULL, &tv); });
	
//    if (sock6.valid() && FD_ISSET(sock6, &fd)) {
//        int err6 = getSocketOptInt2(sock6, SO_ERROR);
//        if (err6 == 0) {
//            sock4.reset(); // We won't be needing this any more...
//            return true;
//        }
//
//        if (!sock4.valid()) {
//            throw SocketException(err6);
//        }
//
//        sock6.reset();
//    }

	if (m_sock != INVALID_SOCKET && FD_ISSET(m_sock, &fd)) {
		int err4 = getSocketOptInt2(m_sock, SO_ERROR);
		if (err4 == 0) {
//            sock6.reset(); // We won't be needing this any more...
			return true;
		}
		
//        if (!sock6.valid()) {
//            throw SocketException(err4);
//        }

		m_sock = INVALID_SOCKET; // .reset();
	}
	
	return false;
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
		//dcassert(g_last_resolve != aDns);
	}
	g_last_resolve = aDns;
#endif
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
			return BaseUtil::emptyString;
		}
		sock_addr.sin_addr.s_addr = *((uint32_t*)host->h_addr);
		return inet_ntoa(sock_addr.sin_addr);
	}
	else
	{
		return aDns;
	}
}
//============================================================================
string Socket::getDefaultGateWay(boost::logic::tribool& p_is_wifi_router)
{
	p_is_wifi_router = false;
	in_addr l_addr = {0};
	MIB_IPFORWARDROW ip_forward = {0};
	memset(&ip_forward, 0, sizeof(ip_forward));
	if (GetBestRoute(inet_addr("0.0.0.0"), 0, &ip_forward) == NO_ERROR)
	{
		l_addr = *(in_addr*)&ip_forward.dwForwardNextHop;
		const string l_ip_gateway = inet_ntoa(l_addr);
		if (l_ip_gateway == "192.168.1.1" ||
		        l_ip_gateway == "192.168.0.1" ||
		        l_ip_gateway == "192.168.88.1" || // http://www.lan23.ru/FAQ-Mikrotik-RouterOS-part1.html
		        l_ip_gateway == "192.168.33.1" || // http://www.speedguide.net/routers/huawei-ws323-300mbps-dual-band-wireless-range-extender-2951&print=friendly
		        l_ip_gateway == "192.168.0.50"    // http://nastroisam.ru/dap-1360-d1/#more-7437
		   )
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
bool Socket::getLocalIPPort(uint16_t& p_port, string& p_ip, bool p_is_calc_ip) const
{
	p_port = 0;
	p_ip.clear();
	if (m_sock == INVALID_SOCKET)
	{
		dcassert(m_sock != INVALID_SOCKET);
		return false;
	}
	sockaddr_in sock_addr = { { 0 } };
	socklen_t len = sizeof(sock_addr);
	if (getsockname(m_sock, (struct sockaddr*)&sock_addr, &len) == 0)
	{
		if (p_is_calc_ip)
		{
			p_ip = inet_ntoa(sock_addr.sin_addr);
			dcassert(!p_ip.empty());
		}
		else
		{
			p_port = ntohs(sock_addr.sin_port);
			dcassert(p_port);
		}
		return true;
	}
	else
	{
		const string l_error = "Error Socket::getLocalIPPort() ::WSAGetLastError() = " + Util::toString(::WSAGetLastError());
		LogManager::message(l_error);
		CFlyServerJSON::pushError(23, l_error);
	}
	dcassert(0);
	return false;
}
//============================================================================
#ifdef FLYLINKDC_USE_DEAD_CODE
string Socket::getLocalIp() const
{
	uint16_t p_port;
	string p_ip;
	getLocalIPPort(p_port, p_ip, true);
	return p_ip;
}
#endif
//============================================================================
uint16_t Socket::getLocalPort() const
{
	uint16_t p_port = 0;
	string p_ip;
	getLocalIPPort(p_port, p_ip, false);
	return p_port;
}
//============================================================================
void Socket::socksUpdated()
{
	g_udpServer.clear();
	g_udpPort = 0;
	
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
			*((unsigned long*)(&connStr[4])) = 0;  // No specific outgoing UDP address
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
			
			g_udpPort = static_cast<uint16_t>(ntohs(*((uint16_t*)(&connStr[8]))));
			
			in_addr serv_addr;
			
			memzero(&serv_addr, sizeof(serv_addr));
			serv_addr.s_addr = *((unsigned long*)(&connStr[4]));
			g_udpServer = inet_ntoa(serv_addr);
		}
		catch (const SocketException&)
		{
			dcdebug("Socket: Failed to register with socks server\n");
		}
	}
}

void Socket::shutdown() noexcept
{
	//dcassert(m_sock != INVALID_SOCKET);
	if (m_sock != INVALID_SOCKET)
	{
		::shutdown(m_sock, SD_BOTH); // !DC++!
	}
}

void Socket::close() noexcept
{
	//dcassert(m_sock != INVALID_SOCKET);
	if (m_sock != INVALID_SOCKET)
	{
		::closesocket(m_sock);
		connected = false;
		m_sock = INVALID_SOCKET;
	}
}

void Socket::disconnect() noexcept
{
	shutdown();
	close();
}

string Socket::getRemoteHost(const string& aIp)
{
	dcassert(!aIp.empty());
	if (aIp.empty())
		return BaseUtil::emptyString;
		
	const unsigned long addr = inet_addr(aIp.c_str());
	
	hostent *h = gethostbyaddr(reinterpret_cast<const char *>(&addr), 4, AF_INET); //-V112
	dcassert(h);
	if (h == nullptr)
	{
		return BaseUtil::emptyString;
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
