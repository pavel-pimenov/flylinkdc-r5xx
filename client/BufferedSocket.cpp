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
#include "Util.h"
#include "ShareManager.h"
#include "DebugManager.h"
#include "../FlyFeatures/flyServer.h"

// Polling is used for tasks...should be fixed...
static const uint64_t POLL_TIMEOUT = 250;
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
volatile long BufferedSocket::g_sockets = 0;
#endif

BufferedSocket::BufferedSocket(char aSeparator) :
	m_separator(aSeparator),
	m_mode(MODE_LINE),
	m_dataBytes(0),
	m_rollback(0),
	m_state(STARTING),
	m_count_search_ddos(0),
// [-] brain-ripper
// should be rewritten using ThrottleManager
//sleep(0), // !SMT!-S
	m_is_disconnecting(false),
	m_threadId(ThreadID(-1)),
	m_myInfoCount(0),
	m_is_all_my_info_loaded(false),
	m_is_hide_share(false)
{
	start(64, "BufferedSocket");
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	Thread::safeInc(g_sockets); // [!] IRainman opt.
#endif
}

BufferedSocket::~BufferedSocket()
{
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	Thread::safeDec(g_sockets); // [!] IRainman opt.
#endif
}

void BufferedSocket::setMode(Modes aMode, size_t aRollback)
{
	if (m_mode == aMode)
	{
		dcdebug("WARNING: Re-entering mode %d\n", m_mode);
		return;
	}
	
	if (m_mode == MODE_ZPIPE && m_ZfilterIn)
	{
		// delete the filter when going out of zpipe mode.
		m_ZfilterIn.reset();
	}
	
	switch (aMode)
	{
		case MODE_LINE:
			m_rollback = aRollback;
			//dcassert(!ClientManager::isShutdown());
#ifdef _DEBUG
			//LogManager::message("BufferedSocket:: = MODE_LINE [2] - m_rollback = aRollback");
#endif;
			break;
		case MODE_ZPIPE:
			m_ZfilterIn = std::unique_ptr<UnZFilter>(new UnZFilter); // [IntelC++ 2012 beta2] warning #734: "std::unique_ptr<_Ty, _Dx>::unique_ptr(const std::unique_ptr<_Ty, _Dx>::_Myt &) [with _Ty=UnZFilter, _Dx=std::default_delete<UnZFilter>]" (declared at line 2347 of "C:\Program Files (x86)\Microsoft Visual Studio 10.0\VC\include\memory"), required for copy that was eliminated, is inaccessible
			break;
		case MODE_DATA:
			break;
	}
	m_mode = aMode;
}

