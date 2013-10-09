/*
 * Copyright (C) 2009-2011 Big Muscle, http://strongdc.sf.net
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

#include "stdafx.h"

#ifdef STRONG_USE_DHT

#include "UDPSocket.h"

#include "Constants.h"
#include "DHT.h"
#include "Utils.h"

#include "../client/AdcCommand.h"
#include "../client/ClientManager.h"
#include "../client/LogManager.h"
#include "../client/SettingsManager.h"

#include "../zlib/zlib.h"

#ifdef _WIN32
#include <mswsock.h>
#endif

#include <openssl/rc4.h>

namespace dht
{

#define BUFSIZE                 16384
#define MAGICVALUE_UDP          0x5b

UDPSocket::UDPSocket(void) : m_stop(false), port(0), delay(100)
#ifdef _DEBUG
	, m_sentBytes(0), m_receivedBytes(0), m_sentPackets(0), m_receivedPackets(0)
#endif
{
}

UDPSocket::~UDPSocket(void)
{
	disconnect();
	
	for_each(sendQueue.begin(), sendQueue.end(), DeleteFunction());
	
#ifdef _DEBUG
	dcdebug("DHT stats, received: %d bytes, sent: %d bytes\n", m_receivedBytes, m_sentBytes);
#endif
}

/*
 * Disconnects UDP socket
 */
void UDPSocket::disconnect() noexcept
{
	if (socket.get())
	{
		m_stop = true;
		socket->disconnect();
		port = 0;
		
		join();
		
		socket.reset();
		
		m_stop = false;
	}
}

/*
 * Starts listening to UDP socket
 */
void UDPSocket::listen() throw(SocketException)
{
	disconnect();
	
	try
	{
		socket.reset(new Socket);
		socket->create(Socket::TYPE_UDP);
		socket->setSocketOpt(SO_REUSEADDR, 1);
#ifdef FLYLINKDC_SUPPORT_WIN_XP
	    socket->setInBufSize();
#endif
		const string& l_bind = SETTING(BIND_ADDRESS);
		port = socket->bind(static_cast<uint16_t>(SETTING(DHT_PORT)), l_bind);
		
		start();
	}
	catch (...)
	{
		socket.reset();
		throw;
	}
}

void UDPSocket::checkIncoming() throw(SocketException)
{
	if (socket->wait(delay, Socket::WAIT_READ) == Socket::WAIT_READ)
	{
		sockaddr_in remoteAddr = { 0 };
		std::unique_ptr<uint8_t[]> buf(new uint8_t[BUFSIZE]);
		int len = socket->read(&buf[0], BUFSIZE, remoteAddr);
		// 2012-05-03_22-00-59_BXMHFQ4XUPHO3PGC3R7LOLCOCEBV57NUA63QOVA_AE6E2832_crash-stack-r502-beta24-build-9900.dmp
		// 2012-05-03_22-00-59_7E4TZRQNDSN3PPIVOY7FFMF3LDVYFBH27L7F6NI_9F52EA4D_crash-stack-r502-beta24-build-9900.dmp
		dcdrun(m_receivedBytes += len);
		dcdrun(m_receivedPackets++);
		
		if (len > 1)
		{
			bool isUdpKeyValid = false;
			if (buf[0] != ADC_PACKED_PACKET_HEADER && buf[0] != ADC_PACKET_HEADER)
			{
				// it seems to be encrypted packet
				if (!decryptPacket(&buf[0], len, inet_ntoa(remoteAddr.sin_addr), isUdpKeyValid))
					return;
			}
			//else
			//  return; // non-encrypted packets are forbidden
			
			unsigned long destLen = BUFSIZE; // what size should be reserved?
			std::unique_ptr<uint8_t[]> destBuf(new uint8_t[destLen]); //17662  [!] PVS V554    Incorrect use of unique_ptr. The memory allocated with 'new []' will be cleaned using 'delete'.
			if (buf[0] == ADC_PACKED_PACKET_HEADER) // is this compressed packet?
			{
				if (!decompressPacket(destBuf.get(), destLen, buf.get(), len))
					return;
			}
			else
			{
				memcpy(destBuf.get(), buf.get(), len);
				destLen = len;
			}
			
			// process decompressed packet
			string s((char*)destBuf.get(), destLen);
			if (s[0] == ADC_PACKET_HEADER && s[s.length() - 1] == ADC_PACKET_FOOTER) // is it valid ADC command?
			{
				string ip = inet_ntoa(remoteAddr.sin_addr);
				uint16_t port = ntohs(remoteAddr.sin_port);
				COMMAND_DEBUG(s.substr(0, s.length() - 1), DebugTask::HUB_IN,  ip + ':' + Util::toString(port));
				DHT::getInstance()->dispatch(s.substr(0, s.length() - 1), ip, port, isUdpKeyValid);
			}
			
			sleep(25);
		}
	}
}

