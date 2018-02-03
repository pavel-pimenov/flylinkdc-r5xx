/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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
#include "File.h"
#ifndef _CONSOLE
#include "LogManager.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#include "../FlyFeatures/flyServer.h"
#include "ClientManager.h"
#endif

#include "CompatibilityManager.h" // [+] IRainman

const FileFindIter FileFindIter::end; // [+] IRainman opt.

void File::init(const tstring& aFileName, int access, int mode, bool isAbsolutePath) // [!] IRainman fix.
{
	dcassert(access == static_cast<int>(WRITE) || access == static_cast<int>(READ) || access == static_cast<int>((READ | WRITE)));
	
	int m;
	if (mode & OPEN)
	{
		if (mode & CREATE)
		{
			m = (mode & TRUNCATE) ? CREATE_ALWAYS : OPEN_ALWAYS;
		}
		else
		{
			m = (mode & TRUNCATE) ? TRUNCATE_EXISTING : OPEN_EXISTING;
		}
	}
	else
	{
		if (mode & CREATE)
		{
			m = (mode & TRUNCATE) ? CREATE_ALWAYS : CREATE_NEW;
		}
		else
		{
			m = 0;
			dcassert(0);
		}
	}
#ifdef _DEBUG
	//if (Text::fromT(aFileName).find(".log") == string::npos)
	//{
	//  //LogManager::message("File::init - file: " + Text::fromT(aFileName));
	//}
#endif
	const DWORD shared = FILE_SHARE_READ | (mode & SHARED ? (FILE_SHARE_WRITE | FILE_SHARE_DELETE) : 0);
	
	const tstring outPath = isAbsolutePath ? formatPath(aFileName) : aFileName;
	
	h = ::CreateFile(outPath.c_str(), access, shared, nullptr, m, mode & NO_CACHE_HINT ? 0 : FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	
	if (h == INVALID_HANDLE_VALUE)
	{
#ifdef _DEBUG
#if 1
		if (outPath.find(_T(".dctmp")) != tstring::npos)
		{
			int a;
			a++;
			dcassert(0);
		}
		std::ofstream l_fs;
		l_fs.open(_T("flylinkdc-file-error.log"), std::ifstream::out | std::ifstream::app);
		if (l_fs.good())
		{
			l_fs << Util::toString(GetLastError()) << " File = " << Text::fromT(outPath) << std::endl;
		}
#endif
#endif
		throw FileException(Util::translateError());
	}
}

int64_t File::getLastWriteTime()const noexcept
{
	FILETIME f = {0};
	::GetFileTime(h, NULL, NULL, &f);
	const int64_t l_res = (((int64_t)f.dwLowDateTime | ((int64_t)f.dwHighDateTime) << 32) - 116444736000000000LL) / (1000LL * 1000LL * 1000LL / 100LL); //1.1.1970
	//[+] PVS Studio V592   The expression was enclosed by parentheses twice: ((expression)). One pair of parentheses is unnecessary or misprint is present.
	return l_res;
}

//[+] Greylink
int64_t File::getTimeStamp(const string& aFileName)
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFileEx(Text::toT(formatPath(aFileName)).c_str(),
	                               CompatibilityManager::g_find_file_level,
	                               &fd,
	                               FindExSearchNameMatch,
	                               NULL,
	                               0);
	if (hFind == INVALID_HANDLE_VALUE)
		throw FileException(Util::translateError() + ": " + aFileName);
	FindClose(hFind);
	return *(int64_t*)&fd.ftLastWriteTime;
}

void File::setTimeStamp(const string& aFileName, const uint64_t stamp)
{
	HANDLE hCreate = CreateFile(formatPath(Text::toT(aFileName)).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hCreate == INVALID_HANDLE_VALUE)
		throw FileException(Util::translateError() + ": " + aFileName);
	if (!SetFileTime(hCreate, NULL, NULL, (FILETIME*)&stamp))
	{
		CloseHandle(hCreate); //[+]PPA
		throw FileException(Util::translateError() + ": " + aFileName);
	}
	CloseHandle(hCreate);
}
//[~]Greylink

