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
#include "MappingManager.h"
#include "QueueManager.h"
#include "SearchManager.h"
#include "Wildcards.h"
#include "../FlyFeatures/flyServer.h"

unsigned Client::g_counts[COUNT_UNCOUNTED];
string   Client::g_last_search_string;
Client::Client(const string& p_HubURL, char p_separator, bool p_is_secure, bool p_is_auto_connect) :
	m_cs(std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock())),
	m_reconnDelay(120), m_lastActivity(GET_TICK()),
//registered(false), [-] IRainman fix.
	autoReconnect(false),
	m_encoding(Text::g_systemCharset),
	state(STATE_DISCONNECTED),
	m_client_sock(0),
	m_HubURL(p_HubURL),
	m_port(0),
	m_separator(p_separator),
	m_is_secure_connect(p_is_secure),
	m_countType(COUNT_UNCOUNTED),
	m_availableBytes(0),
	m_isChangeAvailableBytes(false),
	m_exclChecks(false), // [+] IRainman fix.
	m_message_count(0),
	m_is_hide_share(0),
	m_is_override_name(false),
	m_is_fly_support_hub(false),
	m_vip_icon_index(0),
	m_is_suppress_chat_and_pm(false)
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	, m_isAutobanAntivirusIP(false),
	m_isAutobanAntivirusNick(false)
#endif
	//m_count_validate_denide(0)
{
	dcassert(p_HubURL == Text::toLower(p_HubURL));
	const auto l_my_user = new User(ClientManager::getMyCID());
	const auto l_hub_user = new User(CID());
#ifdef FLYLINKDC_USE_LASTIP_AND_USER_RATIO
	m_HubID = CFlylinkDBManager::getInstance()->get_dic_hub_id(m_HubURL);
	dcassert(m_HubID != 0);
	l_my_user->setHubID(m_HubID); // ƒл€ сохранени€ кол-ва мессаг по самому себе
	//l_hub_user->setHubID(m_HubID); // ƒл€ бота-хаба не сохран€ем пока
#endif
	const auto l_lower_url = Text::toLower(m_HubURL);
	if (!p_is_auto_connect && !Util::isAdcHub(l_lower_url))
	{
		const auto l_pos_ru = l_lower_url.rfind(".ru");
		if (l_pos_ru != string::npos)
		{
			if (l_pos_ru == l_lower_url.size() - 3 ||
			        l_pos_ru < l_lower_url.size() - 4 && l_lower_url[l_pos_ru + 3] == ':')
			{
				m_encoding = Text::g_code1251;
			}
		}
	}
	static const char* g_vip_icons_array[] =   // VIP_ICON
	{
		"dcsamara.net",
		"milenahub.ru",
		"stealthhub.ru",
		"keepclear.org",
		"prime-hub.ru",
		"planet-dc.ru",
		"allavtovo.ru",
		"adc.podryad.tv",
		"nsk154hub.ru",
		"prostoigra24.ru",
		"eva-hub.ru",
		"aab-new-adrenalin.ru",
		"titankaluga.ru"
	};
	if (l_lower_url.find("dc.fly-server.ru") != string::npos ||
	        l_lower_url.find("adcs.flylinkdc.com") != string::npos ||
	        l_lower_url.find("dc.livedc.ru") != string::npos)
	{
		m_is_fly_support_hub = true;
	}
	m_vip_icon_index = 0;
	for (int i = 0; i < _countof(g_vip_icons_array); ++i)
	{
		if (l_lower_url.find(g_vip_icons_array[i]) != string::npos)
		{
			m_vip_icon_index = i + 1;
			break;
		}
	}
	
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
		LogManager::message("m_keyprint = " + m_keyprint);
#endif
	}
#ifdef _DEBUG
	else
	{
		//LogManager::message("hubURL = " + getHubUrl() + " query.empty()");
	}
#endif
	TimerManager::getInstance()->addListener(this);
}

