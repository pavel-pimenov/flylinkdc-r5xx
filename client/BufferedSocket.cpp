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
#include "CompatibilityManager.h"
#include "BufferedSocket.h"
#include "TimerManager.h"
#include "SettingsManager.h"
#include "Streams.h"
#include "CryptoManager.h"
#include "ZUtils.h"
#include "ThrottleManager.h"
#include "LogManager.h"
#include "ResourceManager.h"
#include "IpGuard.h"
#include "ClientManager.h"

// Polling is used for tasks...should be fixed...
static const uint64_t POLL_TIMEOUT = 250;

#pragma warning(push)
#pragma warning(disable:4245)
BufferedSocket::BufferedSocket(char aSeparator) :
	separator(aSeparator), mode(MODE_LINE), dataBytes(0), rollback(0), m_state(STARTING),
// [-] brain-ripper
// should be rewritten using ThrottleManager
//sleep(0), // !SMT!-S
	m_disconnecting(false),
	m_threadId(-1) // [+] IRainman fix.
{
	start(64);
	
	Thread::safeInc(g_sockets); // [!] IRainman opt.
}
#pragma warning(pop)

volatile long BufferedSocket::g_sockets = 0; // [!] IRainman opt.

BufferedSocket::~BufferedSocket()
{
	Thread::safeDec(g_sockets); // [!] IRainman opt.
}

void BufferedSocket::setMode(Modes aMode, size_t aRollback)
{
	if (mode == aMode)
	{
		dcdebug("WARNING: Re-entering mode %d\n", mode);
		return;
	}
	
	if (mode == MODE_ZPIPE && filterIn)
	{
		// delete the filter when going out of zpipe mode.
		filterIn.reset();
	}
	
	switch (aMode)
	{
		case MODE_LINE:
			rollback = aRollback;
			break;
		case MODE_ZPIPE:
			filterIn = std::unique_ptr<UnZFilter>(new UnZFilter); // [IntelC++ 2012 beta2] warning #734: "std::unique_ptr<_Ty, _Dx>::unique_ptr(const std::unique_ptr<_Ty, _Dx>::_Myt &) [with _Ty=UnZFilter, _Dx=std::default_delete<UnZFilter>]" (declared at line 2347 of "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include\memory"), required for copy that was eliminated, is inaccessible
			break;
		case MODE_DATA:
			break;
	}
	mode = aMode;
}

void BufferedSocket::setSocket(std::unique_ptr<Socket>& s) // [!] IRainman fix: add link
{
	if (sock.get())
	{
		LogManager::getInstance()->message("BufferedSocket::setSocket - dcassert(!sock.get())");
	}
	dcassert(!sock.get());
	sock = move(s);
}
#ifndef FLYLINKDC_HE
void BufferedSocket::resizeInBuf()
{
	bool l_is_bad_alloc;
#if 0 // fix http://code.google.com/p/flylinkdc/issues/detail?id=1333
	int l_size = sock->getSocketOptInt(SO_RCVBUF);
#else
	int l_size = MAX_SOCKET_BUFFER_SIZE;
#endif
	do
	{
		try
		{
			dcassert(l_size);
			l_is_bad_alloc = false;
			inbuf.resize(l_size);
		}
		catch (std::bad_alloc&)
		{
			l_size /= 2; // Заказываем в 2 раза меньше
			l_is_bad_alloc = l_size > 1024;
		}
	}
	while (l_is_bad_alloc == true);
}
#endif // FLYLINKDC_HE
uint16_t BufferedSocket::accept(const Socket& srv, bool secure, bool allowUntrusted)
{
	dcdebug("BufferedSocket::accept() %p\n", (void*)this);
	
	std::unique_ptr<Socket> s(secure ? CryptoManager::getInstance()->getServerSocket(allowUntrusted) : new Socket);
	
	auto ret = s->accept(srv);
	
	setSocket(s); // [!] IRainman fix setSocket(std::move(s));
	setOptions();
	
	FastLock l(cs);
	addTask(ACCEPTED, nullptr);
	
	return ret;
}

