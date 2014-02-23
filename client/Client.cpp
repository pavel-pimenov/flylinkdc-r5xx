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
#include "UploadManager.h"
#include "ThrottleManager.h"// [+] IRainman SpeedLimiter
#include "LogManager.h"
#include "CompatibilityManager.h" // [+] IRainman
#include "Wildcards.h"

boost::atomic<uint16_t> Client::counts[COUNT_UNCOUNTED];
string Client::g_last_search_string;

Client::Client(const string& p_HubURL, char separator_, bool secure_) :
	m_cs(std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock())),
	m_reconnDelay(120), m_lastActivity(GET_TICK()),
	//registered(false), [-] IRainman fix.
	autoReconnect(false),
	m_encoding(Text::systemCharset),
	state(STATE_DISCONNECTED),
	sock(0),
	m_HubURL(p_HubURL),
	m_port(0),
	m_separator(separator_),
	m_secure(secure_),
	m_countType(COUNT_UNCOUNTED),
	m_availableBytes(0),
	m_exclChecks(false) // [+] IRainman fix.
{
	dcassert(p_HubURL == Text::toLower(p_HubURL));
	const auto l_my_user = new User(ClientManager::getMyCID());
	const auto l_hub_user = new User(CID());
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
	m_HubID = CFlylinkDBManager::getInstance()->get_dic_hub_id(m_HubURL);
	dcassert(m_HubID != 0);
	l_my_user->setHubID(m_HubID); // ƒл€ сохранени€ кол-ва мессаг по самому себе
	//l_hub_user->setHubID(m_HubID); // ƒл€ бота-хаба не сохран€ем пока
#endif
	m_myOnlineUser = new OnlineUser(UserPtr(l_my_user), *this, 0); // [+] IRainman fix.
	m_hubOnlineUser = new OnlineUser(UserPtr(l_hub_user), *this, AdcCommand::HUB_SID); // [+] IRainman fix.
	
	// [-] IRainman.
	//m_hEventClientInitialized = CreateEvent(NULL, TRUE, FALSE, NULL);//[+]FlylinkDC
	
	string file, proto, query, fragment;
	Util::decodeUrl(getHubUrl(), proto, m_address, m_port, file, query, fragment);
	if (!query.empty())
	{
		m_keyprint = Util::decodeQuery(query)["kp"];
#ifdef _DEBUG
		LogManager::getInstance()->message("keyprint = " + m_keyprint);
#endif
	}
#ifdef _DEBUG
	else
	{
		LogManager::getInstance()->message("hubURL = " + getHubUrl() + " query.empty()");
	}
#endif
	TimerManager::getInstance()->addListener(this);
}

Client::~Client()
{
	dcassert(!sock);
	if (sock)
		LogManager::getInstance()->message("[Error] Client::~Client() sock == nullptr");
	FavoriteManager::getInstance()->removeUserCommand(getHubUrl());
	dcassert(FavoriteManager::getInstance()->countUserCommand(getHubUrl()) == 0);
	// In case we were deleted before we Failed
	// [-] TimerManager::getInstance()->removeListener(this); [-] IRainman fix: please see shutdown().
	updateCounts(true);
//[+]FlylinkDC
	// [-] IRainman.
	//if (m_hEventClientInitialized)
	//  CloseHandle(m_hEventClientInitialized);
//[~]FlylinkDC
}
void Client::reset_socket()
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	if (sock)
		sock->removeListeners(); // http://code.google.com/p/flylinkdc/issues/detail?id=791
	FastLock lock(csSock); // [+] brain-ripper
	if (sock)
		BufferedSocket::putSocket(sock);
#else
	if (sock)
	{
		sock->removeListeners();
		BufferedSocket::putSocket(sock);
	}
#endif
}
void Client::reconnect()
{
	disconnect(true);
	setAutoReconnect(true);
	setReconnDelay(0);
}

