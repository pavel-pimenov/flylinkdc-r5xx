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


#ifndef DCPLUSPLUS_DCPP_BUFFERED_SOCKET_H
#define DCPLUSPLUS_DCPP_BUFFERED_SOCKET_H

#include <atomic>
#include <boost/asio/ip/address_v4.hpp>

#include "BufferedSocketListener.h"
#include "Semaphore.h"
#include "Socket.h"

class UnZFilter;
class InputStream;
class UserConnection;
class BufferedSocket : public Speaker<BufferedSocketListener>, private Thread
{
	public:
		enum Modes
		{
			MODE_LINE,
			MODE_ZPIPE,
			MODE_DATA
		};
		
		enum NatRoles
		{
			NAT_NONE,
			NAT_CLIENT,
			NAT_SERVER
		};
		
		/**
		 * BufferedSocket factory, each BufferedSocket may only be used to create one connection
		 * @param sep Line separator
		 * @return An unconnected socket
		 */
		static BufferedSocket* getBufferedSocket(char sep, UserConnection* p_connection)
		{
			auto l_sock = new BufferedSocket(sep, p_connection);
			return l_sock;
		}
		
		static void putBufferedSocket(BufferedSocket*& p_sock, bool p_delete = false);
		
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
		static void waitShutdown();
#endif
		
		uint16_t accept(const Socket& srv, bool secure, bool allowUntrusted, const string& expKP = BaseUtil::emptyString);
		void connect(const string& aAddress, uint16_t aPort, bool secure,
		             bool allowUntrusted, bool proxy, Socket::Protocol p_proto, const string& expKP = BaseUtil::emptyString);
		void connect(const string& aAddress, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool secure,
		             bool allowUntrusted, bool proxy, Socket::Protocol p_proto, const string& expKP = BaseUtil::emptyString);
		             
		/** Sets data mode for aBytes bytes. Must be called within onLine. */
		void setDataMode(int64_t aBytes = -1)
		{
			m_mode = MODE_DATA;
			m_dataBytes = aBytes;
		}
		/**
		 * Rollback is an ugly hack to solve problems with compressed transfers where not all data received
		 * should be treated as data.
		 * Must be called from within onData.
		 */
		void setLineMode(size_t aRollback)
		{
			setMode(MODE_LINE, aRollback);
		}
		void setMode(Modes mode, size_t aRollback = 0);
		Modes getMode() const
		{
			return m_mode;
		}
		// [+] brain-ripper:
		// added check against sock pointer: isConnected was called from Client::on(Second, ...)
		// before Client::connect called (like thread race case).
		// Also added check to other functions just in case.
		bool isConnected() const
		{
			return hasSocket() && sock->isConnected();
		}
		bool isSecure() const
		{
			return hasSocket() && sock->isSecure();
		}
		bool isTrusted()
		{
			return hasSocket() && sock->isTrusted();
		}
		string getCipherName() const
		{
			return hasSocket() ? sock->getCipherName() : BaseUtil::emptyString;
		}
		string getIp() const
		{
			return hasSocket() ? sock->getIp() : BaseUtil::emptyString;
		}
		string getRemoteIpPort() const
		{
			return hasSocket() ? getIp() + ':' + Util::toString(getPort()) : BaseUtil::emptyString;
		}
		
		boost::asio::ip::address_v4 getIp4() const;
		const uint16_t getPort() const
		{
			return hasSocket() ? sock->getPort() : 0;
		}
		void setPort(uint16_t p_port)
		{
			if (hasSocket())
				sock->setPort(p_port); // https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=12555
		}
		vector<uint8_t> getKeyprint() const
		{
			return sock->getKeyprint();
		}
		bool verifyKeyprint(const string& expKP, bool allowUntrusted) noexcept {
			return sock->verifyKeyprint(expKP, allowUntrusted);
		}
		
		void setMaxSpeed(int64_t maxSpeed)
		{
			if (hasSocket())
				sock->setMaxSpeed(maxSpeed);
		}
		void updateSocketBucket(unsigned int p_numberOfUserConnection) const
		{
			if (hasSocket())
				sock->updateSocketBucket(p_numberOfUserConnection);
		}
		void write(const string& aData)
		{
			write(aData.data(), aData.length());
		}
		void write(const char* aBuf, size_t aLen);
		/** Send the file f over this socket. */
		void transmitFile(InputStream* f)
		{
			addTask(SEND_FILE, new SendFileInfo(f));
		}
		
