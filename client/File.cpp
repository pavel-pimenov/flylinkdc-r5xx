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
#include "File.h"
#ifndef _CONSOLE
#include "LogManager.h"
#include "FilteredFile.h"
#include "BZUtils.h"
#endif

#ifdef _WIN32

#include "CompatibilityManager.h" // [+] IRainman

const FileFindIter FileFindIter::end; // [+] IRainman opt.

void File::init(const tstring& aFileName, int access, int mode, bool isAbsolutePath) // [!] IRainman fix.
{
	dcassert(access == WRITE || access == READ || access == (READ | WRITE));
	
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
	const DWORD shared = FILE_SHARE_READ | (mode & SHARED ? (FILE_SHARE_WRITE | FILE_SHARE_DELETE) : 0);
	
	const tstring& outPath = isAbsolutePath ? formatPath(aFileName) : aFileName;
	
	h = ::CreateFile(outPath.c_str(), access, shared, nullptr, m, mode & NO_CACHE_HINT ? 0 : FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
	
	if (h == INVALID_HANDLE_VALUE)
	{
//[!] Не включать - падаем на рекурсии
//        LogManager::getInstance()->message("File::File error = " + Util::toString(GetLastError()) + " File = " + aFileName);
		throw FileException(Util::translateError(GetLastError()));
	}
}

uint64_t File::getLastWriteTime()const noexcept
{
    FILETIME f = {0};
    ::GetFileTime(h, NULL, NULL, &f);
    const uint64_t l_res = (((int64_t)f.dwLowDateTime | ((int64_t)f.dwHighDateTime) << 32) - 116444736000000000LL) / (1000LL * 1000LL * 1000LL / 100LL); //1.1.1970
    //[+] PVS Studio V592   The expression was enclosed by parentheses twice: ((expression)). One pair of parentheses is unnecessary or misprint is present.
    return l_res;
}

//[+] Greylink
uint64_t File::getTimeStamp(const string& aFileName) throw(FileException)
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(Text::toT(formatPath(aFileName)).c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		throw FileException(Util::translateError(GetLastError()) + ": " + aFileName);
	FindClose(hFind);
	return *(int64_t*)&fd.ftLastWriteTime;
}

void File::setTimeStamp(const string& aFileName, const uint64_t stamp) throw(FileException)
{
	HANDLE h = CreateFile(formatPath(Text::toT(aFileName)).c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
		throw FileException(Util::translateError(GetLastError()) + ": " + aFileName);
	if (!SetFileTime(h, NULL, NULL, (FILETIME*)&stamp))
	{
		CloseHandle(h); //[+]PPA
		throw FileException(Util::translateError(GetLastError()) + ": " + aFileName);
	}
	CloseHandle(h);
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
void File::setPos(int64_t pos) throw(FileException)
{
	// [!] IRainman use SetFilePointerEx function!
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_BEGIN))
		throw(FileException(Util::translateError(GetLastError()))); //[+]PPA
}
void File::setEndPos(int64_t pos) throw(FileException)
{
	// [!] IRainman use SetFilePointerEx function!
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_END))
		throw(FileException(Util::translateError(GetLastError()))); //[+]PPA
}

void File::movePos(int64_t pos) throw(FileException)
{
	// [!] IRainman use SetFilePointerEx function!
	LARGE_INTEGER x = {0};
	x.QuadPart = pos;
	if (!::SetFilePointerEx(h, x, &x, FILE_CURRENT))
		throw(FileException(Util::translateError(GetLastError()))); //[+]PPA
}

size_t File::read(void* buf, size_t& len)
{
	DWORD x = 0;
	if (!::ReadFile(h, buf, (DWORD)len, &x, NULL))
	{
		throw(FileException(Util::translateError(GetLastError())));
	}
	len = x;
	return x;
}

size_t File::write(const void* buf, size_t len)
{
	DWORD x = 0;
	if (!::WriteFile(h, buf, (DWORD)len, &x, NULL))
	{
		throw FileException(Util::translateError(GetLastError()));
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
		throw FileException(Util::translateError(GetLastError()));
	}
}

size_t File::flush()
{
	if (isOpen() && !FlushFileBuffers(h))
		throw FileException(Util::translateError(GetLastError()));
	return 0;
}

