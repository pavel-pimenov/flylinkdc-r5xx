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
#include "NmdcHub.h"
#include "ShareManager.h"
#include "CryptoManager.h"
#include "UserCommand.h"
#include "DebugManager.h"
#include "QueueManager.h"
#include "ThrottleManager.h"
#include "StringTokenizer.h"

NmdcHub::NmdcHub(const string& aHubURL, bool secure) : Client(aHubURL, '|', false), m_supportFlags(0),
#ifdef RIP_USE_CONNECTION_AUTODETECT
	m_bAutodetectionPending(true),
	m_iRequestCount(0),
#endif
	m_bLastMyInfoCommand(DIDNT_GET_YET_FIRST_MYINFO),
	lastbytesshared(0),
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	m_hubSupportsSlots(false),
#endif // IRAINMAN_ENABLE_AUTO_BAN
	lastUpdate(0)
{
	// [+] IRainman fix.
	m_myOnlineUser->getUser()->setFlag(User::NMDC);
	m_hubOnlineUser->getUser()->setFlag(User::NMDC);
	// [~] IRainman fix.
}

NmdcHub::~NmdcHub()
{
	clearUsers();
}


#define checkstate() if(state != STATE_NORMAL) return

void NmdcHub::connect(const OnlineUser& aUser, const string&)
{
	checkstate();
	dcdebug("NmdcHub::connect %s\n", aUser.getIdentity().getNick().c_str());
	if (isActive())
	{
		connectToMe(aUser);
	}
	else
	{
		revConnectToMe(aUser);
	}
}

void NmdcHub::refreshUserList(bool refreshOnly)
{
	if (refreshOnly)
	{
		OnlineUserList v;
		{
			// [!] IRainman fix potential deadlock.
			FastLock l(cs);
			
			for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
			{
				v.push_back(i->second);
			}
		}
		fire(ClientListener::UsersUpdated(), this, v);
	}
	else
	{
		clearUsers();
		getNickList();
	}
}

OnlineUserPtr NmdcHub::getUser(const string& aNick, bool hub /*= false*/)
{
	OnlineUserPtr ou = findUser(aNick); // !SMT!-S
	if (ou)
		return ou; // !SMT!-S
		
	// [+] IRainman fix.
	auto cm = ClientManager::getInstance();
	if (hub)
	{
		{
			FastLock l(cs);
			ou = m_users.insert(make_pair(aNick, getHubOnlineUser().get())).first->second;
			ou->inc();
		}
		ou->getIdentity().setNick(aNick);
	}
	else if (aNick == getMyNick())
	{
		{
			FastLock l(cs);
			ou = m_users.insert(make_pair(aNick, getMyOnlineUser().get())).first->second;
			ou->inc();
		}
	}
	// [~] IRainman fix.
	else
	{
		UserPtr p = cm->getUser(aNick, getHubUrl()
#ifdef PPA_INCLUDE_LASTIP_AND_USER_RATIO
		                        , getHubID()
#endif
		                       );
		OnlineUser* newUser = new OnlineUser(p, *this, 0);
		{
			FastLock l(cs);
			ou = m_users.insert(make_pair(aNick, newUser)).first->second; //2012-06-09_18-19-42_JBOQDRXR35PEW7OJOPLNPXJSQDETX4IUV3SHOHA_DEF32407_crash-stack-r501-x64-build-10294.dmp
			ou->inc();
		}
		ou->getIdentity().setNick(aNick);
	}
	
	if (!ou->getUser()->getCID().isZero()) // [+] IRainman fix.
	{
		cm->putOnline(ou);
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		UserManager::checkUser(ou);
#endif
	}
	return ou;
}

void NmdcHub::supports(const StringList& feat)
{
	const string x = Util::toSupportsCommand(feat);
	send(x);
}

OnlineUserPtr NmdcHub::findUser(const string& aNick) const
{
	FastLock l(cs);
	const auto& i = m_users.find(aNick); // [3] https://www.box.net/shared/03a0bb07362fc510b8a5
	return i == m_users.end() ? nullptr : i->second; // 2012-04-29_13-38-26_EJMPFXUHZAKEQON7Y6X7EIKZVS3S3GMF43CWO3Y_C95F3090_crash-stack-r501-build-9869.dmp
}

void NmdcHub::putUser(const string& aNick)
{
	OnlineUserPtr ou;
	{
		FastLock l(cs);
		const auto& i = m_users.find(aNick);
		if (i == m_users.end())
			return;
		ou = i->second;
		m_users.erase(i);
	}
	m_availableBytes -= ou->getIdentity().getBytesShared(); // https://code.google.com/p/flylinkdc/issues/detail?id=1231
	
	if (!ou->getUser()->getCID().isZero()) // [+] IRainman fix.
		ClientManager::getInstance()->putOffline(ou); // [2] https://www.box.net/shared/7b796492a460fe528961
		
	fire(ClientListener::UserRemoved(), this, ou); // [+] IRainman fix.
	ou->dec();// [!] IRainman fix memoryleak
}

void NmdcHub::clearUsers()
{
	if (ClientManager::isShutdown())
	{
		FastLock l(cs);
		for (auto i = m_users.cbegin(); i != m_users.cend(); ++i)
		{
			i->second->dec();
		}
		m_users.clear();
	}
	else
	{
		clearAvailableBytes();
		NickMap u2;
		{
			FastLock l(cs);
			u2.swap(m_users);
		}
		for (auto i = u2.cbegin(); i != u2.cend(); ++i)
		{
			if (!i->second->getUser()->getCID().isZero()) // [+] IRainman fix.
				ClientManager::getInstance()->putOffline(i->second); // http://code.google.com/p/flylinkdc/issues/detail?id=1064
			// Варианты
			// - скармливать юзеров массивом
			// - проработать команду на убивание всей мапы сразу без поиска
			
			i->second->dec();// [!] IRainman fix memoryleak
		}
	}
}

