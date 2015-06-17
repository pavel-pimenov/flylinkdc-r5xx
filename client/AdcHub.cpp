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
#include "AdcHub.h"

#include "ClientManager.h"
#include "ShareManager.h"
#include "StringTokenizer.h"
#include "ConnectionManager.h"
#include "UserCommand.h"
#include "CryptoManager.h"
#include "LogManager.h"
#include "UploadManager.h"
#include "ThrottleManager.h"

// [+] IRainman fix.
#ifdef FLYLINKDC_COLLECT_UNKNOWN_FEATURES
FastCriticalSection AdcSupports::g_debugCsUnknownAdcFeatures;
boost::unordered_map<string, string> AdcSupports::g_debugUnknownAdcFeatures;

FastCriticalSection NmdcSupports::g_debugCsUnknownNmdcConnection;
boost::unordered_set<string> NmdcSupports::g_debugUnknownNmdcConnection;

#endif // FLYLINKDC_COLLECT_UNKNOWN_FEATURES
#ifdef FLYLINKDC_COLLECT_UNKNOWN_TAG
FastCriticalSection NmdcSupports::g_debugCsUnknownNmdcTagParam;
boost::unordered_map<string, unsigned> NmdcSupports::g_debugUnknownNmdcTagParam;
#endif // FLYLINKDC_COLLECT_UNKNOWN_TAG

// [~] IRainman fix.

const string AdcSupports::CLIENT_PROTOCOL("ADC/1.0");
const string AdcSupports::SECURE_CLIENT_PROTOCOL_TEST("ADCS/0.10");
const string AdcSupports::ADCS_FEATURE("ADC0");
const string AdcSupports::TCP4_FEATURE("TCP4");
const string AdcSupports::TCP6_FEATURE("TCP6");
const string AdcSupports::UDP4_FEATURE("UDP4");
const string AdcSupports::UDP6_FEATURE("UDP6");
const string AdcSupports::NAT0_FEATURE("NAT0");
const string AdcSupports::SEGA_FEATURE("SEGA");
const string AdcSupports::BASE_SUPPORT("ADBASE");
const string AdcSupports::BAS0_SUPPORT("ADBAS0");
const string AdcSupports::TIGR_SUPPORT("ADTIGR");
const string AdcSupports::UCM0_SUPPORT("ADUCM0");
const string AdcSupports::BLO0_SUPPORT("ADBLO0");
#ifdef STRONG_USE_DHT
const string AdcSupports::DHT0_SUPPORT("ADDHT0");
#endif
const string AdcSupports::ZLIF_SUPPORT("ADZLIF");

const vector<StringList> AdcHub::searchExts;

AdcHub::AdcHub(const string& aHubURL, bool secure, bool p_is_auto_connect) : Client(aHubURL, '\n', secure, p_is_auto_connect), m_oldPassword(false), sid(0)
{
}

AdcHub::~AdcHub()
{
	clearUsers();
}
void AdcHub::getUserList(OnlineUserList& p_list) const
{
	webrtc::ReadLockScoped l(*m_cs);
	p_list.reserve(m_users.size());
	for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
	{
		if (i->first != AdcCommand::HUB_SID)
		{
			p_list.push_back(i->second);
		}
	}
}
void AdcHub::resetAntivirusInfo()
{
#ifdef FLYLINKDC_USE_ANTIVIRUS_DB
	webrtc::ReadLockScoped l(*m_cs);
	for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
	{
		i->second->getIdentity().resetAntivirusInfo();
	}
#else
	dcassert(0);
#endif
}

OnlineUserPtr AdcHub::getUser(const uint32_t aSID, const CID& aCID)
{
	OnlineUserPtr ou = findUser(aSID);
	if (ou)
		return ou;
		
	// [+] IRainman fix.
	if (aCID.isZero())
	{
		webrtc::WriteLockScoped l(*m_cs);
		ou = m_users.insert(make_pair(aSID, getHubOnlineUser().get())).first->second;
		ou->inc();
	}
	else if (ClientManager::isMe(aCID))
	{
		{
			webrtc::WriteLockScoped l(*m_cs);
			ou = m_users.insert(make_pair(aSID, getMyOnlineUser().get())).first->second;
			ou->inc();
		}
		ou->getIdentity().setSID(aSID);
		if (ou->getIdentity().isOp())
		{
			messageYouHaweRightOperatorOnThisHub();
		}
	}
	// [~] IRainman fix.
	else // User
	{
		UserPtr u = ClientManager::getUser(aCID, true);
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		u->setHubID(getHubID());
#endif
		OnlineUser* newUser = new OnlineUser(u, *this, aSID);
		webrtc::WriteLockScoped l(*m_cs);
		ou = m_users.insert(make_pair(aSID, newUser)).first->second;
		ou->inc();
	}
	
	if (aSID != AdcCommand::HUB_SID)
	{
		ClientManager::getInstance()->putOnline(ou);
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		UserManager::checkUser(ou);
#endif
	}
	return ou;
}

OnlineUserPtr AdcHub::findUser(const string& aNick) const
{
#ifdef _DEBUG
	//LogManager::message("AdcHub::findUser [slow] aNick = " + aNick);
#endif
	webrtc::ReadLockScoped l(*m_cs);
	for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
	{
		if (i->second->getIdentity().getNick() == aNick)
		{
			return i->second;
		}
	}
	return nullptr;
}

OnlineUserPtr AdcHub::findUser(const uint32_t aSID) const// [!] IRainman fix return OnlineUserPtr
{
#ifdef _DEBUG
	// LogManager::message("AdcHub::findUser aSID = " + Util::toString(aSID));
#endif
	webrtc::ReadLockScoped l(*m_cs);
	const auto& i = m_users.find(aSID);
	return i == m_users.end() ? nullptr : i->second;
}

OnlineUserPtr AdcHub::findUser(const CID& aCID) const// [!] IRainman fix return OnlineUserPtr
{
#ifdef _DEBUG
	// LogManager::message("AdcHub::findUser [slow] aCID = " + aCID.toBase32());
#endif
	webrtc::ReadLockScoped l(*m_cs);
	for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
	{
		if (i->second->getUser()->getCID() == aCID)
		{
			return i->second;
		}
	}
	return nullptr;
}

void AdcHub::putUser(const uint32_t aSID, bool p_is_disconnect)
{
	OnlineUserPtr ou = 0;
	{
		webrtc::WriteLockScoped l(*m_cs);
		const auto& i = m_users.find(aSID);
		if (i == m_users.end())
			return;
		ou = i->second;
		m_users.erase(i);
		decBytesSharedL(ou->getIdentity());
	}
	
	if (aSID != AdcCommand::HUB_SID)
	{
		ClientManager::getInstance()->putOffline(ou, p_is_disconnect);
	}
	
	fire(ClientListener::UserRemoved(), this, ou);
	ou->dec();
}

