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
#include "ZUtils.h"
#include "ThrottleManager.h"
#include "IpGuard.h"
#include "ShareManager.h"
#include "DebugManager.h"
#include "SSLSocket.h"
#include "UserConnection.h"
#include "../FlyFeatures/flyServer.h"

// Polling is used for tasks...should be fixed...
static const uint64_t POLL_TIMEOUT = 250;
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
std::atomic<long> BufferedSocket::g_sockets(0);
#endif

BufferedSocket::BufferedSocket(char aSeparator, UserConnection* p_connection) :
	m_connection(p_connection),
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
	m_myInfoCount(0),
	m_is_all_my_info_loaded(false),
	m_is_hide_share(false)
{
	start(64, "BufferedSocket");
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	++g_sockets;
#endif
}

BufferedSocket::~BufferedSocket()
{
	int l_dummy = 0;
	l_dummy++;
#ifdef FLYLINKDC_USE_SOCKET_COUNTER
	--g_sockets;
#endif
	dcassert(m_tasks.empty());
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
			//dcassert(!ClientManager::isBeforeShutdown());
#ifdef _DEBUG
			//LogManager::message("BufferedSocket:: = MODE_LINE [2] - m_rollback = aRollback");
#endif;
			break;
		case MODE_ZPIPE:
			m_ZfilterIn = std::make_unique<UnZFilter>();
			break;
		case MODE_DATA:
			break;
	}
	m_mode = aMode;
}

void BufferedSocket::setSocket(std::unique_ptr<Socket> && s)
{
	if (sock.get())
	{
		LogManager::message("BufferedSocket::setSocket - dcassert(!sock.get())");
	}
	dcassert(!sock.get());
	sock = move(s);
}
void BufferedSocket::resizeInBuf()
{
	bool l_is_bad_alloc;
	int l_size = MAX_SOCKET_BUFFER_SIZE;
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
			ShareManager::tryFixBadAlloc();
			l_size /= 2; // ���������� � 2 ���� ������
			l_is_bad_alloc = l_size > 128;
			if (l_is_bad_alloc == false)
			{
				throw;
			}
		}
	}
	while (l_is_bad_alloc == true);
}

uint16_t BufferedSocket::accept(const Socket& srv, bool secure, bool allowUntrusted, const string& expKP)
{
	dcdebug("BufferedSocket::accept() %p\n", (void*)this);
	
	unique_ptr<Socket> s(secure ? new SSLSocket(CryptoManager::SSL_SERVER, allowUntrusted, expKP) : new Socket(/*Socket::TYPE_TCP */));
	
	auto ret = s->accept(srv);
	
	setSocket(move(s));
	setOptions();
	
	addTask(ACCEPTED, nullptr);
	
	return ret;
}

void BufferedSocket::connect(const string& aAddress, uint16_t aPort, bool secure, bool allowUntrusted, bool proxy,
                             Socket::Protocol p_proto, const string& expKP /*= Util::emptyString*/)
{
	connect(aAddress, aPort, 0, NAT_NONE, secure, allowUntrusted, proxy, p_proto, expKP);
}

void BufferedSocket::connect(const string& aAddress, uint16_t aPort, uint16_t localPort, NatRoles natRole, bool secure, bool allowUntrusted, bool proxy, Socket::Protocol p_proto, const string& expKP /*= Util::emptyString*/)
{
	dcdebug("BufferedSocket::connect() %p\n", (void*)this);
//	unique_ptr<Socket> s(secure ? new SSLSocket(natRole == NAT_SERVER ? CryptoManager::SSL_SERVER : CryptoManager::SSL_CLIENT_ALPN, allowUntrusted, p_proto, expKP)
//             : new Socket(/*Socket::TYPE_TCP*/));
	std::unique_ptr<Socket> s(secure ? (natRole == NAT_SERVER ?
	                                    CryptoManager::getInstance()->getServerSocket(allowUntrusted) :
	                                    CryptoManager::getInstance()->getClientSocket(allowUntrusted, p_proto)) : new Socket);
	                                    
	s->create(); // � AirDC++ ��� ����� �����... �����������
	
	setSocket(move(s));
	sock->bind(localPort, SETTING(BIND_ADDRESS));
	
	initMyINFOLoader();
	addTask(CONNECT, new ConnectInfo(aAddress, aPort, localPort, natRole, proxy && (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)));
}