void Client::shutdown()
{
	TimerManager::getInstance()->removeListener(this); // [+] IRainman fix.
	state = STATE_DISCONNECTED;//[!] IRainman fix
	// [+] brain-ripper
	// Ugly hack to avoid deadlock:
	// this function captures csSock section
	// and inside putSocket there is call to removeListeners
	// that wants to capture listener's critical section.
	// that section may be captured in another thread (function Client::on(Failed, ...))
	// and that function wait on csSock section.
	//
	// So remove listeners in advance.
	// It is quite unsafe, since there is no critical section on socket
	// but shutdown called from single thread...
	// Hope this helps
	
	// [-] IRainman fix: please see reset_socket().
	// [-] if (sock)
	// [-]  sock->removeListeners();
	// [~]
	reset_socket();
}

const FavoriteHubEntry* Client::reloadSettings(bool updateNick)
{
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
	string speedDescription;
#endif
// [!] FlylinkDC mimicry function
	const FavoriteHubEntry* hub = FavoriteManager::getInstance()->getFavoriteHubEntry(getHubUrl());
	if (hub && hub->getOverrideId()) // mimicry tag
	{
		m_clientName = hub->getClientName();
		m_clientVersion = hub->getClientVersion();
	}
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
	else if (hub && hub->getStealth()) // stealth DC++
	{
		m_clientName =  "++";
		m_clientVersion = DCVERSIONSTRING;
	}
#endif
	else // FlylinkDC native
	{
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
		if (BOOLSETTING(ADD_TO_DESCRIPTION))
		{
			if (BOOLSETTING(ADD_DESCRIPTION_SLOTS))
				speedDescription += '[' + Util::toString(UploadManager::getInstance()->getFreeSlots()) + ']';
			if (BOOLSETTING(ADD_DESCRIPTION_LIMIT) && BOOLSETTING(THROTTLE_ENABLE) && ThrottleManager::getInstance()->getUploadLimitInKBytes() != 0)
				speedDescription += "[L:" + Util::toString(ThrottleManager::getInstance()->getUploadLimitInKBytes()) + "KB]";
		}
#endif
		m_clientName =  APPNAME;
		m_clientVersion = A_SHORT_VERSIONSTRING;
		if (CompatibilityManager::isWine())
		{
			m_clientVersion += "-wine";
		}
	}
// [~] FlylinkDC mimicry function
	if (hub)
	{
		if (updateNick)
		{
			string l_nick = hub->getNick(true);
			checkNick(l_nick);
			setCurrentNick(l_nick);
		}
		
		if (!hub->getUserDescription().empty())
		{
			setCurrentDescription(
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
			    speedDescription +
#endif
			    hub->getUserDescription());
		}
		else
		{
			setCurrentDescription(
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
			    speedDescription +
#endif
			    SETTING(DESCRIPTION));
		}
		
		if (!hub->getEmail().empty())
		{
			setCurrentEmail(hub->getEmail());
		}
		else
		{
			setCurrentEmail(SETTING(EMAIL));
		}
		
		if (!hub->getPassword().empty())
		{
			setPassword(hub->getPassword());
		}
		
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
		setStealth(hub->getStealth());
#endif
		//[+]FlylinkDC
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		setHideShare(hub->getHideShare());
#endif
		setFavIp(hub->getIP());
		
		if (!hub->getEncoding().empty())
		{
			setEncoding(hub->getEncoding());
		}
		
		if (hub->getSearchInterval() < 2) // [!]FlylinkDC changed 10 to 2
		{
			setSearchInterval(SETTING(MINIMUM_SEARCH_INTERVAL) * 1000);
		}
		else
		{
			setSearchInterval(hub->getSearchInterval() * 1000);
		}
		
		// [+] IRainman fix.
		m_opChat = hub->getOpChat();
		m_exclChecks = hub->getExclChecks();
		// [~] IRainman fix.
	}
	else
	{
		if (updateNick)
		{
			string l_nick = SETTING(NICK);
			checkNick(l_nick);
			setCurrentNick(l_nick);
		}
		setCurrentDescription(
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
		    speedDescription +
#endif
		    SETTING(DESCRIPTION));
		setCurrentEmail(SETTING(EMAIL));
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
		setStealth(false);
#endif
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		setHideShare(false);
#endif
		setFavIp(Util::emptyString);
		
		setSearchInterval(SETTING(MINIMUM_SEARCH_INTERVAL) * 1000);
		
		// [+] IRainman fix.
		m_opChat.clear();
		m_exclChecks = false;
		// [~] IRainman fix.
	}
	/* [-] IRainman mimicry function
	// !SMT!-S
	for (string::size_type i = 0; i < ClientId.length(); i++)
	    if (ClientId[i] == '<' || ClientId[i] == '>' || ClientId[i] == ',' || ClientId[i] == '$' || ClientId[i] == '|')
	    {
	        ClientId = ClientId.substr(0, i);
	        break;
	    }
	*/
	// [~] IRainman mimicry function
	return hub;
}

