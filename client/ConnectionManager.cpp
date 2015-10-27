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

uint16_t ConnectionManager::g_ConnToMeCount = 0;
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csConnection = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csDownloads = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csUploads = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csDdosCheck = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csTTHFilter = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> ConnectionManager::g_csFileFilter = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());

boost::unordered_set<UserConnection*> ConnectionManager::g_userConnections;
boost::unordered_map<string, ConnectionManager::CFlyTickTTH> ConnectionManager::g_duplicate_search_tth;
boost::unordered_map<string, ConnectionManager::CFlyTickFile> ConnectionManager::g_duplicate_search_file;
boost::unordered_set<string> ConnectionManager::g_ddos_ctm2hub;
ConnectionQueueItem::List ConnectionManager::g_downloads;
ConnectionQueueItem::List ConnectionManager::g_uploads;

string TokenManager::makeToken() noexcept
{
	string l_token;
	FastLock l(m_cs);
	do {
		l_token = Util::toString(Util::rand());
	}
	while (m_tokens.find(l_token) != m_tokens.end());
	m_tokens.insert(l_token);
	return l_token;
}

string TokenManager::getToken() noexcept
{
	FastLock l(m_cs);
	const string l_token = Util::toString(Util::rand());
	m_tokens.insert(l_token);
	return l_token;
}

bool TokenManager::addToken(const string& aToken) noexcept
{
	FastLock l(m_cs);
	if (m_tokens.find(aToken) == m_tokens.end())
	{
		m_tokens.insert(aToken);
		return true;
	}
	return false;
}

void TokenManager::removeToken(const string& aToken) noexcept
{
	FastLock l(m_cs);
	auto p = m_tokens.find(aToken);
	if (p != m_tokens.end())
		m_tokens.erase(p);
	//else
	//  dcassert(0);
}


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
	ClientManager::getInstance()->addListener(this);
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
	string l_bind = SETTING(BIND_ADDRESS);
	
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		server = new Server(false, 0, Util::emptyString);
	}
	else
	{
		port = static_cast<uint16_t>(SETTING(TCP_PORT));
		server = new Server(false, port, l_bind);
	}
	SET_SETTING(TCP_PORT, server->getServerPort());
	
	if (!CryptoManager::getInstance()->TLSOk())
	{
		LogManager::message("Skipping secure port: " + Util::toString(SETTING(USE_TLS)));
		dcdebug("Skipping secure port: %d\n", SETTING(USE_TLS));
		return;
	}
	
	if (BOOLSETTING(AUTO_DETECT_CONNECTION))
	{
		secureServer = new Server(true, 0, Util::emptyString);
		SET_SETTING(TLS_PORT, secureServer->getServerPort());
	}
	else
	{
		port = static_cast<uint16_t>(SETTING(TLS_PORT));
		secureServer = new Server(true, port, l_bind);
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
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		{
			webrtc::WriteLockScoped l(*g_csDownloads);
			const auto i = find(g_downloads.begin(), g_downloads.end(), aUser);
			if (i == g_downloads.end())
			{
				getCQI_L(HintedUser(aUser, Util::emptyString), true, Util::emptyString); // http://code.google.com/p/flylinkdc/issues/detail?id=1037
				// Не сохраняем указатель. а как и когда будем удалять ?
				return;
			}
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
			else
			{
				if (find(m_checkIdle.begin(), m_checkIdle.end(), aUser) == m_checkIdle.end())
				{
					m_checkIdle.push_back(aUser); // TODO - Лок?
				}
			}
#endif
		}
#ifndef USING_IDLERS_IN_CONNECTION_MANAGER
		DownloadManager::checkIdle(aUser);
#endif
	}
}

ConnectionQueueItem* ConnectionManager::getCQI_L(const HintedUser& aHintedUser, bool download, const string& aToken)
{
	ConnectionQueueItem* cqi = new ConnectionQueueItem(aHintedUser, download, !aToken.empty() ? aToken : m_tokens.makeToken());
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
	
	fire(ConnectionManagerListener::Added(), cqi); // TODO - выбросить фаер без блокировки наружу
	return cqi;
}