uint64_t File::currentTime()
{
	static const SYSTEMTIME s = { 1970, 1, 0, 1, 0, 0, 0, 0 };
	static FILETIME f2 = {0, 0};
	FILETIME f;
	GetSystemTimeAsFileTime(&f);
	if (!f2.dwLowDateTime)
		::SystemTimeToFileTime(&s, &f2);
	//[merge] http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2195
	ULARGE_INTEGER a, b;
	a.LowPart = f.dwLowDateTime;
	a.HighPart = f.dwHighDateTime;
	b.LowPart = f2.dwLowDateTime;
	b.HighPart = f2.dwHighDateTime;
	return (a.QuadPart - b.QuadPart) / (10000000LL); // 100ns > s
}

uint64_t File::convertTime(const FILETIME* f)
{
	static const SYSTEMTIME s = { 1970, 1, 0, 1, 0, 0, 0, 0 };
	static FILETIME f2 = {0, 0};
	if (!f2.dwLowDateTime) //[+]PPA
		::SystemTimeToFileTime(&s, &f2);
	//[merge] http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2195
	ULARGE_INTEGER a, b;
	a.LowPart = f->dwLowDateTime;
	a.HighPart = f->dwHighDateTime;
	b.LowPart = f2.dwLowDateTime;
	b.HighPart = f2.dwHighDateTime;
	return (a.QuadPart - b.QuadPart) / (10000000LL); // 100ns > s
}

bool File::isOpen() const noexcept
{
	return h != INVALID_HANDLE_VALUE;
}

void File::close() noexcept
{
	if (isOpen())
	{
		CloseHandle(h);
		h = INVALID_HANDLE_VALUE;
	}
}

int64_t File::getSize() const noexcept
{
	// [!] IRainman use GetFileSizeEx function!
	// http://msdn.microsoft.com/en-us/library/aa364957(v=VS.85).aspx
	LARGE_INTEGER x = {0};
	BOOL bRet = ::GetFileSizeEx(h, &x);
	
	if (bRet == FALSE)
		return -1;
		
	return x.QuadPart;
}
int64_t File::getPos() const noexcept
{
	// [!] IRainman use SetFilePointerEx function!
	// http://msdn.microsoft.com/en-us/library/aa365542(v=VS.85).aspx
	LARGE_INTEGER x = {0};
	BOOL bRet = ::SetFilePointerEx(h, x, &x, FILE_CURRENT);
	
	if (bRet == FALSE)
		return -1;
		
	return x.QuadPart;
}

void File::setSize(int64_t newSize)
{
	int64_t pos = getPos();
	setPos(newSize);
	setEOF();
	setPos(pos);
}
void File::setPos(int64_t pos)
{
	// [!] IRainman use SetFilePointerEx function!
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_BEGIN))
		throw(FileException(Util::translateError()));
}
int64_t File::setEndPos(int64_t pos)
{
	// [!] IRainman use SetFilePointerEx function!
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_END))
		throw(FileException(Util::translateError())); //[+]PPA
	return x.QuadPart;
}

void File::movePos(int64_t pos)
{
	// [!] IRainman use SetFilePointerEx function!
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_CURRENT))
		throw(FileException(Util::translateError())); //[+]PPA
}

size_t File::read(void* buf, size_t& len)
{
	DWORD x = 0;
	if (!::ReadFile(h, buf, (DWORD)len, &x, NULL))
	{
		throw (FileException(Util::translateError()));
	}
	len = x;
	return x;
}