void AdcHub::clearUsers()
{
	if (ClientManager::isShutdown())
	{
		webrtc::WriteLockScoped l(*m_cs);
		for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
		{
			i->second->dec();
		}
		m_users.clear();
		clearAvailableBytesL();
	}
	else
	{
		SIDMap tmp;
		{
			webrtc::WriteLockScoped l(*m_cs);
			m_users.swap(tmp);
			clearAvailableBytesL();
		}
		
		for (auto i = tmp.cbegin(); i != tmp.cend(); ++i)
		{
			if (i->first != AdcCommand::HUB_SID)
			{
				ClientManager::getInstance()->putOffline(i->second); //TODO crash
			}
			i->second->dec();
		}
	}
}

void AdcHub::handle(AdcCommand::INF, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty())
	{
		dcassert(0);
		return;
	}
	
	OnlineUserPtr ou; // [!] IRainman fix: use OnlineUserPtr here!
	if (c.isCIDexists())
	{
#ifdef _DEBUG
		// LogManager::message("CID [+1] " + c.getParamCID().toBase32() + " Params = " +  c.getParamString(false));
#endif
		ou = findUser(c.getParamCID());
		if (ou)
		{
			if (ou->getIdentity().getSID() != c.getFrom())
			{
				// Same CID but different SID not allowed - buggy hub? [!] IRainman: yes - this is a bug in the hub - it must filter the users with the same cid not depending on the sid! This error is typically used to send spam, as it came from himself.
				string nick;
				if (!c.getParam("NI", 0, nick))
				{
					nick = "[nick unknown]";
				}
				// fix http://code.google.com/p/flylinkdc/issues/detail?id=1435
				const string l_message = ou->getIdentity().getNick() + " (" + ou->getIdentity().getSIDString() +
				                         ") has same CID {" + c.getParamCID().toBase32() + "} as " + nick + " (" + AdcCommand::fromSID(c.getFrom()) + "), ignoring.";
				fire(ClientListener::StatusMessage(), this, l_message, ClientListener::FLAG_IS_SPAM);
				
				//LogManager::ddos_message("Magic spam message filtered on hub: " + getHubUrl() + " detail:" + l_message);
				return;
			}
		}
		else
		{
			ou = getUser(c.getFrom(), c.getParamCID());
		}
	}
	else if (c.getFrom() == AdcCommand::HUB_SID)
	{
#ifdef _DEBUG
		LogManager::message("CID [-1] Params = " + c.getParamString(false));
#endif
		ou = getUser(c.getFrom(), CID());
#ifdef IRAINMAN_USE_HIDDEN_USERS
		ou->getIdentity().setHidden();
#endif
	}
	else
	{
		ou = findUser(c.getFrom());
	}
	if (!ou)
	{
		dcdebug("AdcHub::INF Unknown user / no ID\n");
		return;
	}
	
	auto& id = ou->getIdentity();
	auto& u = ou->getUser();
	PROFILE_THREAD_SCOPED_DESC("getParameters")
	for (auto i = c.getParameters().cbegin(); i != c.getParameters().cend(); ++i)
	{
		if (i->length() < 2)
		{
			dcassert(0);
			continue;
		}
		// [+] brain-ripper
		switch (*(short*)i->c_str())
		{
			case TAG('S', 'L'):
			{
				id.setSlots(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('F', 'S'):
			{
				id.setFreeSlots(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('S', 'S'):
			{
				changeBytesSharedL(id, Util::toInt64(i->c_str() + 2));
				break;
			}
			// [+] IRainman fix.
			case TAG('S', 'U'):
			{
				AdcSupports::setSupports(id, i->substr(2));
				break;
			}
			case TAG('S', 'F'):
			{
				id.setSharedFiles(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('I', '4'):
			{
				const string l_ip4 = i->substr(2);
				dcassert(!l_ip4.empty());
				id.setIp(l_ip4);
				break;
			}
			case TAG('U', '4'):
			{
				id.setUdpPort(Util::toInt(i->substr(2)));
				break;
			}
			case TAG('I', '6'):
			{
				id.setIP6(i->substr(2));
				id.setUseIP6();
				break;
			}
			case TAG('U', '6'):
			{
				id.setUdpPort(Util::toInt(i->substr(2)));
				break;
			}
			case TAG('E', 'M'):
			{
				id.setEmail(i->substr(2));
				break;
			}
			case TAG('D', 'E'):
			{
				id.setDescription(i->substr(2));
				break;
			}
			case TAG('C', 'O'):
			{
				// ignored: non official.
				break;
			}
			case TAG('I', 'D'):
			{
				// ignore, already in user.
				// TODO - хранить отдельно в виде CID ?
				break;
			}
			case TAG('D', 'S'):
			{
				id.setDownloadSpeed(Util::toUInt32(i->c_str() + 2));
				break;
			}
			case TAG('O', 'P'):
			case TAG('B', 'O'):
			{
				// ignore, use CT
				break;
			}
			// [~] IRainman fix.
			case TAG('C', 'T'):
			{
				id.setClientType(Util::toInt(i->c_str() + 2));
				break;
			}
			case TAG('U', 'S'):
			{
				id.setLimit(Util::toUInt32(i->c_str() + 2));
				break;
			}
			case TAG('H', 'N'):
			{
				id.setHubNormal(i->c_str() + 2);
				break;
			}
			case TAG('H', 'R'):
			{
				id.setHubRegister(i->c_str() + 2);
				break;
			}
			case TAG('H', 'O'):
			{
				id.setHubOperator(i->c_str() + 2);
				break;
			}
			case TAG('N', 'I'):
			{
				id.setNick(i->substr(2));
				break;
			}
			case TAG('A', 'W'): // [+] IRainman fix: away mode.
			{
				u->setFlag(User::AWAY);
				break;
			}
#ifdef _DEBUG
			case TAG('V', 'E'):
			{
				id.setStringParam("VE", i->substr(2));
				break;
			}
			case TAG('A', 'P'):
			{
				id.setStringParam("AP", i->substr(2));
				break;
			}
#endif
			default:
			{
				id.setStringParam(i->c_str(), i->substr(2));
			}
		}
	}
	if (isMe(ou)) // [!] IRainman fix.
	{
		state = STATE_NORMAL;
		setAutoReconnect(true);
		// [-] setMyIdentity(ou->getIdentity()); [-] IRainman fix.
		updateCounts(false);
		fire(ClientListener::UserUpdated(), ou); // fix https://code.google.com/p/flylinkdc/issues/detail?id=1537
	}
	else if (ou->getIdentity().isHub())
	{
		// [-] setHubIdentity(ou->getIdentity()); // [-] IRainman fix.
		// [+] FlylinkDC++
		if (SETTING(STRIP_TOPIC))
			ou->getIdentity().setDescription(Util::emptyString);
		// [~] FlylinkDC++
		fire(ClientListener::HubUpdated(), this);
	}
	else
	{
		fire(ClientListener::UserUpdated(), ou);
	}
}

void AdcHub::handle(AdcCommand::SUP, const AdcCommand& c) noexcept
{
	if (state != STATE_PROTOCOL) /** @todo SUP changes */
		return;
	bool baseOk = false;
	bool tigrOk = false;
	for (auto i = c.getParameters().cbegin(); i != c.getParameters().cend(); ++i)
	{
		if (*i == AdcSupports::BAS0_SUPPORT)
		{
			baseOk = true;
			tigrOk = true;
		}
		else if (*i == AdcSupports::BASE_SUPPORT)
		{
			baseOk = true;
		}
		else if (*i == AdcSupports::TIGR_SUPPORT)
		{
			tigrOk = true;
		}
	}
	
	if (!baseOk)
	{
		fire(ClientListener::StatusMessage(), this, "Failed to negotiate base protocol"); // TODO: translate
		disconnect(false);
		return;
	}
	else if (!tigrOk)
	{
		m_oldPassword = true;
		// Some hubs fake BASE support without TIGR support =/
		fire(ClientListener::StatusMessage(), this, "Hub probably uses an old version of ADC, please encourage the owner to upgrade"); // TODO: translate
	}
}

void AdcHub::handle(AdcCommand::SID, const AdcCommand& c) noexcept
{
	if (state != STATE_PROTOCOL)
	{
		dcdebug("Invalid state for SID\n");
		return;
	}
	
	if (c.getParameters().empty())
		return;
		
	sid = AdcCommand::toSID(c.getParam(0));
	
	state = STATE_IDENTIFY;
	info(true);
}

void AdcHub::handle(AdcCommand::MSG, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty())
		return;
		
	unique_ptr<ChatMessage> message(new ChatMessage(c.getParam(0), findUser(c.getFrom())));
	if (!message->m_from)
		return;
		
	string temp;
	
	message->thirdPerson = c.hasFlag("ME", 1);
	
	if (c.getParam("TS", 1, temp))
		message->m_timestamp = Util::toInt64(temp);
		
	if (c.getParam("PM", 1, temp))  // add PM<group-cid> as well
	{
		message->m_to = findUser(c.getTo());
		if (!message->m_to)
			return;
			
		message->m_replyTo = findUser(AdcCommand::toSID(temp));
		if (!message->m_replyTo)
			return;
			
		if (allowPrivateMessagefromUser(*message)) // [+] IRainman.
		{
			fire(ClientListener::Message(), this, message);
		}
	}
	else
	{
		if (allowChatMessagefromUser(*message, Util::emptyString)) // [+] IRainman.
		{
			fire(ClientListener::Message(), this, message);
		}
	}
}

void AdcHub::handle(AdcCommand::GPA, const AdcCommand& c) noexcept
{
	if (c.getParameters().empty())
		return;
	m_salt = c.getParam(0);
	state = STATE_VERIFY;
	
	// [!] IRainman fix.
	setRegistered(); // [+]
	processingPassword(); // [!]
	// [~] IRainman fix.
}

void AdcHub::handle(AdcCommand::QUI, const AdcCommand& c) noexcept
{
	uint32_t s = AdcCommand::toSID(c.getParam(0));
	
	OnlineUserPtr victim = findUser(s);
	if (victim)
	{
	
		string tmp;
		if (c.getParam("MS", 1, tmp))
		{
			OnlineUserPtr source = nullptr;
			string tmp2;
			if (c.getParam("ID", 1, tmp2))
			{
				dcassert(tmp2.size() == 39);
				source = findUser(AdcCommand::toSID(tmp2));
			}
			
			if (source)
			{
				tmp = victim->getIdentity().getNick() + " was kicked by " + source->getIdentity().getNick() + ": " + tmp;
			}
			else
			{
				tmp = victim->getIdentity().getNick() + " was kicked: " + tmp;
			}
			fire(ClientListener::StatusMessage(), this, tmp, ClientListener::FLAG_IS_SPAM);
		}
		
		putUser(s, c.getParam("DI", 1, tmp)); // TODO тут внутри повторно ищем юзера (OnlineUserPtr victim = findUser(s);)
	}
	
	if (s == sid)
	{
		// this QUI is directed to us
		
		string tmp;
		if (c.getParam("TL", 1, tmp))
		{
			if (tmp == "-1")
			{
				setAutoReconnect(false);
			}
			else
			{
				setAutoReconnect(true);
				setReconnDelay(Util::toUInt32(tmp));
			}
		}
		if (!victim && c.getParam("MS", 1, tmp))
		{
			fire(ClientListener::StatusMessage(), this, tmp);
		}
		if (c.getParam("RD", 1, tmp))
		{
			fire(ClientListener::Redirect(), this, tmp);
		}
	}
}

void AdcHub::handle(AdcCommand::CTM, const AdcCommand& c) noexcept
{
	OnlineUserPtr ou = findUser(c.getFrom()); // [!] IRainman fix.
	if (!ou || isMe(ou)) // [!] IRainman fix.
		return;
	if (c.getParameters().size() < 3)
		return;
		
	const string& protocol = c.getParam(0);
	const string& port = c.getParam(1);
	const string& token = c.getParam(2);
	
	bool secure = false;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		// Nothing special
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->TLSOk())
	{
		secure = true;
	}
	else
	{
		unknownProtocol(c.getFrom(), protocol, token);
		return;
	}
	
	if (!ou->getIdentity().isTcpActive())
	{
		send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC, "IP unknown", AdcCommand::TYPE_DIRECT).setTo(c.getFrom()));
		return;
	}
	
	ConnectionManager::getInstance()->adcConnect(*ou, static_cast<uint16_t>(Util::toInt(port)), token, secure);
}


void AdcHub::handle(AdcCommand::ZON, const AdcCommand& /*c*/) noexcept
{
	try
	{
		m_client_sock->setMode(BufferedSocket::MODE_ZPIPE);
		dcdebug("ZLIF mode enabled on hub: %s\n", getHubUrlAndIP().c_str());
	}
	catch (const Exception& e)
	{
		dcdebug("AdcHub::handleZON failed with error: %s\n", e.getError().c_str());
	}
}

void AdcHub::handle(AdcCommand::RCM, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 2)
	{
		return;
	}
	
	OnlineUserPtr ou = findUser(c.getFrom());
	if (!ou || isMe(ou)) // [!] IRainman fix.
		return;
		
	const string& protocol = c.getParam(0);
	const string& token = c.getParam(1);
	
	bool secure;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		secure = false;
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->TLSOk())
	{
		secure = true;
	}
	else
	{
		unknownProtocol(c.getFrom(), protocol, token);
		return;
	}
	
	if (isActive())
	{
		connect_secure(*ou, token, secure);
		return;
	}
	
	if (!BOOLSETTING(ALLOW_NAT_TRAVERSAL) || !ou->getUser()->isSet(User::NAT0))
		return;
		
	// Attempt to traverse NATs and/or firewalls with TCP.
	// If they respond with their own, symmetric, RNT command, both
	// clients call ConnectionManager::adcConnect.
	send(AdcCommand(AdcCommand::CMD_NAT, ou->getIdentity().getSID(), AdcCommand::TYPE_DIRECT).
	addParam(protocol).addParam(Util::toString(m_client_sock->getLocalPort())).addParam(token));
}