void NmdcHub::updateFromTag(Identity& id, string && tag) // [!] IRainman opt.
{
	const StringTokenizer<string> tok(tag, ',');
	string::size_type j;
	id.setLimit(0);
	for (auto i = tok.getTokens().cbegin(); i != tok.getTokens().cend(); ++i)
	{
		if (i->length() < 2)
		{
			continue;
		}
		// [!] IRainman opt: first use the compare, and only then to find.
		else if (i->compare(0, 2, "H:") == 0)
		{
			const StringTokenizer<string> t(i->substr(2), '/');
			if (t.getTokens().size() != 3)
			{
				continue;
			}
			id.setHubNormal(t.getTokens()[0].c_str());
			id.setHubRegister(t.getTokens()[1].c_str());
			id.setHubOperator(t.getTokens()[2].c_str());
		}
		else if (i->compare(0, 2, "S:") == 0)
		{
			const uint16_t slots = Util::toInt(i->c_str() + 2);
			id.setSlots(slots);
#ifdef IRAINMAN_ENABLE_AUTO_BAN
			if (slots > 0)
			{
				m_hubSupportsSlots = true;
			}
#endif // IRAINMAN_ENABLE_AUTO_BAN
		}
#ifdef IRAINMAN_ENABLE_FREE_SLOTS_IN_TAG
		else if (i->compare(0, 2, "F:") == 0) // TODO.
		{
			const uint16_t slots = Util::toInt(i->c_str() + 2);
			id.setFreeSlots(slots);
		}
#endif
		else if (i->compare(0, 2, "M:") == 0)
		{
			if (i->size() == 3)
			{
				// [!] IRainman fix.
				if ((*i)[2] == 'A')
				{
					id.getUser()->unsetFlag(User::NMDC_FILES_PASSIVE);
					id.getUser()->unsetFlag(User::NMDC_SEARCH_PASSIVE);
				}
				else
				{
					id.getUser()->setFlag(User::NMDC_FILES_PASSIVE);
					id.getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
				}
				// [~] IRainman fix.
			}
		}
		// [+] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=1234
#ifdef _DEBUG
		else if (i->compare(0, 2, "O:") == 0)
		{
			// [?] TODO http://nmdc.sourceforge.net/NMDC.html#_tag
		}
#endif
		// [~] IRainman fix.
		else if ((j = i->find("V:")) != string::npos)
		{
			id.setStringParam("AP", i->substr(0, j - 1)); // [+] IRainman fix.
			id.setStringParam("VE", i->substr(j + 2));
		}
		else if ((j = i->find("L:")) != string::npos)
		{
			const uint32_t l_limit = Util::toInt(i->c_str() + j + 2);
			id.setLimit(l_limit * 1024);
#ifdef PPA_INCLUDE_DEAD_CODE
			if (slots > 0)
				id.getUser()->setLastDownloadSpeed((uint16_t)((l_limit * 1024) / slots));
#endif
		}
		// [+] IRainman fix: https://code.google.com/p/flylinkdc/issues/detail?id=1234
		else if ((j = i->find("v:")) != string::npos)
		{
			id.setStringParam("AP", i->substr(0, j - 1));
			id.setStringParam("VE", i->substr(j + 2));
		}
		else if ((j = i->find(' ')) != string::npos)
		{
			id.setStringParam("AP", i->substr(0, j - 1));
			id.setStringParam("VE", i->substr(j + 1));
		}
		else if ((j = i->find("++")) != string::npos)
		{
			id.setStringParam("AP", *i);
		}
#ifdef _DEBUG
		else
		{
			FastLock l(NmdcSupports::g_debugCsUnknownNmdcTagParam);
			NmdcSupports::g_debugUnknownNmdcTagParam.insert(std::move(*i));
		}
#endif
		// [~] IRainman fix.
	}
	/// @todo Think about this
	// [-] id.setStringParam("TA", '<' + tag + '>'); [-] IRainman opt.
}

