/**
 * This file is a part of client manager.
 * It has been divided but shouldn't be used anywhere else.
 */

void sendRawCommand(const OnlineUser& ou, const int aRawCommand)
{
	string rawCommand = ou.getClient().getRawCommand(aRawCommand);
	if (!rawCommand.empty())
	{
		StringMap ucParams;
		
		UserCommand uc = UserCommand(0, 0, 0, 0, "", rawCommand, "", "");
		userCommand(HintedUser(ou.getUser(), ou.getClient().getHubUrl()), uc, ucParams, true);
	}
}

void setListLength(const UserPtr& p, const string& listLen)
{
	SharedLock l(g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		i->second->getIdentity().setStringParam("LL", listLen);
	}
}

void fileListDisconnected(const UserPtr& p)
{
	string report;
	Client* c = nullptr;
	{
		SharedLock l(g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end()
#ifdef STRONG_USE_DHT
		        && i->second->getClientBase().type != ClientBase::DHT
#endif
		   )
		{
			OnlineUser* ou = i->second;
			auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou->getIdentity()' expression repeatedly. cheatmanager.h 43
			
			auto fileListDisconnects = id.incFileListDisconnects(); // 8 бит не мало?
			
			if (SETTING(ACCEPTED_DISCONNECTS) == 0)
				return;
				
			if (fileListDisconnects == SETTING(ACCEPTED_DISCONNECTS))
			{
				c = &ou->getClient();
				report = id.setCheat(ou->getClientBase(), "Disconnected file list " + Util::toString(fileListDisconnects) + " times", false);
				getInstance()->sendRawCommand(*ou, SETTING(DISCONNECT_RAW));
			}
		}
	}
	if (c && !report.empty() && BOOLSETTING(DISPLAY_CHEATS_IN_MAIN_CHAT))
	{
		c->cheatMessage(report);
	}
}

void connectionTimeout(const UserPtr& p)
{
	string report;
	bool remove = false;
	Client* c = nullptr;
	{
		SharedLock l(g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i != g_onlineUsers.end()
#ifdef STRONG_USE_DHT
		        && i->second->getClientBase().type != ClientBase::DHT
#endif
		   )
		{
			OnlineUser& ou = *i->second;
			auto& id = ou.getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou.getIdentity()' expression repeatedly. cheatmanager.h 80
			
			auto connectionTimeouts = id.incConnectionTimeouts(); // 8 бит не мало?
			
			if (SETTING(ACCEPTED_TIMEOUTS) == 0)
				return;
				
			if (connectionTimeouts == SETTING(ACCEPTED_TIMEOUTS))
			{
				c = &ou.getClient();
				report = id.setCheat(ou.getClientBase(), "Connection timeout " + Util::toString(connectionTimeouts) + " times", false);
				remove = true;
				sendRawCommand(ou, SETTING(TIMEOUT_RAW));
			}
		}
	}
	if (remove)
	{
		/*try
		{
		    // TODO: remove user check
		}
		catch (...)
		{
		}*/
	}
	if (c && !report.empty() && BOOLSETTING(DISPLAY_CHEATS_IN_MAIN_CHAT))
	{
		c->cheatMessage(report);
	}
}

void checkCheating(const UserPtr& p, DirectoryListing* dl)
{
	Client* client;
	string report;
	OnlineUserPtr ou;
	{
		SharedLock l(g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end()
#ifdef STRONG_USE_DHT
		        || i->second->getClientBase().type == ClientBase::DHT
#endif
		   )
			return;
			
		ou = i->second;
		auto& id = ou->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'ou->getIdentity()' expression repeatedly. cheatmanager.h 127
		
		const int64_t statedSize = id.getBytesShared();
		const int64_t realSize = dl->getTotalSize();
		
		const double multiplier = (100 + double(SETTING(PERCENT_FAKE_SHARE_TOLERATED))) / 100;
		const int64_t sizeTolerated = (int64_t)(realSize * multiplier);
		id.setRealBytesShared(realSize);
		
		if (statedSize > sizeTolerated)
		{
			id.setFakeCardBit(Identity::BAD_LIST | Identity::CHECKED, true);
			string detectString = STRING(CHECK_MISMATCHED_SHARE_SIZE) + " - ";
			if (realSize == 0)
			{
				detectString += STRING(CHECK_0BYTE_SHARE);
			}
			else
			{
				const double qwe = double(statedSize) / double(realSize);
				char buf[128];
				buf[0] = 0;
				snprintf(buf, _countof(buf), CSTRING(CHECK_INFLATED), Util::toString(qwe).c_str()); //-V111
				detectString += buf;
			}
			detectString += STRING(CHECK_SHOW_REAL_SHARE);
			
			report = id.setCheat(ou->getClientBase(), detectString, false);
			sendRawCommand(*ou.get(), SETTING(FAKESHARE_RAW));
		}
		else
		{
			id.setFakeCardBit(Identity::CHECKED, true);
		}
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
		if (report.empty())
			report = id.updateClientType(*ou);
		else
#endif
			id.updateClientType(*ou);
			
		client = &(ou->getClient());
	}
	client->updated(ou);
	
	if (!report.empty() && BOOLSETTING(DISPLAY_CHEATS_IN_MAIN_CHAT))
		client->cheatMessage(report);
}