void File::renameFile(const tstring& source, const tstring& target)
{
	if (!::MoveFile(formatPath(source).c_str(), formatPath(target).c_str()))
	{
		// Can't move, try copy/delete...
		copyFile(source, target);
		deleteFileT(source);
	}
}

void File::copyFile(const tstring& source, const tstring& target)
{
	if (!::CopyFile(formatPath(source).c_str(), formatPath(target).c_str(), FALSE))
	{
		throw FileException(Util::translateError(GetLastError())); // 2012-05-03_22-05-14_LZE57W5HZ7NI3VC773UG4DNJ4QIKP7Q7AEBLWOA_AA236F48_crash-stack-r502-beta24-x64-build-9900.dmp
	}
}
#ifndef _CONSOLE
size_t File::bz2CompressFile(const wstring& p_file, const wstring& p_file_bz2)
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
			l_outSize += l_outFile.flush();
		}
	}
	return l_outSize;
}
#endif // _CONSOLE

int64_t File::getSize(const tstring& aFileName) noexcept
{
	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile(formatPath(aFileName).c_str(), &fd);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		return -1;
	}
	else
	{
		FindClose(hFind);
		return ((int64_t)fd.nFileSizeHigh << 32 | (int64_t)fd.nFileSizeLow);
	}
}

bool File::isExist(const tstring& aFileName) noexcept // [+] IRainman
{
	DWORD attr = GetFileAttributes(formatPath(aFileName).c_str());
	return (attr != INVALID_FILE_ATTRIBUTES);
}

bool File::isExist(const tstring& filename, int64_t& outFileSize, int64_t& outFiletime) // [+] FlylinkDC++ Team
{
	dcassert(!filename.empty());
	
	if (filename.empty())
		return false;
		
	FileFindIter i(filename);
	if (i != FileFindIter::end)
	{
		outFileSize = i->getSize();
		outFiletime = i->getLastWriteTime();
		return true;
	}
	return false;
}

void File::ensureDirectory(const tstring& aFile) noexcept
{
	dcassert(!aFile.empty());
	// Skip the first dir...
	tstring::size_type start = aFile.find_first_of(_T("\\/"));
	if (start == tstring::npos)
		return;
	start++;
	while ((start = aFile.find_first_of(_T("\\/"), start)) != tstring::npos)
	{
		::CreateDirectory((formatPath(aFile.substr(0, start + 1))).c_str(), NULL); //TODO Crash r501 sp1
		start++;
	}
}

#else // !_WIN32