void NmdcHub::onLine(const string& aLine)
{
	if (aLine.empty())
		return;
		
	if (aLine[0] != '$')
	{
		// Check if we're being banned...
		if (state != STATE_NORMAL)
		{
			if (Util::findSubString(aLine, "banned") != string::npos)
			{
				setAutoReconnect(false);
			}
		}
		// [+] IRainman fix.
		const string line = toUtf8(aLine);
		
		if ((line.find("Hub-Security") != string::npos) && (line.find("was kicked by") != string::npos))
		{
			fire(ClientListener::StatusMessage(), this, unescape(line), ClientListener::FLAG_IS_SPAM);
			return;
		}
		else if ((line.find("is kicking") != string::npos) && (line.find("because:") != string::npos))
		{
			fire(ClientListener::StatusMessage(), this, unescape(line), ClientListener::FLAG_IS_SPAM);
			return;
		}
		// [~] IRainman fix.
		string nick;
		string message;
		bool bThirdPerson = false;
		
		if ((line.size() > 1 && line.substr(0, 2) == "* ") || (line.size() > 2 && line.substr(0, 3) == "** "))
		{
			size_t begin = line[1] == '*' ? begin = 3 : begin = 2;
			size_t end = line.find(' ', begin);
			
			if (end != string::npos)
			{
				nick = line.substr(begin, end - begin);
				message = line.substr(end + 1);
				bThirdPerson = true;
			}
		}
		else if (line[0] == '<')
		{
			string::size_type i = line.find('>', 2);
			
			if (i != string::npos)
			{
				if (line.size() <= i + 2)
				{
					// there is nothing after last '>'
					nick.clear();
				}
				else
				{
					nick = line.substr(1, i - 1);
					message = line.substr(i + 2);
				}
				
				/*
				if ((line.size() > i + 5) && (stricmp(line.substr(i + 2, 3), "/me") == 0 || stricmp(line.substr(i + 2, 3), "+me") == 0))
				{
				    if (BOOLSETTING(NSL_IGNORE_ME))
				        return;
				
				    line = "* " + nick + line.substr(i + 5);
				}
				*/
			}
		}
		
		/*
		if (line[0] != '<')
		{
		    fire(ClientListener::Message(), this, null, unescape(line));
		    return;
		}
		string::size_type i = line.find('>', 2);
		if (i == string::npos)
		{
		    fire(ClientListener::Message(), this, null, unescape(line));
		    return;
		}
		*/
		
		if (nick.empty())
		{
			fire(ClientListener::StatusMessage(), this, unescape(line));
			return;
		}
		
		if (message.empty())
		{
			message = line;
		}
		/* [-] IRainman fix.
		if ((line.find("Hub-Security") != string::npos) && (line.find("was kicked by") != string::npos))
		{
		    fire(ClientListener::StatusMessage(), this, unescape(line), ClientListener::FLAG_IS_SPAM);
		    return;
		}
		else if ((line.find("is kicking") != string::npos) && (line.find("because:") != string::npos))
		{
		    fire(ClientListener::StatusMessage(), this, unescape(line), ClientListener::FLAG_IS_SPAM);
		    return;
		}
		   [~] IRainman fix.*/
		//UserPtr u = NULL;
		//OnlineUserPtr u = NULL;
		//bool update = false;
		
		//if (nick.size())
		//{
		// [-] brain-ripper
		// I can't see any work with ClientListener
		// in this block, so I commented LockInstance,
		// because it caused deadlock in some situations.
		
		//ClientManager::LockInstance l; // !SMT!-fix
		
		// Perhaps we should use Lock(cs) here, because
		// findUser returns array element, that can be (theoretically)
		// deleted in other thread
		
		// [-] Lock _l(cs); [-] IRainman fix.
		
		/*
		OnlineUser* ou = findUser(nick);
		
		if (ou)
		{
		    u = ou;//->getUser(); // !SMT!-fix
		}
		else
		{
		    u = &getUser(nick);
		    // Assume that messages from unknown users come from the hub
		    u->getIdentity().setHub(true);
		    u->getIdentity().setHidden(true);
		    //u = &o;//.getUser(); // !SMT!-fix
		    update = true;
		}
		}
		if (update) fire(ClientListener::UserUpdated(), this, u); // !SMT!-fix
		
		fire(ClientListener::Message(), this, u.get()? u->getUser(): NULL, unescape(line)); // !SMT!-fix
		*/
		
		// [+] IRainman fix.
		ChatMessage chatMessage(unescape(message), findUser(nick));
		chatMessage.thirdPerson = bThirdPerson;
		// [~] IRainman fix.
		
		if (!chatMessage.from)
		{
			chatMessage.from = getUser(nick);
			// [-] if (chatMessage.from) [-] IRainman fix.
			{
				// Assume that messages from unknown users come from the hub
				chatMessage.from->getIdentity().setHub();
#ifdef IRAINMAN_USE_HIDDEN_USERS
				chatMessage.from->getIdentity().setHidden();
#endif
				fire(ClientListener::UserUpdated(), this, chatMessage.from);
			}
		}
		
		if (chatMessage.text.size() >= 4 && (strnicmp(chatMessage.text, "/me ", 4) == 0 ||
		                                     strnicmp(chatMessage.text, "+me ", 4) == 0))
		{
			/* [-] IRainman fix.
			if (BOOLSETTING(NSL_IGNORE_ME))
			    return;
			*/
			chatMessage.thirdPerson = true;
			chatMessage.text = chatMessage.text.substr(4);
		}
		
		if (!allowChatMessagefromUser(chatMessage)) // [+] IRainman fix.
			return;
			
		fire(ClientListener::Message(), this, chatMessage);
		return;
	}
	
	string cmd;
	string param;
	string::size_type x;
	
	if ((x = aLine.find(' ')) == string::npos)
	{
		cmd = aLine.substr(1);
	}
	else
	{
		cmd = aLine.substr(1, x - 1);
		param = toUtf8(aLine.substr(x + 1));
	}
	
	bool bMyInfoCommand = false;
	
	if (cmd == "Search")
	{
		if (state != STATE_NORMAL
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		        || getHideShare()
#endif
		   )
		{
			return;
		}
		string::size_type i = 0;
		string::size_type j = param.find(' ', i);
		if (j == string::npos || i == j)
			return;
			
		const auto seeker = param.substr(i, j - i);
		
		const auto isPassive = (seeker.compare(0, 4, "Hub:") == 0);
		const auto meActive = isActive();
		
		// Filter own searches
		if (meActive && !isPassive)
		{
			if (seeker == ((getFavIp().empty() ? getLocalIp() : getFavIp()) + ":" + Util::toString(SearchManager::getInstance()->getPort())))
			{
				return;
			}
		}
		else
		{
			// [!] IRainman fix: strongly check
			const auto& myNick = getMyNick();
			if (seeker.compare(4, myNick.size(), myNick) == 0) // [!] fix done [5] https://www.box.net/shared/qptgptxlocc0gmpo69q0
			{
				return;
			}
			// [~] IRainman fix.
		}
		
		i = j + 1;
#ifdef IRAINMAN_USE_SEARCH_FLOOD_FILTER
		uint64_t tick = GET_TICK();
		clearFlooders(tick);
		
		seekers.push_back(make_pair(seeker, tick));
		
		// First, check if it's a flooder
		for (auto fi = flooders.cbegin(); fi != flooders.cend(); ++fi)
		{
			if (fi->first == seeker)
			{
				return;
			}
		}
		
		int count = 0;
		for (auto fi = seekers.cbegin(); fi != seekers.cend(); ++fi)
		{
			if (fi->first == seeker) // [1] https://www.box.net/shared/9gb1e0hkp4220a7vwidj
				count++;
				
			if (count > 7)
			{
				if (isOp())
				{
					if (isPassive)
						fire(ClientListener::SearchFlood(), this, seeker.substr(4));
					else
						fire(ClientListener::SearchFlood(), this, seeker + ' ' + STRING(NICK_UNKNOWN));
				}
				
				flooders.push_back(make_pair(seeker, tick));
				return;
			}
		}
#endif // IRAINMAN_USE_SEARCH_FLOOD_FILTER
		Search::SizeModes a;
		if (param[i] == 'F')
		{
			a = Search::SIZE_DONTCARE;
		}
		else if (param[i + 2] == 'F')
		{
			a = Search::SIZE_ATLEAST;
		}
		else
		{
			a = Search::SIZE_ATMOST;
		}
		i += 4;
		j = param.find('?', i);
		if (j == string::npos || i == j)
			return;
		string size = param.substr(i, j - i);
		i = j + 1;
		j = param.find('?', i);
		if (j == string::npos || i == j)
			return;
		int type = Util::toInt(param.substr(i, j - i)) - 1; // TODO уйти от int
		i = j + 1;
		string terms = unescape(param.substr(i));
		
		if (!terms.empty())
		{
			if (isPassive)
			{
				OnlineUserPtr u = findUser(seeker.substr(4));
				
				if (!u)
					return;
					
				// [!] IRainman fix
				if (!u->getUser()->isSet(User::NMDC_SEARCH_PASSIVE))
				{
					u->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
					updated(u);
				}
				// [~] IRainman fix.
				
				// ignore if we or remote client don't support NAT traversal in passive mode
				// although many NMDC hubs won't send us passive if we're in passive too, so just in case...
				if (!meActive && (!u->getUser()->isSet(User::NAT0) || !BOOLSETTING(ALLOW_NAT_TRAVERSAL)))
					return;
			}
			
			fire(ClientListener::NmdcSearch(), this, seeker, a, Util::toInt64(size), type, terms, isPassive);
		}
	}
	else if (cmd == "MyINFO")
	{
		bMyInfoCommand = true;
		
		string::size_type i, j;
		i = 5;
		j = param.find(' ', i);
		if ((j == string::npos) || (j == i))
			return;
		string nick = param.substr(i, j - i);
		
		if (nick.empty())
			return;
			
		i = j + 1;
		
		OnlineUserPtr ou = getUser(nick); // [!] IRainman fix: use OnlineUserPtr
		
		j = param.find('$', i);
		if (j == string::npos)
			return;
			
		string tmpDesc = unescape(param.substr(i, j - i));
		// Look for a tag...
		if (!tmpDesc.empty() && tmpDesc[tmpDesc.size() - 1] == '>')
		{
			x = tmpDesc.rfind('<');
			if (x != string::npos)
			{
				// Hm, we have something...disassemble it...
				updateFromTag(ou->getIdentity(), tmpDesc.substr(x + 1, tmpDesc.length() - x - 2)); // [1] https://www.box.net/shared/182trhjuvkkx2l8nblq2
				tmpDesc.erase(x);
			}
		}
		ou->getIdentity().setDescription(tmpDesc);
		
		i = j + 3;
		j = param.find('$', i);
		if (j == string::npos)
			return;
			
		// [!] IRainman fix.
		if (i == j || j - i - 1 == 0)
		{
			// No connection = bot...
			ou->getIdentity().setBot();
			NmdcSupports::setStatus(ou->getIdentity(), param[j - 1]);
		}
		else
		{
			NmdcSupports::setStatus(ou->getIdentity(), param[j - 1], param.substr(i, j - i - 1));
		}
		// [~] IRainman fix.
		
		i = j + 1;
		j = param.find('$', i);
		
		if (j == string::npos)
			return;
			
		ou->getIdentity().setEmail(unescape(param.substr(i, j - i)));
		
		i = j + 1;
		j = param.find('$', i);
		if (j == string::npos)
			return;
			
		auto l_share_size = Util::toInt64(param.c_str() + i); // Иногда шара бывает == -1 http://www.flickr.com/photos/96019675@N02/9732534452/
		if (l_share_size < 0)
		{
			l_share_size = 0;
			LogManager::getInstance()->message("ShareSize < 0 !, param = " + param);
		}
		changeBytesShared(ou->getIdentity(), l_share_size);
		
		/* [-] IRainman fix.
		if (ou->getUser() == getMyIdentity().getUser())
		{
		    setMyIdentity(ou.getIdentity);
		}
		   [~] IRainman fix.*/
		if (!ClientManager::isShutdown())
		{
			fire(ClientListener::UserUpdated(), this, ou); // !SMT!-fix
		}
	}
	else if (cmd == "Quit")
	{
		if (!param.empty())
		{
			const string& nick = param;
			OnlineUserPtr u = findUser(nick);
			if (!u)
				return;
				
			// [-] fire(ClientListener::UserRemoved(), this, u); // !SMT!-fix [-] IRainman fix.
			
			putUser(nick);
		}
	}
	else if (cmd == "ConnectToMe")
	{
		if (state != STATE_NORMAL)
		{
			return;
		}
		string::size_type i = param.find(' ');
		string::size_type j;
		if ((i == string::npos) || ((i + 1) >= param.size()))
		{
			return;
		}
		i++;
		j = param.find(':', i);
		if (j == string::npos)
		{
			return;
		}
		string server = param.substr(i, j - i);
		if (j + 1 >= param.size())
		{
			return;
		}
		
		string senderNick;
		string port;
		
		i = param.find(' ', j + 1);
		if (i == string::npos)
		{
			port = param.substr(j + 1);
		}
		else
		{
			senderNick = param.substr(i + 1);
			port = param.substr(j + 1, i - j - 1);
		}
		
		bool secure = false;
		if (port[port.size() - 1] == 'S')
		{
			port.erase(port.size() - 1);
			if (CryptoManager::getInstance()->TLSOk())
			{
				secure = true;
			}
		}
		
		if (BOOLSETTING(ALLOW_NAT_TRAVERSAL))
		{
			if (port[port.size() - 1] == 'N')
			{
				if (senderNick.empty())
					return;
					
				port.erase(port.size() - 1);
				
				// Trigger connection attempt sequence locally ...
				ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), sock->getLocalPort(),
				                                              BufferedSocket::NAT_CLIENT, getMyNick(), getHubUrl(),
				                                              getEncoding(),
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
				                                              getStealth(),
#endif
				                                              secure
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
				                                              && !getStealth()
#endif
				                                             );
				                                             
				// ... and signal other client to do likewise.
				send("$ConnectToMe " + senderNick + ' ' + getLocalIp() + ":" + Util::toString(sock->getLocalPort()) + (secure ? "RS" : "R") + '|');
				return;
			}
			else if (port[port.size() - 1] == 'R')
			{
				port.erase(port.size() - 1);
				
				// Trigger connection attempt sequence locally
				ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), sock->getLocalPort(),
				                                              BufferedSocket::NAT_SERVER, getMyNick(), getHubUrl(),
				                                              getEncoding(),
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
				                                              getStealth(),
#endif
				                                              secure);
				return;
			}
		}
		
		if (port.empty())
			return;
			
		// For simplicity, we make the assumption that users on a hub have the same character encoding
		ConnectionManager::getInstance()->nmdcConnect(server, static_cast<uint16_t>(Util::toInt(port)), getMyNick(), getHubUrl(),
		                                              getEncoding(),
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
		                                              getStealth(),
#endif
		                                              secure);
	}
	else if (cmd == "RevConnectToMe")
	{
		if (state != STATE_NORMAL)
		{
			return;
		}
		
		string::size_type j = param.find(' ');
		if (j == string::npos)
		{
			return;
		}
		
		OnlineUserPtr u = findUser(param.substr(0, j));
		if (!u)
			return;
			
		if (isActive())
		{
			connectToMe(*u);
		}
		else if (BOOLSETTING(ALLOW_NAT_TRAVERSAL) && u->getUser()->isSet(User::NAT0))
		{
			bool secure = CryptoManager::getInstance()->TLSOk() && u->getUser()->isSet(User::TLS)
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
			              && !getStealth()
#endif
			              ;
			// NMDC v2.205 supports "$ConnectToMe sender_nick remote_nick ip:port", but many NMDC hubsofts block it
			// sender_nick at the end should work at least in most used hubsofts
			send("$ConnectToMe " + fromUtf8(u->getIdentity().getNick()) + ' ' + getLocalIp() + ":" + Util::toString(sock->getLocalPort()) + (secure ? "NS " : "N ") + fromUtf8(getMyNick()) + '|');
		}
		else
		{
			// [!] IRainman fix.
			if (!u->getUser()->isSet(User::NMDC_FILES_PASSIVE))
			{
				// [!] IRainman fix: You can not reset the user to flag as if we are passive, not him!
				// [-] u->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
				// [-] Notify the user that we're passive too...
				// [-] updated(u); [-]
				revConnectToMe(*u);
				
				return;
			}
			// [~] IRainman fix.
		}
	}
	else if (cmd == "SR")
	{
		SearchManager::getInstance()->onSearchResult(aLine);
	}
	else if (cmd == "HubName")
	{
		// Workaround replace newlines in topic with spaces, to avoid funny window titles
		// If " - " found, the first part goes to hub name, rest to description
		// If no " - " found, first word goes to hub name, rest to description
		
		string::size_type i;
		while ((i = param.find("\r\n")) != string::npos)
			param.replace(i, 2, " ");
			
		while ((i = param.find('\n')) != string::npos) // [+] IRainman fix.
			param.replace(i, 1, " ");
			
		OnlineUserPtr ou;// [+] IRainman fix.
		string hubnick;
		
		i = param.find(" - ");
		if (i == string::npos)
		{
			i = param.find(' ');
			if (i == string::npos)
			{
				hubnick = unescape(param);
				ou = getUser(hubnick, true);
			}
			else
			{
				hubnick = unescape(param.substr(0, i));
				ou = getUser(hubnick, true);
				if (!SETTING(STRIP_TOPIC))
					ou->getIdentity().setDescription(unescape(param.substr(i + 1)));
			}
		}
		else
		{
			hubnick = unescape(param.substr(0, i));
			ou = getUser(hubnick, true); // https://www.box.net/shared/0w28tcl49iv1wnckpwi8
			if (!SETTING(STRIP_TOPIC))
				ou->getIdentity().setDescription(unescape(param.substr(i + 3)));
		}
		ou->getIdentity().setNick(hubnick); // [+] IRainman fix.
		fire(ClientListener::HubUpdated(), this);
	}
	else if (cmd == "Supports")
	{
		const StringTokenizer<string> st(param, ' ');
		const StringList& sl = st.getTokens();
		for (auto i = sl.cbegin(); i != sl.cend(); ++i)
		{
			if (*i == "UserCommand")
			{
				m_supportFlags |= SUPPORTS_USERCOMMAND;
			}
			else if (*i == "NoGetINFO")
			{
				m_supportFlags |= SUPPORTS_NOGETINFO;
			}
			else if (*i == "UserIP2")
			{
				m_supportFlags |= SUPPORTS_USERIP2;
			}
		}
	}
	else if (cmd == "UserCommand")
	{
		string::size_type i = 0;
		string::size_type j = param.find(' ');
		if (j == string::npos)
			return;
			
		int type = Util::toInt(param.substr(0, j));
		i = j + 1;
		if (type == UserCommand::TYPE_SEPARATOR || type == UserCommand::TYPE_CLEAR)
		{
			int ctx = Util::toInt(param.substr(i));
			fire(ClientListener::HubUserCommand(), this, type, ctx, Util::emptyString, Util::emptyString);
		}
		else if (type == UserCommand::TYPE_RAW || type == UserCommand::TYPE_RAW_ONCE)
		{
			j = param.find(' ', i);
			if (j == string::npos)
				return;
			int ctx = Util::toInt(param.substr(i));
			i = j + 1;
			j = param.find('$');
			if (j == string::npos)
				return;
			string name = unescape(param.substr(i, j - i));
			// NMDC uses '\' as a separator but both ADC and our internal representation use '/'
			Util::replace("/", "//", name);
			Util::replace("\\", "/", name);
			i = j + 1;
			string command = unescape(param.substr(i, param.length() - i));
			fire(ClientListener::HubUserCommand(), this, type, ctx, name, command);
		}
	}
	else if (cmd == "Lock")
	{
		if (state != STATE_PROTOCOL || aLine.size() < 6)
		{
			return;
		}
		state = STATE_IDENTIFY;
		
		// Param must not be toUtf8'd...
		param = aLine.substr(6);
		
		if (!param.empty())
		{
			/* [-] IRainman fix: old proto code: http://mydc.ru/topic915.html?p=6719#entry6719
			   [-]string::size_type j = param.find(" Pk=");
			   [-]string lock, pk;
			   [-]if (j != string::npos)
			   [-]{
			   [-]  lock = param.substr(0, j);
			   [-]  pk = param.substr(j + 4);
			   [-]}
			   [-]else
			   [-] */
			// [-]{
			const auto j = param.find(' '); // [!]
			const auto lock = (j != string::npos) ? param.substr(0, j) : param; // [!]
			// [-]}
			// [~] IRainman fix.
			
			if (CryptoManager::isExtended(lock))
			{
				StringList feat;
				feat.push_back("UserCommand");
				feat.push_back("NoGetINFO");
				feat.push_back("NoHello");
				feat.push_back("UserIP2");
				feat.push_back("TTHSearch");
				feat.push_back("ZPipe0");
				
				if (BOOLSETTING(COMPRESS_TRANSFERS))
					feat.push_back("GetZBlock");
					
				if (CryptoManager::getInstance()->TLSOk()
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
				        && !getStealth()
#endif
				   )
				{
					feat.push_back("TLS");
				}
#ifdef STRONG_USE_DHT
				if (BOOLSETTING(USE_DHT))
				{
					feat.push_back("DHT0");
				}
#endif
				supports(feat);
			}
			
			key(CryptoManager::getInstance()->makeKey(lock));
			OnlineUserPtr ou = getUser(getMyNick()); // [!] IRainman fix: use OnlineUserPtr
			validateNick(ou->getIdentity().getNick());
		}
	}
	else if (cmd == "Hello")
	{
		if (!param.empty())
		{
			OnlineUserPtr ou = getUser(param); // [!] IRainman fix: use OnlineUserPtr
			
			if (isMe(ou))
			{
				// [!] IRainman fix.
				if (isActive())
				{
					ou->getUser()->unsetFlag(User::NMDC_FILES_PASSIVE);
					ou->getUser()->unsetFlag(User::NMDC_SEARCH_PASSIVE);
				}
				else
				{
					ou->getUser()->setFlag(User::NMDC_FILES_PASSIVE);
					ou->getUser()->setFlag(User::NMDC_SEARCH_PASSIVE);
				}
				// [~] IRainman fix.
				
				if (state == STATE_IDENTIFY)
				{
					state = STATE_NORMAL;
					updateCounts(false);
					
					version();
					getNickList();
					myInfo(true);
					// [-] IRainman.
					//SetEvent(m_hEventClientInitialized);
				}
			}
			fire(ClientListener::UserUpdated(), this, ou); // !SMT!-fix
		}
	}
	else if (cmd == "ForceMove")
	{
		dcassert(sock);
		if (sock)
			sock->disconnect(false);
		fire(ClientListener::Redirect(), this, param);
	}
	else if (cmd == "HubIsFull")
	{
		fire(ClientListener::HubFull(), this);
	}
	else if (cmd == "ValidateDenide")        // Mind the spelling...
	{
		dcassert(sock);
		if (sock)
			sock->disconnect(false);
		fire(ClientListener::NickTaken(), this);
	}
	else if (cmd == "UserIP")
	{
		if (!param.empty())
		{
			OnlineUserList v;
			const StringTokenizer<string> t(param, "$$");
			const StringList& l = t.getTokens();
			{
				// [-] brain-ripper
				// I can't see any work with ClientListener
				// in this block, so I commented LockInstance,
				// because it caused deadlock in some situations.
				
				//ClientManager::LockInstance lc; // !SMT!-fix
				
				// Perhaps we should use Lock(cs) here, because
				// some elements of this class can be (theoretically)
				// changed in other thread
				
				// Lock _l(cs); [-] IRainman fix.
				
				for (auto it = l.cbegin(); it != l.cend(); ++it)
				{
					string::size_type j = 0;
					if ((j = it->find(' ')) == string::npos)
						continue;
					if ((j + 1) == it->length())
						continue;
						
					OnlineUserPtr ou = findUser(it->substr(0, j));
					
					if (!ou)
						continue;
						
					const string l_ip = it->substr(j + 1);
					ou->getIdentity().setIp(l_ip);
					/* [-] IRainman fix.
					if (ou->getUser() == getMyIdentity().getUser())
					{
					 setMyIdentity(ou->getIdentity); // [-]
					}
					   [-]IRainman fix. */
					v.push_back(ou);
				}
			}
			fire(ClientListener::UsersUpdated(), this, v); // !SMT!-fix
		}
	}
	else if (cmd == "NickList")
	{
		if (!param.empty())
		{
			OnlineUserList v;
			const StringTokenizer<string> t(param, "$$");
			const StringList& sl = t.getTokens();
			
			{
				// [-] brain-ripper
				// I can't see any work with ClientListener
				// in this block, so I commented LockInstance,
				// because it caused deadlock in some situations.
				
				//ClientManager::LockInstance l; // !SMT!-fix
				
				// Perhaps we should use Lock(cs) here, because
				// some elements of this class can be (theoretically)
				// changed in other thread
				
				// Lock _l(cs); [-] IRainman fix: no needs lock here!
				
				for (auto it = sl.cbegin(); it != sl.cend(); ++it)
				{
					if (it->empty())
						continue;
						
					v.push_back(getUser(*it));// [!] IRainman fix: use OnlineUserPtr
				}
				
				if (!(m_supportFlags & SUPPORTS_NOGETINFO))
				{
					string tmp;
					// Let's assume 10 characters per nick...
					tmp.reserve(v.size() * (11 + 10 + getMyNick().length()));
					string n = ' ' +  fromUtf8(getMyNick()) + '|';
					for (auto i = v.cbegin(); i != v.cend(); ++i)
					{
						tmp += "$GetINFO ";
						tmp += fromUtf8((*i)->getIdentity().getNick());
						tmp += n;
					}
					if (!tmp.empty())
					{
						send(tmp);
					}
				}
			}
			fire(ClientListener::UsersUpdated(), this, v); // !SMT!-fix
		}
	}
	else if (cmd == "OpList")
	{
		if (!param.empty())
		{
			OnlineUserList v;
			const StringTokenizer<string> t(param, "$$");
			const StringList& sl = t.getTokens();
			{
				// [-] brain-ripper
				// I can't see any work with ClientListener
				// in this block, so I commented LockInstance,
				// because it caused deadlock in some situations.
				
				//ClientManager::LockInstance l; // !SMT!-fix
				
				// Perhaps we should use Lock(cs) here, because
				// some elements of this class can be (theoretically)
				// changed in other thread
				
				// Lock _l(cs); [-] IRainman fix: no needs any lock here!
				
				for (auto it = sl.cbegin(); it != sl.cend(); ++it)
				{
					if (it->empty())
						continue;
						
					OnlineUserPtr ou = getUser(*it);
					ou->getIdentity().setOp(true);
					/* [-] IRainman fix.
					if (isMe(ou))
					{
					    // [-] setMyIdentity(ou->getIdentity); [-]
					}
					// [-] IRainman fix*/
					v.push_back(ou);
				}
			}
			
			fire(ClientListener::UsersUpdated(), this, v); // !SMT!-fix
			updateCounts(false);
			
			// Special...to avoid op's complaining that their count is not correctly
			// updated when they log in (they'll be counted as registered first...)
			myInfo(false);
		}
	}
	else if (cmd == "To:")
	{
		string::size_type i = param.find("From: "); // [!] IRainman fix: with space after!
		if (i == string::npos)
			return;
			
		i += 6;
		string::size_type j = param.find('$', i);
		if (j == string::npos)
			return;
			
		const string rtNick = param.substr(i, j - 1 - i);
		if (rtNick.empty())
			return;
		i = j + 1;
		
		if (param.size() < i + 3 || param[i] != '<')
			return;
			
		j = param.find('>', i);
		if (j == string::npos)
			return;
			
		const string fromNick = param.substr(i + 1, j - i - 1);
		if (fromNick.empty() || param.size() < j + 2)
			return;
			
		ChatMessage message(param.substr(j + 2), findUser(fromNick), nullptr, findUser(rtNick)); // [+] IRainman fix.
		
		//if (message.replyTo == nullptr || message.from == nullptr) [-] IRainman fix.
		{
			if (message.replyTo == nullptr)
			{
				// Assume it's from the hub
				message.replyTo = getUser(rtNick);// [!] IRainman fix: use OnlineUserPtr
				message.replyTo->getIdentity().setHub();
#ifdef IRAINMAN_USE_HIDDEN_USERS
				message.replyTo->getIdentity().setHidden();
#endif
				fire(ClientListener::UserUpdated(), this, message.replyTo); // !SMT!-fix
			}
			if (message.from == nullptr)
			{
				// Assume it's from the hub
				message.from = getUser(fromNick);// [!] IRainman fix: use OnlineUserPtr
				message.from->getIdentity().setHub();
#ifdef IRAINMAN_USE_HIDDEN_USERS
				message.from->getIdentity().setHidden();
#endif
				fire(ClientListener::UserUpdated(), this, message.from); // !SMT!-fix
			}
			
			// Update pointers just in case they've been invalidated
			// message.replyTo = findUser(rtNick); [-] IRainman fix. Imposibru!!!
			// message.from = findUser(fromNick); [-] IRainman fix. Imposibru!!!
		}
		
		message.to = getMyOnlineUser(); // !SMT!-S [!] IRainman fix.
		
		// [+]IRainman fix: message from you to you is not allowed! Block magical spam.
		if (message.to->getUser() == message.from->getUser() && message.from->getUser() == message.replyTo->getUser())
		{
			fire(ClientListener::StatusMessage(), this, message.text, ClientListener::FLAG_IS_SPAM);
			LogManager::getInstance()->message("Magic spam message (from you to you) filtered on hub: " + getHubUrl() + ".");
			return;
		}
		if (!allowPrivateMessagefromUser(message)) // [+] IRainman fix.
			return;
			
		fire(ClientListener::Message(), this, message); // [+]
		// [~]  IRainman fix.
	}
	else if (cmd == "GetPass")
	{
		// [!] IRainman fix.
		getUser(getMyNick()); // [!] use OnlineUserPtr, don't delete this line.
		setRegistered(); // [!]
		// setMyIdentity(ou->getIdentity()); [-]
		processingPassword(); // [!]
		// [~] IRainman fix.
	}
	else if (cmd == "BadPass")
	{
		setPassword(Util::emptyString);
	}
	else if (cmd == "ZOn")
	{
		try
		{
			sock->setMode(BufferedSocket::MODE_ZPIPE);
		}
		catch (const Exception& e)
		{
			dcdebug("NmdcHub::onLine %s failed with error: %s\n", cmd.c_str(), e.getError().c_str());
		}
	}
	else if (cmd == "HubTopic" && SETTING(ENABLE_HUBTOPIC))
	{
		fire(ClientListener::HubTopic(), this, param);
	}
	// [+] IRainman.
	else if (cmd == "LogedIn")
	{
		messageYouHaweRightOperatorOnThisHub();
	}
