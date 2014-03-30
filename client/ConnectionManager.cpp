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
#include "ConnectionManager.h"
#include "DownloadManager.h"
#include "UploadManager.h"
#include "CryptoManager.h"
#include "QueueManager.h"

#ifdef RIP_USE_CONNECTION_AUTODETECT
#include "nmdchub.h"
#endif
#include "../FlyFeatures/flyServer.h"

uint16_t ConnectionManager::iConnToMeCount = 0;
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csConnection = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csDownloads = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csUploads = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csDdosCheck = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csTTHFilter = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());

boost::unordered_set<UserConnection*> ConnectionManager::g_userConnections;
ConnectionQueueItem::List ConnectionManager::g_downloads;
ConnectionQueueItem::List ConnectionManager::g_uploads;


ConnectionManager::ConnectionManager() : m_floodCounter(0), server(nullptr),
	secureServer(nullptr),
	shuttingDown(false)
{
	nmdcFeatures.reserve(5);
	nmdcFeatures.push_back(UserConnection::FEATURE_MINISLOTS);
	nmdcFeatures.push_back(UserConnection::FEATURE_XML_BZLIST);
	nmdcFeatures.push_back(UserConnection::FEATURE_ADCGET);
	nmdcFeatures.push_back(UserConnection::FEATURE_TTHL);
	nmdcFeatures.push_back(UserConnection::FEATURE_TTHF);
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
	nmdcFeatures.push_back(UserConnection::FEATURE_BANMSG); // !SMT!-B
#endif
	adcFeatures.reserve(4);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BAS0);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BASE);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_TIGR);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BZIP);
	
	TimerManager::getInstance()->addListener(this); // [+] IRainman fix.
}
ConnectionManager::~ConnectionManager()
{
	dcassert(shuttingDown == true);
	dcassert(g_userConnections.empty());
	//shutdown();
	
	// [!] http://code.google.com/p/flylinkdc/issues/detail?id=1037
	dcassert(g_downloads.empty());
	dcassert(g_uploads.empty());
	// [~]
}

void ConnectionManager::listen()
{
	disconnect();
	uint16_t port;
	
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		server = new Server(false, 0, Util::emptyString);
	}
	else
	{
		port = static_cast<uint16_t>(SETTING(TCP_PORT));
		server = new Server(false, port, SETTING(BIND_ADDRESS));
	}
	
	if (!CryptoManager::getInstance()->TLSOk())
	{
		LogManager::getInstance()->message("Skipping secure port: " + Util::toString(SETTING(USE_TLS)));
		dcdebug("Skipping secure port: %d\n", SETTING(USE_TLS));
		return;
	}
	
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		secureServer = new Server(true, 0, Util::emptyString);
	}
	else
	{
		port = static_cast<uint16_t>(SETTING(TLS_PORT));
		secureServer = new Server(true, port, SETTING(BIND_ADDRESS));
	}
}

/**
 * Request a connection for downloading.
 * DownloadManager::addConnection will be called as soon as the connection is ready
 * for downloading.
 * @param aUser The user to connect to.
 */
void ConnectionManager::getDownloadConnection(const UserPtr& aUser)
{
	dcassert(aUser);
	// [!] IRainman fix: Please do not mask the problem of endless checks on empty! If the user is empty - hence the functional is not working!
	// [-] if (aUser) //[+] PPA [-] IRainman fix.
	{
		webrtc::WriteLockScoped l(*g_csDownloads);
		const ConnectionQueueItem::Iter i = find(g_downloads.begin(), g_downloads.end(), aUser);
		if (i == g_downloads.end())
		{
			getCQI_L(HintedUser(aUser, Util::emptyString), true); // http://code.google.com/p/flylinkdc/issues/detail?id=1037
			// Ќе сохран€ем указатель. а как и когда будем удал€ть ?
			return;
		}
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		else
		{
			if (find(m_checkIdle.begin(), m_checkIdle.end(), aUser) == m_checkIdle.end())
			{
				m_checkIdle.push_back(aUser); // TODO - Ћок?
			}
		}
#endif
	}
#ifndef USING_IDLERS_IN_CONNECTION_MANAGER
	DownloadManager::getInstance()->checkIdle(aUser);
#endif
}

ConnectionQueueItem* ConnectionManager::getCQI_L(const HintedUser& aHintedUser, bool download)
{
	ConnectionQueueItem* cqi = new ConnectionQueueItem(aHintedUser, download);
	if (download)
	{
		dcassert(find(g_downloads.begin(), g_downloads.end(), aHintedUser) == g_downloads.end());
		g_downloads.push_back(cqi);
	}
	else
	{
		dcassert(find(g_uploads.begin(), g_uploads.end(), aHintedUser) == g_uploads.end());
		g_uploads.push_back(cqi);
	}
	
	fire(ConnectionManagerListener::Added(), cqi); // TODO - выбросить фаер без блокировки наруже
	return cqi;
}

void ConnectionManager::putCQI_L(ConnectionQueueItem* cqi)
{
	fire(ConnectionManagerListener::Removed(), cqi); // TODO - зоветс€ фаер под локом?
	if (cqi->getDownload())
	{
		g_downloads.erase_and_check(cqi);
	}
	else
	{
		UploadManager::getInstance()->removeDelayUpload(cqi->getUser());
		g_uploads.erase_and_check(cqi);
	}
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	cqi->getUser()->flushRatio(); //[+]PPA branches-dev/ppa/issue-1035
#endif
	delete cqi;
}

#if 0
bool ConnectionManager::getCipherNameAndIP(UserConnection* p_conn, string& p_chiper_name, string& p_ip)
{
	webrtc::ReadLockScoped l(*g_csConnection);
	const auto l_conn = g_userConnections.find(p_conn);
	dcassert(l_conn != g_userConnections.end());
	if (l_conn != g_userConnections.end())
	{
		p_chiper_name = p_conn->getCipherName();
		p_ip = p_conn->getRemoteIp(); // TODO - перевести на boost?
		return true;
	}
	return false;
}
#endif
UserConnection* ConnectionManager::getConnection(bool aNmdc, bool secure) noexcept
{
	dcassert(shuttingDown == false);
	UserConnection* uc = new UserConnection(secure);
	uc->addListener(this);
	{
		webrtc::WriteLockScoped l(*g_csConnection);
		dcassert(g_userConnections.find(uc) == g_userConnections.end());
		g_userConnections.insert(uc);
	}
	if (aNmdc)
	{
		uc->setFlag(UserConnection::FLAG_NMDC);
	}
	return uc;
}