size_t File::write(const void* buf, size_t len)
{
	DWORD x = 0;
	if (!::WriteFile(h, buf, (DWORD)len, &x, NULL))
	{
		throw FileException(Util::translateError());
	}
	if (x != len)
	{
		throw FileException("Error in File::write x != len"); //[+]PPA
	}
	return x;
}
void File::setEOF()
{
	dcassert(isOpen());
	if (!SetEndOfFile(h))
	{
		throw FileException(Util::translateError());
	}
}
#ifdef _DEBUG
/*
string File::getRealPath() const {
    TCHAR buf[MAX_PATH];
    auto ret = GetFinalPathNameByHandle(h, buf, MAX_PATH, FILE_NAME_OPENED);
    if (!ret) {
        throw FileException(Util::translateError(GetLastError()));
    }

    return Text::fromT(buf);
}
*/
#endif

size_t File::flushBuffers(bool aForce)
{
	/* TODO
	if (!aForce) {
	        return 0;
	    }
	*/
#ifndef _CONSOLE
#ifdef _DEBUG
	auto start = boost::posix_time::microsec_clock::universal_time();
#endif
	if (isOpen() && !ClientManager::isShutdown())
	{
		//static int g_count = 0;
		//LogManager::message("File::flush() count = " + Util::toString(++g_count));
		if (!FlushFileBuffers(h)) // TODO - похерить вообще  https://msdn.microsoft.com/ru-ru/library/windows/desktop/aa364439%28v=vs.85%29.aspx
			// и нужно юзать FILE_FLAG_NO_BUFFERING and FILE_FLAG_WRITE_THROUGH ?
			
		{
			string l_error = Util::translateError();
			l_error = "File::flush() error = " + l_error;
			LogManager::message(l_error);
			if (!ClientManager::isShutdown()) // fix https://drdump.com/Bug.aspx?ProblemID=135087
			{
				CFlyServerJSON::pushError(33, l_error);
				throw FileException(Util::translateError());
			}
		}
	}
#endif
#ifdef _DEBUG
	//dcdebug("File %s was flushed in " I64_FMT " ms\n", "File-x-todo"/*getRealPath().c_str() */, (boost::posix_time::microsec_clock::universal_time() - start).total_milliseconds());
#endif
	return 0;
}
bool File::deleteFileT(const tstring& aFileName) noexcept
{
#ifndef _CONSOLE
	//CFlyLog l_log("[Delete]");
#endif
#ifndef _CONSOLE
	//l_log.log("Start delete file: " + Text::fromT(aFileName));
#endif
	const auto l_result_delete = ::DeleteFile(formatPath(aFileName).c_str()) != NULL;
	if (l_result_delete == false)
	{
#ifndef _CONSOLE
		const string l_error = "Error delete file: " + Text::fromT(aFileName) + " code:" + Util::toString(GetLastError());
		LogManager::message(l_error);
#endif
	}
	return l_result_delete;
}

bool File::renameFile(const tstring& p_source, const tstring& p_target)
{
	bool l_res = true;
#ifndef _CONSOLE
	CFlyLog l_log("[Rename]");
#endif
	if (!::MoveFile(formatPath(p_source).c_str(), formatPath(p_target).c_str()))
	{
#ifndef _CONSOLE
		l_log.log("Start copy file: " + Text::fromT(p_source) + " -> " + Text::fromT(p_target) + " code:" + Util::toString(GetLastError()));
#endif
		//dcassert(0);
		try
		{
			copyFile(p_source, p_target);
		}
		catch (FileException & e)
		{
#ifndef _CONSOLE
			l_log.log("Error copy file: " + e.getError());
#endif
			throw;
		}
		if (!deleteFileT(p_source))
		{
			l_res = false;
		}
	}
	else
	{
#ifndef _CONSOLE
		l_log.log("MoveFile OK " + Text::fromT(p_source) + " -> " + Text::fromT(p_target));
#endif
	}
	return l_res;
}