		/** Send an updated signal to all listeners */
		void updated()
		{
			addTask(UPDATED, nullptr);
		}
		void initMyINFOLoader();
		void disconnect(bool graceless = false);
		
#ifdef FLYLINKDC_USE_DEAD_CODE
		string getLocalIp() const
		{
			return sock->getLocalIp();
		}
#endif
		
		uint16_t getLocalPort() const
		{
			return sock->getLocalPort();
		}
		bool hasSocket() const
		{
			return sock.get() != nullptr;
		}
		bool socketIsDisconnecting() const
		{
			return m_is_disconnecting || !hasSocket();
		}
		bool is_all_my_info_loaded() const
		{
			return m_is_all_my_info_loaded;
		}
		void set_all_my_info_loaded()
		{
			m_is_all_my_info_loaded = true;
		}
		void set_is_hide_share(bool p_is_hide_share)
		{
			m_is_hide_share = p_is_hide_share;
		}
	private:
		string getServerAndPort() const
		{
			return getIp() + ':' + Util::toString(getPort());
		}
		
		void all_myinfo_parser(const string::size_type p_pos_next_separator, const string& p_line, StringList& p_all_myInfo, bool p_is_zon);
		bool all_search_parser(const string::size_type p_pos_next_separator, const string& p_line,
		                       CFlySearchArrayTTH& p_tth_search,
		                       CFlySearchArrayFile& p_file_search);
		char m_separator;
		unsigned m_count_search_ddos;
		UserConnection* m_connection;
		
		enum Tasks
		{
			CONNECT,
			DISCONNECT,
			SEND_DATA,
			SEND_FILE,
			SHUTDOWN,
			ACCEPTED,
			UPDATED
		};
		
		enum State
		{
			RUNNING,
			STARTING, // Waiting for CONNECT/ACCEPTED/SHUTDOWN
			FAILED
		};
		
		struct TaskData
		{
			virtual ~TaskData() { }
		};
		struct ConnectInfo : public TaskData
		{
			explicit ConnectInfo(string addr_, uint16_t port_, uint16_t localPort_, NatRoles natRole_, bool proxy_) : addr(addr_), port(port_), localPort(localPort_), natRole(natRole_), proxy(proxy_) { }
			string addr;
			uint16_t port;
			uint16_t localPort;
			NatRoles natRole;
			bool proxy;
		};
		struct SendFileInfo : public TaskData
		{
			explicit SendFileInfo(InputStream* p_stream) : m_stream(p_stream) { }
			InputStream* m_stream;
		};
		
		explicit BufferedSocket(char aSeparator, UserConnection* p_connection);
		
		~BufferedSocket();
		
		FastCriticalSection cs;
		
		Semaphore m_socket_semaphore;
		std::deque<std::pair<Tasks, std::unique_ptr<TaskData> > > m_tasks;
		ByteVector m_inbuf;
		size_t m_myInfoCount; // ������� MyInfo
		bool   m_is_all_my_info_loaded;  // ���� �������� ������� �������� �� MyInfo (��������� �������� ������ �����������)
		bool   m_is_hide_share;
		void resizeInBuf();
		ByteVector m_writeBuf;
		string m_line;
		int64_t m_dataBytes;
		size_t m_rollback;
		
		Modes m_mode;
		State m_state;
		
		std::unique_ptr<UnZFilter> m_ZfilterIn;
		std::unique_ptr<Socket> sock;
		
		volatile bool m_is_disconnecting;
		
		int run() override;
		
		void threadConnect(const string& aAddr, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool proxy);
		void threadAccept();
		void threadRead();
		void threadSendFile(InputStream* is);
		void threadSendData();
		
		void fail(const string& aError);
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
		static std::atomic<long> g_sockets;
#endif
		bool checkEvents();
		void checkSocket();
		
		void setSocket(std::unique_ptr<Socket> && s);
		void shutdown();
		void addTask(Tasks task, TaskData* data);
		void addTaskL(Tasks task, TaskData* data);
	private:
		void BufferedSocket::parseMyINfo(StringList& p_all_myInfo);
		void BufferedSocket::parseSearch(CFlySearchArrayTTH& p_tth_search, CFlySearchArrayFile& p_file_search);
		
};

#endif // !defined(BUFFERED_SOCKET_H)

/**
 * @file
 * $Id: BufferedSocket.h 575 2011-08-25 19:38:04Z bigmuscle $
 */