void Client::connect()
{
	reset_socket();
	// [!]IRainman moved to two function:
	// void Client::on(Failed, const string& aLine)
	// void Client::disconnect(bool graceLess)
	clearAvailableBytes();
	
	setAutoReconnect(true);
	setReconnDelay(120 + Util::rand(0, 60));
	const FavoriteHubEntry* fhe = reloadSettings(true);
	// [!]IRainman fix.
	resetRegistered(); // [!]
	resetOp(); // [+]
	// [-] setMyIdentity(Identity(ClientManager::getInstance()->getMe(), 0)); [-]
	// [-] setHubIdentity(Identity()); [-]
	// [~] IRainman fix.
	
	state = STATE_CONNECTING;
	
	try
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		FastLock lock(csSock); // [+] brain-ripper
#endif
		sock = BufferedSocket::getSocket(m_separator);
		sock->addListener(this);
		sock->connect(m_address,
		              m_port,
		              m_secure,
		              BOOLSETTING(ALLOW_UNTRUSTED_HUBS),
		              true);
		dcdebug("Client::connect() %p\n", (void*)this);
	}
	catch (const Exception& e)
	{
		state = STATE_DISCONNECTED;
		fire(ClientListener::Failed(), this, e.getError());
	}
	m_isActivMode = ClientManager::isActive(fhe); // [+] IRainman opt.
	updateActivity();
}

void Client::send(const char* aMessage, size_t aLen)
{
	if (!isReady())
	{
		dcdebug("Send message failed, hub is disconnected!");//[+] IRainman
		dcassert(isReady()); // ѕод отладкой падаем тут. найти причину.
		return;
	}
	updateActivity();
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		FastLock lock(csSock); // [+] brain-ripper
#endif
		sock->write(aMessage, aLen);
	}
	if (!CompatibilityManager::isWine())
	{
		COMMAND_DEBUG(aMessage, DebugTask::HUB_OUT, getIpPort());
	}
}

void Client::on(Connected) noexcept
{
	updateActivity();
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		FastLock lock(csSock); // [+] brain-ripper
#endif
		boost::system::error_code ec;
		m_ip      = boost::asio::ip::address_v4::from_string(sock->getIp(), ec);
		dcassert(!ec);
	}
	if (sock->isSecure() && m_keyprint.compare(0, 7, "SHA256/", 7) == 0)
	{
		const auto kp = sock->getKeyprint();
		if (!kp.empty())
		{
			vector<uint8_t> kp2v(kp.size());
			Encoder::fromBase32(m_keyprint.c_str() + 7, &kp2v[0], kp2v.size());
			if (!std::equal(kp.begin(), kp.end(), kp2v.begin()))
			{
				state = STATE_DISCONNECTED;
				sock->removeListener(this);
				fire(ClientListener::Failed(), this, "Keyprint mismatch");
				return;
			}
		}
	}
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
	FavoriteManager::getInstance()->changeConnectionStatus(getHubUrl(), ConnectionStatus::SUCCES);