void AdcHub::handle(AdcCommand::CMD, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 1)
		return;
	const string& name = c.getParam(0);
	bool rem = c.hasFlag("RM", 1);
	if (rem)
	{
		fire(ClientListener::HubUserCommand(), this, (int)UserCommand::TYPE_REMOVE, 0, name, Util::emptyString);
		return;
	}
	bool sep = c.hasFlag("SP", 1);
	string sctx;
	if (!c.getParam("CT", 1, sctx))
		return;
	int ctx = Util::toInt(sctx);
	if (ctx <= 0)
		return;
	if (sep)
	{
		fire(ClientListener::HubUserCommand(), this, (int)UserCommand::TYPE_SEPARATOR, ctx, name, Util::emptyString);
		return;
	}
	bool once = c.hasFlag("CO", 1);
	string txt;
	if (!c.getParam("TT", 1, txt))
		return;
	fire(ClientListener::HubUserCommand(), this, (int)(once ? UserCommand::TYPE_RAW_ONCE : UserCommand::TYPE_RAW), ctx, name, txt);
}

void AdcHub::sendUDP(const AdcCommand& cmd) noexcept
{
	string command;
	string l_ip;
	uint16_t l_port;
	{
		webrtc::ReadLockScoped l(*m_cs);
		const auto& i = m_users.find(cmd.getTo());
		if (i == m_users.end())
		{
			dcdebug("AdcHub::sendUDP: invalid user\n");
			return;
		}
		OnlineUser* ou = i->second; // [!] IRainman fix: add pointer
		if (!ou->getIdentity().isUdpActive())
		{
			return;
		}
		l_ip = ou->getIdentity().getIpAsString();
		l_port = ou->getIdentity().getUdpPort();
		command = cmd.toString(ou->getUser()->getCID());
	}
	try
	{
		m_udp.writeTo(l_ip, l_port, command);
		const string l_sr = command;
#ifdef FLYLINKDC_USE_COLLECT_STAT
		string l_tth;
		const auto l_tth_pos = l_sr.find("TTH:");
		if (l_tth_pos != string::npos)
			l_tth = l_sr.substr(l_tth_pos + 4, 39); // getHubUrl()
		CFlylinkDBManager::getInstance()->push_event_statistic("$SR", "UDP-write-adc", l_sr, ip, Util::toString(port), "", l_tth);
#endif
		COMMAND_DEBUG("[ADC UDP][" + l_ip + ':' + Util::toString(l_port) + "] " + command, DebugTask::CLIENT_OUT, getIpPort());
		
	}
	catch (const SocketException& e)
	{
		dcdebug("AdcHub::sendUDP: write failed: %s\n", e.getError().c_str());
		m_udp.close();
	}
}