void File::copyFile(const tstring & source, const tstring & target)
{
	if (!::CopyFile(formatPath(source).c_str(), formatPath(target).c_str(), FALSE))
	{
		throw FileException(Util::translateError());
	}
}
#ifndef _CONSOLE
size_t File::bz2CompressFile(const wstring & p_file, const wstring & p_file_bz2)
{
	size_t l_outSize = 0;
	int64_t l_size = File::getSize(p_file);
	if (l_size > 0)
	{
		unique_ptr<byte[]> l_inData(new byte[l_size]);
		File l_f(p_file, File::READ, File::OPEN, false);
		size_t l_read_size = l_size;
		l_f.read(l_inData.get(), l_read_size);
		if (l_read_size == static_cast<uint64_t>(l_size))
		{
			unique_ptr<OutputStream> l_outFilePtr(new File(p_file_bz2, File::WRITE, File::TRUNCATE | File::CREATE, false));
			FilteredOutputStream<BZFilter, false> l_outFile(l_outFilePtr.get());
			l_outSize += l_outFile.write(l_inData.get(), l_size);
			l_outSize += l_outFile.flushBuffers(true);
		}
	}
	return l_outSize;
}
#endif // _CONSOLE

int64_t File::getSize(const tstring & aFileName) noexcept
{
	auto i = FileFindIter(aFileName);
	return i != FileFindIter::end ? i->getSize() : -1; // -1 не менять!
}

bool File::isExist(const tstring & aFileName) noexcept // [+] IRainman
{
	const DWORD attr = GetFileAttributes(formatPath(aFileName).c_str());
	return (attr != INVALID_FILE_ATTRIBUTES);
}

bool File::isExist(const tstring & filename, int64_t & outFileSize, int64_t & outFiletime, bool& p_is_link) // [+] FlylinkDC++ Team
{
	dcassert(!filename.empty());
	
	if (filename.empty())
		return false;
		
	FileFindIter i(filename); // TODO - formatPath ?
	p_is_link = false;
	if (i != FileFindIter::end)
	{
		p_is_link = i->isLink();
		outFileSize = i->getSize();
		outFiletime = i->getLastWriteTime();
		return true;
	}
	return false;
}

string File::formatPath(const string & path, bool p_is_force_unc /*= false*/)
{
	//dcassert( filename.find(_T("\\\\")) == tstring.npos && filename.find(_T("//")) == tstring.npos));
	if (p_is_force_unc == false && path.size() < (MAX_PATH / 2) - 2)
		return path;
		
	if (path[1] == '\\' && path[0] == '\\')
		return "\\\\?\\UNC\\" + path.substr(2);
	else
		return "\\\\?\\" + path;
}

tstring File::formatPath(const tstring & path, bool p_is_force_unc /*= false*/)
{
	dcassert(std::count(path.cbegin(), path.cend(), L'/') == 0);
	if (p_is_force_unc == false && path.size() < (MAX_PATH / 2) - 2)
		return path;
		
	if (path[1] == _T('\\') && path[0] == _T('\\'))
		return _T("\\\\?\\UNC\\") + path.substr(2);
	else
		return _T("\\\\?\\") + path;
}

void File::addTrailingSlash(string & p_path)
{
	dcassert(!p_path.empty());
	if (!p_path.empty())
	{
		if (p_path[p_path.size() - 1] != '\\' && p_path[p_path.size() - 1] != '/')
		{
			p_path += '\\';
		}
	}
}