void UDPSocket::checkOutgoing(uint64_t& timer) throw(SocketException)
{
	std::unique_ptr<Packet> packet;
	const uint64_t now = GET_TICK();
	
	{
		FastLock l(cs);
		
		size_t queueSize = sendQueue.size();
		if (queueSize && (now - timer > delay))
		{
			// take the first packet in queue
			packet.reset(sendQueue.front());
			sendQueue.pop_front();
			
			//dcdebug("Sending DHT %s packet: %d bytes, %d ms, queue size: %d\n", packet->cmdChar, packet->length, (uint32_t)(now - timer), queueSize);
			
			if (queueSize > 9)
				delay = 1000 / queueSize;
			timer = now;
		}
	}
	
	if (packet.get())
	{
		try
		{
			unsigned long length = compressBound(packet->data.length()) + 2; //-V614
			std::unique_ptr<uint8_t[]> data(new uint8_t[length]); // 17662   [!] PVS  V554    Incorrect use of unique_ptr. The memory allocated with 'new []' will be cleaned using 'delete'.
			// compress packet
			compressPacket(packet->data, data.get(), length);
			
			// encrypt packet
			encryptPacket(packet->targetCID, packet->udpKey, data.get(), length);
			dcdrun(m_sentBytes += packet->data.length());
			dcdrun(m_sentPackets++);
			socket->writeTo(packet->ip, packet->port, data.get(), length);
		}
		catch (const SocketException& e)
		{
			dcdebug("DHT::run Write error: %s\n", e.what());
		}
	}
}

/*
 * Thread for receiving UDP packets
 */
int UDPSocket::run()
{
#ifdef _WIN32
	// Try to avoid the Win2000/XP problem where recvfrom reports
	// WSAECONNRESET after sendto gets "ICMP port unreachable"
	// when sent to port that wasn't listening.
	// See MSDN - Q263823
	DWORD value = FALSE;
#ifndef SIO_UDP_CONNRESET
# define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR,12)
#endif
	ioctlsocket(socket->sock, SIO_UDP_CONNRESET, &value);
#endif
	
	// antiflood variables
	uint64_t timer = GET_TICK();
	
	while (!m_stop && !ClientManager::isShutdown())
	{
		try
		{
			if(ClientManager::isShutdown())
				break;
			// check outgoing queue
			checkOutgoing(timer);
			
			if(ClientManager::isShutdown())
				break;
			// check for incoming data
			checkIncoming();
		}
		catch (const SocketException& p_e)
		{
			dcdebug("DHT::run Error: %s\n", p_e.what());
			
			bool failed = false;
			while (!m_stop)
			{
				try
				{
					socket->disconnect();
					socket->create(Socket::TYPE_UDP);
#ifdef FLYLINKDC_SUPPORT_WIN_XP
					socket->setInBufSize();
#endif
					socket->setSocketOpt(SO_REUSEADDR, 1);
					socket->bind(port, SETTING(BIND_ADDRESS));
					if (failed)
					{
						LogManager::getInstance()->message(STRING(DHT_ENABLED));
						failed = false;
					}
					break;
				}
				catch (const SocketException& p_e2)
				{
					dcdebug("DHT::run Stopped listening: %s\n", p_e2.getError().c_str());
					
					if (!failed)
					{
						LogManager::getInstance()->message(STRING(DHT_DISABLED) + ": " + p_e2.getError());
						failed = true;
					}
					
					// Spin for 60 seconds
					for (int i = 0; i < 6000 && !m_stop && !ClientManager::isShutdown(); ++i)
					{
						sleep(10);
					}
				}
			}
		}
	}	
	return 0;
}

/*
 * Sends command to ip and port
 */
