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

#ifndef DCPLUSPLUS_DCPP_BUFFERED_SOCKET_H
#define DCPLUSPLUS_DCPP_BUFFERED_SOCKET_H

#include <boost/atomic.hpp>
#include <boost/asio/ip/address_v4.hpp>

#include "BufferedSocketListener.h"
#include "Semaphore.h"
#include "Socket.h"

class BufferedSocket : public Speaker<BufferedSocketListener>, private BASE_THREAD
#ifdef _DEBUG
	, virtual NonDerivable<BufferedSocket> // [+] IRainman fix.
#endif
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
		static BufferedSocket* getBufferedSocket(char sep)
		{
			return new BufferedSocket(sep);
		}
		
		static void putBufferedSocket(BufferedSocket*& p_sock, bool p_delete = false)
		{
			if (p_sock)
			{
				p_sock->shutdown();
				if (p_delete)
				{
					delete p_sock;
				}
				p_sock = nullptr;
			}
		}
		
		static void waitShutdown()
		{
			int l_max_count = 500;
			while (g_sockets > 0 && --l_max_count)
			{
				sleep(10); // TODO - Если слишком долго ждем. спросить диалогом и если ответят "да" - закрыться
				// TODO - случай зависания передать на флай-сервер.
			}
		}
		
		uint16_t accept(const Socket& srv, bool secure, bool allowUntrusted);
		void connect(const string& aAddress, uint16_t aPort, bool secure, bool allowUntrusted, bool proxy);
		void connect(const string& aAddress, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool secure, bool allowUntrusted, bool proxy);
		
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
		bool isTrusted() const
		{
			return hasSocket() && sock->isTrusted();
		}
		string getCipherName() const
		{
			return hasSocket() ? sock->getCipherName() : Util::emptyString;
		}
		const string& getIp() const
		{
			return hasSocket() ? sock->getIp() : Util::emptyString;
		}
		boost::asio::ip::address_v4 getIp4() const
		{
			if (hasSocket())
			{
				boost::system::error_code ec;
				const auto l_ip = boost::asio::ip::address_v4::from_string(sock->getIp(), ec); // TODO - конвертнуть IP и в сокетах
				dcassert(!ec);
				return l_ip;
			}
			else
			{
				return boost::asio::ip::address_v4();
			}
		}
		const uint16_t getPort()
		{
			return hasSocket() ? sock->getPort() : 0;
		}
		void setPort(uint16_t p_port)
		{
			if (hasSocket())
				sock->setPort(p_port); // https://crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=12555
		}
		vector<uint8_t> getKeyprint() const
		{
			return sock->getKeyprint();
		}
		//[+]IRainman SpeedLimiter
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
		//[~]IRainman SpeedLimiter
		void write(const string& aData)
		{
			write(aData.data(), aData.length());
		}
		void write(const char* aBuf, size_t aLen) noexcept;
		/** Send the file f over this socket. */
		void transmitFile(InputStream* f)
		{
			FastLock l(cs);
			addTask(SEND_FILE, new SendFileInfo(f));
		}
		
		/** Send an updated signal to all listeners */
		void updated()
		{
			FastLock l(cs);
			addTask(UPDATED, nullptr);
		}
		
		void disconnect(bool graceless = false) noexcept
		{
			FastLock l(cs);
			if (graceless)
				m_is_disconnecting = true;
				
			addTask(DISCONNECT, nullptr);
		}
		
		string getLocalIp() const
		{
			return sock->getLocalIp();
		}
		uint16_t getLocalPort() const
		{
			return sock->getLocalPort();
		}
		bool hasSocket() const
		{
			return sock.get() != nullptr;
		}
		bool socketIsDisconecting() const // [+] IRainman fix
		{
			return m_is_disconnecting || !hasSocket();
		}
		bool is_all_my_info_loaded() const
		{
			return m_myInfoStop;
		}
		void set_all_my_info_loaded()
		{
			m_myInfoStop = true;
		}
	private:
		char m_separator;
	private:
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
#ifdef _DEBUG
				: boost::noncopyable // [+] IRainman fix.
#endif
		{
			virtual ~TaskData() { }
		};
		struct ConnectInfo : public TaskData
#ifdef _DEBUG
				, virtual NonDerivable<ConnectInfo> // [+] IRainman fix.
#endif
		{
			explicit ConnectInfo(string addr_, uint16_t port_, uint16_t localPort_, NatRoles natRole_, bool proxy_) : addr(addr_), port(port_), localPort(localPort_), natRole(natRole_), proxy(proxy_) { }
			string addr;
			uint16_t port;
			uint16_t localPort;
			NatRoles natRole;
			bool proxy;
		};
		struct SendFileInfo : public TaskData
#ifdef _DEBUG
				, virtual NonDerivable<SendFileInfo> // [+] IRainman fix.
#endif
		{
			explicit SendFileInfo(InputStream* stream_) : stream(stream_) { }
			InputStream* stream;
		};
		
		BufferedSocket(char aSeparator);
		
		~BufferedSocket();
		
		FastCriticalSection cs; // [!] IRainman opt: use spinlock here!
		
		Semaphore taskSem;
		deque<pair<Tasks, std::unique_ptr<TaskData>> > m_tasks;
		volatile ThreadID m_threadId; // [+] IRainman fix.
		ByteVector inbuf;
		size_t m_myInfoCount; // Счетчик MyInfo
		bool   m_myInfoStop;  // Флаг передачи команды отличной от MyInfo (загрузка списка закончилась)
#ifdef FLYLINKDC_HE
		void resizeInBuf()
		{
#if 0 // fix http://code.google.com/p/flylinkdc/issues/detail?id=1333
			inbuf.resize(MAX_SOCKET_BUFFER_SIZE);
#else
			const auto l_size = sock->getSocketOptInt(SO_RCVBUF);
			dcassert(l_size)
			if (l_size)
			{
				inbuf.resize(l_size);
			}
			else
			{
				inbuf.resize(MAX_SOCKET_BUFFER_SIZE);
			}
#endif
		}
#else
		void resizeInBuf();
#endif
		
		ByteVector m_writeBuf;
		ByteVector m_sendBuf;
		
		string line;
		int64_t m_dataBytes;
		size_t m_rollback;
		
		Modes m_mode;
		State m_state;
		
		std::unique_ptr<UnZFilter> m_ZfilterIn;
		std::unique_ptr<Socket> sock;
		
		volatile bool m_is_disconnecting; // [!] IRainman fix: this variable is volatile.
		
		int run();
		
		void threadConnect(const string& aAddr, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool proxy);
		void threadAccept();
		void threadRead();
		void threadSendFile(InputStream* is);
		void threadSendData();
		
		void fail(const string& aError);
		static volatile long g_sockets; // [!] IRainman opt: use simple variable here.
		
		bool checkEvents();
		void checkSocket();
		
		void setSocket(std::unique_ptr<Socket>& s); // [!] IRainman fix: add link
		void setOptions()
		{
			sock->setInBufSize();
			sock->setOutBufSize();
		}
		void shutdown();
		void addTask(Tasks task, TaskData* data);
};

#endif // !defined(BUFFERED_SOCKET_H)

/**
 * @file
 * $Id: BufferedSocket.h 575 2011-08-25 19:38:04Z bigmuscle $
 */