void BufferedSocket::setSocket(std::unique_ptr<Socket>& s) // [!] IRainman fix: add link
{
	if (sock.get())
	{
		LogManager::message("BufferedSocket::setSocket - dcassert(!sock.get())");
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
			m_inbuf.resize(l_size);
		}
		catch (std::bad_alloc&)
		{
			l_size /= 2; // Заказываем в 2 раза меньше
			l_is_bad_alloc = l_size > 128;
			if (l_is_bad_alloc == false)
			{
				throw;
			}
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
	
	setSocket(s);
	setOptions();
	
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
	
	setSocket(s);
	sock->bind(localPort, SETTING(BIND_ADDRESS));
	
	initMyINFOLoader();
	addTask(CONNECT, new ConnectInfo(aAddress, aPort, localPort, natRole, proxy && (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)));
}

static const uint16_t LONG_TIMEOUT = 30000;
static const uint16_t SHORT_TIMEOUT = 1000;
void BufferedSocket::threadConnect(const string& aAddr, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool proxy)
{
	m_count_search_ddos = 0;
	dcassert(m_state == STARTING);
	
	dcdebug("threadConnect %s:%d/%d\n", aAddr.c_str(), (int)localPort, (int)aPort);
	fly_fire(BufferedSocketListener::Connecting());
	
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
					//dcassert(0);
					return;
				}
#endif
				if (sock->waitConnected(POLL_TIMEOUT))
				{
					if (!socketIsDisconecting())
					{
						resizeInBuf();
						fly_fire(BufferedSocketListener::Connected());
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
			    fly_fire1(__FUNCTION__,BufferedSocketListener::Connected());
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

bool BufferedSocket::all_search_parser(const string::size_type p_pos_next_separator, const string& p_line,
                                       CFlySearchArrayTTH& p_tth_search,
                                       CFlySearchArrayFile& p_file_search)
{
	extern bool g_isStartupProcess;
	if (p_line.compare(0, 8, "$Search ", 8) == 0)
	{
		if (m_is_hide_share == false && g_isStartupProcess == false)
		{
			const string l_line_item = p_line.substr(0, p_pos_next_separator);
			auto l_marker_tth = l_line_item.find("?0?9?TTH:");
			// TODO научиться обрабатывать лимит по размеру вида
			// "x.x.x.x:yyy T?F?57671680?9?TTH:A3VSWSWKCVC4N6EP2GX47OEMGT5ZL52BOS2LAHA"
			if (l_marker_tth != string::npos && l_marker_tth > 5 && l_line_item[l_marker_tth - 4] == ' ') // Поправка на полную команду  F?T?0?9?TTH: или F?F?0?9?TTH: или T?T?0?9?TTH:
			{
				l_marker_tth -= 4;
				const string l_tth_str = l_line_item.substr(l_marker_tth + 13, 39);
				const TTHValue l_tth(l_tth_str);
				if (!ShareManager::isUnknownTTH(l_tth))
				{
					const CFlySearchItemTTH l_item(TTHValue(l_line_item.substr(l_marker_tth + 13, 39)), l_line_item.substr(8, l_marker_tth - 8));
					dcassert(l_item.m_search.find('|') == string::npos && l_item.m_search.find('$') == string::npos);
					p_tth_search.push_back(l_item);
				}
				else
				{
					COMMAND_DEBUG("[TTH][FastSkip] " + l_line_item, DebugTask::HUB_IN, getIp() + ':' + Util::toString(getPort()));
#ifdef _DEBUG
					//  LogManager::message("BufferedSocket::all_search_parser Skip unknown TTH = " + l_tth.toBase32());
#endif
				}
			}
			else
			{
				if (Util::isValidSearch(l_line_item) == false)
				{
					if (!m_count_search_ddos)
					{
						const string l_error = "BufferedSocket::all_search_parser DDoS $Search command: " + l_line_item + " Hub IP = " + getIp();
						CFlyServerJSON::pushError(20, l_error);
						LogManager::message(l_error);
						if (!m_count_search_ddos)
						{
							fly_fire1(BufferedSocketListener::DDoSSearchDetect(), l_error);
						}
						m_count_search_ddos++;
					}
					COMMAND_DEBUG("[DDoS] " + l_line_item, DebugTask::HUB_IN, getIp() + ':' + Util::toString(getPort()));
					return true;
				}
#if 0
				auto l_marker_file = l_line_item.find(' ', 8);
				if (l_marker_file == string::npos || l_line_item.size() <= 12)
				{
					const string l_error = "BufferedSocket::all_search_parser error format $Search command: " + l_line_item + " Hub IP = " + getIp();
					CFlyServerJSON::pushError(19, l_error);
					LogManager::message(l_error);
					return true;
				}
#endif
#ifdef _DEBUG
//            LogManager::message("BufferedSocket::all_search_parser Skip unknown file = " + aString);
#endif
				CFlySearchItemFile l_item;
				if (l_item.is_parse_nmdc_search(l_line_item.substr(8)) == true)
				{
					if (ShareManager::isUnknownFile(l_item.getRAWQuery()))
					{
						COMMAND_DEBUG("[File][FastSkip][Unknown files] " + l_line_item, DebugTask::HUB_IN, getIp() + ':' + Util::toString(getPort()));
#ifdef _DEBUG
						// LogManager::message("BufferedSocket::all_search_parser Skip unknown File = " + l_item.m_raw_search);
#endif
					}
					else
					{
						p_file_search.push_back(l_item);
					}
				}
				else
				{
#ifdef _DEBUG
					LogManager::message("BufferedSocket::all_search_parser error is_parse_nmdc_search = " + l_item.m_raw_search + " m_error_level = " + Util::toString(l_item.m_error_level));
#endif
				}
			}
		}
		return true;
	}
	return false;
}
void BufferedSocket::all_myinfo_parser(const string::size_type p_pos_next_separator, const string& p_line, StringList& p_all_myInfo, bool p_is_zon)
{
	const bool l_is_MyINFO = m_is_all_my_info_loaded == false ? p_line.compare(0, 8, "$MyINFO ", 8) == 0 : false;
	const string l_line_item = l_is_MyINFO ? p_line.substr(8, p_pos_next_separator - 8) : p_line.substr(0, p_pos_next_separator);
	if (m_is_all_my_info_loaded == false)
	{
		if (l_is_MyINFO)
		{
			++m_myInfoCount;
			p_all_myInfo.push_back(l_line_item);
		}
		else if (m_myInfoCount)
		{
			if (!p_all_myInfo.empty())
			{
				fly_fire1(BufferedSocketListener::MyInfoArray(), p_all_myInfo);
			}
			set_all_my_info_loaded(); // закончился стартовый поток $MyINFO
		}
	}
	if (p_all_myInfo.empty())
	{
		if (l_line_item.compare(0, 4, "$ZOn", 4) == 0)
		{
			setMode(MODE_ZPIPE);
		}
		else
		{
			dcassert(m_mode == MODE_LINE || m_mode == MODE_ZPIPE);
#ifdef _DEBUG
			for (auto i = l_line_item.cbegin(); i != l_line_item.cend(); ++i)
			{
				// dcassert(isascii(*i));
			}
			if (l_line_item.length() && ClientManager::isShutdown())
			{
				if (!(l_line_item[0] == '<' || l_line_item[0] == '$' || l_line_item[l_line_item.length() - 1] == '|'))
				{
					LogManager::message("OnLine: " + l_line_item);
				}
			}
#endif
			if (!ClientManager::isShutdown())
			{
				fly_fire1(BufferedSocketListener::Line(), l_line_item); // TODO - отказаться от временной переменной l и скользить по окну inbuf
			}
		}
	}
}

/*
#ifdef FLYLINKDC_EMULATOR_4000_USERS
                                    static bool g_is_test = false;
                                    const int l_count_guest = 4000;
                                    if (!g_is_test)
                                    {
                                        g_is_test = true;
                                        for (int i = 0; i < l_count_guest; ++i)
                                        {
                                            char bbb[200];
                                            snprintf(bbb, sizeof(bbb), "$ALL Guest%d <<Peers V:(r622),M:P,H:1/0/0,S:15,C:Кемерово>$ $%c$$3171624055$", i, 5);
                                            l_all_myInfo.push_back(bbb);
                                        }
                                    }
#endif // FLYLINKDC_EMULATOR_4000_USERS
#ifdef FLYLINKDC_EMULATOR_4000_USERS
// Генерируем случайные IP-адреса
                                    for (int i = 0; i < l_count_guest; ++i)
                                    {
                                        char bbb[200];
                                        boost::system::error_code ec;
                                        const auto l_start = boost::asio::ip::address_v4::from_string("200.23.17.18", ec);
                                        const auto l_stop = boost::asio::ip::address_v4::from_string("240.200.17.18", ec);
                                        boost::asio::ip::address_v4 l_rnd_ip(Util::rand(l_start.to_ulong(), l_stop.to_ulong()));
                                        snprintf(bbb, sizeof(bbb), "$UserIP Guest%d %s$$", i, l_rnd_ip.to_string().c_str());
                                        fly_fire1(BufferedSocketListener::Line(), bbb);
                                    }
#endif

*/
void BufferedSocket::parseMyINfoAndSearch(
    StringList& p_all_myInfo,
    CFlySearchArrayTTH& p_tth_search,
    CFlySearchArrayFile& p_file_search)
{
	if (!p_all_myInfo.empty())
	{
		fly_fire1(BufferedSocketListener::MyInfoArray(), p_all_myInfo);
	}
	if (!p_tth_search.empty())
	{
		fly_fire1(BufferedSocketListener::SearchArrayTTH(), p_tth_search);
	}
	if (!p_file_search.empty())
	{
		fly_fire1(BufferedSocketListener::SearchArrayFile(), p_file_search);
	}
}

void BufferedSocket::threadRead()
{
	if (m_state != RUNNING)
		return;
		
	int l_left = (m_mode == MODE_DATA) ? ThrottleManager::getInstance()->read(sock.get(), &m_inbuf[0], (int)m_inbuf.size()) : sock->read(&m_inbuf[0], (int)m_inbuf.size());
	if (l_left == -1)
	{
		// EWOULDBLOCK, no data received...
		return;
	}
	else if (l_left == 0)
	{
		// This socket has been closed...
		throw SocketException(STRING(CONNECTION_CLOSED));
	}
	
	string::size_type l_pos = 0;
	// always uncompressed data
	string l;
	int l_bufpos = 0;
	int l_total = l_left;
#ifdef _DEBUG
	if (m_mode == MODE_LINE)
	{
		// LogManager::message("BufferedSocket::threadRead[MODE_LINE] = " + string((char*) & inbuf[0], l_left));
	}
#endif
	
	while (l_left > 0)
	{
		switch (m_mode)
		{
			case MODE_ZPIPE:
			{
				const int BUF_SIZE = 1024;
				// Special to autodetect nmdc connections...
				string::size_type l_zpos = 0; //  warning C6246: Local declaration of 'pos' hides declaration of the same name in outer scope.
				std::unique_ptr<char[]> buffer(new char[BUF_SIZE]);
				l = m_line;
				// decompress all input data and store in l.
				while (l_left)
				{
					size_t l_in = BUF_SIZE;
					size_t l_used = l_left;
					bool ret = (*m_ZfilterIn)(&m_inbuf[0] + l_total - l_left, l_used, &buffer[0], l_in);
					l_left -= l_used;
					l.append(&buffer[0], l_in);
					// if the stream ends before the data runs out, keep remainder of data in inbuf
					if (!ret)
					{
						l_bufpos = l_total - l_left;
						setMode(MODE_LINE, m_rollback);
						break;
					}
				}
				// process all lines
#define USE_FLYLINKDC_MYINFO_ARRAY
#ifdef USE_FLYLINKDC_MYINFO_ARRAY
#ifdef _DEBUG
				// LogManager::message("BufferedSocket::threadRead[MODE_ZPIPE] = " + l);
#endif
				//
				if (!ClientManager::isShutdown())
				{
					StringList l_all_myInfo;
					CFlySearchArrayTTH l_tth_search;
					CFlySearchArrayFile l_file_search;
					while ((l_zpos = l.find(m_separator)) != string::npos)
					{
						if (l_zpos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
						{
							if (all_search_parser(l_zpos, l, l_tth_search, l_file_search) == false)
							{
								all_myinfo_parser(l_zpos, l, l_all_myInfo, true);
							}
						}
						l.erase(0, l_zpos + 1 /* separator char */); //[3] https://www.box.net/shared/74efa5b96079301f7194
					}
					parseMyINfoAndSearch(l_all_myInfo, l_tth_search, l_file_search);
#else
				// process all lines
				while ((pos = l.find(m_separator)) != string::npos)
				{
					if (pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
						fly_fire1(__FUNCTION__, BufferedSocketListener::Line(), l.substr(0, pos));
					l.erase(0, pos + 1 /* separator char */); // TODO не эффективно
				}
#endif
				}
#ifdef _DEBUG
				else
				{
					const string l_log = "Skip MODE_LINE [after ZLIB] - FlylinkDC++ Destroy... l = " + l;
					LogManager::message(l_log);
					
				}
#endif
				// store remainder
				m_line = l;
				
				break;
			}
			case MODE_LINE:
			{
				// Special to autodetect nmdc connections...
				if (m_separator == 0)
				{
					if (m_inbuf[0] == '$')
					{
						m_separator = '|';
					}
					else
					{
						m_separator = '\n';
					}
				}
				//======================================================================
				// TODO - вставить быструю обработку поиска по TTH без вызова листенеров
				// Если пасив - отвечаем в буфер сразу
				// Если актив - кидаем отсылку UDP (тоже через очередь?)
				
				
				//======================================================================
				l = m_line + string((char*)& m_inbuf[l_bufpos], l_left);
				//dcassert(isalnum(l[0]) || isalpha(l[0]) || isascii(l[0]));
#if 0
				int l_count_separator = 0;
#endif
#ifdef _DEBUG
				//LogManager::message("MODE_LINE . m_line = " + m_line);
				//LogManager::message("MODE_LINE = " + l);
#endif
				if (!ClientManager::isShutdown())
				{
					StringList l_all_myInfo;
					CFlySearchArrayTTH l_tth_search;
					CFlySearchArrayFile l_file_search;
					while ((l_pos = l.find(m_separator)) != string::npos)
					{
#if 0
						if (l_count_separator++ && l.length() > 0 && BOOLSETTING(LOG_PROTOCOL))
						{
							StringMap params;
							const string l_log = "MODE_LINE l_count_separator = " + Util::toString(l_count_separator) + " l_left = " + Util::toString(l_left) + " l.length()=" + Util::toString(l.length()) + " l = " + l;
							LogManager::message(l_log);
						}
#endif
						if (l_pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
						{
							if (all_search_parser(l_pos, l, l_tth_search, l_file_search) == false)
							{
								all_myinfo_parser(l_pos, l, l_all_myInfo, false);
							}
						}
						
						l.erase(0, l_pos + 1 /* separator char */);
						// TODO - erase не эффективно.
						if (l.length() < (size_t)l_left)
						{
							l_left = l.length();
						}
						//dcassert(mode == MODE_LINE);
						if (m_mode != MODE_LINE)
						{
							// dcassert(mode == MODE_LINE);
							// TOOD ? m_myInfoStop = true;
							// we changed mode; remainder of l is invalid.
							l.clear();
							l_bufpos = l_total - l_left;
							break;
						}
					}
					parseMyINfoAndSearch(l_all_myInfo, l_tth_search, l_file_search);
				}
				else
				{
#ifdef _DEBUG
					const string l_log = "Skip MODE_LINE [normal] - FlylinkDC++ Destroy... l = " + l;
					LogManager::message(l_log);
#endif
					l.clear();
					l_bufpos = l_total - l_left;
					l_left = 0;
					l_pos = string::npos;
				}
				
				if (l_pos == string::npos)
				{
					l_left = 0;
				}
				m_line = l;
				break;
			}
			case MODE_DATA:
				while (l_left > 0)
				{
					if (m_dataBytes == -1)
					{
						fly_fire2(BufferedSocketListener::Data(), &m_inbuf[l_bufpos], l_left);
						l_bufpos += (l_left - m_rollback);
						l_left = m_rollback;
						m_rollback = 0;
					}
					else
					{
						const int high = (int)min(m_dataBytes, (int64_t)l_left);
						//dcassert(high != 0);
						if (high != 0) // [+] IRainman fix.
						{
							fly_fire2(BufferedSocketListener::Data(), &m_inbuf[l_bufpos], high);
							l_bufpos += high;
							l_left -= high;
							
							m_dataBytes -= high;
						}
						if (m_dataBytes == 0)
						{
							m_mode = MODE_LINE;
#ifdef _DEBUG
							LogManager::message("BufferedSocket:: = MODE_LINE [1]");
#endif;
#ifdef FLYLINKDC_USE_CROOKED_HTTP_CONNECTION
							fly_fire(BufferedSocketListener::ModeChange());
#endif
							break; // [DC++] break loop, in case setDataMode is called with less than read buffer size
						}
					}
				}
				break;
		}
	}
	if (m_mode == MODE_LINE && m_line.size() > static_cast<size_t>(SETTING(MAX_COMMAND_LENGTH)))
	{
		throw SocketException(STRING(COMMAND_TOO_LONG));
	}
}

void BufferedSocket::disconnect(bool p_graceless /*= false */)
{
	if (p_graceless)
	{
		m_is_disconnecting = true;
	}
	m_is_all_my_info_loaded = false;
	addTask(DISCONNECT, nullptr);
}

boost::asio::ip::address_v4 BufferedSocket::getIp4() const
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

void BufferedSocket::putBufferedSocket(BufferedSocket*& p_sock, bool p_delete /*= false*/)
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

#ifdef FLYLINKDC_USE_SOCKET_COUNTER
void BufferedSocket::waitShutdown()
{
	int l_max_count = 500;
	while (g_sockets > 0 && --l_max_count)
	{
		sleep(10); // TODO - Если слишком долго ждем. спросить диалогом и если ответят "да" - закрыться
		// TODO - случай зависания передать на флай-сервер.
	}
}
#endif

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
	
	// const size_t maxSegment = (size_t)sock->getSocketOptInt(TCP_MAXSEG);
	ByteVector l_readBuf(bufSize); // https://www.box.net/shared/07ab0210ed0f83ab842e
	ByteVector l_writeBuf(bufSize);
	
	size_t readPos = 0;
	
	bool readDone = false;
	dcdebug("Starting threadSend\n");
	while (!socketIsDisconecting()) // [!] IRainman fix
	{
	
		// [-] brain-ripper
		// should be rewritten using ThrottleManager
		//int UserSleep = getSleep(); // !SMT!-S
		
		if (!readDone && l_readBuf.size() > readPos)
		{
			// Fill read buffer
			size_t bytesRead = l_readBuf.size() - readPos;
			size_t actual = file->read(&l_readBuf[readPos], bytesRead); // TODO можно узнать что считали последний кусок в файл
			
			if (bytesRead > 0)
			{
				fly_fire2(BufferedSocketListener::BytesSent(), bytesRead, 0);
				// Инфу о том что считали с диск не шлем фаером
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
			fly_fire(BufferedSocketListener::TransmitDone());
			return;
		}
		
		l_readBuf.swap(l_writeBuf);
		l_readBuf.resize(bufSize);
		l_writeBuf.resize(readPos);
		readPos = 0;
		
		size_t writePos = 0;
		int written = 0;
		
		while (writePos < l_writeBuf.size())
		{
			if (socketIsDisconecting()) // [!] IRainman fix
				return;
				
			if (written == -1)
			{
				// workaround for OpenSSL (crashes when previous write failed and now retrying with different writeSize)
				size_t l_writeSize = 0;
				written = sock->write(&l_writeBuf[writePos], l_writeSize);
			}
			else
			{
				size_t l_writeSize = min(sockSize / 2, l_writeBuf.size() - writePos);
				written = ThrottleManager::getInstance()->write(sock.get(), &l_writeBuf[writePos], l_writeSize);
			}
			
			if (written > 0)
			{
				writePos += written;
				fly_fire2(BufferedSocketListener::BytesSent(), 0, written);
			}
			else if (written == -1)
			{
				// [-] brain-ripper
				// should be rewritten using ThrottleManager
				//if(!readDone && readPos < readBuf.size() && UserSleep <= 0)  // !SMT!-S
				if (!readDone && readPos < l_readBuf.size())
				{
					// Read a little since we're blocking anyway...
					size_t bytesRead = min(l_readBuf.size() - readPos, l_readBuf.size() / 2);
					size_t actual = file->read(&l_readBuf[readPos], bytesRead);
					
					if (bytesRead > 0)
					{
						fly_fire2(BufferedSocketListener::BytesSent(), bytesRead, 0);
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
	CFlyFastLock(cs);
	if (m_writeBuf.empty())
	{
		addTaskL(SEND_DATA, nullptr);
	}
#ifdef _DEBUG
	if (aLen > 1)
	{
		dcassert(!(aBuf[aLen - 1] == '|' && aBuf[aLen - 2] == '|'));
	}
#endif
	m_writeBuf.insert(m_writeBuf.end(), aBuf, aBuf + aLen); // [1] std::bad_alloc nomem https://www.box.net/shared/nmobw6wofukhcdr7lx4h
}

void BufferedSocket::threadSendData()
{
	if (m_state != RUNNING)
		return;
	ByteVector l_sendBuf;
	{
		CFlyFastLock(cs);
		if (m_writeBuf.empty())
		{
			dcassert(!m_writeBuf.empty());
			return;
		}
		m_writeBuf.swap(l_sendBuf);
	}
	
	size_t left = l_sendBuf.size();
	size_t done = 0;
	while (left > 0)
	{
		if (socketIsDisconecting()) // [!] IRainman fix
		{
			return;
		}
		
		const int w = sock->wait(POLL_TIMEOUT, Socket::WAIT_READ | Socket::WAIT_WRITE);
		
		if (w & Socket::WAIT_READ)
		{
			threadRead();
		}
		
		if (w & Socket::WAIT_WRITE)
		{
			// TODO - find ("||")
			const int n = sock->write(&l_sendBuf[done], left); // adguard - https://www.box.net/shared/9201edaa1fa1b83a8d3c
			if (n > 0)
			{
				left -= n;
				done += n;
			}
		}
	}
}

bool BufferedSocket::checkEvents()
{
	// [!] application hangs http://www.flickr.com/photos/96019675@N02/9605525265/ http://code.google.com/p/flylinkdc/issues/detail?id=1245
	while (m_state == RUNNING ? m_taskSem.wait(0) : m_taskSem.wait())
	{
		pair<Tasks, std::unique_ptr<TaskData>> p;
		{
			CFlyFastLock(cs);
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
				fly_fire(BufferedSocketListener::Updated());
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
			//LogManager::message("BufferedSocket::run(), error = " + e.getError());
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
	{
		sock->disconnect();
	}
	
	if (m_state == RUNNING)
	{
		m_state = FAILED;
		//dcassert(!ClientManager::isShutdown());
		// fix https://drdump.com/Problem.aspx?ProblemID=112938
		// fix https://drdump.com/Problem.aspx?ProblemID=112262
		// fix https://drdump.com/Problem.aspx?ProblemID=112195
		fly_fire1(BufferedSocketListener::Failed(), aError);
	}
}

void BufferedSocket::shutdown()
{
	m_is_disconnecting = true;
	// [!] IRainman fix: turning off the socket in the asynchronous mode is prohibited because
	// the listeners of its events will have been destroyed by the time of processing the shutdown event.
	{
		// [!] fix http://code.google.com/p/flylinkdc/issues/detail?id=1280
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

void BufferedSocket::addTask(Tasks p_task, TaskData* p_data)
{
	CFlyFastLock(cs);
	addTaskL(p_task, p_data);
}
void BufferedSocket::addTaskL(Tasks p_task, TaskData* p_data)
{
	dcassert(p_task == DISCONNECT || p_task == SHUTDOWN || p_task == UPDATED || sock.get());
	m_tasks.push_back(make_pair(p_task, unique_ptr<TaskData>(p_data)));
	m_taskSem.signal();
}

/**
 * @file
 * $Id: BufferedSocket.cpp 582 2011-12-18 10:00:11Z bigmuscle $
 */