Client::~Client()
{
	dcassert(!m_client_sock);
	if (m_client_sock)
	{
		LogManager::message("[Error] Client::~Client() sock == nullptr");
	}
	FavoriteManager::removeUserCommand(getHubUrl());
	if (!ClientManager::isShutdown())
	{
		dcassert(FavoriteManager::countUserCommand(getHubUrl()) == 0);
		// In case we were deleted before we Failed
		// [-] TimerManager::getInstance()->removeListener(this); [-] IRainman fix: please see shutdown().
		updateCounts(true);
	}
	
}
void Client::reset_socket()
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	if (m_client_sock)
		m_client_sock->removeListeners();
	CFlyFastLock(lock(csSock); // [+] brain-ripper
	             if (m_client_sock)
	             BufferedSocket::putSocket(m_client_sock);
#else
	if (m_client_sock)
	{
		m_client_sock->removeListeners();
		BufferedSocket::putBufferedSocket(m_client_sock);
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
	const FavoriteHubEntry* hub = FavoriteManager::getFavoriteHubEntry(getHubUrl());
	extern bool g_UseStrongDCTag;
	m_is_override_name = (hub && hub->getOverrideId() || g_UseStrongDCTag);
	if (m_is_override_name) // mimicry tag
	{
		if (g_UseStrongDCTag)
		{
			m_clientName    = "StrgDC++";
			m_clientVersion = "2.42";
		}
		else
		{
			if (hub)
			{
				m_clientName = hub->getClientName();
				m_clientVersion = hub->getClientVersion();
			}
		}
	}
	else // FlylinkDC native
	{
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
		if (BOOLSETTING(ADD_TO_DESCRIPTION))
		{
			if (BOOLSETTING(ADD_DESCRIPTION_SLOTS))
				speedDescription += '[' + Util::toString(UploadManager::getFreeSlots()) + ']';
			if (BOOLSETTING(ADD_DESCRIPTION_LIMIT) && BOOLSETTING(THROTTLE_ENABLE) && ThrottleManager::getInstance()->getUploadLimitInKBytes() != 0)
				speedDescription += "[L:" + Util::toString(ThrottleManager::getInstance()->getUploadLimitInKBytes()) + "KB]";
		}
#endif
		m_clientName = getFlylinkDCAppCaption();
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
			if (!getRandomNick().empty())
				l_nick = getRandomNick();
			checkNick(l_nick);
			setMyNick(l_nick);
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
			dcassert(SETTING(MINIMUM_SEARCH_INTERVAL) > 2);
			const auto l_new_interval = std::max(SETTING(MINIMUM_SEARCH_INTERVAL), 2);
			setSearchInterval(l_new_interval * 1000);
		}
		else
		{
			setSearchInterval(hub->getSearchInterval() * 1000);
		}
		
		// [+] IRainman fix.
		m_opChat = hub->getOpChat();
		m_exclChecks = hub->getExclChecks();
		// [~] IRainman fix.
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		m_isAutobanAntivirusIP = hub->getAutobanAntivirusIP();
		m_isAutobanAntivirusNick = hub->getAutobanAntivirusNick();
		m_AntivirusCommandIP = hub->getAntivirusCommandIP();
#endif
	}
	else
	{
		if (updateNick)
		{
			string l_nick = SETTING(NICK);
			checkNick(l_nick);
			setMyNick(l_nick);
		}
		setCurrentDescription(
#ifdef IRAINMAN_ENABLE_SLOTS_AND_LIMIT_IN_DESCRIPTION
		    speedDescription +
#endif
		    SETTING(DESCRIPTION));
		setCurrentEmail(SETTING(EMAIL));
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		setHideShare(false);
#endif
		setFavIp(Util::emptyString);
		
		setSearchInterval(SETTING(MINIMUM_SEARCH_INTERVAL) * 1000);
		
		// [+] IRainman fix.
		m_opChat.clear();
		m_exclChecks = false;
		// [~] IRainman fix.
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
		m_isAutobanAntivirusIP = false;
		m_isAutobanAntivirusNick = false;
		m_AntivirusCommandIP.clear();
#endif
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
	clearAvailableBytesL();
	setAutoReconnect(true);
	setReconnDelay(120 + Util::rand(0, 60));
	//const FavoriteHubEntry* l_fhe =
	reloadSettings(true);
	resetRegistered();
	resetOp();
	
	state = STATE_CONNECTING;
	
	try
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
		             m_client_sock = BufferedSocket::getBufferedSocket(m_separator);
		             m_client_sock->addListener(this);
		             m_client_sock->connect(m_address,
		                                    m_port,
		                                    m_is_secure_connect,
		                                    BOOLSETTING(ALLOW_UNTRUSTED_HUBS),
		                                    true);
		             dcdebug("Client::connect() %p\n", (void*)this);
	}
	             catch (const Exception& e)
	{
		state = STATE_DISCONNECTED;
		fly_fire2(ClientListener::ClientFailed(), this, e.getError());
	}
	//m_isActivMode = ClientManager::isActive(fhe); // [+] IRainman opt.
	updateActivity();
}
bool ClientBase::isActive() const
{
	if (SETTING(FORCE_PASSIVE_INCOMING_CONNECTIONS))
	{
		return false;
	}
	else
	{
		const FavoriteHubEntry* fe = FavoriteManager::getFavoriteHubEntry(getHubUrl());
		const auto l_res = ClientManager::isActive(fe) || isDetectActiveConnection();
		return l_res;
	}
}