void ConnectionManager::putConnection(UserConnection* aConn)
{
	aConn->removeListener(this);
	aConn->disconnect(true);
	webrtc::WriteLockScoped l(*g_csConnection);
	dcassert(g_userConnections.find(aConn) != g_userConnections.end());
	g_userConnections.erase(aConn);
}

void ConnectionManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (((aTick / 1000) % (CFlyServerConfig::g_max_unique_tth_search + 2)) == 0)
	{
		cleanupTTHDuplicateSearch(aTick);
	}
	
	ConnectionQueueItem::List l_removed;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	UserList l_idlers;
#endif
	{
		webrtc::ReadLockScoped l(*g_csDownloads);
		uint16_t attempts = 0;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		l_idlers.swap(m_checkIdle); // [!] IRainman opt: use swap.
#endif
		for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
		{
			ConnectionQueueItem* cqi = *i;
			if (cqi->getState() != ConnectionQueueItem::ACTIVE) // crash - https://www.crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=44111
			{
				if (!cqi->getUser()->isOnline())
				{
					// Not online anymore...remove it from the pending...
					l_removed.push_back(cqi);
					continue;
				}
				
				if (cqi->getErrors() == -1 && cqi->getLastAttempt() != 0)
				{
					// protocol error, don't reconnect except after a forced attempt
					continue;
				}
				
				if (cqi->getLastAttempt() == 0 || ((SETTING(DOWNCONN_PER_SEC) == 0 || attempts < SETTING(DOWNCONN_PER_SEC)) &&
				                                   cqi->getLastAttempt() + 60 * 1000 * max(1, cqi->getErrors()) < aTick))
				{
					cqi->setLastAttempt(aTick);
					
					QueueItem::Priority prio = QueueManager::getInstance()->hasDownload(cqi->getUser()); // [10] https://www.box.net/shared/i6hgw2qzhr9zyy15vhh1
					
					if (prio == QueueItem::PAUSED)
					{
						l_removed.push_back(cqi);
						continue;
					}
					
					bool startDown = DownloadManager::getInstance()->startDownload(prio);
					
					if (cqi->getState() == ConnectionQueueItem::WAITING)
					{
						if (startDown)
						{
							const string hubHint = cqi->getHubUrl(); // TODO - прокинуть туда хинт на хаб
							cqi->setState(ConnectionQueueItem::CONNECTING);
							ClientManager::getInstance()->connect(HintedUser(cqi->getUser(), hubHint), cqi->getToken());
							fire(ConnectionManagerListener::StatusChanged(), cqi);
							attempts++;
						}
						else
						{
							cqi->setState(ConnectionQueueItem::NO_DOWNLOAD_SLOTS);
							fire(ConnectionManagerListener::Failed(), cqi, STRING(ALL_DOWNLOAD_SLOTS_TAKEN));
						}
					}
					else if (cqi->getState() == ConnectionQueueItem::NO_DOWNLOAD_SLOTS && startDown)
					{
						cqi->setState(ConnectionQueueItem::WAITING);
					}
				}
				else if (cqi->getState() == ConnectionQueueItem::CONNECTING && cqi->getLastAttempt() + 50 * 1000 < aTick)
				{
					ClientManager::getInstance()->connectionTimeout(cqi->getUser());
					
					cqi->setErrors(cqi->getErrors() + 1);
					fire(ConnectionManagerListener::Failed(), cqi, STRING(CONNECTION_TIMEOUT));
					cqi->setState(ConnectionQueueItem::WAITING);
				}
			}
		}
	}
	if (!l_removed.empty())
	{
		for (auto m = l_removed.cbegin(); m != l_removed.cend(); ++m)
		{
			if ((*m)->getDownload())
			{
				webrtc::WriteLockScoped l(*g_csDownloads);
				putCQI_L(*m);
			}
			else
			{
				webrtc::WriteLockScoped l(*g_csUploads);
				putCQI_L(*m);
			}
		}
	}
	
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	for (auto i = l_idlers.cbegin(); i != l_idlers.cend(); ++i)
	{
		DownloadManager::getInstance()->checkIdle(*i);
	}
#endif
}

void ConnectionManager::cleanupIpFlood(const uint64_t p_tick)
{
	webrtc::WriteLockScoped l_ddos(*g_csDdosCheck);
	for (auto j = m_ddos_map.cbegin(); j != m_ddos_map.cend();)
	{
		// ≈сли коннектов совершено меньше чем предел в течении минуты - убираем адрес из таблицы - с ним все хорошо!
		const auto l_tick_delta = p_tick - j->second.m_first_tick;
		const bool l_is_min_ban_close = j->second.m_count_connect < CFlyServerConfig::g_max_ddos_connect_to_me && l_tick_delta > 1000 * 60;
		if (l_is_min_ban_close)
		{
#ifdef _DEBUG
			LogManager::getInstance()->ddos_message("BlockID = " + Util::toString(j->second.m_block_id) + ", Removed mini-ban for: " +
			                                        j->first.first + j->second.getPorts() + ", Hub IP = " + j->first.second.to_string() +
			                                        " m_ddos_map.size() = " + Util::toString(m_ddos_map.size()));
#endif
		}
		// ≈сли коннектов совершено много и IP находитс€ в бане, но уже прошло врем€ больше чем 10 ћинут(по умолчанию)
		// “акже убираем запись из таблицы блокировки
		const bool l_is_ddos_ban_close = j->second.m_count_connect > CFlyServerConfig::g_max_ddos_connect_to_me
		                                 && l_tick_delta > CFlyServerConfig::g_ban_ddos_connect_to_me * 1000 * 60;
		if (l_is_ddos_ban_close)
		{
			string l_type;
			if (j->first.second.is_unspecified()) // ≈сли нет второго IP то это команада  ConnectToMe
			{
				l_type =  "IP-1:" + j->first.first + j->second.getPorts();
			}
			else
			{
				l_type = " IP-1:" + j->first.first + j->second.getPorts() + " IP-2: " + j->first.second.to_string();
			}
			LogManager::getInstance()->ddos_message("BlockID = " + Util::toString(j->second.m_block_id) + ", Removed DDoS lock " + j->second.m_type_block +
			                                        ", Count connect = " + Util::toString(j->second.m_count_connect) + l_type +
			                                        ", m_ddos_map.size() = " + Util::toString(m_ddos_map.size()));
		}
		if (l_is_ddos_ban_close || l_is_min_ban_close)
			m_ddos_map.erase(j++);
		else
			++j;
	}
}
void ConnectionManager::cleanupTTHDuplicateSearch(const uint64_t p_tick)
{
	webrtc::WriteLockScoped l_lock(*g_csTTHFilter);
	
	for (auto j = m_tth_duplicate_search.cbegin(); j != m_tth_duplicate_search.cend();)
	{
		if ((p_tick - j->second.m_first_tick) > 1000 * CFlyServerConfig::g_max_unique_tth_search)
		{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_TTH_SEARCH
			if (j->second.m_count_connect > 1) // —обытие возникало больше одного раза - логируем?
			{
				LogManager::getInstance()->ddos_message(string(j->second.m_count_connect, '*') + " BlockID = " + Util::toString(j->second.m_block_id) +
				                                        ", Unlock duplicate TTH search: " + j->first +
				                                        ", Count connect = " + Util::toString(j->second.m_count_connect) +
				                                        ", Hash map size: " + Util::toString(m_tth_duplicate_search.size()));
			}
#endif
			m_tth_duplicate_search.erase(j++);
		}
		else
			++j;
	}
}
void ConnectionManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept
{
	cleanupIpFlood(aTick);
	
	webrtc::ReadLockScoped l(*g_csConnection);
	
	for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
	{
		auto& l_connection = *j;
#ifdef _DEBUG
		if ((l_connection->getLastActivity() + 180 * 1000) < aTick)
#else
		if ((l_connection->getLastActivity() + 180 * 1000) < aTick) // «ачем так много минут висеть?
#endif
		{
			l_connection->disconnect(true);
		}
	}
}

