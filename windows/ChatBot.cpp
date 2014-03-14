
/*
 * ApexDC speedmod (c) SMT 2007
 */

#include "stdafx.h"

#include "../client/ClientManager.h"
#include "../client/FavoriteUser.h"
#include "../client/UploadManager.h"
#include "ChatBot.h"
#include "PrivateFrame.h"

HINSTANCE ChatBot::g_ChatBotDll = nullptr;

ChatBot::ChatBot() : m_qrycount(0)
{
	m_init.apiVersion = 2;
	m_init.appName = APPNAME
#ifdef FLYLINKDC_HE
	                 "HE"
#endif
	                 ;
	m_init.appVersion = A_VERSIONSTRING;
	m_init.appConfigPath = Util::getConfigPath().c_str(); // [+] IRainman
	m_init.botId = nullptr;
	m_init.botVersion = nullptr;
	m_init.SendMessage = botSendMessage;
	m_init.RecvMessage = nullptr;
	m_init.SendMessage2 = botSendMessage2;
	m_init.RecvMessage2 = nullptr;
	m_init.QueryInfo = botQueryInfo_rc;
	m_init.FreeInfo = botFreeInfo;
	
	g_ChatBotDll = ::LoadLibrary(_T("ChatBot.dll"));
	if (g_ChatBotDll)
	{
		BotInit::tInit initproc = (BotInit::tInit)GetProcAddress(g_ChatBotDll, "init");
		if (initproc)
		{
			if (!initproc(&m_init))
				m_init.RecvMessage = nullptr;
		}
	}
}

ChatBot::~ChatBot()
{
	if (g_ChatBotDll)
	{
		::FreeLibrary(g_ChatBotDll);
		g_ChatBotDll = nullptr;
	}
}

void ChatBot::botSendMessage(const WCHAR *params, const WCHAR *message)
{
	const WCHAR *cid = wcsstr(params, L"CID=");
	if (cid)
	{
		botSendMessage2(BotInit::SEND_PM, cid + 4, message, (wcslen(message) + 1)*sizeof(WCHAR)); //-V112 //-V107
	}
}

bool ChatBot::botSendMessage2(int msgid, const WCHAR* objid, const void *param, unsigned /*paramsize*/)
{
	switch (msgid)
	{
		case BotInit::SEND_CM:
		{
			Client* client = ClientManager::getInstance()->findClient(Text::fromT((WCHAR*)objid));
			if (client)
			{
				client->hubMessage(Text::fromT((WCHAR*)param));
				return true;
			}
			return false;
		}
		case BotInit::DL_MAGNET:
			// todo: fixme
			return false;
	}
	
	const UserPtr user = crateUser(objid);
	
	if (user)
	{
		switch (msgid)
		{
			case BotInit::SEND_PM:
				ClientManager::getInstance()->privateMessage(HintedUser(user, Util::emptyString), Text::fromT((WCHAR*)param), false);
				return true;
			case BotInit::USER_CLOSE:
				/* TODO //in MainFrame: needs only if simple PM frame, move to PrivateFrame.
				if(m_bIsPM && m_bTrayIcon == true) {
				    setIcon(m_normalicon);
				    bIsPM = false;
				}
				*/
				return PrivateFrame::closeUser(user);
			case BotInit::USER_IGNORE:
				FavoriteManager::getInstance()->addFavoriteUser(user);
				FavoriteManager::getInstance()->setIgnorePM(user, (param && ((DWORD)param == 1) || *(DWORD*)param));
				return true;
			case BotInit::USER_BAN:
				FavoriteManager::getInstance()->addFavoriteUser(user);
				FavoriteManager::getInstance()->setUploadLimit(user, (param && ((DWORD)param == 1) || *(DWORD*)param) ? FavoriteUser::UL_BAN : FavoriteUser::UL_NONE);
				return true;
			case BotInit::USER_SLOT:
				if (param)
				{
					UploadManager::getInstance()->reserveSlot(HintedUser(user, Util::emptyString), (uint64_t)(param ? * (uint64_t*)param : 0));
				}
				else
				{
					UploadManager::getInstance()->unreserveSlot(HintedUser(user, Util::emptyString));
				}
				return true;
		}
	}
	return false;
}