#endif
	fire(ClientListener::Connected(), this);
	state = STATE_PROTOCOL;
}

void Client::on(Failed, const string& aLine) noexcept
{
	// although failed consider initialized
	state = STATE_DISCONNECTED;//[!] IRainman fix
	FavoriteManager* l_fm = FavoriteManager::getInstance();
	l_fm->removeUserCommand(getHubUrl());
	
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		FastLock lock(csSock); // [+] brain-ripper
#endif
		if (sock)
			sock->removeListener(this);
	}
	// [-] IRainman.
	//SetEvent(m_hEventClientInitialized);
	updateActivity();
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
	l_fm->changeConnectionStatus(getHubUrl(), ConnectionStatus::CONNECTION_FAILURE);
#endif
	fire(ClientListener::Failed(), this, aLine);
}

void Client::disconnect(bool graceLess)
{
	state = STATE_DISCONNECTED;//[!] IRainman fix
	FavoriteManager::getInstance()->removeUserCommand(getHubUrl());
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	FastLock lock(csSock); // [+] brain-ripper
#endif
	if (sock)
		sock->disconnect(graceLess);
}

bool Client::isSecure() const
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	if (!isReady())//[+] IRainman fast shutdown!
		return false;
		
	FastLock lock(csSock); // [+] brain-ripper
#endif
	return sock && sock->isSecure();//[!] IRainman fix
}

bool Client::isTrusted() const
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	if (!isReady())//[+] IRainman fast shutdown!
		return false;
		
	FastLock lock(csSock); // [+] brain-ripper
#endif
	return sock && sock->isTrusted();//[!] IRainman fix
}

string Client::getCipherName() const noexcept
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
    if (!isReady())//[+] IRainman fast shutdown!
    return Util::emptyString;
    
    FastLock lock(csSock); // [+] brain-ripper
#endif
    return sock ? sock->getCipherName() : Util::emptyString;//[!] IRainman fix
}

vector<uint8_t> Client::getKeyprint() const
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		if (!isReady())//[+] IRainman fast shutdown!
			return Util::emptyByteVector;
			
		FastLock lock(csSock); // [+] brain-ripper
#endif
		return sock ? sock->getKeyprint() : Util::emptyByteVector; // [!] IRainman fix
	}

void Client::updateCounts(bool aRemove)
{
	// We always remove the count and then add the correct one if requested...
	if (m_countType != COUNT_UNCOUNTED)
	{
		--counts[m_countType];
		m_countType = COUNT_UNCOUNTED;
	}
	
	if (!aRemove)
	{
		if (isOp())
		{
			m_countType = COUNT_OP;
		}
		else if (isRegistered())
		{
			m_countType = COUNT_REGISTERED;
		}
		else
		{
			m_countType = COUNT_NORMAL;
		}
		++counts[m_countType];
	}
}

string Client::getLocalIp() const
{
	// [!] IRainman fix:
	// [!] If possible, always return the hub that IP, which he identified with us when you connect.
	// [!] This saves the user from a variety of configuration problems.
	const string& myUserIp = getMyIdentity().getIpAsString(); // [!] opt, and fix done: [4] https://www.box.net/shared/c497f50da28f3dfcc60a
	if (!myUserIp.empty())
	{
		return myUserIp; // [!] Best case - the server detected it.
	}
	// Favorite hub Ip
	if (!getFavIp().empty())
	{
		return Socket::resolve(getFavIp());
	}
	const auto& settingIp = SETTING(EXTERNAL_IP);
	if (!BOOLSETTING(NO_IP_OVERRIDE) && !settingIp.empty() &&
	        SETTING(INCOMING_CONNECTIONS) != SettingsManager::INCOMING_DIRECT)   // !SMT!-F
	{
		return Socket::resolve(settingIp);
	}
	
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		FastLock lock(csSock); // [+] brain-ripper
#endif
		if (sock)
		{
			return sock->getLocalIp();
		}
	}
	return Util::getLocalOrBindIp(false);
	// [~] IRainman fix.
}