static const uint64_t FLOOD_TRIGGER = 20000;
static const uint64_t FLOOD_ADD = 2000;

ConnectionManager::Server::Server(bool p_secure
                                  , uint16_t p_port, const string& p_ip /* = "0.0.0.0" */) :
	m_secure(p_secure),
	m_die(false)
{
	m_sock.create();
	m_sock.setSocketOpt(SO_REUSEADDR, 1);
	m_port = m_sock.bind(p_port, p_ip); // [7] Wizard https://www.box.net/shared/45acc9cef68ecb499cb5
	m_sock.listen();
	start(64);
}

static const uint64_t POLL_TIMEOUT = 250;

int ConnectionManager::Server::run() noexcept
{
	while (!m_die)
	{
		try
		{
			while (!m_die)
			{
				auto ret = m_sock.wait(POLL_TIMEOUT, Socket::WAIT_READ);
				if (ret == Socket::WAIT_READ)
				{
					ConnectionManager::getInstance()->accept(m_sock, m_secure);
				}
			}
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message(STRING(LISTENER_FAILED) + ' ' + e.getError());
		}
		bool failed = false;
		while (!m_die)
		{
			try
			{
				m_sock.disconnect();
				m_sock.create();
				m_sock.bind(m_port, m_ip);
				m_sock.listen();
				if (failed)
				{
					LogManager::getInstance()->message(STRING(CONNECTIVITY_RESTORED));
					failed = false;
				}
				break;
			}
			catch (const SocketException& e)
			{
				dcdebug("ConnectionManager::Server::run Stopped listening: %s\n", e.getError().c_str());
				
				if (!failed)
				{
					LogManager::getInstance()->message(STRING(CONNECTIVITY_ERROR) + ' ' + e.getError());
					failed = true;
				}
				
				// Spin for 60 seconds
				for (int i = 0; i < 60 && !m_die; ++i)
				{
					sleep(1000);
				}
			}
		}
	}
	return 0;
}

/**
 * Someone's connecting, accept the connection and wait for identification...
 * It's always the other fellow that starts sending if he made the connection.
 */