void AdcHub::handle(AdcCommand::STA, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 2)
		return;
		
	OnlineUserPtr ou = c.getFrom() == AdcCommand::HUB_SID ? getUser(c.getFrom(), CID()) : findUser(c.getFrom());
	if (!ou)
		return;
		
	//int severity = Util::toInt(c.getParam(0).substr(0, 1));
	if (c.getParam(0).size() != 3)
	{
		return;
	}
	
	switch (Util::toInt(c.getParam(0).c_str() + 1))
	{
	
		case AdcCommand::ERROR_BAD_PASSWORD:
		{
			setPassword(Util::emptyString);
			break;
		}
		
		case AdcCommand::ERROR_COMMAND_ACCESS:
		{
			string tmp;
			if (c.getParam("FC", 1, tmp) && tmp.size() == 4)
				forbiddenCommands.insert(AdcCommand::toFourCC(tmp.c_str()));
				
			break;
		}
		
		case AdcCommand::ERROR_PROTOCOL_UNSUPPORTED:
		{
			string tmp;
			if (c.getParam("PR", 1, tmp))
			{
				if (tmp == AdcSupports::CLIENT_PROTOCOL)
				{
					ou->getUser()->setFlag(User::NO_ADC_1_0_PROTOCOL);
				}
				else if (tmp == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST)
				{
					ou->getUser()->setFlag(User::NO_ADCS_0_10_PROTOCOL);
					ou->getUser()->unsetFlag(User::ADCS);
				}
				// Try again...
				ConnectionManager::getInstance()->force(ou->getUser());
			}
			return;
		}
	}
	unique_ptr<ChatMessage> message(new ChatMessage(c.getParam(1), ou));
	fire(ClientListener::Message(), this, message);
}

void AdcHub::handle(AdcCommand::SCH, const AdcCommand& c) noexcept
{
	OnlineUserPtr ou = findUser(c.getFrom()); // [!] IRainman fix.
	if (!ou)
	{
		dcdebug("Invalid user in AdcHub::onSCH\n");
		return;
	}
	
	fire(ClientListener::AdcSearch(), this, c, ou->getUser()->getCID());
}

void AdcHub::handle(AdcCommand::RES, const AdcCommand& c) noexcept
{
	OnlineUserPtr ou = findUser(c.getFrom()); // [!] IRainman fix.
	if (!ou)
	{
		dcdebug("Invalid user in AdcHub::onRES\n");
		return;
	}
	SearchManager::getInstance()->onRES(c, ou->getUser(), boost::asio::ip::address_v4());
}

void AdcHub::handle(AdcCommand::PSR, const AdcCommand& c) noexcept
{
	OnlineUserPtr ou = findUser(c.getFrom()); // [!] IRainman fix.
	if (!ou)
	{
		dcdebug("Invalid user in AdcHub::onPSR\n");
		LogManager::psr_message("Invalid user in AdcHub::onPSR = " + c.toString(c.getFrom()));
		return;
	}
	SearchManager::getInstance()->onPSR(c, ou->getUser(), boost::asio::ip::address_v4());
}

