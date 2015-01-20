
/*
 * ApexDC speedmod (c) SMT 2007
 */


#if !defined(CHAT_BOT_H)
#define CHAT_BOT_H

#pragma once

#include "../client/User.h"
#include "ChatBotAPI.h"

// !SMT!-CB
class ChatBot : public Singleton<ChatBot>
{
	public:
		ChatBot();
		~ChatBot();
		
		void onMessage(const Identity& myId, const Identity& msgFrom, const tstring& message, bool newSession);
		void onMessage(const string& huburl, const Identity& msgFrom, const string& message);
		void onUserAction(BotInit::CODES, const UserPtr& aUser);
		void onHubAction(BotInit::CODES, const string& hubUrl);
		static bool isLoaded()
		{
			return g_ChatBotDll != nullptr;
		}
	private:
		static HINSTANCE g_ChatBotDll;
		BotInit m_init;
		int m_qrycount;
		
		void onMessageV1(const Identity& myId, const Identity& msgFrom, const tstring& message, bool newSession);
		void onMessageV2(const Identity& myId, const Identity& msgFrom, const tstring& message, bool newSession);
		void* botQueryInfo(int qryid, const WCHAR* objid, const void *param, unsigned paramsize);
		WCHAR* onQueryUserByCid(const WCHAR* cid);
		WCHAR* onQueryHubByUrl(const WCHAR* huburl);
		WCHAR* onQueryConnectedHubs();
		WCHAR* onQueryHubUsers(const WCHAR* huburl);
		WCHAR* onQueryRunningUploads(const WCHAR* cid);
		WCHAR* onQueryQueuedUploads(const WCHAR* cid);
		WCHAR* onQueryDownloads(const WCHAR* cid);
		void externalFailure();
		
		static void  __stdcall botSendMessage(const WCHAR *params, const WCHAR *message);
		static bool  __stdcall botSendMessage2(int msgid, const WCHAR* objid, const void *param, unsigned paramsize);
		static void* __stdcall botQueryInfo_rc(int qryid, const WCHAR* objid, const void *param, unsigned paramsize);
		static void  __stdcall botFreeInfo(void *info);
		
	private:
		static UserPtr crateUser(const WCHAR* cid);
};


class ParamSet
#ifdef _DEBUG
	: public boost::noncopyable
#endif
{
	public:
		void addVariable(const WCHAR* varName, const WCHAR* value);
		void addValue(const WCHAR* value = nullptr);
		WCHAR* getParams();
		WCHAR* cutParams();
		ParamSet();
		~ParamSet();
	private:
		void addStr(const WCHAR* str);
		void putStr(const WCHAR* str, size_t sz);
		WCHAR *m_buf;
		size_t m_bufSize, m_bufUsed;
};


#endif // CHAT_BOT_H