void ConnectionManager::accept(const Socket& sock, bool secure) noexcept
{
	uint32_t now = GET_TICK();
	
	if (iConnToMeCount > 0)
		iConnToMeCount--;
		
	if (now > m_floodCounter)
	{
		m_floodCounter = now + FLOOD_ADD;
	}
	else
	{
		if (now + FLOOD_TRIGGER < m_floodCounter)// [!] IRainman fix
		{
			Socket s;
			try
			{
				s.accept(sock);
			}
			catch (const SocketException&)
			{
				// ...
			}
			LogManager::getInstance()->message("Connection flood detected, port = " + Util::toString(sock.getPort()) + " IP = " + sock.getIp());
			dcdebug("Connection flood detected!\n");
			return;
		}
		else
		{
			if (iConnToMeCount <= 0)
				m_floodCounter += FLOOD_ADD;
		}
	}
	UserConnection* uc = getConnection(false, secure);
	uc->setFlag(UserConnection::FLAG_INCOMING);
	uc->setState(UserConnection::STATE_SUPNICK);
	uc->setLastActivity();
	try
	{
		uc->accept(sock);
	}
	catch (const Exception&)
	{
		deleteConnection(uc);
	}
}
bool ConnectionManager::checkTTHDuplicateSearch(const string& p_search_command)
{
	webrtc::WriteLockScoped l_lock(*g_csTTHFilter);
	const auto l_tick = GET_TICK();
	CFlyTTHTick l_item;
	l_item.m_first_tick = l_tick;
	l_item.m_last_tick  = l_tick;
	auto l_result = m_tth_duplicate_search.insert(std::pair<string, CFlyTTHTick>(p_search_command, l_item));
	auto& l_cur_value = l_result.first->second;
	++l_cur_value.m_count_connect;
	if (l_result.second == false) // Ёлемент уже существует - проверим его счетчик и старость.
	{
		l_cur_value.m_last_tick  = l_tick;
		if (l_tick - l_cur_value.m_first_tick > 1000 * CFlyServerConfig::g_max_unique_tth_search)
		{
			// “ут можно сразу стереть элемент устаревший
			return false;
		}
		if (l_cur_value.m_count_connect > 1)
		{
			static uint16_t g_block_id = 0;
			if (l_cur_value.m_block_id == 0)
			{
				l_cur_value.m_block_id = ++g_block_id;
			}
			if (l_cur_value.m_count_connect >= 2)
			{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_TTH_SEARCH
				LogManager::getInstance()->ddos_message(string(l_cur_value.m_count_connect, '*') + " BlockID = " + Util::toString(l_cur_value.m_block_id) +
				                                        ", Lock TTH search = " + p_search_command +
				                                        ", Count = " + Util::toString(l_cur_value.m_count_connect) +
				                                        ", Hash map size: " + Util::toString(m_tth_duplicate_search.size()));
#endif
			}
			return true;
		}
	}
	return false;
}
bool ConnectionManager::checkIpFlood(const string& aIPServer, uint16_t aPort, const boost::asio::ip::address_v4& p_ip_hub, const string& p_userInfo, const string& p_HubInfo)
{
	{
		const auto l_tick = GET_TICK();
		// boost::system::error_code ec;
		// const auto l_ip = boost::asio::ip::address_v4::from_string(aIPServer, ec);
		const CFlyDDOSkey l_key(aIPServer, p_ip_hub);
		// dcassert(!ec); // TODO - тут бывает и Host
		
		webrtc::WriteLockScoped l_ddos(*g_csDdosCheck);
		CFlyDDoSTick l_item;
		l_item.m_first_tick = l_tick;
		l_item.m_last_tick = l_tick;
		auto l_result = m_ddos_map.insert(std::pair<CFlyDDOSkey, CFlyDDoSTick>(l_key, l_item));
		auto& l_cur_value = l_result.first->second;
		++l_cur_value.m_count_connect;
		string l_debug_key = " Time: " + Util::getShortTimeString() + " Hub info = " + p_HubInfo;
		if (!p_userInfo.empty())
		{
			l_debug_key + " UserInfo = [" + p_userInfo + "]";
		}
		l_cur_value.m_original_query_for_debug[l_debug_key]++;
		if (l_result.second == false)
		{
			// Ёлемент уже существует
			l_cur_value.m_last_tick = l_tick;   //  орректируем врем€ последней активности.
			l_cur_value.m_ports.insert(aPort);  // —охраним последний порт
			if (l_cur_value.m_count_connect == CFlyServerConfig::g_max_ddos_connect_to_me) // ѕревысили кол-во коннектов по одному IP
			{
				const string l_info   = "[Count limit: " + Util::toString(CFlyServerConfig::g_max_ddos_connect_to_me) + "]\t";
				const string l_target = "[Target: " + aIPServer + l_cur_value.getPorts() + "]\t";
				const string l_user_info = !p_userInfo.empty() ? "[UserInfo: " + p_userInfo + "]\t"  : "";
				l_cur_value.m_type_block = "Type DDoS:" + p_ip_hub.is_unspecified() ? "[$ConnectToMe]" : "[$Search]";
				static uint16_t g_block_id = 0;
				l_cur_value.m_block_id = ++g_block_id;
				LogManager::getInstance()->ddos_message("BlockID=" + Util::toString(l_cur_value.m_block_id) + ", " + l_cur_value.m_type_block + p_HubInfo + l_info + l_target + l_user_info);
				for (auto k = l_cur_value.m_original_query_for_debug.cbegin() ; k != l_cur_value.m_original_query_for_debug.cend(); ++k)
				{
					LogManager::getInstance()->ddos_message("  Detail BlockID=" + Util::toString(l_cur_value.m_block_id) + " " + k->first + " Count:" + Util::toString(k->second)); // TODO - сдать дубликаты + показать кол-во
				}
				l_cur_value.m_original_query_for_debug.clear();
			}
			if (l_cur_value.m_count_connect >= CFlyServerConfig::g_max_ddos_connect_to_me)
			{
				if ((l_cur_value.m_last_tick - l_cur_value.m_first_tick) < CFlyServerConfig::g_ban_ddos_connect_to_me * 1000 * 60)
				{
					return true; // Ћочим этот коннект до наступлени€ амнистии. TODO - проверить эту часть внимательей
					// в след части фикса - проводить анализ протокола и коннекты на порты лочить на вечно.
				}
			}
		}
	}
	webrtc::ReadLockScoped l(*g_csConnection);
	
	// We don't want to be used as a flooding instrument
	int count = 0;
	for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
	{
	
		const UserConnection& uc = **j;
		
		if (uc.socket == nullptr || !uc.socket->hasSocket())
			continue;
			
		if (uc.getPort() == aPort && uc.getRemoteIp() == aIPServer)
		{
			if (++count >= 5)
			{
				// More than 5 outbound connections to the same addr/port? Can't trust that..
				// LogManager::getInstance()->message("ConnectionManager::connect Tried to connect more than 5 times to " + aIPServer + ":" + Util::toString(aPort));
				dcdebug("ConnectionManager::connect Tried to connect more than 5 times to %s:%hu, connect dropped\n", aIPServer.c_str(), aPort);
				return true;
			}
		}
	}
	return false;
}

void ConnectionManager::nmdcConnect(const string& aIPServer, uint16_t aPort, const string& aNick, const string& hubUrl,
                                    const string& encoding,
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
                                    bool stealth,
#endif
                                    bool secure)
{
	nmdcConnect(aIPServer, aPort, 0, BufferedSocket::NAT_NONE, aNick, hubUrl,
	            encoding,
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
	            stealth,
#endif
	            secure);
}

void ConnectionManager::nmdcConnect(const string& aIPServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aNick, const string& hubUrl,
                                    const string& encoding,
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
                                    bool stealth,
#endif
                                    bool secure)
{
	if (isShuttingDown())
		return;
		
	if (checkIpFlood(aIPServer, aPort, boost::asio::ip::address_v4(), "", "[Hub: " + hubUrl + "]"))
		return;
		
	UserConnection* uc = getConnection(true, secure); // [!] IRainman fix SSL connection on NMDC(S) hubs.
	uc->setToken(aNick);
	uc->setHubUrl(hubUrl);
	uc->setEncoding(encoding);
	uc->setState(UserConnection::STATE_CONNECT);
	uc->setFlag(UserConnection::FLAG_NMDC);
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
	if (stealth)
	{
		uc->setFlag(UserConnection::FLAG_STEALTH);
	}
#endif
	try
	{
		uc->connect(aIPServer, aPort, localPort, natRole);
	}
	catch (const Exception&)
	{
		deleteConnection(uc);
	}
}

void ConnectionManager::adcConnect(const OnlineUser& aUser, uint16_t aPort, const string& aToken, bool secure)
{
	adcConnect(aUser, aPort, 0, BufferedSocket::NAT_NONE, aToken, secure);
}

void ConnectionManager::adcConnect(const OnlineUser& aUser, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aToken, bool secure)
{
	if (isShuttingDown())
		return;
		
	if (checkIpFlood(aUser.getIdentity().getIpAsString(), aPort, boost::asio::ip::address_v4(), "", "[Hub: " + aUser.getClientBase().getHubName() + "]")) // "ADC Nick: " + aUser.getIdentity().getNick() +
		return;
		
	UserConnection* uc = getConnection(false, secure);
	uc->setToken(aToken);
	uc->setEncoding(Text::g_utf8);
	uc->setState(UserConnection::STATE_CONNECT);
	uc->setHubUrl(&aUser.getClient() == nullptr ? "DHT" : aUser.getClient().getHubUrl());
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	if (aUser.getIdentity().isOp())
	{
		uc->setFlag(UserConnection::FLAG_OP);
	}
#endif
	try
	{
		uc->connect(aUser.getIdentity().getIpAsString(), aPort, localPort, natRole);
	}
	catch (const Exception&)
	{
		deleteConnection(uc);
	}
}