void AdcHub::handle(AdcCommand::GET, const AdcCommand& c) noexcept
{
	if (c.getParameters().size() < 5)
	{
		if (!c.getParameters().empty())
		{
			if (c.getParam(0) == "blom")
			{
				send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC,
				"Too few parameters for blom", AdcCommand::TYPE_HUB));
			}
			else
			{
				send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC,
				                "Unknown transfer type", AdcCommand::TYPE_HUB));
			}
		}
		else
		{
			send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_GENERIC,
			                "Too few parameters for GET", AdcCommand::TYPE_HUB));
		}
		return;
	}
	
	const string& type = c.getParam(0);
	string sk, sh;
	if (type == "blom" && c.getParam("BK", 4, sk) && c.getParam("BH", 4, sh))
	{
		ByteVector v;
		size_t m = Util::toUInt32(c.getParam(3)) * 8;
		size_t k = Util::toUInt32(sk);
		size_t h = Util::toUInt32(sh);
		
		if (k > 8 || k < 1)
		{
			send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC,
			                "Unsupported k", AdcCommand::TYPE_HUB));
			return;
		}
		if (h > 64 || h < 1)
		{
			send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC,
			                "Unsupported h", AdcCommand::TYPE_HUB));
			return;
		}
		
		const size_t n = ShareManager::getSharedFiles();
		
		// When h >= 32, m can't go above 2^h anyway since it's stored in a size_t.
		if (m > (static_cast<size_t>(5 * Util::roundUp((int64_t)(n * k / log(2.)), (int64_t)64))) || (h < 32 && m > static_cast<size_t>(1U << h)))
		{
			send(AdcCommand(AdcCommand::SEV_FATAL, AdcCommand::ERROR_TRANSFER_GENERIC,
			                "Unsupported m", AdcCommand::TYPE_HUB));
			return;
		}
		if (m > 0)  //[+] http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2282
		{
			ShareManager::getInstance()->getBloom(v, k, m, h);
		}
		AdcCommand cmd(AdcCommand::CMD_SND, AdcCommand::TYPE_HUB);
		cmd.addParam(c.getParam(0));
		cmd.addParam(c.getParam(1));
		cmd.addParam(c.getParam(2));
		cmd.addParam(c.getParam(3));
		cmd.addParam(c.getParam(4));
		send(cmd);
		if (m > 0 && !v.empty())  //[+] http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2282
		{
			send((char*)&v[0], v.size());
		}
	}
}

void AdcHub::handle(AdcCommand::NAT, const AdcCommand& c) noexcept
{
	if (!BOOLSETTING(ALLOW_NAT_TRAVERSAL))
		return;
		
	OnlineUserPtr ou = findUser(c.getFrom()); // [!] IRainman fix.
	if (!ou || isMe(ou) || c.getParameters().size() < 3) // [!] IRainman fix.
		return;
		
	const string& protocol = c.getParam(0);
	const string& port = c.getParam(1);
	const string& token = c.getParam(2);
	
	// bool secure = secureAvail(c.getFrom(), protocol, token);
	bool secure = false;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		// Nothing special
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->TLSOk())
	{
		secure = true;
	}
	else
	{
		unknownProtocol(c.getFrom(), protocol, token);
		return;
	}
	
	// Trigger connection attempt sequence locally ...
	dcdebug("triggering connecting attempt in NAT: remote port = %s, local IP = %s, local port = %d\n", port.c_str(), m_client_sock->getLocalIp().c_str(), m_client_sock->getLocalPort());
	ConnectionManager::getInstance()->adcConnect(*ou, static_cast<uint16_t>(Util::toInt(port)), m_client_sock->getLocalPort(), BufferedSocket::NAT_CLIENT, token, secure);
	
	// ... and signal other client to do likewise.
	send(AdcCommand(AdcCommand::CMD_RNT, ou->getIdentity().getSID(), AdcCommand::TYPE_DIRECT).addParam(protocol).
	addParam(Util::toString(m_client_sock->getLocalPort())).addParam(token));
}

void AdcHub::handle(AdcCommand::RNT, const AdcCommand& c) noexcept
{
	// Sent request for NAT traversal cooperation, which
	// was acknowledged (with requisite local port information).
	
	if (!BOOLSETTING(ALLOW_NAT_TRAVERSAL))
		return;
		
	OnlineUserPtr ou = findUser(c.getFrom()); // [!] IRainman fix.
	if (!ou || isMe(ou) || c.getParameters().size() < 3)// [!] IRainman fix.
		return;
		
	const string& protocol = c.getParam(0);
	const string& port = c.getParam(1);
	const string& token = c.getParam(2);
	
	bool secure = false;
	if (protocol == AdcSupports::CLIENT_PROTOCOL)
	{
		// Nothing special
	}
	else if (protocol == AdcSupports::SECURE_CLIENT_PROTOCOL_TEST && CryptoManager::getInstance()->TLSOk())
	{
		secure = true;
	}
	else
	{
		unknownProtocol(c.getFrom(), protocol, token);
		return;
	}
	
	// Trigger connection attempt sequence locally
	dcdebug("triggering connecting attempt in RNT: remote port = %s, local IP = %s, local port = %d\n", port.c_str(), m_client_sock->getLocalIp().c_str(), m_client_sock->getLocalPort());
	ConnectionManager::getInstance()->adcConnect(*ou, static_cast<uint16_t>(Util::toInt(port)), m_client_sock->getLocalPort(), BufferedSocket::NAT_SERVER, token, secure);
}

void AdcHub::handle(AdcCommand::ZOF, const AdcCommand& c) noexcept
{
	try {
		m_client_sock->setMode(BufferedSocket::MODE_LINE);
	}
	catch (const Exception& e)
	{
		dcdebug("AdcHub::handleZOF failed with error: %s\n", e.getError().c_str());
	}
}

void AdcHub::connect(const OnlineUser& user, const string& token, bool p_is_force_passive)
{
	connect_secure(user, token, CryptoManager::getInstance()->TLSOk() && user.getUser()->isSet(User::ADCS));
}

void AdcHub::connect_secure(const OnlineUser& user, const string& token, bool secure)
{
	if (state != STATE_NORMAL)
		return;
		
	const string* proto;
	if (secure)
	{
		if (user.getUser()->isSet(User::NO_ADCS_0_10_PROTOCOL))
		{
			/// @todo log
			return;
		}
		proto = &AdcSupports::SECURE_CLIENT_PROTOCOL_TEST;
	}
	else
	{
		if (user.getUser()->isSet(User::NO_ADC_1_0_PROTOCOL))
		{
			/// @todo log
			return;
		}
		proto = &AdcSupports::CLIENT_PROTOCOL;
	}
	// DC++ TODO ConnectionManager::getInstance()->m_tokens.addToken(token);
	if (isActive())
	{
		uint16_t port = secure ? ConnectionManager::getInstance()->getSecurePort() : ConnectionManager::getInstance()->getPort();
		if (port == 0)
		{
			// Oops?
			LogManager::message(STRING(NOT_LISTENING));
			return;
		}
		send(AdcCommand(AdcCommand::CMD_CTM, user.getIdentity().getSID(), AdcCommand::TYPE_DIRECT).addParam(*proto).addParam(Util::toString(port)).addParam(token));
		ConnectionManager::g_ConnToMeCount++;
	}
	else
	{
		send(AdcCommand(AdcCommand::CMD_RCM, user.getIdentity().getSID(), AdcCommand::TYPE_DIRECT).addParam(*proto).addParam(token));
	}
}