void BufferedSocket::initMyINFOLoader()
{
	m_is_disconnecting = false;
	m_is_all_my_info_loaded = false;
	m_myInfoCount = 0;
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
		if (socketIsDisconnecting())
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
			
			while (true)
			{
				if (ClientManager::isBeforeShutdown())
				{
					throw SocketException(STRING(COMMAND_SHUTDOWN_IN_PROGRESS));
				}
				if (sock->waitConnected(POLL_TIMEOUT))
				{
					if (!socketIsDisconnecting())
					{
						resizeInBuf();
						fly_fire(BufferedSocketListener::Connected());
					}
					return;
				}
				if (endTime <= GET_TICK())
					break;
					
				if (socketIsDisconnecting())
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
		if (socketIsDisconnecting())
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
	// dcassert(m_is_disconnecting == false);
	if (m_is_disconnecting == true)
		return false;
	if (p_line.size() < 8)
		return false;
	if (p_line.compare(0, 2, "$S", 2) != 0)
		return false;
	if (ShareManager::g_is_initial == true)
	{
#ifdef _DEBUG
		//LogManager::message("[ShareManager::g_is_initial] BufferedSocket::all_search_parser p_line = " + p_line);
#endif
		return true;
	}
	if (m_is_hide_share == true || ClientManager::isStartup() == true)
	{
		return true;
	}
	if (p_line.compare(2, 6, "earch ", 6) == 0)
	{
#ifndef _DEBUG
		const
#endif
		string l_line_item = p_line.substr(0, p_pos_next_separator);
		auto l_marker_tth = l_line_item.find("?0?9?TTH:");
		// TODO ��������� ������������ ����� �� ������� ����
		// "x.x.x.x:yyy T?F?57671680?9?TTH:A3VSWSWKCVC4N6EP2GX47OEMGT5ZL52BOS2LAHA"
		if (l_marker_tth != string::npos &&
		        l_marker_tth > 5 &&
		        l_line_item[l_marker_tth - 4] == ' ' &&
		        l_line_item.size() >= l_marker_tth + 9 + 39
		   ) // �������� �� ������ �������  F?T?0?9?TTH: ��� F?F?0?9?TTH: ��� T?T?0?9?TTH:
		{
			dcassert(l_line_item.size() == l_marker_tth + 9 + 39 ||
			         l_line_item.size() == l_marker_tth + 9 + 40
			        );
			l_marker_tth -= 4;
#ifdef _DEBUG
			static FastCriticalSection g_stat_cs;
			static std::unordered_map<TTHValue, unsigned> g_tth_count;
			const string l_tth_str = l_line_item.substr(l_marker_tth + 13, 39);
			const TTHValue l_tth_orig(l_tth_str);
			unsigned l_count_tth = 0;
			{
				CFlyFastLock(g_stat_cs);
				l_count_tth = ++g_tth_count[l_tth_orig];
			}
#endif
			const TTHValue l_tth(l_line_item.c_str() + l_marker_tth + 13, 39);
			//dcassert(l_tth == l_tth_orig);
			if (ShareManager::isUnknownTTH(l_tth) == false)
			{
				const string l_search_str = l_line_item.substr(8, l_marker_tth - 8);
				dcassert(l_search_str.size() > 4);
				if (l_search_str.size() > 4)
				{
					p_tth_search.emplace_back(CFlySearchItemTTH(l_tth, l_search_str));
					dcassert(p_tth_search.back().m_search.find('|') == string::npos && p_tth_search.back().m_search.find('$') == string::npos);
				}
			}
			else
			{
#ifdef _DEBUG
				static unsigned g_count_skip = 0;
				++g_count_skip;
				if (DebugManager::g_isCMDDebug)
				{
					l_line_item = "[count All = " + Util::toString(g_count_skip) + "] "
					              + "[count TTH = " + Util::toString(l_count_tth) + "] "
					              + "[size_map = "   + Util::toString(g_tth_count.size()) + "] "
					              + l_line_item;
				}
#endif
				COMMAND_DEBUG("[TTH][FastSkip]" + l_line_item, DebugTask::HUB_IN, getServerAndPort());
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
					const string l_error = "[" + Util::formatDigitalDate() + "] BufferedSocket::all_search_parser DDoS $Search command: " + l_line_item + " Hub IP = " + getIp();
					CFlyServerJSON::pushError(20, l_error);
					LogManager::message(l_error);
					if (!m_count_search_ddos)
					{
						fly_fire1(BufferedSocketListener::DDoSSearchDetect(), l_error);
					}
					m_count_search_ddos++;
				}
				COMMAND_DEBUG("[DDoS] " + l_line_item, DebugTask::HUB_IN, getServerAndPort());
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
			const bool l_is_valid_search = l_item.is_parse_nmdc_search(l_line_item.substr(8));
			if (l_is_valid_search)
			{
				if (CFlyServerConfig::g_detect_search_bot.find(l_item.m_filter) != CFlyServerConfig::g_detect_search_bot.end())
				{
					if (ShareManager::addSearchBot(l_item) == 1)
					{
						COMMAND_DEBUG("[File][SearchBot-First]" + l_line_item, DebugTask::HUB_IN, getServerAndPort());
					}
				}
				if (ShareManager::getCountSearchBot(l_item) > 1)
				{
					COMMAND_DEBUG("[File][SearchBot-BAN]" + l_line_item, DebugTask::HUB_IN, getServerAndPort());
					return true;
				}
				if (ShareManager::isUnknownFile(l_item.getRAWQuery()))
				{
#ifdef _DEBUG
					static unsigned g_count_skip = 0;
					++g_count_skip;
					if (DebugManager::g_isCMDDebug)
					{
						l_line_item = "[count = " + Util::toString(g_count_skip) + "] " + l_line_item;
					}
#endif
					COMMAND_DEBUG("[File][FastSkip][Unknown files]" + l_line_item, DebugTask::HUB_IN, getServerAndPort());
#ifdef _DEBUG
//						LogManager::message("BufferedSocket::all_search_parser Skip unknown File = " + l_item.m_raw_search + " count_dup = " + Util::toString(l_count_dup));
#endif
				}
				else
				{
#if 0
					int l_count_dup = 0;
					std::map<string, int> l_stat_map;
					{
						static CriticalSection g_debug_cs;
						static boost::unordered_map<string, std::pair<int, std::map<string, int> > > g_count_dup_ip_port;
						if (!l_item.m_is_passive)
						{
							CFlyLock(g_debug_cs);
							l_count_dup = g_count_dup_ip_port[l_item.m_seeker].first++;
							g_count_dup_ip_port[l_item.m_seeker].second[l_item.m_filter]++;
							l_stat_map = g_count_dup_ip_port[l_item.m_seeker].second;
						}
					}
					if (l_count_dup > 1)
					{
						l_line_item += " count_dup = " + Util::toString(l_count_dup);
						
						if (!l_stat_map.empty())
						{
							l_line_item += " [";
							for (auto i = l_stat_map.cbegin(), iend = l_stat_map.cend(); i != iend; ++i)
							{
								l_line_item += "'" + i->first + "' = " + Util::toString(i->second);
							}
							l_line_item.push_back(']');
						}
					}
					COMMAND_DEBUG("[File][Valid][files] " + l_line_item, DebugTask::HUB_IN, getServerAndPort());
#endif
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
		return true;
	}
	else
	{
		if (p_line.size() >= 45 && p_line[3] == ' ' && (p_line[2] == 'P' || p_line[2] == 'A') && p_line[43] == ' ')
		{
			const TTHValue l_tth(p_line.c_str() + 4, 39);
			if (ShareManager::isUnknownTTH(l_tth) == false)
			{
				const auto l_end_cmd = p_line.find('|', 43);
				if (l_end_cmd != string::npos)
				{
					string l_search_str = p_line.substr(44, l_end_cmd - 44);
					if (p_line[2] == 'P')
						l_search_str = "Hub:" + l_search_str;
					dcassert(l_search_str.size() > 4);
					if (l_search_str.size() > 4)
					{
						p_tth_search.emplace_back(CFlySearchItemTTH(l_tth, l_search_str));
					}
				}
			}
			else
			{
				string l_line_item = p_line.substr(0, p_pos_next_separator);
				COMMAND_DEBUG("[TTHS][FastSkip]" + l_line_item, DebugTask::HUB_IN, getServerAndPort());
#ifdef _DEBUG
				//  LogManager::message("BufferedSocket::all_search_parser Skip unknown TTH = " + l_tth.toBase32());
#endif
			}
			
			return true;
		}
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
			dcassert(l_line_item.compare(0, 5, "$ALL ", 5) == 0);
			if (!l_line_item.empty())
			{
				++m_myInfoCount;
				p_all_myInfo.push_back(l_line_item);
			}
		}
		else if (m_myInfoCount)
		{
			if (!p_all_myInfo.empty())
			{
				if (m_is_disconnecting == false)
				{
					fly_fire1(BufferedSocketListener::MyInfoArray(), p_all_myInfo); // todo zmq
				}
			}
			set_all_my_info_loaded(); // ���������� ��������� ����� $MyINFO
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
			if (l_line_item.length() && ClientManager::isBeforeShutdown())
			{
				if (!(l_line_item[0] == '<' || l_line_item[0] == '$' || l_line_item[l_line_item.length() - 1] == '|'))
				{
					LogManager::message("OnLine: " + l_line_item);
				}
			}
#endif
			if (!ClientManager::isBeforeShutdown())
			{
				//dcassert(m_is_disconnecting == false)
				if (m_is_disconnecting == false)
				{
					fly_fire1(BufferedSocketListener::Line(), l_line_item); // TODO - ���������� �� ��������� ���������� l � ��������� �� ���� inbuf
				}
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
                                            snprintf(bbb, sizeof(bbb), "$ALL Guest%d <<Peers V:(r622),M:P,H:1/0/0,S:15,C:��������>$ $%c$$3171624055$", i, 5);
                                            l_all_myInfo.push_back(bbb);
                                        }
                                    }
#endif // FLYLINKDC_EMULATOR_4000_USERS
#ifdef FLYLINKDC_EMULATOR_4000_USERS
// ���������� ��������� IP-������
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
void BufferedSocket::parseMyINfo(
    StringList& p_all_myInfo)
{
	if (!p_all_myInfo.empty())
	{
		if (m_is_disconnecting == false)
		{
			fly_fire1(BufferedSocketListener::MyInfoArray(), p_all_myInfo); // todo zmq
		}
	}
}
void BufferedSocket::parseSearch(
    CFlySearchArrayTTH& p_tth_search,
    CFlySearchArrayFile& p_file_search)
{
	if (!p_tth_search.empty())
	{
		if (m_is_disconnecting == false)
		{
			fly_fire1(BufferedSocketListener::SearchArrayTTH(), p_tth_search);
		}
	}
	if (!p_file_search.empty())
	{
		if (m_is_disconnecting == false)
		{
			fly_fire1(BufferedSocketListener::SearchArrayFile(), p_file_search);
		}
	}
}

void BufferedSocket::threadRead()
{
	if (m_state != RUNNING)
		return;
	try
	{
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
#define FLYLINKDC_USE_MYINFO_ARRAY
#ifdef FLYLINKDC_USE_MYINFO_ARRAY
#ifdef _DEBUG
					// LogManager::message("BufferedSocket::threadRead[MODE_ZPIPE] = " + l);
#endif
					//
					if (!ClientManager::isBeforeShutdown())
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
						parseMyINfo(l_all_myInfo);
						parseSearch(l_tth_search, l_file_search);
#else
					// process all lines
					while ((pos = l.find(m_separator)) != string::npos)
					{
						if (pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
							fly_fire1(__FUNCTION__, BufferedSocketListener::Line(), l.substr(0, pos));
						l.erase(0, pos + 1 /* separator char */); // TODO �� ����������
					}
#endif
					}
					else
					{
#ifdef _DEBUG
						const string l_log = "Skip MODE_LINE [after ZLIB] - FlylinkDC++ Destroy... l = " + l;
						LogManager::message(l_log);
#endif
						throw SocketException(STRING(COMMAND_SHUTDOWN_IN_PROGRESS));
					}
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
					// TODO - �������� ������� ��������� ������ �� TTH ��� ������ ����������
					// ���� ����� - �������� � ����� �����
					// ���� ����� - ������ ������� UDP (���� ����� �������?)
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
					if (!ClientManager::isBeforeShutdown())
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
							if (ClientManager::isBeforeShutdown())
							{
								m_line.clear();
								throw SocketException(STRING(COMMAND_SHUTDOWN_IN_PROGRESS));
							}
							if (l_pos > 0) // check empty (only pipe) command and don't waste cpu with it ;o)
							{
								if (all_search_parser(l_pos, l, l_tth_search, l_file_search) == false)
								{
									all_myinfo_parser(l_pos, l, l_all_myInfo, false);
								}
							}
							l.erase(0, l_pos + 1 /* separator char */);
							// TODO - erase �� ����������.
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
						parseMyINfo(l_all_myInfo);
						parseSearch(l_tth_search, l_file_search);
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
						m_line.clear();
						throw SocketException(STRING(COMMAND_SHUTDOWN_IN_PROGRESS));
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
							// fly_fire2(BufferedSocketListener::Data(), &m_inbuf[l_bufpos], l_left);
							dcassert(m_connection);
							if (m_connection)
							{
								m_connection->fireData(&m_inbuf[l_bufpos], l_left);
							}
							l_bufpos += (l_left - m_rollback);
							l_left = m_rollback;
							m_rollback = 0;
						}
						else
						{
							const int high = (int)std::min(m_dataBytes, (int64_t)l_left);
							//dcassert(high != 0);
							if (high != 0) // [+] IRainman fix.
							{
								//fly_fire2(BufferedSocketListener::Data(), &m_inbuf[l_bufpos], high);
								dcassert(m_connection);
								if (m_connection)
								{
									m_connection->fireData(&m_inbuf[l_bufpos], high);
								}
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
	catch (const std::bad_alloc&) // fix https://drdump.com/Problem.aspx?ProblemID=254736
	{
		ShareManager::tryFixBadAlloc();
		throw SocketException(STRING(BAD_ALLOC));
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
		const auto l_ip = boost::asio::ip::address_v4::from_string(sock->getIp(), ec); // TODO - ����������� IP � � �������
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
		p_sock->m_connection = nullptr;
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
	while (g_sockets > 0)
	{
		sleep(10);
	}
}
#endif

void BufferedSocket::threadSendFile(InputStream* p_file)
{
	if (m_state != RUNNING)
		return;
		
	if (socketIsDisconnecting())
		return;
	dcassert(p_file != NULL);
	
	const size_t l_sockSize = MAX_SOCKET_BUFFER_SIZE; // �������� ������ size_t(sock->getSocketOptInt(SO_SNDBUF));
	static size_t g_bufSize = 0;
	if (g_bufSize == 0)
	{
		g_bufSize = std::max(l_sockSize, size_t(MAX_SOCKET_BUFFER_SIZE));
	}
	
	ByteVector l_readBuf; // TODO �������� �� - �� ����� ������ 0-��� std::unique_ptr<uint8_t[]> buf(new uint8_t[BUFSIZE]);
	ByteVector l_writeBuf;
	bool l_is_bad_alloc = false;
	do
	{
		try
		{
			l_is_bad_alloc = false;
			l_readBuf.resize(g_bufSize);
			l_writeBuf.resize(g_bufSize);
		}
		catch (std::bad_alloc&)
		{
			ShareManager::tryFixBadAlloc();
			g_bufSize /= 2;
			l_is_bad_alloc = g_bufSize > 1024;
			if (l_is_bad_alloc == false)
			{
				throw;
			}
		}
	}
	while (l_is_bad_alloc == true);
	
	size_t readPos = 0;
	bool readDone = false;
	dcdebug("Starting threadSend\n");
	while (!socketIsDisconnecting())
	{
		if (!readDone && l_readBuf.size() > readPos)
		{
			size_t bytesRead = l_readBuf.size() - readPos;
			size_t actual = p_file->read(&l_readBuf[readPos], bytesRead); // TODO ����� ������ ��� ������� ��������� ����� � ����
#ifdef _DEBUG
			if (actual)
			{
				std::ofstream l_fs;
				l_fs.open(_T("flylinkdc-buffered-socket.log"), std::ifstream::out | std::ifstream::app);
				if (l_fs.good())
				{
					const string l_str = std::string((const char*) &l_readBuf[readPos], actual);
					l_fs << std::endl << std::endl << std::endl << " Body: [" << l_str << "]" << std::endl;
				}
				else
				{
					//dcassert(0);
				}
			}
#endif
			
			if (bytesRead > 0)
			{
				dcassert(m_connection);
				if (m_connection)
				{
					m_connection->fireBytesSent(bytesRead, 0);
				}
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
		if (readDone && readPos == 0)
		{
			fly_fire(BufferedSocketListener::TransmitDone());
			return;
		}
		l_readBuf.swap(l_writeBuf);
		l_readBuf.resize(g_bufSize);
		l_writeBuf.resize(readPos); // TODO - l_writeBuf �������� ���� � ���������� ������� �����.
		readPos = 0;
		
		size_t writePos = 0, writeSize = 0;
		int written = 0;
		
		while (writePos < l_writeBuf.size())
		{
			if (socketIsDisconnecting())
				return;
				
			if (written == -1)
			{
				// workaround for OpenSSL (crashes when previous write failed and now retrying with different writeSize)
				written = sock->write(&l_writeBuf[writePos], writeSize);
			}
			else
			{
				writeSize = std::min(l_sockSize / 2, l_writeBuf.size() - writePos);
				written = ThrottleManager::getInstance()->write(sock.get(), &l_writeBuf[writePos], writeSize);
#ifdef _DEBUG
				COMMAND_DEBUG("BufferedSocket: write bytes = " + Util::toString(written), DebugTask::CLIENT_OUT, getRemoteIpPort());
#endif
			}
			
			if (written > 0)
			{
				writePos += written;
				dcassert(m_connection);
				if (m_connection)
				{
					m_connection->fireBytesSent(0, written);
				}
			}
			else if (written == -1)
			{
				if (!readDone && readPos < l_readBuf.size())
				{
					size_t bytesRead = std::min(l_readBuf.size() - readPos, l_readBuf.size() / 2);
					size_t actual = p_file->read(&l_readBuf[readPos], bytesRead);
					if (bytesRead > 0)
					{
						dcassert(m_connection);
						if (m_connection)
						{
							m_connection->fireBytesSent(bytesRead, 0);
						}
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
					while (!socketIsDisconnecting())
					{
						auto w = sock->wait(POLL_TIMEOUT, true, true);
						if (w.first) {
							threadRead();
						}
						if (w.second) {
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
	/*
	   //dcassert(!ClientManager::isBeforeShutdown());
	    if (ClientManager::isBeforeShutdown())
	    {
	#ifdef _DEBUG
	        LogManager::message("[ClientManager::isBeforeShutdown()]Skip BufferedSocket::write! Data = " + string(aBuf, aLen));
	#endif
	        return;
	    }
	*/
	
	if (!hasSocket())
	{
		dcassert(0);
		return;
	}
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
	try
	{
		m_writeBuf.reserve(m_writeBuf.size() + aLen);
	}
	catch (std::bad_alloc&)
	{
		ShareManager::tryFixBadAlloc();
	}
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
		if (ClientManager::isBeforeShutdown())
		{
#ifdef _DEBUG
			LogManager::message("[ClientManager::isBeforeShutdown()]Skip BufferedSocket::threadSendData Data = " + string((const char*)&l_sendBuf[0], l_sendBuf.size()));
#endif
		}
		if (socketIsDisconnecting())
		{
			return;
		}
		
		const auto w = sock->wait(POLL_TIMEOUT, true, true);
		
		if (w.first) {
			threadRead();
		}
		
		if (w.second) {
			// TODO - find ("||")
			int n = sock->write(&l_sendBuf[done], left);  // adguard - https://www.box.net/shared/9201edaa1fa1b83a8d3c
			if (n > 0) {
				left -= n;
				done += n;
			}
		}
	}
}

bool BufferedSocket::checkEvents()
{
	while (m_state == RUNNING ? m_socket_semaphore.wait(0) : m_socket_semaphore.wait())
	{
		//dcassert(!ClientManager::isShutdown());
		pair<Tasks, std::unique_ptr<TaskData>> p;
		{
			CFlyFastLock(cs);
			if (!m_tasks.empty())
			{
				swap(p, m_tasks.front());
				m_tasks.pop_front();
			}
			else
			{
				dcassert(!m_tasks.empty());
				return false;
			}
		}
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
				threadSendFile(static_cast<SendFileInfo*>(p.second.get())->m_stream);
				break;
			}
			else if (p.first == DISCONNECT)
			{
				fail(STRING(DISCONNECTED));
			}
			else if (p.first == SHUTDOWN)
			{
				return false;
			}
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
			else if (p.first == SHUTDOWN)
			{
				return false;
			}
			else
			{
				dcdebug("%d unexpected in STARTING state\n", p.first);
			}
		}
		else
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
	}
	return true;
}

void BufferedSocket::checkSocket() {
	auto w = sock->wait(POLL_TIMEOUT, true, false);
	
	if (w.first) {
		threadRead();
	}
}


/**
 * Main task dispatcher for the buffered socket abstraction.
 * @todo Fix the polling...
 */
int BufferedSocket::run()
{
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
#ifdef _DEBUG
			LogManager::message("BufferedSocket::run(), error = " + e.getError());
#endif
			fail(e.getError());
		}
	}
	dcdebug("BufferedSocket::run() end %p\n", (void*)this);
	delete this;
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
		// dcassert(!ClientManager::isBeforeShutdown());
		// fix https://drdump.com/Problem.aspx?ProblemID=112938
		// fix https://drdump.com/Problem.aspx?ProblemID=112262
		// fix https://drdump.com/Problem.aspx?ProblemID=112195
		// ������ - �������� if (!ClientManager::isBeforeShutdown())
		{
			fly_fire1(BufferedSocketListener::Failed(), aError);
		}
	}
}

void BufferedSocket::shutdown()
{
	m_is_disconnecting = true;
	addTask(SHUTDOWN, nullptr);
}

void BufferedSocket::addTask(Tasks p_task, TaskData* p_data)
{
	CFlyFastLock(cs);
	addTaskL(p_task, p_data);
}
void BufferedSocket::addTaskL(Tasks p_task, TaskData* p_data)
{
	dcassert(p_task == DISCONNECT || p_task == SHUTDOWN || p_task == UPDATED || sock.get());
	if (p_task == DISCONNECT && !m_tasks.empty())
	{
		if (m_tasks.back().first == DISCONNECT)
		{
			//dcassert(0);
			return;
		}
	}
#ifdef _DEBUG
	if (p_task == SHUTDOWN && !m_tasks.empty())
	{
		if (m_tasks.back().first == SHUTDOWN)
		{
			dcassert(0);
			return;
		}
	}
	if (p_task == UPDATED && !m_tasks.empty())
	{
		if (m_tasks.back().first == UPDATED)
		{
			dcassert(0);
			return;
		}
	}
	if (p_task == SEND_DATA && !m_tasks.empty())
	{
		if (m_tasks.back().first == SEND_DATA)
		{
			dcassert(0);
			return;
		}
	}
	if (p_task == ACCEPTED && !m_tasks.empty())
	{
		if (m_tasks.back().first == ACCEPTED)
		{
			dcassert(0);
			return;
		}
	}
#endif
	
	m_tasks.push_back(std::make_pair(p_task, std::unique_ptr<TaskData>(p_data)));
	m_socket_semaphore.signal();
}

/**
 * @file
 * $Id: BufferedSocket.cpp 582 2011-12-18 10:00:11Z bigmuscle $
 */