void File::init(const string& aFileName, int access, int mode) // [!] IRainman fix.
{
	dcassert(access == WRITE || access == READ || access == (READ | WRITE));

	int m = 0;
	if (access == READ)
		m |= O_RDONLY;
	else if (access == WRITE)
		m |= O_WRONLY;
	else
		m |= O_RDWR;

	if (mode & CREATE)
	{
		m |= O_CREAT;
	}
	if (mode & TRUNCATE)
	{
		m |= O_TRUNC;
	}

	string filename = Text::fromUtf8(aFileName);

	struct stat s;
	if (lstat(filename.c_str(), &s) != -1)
	{
		if (!S_ISREG(s.st_mode) && !S_ISLNK(s.st_mode))
			throw FileException("Invalid file type");
	}

	h = open(filename.c_str(), m, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (h == -1)
		throw FileException(Util::translateError(errno));
}

uint32_t File::getLastModified() const noexcept
{
    struct stat s;
    if (::fstat(h, &s) == -1)
    return 0;

    return (uint32_t)s.st_mtime;
}

bool File::isOpen() const noexcept
	{
	    return h != -1;
	}

	void File::close() noexcept
{
	if (h != -1)
	{
		::close(h);
		h = -1;
	}
}

int64_t File::getSize() const noexcept
{
    struct stat s;
    if (::fstat(h, &s) == -1)
    return -1;

    return (int64_t)s.st_size;
}

int64_t File::getPos() const noexcept
	{
	    return (int64_t)lseek(h, 0, SEEK_CUR);
	}

	void File::setPos(int64_t pos) noexcept
{
	lseek(h, (off_t)pos, SEEK_SET);
}

void File::setEndPos(int64_t pos) noexcept
{
	lseek(h, (off_t)pos, SEEK_END);
}

void File::movePos(int64_t pos) noexcept
{
	lseek(h, (off_t)pos, SEEK_CUR);
}

size_t File::read(void* buf, size_t& len)
{
	ssize_t result = ::read(h, buf, len);
	if (result == -1)
	{
		throw FileException(Util::translateError(errno));
	}
	len = result;
	return (size_t)result;
}

size_t File::write(const void* buf, size_t len)
{
	ssize_t result;
	char* pointer = (char*)buf;
	ssize_t left = len;

	while (left > 0)
	{
		result = ::write(h, pointer, left);
		if (result == -1)
		{
			if (errno != EINTR)
			{
				throw FileException(Util::translateError(errno));
			}
		}
		else
		{
			pointer += result;
			left -= result;
		}
	}
	return len;
}

// some ftruncate implementations can't extend files like SetEndOfFile,
// not sure if the client code needs this...
int File::extendFile(int64_t len) noexcept
{
	char zero;

	if ((lseek(h, (off_t)len, SEEK_SET) != -1) && (::write(h, &zero, 1) != -1))
	{
		ftruncate(h, (off_t)len);
		return 1;
	}
	return -1;
}

void File::setEOF()
{
	int64_t pos;
	int64_t eof;
	int ret;

	pos = (int64_t)lseek(h, 0, SEEK_CUR);
	eof = (int64_t)lseek(h, 0, SEEK_END);
	if (eof < pos)
		ret = extendFile(pos);
	else
		ret = ftruncate(h, (off_t)pos);
	lseek(h, (off_t)pos, SEEK_SET);
	if (ret == -1)
		throw FileException(Util::translateError(errno));
}

void File::setSize(int64_t newSize) throw(FileException)
{
	int64_t pos = getPos();
	setPos(newSize);
	setEOF();
	setPos(pos);
}

size_t File::flush()
{
	if (isOpen() && fsync(h) == -1)
		throw FileException(Util::translateError(errno));
	return 0;
}

/**
 * ::rename seems to have problems when source and target is on different partitions
 * from "man 2 rename":
 * EXDEV oldpath and newpath are not on the same mounted filesystem. (Linux permits a
 * filesystem to be mounted at multiple points, but rename(2) does not
 * work across different mount points, even if the same filesystem is mounted on both.)
*/
void File::renameFile(const string& source, const string& target)
{
	int ret = ::rename(Text::fromUtf8(source).c_str(), Text::fromUtf8(target).c_str());
	if (ret != 0 && errno == EXDEV)
	{
		copyFile(source, target);
		deleteFile(source);
	}
	else if (ret != 0)
		throw FileException(source + Util::translateError(errno));
}

// This doesn't assume all bytes are written in one write call, it is a bit safer
void File::copyFile(const string& source, const string& target)
{
	const size_t BUF_SIZE = 64 * 1024;
	std::unique_ptr<char[]> buffer(new char[BUF_SIZE]);
	size_t count = BUF_SIZE;
	File src(source, File::READ, 0);
	File dst(target, File::WRITE, File::CREATE | File::TRUNCATE);

	while (src.read(&buffer[0], count) > 0)
	{
		char* p = &buffer[0];
		while (count > 0)
		{
			size_t ret = dst.write(p, count);
			p += ret;
			count -= ret;
		}
		count = BUF_SIZE;
	}
}

void File::deleteFile(const string& aFileName) noexcept
{
	::unlink(Text::fromUtf8(aFileName).c_str());
}

int64_t File::getSize(const string& aFileName) noexcept
{
	dcassert(!aFileName.empty())
	struct stat s;
	if (stat(Text::fromUtf8(aFileName).c_str(), &s) == -1)
		return -1;

	return s.st_size;
}
void File::ensureDirectory(const string& aFile) noexcept
{
	string file = Text::fromUtf8(aFile);
	string::size_type start = 0;
	while ((start = file.find_first_of('/', start)) != string::npos)
	{
		mkdir(file.substr(0, start + 1).c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
		start++;
	}
}
bool File::isAbsolute(const string& path) noexcept
{
	return path.size() > 1 && path[0] == '/';
}

#endif // !_WIN32

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
	if (sz <= 0) // http://code.google.com/p/flylinkdc/issues/detail?id=571
		return Util::emptyString;
	return read((uint32_t)sz);
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
#ifdef _WIN32
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFile(formatPath(Text::toT(path + pattern)).c_str(), &data);
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
#else
	DIR* dir = opendir(Text::fromUtf8(path).c_str());
	if (dir)
	{
		while (struct dirent* ent = readdir(dir))
		{
			if (fnmatch(pattern.c_str(), ent->d_name, 0) == 0)
			{
				struct stat s;
				stat(ent->d_name, &s);
				const char* extra = (s.st_mode & S_IFDIR) ? '/' : "";
				// [!] FlylinkDC
				const string l_name = Text::toUtf8(ent->d_name);
				appendToRet(ret, l_name, extra);
				// [~] FlylinkDC
			}
		}
		closedir(dir);
	}
#endif
	return ret;
}

#ifdef _WIN32

void FileFindIter::init(const tstring& path)
{
	m_handle = FindFirstFile(File::formatPath(path).c_str(), &m_data);
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
			FindClose(m_handle); // 2012-06-18_22-41-13_32LXRE65BDS4MW565YGIC2C5SIUWRTVATKY6SZQ_2051D746_crash-stack-r502-beta37-build-10387.dmp
			m_handle = INVALID_HANDLE_VALUE;
		}
	}
	return *this;
}