void ConnectionManager::putCQI_L(ConnectionQueueItem* cqi)
{
	fire(ConnectionManagerListener::Removed(), cqi); // TODO - зовется фаер под локом?
	if (cqi->isDownload())
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
	m_tokens.removeToken(cqi->getConnectionQueueToken());
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
void ConnectionManager::onUserUpdated(const UserPtr& aUser)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		{
			webrtc::ReadLockScoped l(*g_csDownloads);
			for (auto i = g_downloads.cbegin(); i != g_downloads.cend(); ++i)
			{
				ConnectionQueueItem* cqi = *i;
				if (cqi->getUser() == aUser)
				{
					fire(ConnectionManagerListener::UserUpdated(), cqi);
				}
			}
		}
		{
			webrtc::ReadLockScoped l(*g_csUploads);
			for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
			{
				ConnectionQueueItem* cqi = *i;
				if (cqi->getUser() == aUser)
				{
					fire(ConnectionManagerListener::UserUpdated(), cqi);
				}
			}
		}
	}
}

void ConnectionManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (((aTick / 1000) % (CFlyServerConfig::g_max_unique_tth_search + 2)) == 0)
	{
		cleanupDuplicateSearchTTH(aTick);
	}
	if (((aTick / 1000) % (CFlyServerConfig::g_max_unique_file_search + 2)) == 0)
	{
		cleanupDuplicateSearchFile(aTick);
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
#ifdef _DEBUG
				const unsigned l_count_sec = 10;
				const unsigned l_count_sec_connecting = 5;
#else
				const unsigned l_count_sec = 60;
				const unsigned l_count_sec_connecting = 50;
#endif
				if (cqi->getLastAttempt() == 0 || ((SETTING(DOWNCONN_PER_SEC) == 0 || attempts < SETTING(DOWNCONN_PER_SEC)) &&
				                                   cqi->getLastAttempt() + l_count_sec * 1000 * max(1, cqi->getErrors()) < aTick))
				{
					cqi->setLastAttempt(aTick);
					
					QueueItem::Priority prio = QueueManager::hasDownload(cqi->getUser()); // [10] https://www.box.net/shared/i6hgw2qzhr9zyy15vhh1
					
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
							cqi->setState(ConnectionQueueItem::CONNECTING);
							
							if (BOOLSETTING(AUTO_PASSIVE_INCOMING_CONNECTIONS))
							{
								cqi->m_count_waiting++;
								cqi->m_is_force_passive = cqi->m_is_active_client ? (cqi->m_count_waiting > 1) : false; // Делаем вторую попытку подключения в пассивке ?
							}
							else
							{
								cqi->m_is_force_passive = false;
							}
							
							
							ClientManager::getInstance()->connect(cqi->getHintedUser(),
							                                      cqi->getConnectionQueueToken(),
							                                      cqi->m_is_force_passive,
							                                      cqi->m_is_active_client // <- Out param!
							                                     );
							fire(ConnectionManagerListener::ConnectionStatusChanged(), cqi);
							attempts++;
						}
						else
						{
							cqi->m_count_waiting = 0;
							cqi->setState(ConnectionQueueItem::NO_DOWNLOAD_SLOTS);
							fire(ConnectionManagerListener::Failed(), cqi, STRING(ALL_DOWNLOAD_SLOTS_TAKEN));
						}
					}
					else if (cqi->getState() == ConnectionQueueItem::NO_DOWNLOAD_SLOTS && startDown)
					{
						cqi->m_count_waiting = 0;
						cqi->setState(ConnectionQueueItem::WAITING);
					}
				}
				else if (cqi->getState() == ConnectionQueueItem::CONNECTING && cqi->getLastAttempt() + l_count_sec_connecting * 1000 < aTick)
				{
					ClientManager::getInstance()->connectionTimeout(cqi->getUser());
					
					cqi->setErrors(cqi->getErrors() + 1);
					if (cqi->m_count_waiting > 2)
					{
						fire(ConnectionManagerListener::Failed(), cqi, STRING(CONNECTION_TIMEOUT)); // CONNECTION_DID_NOT_WORK
						cqi->setState(ConnectionQueueItem::WAITING);
						// TODO - удаление пока не делаем - нужно потестировать лучше
						// l_removed.push_back(cqi);
					}
					else
					{
						fire(ConnectionManagerListener::Failed(), cqi, STRING(CONNECTION_TIMEOUT));
						cqi->setState(ConnectionQueueItem::WAITING);
					}
				}
			}
		}
	}
	if (!l_removed.empty())
	{
		for (auto m = l_removed.cbegin(); m != l_removed.cend(); ++m)
		{
			if ((*m)->isDownload())
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
		DownloadManager::checkIdle(*i);
	}
#endif
}