void UDPSocket::send(AdcCommand& cmd, const string& ip, uint16_t port, const CID& targetCID, const UDPKey& udpKey)
{
	// store packet for antiflooding purposes
	Utils::trackOutgoingPacket(ip, cmd);
	
	// pack data
	cmd.addParam("UK", Utils::getUdpKey(ip).toBase32()); // add our key for the IP address
	string command = cmd.toString(ClientManager::getMyCID()); // [!] IRainman fix.
	COMMAND_DEBUG(command, DebugTask::HUB_OUT, ip + ':' + Util::toString(port));
	
	Packet* p = new Packet(ip, port, command, targetCID, udpKey);
	
	FastLock l(cs);
	sendQueue.push_back(p);
}

void UDPSocket::compressPacket(const string& data, uint8_t* destBuf, unsigned long& destSize)
{
	int result = compress2(destBuf + 1, &destSize, (uint8_t*)data.data(), data.length(), Z_BEST_COMPRESSION);
	if (result == Z_OK && destSize <= data.length())
	{
		destBuf[0] = ADC_PACKED_PACKET_HEADER;
		destSize += 1;
	}
	else
	{
		// compression failed, send uncompressed packet
		destSize = data.length();
		memcpy(destBuf, (uint8_t*)data.data(), destSize);
		
		dcassert(destBuf[0] == ADC_PACKET_HEADER);
	}
}

void UDPSocket::encryptPacket(const CID& targetCID, const UDPKey& udpKey, uint8_t* destBuf, unsigned long& destSize)
{
#ifdef HEADER_RC4_H
	// generate encryption key
	TigerHash th;
	if (!udpKey.isZero())
	{
		th.update(udpKey.m_key.data(), sizeof(udpKey.m_key));
		th.update(targetCID.data(), sizeof(targetCID));
		
		RC4_KEY sentKey;
		RC4_set_key(&sentKey, TigerTree::BYTES, th.finalize());
		
		// encrypt data
		memmove(destBuf + 2, destBuf, destSize);
		
		// some random character except of ADC_PACKET_HEADER or ADC_PACKED_PACKET_HEADER
		uint8_t randomByte = static_cast<uint8_t>(Util::rand(0, 256));
		destBuf[0] = (randomByte == ADC_PACKET_HEADER || randomByte == ADC_PACKED_PACKET_HEADER) ? (randomByte + 1) : randomByte;
		destBuf[1] = MAGICVALUE_UDP;
		
		RC4(&sentKey, destSize + 1, destBuf + 1, destBuf + 1);
		destSize += 2;
	}
#endif
}
bool UDPSocket::decompressPacket(uint8_t* destBuf, unsigned long& destLen, const uint8_t* buf, size_t len)
{
	// decompress incoming packet
	int result = uncompress(destBuf, &destLen, buf + 1, len - 1);
	if (result != Z_OK)
	{
		// decompression error!!!
		return false;
	}
	
	return true;
}
bool UDPSocket::decryptPacket(uint8_t* buf, int& len, const string& remoteIp, bool& isUdpKeyValid)
{
#ifdef HEADER_RC4_H
	std::unique_ptr<uint8_t[]> destBuf(new uint8_t[len]);
	
	// the first try decrypts with our UDP key and CID
	// if it fails, decryption will happen with CID only
	int tries = 0;
	len -= 1;
	
	do
	{
		if (++tries == 3)
		{
			// decryption error, it could be malicious packet
			return false;
		}
		
		// generate key
		TigerHash th;
		if (tries == 1)
			th.update(Utils::getUdpKey(remoteIp).data(), sizeof(CID));
		th.update(ClientManager::getMyCID().data(), sizeof(CID)); // [!] IRainman fix.
		
		RC4_KEY recvKey;
		RC4_set_key(&recvKey, TigerTree::BYTES, th.finalize());
		
		// decrypt data
		RC4(&recvKey, len, buf + 1, &destBuf[0]);
	}
	while (destBuf[0] != MAGICVALUE_UDP);
	
	len -= 1;
	memcpy(buf, &destBuf[1], len);
	
	// if decryption was successful in first try, it happened via UDP key
	// it happens only when we sent our UDP key to this node some time ago
	if (tries == 1) isUdpKeyValid = true;
#endif
	return true;
}

}

#endif // STRONG_USE_DHT