void* ChatBot::botQueryInfo_rc(int qryid, const WCHAR* objid, const void *param, unsigned paramsize)
{
	void* res = ChatBot::getInstance()->botQueryInfo(qryid, objid, param, paramsize);
	if (res)
	{
		ChatBot::getInstance()->m_qrycount++;
	}
	return res;
}

void ChatBot::onUserAction(BotInit::CODES c, const UserPtr& aUser)
{
	dcassert(aUser); // [+] PPA, [!] IRainman: replace the condition to diagnose because the code is called from places where there can not be of empty users, if they are there, it's critical defect!
	if (m_init.RecvMessage2)
	{
		try
		{
			m_init.RecvMessage2(c, Text::toT(aUser->getCID().toBase32()).c_str(), nullptr, 0); // https://www.box.com/shared/c0b34dc7f52b7775a44b
		}
		catch (const Exception&)
		{
			externalFailure();
		}
	}
}

void ChatBot::onHubAction(BotInit::CODES c, const string& hubUrl)
{
	dcassert(!hubUrl.empty());
	if (m_init.RecvMessage2)
	{
		try
		{
			m_init.RecvMessage2(c, Text::toT(hubUrl).c_str(), nullptr, 0);
		}
		catch (const Exception&)
		{
			externalFailure();
		}
	}
}

WCHAR* ChatBot::onQueryUserByCid(const WCHAR* cid)
{
	const UserPtr user = crateUser(cid);
	if (!user)
		return nullptr;
		
	ParamSet ps;
	ps.addVariable(L"CID", cid);
	ps.addVariable(L"NICK", user->getLastNickT().c_str());
	
	{
		ClientManager::LockInstanceOnlineUsers lockedInstance;
		const OnlineUser* ou = lockedInstance->getOnlineUserL(user);
		if (!ou)
			return nullptr;
			
		const Identity& id = ou->getIdentity();
		const string& ip = id.getIpAsString();
		
		ps.addVariable(L"IP", Text::toT(ip).c_str());
#ifdef PPA_INCLUDE_DNS
		ps.addVariable(L"DNS", Text::toT(Socket::nslookup(ip)).c_str());
#endif
		ps.addVariable(L"DESC", Text::toT(id.getDescription()).c_str());
		ps.addVariable(L"OP", id.isOp() ? L"1" : L"0");
		ps.addVariable(L"BOT", id.isBot() ? L"1" : L"0");
		ps.addVariable(L"AWAY", user->isAway() ? L"1" : L"0");
		ps.addVariable(L"SLOTS", Util::toStringW(id.getSlots()).c_str());
		ps.addVariable(L"LIMIT", Util::toStringW(id.getLimit()).c_str());
		ps.addVariable(L"EXACTSHARE", Util::toStringW(id.getBytesShared()).c_str());
		ps.addVariable(L"SHARE", Util::formatBytesW(id.getBytesShared()).c_str());
		ps.addVariable(L"HUBURL", Text::toT(ou->getClient().getHubUrl()).c_str());
	}
	
	FavoriteUser::MaskType l_flags;
	int l_ul;
	const bool isFav = FavoriteManager::getInstance()->getFavUserParam(user, l_flags, l_ul);
	ps.addVariable(L"ISFAV", isFav ? L"1" : L"0");
	if (isFav)
	{
		// TODO: add all fav user param
		ps.addVariable(L"FAVSLOT", FavoriteManager::hasAutoGrantSlot(l_flags) ? L"1" : L"0");
		ps.addVariable(L"FAVBAN", FavoriteManager::hasUploadBan(l_ul) ? L"1" : L"0");
		ps.addVariable(L"FAVIGNORE", FavoriteManager::hasIgnorePM(l_flags) ? L"1" : L"0");
	}
	return ps.cutParams();
}

WCHAR* ChatBot::onQueryHubByUrl(const WCHAR* huburl)
{
	Client* c = ClientManager::getInstance()->findClient(Text::fromT(huburl));
	if (c)
	{
		ParamSet ps;
		ps.addVariable(L"HUBURL", huburl);
		ps.addVariable(L"HUBNAME", Text::toT(c->getHubName()).c_str());
		ps.addVariable(L"HUBDESC", Text::toT(c->getHubDescription()).c_str());
		ps.addVariable(L"IP", Text::toT(c->getIpAsString()).c_str());
		ps.addVariable(L"PORT", Util::toStringW(c->getPort()).c_str());
		return ps.cutParams();
	}
	return nullptr;
}