void BufferedSocket::connect(const string& aAddress, uint16_t aPort, bool secure, bool allowUntrusted, bool proxy)
{
	connect(aAddress, aPort, 0, NAT_NONE, secure, allowUntrusted, proxy);
}

void BufferedSocket::connect(const string& aAddress, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool secure, bool allowUntrusted, bool proxy)
{
	dcdebug("BufferedSocket::connect() %p\n", (void*)this);
	std::unique_ptr<Socket> s(secure ? (natRole == NAT_SERVER ? CryptoManager::getInstance()->getServerSocket(allowUntrusted) : CryptoManager::getInstance()->getClientSocket(allowUntrusted)) : new Socket);
	
	s->create();
	
	setSocket(s); // [!] IRainman fix setSocket(std::move(s));
	sock->bind(localPort, SETTING(BIND_ADDRESS));
	
	FastLock l(cs);
	addTask(CONNECT, new ConnectInfo(aAddress, aPort, localPort, natRole, proxy && (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)));
}

static const uint16_t LONG_TIMEOUT = 30000;
static const uint16_t SHORT_TIMEOUT = 1000;
void BufferedSocket::threadConnect(const string& aAddr, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool proxy)
{
	dcassert(m_state == STARTING);
	
	dcdebug("threadConnect %s:%d/%d\n", aAddr.c_str(), (int)localPort, (int)aPort);
	fire(BufferedSocketListener::Connecting());
	
	const uint64_t endTime = GET_TICK() + LONG_TIMEOUT;
	m_state = RUNNING;
	do // while (GET_TICK() < endTime) // [~] IRainman opt
	{
		if (socketIsDisconecting()) // [+] IRainman fix.
			break;
			
		dcdebug("threadConnect attempt to addr \"%s\"\n", aAddr.c_str());
		try
		{
			if (proxy)
			{
				sock->socksConnect(aAddr, aPort, LONG_TIMEOUT);
			}
			else
			{
				sock->connect(aAddr, aPort); // https://www.box.net/shared/l08o2vdekthrrp319m8n + http://www.flylinkdc.ru/2012/10/ashampoo-firewall.html
			}
			
			setOptions();
			
			// [+] IRainman fix
			while (true)
			{
#ifndef FLYLINKDC_HE
				if (ClientManager::isShutdown())
				{
					dcassert(0);
					return;
				}
#endif
				if (sock->waitConnected(POLL_TIMEOUT))
				{
					if (!socketIsDisconecting())
					{
						resizeInBuf();
						fire(BufferedSocketListener::Connected()); //[1] https://www.box.net/shared/52748dbc4f8a46f0a71b
					}
					return;
				}
				if (endTime <= GET_TICK())
					break;
					
				if (socketIsDisconecting())
					return;
			}
			/* [-] IRainman fix
			bool connSucceeded;
			while (!(connSucceeded = sock->waitConnected(POLL_TIMEOUT)) && endTime >= GET_TICK())
			{
			    if (disconnecting) return;
			}
			
			if (connSucceeded)
			{
			    resizeInBuf();
			    fire(BufferedSocketListener::Connected());
			    return;
			}
			[~] IRainman fix end*/
		}
		catch (const SSLSocketException&)
		{
			throw;
		}
		catch (const SocketException&)
		{
			if (natRole == NAT_NONE)
				throw;
			sleep(SHORT_TIMEOUT);
		}
	}
	while (GET_TICK() < endTime); // [~] IRainman opt
	
	throw SocketException(STRING(CONNECTION_TIMEOUT));
}

void BufferedSocket::threadAccept()
{
	dcassert(m_state == STARTING);
	
	dcdebug("threadAccept\n");
	
	m_state = RUNNING;
	
	resizeInBuf();
	
	const uint64_t startTime = GET_TICK();
	while (!sock->waitAccepted(POLL_TIMEOUT))
	{
		if (socketIsDisconecting()) // [!] IRainman fix
			return;
			
		if ((startTime + LONG_TIMEOUT) < GET_TICK())
		{
			throw SocketException(STRING(CONNECTION_TIMEOUT));
		}
	}
}