void ConnectionManager::disconnect() noexcept
{
	safe_delete(server); // TODO «оветс€ чаще чем нужно.
	safe_delete(secureServer);
}


void ConnectionManager::on(AdcCommand::SUP, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	if (aSource->getState() != UserConnection::STATE_SUPNICK)
	{
		// Already got this once, ignore...@todo fix support updates
		dcdebug("CM::onSUP %p sent sup twice\n", (void*)aSource);
		return;
	}
	
	bool baseOk = false;
	bool tigrOk = false;
	
	for (auto i = cmd.getParameters().cbegin(); i != cmd.getParameters().cend(); ++i)
	{
		if (i->compare(0, 2, "AD", 2) == 0)
		{
			string feat = i->substr(2);
			if (feat == UserConnection::FEATURE_ADC_BASE || feat == UserConnection::FEATURE_ADC_BAS0)
			{
				baseOk = true;
				// For bas0 tiger is implicit
				if (feat == UserConnection::FEATURE_ADC_BAS0)
				{
					tigrOk = true;
				}
				// ADC clients must support all these...
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_ADCGET);
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_MINISLOTS);
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_TTHF);
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_TTHL);
				// For compatibility with older clients...
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_XML_BZLIST);
			}
			else if (feat == UserConnection::FEATURE_ZLIB_GET)
			{
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_ZLIB_GET);
			}
			else if (feat == UserConnection::FEATURE_ADC_BZIP)
			{
				aSource->setFlag(UserConnection::FLAG_SUPPORTS_XML_BZLIST);
			}
			else if (feat == UserConnection::FEATURE_ADC_TIGR)
			{
				tigrOk = true; // Variable 'tigrOk' is assigned a value that is never used.
			}
		}
	}
	
	if (!baseOk)
	{
		aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Invalid SUP"));
		aSource->disconnect();
		return;
	}
	
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		StringList defFeatures = adcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back("AD" + UserConnection::FEATURE_ZLIB_GET);
		}
		aSource->sup(defFeatures);
	}
	else
	{
		aSource->inf(true);
	}
	aSource->setState(UserConnection::STATE_INF);
}

void ConnectionManager::on(AdcCommand::STA, UserConnection*, const AdcCommand& /*cmd*/) noexcept
{

}

void ConnectionManager::on(UserConnectionListener::Connected, UserConnection* aSource) noexcept
{
	if (aSource->isSecure() && !aSource->isTrusted() && !BOOLSETTING(ALLOW_UNTRUSTED_CLIENTS))
	{
		putConnection(aSource);
		LogManager::getInstance()->message(STRING(CERTIFICATE_NOT_TRUSTED));
		return;
	}
	dcassert(aSource->getState() == UserConnection::STATE_CONNECT);
	if (aSource->isSet(UserConnection::FLAG_NMDC))
	{
		aSource->myNick(aSource->getToken());
		aSource->lock(CryptoManager::getInstance()->getLock(), CryptoManager::getInstance()->getPk() + "Ref=" + aSource->getHubUrl());
	}
	else
	{
		StringList defFeatures = adcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back("AD" + UserConnection::FEATURE_ZLIB_GET);
		}
		aSource->sup(defFeatures);
		aSource->send(AdcCommand(AdcCommand::SEV_SUCCESS, AdcCommand::SUCCESS, Util::emptyString).addParam("RF", aSource->getHubUrl()));
	}
	aSource->setState(UserConnection::STATE_SUPNICK);
}

void ConnectionManager::on(UserConnectionListener::MyNick, UserConnection* aSource, const string& aNick) noexcept
{
	if (aSource->getState() != UserConnection::STATE_SUPNICK)
	{
		// Already got this once, ignore...
		dcdebug("CM::onMyNick %p sent nick twice\n", (void*)aSource);
		return;
	}
	
	dcassert(!aNick.empty());
	dcdebug("ConnectionManager::onMyNick %p, %s\n", (void*)aSource, aNick.c_str());
	dcassert(!aSource->getUser());
	
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		// Try to guess where this came from...
		const auto& i = m_expectedConnections.remove(aNick);
		
		if (i.m_HubUrl.empty())
		{
			dcassert(i.m_Nick.empty());
			dcdebug("Unknown incoming connection from %s\n", aNick.c_str());
			putConnection(aSource);
			return;
		}
		
#ifdef RIP_USE_CONNECTION_AUTODETECT
		if (i.reason == ExpectedMap::REASON_DETECT_CONNECTION)
		{
			dcdebug("REASON_DETECT_CONNECTION received %s\n", aNick.c_str());
			
			// Auto-detection algorithm: we send $ConnectToMe command to first found user
			// and mark this user for expecting only for connection auto-detection.
			// So if we receive connection of this type, simply drop it.
			
			FavoriteHubEntry* fhub = FavoriteManager::getInstance()->getFavoriteHubEntry(i.aHubUrl);
			if (!fhub)
				dcdebug("REASON_DETECT_CONNECTION: can't find favorite hub %s\n", i.aHubUrl.c_str());
			dcassert(fhub);
			
			// WARNING: only Nmdc hub requests for REASON_DETECT_CONNECTION.
			// if another hub added, one must implement autodetection in base Client class
			NmdcHub* hub = static_cast<NmdcHub*>(ClientManager::getInstance()->findClient(i.aHubUrl));
			if (!hub)
				dcdebug("REASON_DETECT_CONNECTION: can't find hub %s\n", i.aHubUrl.c_str());
			dcassert(hub);
			
			if (hub && fhub && hub->IsAutodetectPending())
			{
				// set connection type to ACTIVE
				if (fhub)
					fhub->setMode(1);
					
				hub->AutodetectComplete();
				
				// TODO: allow to disable through GUI saving of detected mode to
				// favorite hub's settings
				
				fire(ConnectionManagerListener::DirectModeDetected(), i.aHubUrl);
			}
			
			putConnection(aSource);
			
			return;
		}
#endif // RIP_USE_CONNECTION_AUTODETECT
		aSource->setToken(i.m_Nick);
		aSource->setHubUrl(i.m_HubUrl); // TODO - тут юзера почему-то еще нет
		const auto l_encoding = ClientManager::findHubEncoding(i.m_HubUrl);
		aSource->setEncoding(l_encoding);
	}
	
	const string nick = Text::toUtf8(aNick, aSource->getEncoding());// TODO IRAINMAN_USE_UNICODE_IN_NMDC
	const CID cid = ClientManager::makeCid(nick, aSource->getHubUrl());
	
	// First, we try looking in the pending downloads...hopefully it's one of them...
	{
		webrtc::ReadLockScoped l(*g_csDownloads);
		for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
		{
			ConnectionQueueItem* cqi = *i;
			cqi->setErrors(0);
			if ((cqi->getState() == ConnectionQueueItem::CONNECTING || cqi->getState() == ConnectionQueueItem::WAITING) &&
			cqi->getUser()->getCID() == cid)
			{
				aSource->setUser(cqi->getUser());
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
//				cqi->getUser()->initRatio(false);
#endif
				// Indicate that we're interested in this file...
				aSource->setFlag(UserConnection::FLAG_DOWNLOAD);
				break;
			}
		}
	}
	
	if (!aSource->getUser())
	{
		// Make sure we know who it is, i e that he/she is connected...
		
		aSource->setUser(ClientManager::findUser(cid));
		if (!aSource->getUser() || !aSource->getUser()->isOnline())
		{
			dcdebug("CM::onMyNick Incoming connection from unknown user %s\n", nick.c_str());
			putConnection(aSource);
			return;
		}
		// We don't need this connection for downloading...make it an upload connection instead...
		aSource->setFlag(UserConnection::FLAG_UPLOAD);
	}
	
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
	if (ClientManager::getInstance()->isStealth(aSource->getHubUrl()))
		aSource->setFlag(UserConnection::FLAG_STEALTH);