void setClientStatus(const UserPtr& p, const string& aCheatString, const int aRawCommand, bool aBadClient)
{
	Client* client;
	OnlineUserPtr ou;
	string report;
	{
		SharedLock l(g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end()
#ifdef STRONG_USE_DHT
		        || i->second->getClientBase().type == ClientBase::DHT
#endif
		   )
			return;
			
		ou = i->second;
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
		report = ou->getIdentity().updateClientType(*ou);
#else
		ou->getIdentity().updateClientType(*ou);
#endif
		
		
		if (!aCheatString.empty())
		{
			report += ou->getIdentity().setCheat(ou->getClientBase(), aCheatString, aBadClient);
		}
		if (aRawCommand != -1)
			sendRawCommand(*ou.get(), aRawCommand);
			
		client = &(ou->getClient());
	}
	client->updated(ou);
	if (!report.empty() && BOOLSETTING(DISPLAY_CHEATS_IN_MAIN_CHAT))
		client->cheatMessage(report);
}

void setPkLock(const UserPtr& p
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
               , const string& aPk, const string& aLock
#endif
              )
{
	Client *client; // !SMT!-fix
	OnlineUserPtr ou;
	{
		SharedLock l(g_csOnlineUsers);
		OnlineIterC i = g_onlineUsers.find(p->getCID());
		if (i == g_onlineUsers.end())
			return;
		ou = i->second;
#ifdef IRAINMAN_INCLUDE_PK_LOCK_IN_IDENTITY
		ou->getIdentity().setStringParam("PK", aPk);
		ou->getIdentity().setStringParam("LO", aLock);
#endif
		client = &(ou->getClient()); // !SMT!-fix
	}
	client->updated(ou);
}

void setSupports(const UserPtr& p, StringList && aSupports, const uint8_t knownUcSupports) // [!] IRainamn fix: http://code.google.com/p/flylinkdc/issues/detail?id=1112
{
	SharedLock l(g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		// [!] IRainman fix.
		auto& id = i->second->getIdentity();
		id.setKnownUcSupports(knownUcSupports);
		/*
		if (p->isNMDC())
		{
		    NmdcSupports::setSupports(id, move(aSupports));
		}
		else
		*/
		{
			AdcSupports::setSupports(id, move(aSupports));
		}
		// [~] IRainman fix.
	}
}
#ifdef IRAINMAN_INCLUDE_DETECTION_MANAGER
void setGenerator(const UserPtr& p, const string& aGenerator)
{
	SharedLock l(g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
		i->second->getIdentity().setStringParam("GE", aGenerator);
}
#endif
void setUnknownCommand(const UserPtr& p, const string& aUnknownCommand)
{
	SharedLock l(g_csOnlineUsers);
	OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
		i->second->getIdentity().setStringParam("UC", aUnknownCommand);
}

void reportUser(const HintedUser& user)
{
	const bool priv = FavoriteManager::getInstance()->isPrivate(user.hint);
	string report;// [+] FlylinkDC report
	Client* client; // [+] IRainman fix
	{
		SharedLock l(g_csOnlineUsers);
		OnlineUser* ou = findOnlineUserL(user.user->getCID(), user.hint, priv);
		if (!ou
#ifdef STRONG_USE_DHT
		        || ou->getClientBase().type == ClientBase::DHT
#endif
		   )
			return;
			
		ou->getIdentity().getReport(report);// [+] FlylinkDC report
		client = &(ou->getClient()); // [!] IRainman fix
		
		// [-] IRainman fix
		// ou->getClient().reportUser(ou->getIdentity());
	}
	client->reportUser(report); // [+] IRainman fix
}

// [+] FlylinkDC
void setFakeList(const UserPtr& p, const string& aCheatString)
{
	SharedLock l(g_csOnlineUsers);
	const OnlineIterC i = g_onlineUsers.find(p->getCID());
	if (i != g_onlineUsers.end())
	{
		auto& id = i->second->getIdentity(); // [!] PVS V807 Decreased performance. Consider creating a pointer to avoid using the 'i->second' expression repeatedly. cheatmanager.h 285
		id.setCheat(i->second->getClient(), aCheatString, false);
	}
}