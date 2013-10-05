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

uint16_t ConnectionManager::iConnToMeCount = 0;

ConnectionManager::ConnectionManager() : floodCounter(0), server(nullptr),
	secureServer(nullptr),
	shuttingDown(false)
{
	// [-] TimerManager::getInstance()->addListener(this); [-] IRainman fix.
	
	nmdcFeatures.push_back(UserConnection::FEATURE_MINISLOTS);
	nmdcFeatures.push_back(UserConnection::FEATURE_XML_BZLIST);
	nmdcFeatures.push_back(UserConnection::FEATURE_ADCGET);
	nmdcFeatures.push_back(UserConnection::FEATURE_TTHL);
	nmdcFeatures.push_back(UserConnection::FEATURE_TTHF);
#ifdef SMT_ENABLE_FEATURE_BAN_MSG
	nmdcFeatures.push_back(UserConnection::FEATURE_BANMSG); // !SMT!-B
#endif
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BAS0);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BASE);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_TIGR);
	adcFeatures.push_back("AD" + UserConnection::FEATURE_ADC_BZIP);
	
	TimerManager::getInstance()->addListener(this); // [+] IRainman fix.
}
ConnectionManager::~ConnectionManager()
{
	dcassert(shuttingDown == true);
	dcassert(userConnections.empty());
	//shutdown();
	
	// [!] http://code.google.com/p/flylinkdc/issues/detail?id=1037
	dcassert(downloads.empty());
	dcassert(uploads.empty());
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
		UniqueLock l(cs);
		const ConnectionQueueItem::Iter i = find(downloads.begin(), downloads.end(), aUser);
		if (i == downloads.end())
		{
			getCQI(aUser, true); // http://code.google.com/p/flylinkdc/issues/detail?id=1037
			// Не сохраняем указатель. а как и когда будем удалять ?
			return;
		}
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		else
		{
			if (find(checkIdle.begin(), checkIdle.end(), aUser) == checkIdle.end())
				checkIdle.push_back(aUser);
		}
#endif
	}
#ifndef USING_IDLERS_IN_CONNECTION_MANAGER
	DownloadManager::getInstance()->checkIdle(aUser);
#endif
}

ConnectionQueueItem* ConnectionManager::getCQI(const UserPtr& aUser, bool download)
{
	ConnectionQueueItem* cqi = new ConnectionQueueItem(aUser, download);
	if (download)
	{
		dcassert(find(downloads.begin(), downloads.end(), aUser) == downloads.end());
		downloads.push_back(cqi);
	}
	else
	{
		dcassert(find(uploads.begin(), uploads.end(), aUser) == uploads.end());
		uploads.push_back(cqi);
	}
	
	fire(ConnectionManagerListener::Added(), cqi);
	return cqi;
}

void ConnectionManager::putCQI(ConnectionQueueItem* cqi)
{
	fire(ConnectionManagerListener::Removed(), cqi);
	if (cqi->getDownload())
	{
		downloads.erase_and_check(cqi);
	}
	else
	{
		UploadManager::getInstance()->removeDelayUpload(cqi->getUser());
		uploads.erase_and_check(cqi);
	}
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	cqi->getUser()->flushRatio(); //[+]PPA branches-dev/ppa/issue-1035
#endif
	delete cqi;
}

