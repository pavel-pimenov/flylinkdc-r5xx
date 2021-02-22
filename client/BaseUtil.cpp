#include "stdinc.h"
#include "BaseUtil.h"
#include "StrUtil.h"

const string BaseUtil::emptyString;
const wstring BaseUtil::emptyStringW;
const tstring BaseUtil::emptyStringT;
const vector<uint8_t> BaseUtil::emptyByteVector;

string BaseUtil::translateError(DWORD error)
{
#ifdef _WIN32
	LPCVOID lpSource = nullptr;
	LPTSTR lpMsgBuf = 0;
	DWORD chars = FormatMessage(
	                  FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                  lpSource,
	                  error,
#if defined (_CONSOLE) || defined (_DEBUG)
	                  MAKELANGID(LANG_NEUTRAL, SUBLANG_ENGLISH_US), // US
#else
	                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
#endif
	                  (LPTSTR) &lpMsgBuf, 0, nullptr);
	string tmp;
	if (chars != 0)
	{
		tmp = Text::fromT(lpMsgBuf);
		// Free the buffer.
		LocalFree(lpMsgBuf);
		string::size_type i = 0;
		
		while ((i = tmp.find_first_of("\r\n", i)) != string::npos)
		{
			tmp.erase(i, 1);
		}
	}
	tmp += "[error: " + Util::toString(error) + "]";
	return tmp;
#else // _WIN32
	return Text::toUtf8(strerror(error));
#endif // _WIN32
}
