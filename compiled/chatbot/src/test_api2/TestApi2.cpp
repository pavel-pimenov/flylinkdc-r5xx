// comiple with:
// cl TestApi2.cpp /Fe"ChatBot.dll" /O1 /MD /link /DLL /EXPORT:init

#include <windows.h>
#include <stdio.h>
#include "../ChatBotAPI.h"

struct BotInit _init;
CRITICAL_SECTION logcs;

void __stdcall OnRecvMessage2(int msgid, const WCHAR* objid, const void* param, unsigned paramsize)
{
   // first of all, log event (except user join/part/update)
   if (msgid != BotInit::RECV_UPDATE && msgid != BotInit::RECV_PART) {
      EnterCriticalSection(&logcs);
      FILE *log = fopen("v2test.log", "a+t, ccs=UTF-8");
      if (log) {
         fwprintf(log, L"RecvMessage2: id=%2d, ", msgid);
         MEMORY_BASIC_INFORMATION i;
         if (VirtualQuery(objid, &i, sizeof(i)) && (i.State == MEM_COMMIT))
            fwprintf(log, L"objid=\"%s\" ", objid);
         else
            fwprintf(log, L"objid=%08X ", objid);
         if (VirtualQuery(param, &i, sizeof(i)) && (i.State == MEM_COMMIT))
            fwprintf(log, L"param=\"%s\" ", param);
         else
            fwprintf(log, L"param=%08X ", param);
         fwprintf(log, L" paramsize=%d\n", paramsize);
         fwprintf(log, L"raw param: <");
         fwrite(param, 1, paramsize, log);
         fwprintf(log, L">\n");
         fclose(log);
      }
      LeaveCriticalSection(&logcs);
   }

   // then parse some test commands
   if (msgid != BotInit::RECV_PM_NEW && msgid != BotInit::RECV_PM) return;

   WCHAR cid[64], *msg = (WCHAR*)param, *p, *q = 0;
   WCHAR *str;
   DWORD value;

   switch (msg[0]) {
      // s <cid> <message>
      // send private message to user <cid>
      case 's':
         memcpy(cid, msg+2, 39*sizeof(WCHAR));
         cid[39] = 0;
         str = msg+2+39+1;
         _init.SendMessage2(BotInit::SEND_PM, cid, str, (wcslen(str)+1)*sizeof(WCHAR));
         break;
      // t <huburl> <message>
      // send public message to hub <huburl>
      case 't':
         p = wcschr(msg+2, ' ');
         if (!p) break;
         memcpy(cid, msg+2, (p-(msg+2))*sizeof(WCHAR));
         cid[p-(msg+2)] = 0;
         str = p+1;
         _init.SendMessage2(BotInit::SEND_CM, cid, str, (wcslen(str)+1)*sizeof(WCHAR));
         break;
      // x <cid>
      // close PM window
      case 'x':
         memcpy(cid, msg+2, 39*sizeof(WCHAR));
         cid[39] = 0;
         _init.SendMessage2(BotInit::USER_CLOSE, cid, 0, 0);
         break;
      // b <cid> <0/1>
      // ban/unban
      case 'b':
         memcpy(cid, msg+2, 39*sizeof(WCHAR));
         cid[39] = 0;
         value = msg[2+39+1]-'0';
         _init.SendMessage2(BotInit::USER_BAN, cid, &value, sizeof(value));
         break;
      // i <cid> <0/1>
      // ignore/unignore
      case 'i':
         memcpy(cid, msg+2, 39*sizeof(WCHAR));
         cid[39] = 0;
         value = msg[2+39+1]-'0';
         _init.SendMessage2(BotInit::USER_IGNORE, cid, &value, sizeof(value));
         break;
      // l <cid> <time>
      // give/remove slot
      case 'l':
         memcpy(cid, msg+2, 39*sizeof(WCHAR));
         cid[39] = 0;
         value = _wtoi(&msg[2+39+1]);
         _init.SendMessage2(BotInit::USER_SLOT, cid, &value, sizeof(value));
         break;
      case 'q':
         switch (msg[1]) {
            // qu <cid>
            // query user by CID
            case 'u':
               q = (WCHAR*)_init.QueryInfo(BotInit::QUERY_USER_BY_CID, msg+3, NULL, 0);
               break;
            // qh <url>
            // query hub by url
            case 'h':
               q = (WCHAR*)_init.QueryInfo(BotInit::QUERY_HUB_BY_URL, msg+3, NULL, 0);
               break;
            // qs
            // query self CID
            case 's':
               q = (WCHAR*)_init.QueryInfo(BotInit::QUERY_SELF, NULL, NULL, 0);
               break;
            // qc
            // query connected hubs
            case 'c':
               q = (WCHAR*)_init.QueryInfo(BotInit::QUERY_CONNECTED_HUBS, NULL, NULL, 0);
               break;
            // ql <url>
            // query hub users
            case 'l':
               q = (WCHAR*)_init.QueryInfo(BotInit::QUERY_HUB_USERS, NULL, NULL, 0);
               break;
            case '1':
            case '2':
            case '3':
            case '4':
            {
               // query transfers
               // q1-4 [<cid>]
               static int c[3] = { BotInit::QUERY_RUNNING_UPLOADS, BotInit::QUERY_QUEUED_UPLOADS, BotInit::QUERY_DOWNLOADS };
               memcpy(cid, msg+3, 39*sizeof(WCHAR)); cid[39] = 0;
               q = (WCHAR*)_init.QueryInfo(c[msg[1]-'1'], msg[2]==' '? cid : NULL, NULL, 0);
               break;
            }
         }
         EnterCriticalSection(&logcs);
         FILE *log = fopen("v2test.log", "a+t, ccs=UTF-8");
         if (log) {
            fwprintf(log, L"query result: %s\n", q? q : L"query failed!");
            fclose(log);
         }
         LeaveCriticalSection(&logcs);
         _init.FreeInfo(q);
         break;
   }
}

extern "C" __declspec(dllexport)
bool __stdcall init(BotInit* _init)
{
   if (_init->apiVersion < 2) return false;
   _init->botId = "Api2Test";
   _init->botVersion = "1.0";
   _init->RecvMessage2 = OnRecvMessage2;
   memcpy(&::_init, _init, sizeof(BotInit));
   return true;
}

BOOL WINAPI DllMain(
  HINSTANCE hinstDLL,  // handle to DLL module
  DWORD fdwReason,     // reason for calling function
  LPVOID lpvReserved   // reserved
)
{
   if (fdwReason == DLL_PROCESS_ATTACH)
      InitializeCriticalSection(&logcs);
   if (fdwReason == DLL_PROCESS_DETACH)
      DeleteCriticalSection(&logcs);
   return TRUE;
}