#endif
		
	ClientManager::setIPUser(aSource->getUser(), aSource->getRemoteIp());
	
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE_ON_NMDC
	if (ClientManager::isOp(aSource->getUser(), aSource->getHubUrl()))
		aSource->setFlag(UserConnection::FLAG_OP);
#endif
		
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		aSource->myNick(aSource->getToken());
		aSource->lock(CryptoManager::getInstance()->getLock(), CryptoManager::getInstance()->getPk());
	}
	
	aSource->setState(UserConnection::STATE_LOCK);
}

void ConnectionManager::on(UserConnectionListener::CLock, UserConnection* aSource, const string& aLock
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
                           , const string& aPk
#endif
                          ) noexcept
{
	if (aSource->getState() != UserConnection::STATE_LOCK)
	{
		dcdebug("CM::onLock %p received lock twice, ignoring\n", (void*)aSource);
		return;
	}
	
	if (CryptoManager::isExtended(aLock))
	{
		StringList defFeatures = nmdcFeatures;
		if (BOOLSETTING(COMPRESS_TRANSFERS))
		{
			defFeatures.push_back(UserConnection::FEATURE_ZLIB_GET);
		}
		aSource->supports(defFeatures);
	}
	
	aSource->setState(UserConnection::STATE_DIRECTION);
	aSource->direction(aSource->getDirectionString(), aSource->getNumber());
	aSource->key(CryptoManager::getInstance()->makeKey(aLock));
	
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
	if (aSource->getUser())
		ClientManager::getInstance()->setPkLock(aSource->getUser(), aPk, aLock);
#endif
}

void ConnectionManager::on(UserConnectionListener::Direction, UserConnection* aSource, const string& dir, const string& num) noexcept
{
	if (aSource->getState() != UserConnection::STATE_DIRECTION)
	{
		dcdebug("CM::onDirection %p received direction twice, ignoring\n", (void*)aSource);
		return;
	}
	
	dcassert(aSource->isSet(UserConnection::FLAG_DOWNLOAD) ^ aSource->isSet(UserConnection::FLAG_UPLOAD));
	if (dir == "Upload")
	{
		// Fine, the other fellow want's to send us data...make sure we really want that...
		if (aSource->isSet(UserConnection::FLAG_UPLOAD))
		{
			// Huh? Strange...disconnect...
			putConnection(aSource);
			return;
		}
	}
	else
	{
		if (aSource->isSet(UserConnection::FLAG_DOWNLOAD))
		{
			int number = Util::toInt(num);
			// Damn, both want to download...the one with the highest number wins...
			if (aSource->getNumber() < number)
			{
				// Damn! We lost!
				aSource->unsetFlag(UserConnection::FLAG_DOWNLOAD);
				aSource->setFlag(UserConnection::FLAG_UPLOAD);
			}
			else if (aSource->getNumber() == number)
			{
				putConnection(aSource);
				return;
			}
		}
	}
	
	dcassert(aSource->isSet(UserConnection::FLAG_DOWNLOAD) ^ aSource->isSet(UserConnection::FLAG_UPLOAD));
	
	aSource->setState(UserConnection::STATE_KEY);
}

void ConnectionManager::setIP(UserConnection* p_uc, ConnectionQueueItem* p_qi)
{
	dcassert(p_uc->getUser());
	dcassert(p_qi);
	dcassert(p_uc);
	/* [!] Ќеотображаютс€ IP адреса у юзеров, подробнее спрашивать у “Ємы. Ќужен рефакторинг!
	#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	if (!p_qi->getUser()->getLastNick().empty())
	{
	    // TODO “ак бывает пока не пон€л как - срабатывает if (aLine[0] == 'C' && !isSet(FLAG_NMDC))
	    // L: это нормальное состо€ние если юзер ушЄл в оффлайн! Ќеобходимо уже давно отв€зать статистику от ника, и использовать CID!!!
	    const string& l_ip = p_uc->getSocket()->getIp();
	    p_uc->getUser()->setIP(l_ip);
	}
	#else // PPA_INCLUDE_LASTIP_AND_USER_RATIO
	*/
	
	p_uc->getUser()->setIP(p_uc->getSocket()->getIp());
	
	
//#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
}