void Client::send(const char* aMessage, size_t aLen)
{
	if (!isReady())
	{
		dcdebug("Send message failed, hub is disconnected!");//[+] IRainman
		//dcassert(isReady()); // ѕод отладкой падаем тут. найти причину.
		return;
	}
	updateActivity();
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
		             m_client_sock->write(aMessage, aLen);
	}
	
	             COMMAND_DEBUG(toUtf8(string(aMessage, aLen)), DebugTask::HUB_OUT, getIpPort());
}

void Client::on(Connected) noexcept
{
	updateActivity();
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
		boost::system::error_code ec;
		m_ip      = boost::asio::ip::address_v4::from_string(m_client_sock->getIp(), ec);
		dcassert(!ec);
	}
	if (m_client_sock->isSecure() && m_keyprint.compare(0, 7, "SHA256/", 7) == 0)
	{
		const auto kp = m_client_sock->getKeyprint();
		if (!kp.empty())
		{
			vector<uint8_t> kp2v(kp.size());
			Encoder::fromBase32(m_keyprint.c_str() + 7, &kp2v[0], kp2v.size());
			if (!std::equal(kp.begin(), kp.end(), kp2v.begin()))
			{
				state = STATE_DISCONNECTED;
				m_client_sock->removeListener(this);
				fly_fire2(ClientListener::ClientFailed(), this, "Keyprint mismatch");
				return;
			}
		}
	}
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
	FavoriteManager::changeConnectionStatus(getHubUrl(), ConnectionStatus::SUCCES);
#endif
	fly_fire1(ClientListener::Connected(), this);
	state = STATE_PROTOCOL;
}

void Client::on(Failed, const string& aLine) noexcept
{
	// although failed consider initialized
	state = STATE_DISCONNECTED;//[!] IRainman fix
	FavoriteManager::removeUserCommand(getHubUrl());
	
	{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
		CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
		if (m_client_sock)
	{
		m_client_sock->removeListener(this);
		}
	}
// [-] IRainman.
//SetEvent(m_hEventClientInitialized);
	if (!ClientManager::isShutdown())
	{
		updateActivity();
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		FavoriteManager::changeConnectionStatus(getHubUrl(), ConnectionStatus::CONNECTION_FAILURE);
#endif
	}
	fly_fire2(ClientListener::ClientFailed(), this, aLine);
}