void File::ensureDirectory(const tstring & aFile)
{
	dcassert(!aFile.empty());
	// Skip the first dir...
	tstring::size_type start = aFile.find_first_of(_T("\\/"));
	if (start == tstring::npos)
		return;
	start++;
	while ((start = aFile.find_first_of(_T("\\/"), start)) != tstring::npos)
	{
		const auto l_dir = formatPath(aFile.substr(0, start + 1));
		const BOOL result = ::CreateDirectory(l_dir.c_str(), NULL);
#if 0
		const auto l_last_error_code = GetLastError();
		string l_error;
		if (result == FALSE)
		{
			l_error = "[!!!!] Error File::ensureDirectory: " + Text::fromT(l_dir) + " error = " + Util::translateError(l_last_error_code);
		}
		else
		{
			l_error = "[!!!!] OK File::ensureDirectory: " + Text::fromT(l_dir) + " error = " + Util::translateError(l_last_error_code);
		}
		{
			std::ofstream l_fs;
			l_fs.open(_T("flylinkdc-file-error.log"), std::ifstream::out | std::ifstream::app);
			if (l_fs.good())
			{
				l_fs << l_error << std::endl;
			}
			
		}
#endif
		++start;
	}
}

#if 0
void File::ensureDirectory(tstring aFile)
{
	dcassert(!aFile.empty());
	// dcassert(aFile[aFile.size()-1] == _T('\\') || aFile[aFile.size()-1] == _T('/'));
	// addTrailingSlash(aFile);
	// Skip the first dir...
	// aFile = _T("D:\\TempDC\\gta4_complete_dvd2.iso.FD3FBG7AR4MJZYATYNW2EV3BJPGRRLKK6TX4UTA.dctmp");
	tstring::size_type start = 0;
	if (aFile.size() > 2 && aFile[0] == L'\\' && aFile[1] == L'\\')
		start++;
	else
		start = aFile.find_first_of(_T("\\/"), start);
	if (start == tstring::npos)
		return;
	start++;
	while ((start = aFile.find_first_of(_T("\\/"), start)) != tstring::npos)
	{
		const auto l_dir = aFile.substr(0, start + 1);
		const BOOL result = ::CreateDirectory(formatPath(l_dir).c_str(), NULL);
		if (result == FALSE)
		{
			const auto l_last_error_code = GetLastError();
			if (l_last_error_code != ERROR_ALREADY_EXISTS)
			{
				const string l_error = "Error File::ensureDirectory: " +  Text::fromT(aFile) + " error = " + Util::translateError(l_last_error_code);
				const tstring l_email_message = Text::toT(l_error + "\r\nSend screenshot (or text - press ctrl+c for copy to clipboard) e-mail ppa74@ya.ru for diagnostic error!");
				// ::MessageBox(NULL, l_email_message.c_str() , getFlylinkDCAppCaptionWithVersionT().c_str()  , MB_OK | MB_ICONERROR);
				// Отрубил месаагу - глючит сетевом диске
				// Error File::ensureDirectory: \\FLYLINKDC-SERV\video\Metallica_-_Sad_But_True.mpg.KMTY5VOGVN7YESAWLKR7FKJPXQT5J5B2PYEFDGY.dctmp error = Синтаксическая ошибка в имени файла, имени папки или метке тома.[error: 123]
				if (LogManager::isValidInstance())
				{
					CFlyServerJSON::pushError(10, l_error);
					LogManager::message(l_error);
				}
				// TODO - исключить выброс исключения - пусть дальше ковыляет
				// throw FileException(l_error);
			}
		}
		start++;
	}
}
#endif

string File::read(size_t len)
{
	string s(len, 0);
	size_t x = read(&s[0], len);
	if (x != s.size())
		s.resize(x);
	return s;
}