void BufferedSocket::threadRead()
{
	if (m_state != RUNNING)
		return;
		
	int left = (mode == MODE_DATA) ? ThrottleManager::getInstance()->read(sock.get(), &inbuf[0], (int)inbuf.size()) : sock->read(&inbuf[0], (int)inbuf.size());
	if (left == -1)
	{
		// EWOULDBLOCK, no data received...
		return;
	}
	else if (left == 0)
	{
		// This socket has been closed...
		throw SocketException(STRING(CONNECTION_CLOSED));
	}
	
	string::size_type pos = 0;
	// always uncompressed data
	string l;
	int bufpos = 0, total = left;
	
	while (left > 0)
	{
		switch (mode)
		{
			case MODE_ZPIPE:
			{
				const int BUF_SIZE = 1024;
				// Special to autodetect nmdc connections...
				string::size_type pos = 0; //  warning C6246: Local declaration of 'pos' hides declaration of the same name in outer scope.
				std::unique_ptr<char[]> buffer(new char[BUF_SIZE]);
				l = line;
				// decompress all input data and store in l.
				while (left)
				{
					size_t in = BUF_SIZE;
					size_t used = left;
					bool ret = (*filterIn)(&inbuf[0] + total - left, used, &buffer[0], in);
					left -= used;
					l.append(&buffer[0], in);
					// if the stream ends before the data runs out, keep remainder of data in inbuf
					if (!ret)
					{
						bufpos = total - left;
						setMode(MODE_LINE, rollback);
						break;
					}
				}
				// process all lines
				while ((pos = l.find(separator)) != string::npos)
				{
					if (pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
						fire(BufferedSocketListener::Line(), l.substr(0, pos));
					l.erase(0, pos + 1 /* separator char */);
				}
				// store remainder
				line = l;
				
				break;
			}
			case MODE_LINE:
			{
				// Special to autodetect nmdc connections...
				if (separator == 0)
				{
					if (inbuf[0] == '$')
					{
						separator = '|';
					}
					else
					{
						separator = '\n';
					}
				}
				l = line + string((char*) & inbuf[bufpos], left);
				// TODO - bad_alloc 2012-04-23_22-28-18_L4N2H5DQSWJDZVGEWQRLCAQCSP3HVHJ3ZRWM73Q_EA6DB66F_crash-stack-r501-build-9812.dmp
				// 2012-04-23_22-28-18_XIBJZTGAX3SV6EEOL6UOPEPBV3JEIWX2TEWIU5I_BE4E9488_crash-stack-r501-build-9812.dmp
#if 0
				int l_count_separator = 0;
#endif
				while ((pos = l.find(separator)) != string::npos)
				{
#if 0
					if (l_count_separator++ && l.length() > 0 && BOOLSETTING(LOG_PROTOCOL))
					{
						StringMap params;
						const string l_log = "MODE_LINE l_count_separator = " + Util::toString(l_count_separator) + " left = " + Util::toString(left) + " l.length()=" + Util::toString(l.length()) + " l = " + l;
						LogManager::getInstance()->message(l_log);
					}
#endif
					if (pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
						fire(BufferedSocketListener::Line(), l.substr(0, pos)); // // TODO - отказаться от временной переменной l и скользить по окну inbuf
						
					l.erase(0, pos + 1 /* separator char */); //[3] https://www.box.net/shared/74efa5b96079301f7194
					// TODO - erase не эффективно.
					if (l.length() < (size_t)left)
					{
						left = l.length();
					}
					//dcassert(mode == MODE_LINE);
					if (mode != MODE_LINE)
					{
						// we changed mode; remainder of l is invalid.
						l.clear();
						bufpos = total - left;
						break;
					}
				}
				if (pos == string::npos)
					left = 0;
				line = l;
				break;
			}
			case MODE_DATA:
				while (left > 0)
				{
					if (dataBytes == -1)
					{
						fire(BufferedSocketListener::Data(), &inbuf[bufpos], left);
						bufpos += (left - rollback);
						left = rollback;
						rollback = 0;
					}
					else
					{
						const int high = (int)min(dataBytes, (int64_t)left);
						dcassert(high != 0);
						if (high != 0) // [+] IRainman fix.
						{
							fire(BufferedSocketListener::Data(), &inbuf[bufpos], high);
							bufpos += high;
							left -= high;
							
							dataBytes -= high;
						}
						if (dataBytes == 0)
						{
							mode = MODE_LINE;
							fire(BufferedSocketListener::ModeChange());
							break; // [DC++] break loop, in case setDataMode is called with less than read buffer size
						}
					}
				}
				break;
		}
	}
	if (mode == MODE_LINE && line.size() > static_cast<size_t>(SETTING(MAX_COMMAND_LENGTH)))
	{
		throw SocketException(STRING(COMMAND_TOO_LONG));
	}
}

void BufferedSocket::threadSendFile(InputStream* file)
{
	if (m_state != RUNNING)
		return;
		
	if (socketIsDisconecting()) // [!] IRainman fix
		return;
	dcassert(file != NULL);
#if 0 // fix http://code.google.com/p/flylinkdc/issues/detail?id=1333
	const size_t sockSize = (size_t)sock->getSocketOptInt(SO_SNDBUF);
	const size_t bufSize = max(sockSize, (size_t)MAX_SOCKET_BUFFER_SIZE);
#else
	const size_t sockSize = MAX_SOCKET_BUFFER_SIZE;
	const size_t bufSize = MAX_SOCKET_BUFFER_SIZE;
#endif
	
	ByteVector readBuf(bufSize); // https://www.box.net/shared/07ab0210ed0f83ab842e
	ByteVector writeBuf(bufSize);
	
	size_t readPos = 0;
	
	bool readDone = false;
	dcdebug("Starting threadSend\n");
	while (!socketIsDisconecting()) // [!] IRainman fix
	{
	
		// [-] brain-ripper
		// should be rewritten using ThrottleManager
		//int UserSleep = getSleep(); // !SMT!-S
		
		if (!readDone && readBuf.size() > readPos)
		{
			// Fill read buffer
			size_t bytesRead = readBuf.size() - readPos;
			size_t actual = file->read(&readBuf[readPos], bytesRead);
			
			if (bytesRead > 0)
			{
				fire(BufferedSocketListener::BytesSent(), bytesRead, 0);
			}
			
			if (actual == 0)
			{
				readDone = true;
			}
			else
			{
				readPos += actual;
			}
			
			// [-] brain-ripper
			// should be rewritten using ThrottleManager
			/*
			if (UserSleep > 0)
			{
			    Thread::sleep(UserSleep);    // !SMT!-S
			    bytesRead = min(bytesRead, (size_t)1024);
			}
			*/
		}
		
		if (readDone && readPos == 0)
		{
			fire(BufferedSocketListener::TransmitDone());
			return;
		}
		
		readBuf.swap(writeBuf);
		readBuf.resize(bufSize);
		writeBuf.resize(readPos);
		readPos = 0;
		
		size_t writePos = 0, writeSize = 0;
		int written = 0;
		
		while (writePos < writeBuf.size())
		{
			if (socketIsDisconecting()) // [!] IRainman fix
				return;
				
			if (written == -1)
			{
				// workaround for OpenSSL (crashes when previous write failed and now retrying with different writeSize)
				written = sock->write(&writeBuf[writePos], writeSize);
			}
			else
			{
				writeSize = min(sockSize / 2, writeBuf.size() - writePos);
				written = ThrottleManager::getInstance()->write(sock.get(), &writeBuf[writePos], writeSize);
			}
			
			if (written > 0)
			{
				writePos += written;
				
				fire(BufferedSocketListener::BytesSent(), 0, written);
			}
			else if (written == -1)
			{
				// [-] brain-ripper
				// should be rewritten using ThrottleManager
				//if(!readDone && readPos < readBuf.size() && UserSleep <= 0)  // !SMT!-S
				if (!readDone && readPos < readBuf.size())
				{
					// Read a little since we're blocking anyway...
					size_t bytesRead = min(readBuf.size() - readPos, readBuf.size() / 2);
					size_t actual = file->read(&readBuf[readPos], bytesRead);
					
					if (bytesRead > 0)
					{
						fire(BufferedSocketListener::BytesSent(), bytesRead, 0);
					}
					
					if (actual == 0)
					{
						readDone = true;
					}
					else
					{
						readPos += actual;
					}
				}
				else
				{
					while (!socketIsDisconecting()) // [!] IRainman fix
					{
						const int w = sock->wait(POLL_TIMEOUT, Socket::WAIT_WRITE | Socket::WAIT_READ);
						if (w & Socket::WAIT_READ)
						{
							threadRead();
						}
						if (w & Socket::WAIT_WRITE)
						{
							break;
						}
					}
				}
			}
		}
	}
}

void BufferedSocket::write(const char* aBuf, size_t aLen)
{
	if (!hasSocket())
		return;
	FastLock l(cs);
	if (writeBuf.empty())
		addTask(SEND_DATA, nullptr);
		
	writeBuf.insert(writeBuf.end(), aBuf, aBuf + aLen); // [1] std::bad_alloc nomem https://www.box.net/shared/nmobw6wofukhcdr7lx4h
}

void BufferedSocket::threadSendData()
{
	if (m_state != RUNNING)
		return;
		
	{
		FastLock l(cs);
		if (writeBuf.empty())
			return;
			
		writeBuf.swap(sendBuf);
	}
	
	size_t left = sendBuf.size();
	size_t done = 0;
	while (left > 0)
	{
		if (socketIsDisconecting()) // [!] IRainman fix
		{
			return;
		}
		
		int w = sock->wait(POLL_TIMEOUT, Socket::WAIT_READ | Socket::WAIT_WRITE);
		
		if (w & Socket::WAIT_READ)
		{
			threadRead();
		}
		
		if (w & Socket::WAIT_WRITE)
		{
			int n = sock->write(&sendBuf[done], left); // adguard - https://www.box.net/shared/9201edaa1fa1b83a8d3c
			if (n > 0)
			{
				left -= n;
				done += n;
			}
		}
	}
	sendBuf.clear();
}

bool BufferedSocket::checkEvents()
{
	// [!] application hangs http://www.flickr.com/photos/96019675@N02/9605525265/ http://code.google.com/p/flylinkdc/issues/detail?id=1245
	while (m_state == RUNNING ? taskSem.wait(0) : taskSem.wait())
	{
		pair<Tasks, std::unique_ptr<TaskData>> p;
		{
			FastLock l(cs);
			if (!m_tasks.empty())
			{
				swap(p, m_tasks.front()); // [!] IRainman opt.
				m_tasks.pop_front(); // [!] IRainman opt.
			}
			else
			{
				dcassert(!m_tasks.empty());
				return false;
			}
		}
		
		/* [-] IRainman fix.
		if (p.first == SHUTDOWN)
		{
		    return false;
		}
		else
		   [-] */
		// [!]
		if (m_state == RUNNING)
		{
			if (p.first == UPDATED)
			{
				fire(BufferedSocketListener::Updated());
				continue;
			}
			else if (p.first == SEND_DATA)
			{
				threadSendData();
			}
			else if (p.first == SEND_FILE)
			{
				threadSendFile(static_cast<SendFileInfo*>(p.second.get())->stream);
				break;
			}
			else if (p.first == DISCONNECT)
			{
				fail(STRING(DISCONNECTED));
			}
			// [+]
			else if (p.first == SHUTDOWN)
			{
				return false;
			}
			// [~]
			else
			{
				dcdebug("%d unexpected in RUNNING state\n", p.first);
			}
		}
		else if (m_state == STARTING)
		{
			if (p.first == CONNECT)
			{
				ConnectInfo* ci = static_cast<ConnectInfo*>(p.second.get());
				threadConnect(ci->addr, ci->port, ci->localPort, ci->natRole, ci->proxy);
			}
			else if (p.first == ACCEPTED)
			{
				threadAccept();
			}
			// [+]
			else if (p.first == SHUTDOWN)
			{
				return false;
			}
			// [~]
			else
			{
				dcdebug("%d unexpected in STARTING state\n", p.first);
			}
		}
		// [+]
		else // FAILED
		{
			if (p.first == SHUTDOWN)
			{
				return false;
			}
			else
			{
				dcdebug("%d unexpected in FAILED state\n", p.first);
			}
		}
		// [~]
		// [~] IRainman fix.
	}
	return true;
}

#ifdef FLYLINKDC_HE
inline // [+] IRainman opt.
#endif
void BufferedSocket::checkSocket()
{
	if (hasSocket())
	{
		int waitFor = sock->wait(POLL_TIMEOUT, Socket::WAIT_READ);
		if (waitFor & Socket::WAIT_READ)
		{
			threadRead();
		}
	}
}

/**
 * Main task dispatcher for the buffered socket abstraction.
 * @todo Fix the polling...
 */
int BufferedSocket::run()
{
	m_threadId = GetSelfThreadID(); // [+] IRainman fix.
	dcdebug("BufferedSocket::run() start %p\n", (void*)this);
	while (true)
	{
		try
		{
			if (!checkEvents())
			{
				break;
			}
			if (m_state == RUNNING)
			{
				checkSocket();
			}
		}
		catch (const Exception& e)
		{
			fail(e.getError());
		}
	}
	dcdebug("BufferedSocket::run() end %p\n", (void*)this);
	if (m_threadId == 0) // [!] IRainman fix: You can not explicitly delete this when you inherit from Thread, which has a virtual destructor. Cleaning of this is performed in BufferedSocket::shutdown
	{
		dcdebug("BufferedSocket delayed cleanup %p\n", (void*)this);
		delete this;
	}
	else
	{
		m_threadId = 0; // [!] IRainman fix. [!] https://code.google.com/p/flylinkdc/issues/detail?id=1245 , https://code.google.com/p/flylinkdc/issues/detail?id=1246
	}
	return 0;
}

void BufferedSocket::fail(const string& aError)
{
	if (hasSocket())
		sock->disconnect();
		
	if (m_state == RUNNING)
	{
		m_state = FAILED;
		fire(BufferedSocketListener::Failed(), aError);
	}
}

void BufferedSocket::shutdown()
{
	m_disconnecting = true;
	// [!] IRainman fix: turning off the socket in the asynchronous mode is prohibited because
	// the listeners of its events will have been destroyed by the time of processing the shutdown event.
	{
		// [!] fix http://code.google.com/p/flylinkdc/issues/detail?id=1280
		FastLock l(cs);
		addTask(SHUTDOWN, nullptr);
	}
	// [+]
	if (m_threadId == GetSelfThreadID()) // shutdown called from the thread socket.
	{
		m_threadId = 0; // Set to delayed cleanup.
	}
	else
	{
		while (m_threadId) // [!] https://code.google.com/p/flylinkdc/issues/detail?id=1245 , https://code.google.com/p/flylinkdc/issues/detail?id=1246
		{
			sleep(1); // [+] We are waiting for the full processing of the events.
		}
		dcdebug("BufferedSocket instantly cleanup %p\n", (void*)this);
		delete this;
	}
	// [~] IRainman fix.
}

#ifdef FLYLINKDC_HE
inline // [+] IRainman opt.
#endif
void BufferedSocket::addTask(Tasks task, TaskData* data)
{
	dcassert(task == DISCONNECT || task == SHUTDOWN || task == UPDATED || sock.get());
	m_tasks.push_back(make_pair(task, unique_ptr<TaskData>(data)));
	taskSem.signal();
}

/**
 * @file
 * $Id: BufferedSocket.cpp 582 2011-12-18 10:00:11Z bigmuscle $
 */