#ifdef IRAINMAN_SET_USER_IP_ON_LOGON
	else if (cmd == "UserIP2" && !param.empty())
	{
		getMyIdentity().setIp(param);
	}
#endif
	// [~] IRainman.
	else
	{
		dcdebug("NmdcHub::onLine Unknown command %s\n", aLine.c_str());
	}
	
	if (!bMyInfoCommand && m_bLastMyInfoCommand == FIRST_MYINFO)
	{
#ifdef RIP_USE_CONNECTION_AUTODETECT
		// This is first command after $MyInfo.
		// Do autodetection now, because at least VerliHub hag such a bug:
		// when hub sends $myInfo for each user on handshake sequence, it may skip
		// much connection requests (or may be also other commands), so it is better not to send
		// anything to it when receiving $myInfos is in progress
		RequestConnectionForAutodetect();
#endif
		m_bLastMyInfoCommand = ALREADY_GOT_MYINFO;
	}
	
	if (bMyInfoCommand && m_bLastMyInfoCommand == DIDNT_GET_YET_FIRST_MYINFO)
		m_bLastMyInfoCommand = FIRST_MYINFO;
}

string NmdcHub::checkNick(const string& aNick)
{
	string tmp = aNick;
	for (size_t i = 0; i < aNick.size(); ++i)
	{
		if (static_cast<uint8_t>(tmp[i]) <= 32 || tmp[i] == '|' || tmp[i] == '$' || tmp[i] == '<' || tmp[i] == '>')
		{
			tmp[i] = '_';
		}
	}
	return tmp;
}