string File::read()
{
	setPos(0);
	int64_t sz = getSize();
	if (sz <= 0)
		return Util::emptyString;
	return read((uint32_t)sz);
}
uint64_t File::calcFilesSize(const string& path, const string& pattern)
{
	uint64_t l_size = 0;
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFileEx(formatPath(Text::toT(path + pattern)).c_str(),
	                               CompatibilityManager::g_find_file_level,
	                               &data,
	                               FindExSearchNameMatch,
	                               NULL,
	                               0);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				l_size += (int64_t)data.nFileSizeLow | ((int64_t)data.nFileSizeHigh) << 32;
			}
		}
		while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	return l_size;
}
StringList File::findFiles(const string& path, const string& pattern, bool p_append_path /*= true */)
{
	StringList ret;
	
	// [+] FlylinkDC
	auto appendToRet = [p_append_path, path](StringList & ret, const string & l_name, const char * extra) -> void
	{
		if (p_append_path) //[+]FlylinkDC++ Team TODO - проверить все места, где наружу не нужно отдавать путь
			ret.push_back(path + l_name + extra);
		else
			ret.push_back(l_name + extra);
	};
	// [~] FlylinkDC
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFileEx(formatPath(Text::toT(path + pattern)).c_str(),
	                               CompatibilityManager::g_find_file_level,
	                               &data,
	                               FindExSearchNameMatch,
	                               NULL,
	                               0);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			const char* extra = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "\\" : "";
			// [!] FlylinkDC
			const string l_name = Text::fromT(data.cFileName);
			appendToRet(ret, l_name, extra);
			// [~] FlylinkDC
		}
		while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}
	return ret;
}

void FileFindIter::init(const tstring & path)
{
	//WIN32_FIND_DATA l_init = {0};
	//m_data = l_init;
	m_handle = FindFirstFileEx(File::formatPath(path).c_str(),
	                           CompatibilityManager::g_find_file_level,
	                           &m_data,
	                           FindExSearchNameMatch,
	                           NULL,
	                           CompatibilityManager::g_find_file_flags);
	if (m_handle == INVALID_HANDLE_VALUE)
	{
		// dcassert(0);
		// LogManager::message("FileFindIter::init path = " + Text::fromT(path) + " GetLastError() = " + Util::toString(GetLastError()));
	}
	// else
	// {
	//   if(m_data.isLink())
	//   {
	//     FindClose(m_handle);
	//     m_handle = FindFirstFile(File::formatPath(path).c_str(),&m_data);
	//   }
	// }
}

FileFindIter::~FileFindIter()
{
	if (m_handle != INVALID_HANDLE_VALUE)
	{
		FindClose(m_handle);
	}
}

FileFindIter& FileFindIter::operator++()
{
	//[12] https://www.box.net/shared/067924cecdb252c9d26c
	if (m_handle != INVALID_HANDLE_VALUE)
	{
		if (::FindNextFile(m_handle, &m_data) == FALSE)
		{
			FindClose(m_handle);
			m_handle = INVALID_HANDLE_VALUE;
		}
	}
	return *this;
}

bool FileFindIter::operator!=(const FileFindIter & rhs) const
{
	return m_handle != rhs.m_handle;
}

FileFindIter::DirData::DirData()
{
// TODO   WIN32_FIND_DATA l_init = {0};
// TODO   *this = l_init;
}

string FileFindIter::DirData::getFileName() const
{
	return Text::fromT(cFileName);
}

bool FileFindIter::DirData::isDirectory() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) > 0;
}

bool FileFindIter::DirData::isHidden() const
{
	return ((dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) > 0
	        || (CompatibilityManager::isWine() && cFileName[0] == L'.')); //[+]IRainman Posix hidden files
}
bool FileFindIter::DirData::isLink() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) > 0;
}
int64_t FileFindIter::DirData::getSize() const
{
	return (int64_t)nFileSizeLow | ((int64_t)nFileSizeHigh) << 32;
}

int64_t FileFindIter::DirData::getLastWriteTime() const
{
	return File::convertTime(&ftLastWriteTime);
}

// [+]IRainman
bool FileFindIter::DirData::isSystem() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) > 0;
}

bool FileFindIter::DirData::isTemporary() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_TEMPORARY) > 0;
}

bool FileFindIter::DirData::isVirtual() const
{
	return (dwFileAttributes & FILE_ATTRIBUTE_VIRTUAL) > 0;
}

FILE* dcpp_fopen(const char* filename, const char* mode)
{
	return _wfopen(Text::toT(filename).c_str(), Text::toT(mode).c_str());
}