void Client::disconnect(bool p_graceLess)
{
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	{
		CFlyFastLock(m_cs_virus);
		m_virus_nick.clear();
	}
#endif
	
	state = STATE_DISCONNECTED;//[!] IRainman fix
	FavoriteManager::removeUserCommand(getHubUrl());
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
	             if (m_client_sock)
	             m_client_sock->disconnect(p_graceLess);
}

             bool Client::isSecure() const
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	if (!isReady())//[+] IRainman fast shutdown!
		return false;
		
	CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
	             return m_client_sock && m_client_sock->isSecure();//[!] IRainman fix
}

             bool Client::isTrusted() const
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	if (!isReady())//[+] IRainman fast shutdown!
		return false;
		
	CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
	             return m_client_sock && m_client_sock->isTrusted();//[!] IRainman fix
}

             string Client::getCipherName() const
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	if (!isReady())//[+] IRainman fast shutdown!
		return Util::emptyString;
		
	CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
	             return m_client_sock ? m_client_sock->getCipherName() : Util::emptyString;//[!] IRainman fix
}

             vector<uint8_t> Client::getKeyprint() const
{
#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	if (!isReady())//[+] IRainman fast shutdown!
		return Util::emptyByteVector;
		
	CFlyFastLock(lock(csSock); // [+] brain-ripper
#endif
	             return m_client_sock ? m_client_sock->getKeyprint() : Util::emptyByteVector; // [!] IRainman fix
}

             void Client::updateCounts(bool aRemove)
{
	// We always remove the count and then add the correct one if requested...
	if (m_countType != COUNT_UNCOUNTED)
	{
		--g_counts[m_countType];
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
		++g_counts[m_countType];
	}
}

void Client::clearAvailableBytesL()
{
	m_isChangeAvailableBytes = true;
	m_availableBytes = 0;
}
void Client::decBytesSharedL(int64_t p_bytes_shared)
{
	//dcdrun(const auto l_oldSum = m_availableBytes);
	//dcassert(l_oldSum >= 0);
	const auto l_old = p_bytes_shared;
	dcassert(l_old >= 0);
	m_availableBytes -= l_old;
	m_isChangeAvailableBytes = l_old != 0;
}
bool Client::changeBytesSharedL(Identity& p_id, const int64_t p_bytes)
{
	dcassert(p_bytes >= 0);
	//dcdrun(const auto l_oldSum = m_availableBytes);
	//dcassert(l_oldSum >= 0);
	const auto l_old = p_id.getBytesShared();
	m_isChangeAvailableBytes = p_bytes != l_old;
	if (m_isChangeAvailableBytes)
	{
		dcassert(l_old >= 0);
		m_availableBytes -= l_old;
		p_id.setBytesShared(p_bytes);
		m_availableBytes += p_bytes;
	}
	return m_isChangeAvailableBytes;
}

void Client::fire_user_updated(const OnlineUserList& p_list)
{
	if (!p_list.empty() && !ClientManager::isShutdown())
	{
		fly_fire2(ClientListener::UsersUpdated(), this, p_list);
	}
}

void Client::updatedMyINFO(const OnlineUserPtr& aUser)
{
	if (!ClientManager::isBeforeShutdown())
	{
		fly_fire1(ClientListener::UserUpdatedMyINFO(), aUser);
	}
}

string Client::getLocalIp() const
{
	// [!] IRainman fix:
	// [!] If possible, always return the hub that IP, which he identified with us when you connect.
	// [!] This saves the user from a variety of configuration problems.
	if (getMyIdentity().isIPValid())
	{
		const string& myUserIp = getMyIdentity().getIpAsString(); // [!] opt, and fix done: [4] https://www.box.net/shared/c497f50da28f3dfcc60a
		if (!myUserIp.empty())
		{
			return myUserIp; // [!] Best case - the server detected it.
		}
	}
	// Favorite hub Ip
	if (!getFavIp().empty())
	{
		return Socket::resolve(getFavIp());
	}
	const auto settingIp = SETTING(EXTERNAL_IP);
	if (!settingIp.empty())  // !SMT!-F
	{
		return Socket::resolve(settingIp);
	}
	if (!MappingManager::getExternaIP().empty() && SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_UPNP)
	{
		return MappingManager::getExternaIP();
	}
	const string l_local_ip = Util::getLocalOrBindIp(false);
	return l_local_ip;
	// [~] IRainman fix.
	/*
	{
	#ifdef FLYLINKDC_USE_CS_CLIENT_SOCKET
	        CFlyFastLock(lock(csSock); // [+] brain-ripper
	#endif
	        if (m_client_sock)
	        {
	            return m_client_sock->getLocalIp();
	        }
	    }
	*/
}

uint64_t Client::search_internal(const SearchParamToken& p_search_param)
{
	//dcdebug("Queue search %s\n", p_search_param.m_filter.c_str());
	
	if (m_searchQueue.m_interval)
	{
		Search s;
		s.m_is_force_passive_searh = p_search_param.m_is_force_passive_searh;
		s.m_fileTypes_bitmap = p_search_param.m_file_type; // TODO - проверить что тут все ок?
		s.m_size     = p_search_param.m_size;
		s.m_query    = p_search_param.m_filter;
		s.m_sizeMode = p_search_param.m_size_mode;
		s.m_token    = p_search_param.m_token;
		s.m_ext_list     = p_search_param.m_ext_list;
		s.m_owners.insert(p_search_param.m_owner);
		
		m_searchQueue.add(s);
		
		const uint64_t now = GET_TICK(); // [+] IRainman opt
		return m_searchQueue.getSearchTime(p_search_param.m_owner, now) - now;
	}
	// TODO - разобратьс€ с этим местом.
	// мертвый код
	SearchParamToken l_search_param_token;
	l_search_param_token.m_token = p_search_param.m_token;
	l_search_param_token.m_size_mode = p_search_param.m_size_mode;
	l_search_param_token.m_file_type = p_search_param.m_file_type;
	l_search_param_token.m_size = p_search_param.m_size;
	l_search_param_token.m_filter = p_search_param.m_filter;
	l_search_param_token.m_is_force_passive_searh = p_search_param.m_is_force_passive_searh;
	l_search_param_token.m_ext_list = p_search_param.m_ext_list;
	l_search_param_token.m_owner = p_search_param.m_owner; // –аньше тут его не было.
	search_token(l_search_param_token);
	return 0;
	
}

void Client::on(Line, const string& aLine) noexcept
{
	updateActivity();
	COMMAND_DEBUG(aLine, DebugTask::HUB_IN, getIpPort());
}

void Client::on(Minute, uint64_t aTick) noexcept
{
	if (state == STATE_NORMAL && (aTick >= (getLastActivity() + 118 * 1000)))
	{
		send(&m_separator, 1);
	}
}

void Client::on(Second, uint64_t aTick) noexcept
{
	if (state == STATE_DISCONNECTED && getAutoReconnect() && (aTick > (getLastActivity() + getReconnDelay() * 1000)))
	{
		// Try to reconnect...
		connect();
	}
	else if (state == STATE_IDENTIFY && (getLastActivity() + 30000) < aTick) // (c) PPK http://www.czdc.org
	{
		if (m_client_sock)
		{
			m_client_sock->disconnect(false);
		}
	}
	else if ((state == STATE_CONNECTING || state == STATE_PROTOCOL) && (getLastActivity() + 60000) < aTick)
	{
		reconnect();
	}
	if (m_searchQueue.m_interval == 0)
	{
		//dcassert(m_searchQueue.m_interval != 0);
		return;
	}
	
	if (isConnected() && !m_searchQueue.empty())
	{
		Search s;
		if (m_searchQueue.pop(s, aTick)) // [!] IRainman opt
		{
			// TODO - пробежатьс€ по битовой маске?
			// ≈сли она там есть
			SearchParamToken l_search_param_token;
			l_search_param_token.m_token = s.m_token;
			l_search_param_token.m_size_mode = s.m_sizeMode;
			l_search_param_token.m_file_type = Search::TypeModes(s.m_fileTypes_bitmap);
			l_search_param_token.m_size = s.m_size;
			l_search_param_token.m_filter = s.m_query;
			l_search_param_token.m_is_force_passive_searh = s.m_is_force_passive_searh;
			l_search_param_token.m_ext_list = s.m_ext_list;
			l_search_param_token.m_owner = nullptr;
			search_token(l_search_param_token);
		}
	}
}
bool Client::isFloodCommand(const string& p_command, const string& p_line)
{
	if (is_all_my_info_loaded() && CFlyServerConfig::g_interval_flood_command)
	{
		if (!CFlyServerConfig::isIgnoreFloodCommand(p_command))
		{
			CFlyFloodCommand l_item;
			l_item.m_start_tick = GET_TICK();
			l_item.m_tick = l_item.m_start_tick;
			auto l_flood_find = m_flood_detect.insert(std::make_pair(p_command, l_item));
			if (l_flood_find.second == false)
			{
				auto& l_result = l_flood_find.first->second;
				l_result.m_count++;
				l_result.m_tick = l_item.m_tick;
				if (BOOLSETTING(LOG_FLOOD_TRACE))
				{
					l_result.m_flood_command.push_back(make_pair(p_line, GET_TICK() - l_result.m_start_tick));
				}
				const auto l_delta = l_result.m_tick - l_result.m_start_tick;
				if (l_delta > CFlyServerConfig::g_interval_flood_command * 1000)
				{
					// ѕрошла секунда и команд пришло больше 100 (CFlyServerConfig::g_max_flood_command)
					// логируем счетчик и баним на 20 секунд данные команды (CFlyServerConfig::g_ban_flood_command)
					if (l_result.m_count > CFlyServerConfig::g_max_flood_command)  // в секунду больше чем 20
					{
						if (l_result.m_is_ban == false) // ¬ лог кидаем первую мессагу
						{
							if (BOOLSETTING(LOG_FLOOD_TRACE))
							{
								const string l_msg = "[Start flood][" + m_HubURL + "] command = " + l_flood_find.first->first
								                     + " count = " + Util::toString(l_result.m_count);
								LogManager::flood_message(l_msg);
								unsigned l_index = 0;
								for (auto i = l_result.m_flood_command.cbegin(); i != l_result.m_flood_command.cend(); ++i)
								{
									LogManager::flood_message("[DeltaTime:" + Util::toString(i->second) + "][Index = " + Util::toString(l_index) + "][Message = " + i->first + "]");
								}
								l_result.m_flood_command.clear();
							}
							l_result.m_is_ban = true;
						}
						if (l_delta > CFlyServerConfig::g_ban_flood_command * 1000) // 20 секунд данные команды в бане!
						{
							if (BOOLSETTING(LOG_FLOOD_TRACE))
							{
								const string l_msg = "[Stop flood][" + m_HubURL + "] command = " + l_flood_find.first->first +
								                     " count = " + Util::toString(l_result.m_count);
								LogManager::flood_message(l_msg);
								l_result.m_flood_command.clear();
							}
							l_result.m_is_ban = false;
							l_result.m_count = 0;
							l_result.m_start_tick = l_result.m_tick = GET_TICK();
							
						}
						return l_result.m_is_ban;
					}
					else
					{
						//  оманд прибежало мало - зачищаемс€
						l_result.m_is_ban = false;
						l_result.m_count = 0;
						l_result.m_start_tick = l_result.m_tick = GET_TICK();
						if (BOOLSETTING(LOG_FLOOD_TRACE))
						{
							l_result.m_flood_command.clear();
						}
					}
				}
			}
		}
	}
	return false;
}

// !SMT!-S
OnlineUserPtr Client::getUser(const UserPtr& aUser)
{
	// for generic client, use ClientManager, but it does not correctly handle ClientManager::me
	ClientManager::LockInstanceOnlineUsers lockedInstance; // [+] IRainman fix.
	return lockedInstance->getOnlineUserL(aUser);
}
// [+] IRainman fix.
bool Client::isMeCheck(const OnlineUserPtr& ou)
{
	if (!ou || ou->getUser() == ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks())
		return true;
	else
		return false;
}
bool Client::allowPrivateMessagefromUser(const ChatMessage& message)
{
	if (isMe(message.m_replyTo))
	{
		if (UserManager::expectPasswordFromUser(message.m_to->getUser())
#ifdef IRAINMAN_ENABLE_AUTO_BAN
		        || UploadManager::isBanReply(message.m_to->getUser()) // !SMT!-S
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
		if (UploadManager::isBanReply(message.m_replyTo->getUser())) // !SMT!-S
		{
			return false;
		}
		else
#endif
			if (FavoriteManager::isNoFavUserOrUserIgnorePrivate(message.m_replyTo->getUser()))
			{
				if (BOOLSETTING(LOG_IF_SUPPRESS_PMS))
				{
					LocalArray<char, 200> l_buf;
					_snprintf(l_buf.data(), l_buf.size(), CSTRING(LOG_IF_SUPPRESS_PMS), message.m_replyTo->getIdentity().getNick().c_str(), getHubName().c_str(), getHubUrl().c_str());
					LogManager::message(l_buf.data());
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
			fly_fire2(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.m_text);
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
			fly_fire2(ClientListener::StatusMessage(), this, STRING(IGNORED_HUB_BOT_PM) + ": " + message.m_text);
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
	else if (BOOLSETTING(PROTECT_PRIVATE) && !FavoriteManager::hasFreePM(message.m_replyTo->getUser())) // !SMT!-PSW
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
					fly_fire2(ClientListener::StatusMessage(), this, STRING(REJECTED_PRIVATE_MESSAGE_FROM) + ": " + message.m_replyTo->getIdentity().getNick());
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
		        || UploadManager::isBanReply(message.m_replyTo->getUser()) // !SMT!-S
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
		if (!p_nick.empty() && UserManager::isInIgnoreList(p_nick))
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
	_snprintf(buf.data(), 512, CSTRING(AT_HUB_YOU_HAVE_RIGHT_OPERATOR), getHubUrl().c_str());
	LogManager::message(buf.data());
}
bool  Client::isInOperatorList(const string& userName) const
{
	if (m_opChat.empty())
		return false;
	else
		return Wildcard::patternMatch(userName, m_opChat, ';', false);
}

// [~] IRainman fix.

bool Client::NmdcPartialSearch(const SearchParam& p_search_param)
{
	bool l_is_partial = false;
	if (p_search_param.m_file_type == Search::TYPE_TTH && isTTHBase64(p_search_param.m_filter)) //[+]FlylinkDC++ opt.
	{
		// TODO - унести код в отдельный метод
		PartsInfo partialInfo;
		TTHValue aTTH(p_search_param.m_filter.c_str() + 4);  //[+]FlylinkDC++ opt. //-V112
#ifdef _DEBUG
//		LogManager::message("[Try] handlePartialSearch TTH = " + aString);
#endif
		if (QueueManager::handlePartialSearch(aTTH, partialInfo)) // TODO - часто ищетс€ по ““’
		{
#ifdef _DEBUG
			LogManager::message("[OK] handlePartialSearch TTH = " + p_search_param.m_filter);
#endif
			l_is_partial = true;
			string l_ip;
			uint16_t l_port = 0;
			Util::parseIpPort(p_search_param.m_seeker, l_ip, l_port);
			dcassert(p_search_param.m_seeker == l_ip + ':' + Util::toString(l_port));
			if (l_port == 0)
			{
				dcassert(0);
				return false;
			}
			try
			{
				AdcCommand cmd(AdcCommand::CMD_PSR, AdcCommand::TYPE_UDP);
				SearchManager::toPSR(cmd, true, getMyNick(), getIpPort(), aTTH.toBase32(), partialInfo);
				Socket l_udp;
				l_udp.writeTo(Socket::resolve(l_ip), l_port, cmd.toString(ClientManager::getMyCID())); // TODO - зачем тут resolve кроме IP может быть что-то другое?
				
				COMMAND_DEBUG("[NmdcPartialSearch]" + cmd.toString(ClientManager::getMyCID()), DebugTask::CLIENT_OUT,  l_ip + ':' + Util::toString(l_port));
				
				LogManager::psr_message(
				    "[ClientManager::NmdcSearch Send UDP IP = " + l_ip +
				    " param->udpPort = " + Util::toString(l_port) +
				    " cmd = " + cmd.toString(ClientManager::getMyCID())
				);
			}
			catch (Exception& e)
			{
				LogManager::psr_message(
				    "[Partial search caught error] Error = " + e.getError() +
				    " IP = " + l_ip +
				    " param->udpPort = " + Util::toString(l_port)
				);
				
#ifdef _DEBUG
				LogManager::message("ClientManager::on(NmdcSearch, Partial search caught error = " + e.getError() + " TTH = " + p_search_param.m_filter);
				dcdebug("Partial search caught error\n");
#endif
			}
		}
	}
	return l_is_partial;
}

string Client::getCounts()
{
	char buf[128];
	buf[0] = 0;
	return string(buf, _snprintf(buf, _countof(buf), "%u/%u/%u", g_counts[COUNT_NORMAL], g_counts[COUNT_REGISTERED], g_counts[COUNT_OP]));
}

const string& Client::getCountsIndivid() const
{
	// [!] IRainman Exclusive hub, send H:1/0/0 or similar
	if (isOp())
	{
		static const string g_001 = "0/0/1";
		return g_001;
	}
	else if (isRegistered())
	{
		static const string g_010 = "0/1/0";
		return g_010;
	}
	else
	{
		static const string g_100 = "1/0/0";
		return g_100;
	}
}
void Client::getCountsIndivid(uint8_t& p_normal, uint8_t& p_registered, uint8_t& p_op) const
{
	// [!] IRainman Exclusive hub, send H:1/0/0 or similar
	p_normal = p_registered = p_op = 0;
	if (isOp())
	{
		p_op = 1;
	}
	else if (isRegistered())
	{
		p_registered = 1;
	}
	else
	{
		p_normal = 1;
	}
}
const string& Client::getRawCommand(const int aRawCommand) const
{
	switch (aRawCommand)
	{
		case 1:
			return rawOne;
		case 2:
			return rawTwo;
		case 3:
			return rawThree;
		case 4:
			return rawFour;
		case 5:
			return rawFive;
	}
	return Util::emptyString;
}

void Client::processingPassword()
{
	if (!getPassword().empty())
	{
		password(getPassword());
		fly_fire2(ClientListener::StatusMessage(), this, STRING(STORED_PASSWORD_SENT));
	}
	else
	{
		fly_fire1(ClientListener::GetPassword(), this);
	}
}

void Client::escapeParams(StringMap& sm) const
{
	for (auto i = sm.begin(); i != sm.end(); ++i)
	{
		i->second = escape(i->second);
	}
}

void Client::setSearchInterval(uint32_t aInterval)
{
	// min interval is 2 seconds in FlylinkDC
	m_searchQueue.m_interval = max(aInterval, (uint32_t)(2000)); // [!] FlylinkDC
	m_searchQueue.m_interval = min(m_searchQueue.m_interval, (uint32_t)(120000));
	dcassert(m_searchQueue.m_interval != 0);
}

string Client::getTagVersion() const
{
	string l_version = getClientVersion();
	if (!m_is_override_name)
	{
		l_version += '-';
		l_version += A_REVISION_NUM_STR;
	}
	return l_version;
}
/**
 * @file
 * $Id: Client.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