WCHAR* ChatBot::onQueryConnectedHubs()
{
	ParamSet l_ps;
#ifdef IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
	{
		ClientManager::LockInstanceClients lockedInstanceClients;
		const auto& l = lockedInstanceClients->getClientsL();
		for (auto i = l.cbegin(); i != l.cend(); ++i)
		{
			if ((*i).second->isConnected())
			{
				l_ps.addValue(Text::toT((*i).second->getHubUrl()).c_str());
			}
		}
	}
#else
	StringList l_hubs;
	ClientManager::getConnectedHubUrls(l_hubs);
	for (auto i = l_hubs.cbegin(); i != l_hubs.cend(); ++i)
	{
		l_ps.addValue(Text::toT((*i)).c_str());
	}
#endif // IRAINMAN_NON_COPYABLE_CLIENTS_IN_CLIENT_MANAGER
	return l_ps.cutParams();
}

WCHAR* ChatBot::onQueryHubUsers(const WCHAR* /* huburl */)
{
	// todo: fixme
	return nullptr;
}

WCHAR* ChatBot::onQueryRunningUploads(const WCHAR* /* cid */)
{
	// todo: fixme
	return nullptr;
}
UserPtr ChatBot::crateUser(const WCHAR* cid)
{
	return cid ? ClientManager::getUser(CID(Text::fromT(cid)), true) : nullptr;
}
WCHAR* ChatBot::onQueryDownloads(const WCHAR* cid)
{
	const UserPtr user = crateUser(cid);
	if (user)
	{
		ParamSet ps;
		RLock l(*QueueItem::g_cs);
		QueueManager::LockFileQueueShared l_fileQueue;
		const auto& downloads = l_fileQueue.getQueueL();
		for (auto j = downloads.cbegin(); j != downloads.cend(); ++j)
		{
			const QueueItemPtr& qi = j->second;
			const bool src = qi->isSourceL(user);
			const bool badsrc = src ? false : qi->isBadSourceL(user);
			
			if (!src && !badsrc)
				continue;
				
			ps.addVariable(L"FILENAME", Text::toT(qi->getTarget()).c_str());
			ps.addVariable(L"FILESIZE", Util::toStringW(qi->getSize()).c_str());
			ps.addVariable(L"DOWNLOADED", Util::toStringW(qi->getDownloadedBytes()).c_str());
			ps.addVariable(L"ISBADSRC", badsrc ? L"1" : L"0");
			ps.addVariable(L"STATUS", qi->isRunningL() ? L"RUN" : L"WAIT");
			ps.addVariable(L"PRIORITY", Util::toStringW(qi->getPriority()).c_str());
			ps.addValue();
		}
		return ps.cutParams();
	}
	return nullptr;
}

WCHAR* ChatBot::onQueryQueuedUploads(const WCHAR* cid)
{
	const UserPtr user = crateUser(cid);
	if (user)
	{
		ParamSet ps;
		UploadManager::LockInstanceQueue lockedInstance;
		const auto& users = lockedInstance->getUploadQueueL();
		for (auto uit = users.cbegin(); uit != users.cend(); ++uit)
		{
			if (uit->getUser() == user)
			{
				for (auto i = uit->m_waiting_files.cbegin(); i != uit->m_waiting_files.cend(); ++i)
				{
					ps.addVariable(L"CID", Text::toT(uit->getUser()->getCID().toBase32()).c_str());
					ps.addVariable(L"FILENAME", Text::toT((*i)->getFile()).c_str());
					ps.addVariable(L"FILESIZE", Util::toStringW((*i)->getSize()).c_str());
					ps.addVariable(L"POS", Util::toStringW((*i)->getPos()).c_str());
					ps.addVariable(L"TIME", Util::toStringW(GET_TIME() - (*i)->getTime()).c_str());
					ps.addValue();
				}
			}
		}
		return ps.cutParams();
	}
	return nullptr;
}