void NmdcHub::connectToMe(const OnlineUser& aUser
#ifdef RIP_USE_CONNECTION_AUTODETECT
                          , ExpectedMap::DefinedExpectedReason reason /* = ExpectedMap::REASON_DEFAULT */
#endif
                         )
{
	checkstate();
	dcdebug("NmdcHub::connectToMe %s\n", aUser.getIdentity().getNick().c_str());
	string nick = fromUtf8(aUser.getIdentity().getNick());
	ConnectionManager::getInstance()->nmdcExpect(nick, getMyNick(), getHubUrl()
#ifdef RIP_USE_CONNECTION_AUTODETECT
	                                             , reason
#endif
	                                            );
	ConnectionManager::iConnToMeCount++;
	
	bool secure = CryptoManager::getInstance()->TLSOk() && aUser.getUser()->isSet(User::TLS)
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
	              && !getStealth()
#endif
	              ;
	uint16_t port = secure ? ConnectionManager::getInstance()->getSecurePort() : ConnectionManager::getInstance()->getPort();
	send("$ConnectToMe " + nick + ' ' + getLocalIp() + ":" + Util::toString(port) + (secure ? "S" : "") + '|');
}

void NmdcHub::revConnectToMe(const OnlineUser& aUser)
{
	checkstate();
	dcdebug("NmdcHub::revConnectToMe %s\n", aUser.getIdentity().getNick().c_str());
	send("$RevConnectToMe " + fromUtf8(getMyNick()) + ' ' + fromUtf8(aUser.getIdentity().getNick()) + '|'); //[1] https://www.box.net/shared/f8330d2c54b2d7dcf3e4
}