void AdcHub::hubMessage(const string& aMessage, bool thirdPerson)
{
	if (state != STATE_NORMAL)
		return;
	AdcCommand cmd(AdcCommand::CMD_MSG, AdcCommand::TYPE_BROADCAST);
	cmd.addParam(aMessage);
	if (thirdPerson)
		cmd.addParam("ME", "1");
	send(cmd);
}

void AdcHub::privateMessage(const OnlineUserPtr& user, const string& aMessage, bool thirdPerson)
{
	if (state != STATE_NORMAL)
		return;
	AdcCommand cmd(AdcCommand::CMD_MSG, user->getIdentity().getSID(), AdcCommand::TYPE_ECHO);
	cmd.addParam(aMessage);
	if (thirdPerson)
	{
		cmd.addParam("ME", "1");
	}
	cmd.addParam("PM", getMySID());
	send(cmd);
}

void AdcHub::sendUserCmd(const UserCommand& command, const StringMap& params)
{
	if (state != STATE_NORMAL)
		return;
	string cmd = Util::formatParams(command.getCommand(), params, false);
	if (command.isChat())
	{
		if (command.getTo().empty())
		{
			hubMessage(cmd);
		}
		else
		{
			const string& to = command.getTo();
			webrtc::ReadLockScoped l(*m_cs);
			for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
			{
				if (i->second->getIdentity().getNick() == to)
				{
					privateMessage(i->second, cmd);
					return;
				}
			}
		}
	}
	else
	{
		send(cmd);
	}
}

const vector<StringList>& AdcHub::getSearchExts()
{
	if (!searchExts.empty())
		return searchExts;
		
	// the list is always immutable except for this function where it is initially being filled.
	auto& xSearchExts = const_cast<vector<StringList>&>(searchExts);
	
	xSearchExts.resize(6);
	
	/// @todo simplify this as searchExts[0] = { "mp3", "etc" } when VC++ supports initializer lists
	
	// these extensions *must* be sorted alphabetically!
	// TODO - изменить этот код
	{
		StringList& l = xSearchExts[0];
		l.push_back("ape");
		l.push_back("flac");
		l.push_back("m4a");
		l.push_back("mid");
		l.push_back("mp3");
		l.push_back("mpc");
		l.push_back("ogg");
		l.push_back("ra");
		l.push_back("wav");
		l.push_back("wma");
	}
	
	{
		StringList& l = xSearchExts[1];
		l.push_back("7z");
		l.push_back("ace");
		l.push_back("arj");
		l.push_back("bz2");
		l.push_back("gz");
		l.push_back("lha");
		l.push_back("lzh");
		l.push_back("rar");
		l.push_back("tar");
		l.push_back("z");
		l.push_back("zip");
	}
	
	{
		StringList& l = xSearchExts[2];
		l.push_back("doc");
		l.push_back("docx");
		l.push_back("htm");
		l.push_back("html");
		l.push_back("nfo");
		l.push_back("odf");
		l.push_back("odp");
		l.push_back("ods");
		l.push_back("odt");
		l.push_back("pdf");
		l.push_back("ppt");
		l.push_back("pptx");
		l.push_back("rtf");
		l.push_back("txt");
		l.push_back("xls");
		l.push_back("xlsx");
		l.push_back("xml");
		l.push_back("xps");
	}
	
	{
		StringList& l = xSearchExts[3];
		l.push_back("app");
		l.push_back("bat");
		l.push_back("cmd");
		l.push_back("com");
		l.push_back("dll");
		l.push_back("exe");
		l.push_back("jar");
		l.push_back("msi");
		l.push_back("ps1");
		l.push_back("vbs");
		l.push_back("wsf");
	}
	
	{
		StringList& l = xSearchExts[4];
		l.push_back("bmp");
		l.push_back("cdr");
		l.push_back("eps");
		l.push_back("gif");
		l.push_back("ico");
		l.push_back("img");
		l.push_back("jpeg");
		l.push_back("jpg");
		l.push_back("png");
		l.push_back("ps");
		l.push_back("psd");
		l.push_back("sfw");
		l.push_back("tga");
		l.push_back("tif");
		l.push_back("webp");
	}
	
	{
		StringList& l = xSearchExts[5];
		l.push_back("3gp");
		l.push_back("asf");
		l.push_back("asx");
		l.push_back("avi");
		l.push_back("divx");
		l.push_back("flv");
		l.push_back("mkv");
		l.push_back("mov");
		l.push_back("mp4");
		l.push_back("mpeg");
		l.push_back("mpg");
		l.push_back("ogm");
		l.push_back("pxp");
		l.push_back("qt");
		l.push_back("rm");
		l.push_back("rmvb");
		l.push_back("swf");
		l.push_back("vob");
		l.push_back("webm");
		l.push_back("wmv");
	}
	
	return searchExts;
}