uint64_t Client::search(Search::SizeModes aSizeMode, int64_t aSize, Search::TypeModes aFileType, const string& aString, const string& aToken, const StringList& aExtList, void* owner, bool p_is_force_passive)
{
	dcdebug("Queue search %s\n", aString.c_str());
	
	if (searchQueue.interval)
	{
		Search s;
		s.m_is_force_passive = p_is_force_passive;
		s.m_fileTypes_bitmap = aFileType; // TODO - проверить что тут все ок.
		s.size     = aSize;
		s.query    = aString;
		s.sizeMode = aSizeMode;
		s.token    = aToken;
		s.exts     = aExtList;
		s.owners.insert(owner);
		
		searchQueue.add(s);
		
		uint64_t now = GET_TICK(); // [+] IRainman opt
		return searchQueue.getSearchTime(owner, now) - now;
	}
	
	search(aSizeMode, aSize, aFileType , aString, aToken, aExtList, p_is_force_passive);
	return 0;
	
}

void Client::on(Line, const string& aLine) noexcept
{
	updateActivity();
	COMMAND_DEBUG(aLine, DebugTask::HUB_IN, getIpPort());
}

void Client::on(Second, uint64_t aTick) noexcept
{
	if (state == STATE_DISCONNECTED && getAutoReconnect() && (aTick > (getLastActivity() + getReconnDelay() * 1000)))
	{
		// Try to reconnect...
		connect();
	}
	
	if (!searchQueue.interval)
		return;
		
	if (isConnected())
	{
		Search s;
		if (searchQueue.pop(s, aTick)) // [!] IRainman opt
		{
			// TODO - пробежатьс€ по битовой маске?
			// ≈сли она там есть
			search(s.sizeMode, s.size, Search::TypeModes(s.m_fileTypes_bitmap), s.query, s.token, s.exts, s.m_is_force_passive);
		}
	}
}