void NmdcHub::hubMessage(const string& aMessage, bool thirdPerson)
{
	checkstate();
	send(fromUtf8Chat('<' + getMyNick() + "> " + escape(thirdPerson ? "/me " + aMessage : aMessage) + '|')); // IRAINMAN_USE_UNICODE_IN_NMDC
}

void NmdcHub::myInfo(bool alwaysSend)
{
	const uint64_t currentTick = GET_TICK(); // [!] IRainman opt.
	if (!alwaysSend && lastUpdate + 15000 > currentTick) return; // antispam
	
	checkstate();
	
	const FavoriteHubEntry *fhe = reloadSettings(false);
	
	//[-]PPA    dcdebug("MyInfo %s...\n", getMyNick().c_str());
	
	char modeChar;
	if (SETTING(OUTGOING_CONNECTIONS) == SettingsManager::OUTGOING_SOCKS5)
		modeChar = '5';
	else if (isActive())
		modeChar = 'A';
	else
		modeChar = 'P';
		
	const int64_t upLimit = BOOLSETTING(THROTTLE_ENABLE) ? ThrottleManager::getInstance()->getUploadLimitInKBytes() : 0;// [!] IRainman SpeedLimiter
	const string uploadSpeed = (upLimit > 0) ? Util::toString(upLimit) + " KiB/s" : SETTING(UPLOAD_SPEED); // [!] IRainman SpeedLimiter
	
	char status = NmdcSupports::NORMAL;
	
#ifdef IRAINMAN_ENABLE_STEALTH_MODE
	if (getStealth())
	{
	}
	else
#endif
	{
		if (Util::getAway())
		{
			status |= NmdcSupports::AWAY;
		}
		if (UploadManager::getInstance()->getIsFileServerStatus())
		{
			status |= NmdcSupports::SERVER;
		}
		if (UploadManager::getInstance()->getIsFireballStatus())
		{
			status |= NmdcSupports::FIREBALL;
		}
		if (BOOLSETTING(ALLOW_NAT_TRAVERSAL) && !isActive())
		{
			status |= NmdcSupports::NAT0;
		}
	}
	
	if (CryptoManager::getInstance()->TLSOk())
	{
		status |= NmdcSupports::TLS;
	}
#ifndef IRAINMAN_TEMPORARY_DISABLE_XXX_ICON
	switch (SETTING(FLY_GENDER))
	{
		case 1:
			status |= 0x20;
			break;
		case 2:
			status |= 0x40;
			break;
		case 3:
			status |= 0x60;
			break;
	}
#endif
//[+]FlylinkDC
	const string currentCounts = (fhe && fhe->getExclusiveHub()) ? getCountsIndivid() : getCounts();
//[~]FlylinkDC

	// IRAINMAN_USE_UNICODE_IN_NMDC
	string currentMyInfo;
	currentMyInfo.resize(256);
	currentMyInfo.resize(snprintf(&currentMyInfo[0], currentMyInfo.size(), "$MyINFO $ALL %s %s<%s,M:%c,H:%s,S:%d"
#ifdef IRAINMAN_ENABLE_FREE_SLOTS_IN_TAG
	                              ",F%d"
#endif
	                              ">$ $%s%c$%s$",
	                              fromUtf8(getMyNick()).c_str(),
	                              fromUtf8Chat(escape(getCurrentDescription())).c_str(),
	                              (getClientName() + " V:" + getClientVersion()).c_str(), // [!] IRainman mimicry function.
	                              modeChar,
	                              currentCounts.c_str(),
	                              UploadManager::getInstance()->getSlots(),
#ifdef IRAINMAN_ENABLE_FREE_SLOTS_IN_TAG
	                              UploadManager::getInstance()->getFreeSlots(),
#endif
	                              uploadSpeed.c_str(), status,
	                              fromUtf8Chat(escape(getCurrentEmail())).c_str()));
	                              
	const int64_t currentBytesShared =
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	    getHideShare() ? 0 :
#endif
	    ShareManager::getInstance()->getShareSize();
	    
	if (alwaysSend ||
	        (currentBytesShared != lastbytesshared && lastUpdate + 15 * 60 * 1000 < currentTick) ||
	        currentMyInfo != lastmyinfo)  // [!] IRainman opt.
	{
		lastmyinfo = currentMyInfo;
		lastbytesshared = currentBytesShared;
		send(lastmyinfo + Util::toString(currentBytesShared) + "$|");
		lastUpdate = currentTick;
	}
}