bool FileFindIter::operator!=(const FileFindIter& rhs) const
{
	return m_handle != rhs.m_handle;
}

FileFindIter::DirData::DirData() { }

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

// ~[+]IRainman

#else // _WIN32

FileFindIter::FileFindIter()
{
	m_dir = NULL;
	m_data.m_ent = NULL;
}

FileFindIter::FileFindIter(const string& path)
{
	string filename = Text::fromUtf8(path);
	m_dir = opendir(filename.c_str());
	if (!m_dir)
		return;
	m_data.m_base = filename;
	m_data.m_ent = readdir(m_dir);
	if (!m_data.m_ent)
	{
		closedir(m_dir);
		m_dir = NULL;
	}
}

FileFindIter::~FileFindIter()
{
	if (m_dir) closedir(m_dir);
}

FileFindIter& FileFindIter::operator++()
{
	if (!m_dir)
		return *this;
	m_data.m_ent = readdir(m_dir);
	if (!m_data.m_ent)
	{
		closedir(m_dir);
		m_dir = NULL;
	}
	return *this;
}

bool FileFindIter::operator != (const FileFindIter& rhs) const
{
	// good enough to to say if it's null
	return m_dir != rhs.m_dir;
}

FileFindIter::DirData::DirData() : m_ent(NULL) {}

string FileFindIter::DirData::getFileName() const
{
	if (!m_ent) return Util::emptyString;
	return Text::toUtf8(m_ent->d_name);
}

bool FileFindIter::DirData::isDirectory() const
{
	struct stat inode;
	if (!m_ent) return false;
	if (stat((m_base + PATH_SEPARATOR + m_ent->d_name).c_str(), &inode) == -1) return false;
	return S_ISDIR(inode.st_mode);
}

bool FileFindIter::DirData::isHidden() const
{
	if (!m_ent) return false;
	// Check if the parent directory is hidden for '.'
	if (strcmp(m_ent->d_name, ".") == 0 && m_base[0] == '.') return true;
	return m_ent->d_name[0] == '.' && strlen(m_ent->d_name) > 1;
}
bool FileFindIter::DirData::isLink() const
{
	struct stat inode;
	if (!m_ent) return false;
	if (lstat((m_base + PATH_SEPARATOR + m_ent->d_name).c_str(), &inode) == -1) return false;
	return S_ISLNK(inode.st_mode);
}

int64_t FileFindIter::DirData::getSize() const
{
	struct stat inode;
	if (!m_ent) return false;
	if (stat((m_base + PATH_SEPARATOR + m_ent->d_name).c_str(), &inode) == -1) return 0;
	return inode.st_size;
}

uint64_t FileFindIter::DirData::getLastWriteTime() const
{
	struct stat inode;
	if (!m_ent) return false;
	if (stat((m_base + PATH_SEPARATOR + m_ent->d_name).c_str(), &inode) == -1) return 0;
	return inode.st_mtime;
}

#endif // _WIN32