StringList AdcHub::parseSearchExts(int flag)
{
	StringList ret;
	const auto& l_search = getSearchExts();
	for (auto i = l_search.cbegin(), iend = l_search.cend(); i != iend; ++i)
	{
		if (flag & (1 << (i - l_search.cbegin())))
		{
			ret.insert(ret.begin(), i->begin(), i->end());
		}
	}
	return ret;
}
void AdcHub::search_token(const SearchParamToken& p_search_param)
{
	if (state != STATE_NORMAL
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	        || getHideShare()//[+]FlylinkDC HideShare mode
#endif
	   )
		return;
		
	AdcCommand cmd(AdcCommand::CMD_SCH, AdcCommand::TYPE_BROADCAST);
	
	//dcassert(aToken);
	cmd.addParam("TO", Util::toString(p_search_param.m_token));
	
	if (p_search_param.m_file_type == Search::TYPE_TTH)
	{
		cmd.addParam("TR", p_search_param.m_filter);
	}
	else
	{
		if (p_search_param.m_size_mode == Search::SIZE_ATLEAST)
		{
			cmd.addParam("GE", Util::toString(p_search_param.m_size));
		}
		else if (p_search_param.m_size_mode == Search::SIZE_ATMOST)
		{
			cmd.addParam("LE", Util::toString(p_search_param.m_size));
		}
		
		const StringTokenizer<string> st(p_search_param.m_filter, ' ');
		for (auto i = st.getTokens().cbegin(); i != st.getTokens().cend(); ++i)
		{
			cmd.addParam("AN", *i);
		}
		
		if (p_search_param.m_file_type == Search::TYPE_DIRECTORY)
		{
			cmd.addParam("TY", "2");
		}
		
		if (p_search_param.m_ext_list.size() > 2)
		{
			StringList exts = p_search_param.m_ext_list;
			sort(exts.begin(), exts.end());
			
			uint8_t gr = 0;
			StringList rx;
			
			const auto& searchExts = getSearchExts();
			for (auto i = searchExts.cbegin(), iend = searchExts.cend(); i != iend; ++i)
			{
				const StringList& def = *i;
				
				// gather the exts not present in any of the lists
				StringList temp(def.size() + exts.size());
				temp = StringList(temp.begin(), set_symmetric_difference(def.begin(), def.end(),
				                                                         exts.begin(), exts.end(), temp.begin()));
				                                                         
				// figure out whether the remaining exts have to be added or removed from the set
				StringList rx_;
				bool ok = true;
				for (auto diff = temp.cbegin(); diff != temp.cend();)
				{
					if (find(def.cbegin(), def.cend(), *diff) == def.cend())
					{
						++diff; // will be added further below as an "EX"
					}
					else
					{
						if (rx_.size() == 2)
						{
							ok = false;
							break;
						}
						rx_.push_back(*diff);
						diff = temp.erase(diff);
					}
				}
				if (!ok) // too many "RX"s necessary - disregard this group
					continue;
					
				// let's include this group!
				gr += 1 << (i - searchExts.cbegin());
				
				exts = temp; // the exts to still add (that were not defined in the group)
				
				rx.insert(rx.begin(), rx_.begin(), rx_.end());
				
				if (exts.size() <= 2)
					break;
				// keep looping to see if there are more exts that can be grouped
			}
			
			if (gr)
			{
				// some extensions can be grouped; let's send a command with grouped exts.
				AdcCommand c_gr(AdcCommand::CMD_SCH, AdcCommand::TYPE_FEATURE);
				c_gr.setFeatures('+' + AdcSupports::SEGA_FEATURE);
				
				const auto& params = cmd.getParameters();
				for (auto i = params.cbegin(), iend = params.cend(); i != iend; ++i)
					c_gr.addParam(*i);
					
				for (auto i = exts.cbegin(), iend = exts.cend(); i != iend; ++i)
					c_gr.addParam("EX", *i);
				c_gr.addParam("GR", Util::toString(gr));
				for (auto i = rx.cbegin(), iend = rx.cend(); i != iend; ++i)
					c_gr.addParam("RX", *i);
					
				sendSearch(c_gr);
				
				// make sure users with the feature don't receive the search twice.
				cmd.setType(AdcCommand::TYPE_FEATURE);
				cmd.setFeatures('-' + AdcSupports::SEGA_FEATURE);
			}
		}
		
		for (auto i = p_search_param.m_ext_list.cbegin(), iend = p_search_param.m_ext_list.cend(); i != iend; ++i)
		{
			cmd.addParam("EX", *i);
		}
	}
	// TODO - залогировать обратный IP в Adc
	// const string l_debug_string =  "[" + (isActive() ? string("Active"): string("Passive")) + " search][Client:" + getHubUrl() + "][Command:" + l_search_command + " ]";
	// dcdebug("[AdcHub::search] %s \r\n", l_debug_string.c_str());
	// TODO g_last_search_string = l_debug_string;
	sendSearch(cmd);
}

void AdcHub::sendSearch(AdcCommand& c)
{
	if (isActive()) // && (SETTING(OUTGOING_CONNECTIONS) != SettingsManager::OUTGOING_SOCKS5))
	{
		send(c);
	}
	else
	{
		c.setType(AdcCommand::TYPE_FEATURE);
		string features = c.getFeatures();
		if (BOOLSETTING(ALLOW_NAT_TRAVERSAL))
		{
			c.setFeatures(features + '+' + AdcSupports::TCP4_FEATURE + '-' + AdcSupports::NAT0_FEATURE);
			send(c);
			c.setFeatures(features + "+" + AdcSupports::NAT0_FEATURE);
		}
		else
		{
			c.setFeatures(features + "+" + AdcSupports::TCP4_FEATURE);
		}
		send(c);
	}
}

void AdcHub::password(const string& pwd)
{
	if (state != STATE_VERIFY)
		return;
	if (!m_salt.empty())
	{
		size_t saltBytes = m_salt.size() * 5 / 8;
		std::unique_ptr<uint8_t[]> buf(new uint8_t[saltBytes]);
		Encoder::fromBase32(m_salt.c_str(), &buf[0], saltBytes);
		TigerHash th;
		if (m_oldPassword)
		{
			const CID cid = getMyIdentity().getUser()->getCID();
			th.update(cid.data(), CID::SIZE);
		}
		th.update(pwd.data(), pwd.length());
		th.update(&buf[0], saltBytes);
		send(AdcCommand(AdcCommand::CMD_PAS, AdcCommand::TYPE_HUB).addParam(Encoder::toBase32(th.finalize(), TigerHash::BYTES)));
		m_salt.clear();
	}
}

void AdcHub::addParam(AdcCommand& c, const string& var, const string& value)
{
	FastLock l(m_info_cs);
	const auto& i = m_lastInfoMap.find(var);
	
	if (i != m_lastInfoMap.end())
	{
		if (i->second != value)
		{
			if (value.empty())
			{
				m_lastInfoMap.erase(i);
			}
			else
			{
				i->second = value;
			}
			c.addParam(var, value);
		}
	}
	else if (!value.empty())
	{
		m_lastInfoMap.insert(make_pair(var, value));
		c.addParam(var, value);
	}
}

bool AdcHub::resendMyINFO(bool p_is_force_passive)
{
	return false;
}

