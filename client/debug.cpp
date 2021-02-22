#include "stdinc.h"
#include <tchar.h>

#ifdef _WIN32
void DumpDebugMessage(const TCHAR *filename, const char *msg, size_t msgSize, bool appendNL)
{
	TCHAR path[MAX_PATH + 64];
	DWORD result = GetModuleFileName(NULL, path, _countof(path));
	if (!result || result >= MAX_PATH) return;
	TCHAR *delim = _tcsrchr(path, _T('\\'));
	if (!delim) return;
	_tcscpy(delim + 1, filename);
	HANDLE file = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) return;
	SetFilePointer(file, 0, NULL, FILE_END);
	WriteFile(file, msg, msgSize, &result, NULL);
	if (appendNL)
	{
		char c = '\n';
		WriteFile(file, &c, 1, &result, NULL);
	}
	CloseHandle(file);
}
#endif