UserConnection* ConnectionManager::getConnection(bool aNmdc, bool secure) noexcept
{
	dcassert(shuttingDown == false);
	UserConnection* uc = new UserConnection(secure);
	uc->addListener(this);
	{
		UniqueLock l(cs);
		userConnections.push_back(uc);
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
	UniqueLock l(cs);
	userConnections.erase(remove(userConnections.begin(), userConnections.end(), aConn), userConnections.end());
}

void ConnectionManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	ConnectionQueueItem::List removed;
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	UserList idlers;
#endif
	
	{
		SharedLock l(cs); // [!] IRainman opt.
		
		uint16_t attempts = 0;
		
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
		idlers.swap(checkIdle); // [!] IRainman opt: use swap.
#endif
		
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		{
			ConnectionQueueItem* cqi = *i;
			if (cqi->getState() != ConnectionQueueItem::ACTIVE)
			{
				if (!cqi->getUser()->isOnline())
				{
					// Not online anymore...remove it from the pending...
					removed.push_back(cqi);
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
						removed.push_back(cqi);
						continue;
					}
					
					bool startDown = DownloadManager::getInstance()->startDownload(prio);
					
					if (cqi->getState() == ConnectionQueueItem::WAITING)
					{
						if (startDown)
						{
							cqi->setState(ConnectionQueueItem::CONNECTING);
							ClientManager::getInstance()->connect(HintedUser(cqi->getUser(), Util::emptyString), cqi->getToken());
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
#ifdef IRAINMAN_USE_RECURSIVE_SHARED_CRITICAL_SECTION
	}
	if (!removed.empty())
	{
		UniqueLock l(cs);
#endif
		for (auto m = removed.cbegin(); m != removed.cend(); ++m)
		{
			putCQI(*m);
		}
	}
	
#ifdef USING_IDLERS_IN_CONNECTION_MANAGER
	for (auto i = idlers.cbegin(); i != idlers.cend(); ++i)
	{
		DownloadManager::getInstance()->checkIdle(*i);
	}
#endif
}

void ConnectionManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept
{
	SharedLock l(cs);
	
	for (auto j = userConnections.cbegin(); j != userConnections.cend(); ++j)
	{
		if (((*j)->getLastActivity() + 180 * 1000) < aTick)
		{
			(*j)->disconnect(true);
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
	start();
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
		
	if (now > floodCounter)
	{
		floodCounter = now + FLOOD_ADD;
	}
	else
	{
		if (now + FLOOD_TRIGGER < floodCounter)// [!] IRainman fix
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
			dcdebug("Connection flood detected!\n");
			return;
		}
		else
		{
			if (iConnToMeCount <= 0)
				floodCounter += FLOOD_ADD;
		}
	}
	UserConnection* uc = getConnection(false, secure);
	uc->setFlag(UserConnection::FLAG_INCOMING);
	uc->setState(UserConnection::STATE_SUPNICK);
	uc->setLastActivity(GET_TICK());
	try
	{
		uc->accept(sock);
	}
	catch (const Exception&)
	{
		putConnection(uc);
		delete uc;
	}
}

bool ConnectionManager::checkIpFlood(const string& aServer, uint16_t aPort, const string& userInfo)
{
	// [!] IRainman fix: no data to lock!
	/* [-] IRainman
	// Temporary fix to avoid spamming
	if (aPort == 80 || aPort == 2501)
	{
	    // FlylinkDC Team TODO: this code do we need?
	    // IRainman: Hubs are the same kick when sending requests to a foreign IP. So if a port is selected then it is really necessary.
	    AutoArray<char> buf(512);
	    snprintf(buf.get(), 512, CSTRING(ATTEMPT_TO_USE_SPAM_MESSAGE_CM), userInfo.c_str(), aServer.c_str(), aPort);
	    LogManager::getInstance()->message(buf.get());
	    return true;
	}
	*/
	// [~] IRainman fix: no data to lock!
	SharedLock l(cs);
	
	// We don't want to be used as a flooding instrument
	int count = 0;
	for (auto j = userConnections.cbegin(); j != userConnections.cend(); ++j)
	{
	
		const UserConnection& uc = **j;
		
		if (uc.socket == nullptr || !uc.socket->hasSocket()) // TODO 2012-04-23_22-28-18_ETFY7EDN5BIPZZSIMVUUBOZFZOGWBZY3F4D2HUA_2B5B184F_crash-stack-r501-build-9812.dmp
			continue;
			
		if (uc.getPort() == aPort && uc.getRemoteIp() == aServer) // https://www.box.net/shared/amuyfgz5q3o4f4ng8v76
		{
			if (++count >= 5)
			{
				// More than 5 outbound connections to the same addr/port? Can't trust that..
				dcdebug("ConnectionManager::connect Tried to connect more than 5 times to %s:%hu, connect dropped\n", aServer.c_str(), aPort);
				return true;
			}
		}
	}
	return false;
}

void ConnectionManager::nmdcConnect(const string& aServer, uint16_t aPort, const string& aNick, const string& hubUrl,
                                    const string& encoding,
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
                                    bool stealth,
#endif
                                    bool secure)
{
	nmdcConnect(aServer, aPort, 0, BufferedSocket::NAT_NONE, aNick, hubUrl,
	            encoding,
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
	            stealth,
#endif
	            secure);
}

void ConnectionManager::nmdcConnect(const string& aServer, uint16_t aPort, uint16_t localPort, BufferedSocket::NatRoles natRole, const string& aNick, const string& hubUrl,
                                    const string& encoding,
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
                                    bool stealth,
#endif
                                    bool secure)
{
	if (isShuttingDown())
		return;
		
	if (checkIpFlood(aServer, aPort, "NMDC Hub: " + hubUrl))
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
		uc->connect(aServer, aPort, localPort, natRole);
	}
	catch (const Exception&)
	{
		putConnection(uc);
		delete uc; // https://www.box.net/shared/0eeae406dbcda0198604
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
		
	if (checkIpFlood(aUser.getIdentity().getIp(), aPort, "ADC Nick: " + aUser.getIdentity().getNick() + ", Hub: " + aUser.getClientBase().getHubName()))
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
		uc->connect(aUser.getIdentity().getIp(), aPort, localPort, natRole);
	}
	catch (const Exception&)
	{
		putConnection(uc);
		delete uc;
	}
}

void ConnectionManager::disconnect() noexcept
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
		if (i->compare(0, 2, "AD") == 0)
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
		ExpectedMap::NickHubPair i = expectedConnections.remove(aNick);
		
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
		aSource->setHubUrl(i.m_HubUrl);
		aSource->setEncoding(ClientManager::getInstance()->findHubEncoding(i.m_HubUrl));
	}
	
	const string nick = Text::toUtf8(aNick, aSource->getEncoding());// TODO IRAINMAN_USE_UNICODE_IN_NMDC
	const CID cid = ClientManager::makeCid(nick, aSource->getHubUrl());
	
	// First, we try looking in the pending downloads...hopefully it's one of them...
	{
		SharedLock l(cs);
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
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
		
		aSource->setUser(ClientManager::getInstance()->findUser(cid));
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
		
	ClientManager::getInstance()->setIPUser(aSource->getUser(), aSource->getRemoteIp());
	
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE_ON_NMDC
	if (ClientManager::getInstance()->isOp(aSource->getUser(), aSource->getHubUrl()))
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
	
	if (aSource->getUser())
		ClientManager::getInstance()->setPkLock(aSource->getUser()
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
		                                        , aPk, aLock
#endif
		                                       );
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
	
	p_uc->getUser()->setIP(p_uc->getSocket()->getIp());
	
	
//#endif // PPA_INCLUDE_LASTIP_AND_USER_RATIO
}

void ConnectionManager::addDownloadConnection(UserConnection* uc)
{
	dcassert(uc->isSet(UserConnection::FLAG_DOWNLOAD));
	ConnectionQueueItem* cqi = nullptr;
	{
		SharedLock l(cs);
		
		const ConnectionQueueItem::Iter i = find(downloads.begin(), downloads.end(), uc->getUser());
		if (i != downloads.end())
		{
			cqi = *i;
			if (cqi->getState() == ConnectionQueueItem::WAITING || cqi->getState() == ConnectionQueueItem::CONNECTING)
			{
				cqi->setState(ConnectionQueueItem::ACTIVE);
				uc->setFlag(UserConnection::FLAG_ASSOCIATED);
				
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
		DownloadManager::getInstance()->addConnection(uc);
		setIP(uc, cqi); //[+]PPA
	}
	else
	{
		putConnection(uc);
	}
}

void ConnectionManager::addUploadConnection(UserConnection* uc)
{
	dcassert(uc->isSet(UserConnection::FLAG_UPLOAD));
	
#ifdef IRAINMAN_DISALLOWED_BAN_MSG
	if (uc->isSet(UserConnection::FLAG_SUPPORTS_BANMSG))
	{
		uc->error(UserConnection::PLEASE_UPDATE_YOUR_CLIENT);
		return;
	}
#endif
	
	ConnectionQueueItem* cqi = nullptr;
	{
		UniqueLock l(cs);
		
		const auto i = find(uploads.begin(), uploads.end(), uc->getUser());
		if (i == uploads.cend())
		{
			cqi = getCQI(uc->getUser(), false);
			cqi->setState(ConnectionQueueItem::ACTIVE);
			uc->setFlag(UserConnection::FLAG_ASSOCIATED);
			
#ifdef FLYLINKDC_USE_CONNECTED_EVENT
			fire(ConnectionManagerListener::Connected(), cqi);
#endif
			dcdebug("ConnectionManager::addUploadConnection, leaving to uploadmanager\n");
		}
	}
	if (cqi)
	{
		UploadManager::getInstance()->addConnection(uc);
		setIP(uc, cqi); //[+]PPA
	}
	else
	{
		putConnection(uc);
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
	
	aSource->setUser(ClientManager::getInstance()->findUser(CID(cid)));
	
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
		SharedLock l(cs);
		const ConnectionQueueItem::Iter i = find(downloads.begin(), downloads.end(), aSource->getUser());
		
		if (i != downloads.cend())
		{
			(*i)->setErrors(0);
			
			const string& to = (*i)->getToken();
			
			if (to == token)
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
	SharedLock l(cs);
	
	const ConnectionQueueItem::Iter i = find(downloads.begin(), downloads.end(), aUser);
	if (i != downloads.end())
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
	
	auto kp2 = ClientManager::getInstance()->getStringField(aSource->getUser()->getCID(), aSource->getHubUrl(), "KP");
	if (kp2.empty())
	{
		// TODO false probably
		return true;
	}
	
	if (kp2.compare(0, 7, "SHA256/") != 0)
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
	// [-] Lock l(cs); [-] IRainman fix.
	
	if (aSource->isSet(UserConnection::FLAG_ASSOCIATED))
	{
		if (aSource->isSet(UserConnection::FLAG_DOWNLOAD))
		{
			UniqueLock l(cs); // [+] IRainman fix.
			const ConnectionQueueItem::Iter i = find(downloads.begin(), downloads.end(), aSource->getUser());
			dcassert(i != downloads.end());
			ConnectionQueueItem* cqi = *i;
			cqi->setState(ConnectionQueueItem::WAITING);
			cqi->setLastAttempt(GET_TICK());
			cqi->setErrors(protocolError ? -1 : (cqi->getErrors() + 1));
			fire(ConnectionManagerListener::Failed(), cqi, aError);
			if (isShuttingDown())
			{
				putCQI(cqi); // Удалять всегда нельзя - только при разрушении
				// (Closed issue 983) https://code.google.com/p/flylinkdc/issues/detail?id=983 Бесконечные подключения для скачки файл-листа
				// TODO - Найти более другое решение бага
			}
			
		}
		else if (aSource->isSet(UserConnection::FLAG_UPLOAD))
		{
			UniqueLock l(cs); // [+] IRainman fix.
			const ConnectionQueueItem::Iter i = find(uploads.begin(), uploads.end(), aSource->getUser());
			dcassert(i != uploads.end());
			ConnectionQueueItem* cqi = *i;
			putCQI(cqi);
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
	SharedLock l(cs);
	for (auto i = userConnections.cbegin(); i != userConnections.cend(); ++i)
	{
		UserConnection* uc = *i;
		if (uc->getUser() == aUser)
			uc->disconnect(true);
	}
}

void ConnectionManager::disconnect(const UserPtr& aUser, bool isDownload) // [!] IRainman fix.
{
	SharedLock l(cs);
	for (auto i = userConnections.cbegin(); i != userConnections.cend(); ++i)
	{
		// [!] IRainman fix: please don't problem maskerate.
		// [-] if (UserConnection* uc = *i)
		UserConnection* uc = *i;
		dcassert(uc);
		// [~]
		if (uc->getUser() == aUser && uc->isSet((Flags::MaskType)(isDownload ? UserConnection::FLAG_DOWNLOAD : UserConnection::FLAG_UPLOAD)))
		{
			uc->disconnect(true);
			break;
		}
	}
}

void ConnectionManager::shutdown()
{
	dcassert(shuttingDown == false);
	shuttingDown = true;
	TimerManager::getInstance()->removeListener(this);
	disconnect();
	{
		SharedLock l(cs);
		for (auto j = userConnections.cbegin(); j != userConnections.cend(); ++j)
		{
			(*j)->disconnect(true);
		}
	}
	// Wait until all connections have died out...
	while (true)
	{
		{
			SharedLock l(cs);
			if (userConnections.empty())
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
	// Сбрасываем рейтинг в базу пока не нашли причину почему тут остаются записи.
	{
		SharedLock l(cs);
		for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
		{
			ConnectionQueueItem* cqi = *i;
			cqi->getUser()->flushRatio();
		}
		for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
		{
			ConnectionQueueItem* cqi = *i;
			cqi->getUser()->flushRatio();
		}
	}
#endif
}

// UserConnectionListener
void ConnectionManager::on(UserConnectionListener::Supports, UserConnection* conn, StringList && feat) noexcept // [!] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
{
	dcassert(conn->getUser()); // [!] IRainman fix: please don't problem maskerate.
	//PROFILE_THREAD_SCOPED();
	// [!] IRainman fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
	uint8_t knownUcSupports = 0;
	auto unknownUcSupports = UcSupports::setSupports(conn, move(feat), knownUcSupports);
	ClientManager::getInstance()->setSupports(conn->getUser(), move(unknownUcSupports), knownUcSupports);
	// [~] IRainman fix.
}

// !SMT!-S
void ConnectionManager::setUploadLimit(const UserPtr& aUser, int lim)
{
	SharedLock l(cs);
	for (auto i = userConnections.cbegin(); i != userConnections.cend(); ++i)
	{
		if ((*i)->getUser() == aUser && (*i)->isSet(UserConnection::FLAG_UPLOAD))
		{
			(*i)->setUploadLimit(lim);
		}
	}
}

/**
 * @file
 * $Id: ConnectionManager.cpp 575 2011-08-25 19:38:04Z bigmuscle $
 */