void NmdcHub::search(Search::SizeModes aSizeType, int64_t aSize, int aFileType, const string& aString, const string&, const StringList&)
{
	checkstate();
	const char c1 = (aSizeType == Search::SIZE_DONTCARE || aSizeType == Search::SIZE_EXACT) ? 'F' : 'T';
	const char c2 = (aSizeType == Search::SIZE_ATLEAST) ? 'F' : 'T';
	string tmp = ((aFileType == SearchManager::TYPE_TTH) ? g_tth + aString : fromUtf8(escape(aString))); // [!] IRainman opt.
	string::size_type i;
	while ((i = tmp.find(' ')) != string::npos)
	{
		tmp[i] = '$';
	}
	string tmp2;
	if (isActive() && !BOOLSETTING(SEARCH_PASSIVE))
	{
		tmp2 = (getFavIp().empty() ? getLocalIp() : getFavIp()) + ':' + Util::toString(SearchManager::getInstance()->getPort());
	}
	else
	{
		tmp2 = "Hub:" + fromUtf8(getMyNick());
	}
	const string l_search_command = "$Search " + tmp2 + ' ' + c1 + '?' + c2 + '?' + Util::toString(aSize) + '?' + Util::toString(aFileType + 1) + '?' + tmp + '|';
	const string l_debug_string =  "[" + (isActive() ? string("Active") : string("Passive")) + " search][Client:" + getHubUrl() + "][Command:" + l_search_command + " ]";
	dcdebug("[NmdcHub::search] %s \r\n", l_debug_string.c_str());
	g_last_search_string = l_debug_string;
	//LogManager::getInstance()->message(l_debug_string);
	send(l_search_command);
}

