#ifndef CHAT_BOT_API_H
#define CHAT_BOT_API_H
struct BotInit
{
	typedef bool (__stdcall * tInit)(struct BotInit *);
	
	typedef void (__stdcall * tSendMessage)(const WCHAR *params, const WCHAR *message);
	typedef void (__stdcall * tRecvMessage)(const WCHAR *params, const WCHAR *message);
	
	typedef bool (__stdcall * tSendMessage2)(int msgid, const WCHAR* objid, const void* param, unsigned paramsize);
	typedef void (__stdcall * tRecvMessage2)(int msgid, const WCHAR* objid, const void* param, unsigned paramsize);
	
	typedef void*(__stdcall * tQueryInfo)(int queryid, const WCHAR* objid, const void *param, unsigned paramsize);
	typedef void (__stdcall * tFreeInfo)(void *info);
	
	DWORD apiVersion;
	char* appName;
	char* appVersion;
	tSendMessage SendMessage;
	tRecvMessage RecvMessage;
	char* botId;
	char* botVersion;
	tSendMessage2 SendMessage2;
	tRecvMessage2 RecvMessage2;
	tQueryInfo QueryInfo;
	tFreeInfo FreeInfo;
	
	enum CODES
	{
		SEND_PM         = 0,
		SEND_CM         = 1,
		USER_CLOSE      = 2,
		USER_IGNORE     = 3,
		USER_BAN        = 4,
		USER_SLOT       = 5,
		DL_MAGNET       = 6,
		
		RECV_PM_NEW     = 40,
		RECV_PM         = 41,
		RECV_CM         = 42,
		//RECV_JOIN     = 43,
		RECV_PART       = 44,
		RECV_UPDATE     = 45,
		RECV_CONNECT    = 46,
		RECV_DISCONNECT = 47,
		
		QUERY_USER_BY_CID       = 80,
		QUERY_HUB_BY_URL        = 81,
		QUERY_CONNECTED_HUBS    = 82,
		QUERY_HUB_USERS         = 83,
		QUERY_SELF              = 84,
		QUERY_RUNNING_UPLOADS   = 85,
		QUERY_QUEUED_UPLOADS    = 86,
		QUERY_DOWNLOADS         = 87,
		
		LAST
	};
	const char* appConfigPath; // [+]IRainman
};
#endif // CHAT_BOT_API_H