void* ChatBot::botQueryInfo(int qryid, const WCHAR* objid, const void* /*param*/, unsigned /*paramsize*/)
{
	switch (qryid)
	{
		case BotInit::QUERY_USER_BY_CID:
			return onQueryUserByCid((const WCHAR*)objid);
		case BotInit::QUERY_HUB_BY_URL:
			return onQueryHubByUrl((const WCHAR*)objid);
		case BotInit::QUERY_CONNECTED_HUBS:
			return onQueryConnectedHubs();
		case BotInit::QUERY_HUB_USERS:
			return onQueryHubUsers((const WCHAR*)objid);
		case BotInit::QUERY_RUNNING_UPLOADS:
			return onQueryRunningUploads((const WCHAR*)objid);
		case BotInit::QUERY_QUEUED_UPLOADS:
			return onQueryQueuedUploads((const WCHAR*)objid);
		case BotInit::QUERY_DOWNLOADS:
			return onQueryDownloads((const WCHAR*)objid);
		case BotInit::QUERY_SELF:
		{
			const string selfcid = ClientManager::getMyCID().toBase32(); // [!] IRainman fix.
			int sz = (selfcid.length() + 1) * sizeof(WCHAR);
			WCHAR* info = (WCHAR*)malloc(sz);
			memcpy(info, Text::toT(selfcid).c_str(), sz);
			return info;
		}
	}
	return nullptr;
}

void  ChatBot::botFreeInfo(void *info)
{
	if (info)
	{
		ChatBot::getInstance()->m_qrycount--;
	}
	free(info);
}

void ChatBot::onMessage(const string& huburl, const Identity& /*msgFrom*/, const string& message)
{
	if (m_init.RecvMessage2)
	{
		try
		{
			const tstring msg = Text::toT(message);
			m_init.RecvMessage2(BotInit::RECV_CM, Text::toT(huburl).c_str(), msg.c_str(), (msg.length() + 1)*sizeof(WCHAR));
		}
		catch (const Exception&)
		{
			externalFailure();
		}
	}
}

void ChatBot::onMessage(const Identity& myId, const Identity& msgFrom, const tstring& message, bool newSession)
{
	// [!] IRainman fix: NG core uses clean messages!
	// [-] tstring::size_type pos = message.find(_T("> "));
	// [-] if (pos == tstring::npos) return;
	// [-] tstring realmessage = message.substr(pos + 2);
	
	if (m_init.RecvMessage)
	{
		onMessageV1(myId, msgFrom, message, newSession);
	}
	if (m_init.RecvMessage2)
	{
		onMessageV2(myId, msgFrom, message, newSession);
	}
}

void ChatBot::externalFailure()
{
	// stop further notifications
	m_init.RecvMessage = nullptr;
	m_init.RecvMessage2 = nullptr;
	ClientManager::getInstance()->privateMessage(HintedUser(ClientManager::getMe_UseOnlyForNonHubSpecifiedTasks(), Util::emptyString), "ChatBot died!", false);
}

void ChatBot::onMessageV1(const Identity& myId, const Identity& msgFrom, const tstring& message, bool newSession)
{
	ParamSet ps;
	ps.addVariable(L"CID", Text::toT(msgFrom.getUser()->getCID().toBase32()).c_str());
	ps.addVariable(L"NICK", msgFrom.getNickT().c_str());
	
	const auto& ip = msgFrom.getIpAsString();
	ps.addVariable(L"IP", Text::toT(ip).c_str());
#ifdef PPA_INCLUDE_DNS
	ps.addVariable(L"DNS", Text::toT(Socket::nslookup(ip)).c_str());
#endif
	ps.addVariable(L"DESC", Text::toT(msgFrom.getDescription()).c_str());
	ps.addVariable(L"OP", msgFrom.isOp() ? L"1" : L"0");
	ps.addVariable(L"BOT", msgFrom.isBot() ? L"1" : L"0");
	ps.addVariable(L"AWAY", msgFrom.getUser()->isAway() ? L"1" : L"0");
	ps.addVariable(L"SLOTS", Util::toStringW(msgFrom.getSlots()).c_str());
	ps.addVariable(L"LIMIT", Util::toStringW(msgFrom.getLimit()).c_str());
	ps.addVariable(L"EXACTSHARE", Util::toStringW(msgFrom.getBytesShared()).c_str());
	ps.addVariable(L"SHARE", Util::formatBytesW(msgFrom.getBytesShared()).c_str());
	ps.addVariable(L"MYNICK", myId.getNickT().c_str());
	ps.addVariable(L"MYSLOTS", Util::toStringW(myId.getSlots()).c_str());
	ps.addVariable(L"MYLIMIT", Util::toStringW(myId.getLimit()).c_str());
	ps.addVariable(L"MYEXACTSHARE", Util::toStringW(myId.getBytesShared()).c_str());
	ps.addVariable(L"MYSHARE", Util::formatBytesW(myId.getBytesShared()).c_str());
	ps.addVariable(L"MYAWAY", Util::getAway() ? L"1" : L"0");
	/* TODO
	ps.addVariable(L"HUBURL", Text::toT(client.getHubUrl()).c_str());
	ps.addVariable(L"HUBNAME", Text::toT(client.getHubName()).c_str());
	ps.addVariable(L"HUBDESC", Text::toT(client.getHubDescription()).c_str());
	*/
	FavoriteUser::MaskType l_flags;
	int l_ul;
	const bool isFav = FavoriteManager::getInstance()->getFavUserParam(msgFrom.getUser(), l_flags, l_ul);
	ps.addVariable(L"ISFAV", isFav ? L"1" : L"0");
	if (isFav)
	{
		ps.addVariable(L"FAVSLOT", FavoriteManager::hasAutoGrantSlot(l_flags) ? L"1" : L"0");
		ps.addVariable(L"FAVBAN", FavoriteManager::hasUploadBan(l_ul) ? L"1" : L"0");
		ps.addVariable(L"FAVIGNORE", FavoriteManager::hasIgnorePM(l_flags) ? L"1" : L"0");
	}
	ps.addVariable(L"NEW", Util::toStringW((int)newSession).c_str());
	ps.addValue(L"PRIVATE=1");
	
	try
	{
		m_init.RecvMessage(ps.getParams(), message.c_str());
	}
	catch (const Exception&)
	{
		externalFailure();
	}
}