void ConnectionManager::cleanupIpFlood(const uint64_t p_tick)
{
	webrtc::WriteLockScoped l_ddos(*g_csDdosCheck);
	for (auto j = m_ddos_map.cbegin(); j != m_ddos_map.cend();)
	{
		// Если коннектов совершено меньше чем предел в течении минуты - убираем адрес из таблицы - с ним все хорошо!
		const auto l_tick_delta = p_tick - j->second.m_first_tick;
		const bool l_is_min_ban_close = j->second.m_count_connect < CFlyServerConfig::g_max_ddos_connect_to_me && l_tick_delta > 1000 * 60;
		if (l_is_min_ban_close)
		{
#ifdef _DEBUG
			//LogManager::ddos_message("BlockID = " + Util::toString(j->second.m_block_id) + ", Removed mini-ban for: " +
			//                         j->first.first + j->second.getPorts() + ", Hub IP = " + j->first.second.to_string() +
			//                         " m_ddos_map.size() = " + Util::toString(m_ddos_map.size()));
#endif
		}
		// Если коннектов совершено много и IP находится в бане, но уже прошло время больше чем 10 Минут(по умолчанию)
		// Также убираем запись из таблицы блокировки
		const bool l_is_ddos_ban_close = j->second.m_count_connect > CFlyServerConfig::g_max_ddos_connect_to_me
		                                 && l_tick_delta > CFlyServerConfig::g_ban_ddos_connect_to_me * 1000 * 60;
		if (l_is_ddos_ban_close && BOOLSETTING(LOG_DDOS_TRACE))
		{
			string l_type;
			if (j->first.second.is_unspecified()) // Если нет второго IP то это команада  ConnectToMe
			{
				l_type =  "IP-1:" + j->first.first + j->second.getPorts();
			}
			else
			{
				l_type = " IP-1:" + j->first.first + j->second.getPorts() + " IP-2: " + j->first.second.to_string();
			}
			LogManager::ddos_message("BlockID = " + Util::toString(j->second.m_block_id) + ", Removed DDoS lock " + j->second.m_type_block +
			                         ", Count connect = " + Util::toString(j->second.m_count_connect) + l_type +
			                         ", m_ddos_map.size() = " + Util::toString(m_ddos_map.size()));
		}
		if (l_is_ddos_ban_close || l_is_min_ban_close)
			m_ddos_map.erase(j++);
		else
			++j;
	}
}
void ConnectionManager::cleanupDuplicateSearchFile(const uint64_t p_tick)
{
	webrtc::WriteLockScoped l_lock(*g_csFileFilter);
	for (auto j = g_duplicate_search_file.cbegin(); j != g_duplicate_search_file.cend();)
	{
		if ((p_tick - j->second.m_first_tick) > 1000 * CFlyServerConfig::g_max_unique_file_search)
		{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_FILE_SEARCH
			if (j->second.m_count_connect > 1) // Событие возникало больше одного раза - логируем?
			{
				LogManager::ddos_message(string(j->second.m_count_connect, '*') + " BlockID = " + Util::toString(j->second.m_block_id) +
				                         ", Unlock duplicate File search: " + j->first +
				                         ", Count connect = " + Util::toString(j->second.m_count_connect) +
				                         ", Hash map size: " + Util::toString(g_duplicate_search_tth.size()));
			}
#endif
			g_duplicate_search_file.erase(j++);
		}
		else
			++j;
	}
}
void ConnectionManager::cleanupDuplicateSearchTTH(const uint64_t p_tick)
{
	webrtc::WriteLockScoped l_lock(*g_csTTHFilter);
	for (auto j = g_duplicate_search_tth.cbegin(); j != g_duplicate_search_tth.cend();)
	{
		if ((p_tick - j->second.m_first_tick) > 1000 * CFlyServerConfig::g_max_unique_tth_search)
		{
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_TTH_SEARCH
			if (j->second.m_count_connect > 1) // Событие возникало больше одного раза - логируем?
			{
				LogManager::ddos_message(string(j->second.m_count_connect, '*') + " BlockID = " + Util::toString(j->second.m_block_id) +
				                         ", Unlock duplicate TTH search: " + j->first +
				                         ", Count connect = " + Util::toString(j->second.m_count_connect) +
				                         ", Hash map size: " + Util::toString(g_duplicate_search_tth.size()));
			}
#endif
			g_duplicate_search_tth.erase(j++);
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
		if ((l_connection->getLastActivity() + 180 * 1000) < aTick) // Зачем так много минут висеть?
#endif
		{
			l_connection->disconnect(true);
		}
	}
}

static const uint64_t g_FLOOD_TRIGGER = 20000;
static const uint64_t g_FLOOD_ADD = 2000;

ConnectionManager::Server::Server(bool p_secure
                                  , uint16_t p_port, const string& p_server_ip /* = "0.0.0.0" */) :
	m_secure(p_secure),
	m_die(false)
{
	m_sock.create();
	m_sock.setSocketOpt(SO_REUSEADDR, 1);
	m_server_ip   = p_server_ip; // в AirDC++ и дургих этого уже нет
	m_server_port = m_sock.bind(p_port, p_server_ip); // [7] Wizard https://www.box.net/shared/45acc9cef68ecb499cb5
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
					ConnectionManager::getInstance()->accept(m_sock, m_secure, this);
				}
			}
		}
		catch (const Exception& e)
		{
			LogManager::message(STRING(LISTENER_FAILED) + ' ' + e.getError());
		}
		bool failed = false;
		while (!m_die)
		{
			try
			{
				m_sock.disconnect();
				m_sock.create();
				m_server_port = m_sock.bind(m_server_port, m_server_ip);
				dcassert(m_server_port);
				m_sock.listen();
				if (failed)
				{
					LogManager::message(STRING(CONNECTIVITY_RESTORED));
					failed = false;
				}
				break;
			}
			catch (const SocketException& e)
			{
				dcdebug("ConnectionManager::Server::run Stopped listening: %s\n", e.getError().c_str());
				
				if (!failed)
				{
					LogManager::message(STRING(CONNECTIVITY_ERROR) + ' ' + e.getError());
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
void ConnectionManager::accept(const Socket& sock, bool secure, Server* p_server) noexcept
{
	uint32_t now = GET_TICK();
	
	if (g_ConnToMeCount > 0)
		g_ConnToMeCount--;
		
	if (now > m_floodCounter)
	{
		m_floodCounter = now + g_FLOOD_ADD;
	}
	else
	{
		if (false  // TODO - узнать почему тут такой затыкон оставлен в оригинальном dc++
		&& now + g_FLOOD_TRIGGER < m_floodCounter)
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
			LogManager::flood_message("Connection flood detected, port = " + Util::toString(sock.getPort()) + " IP = " + sock.getIp());
			dcdebug("Connection flood detected!\n");
			return;
		}
		else
		{
			if (g_ConnToMeCount <= 0)
			{
				m_floodCounter += g_FLOOD_ADD;
			}
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
	catch (const Exception& e)
	{
#ifdef _DEBUG
		LogManager::message("uc->accept(sock) Error = " + e.getError());
#endif
		dcdebug("uc->accept(sock); error\n");
		// Обработка теста порта TLS
		if (secure && p_server)
		{
			const auto l_remote_port = p_server->getServerPort();
			if (l_remote_port == SETTING(TLS_PORT)) // TODO проверить тут IP внешнего сервера - он должен совпадать с test.fly-server.ru
			{
				SettingsManager::g_TestTLSLevel = true; // Проверьть магическое число тут не знаю как - считаем если пришел accept на этот порт, то он открыт
				// ClientManager::getMyCID().toBase32() == l_magic;
			}
		}
		deleteConnection(uc);
	}
}
bool ConnectionManager::checkDuplicateSearchFile(const string& p_search_command)
{
	webrtc::WriteLockScoped l_lock(*g_csFileFilter);
	const auto l_tick = GET_TICK();
	CFlyTickFile l_item;
	l_item.m_first_tick = l_tick;
	l_item.m_last_tick = l_tick;
	const auto l_key_pos = p_search_command.rfind(' ');
	if (l_key_pos != string::npos && l_key_pos)
	{
		const string l_key = p_search_command.substr(0, l_key_pos);
		auto l_result = g_duplicate_search_file.insert(std::pair<string, CFlyTickFile>(l_key, l_item));
		auto& l_cur_value = l_result.first->second;
		++l_cur_value.m_count_connect;
		if (l_result.second == false) // Элемент уже существует - проверим его счетчик и старость.
		{
			l_cur_value.m_last_tick = l_tick;
			if (l_tick - l_cur_value.m_first_tick > 1000 * CFlyServerConfig::g_max_unique_file_search)
			{
				// Тут можно сразу стереть элемент устаревший
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
#ifdef FLYLINKDC_USE_LOG_FOR_DUPLICATE_FILE_SEARCH
					LogManager::ddos_message(string(l_cur_value.m_count_connect, '*') + " BlockID = " + Util::toString(l_cur_value.m_block_id) +
					                         ", Lock File search = " + p_search_command +
					                         ", Count = " + Util::toString(l_cur_value.m_count_connect) +
					                         ", Hash map size: " + Util::toString(g_duplicate_search_file.size()));
#endif
				}
				return true;
			}
		}
	}
	return false;
}

bool ConnectionManager::checkDuplicateSearchTTH(const string& p_search_command, const TTHValue& p_tth)
{
	webrtc::WriteLockScoped l_lock(*g_csTTHFilter);
	const auto l_tick = GET_TICK();
	CFlyTickTTH l_item;
	l_item.m_first_tick = l_tick;
	l_item.m_last_tick  = l_tick;
	auto l_result = g_duplicate_search_tth.insert(std::pair<string, CFlyTickTTH>(p_search_command + ' ' + p_tth.toBase32(), l_item));
	auto& l_cur_value = l_result.first->second;
	++l_cur_value.m_count_connect;
	if (l_result.second == false) // Элемент уже существует - проверим его счетчик и старость.
	{
		l_cur_value.m_last_tick  = l_tick;
		if (l_tick - l_cur_value.m_first_tick > 1000 * CFlyServerConfig::g_max_unique_tth_search)
		{
			// Тут можно сразу стереть элемент устаревший
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
				LogManager::ddos_message(string(l_cur_value.m_count_connect, '*') + " BlockID = " + Util::toString(l_cur_value.m_block_id) +
				                         ", Lock TTH search = " + p_search_command +
				                         ", TTH = " + p_tth.toBase32() +
				                         ", Count = " + Util::toString(l_cur_value.m_count_connect) +
				                         ", Hash map size: " + Util::toString(g_duplicate_search_tth.size()));
#endif
			}
			return true;
		}
	}
	return false;
}
void ConnectionManager::addCTM2HUB(const string& p_server_port, const HintedUser& p_hinted_user)
{
	const string l_cmt2hub = "CTM2HUB = " + p_server_port + " <<= DDoS block from: " + p_hinted_user.hint;;
	bool l_is_duplicate;
	{
		webrtc::WriteLockScoped l_ddos(*g_csDdosCheck);
		dcassert(p_hinted_user.user);
		l_is_duplicate = g_ddos_ctm2hub.insert(Text::toLower(p_server_port)).second;
	}
	CFlyServerJSON::pushError(18, l_cmt2hub);
	dcassert(l_is_duplicate == true);
	if (l_is_duplicate == false)
	{
		const string l_message = "Duplicate message: " + l_cmt2hub;
#ifdef FLYLINKDC_BETA
		// LogManager::message(l_message);
#endif
		CFlyServerJSON::pushError(18, l_message);
	}
}
bool ConnectionManager::checkIpFlood(const string& aIPServer, uint16_t aPort, const boost::asio::ip::address_v4& p_ip_hub, const string& p_userInfo, const string& p_HubInfo)
{
	{
		const auto l_tick = GET_TICK();
		const string l_server_lower = Text::toLower(aIPServer);
		dcassert(l_server_lower == aIPServer);
		// boost::system::error_code ec;
		// const auto l_ip = boost::asio::ip::address_v4::from_string(aIPServer, ec);
		const CFlyDDOSkey l_key(l_server_lower, p_ip_hub);
		// dcassert(!ec); // TODO - тут бывает и Host
		webrtc::WriteLockScoped l_ddos(*g_csDdosCheck);
		if (!g_ddos_ctm2hub.empty() && g_ddos_ctm2hub.find(l_server_lower + ':' + Util::toString(aPort)) != g_ddos_ctm2hub.end())
		{
			const string l_cmt2hub = "Block CTM2HUB = " + aIPServer + ':' + Util::toString(aPort) + " HubInfo: " + p_HubInfo + " UserInfo: " + p_userInfo;
#ifdef FLYLINKDC_BETA
			// LogManager::message(l_cmt2hub);
#endif
			LogManager::ddos_message(l_cmt2hub);
			return true;
		}
		CFlyDDoSTick l_item;
		l_item.m_first_tick = l_tick;
		l_item.m_last_tick = l_tick;
		auto l_result = m_ddos_map.insert(std::pair<CFlyDDOSkey, CFlyDDoSTick>(l_key, l_item));
		auto& l_cur_value = l_result.first->second;
		++l_cur_value.m_count_connect;
		string l_debug_key;
		if (BOOLSETTING(LOG_DDOS_TRACE))
		{
			l_debug_key = " Time: " + Util::getShortTimeString() + " Hub info = " + p_HubInfo; // https://drdump.com/Problem.aspx?ClientID=ppa&ProblemID=92733
			if (!p_userInfo.empty())
			{
				l_debug_key + " UserInfo = [" + p_userInfo + "]";
			}
			l_cur_value.m_original_query_for_debug[l_debug_key]++;
		}
		if (l_result.second == false)
		{
			// Элемент уже существует
			l_cur_value.m_last_tick = l_tick;   // Корректируем время последней активности.
			l_cur_value.m_ports.insert(aPort);  // Сохраним последний порт
			if (l_cur_value.m_count_connect == CFlyServerConfig::g_max_ddos_connect_to_me) // Превысили кол-во коннектов по одному IP
			{
				static uint16_t g_block_id = 0;
				l_cur_value.m_block_id = ++g_block_id;
				if (BOOLSETTING(LOG_DDOS_TRACE))
				{
					const string l_info   = "[Count limit: " + Util::toString(CFlyServerConfig::g_max_ddos_connect_to_me) + "]\t";
					const string l_target = "[Target: " + aIPServer + l_cur_value.getPorts() + "]\t";
					const string l_user_info = !p_userInfo.empty() ? "[UserInfo: " + p_userInfo + "]\t"  : "";
					l_cur_value.m_type_block = "Type DDoS:" + std::string(p_ip_hub.is_unspecified() ? "[$ConnectToMe]" : "[$Search]");
					LogManager::ddos_message("BlockID=" + Util::toString(l_cur_value.m_block_id) + ", " + l_cur_value.m_type_block + p_HubInfo + l_info + l_target + l_user_info);
					for (auto k = l_cur_value.m_original_query_for_debug.cbegin() ; k != l_cur_value.m_original_query_for_debug.cend(); ++k)
					{
						LogManager::ddos_message("  Detail BlockID=" + Util::toString(l_cur_value.m_block_id) + " " + k->first + " Count:" + Util::toString(k->second)); // TODO - сдать дубликаты + показать кол-во
					}
				}
				l_cur_value.m_original_query_for_debug.clear();
			}
			if (l_cur_value.m_count_connect >= CFlyServerConfig::g_max_ddos_connect_to_me)
			{
				if ((l_cur_value.m_last_tick - l_cur_value.m_first_tick) < CFlyServerConfig::g_ban_ddos_connect_to_me * 1000 * 60)
				{
					return true; // Лочим этот коннект до наступления амнистии. TODO - проверить эту часть внимательей
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
			
		if (uc.getPort() == aPort && uc.getRemoteIp() == aIPServer) // TODO - не поддерживается DNS
		{
			if (++count >= 5)
			{
				// More than 5 outbound connections to the same addr/port? Can't trust that..
				// LogManager::message("ConnectionManager::connect Tried to connect more than 5 times to " + aIPServer + ":" + Util::toString(aPort));
				dcdebug("ConnectionManager::connect Tried to connect more than 5 times to %s:%hu, connect dropped\n", aIPServer.c_str(), aPort);
				return true;
			}
		}
	}
	return false;
}

void ConnectionManager::nmdcConnect(const string& aIPServer, uint16_t aPort, const string& aNick, const string& hubUrl,
                                    const string& encoding,
                                    bool secure)
{
	nmdcConnect(aIPServer, aPort, 0, BufferedSocket::NAT_NONE, aNick, hubUrl,
	            encoding,
	            secure);
}

void ConnectionManager::nmdcConnect(const string& aIPServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aNick, const string& hubUrl,
                                    const string& encoding,
                                    bool secure)
{
	if (isShuttingDown())
		return;
		
	if (checkIpFlood(aIPServer, aPort, boost::asio::ip::address_v4(), "", "[Hub: " + hubUrl + "]"))
		return;
		
	UserConnection* uc = getConnection(true, secure); // [!] IRainman fix SSL connection on NMDC(S) hubs.
	uc->setServerPort(aIPServer + ':' + Util::toString(aPort)); // CTM2HUB
	uc->setUserConnectionToken(aNick); // Токен = ник?
	uc->setHubUrl(hubUrl);
	uc->setEncoding(encoding);
	uc->setState(UserConnection::STATE_CONNECT);
	uc->setFlag(UserConnection::FLAG_NMDC);
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
	uc->setUserConnectionToken(aToken);
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

void ConnectionManager::disconnect()
{
	safe_delete(server); // TODO Зовется чаще чем нужно.
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
		LogManager::message(STRING(CERTIFICATE_NOT_TRUSTED));
		return;
	}
	dcassert(aSource->getState() == UserConnection::STATE_CONNECT);
	if (aSource->isSet(UserConnection::FLAG_NMDC))
	{
		aSource->myNick(aSource->getUserConnectionToken());
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
	dcassert(!ClientManager::isShutdown());
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
			
			FavoriteHubEntry* fhub = FavoriteManager::getFavoriteHubEntry(i.m_HubUrl);
			if (!fhub)
				dcdebug("REASON_DETECT_CONNECTION: can't find favorite hub %s\n", i.m_HubUrl.c_str());
			dcassert(fhub);
			
			// WARNING: only Nmdc hub requests for REASON_DETECT_CONNECTION.
			// if another hub added, one must implement autodetection in base Client class
			NmdcHub* hub = static_cast<NmdcHub*>(ClientManager::getInstance()->findClient(i.m_HubUrl));
			if (!hub)
				dcdebug("REASON_DETECT_CONNECTION: can't find hub %s\n", i.m_HubUrl.c_str());
			dcassert(hub);
			
			if (hub && hub->IsAutodetectPending())
			{
				// set connection type to ACTIVE
				if (fhub)
					fhub->setMode(1);
					
				hub->AutodetectComplete();
				
				// TODO: allow to disable through GUI saving of detected mode to
				// favorite hub's settings
				
				fire(ConnectionManagerListener::DirectModeDetected(), i.m_HubUrl);
			}
			
			putConnection(aSource);
			
			return;
		}
#endif // RIP_USE_CONNECTION_AUTODETECT
		aSource->setUserConnectionToken(i.m_Nick);
		aSource->setHubUrl(i.m_HubUrl); // TODO - тут юзера почему-то еще нет
		const auto l_encoding = ClientManager::findHubEncoding(i.m_HubUrl);
		aSource->setEncoding(l_encoding);
	}
	
	const string nick = Text::toUtf8(aNick, aSource->getEncoding());// TODO IRAINMAN_USE_UNICODE_IN_NMDC
	const CID cid = ClientManager::makeCid(nick, aSource->getHubUrl());
	
	// First, we try looking in the pending downloads...hopefully it's one of them...
	if (!ClientManager::isShutdown())
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
	
	ClientManager::setIPUser(aSource->getUser(), aSource->getRemoteIp());
	
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE_ON_NMDC
	if (ClientManager::isOp(aSource->getUser(), aSource->getHubUrl()))
		aSource->setFlag(UserConnection::FLAG_OP);
#endif
		
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		aSource->myNick(aSource->getUserConnectionToken());
		aSource->lock(CryptoManager::getInstance()->getLock(), CryptoManager::getInstance()->getPk());
	}
	
	aSource->setState(UserConnection::STATE_LOCK);
}

void ConnectionManager::on(UserConnectionListener::CLock, UserConnection* aSource, const string& aLock) noexcept
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
	/* [!] Неотображаются IP адреса у юзеров, подробнее спрашивать у Тёмы. Нужен рефакторинг!
	#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	if (!p_qi->getUser()->getLastNick().empty())
	{
	    // TODO Так бывает пока не понял как - срабатывает if (aLine[0] == 'C' && !isSet(FLAG_NMDC))
	    // L: это нормальное состояние если юзер ушёл в оффлайн! Необходимо уже давно отвязать статистику от ника, и использовать CID!!!
	    const string& l_ip = p_uc->getSocket()->getIp();
	    p_uc->getUser()->setIP(l_ip);
	}
	#else // PPA_INCLUDE_LASTIP_AND_USER_RATIO
	*/
	
	p_uc->getUser()->setIP(p_uc->getSocket()->getIp(), true);
	
	
//#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
}

void ConnectionManager::addDownloadConnection(UserConnection* p_conn)
{
	dcassert(p_conn->isSet(UserConnection::FLAG_DOWNLOAD));
	ConnectionQueueItem* cqi = nullptr;
	{
		webrtc::ReadLockScoped l(*g_csDownloads);
		
		const auto i = find(g_downloads.begin(), g_downloads.end(), p_conn->getUser());
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
		uc->error(UserConnection::g_PLEASE_UPDATE_YOUR_CLIENT);
		return;
	}
#endif
	
	ConnectionQueueItem* cqi = nullptr;
	{
		webrtc::WriteLockScoped l(*g_csUploads);
		
		const auto i = find(g_uploads.begin(), g_uploads.end(), p_conn->getUser());
		if (i == g_uploads.cend())
		{
			p_conn->setFlag(UserConnection::FLAG_ASSOCIATED);
			cqi = getCQI_L(p_conn->getHintedUser(), false, p_conn->getUserConnectionToken());
			cqi->setState(ConnectionQueueItem::ACTIVE);
			
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
	
	string l_token;
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		if (!cmd.getParam("TO", 0, l_token))
		{
			aSource->send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_GENERIC, "TO missing"));
			putConnection(aSource);
			return;
		}
	}
	else
	{
		l_token = aSource->getUserConnectionToken();
	}
	
	if (aSource->isSet(UserConnection::FLAG_INCOMING))
	{
		aSource->inf(false);
	}
	
	dcassert(!l_token.empty());
	bool down;
	{
		webrtc::ReadLockScoped l(*g_csDownloads);
		const auto i = find(g_downloads.begin(), g_downloads.end(), aSource->getUser());
		
		if (i != g_downloads.cend())
		{
			(*i)->setErrors(0);
			if ((*i)->getConnectionQueueToken() == l_token) // TODO - мутоное место Ник с рандомным числом никогда ведь не могут быть равны?
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
	
	const auto i = find(g_downloads.begin(), g_downloads.end(), aUser);
	if (i != g_downloads.end())
	{
		fire(ConnectionManagerListener::Forced(), *i);
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
			const auto i = find(g_downloads.begin(), g_downloads.end(), aSource->getUser());
			dcassert(i != g_downloads.end());
			if (i == g_downloads.end())
			{
				CFlyServerJSON::pushError(5, "ConnectionManager::failed (i == g_downloads.end()) aError = " + aError);
			}
			else
			{
				ConnectionQueueItem* cqi = *i;
				cqi->setState(ConnectionQueueItem::WAITING);
				cqi->setLastAttempt(GET_TICK());
				cqi->setErrors(protocolError ? -1 : (cqi->getErrors() + 1));
				fire(ConnectionManagerListener::Failed(), cqi, aError);
				if (isShuttingDown()) // TODO
				{
					putCQI_L(cqi); // Удалять всегда нельзя - только при разрушении
					// (Closed issue 983) https://code.google.com/p/flylinkdc/issues/detail?id=983 Бесконечные подключения для скачки файл-листа
					// TODO - Найти более другое решение бага
				}
			}
		}
		else if (aSource->isSet(UserConnection::FLAG_UPLOAD))
		{
			webrtc::WriteLockScoped l(*g_csUploads); // http://code.google.com/p/flylinkdc/issues/detail?id=1439
			const auto i = find(g_uploads.begin(), g_uploads.end(), aSource->getUser());
			dcassert(i != g_uploads.end());
			if (i == g_uploads.end())
			{
				CFlyServerJSON::pushError(6, "ConnectionManager::failed (i == g_uploads.end()) aError = " + aError);
			}
			else
			{
				ConnectionQueueItem* cqi = *i;
				putCQI_L(cqi);
			}
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
	ClientManager::getInstance()->removeListener(this);
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
#ifdef _DEBUG
		LogManager::message("Wait remove from g_userConnections!");
#endif
		Thread::sleep(20);
	}
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	// TODO: please fix me: this code is not valid! downloads and uploads is empty!
	// [!] http://code.google.com/p/flylinkdc/issues/detail?id=1037
	//dcassert(downloads.empty());
	//dcassert(uploads.empty());
	// [~]
	// Сбрасываем рейтинг в базу пока не нашли причину почему тут остаются записи.
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
	if (p_conn->getUser()) // 44 падения https://www.crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=48388
	{
		uint8_t knownUcSupports = 0;
		auto unknownUcSupports = UcSupports::setSupports(p_conn, feat, knownUcSupports);
		ClientManager::getInstance()->setSupports(p_conn->getUser(), unknownUcSupports, knownUcSupports);
	}
	else
	{
		LogManager::message("Error UserConnectionListener::Supports conn->getUser() == nullptr, url = " + p_conn->getHintedUser().hint);
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