// !SMT!-S
OnlineUserPtr Client::getUser(const UserPtr& aUser)
{
	// for generic client, use ClientManager, but it does not correctly handle ClientManager::me
	ClientManager::LockInstanceOnlineUsers lockedInstance; // [+] IRainman fix.
	return lockedInstance->getOnlineUserL(aUser);
}
// [+] IRainman fix.
bool Client::allowPrivateMessagefromUser(const ChatMessage& message)
{
	if (isMe(message.m_replyTo))
	{
		if (UserManager::expectPasswordFromUser(message.m_to->getUser())
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		        || UploadManager::getInstance()->isBanReply(message.m_to->getUser()) // !SMT!-S
#endif
		   )
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	else if (message.thirdPerson && BOOLSETTING(NSL_IGNORE_ME))
	{
		return false;
	}
	else if (UserManager::isInIgnoreList(message.m_replyTo->getIdentity().getNick())) // !SMT!-S
	{
		return false;
	}
	else if (BOOLSETTING(SUPPRESS_PMS))
	{
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		if (UploadManager::getInstance()->isBanReply(message.m_replyTo->getUser())) // !SMT!-S
		{
			return false;
		}
		else
#endif
			if (FavoriteManager::getInstance()->isNoFavUserOrUserIgnorePrivate(message.m_replyTo->getUser()))
			{
				if (BOOLSETTING(LOG_IF_SUPPRESS_PMS))
				{
					LocalArray<char, 200> l_buf;
					snprintf(l_buf.data(), l_buf.size(), CSTRING(LOG_IF_SUPPRESS_PMS), message.m_replyTo->getIdentity().getNick().c_str(), getHubName().c_str(), getHubUrl().c_str());
					LogManager::getInstance()->message(l_buf.data());
				}
				return false;
			}
			else
			{
				return true;
			}
	}
	else if (message.m_replyTo->getIdentity().isHub())
	{
		if (BOOLSETTING(IGNORE_HUB_PMS) && !isInOperatorList(message.m_replyTo->getIdentity().getNick()))
		{
			fire(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.m_text);
			return false;
		}
		else if (FavoriteManager::getInstance()->hasIgnorePM(message.m_replyTo->getUser()))
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	else if (message.m_replyTo->getIdentity().isBot())
	{
		if (BOOLSETTING(IGNORE_BOT_PMS) && !isInOperatorList(message.m_replyTo->getIdentity().getNick()))
		{
			fire(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.m_text);
			return false;
		}
		else if (FavoriteManager::getInstance()->hasIgnorePM(message.m_replyTo->getUser()))
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	else if (BOOLSETTING(PROTECT_PRIVATE) && !FavoriteManager::getInstance()->hasFreePM(message.m_replyTo->getUser())) // !SMT!-PSW
	{
		switch (UserManager::checkPrivateMessagePassword(message))
		{
			case UserManager::FREE:
			{
				return true;
			}
			case UserManager::WAITING:
			{
				return false;
			}
			case UserManager::FIRST:
			{
				StringMap params;
				params["pm_pass"] = SETTING(PM_PASSWORD);
				privateMessage(message.m_replyTo, Util::formatParams(SETTING(PM_PASSWORD_HINT), params, false), false);
				if (BOOLSETTING(PROTECT_PRIVATE_SAY))
				{
					fire(ClientListener::StatusMessage(), this, STRING(REJECTED_PRIVATE_MESSAGE_FROM) + ": " + message.m_replyTo->getIdentity().getNick());
				}
				return false;
			}
			case UserManager::CHECKED:
			{
				privateMessage(message.m_replyTo, SETTING(PM_PASSWORD_OK_HINT), true);
				
				// TODO needs?
				// const tstring passwordOKMessage = _T('<') + message.m_replyTo->getUser()->getLastNickT() + _T("> ") + TSTRING(PRIVATE_CHAT_PASSWORD_OK_STARTED);
				// PrivateFrame::gotMessage(from, to, replyTo, passwordOKMessage, getHubHint(), myPM, pm.thirdPerson); // !SMT!-S
				
				return true;
			}
			default: // Only for compiler.
			{
				dcassert(0);
				return false;
			}
		}
	}
	else
	{
		if (FavoriteManager::getInstance()->hasIgnorePM(message.m_replyTo->getUser())
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		        || UploadManager::getInstance()->isBanReply(message.m_replyTo->getUser()) // !SMT!-S
#endif
		   )
		{
			return false;
		}
		else
		{
			return true;
		}
	}
}

bool Client::allowChatMessagefromUser(const ChatMessage& message, const string& p_nick)
{
	if (!message.m_from)
	{
		if (!p_nick.empty() && UserManager::isInIgnoreList(p_nick)) // http://code.google.com/p/flylinkdc/issues/detail?id=1432
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	else if (isMe(message.m_from))
	{
		return true;
	}
	else if (message.thirdPerson && BOOLSETTING(NSL_IGNORE_ME))
	{
		return false;
	}
	if (BOOLSETTING(SUPPRESS_MAIN_CHAT) && !isOp())
	{
		return false;
	}
	else if (UserManager::isInIgnoreList(message.m_from->getIdentity().getNick())) // !SMT!-S
	{
		return false;
	}
	else
	{
		return true;
	}
}

void Client::messageYouHaweRightOperatorOnThisHub()
{
	AutoArray<char> buf(512);
	snprintf(buf.data(), 512, CSTRING(AT_HUB_YOU_HAVE_RIGHT_OPERATOR), getHubUrl().c_str());
	LogManager::getInstance()->message(buf.data());
}
// [~] IRainman fix.

/**
 * @file
 * $Id: Client.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