void ChatBot::onMessageV2(const Identity& myId, const Identity& msgFrom, const tstring& message, bool newSession)
{
	try
	{
		m_init.RecvMessage2(newSession ? BotInit::RECV_PM_NEW : BotInit::RECV_PM, Text::toT(msgFrom.getUser()->getCID().toBase32()).c_str(), message.c_str(), (message.length() + 1)*sizeof(WCHAR));
	}
	catch (const Exception&)
	{
		externalFailure();
	}
}

ParamSet::ParamSet() : m_bufUsed(0)
{
	const auto pageSize = CompatibilityManager::getPageSize();
	m_buf = (WCHAR*)malloc(pageSize);
	m_bufSize = pageSize / sizeof(WCHAR);
}

ParamSet::~ParamSet()
{
	free(m_buf);
}

void ParamSet::addStr(const WCHAR* str)
{
	size_t sz = 0;
	for (size_t i = 0; str[i]; i++, sz++)
		if (str[i] == '|' || str[i] == '\\')
			sz++;
	if (m_bufUsed + sz >= m_bufSize)
		m_buf = (WCHAR*)realloc(m_buf, (m_bufSize = m_bufSize * 2 + sz + 1) * sizeof(WCHAR));
	for (size_t j = 0; str[j]; j++)
	{
		if (str[j] == '|' || str[j] == '\\')
			m_buf[m_bufUsed++] = '\\';
		m_buf[m_bufUsed++] = str[j];
	}
}

void ParamSet::putStr(const WCHAR* str, size_t sz)
{
	if (m_bufUsed + sz >= m_bufSize)
		m_buf = (WCHAR*)realloc(m_buf, (m_bufSize = m_bufSize * 2 + sz + 1) * sizeof(WCHAR));
	if (sz == 1)
		m_buf[m_bufUsed] = *str;
	else
		memcpy(m_buf + m_bufUsed, str, sz * sizeof(WCHAR));
	m_bufUsed += sz;
}

void ParamSet::addVariable(const WCHAR *varName, const WCHAR *value)
{
	putStr(varName, wcslen(varName));
	putStr(L"=", 1);
	addStr(value);
	putStr(L"|", 1);
}

void ParamSet::addValue(const WCHAR *value)
{
	if (value)
	{
		addStr(value);
	}
	putStr(L"|", 1);
}

WCHAR* ParamSet::getParams()
{
	m_buf[m_bufUsed] = '\0';
	return m_buf;
}

WCHAR* ParamSet::cutParams()
{
	m_buf[m_bufUsed] = '\0';
	WCHAR *res = m_buf;
	m_buf = nullptr;
	m_bufSize = m_bufUsed = 0;
	return res;
}