string NmdcHub::validateMessage(string tmp, bool reverse)
{
	string::size_type i = 0;
	
	if (reverse)
	{
		while ((i = tmp.find("&#36;", i)) != string::npos)
		{
			tmp.replace(i, 5, "$");
			i++;
		}
		i = 0;
		while ((i = tmp.find("&#124;", i)) != string::npos)
		{
			tmp.replace(i, 6, "|");
			i++;
		}
		i = 0;
		while ((i = tmp.find("&amp;", i)) != string::npos)
		{
			tmp.replace(i, 5, "&");
			i++;
		}
	}
	else
	{
		i = 0;
		while ((i = tmp.find("&amp;", i)) != string::npos)
		{
			tmp.replace(i, 1, "&amp;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find("&#36;", i)) != string::npos)
		{
			tmp.replace(i, 1, "&amp;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find("&#124;", i)) != string::npos)
		{
			tmp.replace(i, 1, "&amp;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find('$', i)) != string::npos)
		{
			tmp.replace(i, 1, "&#36;");
			i += 4;
		}
		i = 0;
		while ((i = tmp.find('|', i)) != string::npos)
		{
			tmp.replace(i, 1, "&#124;");
			i += 5;
		}
	}
	return tmp;
}

void NmdcHub::privateMessage(const string& nick, const string& message, bool thirdPerson)
{
	send("$To: " + fromUtf8(nick) + " From: " + fromUtf8(getMyNick()) + " $" + fromUtf8Chat(escape('<' + getMyNick() + "> " + (thirdPerson ? "/me " + message : message))) + '|'); // IRAINMAN_USE_UNICODE_IN_NMDC
}

void NmdcHub::privateMessage(const OnlineUserPtr& aUser, const string& aMessage, bool thirdPerson)   // !SMT!-S
{
	checkstate();
	
	privateMessage(aUser->getIdentity().getNick(), aMessage, thirdPerson);
	// Emulate a returning message...
	// Lock l(cs); // !SMT!-fix: no data to lock
	
	// [!] IRainman fix.
	const OnlineUserPtr& me = getMyOnlineUser();
	// fire(ClientListener::PrivateMessage(), this, getMyNick(), me, aUser, me, '<' + getMyNick() + "> " + aMessage, thirdPerson); // !SMT!-S [-] IRainman fix.
	
	ChatMessage message(aMessage, me, aUser, me, thirdPerson);
	if (!allowPrivateMessagefromUser(message))
		return;
		
	fire(ClientListener::Message(), this, message);
	// [~] IRainman fix.
}

void NmdcHub::sendUserCmd(const UserCommand& command, const StringMap& params)
{
	checkstate();
	string cmd = Util::formatParams(command.getCommand(), params, false);
	if (command.isChat())
	{
		if (command.getTo().empty())
		{
			hubMessage(cmd);
		}
		else
		{
			privateMessage(command.getTo(), cmd, false);
		}
	}
	else
	{
		send(fromUtf8(cmd));
	}
}
#ifdef IRAINMAN_USE_SEARCH_FLOOD_FILTER
void NmdcHub::clearFlooders(uint64_t aTick)
{
	while (!seekers.empty() && seekers.front().second + (5 * 1000) < aTick)
	{
		seekers.pop_front();
	}
	
	while (!flooders.empty() && flooders.front().second + (120 * 1000) < aTick)
	{
		flooders.pop_front();
	}
}
#endif // IRAINMAN_USE_SEARCH_FLOOD_FILTER
void NmdcHub::on(Connected) noexcept
{
	Client::on(Connected());
	
	if (state != STATE_PROTOCOL)
	{
		return;
	}
	
	m_supportFlags = 0;
	lastmyinfo.clear();
	lastbytesshared = 0;
	lastUpdate = 0;
}

void NmdcHub::on(Line, const string& aLine) noexcept
{
	if (!ClientManager::isShutdown())
	{
		Client::on(Line(), aLine);
#ifdef IRAINMAN_INCLUDE_PROTO_DEBUG_FUNCTION
		if (BOOLSETTING(NMDC_DEBUG))
		{
			fire(ClientListener::StatusMessage(), this, "<NMDC>" + toUtf8(aLine) + "</NMDC>");
		}
#endif
		onLine(aLine);
	}
}

void NmdcHub::on(Failed, const string& aLine) noexcept
{
	clearUsers();
	Client::on(Failed(), aLine);
	updateCounts(true);
}

void NmdcHub::on(Second, uint64_t aTick) noexcept
{
	Client::on(Second(), aTick);
	
	if (state == STATE_NORMAL && (aTick > (getLastActivity() + 120 * 1000)))
	{
		send("|", 1);
	}
}

#ifdef RIP_USE_CONNECTION_AUTODETECT
void NmdcHub::RequestConnectionForAutodetect()
{
	bool bWantAutodetect = false;
	if (m_bAutodetectionPending &&
	        m_iRequestCount < MAX_CONNECTION_REQUESTS_COUNT &&
	        ClientManager::getInstance()->getMode(getHubUrl(), &bWantAutodetect) == SettingsManager::INCOMING_FIREWALL_PASSIVE && bWantAutodetect)
	{
		FastLock l(cs);
		for (auto i = m_users.cbegin(); i != m_users.cend() && m_iRequestCount < MAX_CONNECTION_REQUESTS_COUNT; ++i)
		{
			if (i->second->getIdentity().isBot() ||
			        i->second->getUser()->getFlags() & User::PASSIVE ||
			        i->first == getMyNick())
				continue;
			// TODO optimize:
			// request for connection from users with fastest connection, or operators
			connectToMe(*i->second, ExpectedMap::REASON_DETECT_CONNECTION);
			m_iRequestCount++;
		}
	}
}
#endif // RIP_USE_CONNECTION_AUTODETECT

/**
 * @file
 * $Id: nmdchub.cpp 578 2011-10-04 14:27:51Z bigmuscle $
 */