void AdcHub::info(bool p_force)
{
	if (state != STATE_IDENTIFY && state != STATE_NORMAL)
		return;
		
	reloadSettings(false);
	
	AdcCommand c(AdcCommand::CMD_INF, AdcCommand::TYPE_BROADCAST);
	c.getParameters().reserve(20);
	
	if (state == STATE_NORMAL)
	{
		updateCounts(false);
	}
	
	addParam(c, "ID", ClientManager::getMyCID().toBase32()); // [!] IRainman fix.
	addParam(c, "PD", ClientManager::getMyPID().toBase32()); // [!] IRainman fix.
	addParam(c, "NI", getMyNick());
	addParam(c, "DE", getCurrentDescription());
	addParam(c, "SL", Util::toString(UploadManager::getSlots()));
	addParam(c, "FS", Util::toString(UploadManager::getFreeSlots()));
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	if (getHideShare())
	{
		addParam(c, "SS", "0"); // [!] Flylink++ HideShare mode
		addParam(c, "SF", "0"); // [!] Flylink HideShare mode
	}
	else
#endif
	{
		addParam(c, "SS", ShareManager::getInstance()->getShareSizeString()); // [!] Flylink++ HideShare mode
		addParam(c, "SF", Util::toString(ShareManager::getSharedFiles())); // [!] Flylink++ HideShare mode
	}
	
	
	addParam(c, "EM", SETTING(EMAIL));
	// [!] Flylink++ Exclusive hub mode
	const FavoriteHubEntry *fhe = FavoriteManager::getFavoriteHubEntry(getHubUrl());
	if (fhe && fhe->getExclusiveHub())
	{
		uint8_t l_normal, l_registered, l_op;
		getCountsIndivid(l_normal, l_registered, l_op);
		addParam(c, "HN", Util::toString(l_normal));
		addParam(c, "HR", Util::toString(l_registered));
		addParam(c, "HO", Util::toString(l_op));
	}
	else
	{
		addParam(c, "HN", Util::toString(g_counts[COUNT_NORMAL]));
		addParam(c, "HR", Util::toString(g_counts[COUNT_REGISTERED]));
		addParam(c, "HO", Util::toString(g_counts[COUNT_OP]));
	}
	// [~] Flylink++ Exclusive hub mode
	// [!] IRainman mimicry function
	addParam(c, "AP", getClientName());
	
	addParam(c, "VE", getTagVersion());
	// [~] IRainman mimicry function
	addParam(c, "AW", Util::getAway() ? "1" : Util::emptyString);
	
	size_t limit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getDownloadLimitInBytes() : 0;// [!] IRainman SpeedLimiter
	if (limit > 0)
	{
		addParam(c, "DS", Util::toString(limit));
	}
	
	limit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getUploadLimitInBytes() : 0;// [!] IRainman SpeedLimiter
	if (limit > 0)
	{
		addParam(c, "US", Util::toString(limit));
	}
	else
	{
		limit = Util::toInt64(SETTING(UPLOAD_SPEED)) * 1024 * 1024 / 8;
		addParam(c, "US", Util::toString(limit));
	}
	
	addParam(c, "LC", Util::getIETFLang()); // http://adc.sourceforge.net/ADC-EXT.html#_lc_locale_specification
	
	string su(AdcSupports::SEGA_FEATURE);
	
	if (CryptoManager::getInstance()->TLSOk())
	{
		su += "," + AdcSupports::ADCS_FEATURE;
		const auto &kp = CryptoManager::getInstance()->getKeyprint();
		addParam(c, "KP", "SHA256/" + Encoder::toBase32(&kp[0], kp.size()));
	}
	if (isActive() || BOOLSETTING(ALLOW_NAT_TRAVERSAL))
	{
		if (getMyIdentity().isIPValid())
		{
			const string& myUserIp = getMyIdentity().getIpAsString();
			addParam(c, "I4", myUserIp);
		}
		else if (!getFavIp().empty())
		{
			addParam(c, "I4", getFavIp());
		}
		else if (!SETTING(EXTERNAL_IP).empty())
		{
			addParam(c, "I4", Socket::resolve(SETTING(EXTERNAL_IP)));
		}
		else
		{
			addParam(c, "I4", "0.0.0.0");
		}
	}
	
	if (isActive())
	{
		addParam(c, "U4", SearchManager::getSearchPort());
		su += "," + AdcSupports::TCP4_FEATURE;
		su += "," + AdcSupports::UDP4_FEATURE;
	}
	else
	{
		if (BOOLSETTING(ALLOW_NAT_TRAVERSAL))
		{
			su += "," + AdcSupports::NAT0_FEATURE;
		}
		else
		{
			addParam(c, "I4", "");
		}
		addParam(c, "U4", "");
	}
	
	addParam(c, "SU", su);
	
	if (!c.getParameters().empty())
	{
		send(c);
	}
}

void AdcHub::refreshUserList(bool)
{
	OnlineUserList v;
	{
		// [!] IRainman fix potential deadlock.
		webrtc::ReadLockScoped l(*m_cs);
		for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
		{
			if (i->first != AdcCommand::HUB_SID)
			{
				v.push_back(i->second);
			}
		}
	}
	fire_user_updated(v);
}

void AdcHub::checkNick(string& aNick)
{
	for (size_t i = 0; i < aNick.size(); ++i)
	{
		if (static_cast<uint8_t>(aNick[i]) <= 32)
		{
			aNick[i] = '_';
		}
	}
}

void AdcHub::send(const AdcCommand& cmd)
{
	if (forbiddenCommands.find(AdcCommand::toFourCC(cmd.getFourCC().c_str())) == forbiddenCommands.end())
	{
		if (cmd.getType() == AdcCommand::TYPE_UDP)
			sendUDP(cmd);
		send(cmd.toString(sid));
	}
}

void AdcHub::unknownProtocol(uint32_t target, const string& protocol, const string& token)
{
	AdcCommand cmd(AdcCommand::SEV_FATAL, AdcCommand::ERROR_PROTOCOL_UNSUPPORTED, "Protocol unknown", AdcCommand::TYPE_DIRECT);
	cmd.setTo(target);
	cmd.addParam("PR", protocol);
	cmd.addParam("TO", token);
	
	send(cmd);
}

void AdcHub::on(Connected c) noexcept
{
	Client::on(c);
	set_all_my_info_loaded(); // TODO - разобраться и перехватить факт окончания передачи всех юзеров.
	if (state != STATE_PROTOCOL)
	{
		return;
	}
	
	{
		FastLock l(m_info_cs);
		m_lastInfoMap.clear();
	}
	sid = 0;
	forbiddenCommands.clear();
	
	AdcCommand cmd(AdcCommand::CMD_SUP, AdcCommand::TYPE_HUB);
	cmd.addParam(AdcSupports::BAS0_SUPPORT).addParam(AdcSupports::BASE_SUPPORT).addParam(AdcSupports::TIGR_SUPPORT);
	
	if (BOOLSETTING(HUB_USER_COMMANDS))
	{
		cmd.addParam(AdcSupports::UCM0_SUPPORT);
	}
	if (BOOLSETTING(SEND_BLOOM))
	{
		cmd.addParam(AdcSupports::BLO0_SUPPORT);
	}
#ifdef STRONG_USE_DHT
	if (BOOLSETTING(USE_DHT))
	{
		cmd.addParam(AdcSupports::DHT0_SUPPORT);
	}
#endif
	
	cmd.addParam(AdcSupports::ZLIF_SUPPORT);
	
	send(cmd);
}

void AdcHub::on(Line l, const string& aLine) noexcept
{
	if (!ClientManager::isShutdown())
	{
		Client::on(l, aLine);
		
		if (!Text::validateUtf8(aLine))
		{
			// @todo report to user?
			return;
		}
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		if (BOOLSETTING(ADC_DEBUG))
		{
			fire(ClientListener::StatusMessage(), this, "<ADC>" + aLine + "</ADC>");
		}
#endif
		dispatch(aLine);
	}
}

void AdcHub::on(Failed f, const string& aLine) noexcept
{
	clearUsers();
	Client::on(f, aLine);
}



/**
 * @file
 * $Id: AdcHub.cpp 573 2011-08-04 22:33:45Z bigmuscle $
 */