void ConnectionManager::addDownloadConnection(UserConnection* p_conn)
{
	dcassert(p_conn->isSet(UserConnection::FLAG_DOWNLOAD));
	ConnectionQueueItem* cqi = nullptr;
	{
		webrtc::ReadLockScoped l(*g_csDownloads);
		
		const ConnectionQueueItem::Iter i = find(g_downloads.begin(), g_downloads.end(), p_conn->getUser());
		if (i != g_downloads.end())
		{
			cqi = *i;
			if (cqi->getState() == ConnectionQueueItem::WAITING || cqi->getState() == ConnectionQueueItem::CONNECTING)
			{
				cqi->setState(ConnectionQueueItem::ACTIVE);
				p_conn->setFlag(UserConnection::FLAG_ASSOCIATED);
				
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
				fire(ConnectionManagerListener::Connected(), cqi);
#endif
				dcdebug("ConnectionManager::addDownloadConnection, leaving to downloadmanager\n");
			}
			else
				cqi = nullptr;
		}
	}
	
	if (cqi)
	{
		DownloadManager::getInstance()->addConnection(p_conn);
		setIP(p_conn, cqi); //[+]PPA
	}
	else
	{
		putConnection(p_conn);
	}
}

void ConnectionManager::addUploadConnection(UserConnection* p_conn)
{
	dcassert(p_conn->isSet(UserConnection::FLAG_UPLOAD));
	
#ifdef IRAINMAN_DISALLOWED_BAN_MSG
	if (uc->isSet(UserConnection::FLAG_SUPPORTS_BANMSG))
	{
		uc->error(UserConnection::PLEASE_UPDATE_YOUR_CLIENT);
		return;
	}
#endif
	
	ConnectionQueueItem* cqi = nullptr;
	{
		webrtc::WriteLockScoped l(*g_csUploads);
		
		const auto i = find(g_uploads.begin(), g_uploads.end(), p_conn->getUser());
		if (i == g_uploads.cend())
		{
			cqi = getCQI_L(p_conn->getHintedUser(), false);
			cqi->setState(ConnectionQueueItem::ACTIVE);
			p_conn->setFlag(UserConnection::FLAG_ASSOCIATED);
			
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
			fire(ConnectionManagerListener::Connected(), cqi);
#endif
			dcdebug("ConnectionManager::addUploadConnection, leaving to uploadmanager\n");
		}
	}
	if (cqi)
	{
		UploadManager::getInstance()->addConnection(p_conn);
		setIP(p_conn, cqi); //[+]PPA
	}
	else
	{
		putConnection(p_conn);
	}
}

void ConnectionManager::on(UserConnectionListener::Key, UserConnection* aSource, const string&/* aKey*/) noexcept
{
	if (aSource->getState() != UserConnection::STATE_KEY)
	{
		dcdebug("CM::onKey Bad state, ignoring");
		return;
	}
	dcassert(aSource->getUser());
	// [-] IRainman fix: please don't problem maskerate.
	// [-]if (aSource->getUser()) //[+]PPA
	if (aSource->isSet(UserConnection::FLAG_DOWNLOAD))
	{
		addDownloadConnection(aSource);
	}
	else
	{
		addUploadConnection(aSource);
	}
}

void ConnectionManager::on(AdcCommand::INF, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	if (aSource->getState() != UserConnection::STATE_INF)
	{
		// Already got this once, ignore...
		aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "Expecting INF"));
		dcdebug("CM::onINF %p sent INF twice\n", (void*)aSource);
		aSource->disconnect();
		return;
	}
	
	string cid;
	if (!cmd.getParam("ID", 0, cid))
	{
		aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_INF_MISSING, "ID missing").addParam("FL", "ID"));
		dcdebug("CM::onINF missing ID\n");
		aSource->disconnect();
		return;
	}
	
	aSource->setUser(ClientManager::findUser(CID(cid)));
	
	if (!aSource->getUser())
	{
		dcdebug("CM::onINF: User not found");
		aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "User not found"));
		putConnection(aSource);
		return;
	}
	
	if (!checkKeyprint(aSource))
	{
		putConnection(aSource);
		return;
	}
	
	string token;
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		if (!cmd.getParam("TO", 0, token))
		{
			aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "TO missing"));
			putConnection(aSource);
			return;
		}
	}
	else
	{
		token = aSource->getToken();
	}
	
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		aSource->inf(false);
	}
	
	bool down;
	{
		webrtc::ReadLockScoped l(*g_csDownloads);
		const ConnectionQueueItem::Iter i = find(g_downloads.begin(), g_downloads.end(), aSource->getUser());
		
		if (i != g_downloads.cend())
		{
			(*i)->setErrors(0);
			if ((*i)->getToken() == token)
			{
				down = true;
			}
			else
			{
				down = false;
#ifdef IRAINMAN_CONNECTION_MANAGER_TOKENS_DEBUG
				dcassert(0); // [+] IRainman fix: download token mismatch.
#endif
			}
		}
		else // [!] IRainman fix: check tokens for upload connections.
#ifndef IRAINMAN_CONNECTION_MANAGER_TOKENS_DEBUG
		{
			down = false;
		}
#else
		{
			const ConnectionQueueItem::Iter j = find(uploads.begin(), uploads.end(), aSource->getUser());
		
			if (j != uploads.cend())
			{
				const string& to = (*j)->getToken();
		
				if (to == token)
				{
					down = false;
				}
				else
				{
					down = false;
					dcassert(0); // [!] IRainman fix: upload token mismatch.
				}
			}
			else
			{
				down = false;
			}
		}
#endif
	}
	
	if (down)
	{
		aSource->setFlag(UserConnection::FLAG_DOWNLOAD);
		addDownloadConnection(aSource);
	}
	else
	{
		aSource->setFlag(UserConnection::FLAG_UPLOAD);
		addUploadConnection(aSource);
	}
}

void ConnectionManager::force(const UserPtr& aUser)
{
	webrtc::ReadLockScoped l(*g_csDownloads);
	
	const ConnectionQueueItem::Iter i = find(g_downloads.begin(), g_downloads.end(), aUser);
	if (i != g_downloads.end())
	{
		(*i)->setLastAttempt(0);
	}
}

bool ConnectionManager::checkKeyprint(UserConnection *aSource)
{
	dcassert(aSource->getUser());
	// [-] IRainman fix: please don't problem maskerate.
	// [-] if (!aSource->getUser()) return false; //[+]PPA
	auto kp = aSource->getKeyprint();
	if (kp.empty())
	{
		return true;
	}
	
	auto kp2 = ClientManager::getStringField(aSource->getUser()->getCID(), aSource->getHubUrl(), "KP");
	if (kp2.empty())
	{
		// TODO false probably
		return true;
	}
	
	if (kp2.compare(0, 7, "SHA256/", 7) != 0)
	{
		// Unsupported hash
		return true;
	}
	
	dcdebug("Keyprint: %s vs %s\n", Encoder::toBase32(&kp[0], kp.size()).c_str(), kp2.c_str() + 7);
	
	vector<uint8_t> kp2v(kp.size());
	Encoder::fromBase32(&kp2[7], &kp2v[0], kp2v.size());
	if (!std::equal(kp.begin(), kp.end(), kp2v.begin()))
	{
		dcdebug("Not equal...\n");
		return false;
	}
	
	return true;
}

