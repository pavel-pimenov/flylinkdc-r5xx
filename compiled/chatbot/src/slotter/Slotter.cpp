// comiple with:
// cl TestApi2.cpp /Fe"ChatBot.dll" /O1 /MD /link /DLL /EXPORT:init

#include <windows.h>
#include <stdio.h>
#include "../ChatBotAPI.h"

struct BotInit _init;

void __stdcall OnRecvMessage2(int msgid, const WCHAR* objid, const void* param, unsigned paramsize)
{
   if (msgid != BotInit::RECV_PM && msgid != BotInit::RECV_PM_NEW)
      return;

   WCHAR* msg = NULL;

   if (wcsstr((WCHAR*)param, L"slot")) {
      msg = L"Slot granted";
   } else if (wcsstr((WCHAR*)param, L"סכמע")) {
      msg = L"ֲûהאם סכמע";
   }

   DWORD slottime = 10*60;
   if (msg) {
      _init.SendMessage2(BotInit::USER_SLOT, objid, &slottime, sizeof(slottime));
   } else
      msg = L"I'm a bot. Say 'give me slot, please' if you wanna slot";

   _init.SendMessage2(BotInit::SEND_PM, objid, msg, (wcslen(msg)+1)*sizeof(WCHAR));
}

extern "C" __declspec(dllexport)
bool __stdcall init(BotInit* _init)
{
   if (_init->apiVersion < 2) return false;
   _init->botId = "Slotter";
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
   return TRUE;
}