void ConnectionManager::failed(UserConnection* aSource, const string& aError, bool protocolError)
{

	if (aSource->isSet(UserConnection::FLAG_ASSOCIATED))
	{
		if (aSource->isSet(UserConnection::FLAG_DOWNLOAD))
		{
			webrtc::WriteLockScoped l(*g_csDownloads); // TODO http://code.google.com/p/flylinkdc/issues/detail?id=1439
			const ConnectionQueueItem::Iter i = find(g_downloads.begin(), g_downloads.end(), aSource->getUser());
			dcassert(i != g_downloads.end());
			ConnectionQueueItem* cqi = *i;
			cqi->setState(ConnectionQueueItem::WAITING);
			cqi->setLastAttempt(GET_TICK());
			cqi->setErrors(protocolError ? -1 : (cqi->getErrors() + 1));
			fire(ConnectionManagerListener::Failed(), cqi, aError);
			if (isShuttingDown()) // TODO
			{
				putCQI_L(cqi); // ”дал€ть всегда нельз€ - только при разрушении
				// (Closed issue 983) https://code.google.com/p/flylinkdc/issues/detail?id=983 Ѕесконечные подключени€ дл€ скачки файл-листа
				// TODO - Ќайти более другое решение бага
			}
			
		}
		else if (aSource->isSet(UserConnection::FLAG_UPLOAD))
		{
			webrtc::WriteLockScoped l(*g_csUploads); // http://code.google.com/p/flylinkdc/issues/detail?id=1439
			const ConnectionQueueItem::Iter i = find(g_uploads.begin(), g_uploads.end(), aSource->getUser());
			dcassert(i != g_uploads.end());
			ConnectionQueueItem* cqi = *i;
			putCQI_L(cqi);
		}
	}
	putConnection(aSource);
}

void ConnectionManager::on(UserConnectionListener::Failed, UserConnection* aSource, const string& aError) noexcept
{
	failed(aSource, aError, false);
}

void ConnectionManager::on(UserConnectionListener::ProtocolError, UserConnection* aSource, const string& aError) noexcept
{
	failed(aSource, aError, true);
}

void ConnectionManager::disconnect(const UserPtr& aUser)
{
	webrtc::ReadLockScoped l(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); ++i)
	{
		UserConnection* uc = *i;
		if (uc->getUser() == aUser)
			uc->disconnect(true);
	}
	/*
	    const auto & l_find = g_userConnections.find(aUser);
	    if(l_find != g_userConnections.end())
	        l_find->second->disconnect(true);
	*/
}

void ConnectionManager::disconnect(const UserPtr& aUser, bool isDownload) // [!] IRainman fix.
{
	webrtc::ReadLockScoped l(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); ++i)
	{
		UserConnection* uc = *i;
		dcassert(uc);
		if (uc->getUser() == aUser && uc->isSet((Flags::MaskType)(isDownload ? UserConnection::FLAG_DOWNLOAD : UserConnection::FLAG_UPLOAD)))
		{
			uc->disconnect(true);
			break;
		}
	}
	/*
	    const auto & l_find = g_userConnections.find(aUser);
	    if(l_find != g_userConnections.end())
	        if(l_find->second->isSet((Flags::MaskType)(isDownload ? UserConnection::FLAG_DOWNLOAD : UserConnection::FLAG_UPLOAD)))
	            l_find->second->disconnect(true);
	*/
}

void ConnectionManager::shutdown()
{
	dcassert(shuttingDown == false);
	shuttingDown = true;
	TimerManager::getInstance()->removeListener(this);
	disconnect();
	{
		webrtc::ReadLockScoped l(*g_csConnection);
		for (auto j = g_userConnections.cbegin(); j != g_userConnections.cend(); ++j)
		{
			(*j)->disconnect(true);
		}
	}
	// Wait until all connections have died out...
	while (true)
	{
		{
			webrtc::ReadLockScoped l(*g_csConnection);
			if (g_userConnections.empty())
			{
				break;
			}
		}
		Thread::sleep(100);
	}
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	// TODO: please fix me: this code is not valid! downloads and uploads is empty!
	// [!] http://code.google.com/p/flylinkdc/issues/detail?id=1037
	//dcassert(downloads.empty());
	//dcassert(uploads.empty());
	// [~]
	// —брасываем рейтинг в базу пока не нашли причину почему тут остаютс€ записи.
	{
		{
			webrtc::ReadLockScoped l(*g_csDownloads);
			for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
			{
				ConnectionQueueItem* cqi = *i;
				cqi->getUser()->flushRatio();
			}
		}
		{
			webrtc::ReadLockScoped l(*g_csUploads);
			for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
			{
				ConnectionQueueItem* cqi = *i;
				cqi->getUser()->flushRatio();
			}
		}
	}
#endif
}

// UserConnectionListener
void ConnectionManager::on(UserConnectionListener::Supports, UserConnection* p_conn, StringList& feat) noexcept
{
	dcassert(p_conn->getUser()); // [!] IRainman fix: please don't problem maskerate.
	// [!] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
	if (p_conn->getUser()) // 44 падени€ https://www.crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=48388
	{
		uint8_t knownUcSupports = 0;
		auto unknownUcSupports = UcSupports::setSupports(p_conn, feat, knownUcSupports);
		ClientManager::getInstance()->setSupports(p_conn->getUser(), unknownUcSupports, knownUcSupports);
	}
	else
	{
		LogManager::getInstance()->message("Error UserConnectionListener::Supports conn->getUser() == nullptr, url = " + p_conn->getHintedUser().hint);
	}
}

// !SMT!-S
void ConnectionManager::setUploadLimit(const UserPtr& aUser, int lim)
{
	webrtc::ReadLockScoped l(*g_csConnection);
	for (auto i = g_userConnections.cbegin(); i != g_userConnections.cend(); ++i)
	{
		if ((*i)->isSet(UserConnection::FLAG_UPLOAD) && (*i)->getUser() == aUser)
		{
			(*i)->setUploadLimit(lim);
		}
	}
	/*
	    const auto & l_find = g_userConnections.find(aUser);
	    if(l_find != g_userConnections.end() && l_find->second->isSet(UserConnection::FLAG_UPLOAD))
	        l_find->second->setUploadLimit(lim);
	*/
}

/**
 * @file
 * $Id: ConnectionManager.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */
