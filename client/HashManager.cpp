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
#include "HashManager.h"
#include "ResourceManager.h"
#include "SimpleXML.h"
#include "LogManager.h"
#include "CFlylinkDBManager.h"
#include "CompatibilityManager.h" // [+] IRainman
#include "../FlyFeatures/flyServer.h"

#ifdef IRAINMAN_NTFS_STREAM_TTH
//[+] Greylink
const string HashManager::StreamStore::g_streamName(".gltth");

#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
void HashManager::StreamStore::SetFsDetectorNotifyWnd(HWND hWnd)
{
	m_FsDetect.SetNotifyWnd(hWnd);
}
#else
std::unordered_set<char> HashManager::StreamStore::g_error_tth_stream;
#endif

inline void HashManager::StreamStore::setCheckSum(TTHStreamHeader& p_header)
{
	p_header.magic = g_MAGIC;
	uint32_t l_sum = 0;
	for (size_t i = 0; i < sizeof(TTHStreamHeader) / sizeof(uint32_t); i++)
		l_sum ^= ((uint32_t*) & p_header)[i];
	p_header.checksum ^= l_sum;
}

inline bool HashManager::StreamStore::validateCheckSum(const TTHStreamHeader& p_header)
{
	if (p_header.magic != g_MAGIC)
		return false;
	uint32_t l_sum = 0;
	for (size_t i = 0; i < sizeof(TTHStreamHeader) / sizeof(uint32_t); i++)
		l_sum ^= ((uint32_t*) & p_header)[i];
	return (l_sum == 0);
}
void HashManager::addTree(const TigerTree& p_tree)
{
	CFlylinkDBManager::getInstance()->add_tree(p_tree);
}

bool HashManager::StreamStore::loadTree(const string& p_filePath, TigerTree& p_Tree, int64_t p_FileSize)
{
	try
	{
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
		if (!m_FsDetect.IsStreamSupported(p_filePath.c_str()))
#else
		if (isBan(p_filePath))
#endif
			return false;
		dcassert(p_FileSize > 0);
		const int64_t l_fileSize = p_FileSize == -1 ? File::getSize(p_filePath) : p_FileSize;
		if (l_fileSize < SETTING(SET_MIN_LENGHT_TTH_IN_NTFS_FILESTREAM) * 1048576) // that's why minStreamedFileSize never be changed![*]NightOrion
			return false;
		const int64_t l_timeStamp = File::getTimeStamp(p_filePath);
		{
			File l_stream(p_filePath + ":" + g_streamName, File::READ, File::OPEN);
			size_t l_sz = sizeof(TTHStreamHeader);
			TTHStreamHeader h;
			if (l_stream.read(&h, l_sz) != sizeof(TTHStreamHeader))
				return false;
			if (uint64_t(l_fileSize) != h.fileSize || l_timeStamp != uint64_t(h.timeStamp) || !validateCheckSum(h))
				return false;
			const size_t l_datalen = TigerTree::calcBlocks(l_fileSize, h.blockSize) * TTHValue::BYTES;
			l_sz = l_datalen;
			AutoArray<uint8_t> buf(l_datalen);
			if (l_stream.read((uint8_t*)buf, l_sz) != l_datalen)
				return false;
			p_Tree = TigerTree(l_fileSize, h.blockSize, buf, l_datalen);
			if (!(p_Tree.getRoot() == h.root))
				return false;
		}
	}
	catch (const Exception& /*e*/)
	{
		// Отключил спам http://code.google.com/p/flylinkdc/issues/detail?id=1415
		// LogManager::message(STRING(ERROR_GET_TTH_STREAM) + ' ' + p_filePath + " Error = " + e.getError());// [+]IRainman
		return false;
	}
	/*
	if (speed > 0) {
	        LogManager::message(STRING(HASHING_FINISHED) + ' ' + fn + " (" + Util::formatBytes(speed) + "/s)");
	    } else {
	        LogManager::message(STRING(HASHING_FINISHED) + ' ' + fn);
	    }
	*/
	return true;
}

bool HashManager::StreamStore::saveTree(const string& p_filePath, const TigerTree& p_Tree)
{
	if (CompatibilityManager::isWine() || //[+]PPA под wine не пишем в поток
	        !BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM))
		return false;
#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
	if (!m_FsDetect.IsStreamSupported(p_filePath.c_str()))
#else
	if (isBan(p_filePath))
#endif
		return false;
	try
	{
		TTHStreamHeader h;
		h.fileSize = File::getSize(p_filePath);
		if (h.fileSize < (SETTING(SET_MIN_LENGHT_TTH_IN_NTFS_FILESTREAM) * 1048576) || h.fileSize != (uint64_t)p_Tree.getFileSize()) //[*]NightOrion
			return false; // that's why minStreamedFileSize never be changed!
		h.timeStamp = File::getTimeStamp(p_filePath);
		h.root = p_Tree.getRoot();
		h.blockSize = p_Tree.getBlockSize();
		setCheckSum(h);
		{
			File stream(p_filePath + ":" + g_streamName, File::WRITE, File::CREATE | File::TRUNCATE);
			stream.write(&h, sizeof(TTHStreamHeader));
			stream.write(p_Tree.getLeaves()[0].data, p_Tree.getLeaves().size() * TTHValue::BYTES);
		}
		File::setTimeStamp(p_filePath, h.timeStamp);
	}
	catch (const Exception& e)
	{
#ifndef RIP_USE_STREAM_SUPPORT_DETECTION
		addBan(p_filePath);
#endif
		LogManager::message(STRING(ERROR_ADD_TTH_STREAM) + ' ' + p_filePath + " : " + e.getError());// [+]IRainman
		return false;
	}
	return true;
}
//[~] Greylink

void HashManager::StreamStore::deleteStream(const string& p_filePath)
{
	try
	{
		File::deleteFile(p_filePath + ":" + g_streamName);
	}
	catch (const FileException& e)
	{
		LogManager::message(STRING(ERROR_DELETE_TTH_STREAM) + ' ' + p_filePath + " : " + e.getError());
	}
}

#ifdef RIP_USE_STREAM_SUPPORT_DETECTION
void HashManager::SetFsDetectorNotifyWnd(HWND hWnd)
{
	m_streamstore.SetFsDetectorNotifyWnd(hWnd);
}
#endif

void HashManager::addFileFromStream(int64_t p_path_id, const string& p_name, const TigerTree& p_TT, int64_t p_size)
{
	const int64_t l_TimeStamp = File::getTimeStamp(p_name);
	CFlyMediaInfo l_out_media;
	addFile(p_path_id, p_name, l_TimeStamp, p_TT, p_size, l_out_media);
}
#endif // IRAINMAN_NTFS_STREAM_TTH

#if 0
bool HashManager::checkTTH(const string& fname, const string& fpath, int64_t p_path_id, int64_t aSize, int64_t aTimeStamp, TTHValue& p_out_tth)
{
	// Lock l(cs); [-] IRainman fix: no data to lock.
	const bool l_db = CFlylinkDBManager::getInstance()->check_tth(fname, p_path_id, aSize, aTimeStamp, p_out_tth);
#ifdef IRAINMAN_NTFS_STREAM_TTH
	const string name = fpath + fname;
#endif // IRAINMAN_NTFS_STREAM_TTH
	if (!l_db)
	{
#ifdef IRAINMAN_NTFS_STREAM_TTH
		TigerTree l_TT;
		if (m_streamstore.loadTree(name, l_TT)) // [!] IRainman fix: no needs to lock.
		{
			addFileFromStream(p_path_id, name, l_TT, aSize); // [!] IRainman fix: no needs to lock.
		}
		else
		{
#endif // IRAINMAN_NTFS_STREAM_TTH      
			hasher.hashFile(p_path_id, name, aSize); // [!] IRainman fix: no needs to lock.
			return false;
			
#ifdef IRAINMAN_NTFS_STREAM_TTH
		}
#endif // IRAINMAN_NTFS_STREAM_TTH
	}
#ifdef IRAINMAN_NTFS_STREAM_TTH
	else
	{
		if (File::isExist(name))
		{
			TigerTree l_TT;
			__int64 l_block_size;
			if (CFlylinkDBManager::getInstance()->get_tree(p_out_tth, l_TT, l_block_size)) // [!] IRainman fix: no needs to lock.
			{
				m_streamstore.saveTree(name, l_TT); // [!] IRainman fix: no needs to lock.
			}
		}
	}
#endif // IRAINMAN_NTFS_STREAM_TTH
	return true;
}
#endif

void HashManager::hashDone(__int64 p_path_id, const string& aFileName, int64_t aTimeStamp, const TigerTree& tth, int64_t speed,
                           bool p_is_ntfs, int64_t p_size)
{
	// Lock l(cs); [-] IRainman fix: no data to lock.
	dcassert(!aFileName.empty());
	if (aFileName.empty())
	{
		LogManager::message("HashManager::hashDone - aFileName.empty()");
		return;
	}
	CFlyMediaInfo l_out_media;
	try
	{
		addFile(p_path_id, aFileName, aTimeStamp, tth, p_size, l_out_media);
#ifdef IRAINMAN_NTFS_STREAM_TTH
		if (BOOLSETTING(SAVE_TTH_IN_NTFS_FILESTREAM))
		{
			if (!p_is_ntfs) // [+] PPA TTH received from the NTFS stream, do not write back!
			{
				HashManager::getInstance()->m_streamstore.saveTree(aFileName, tth);
			}
		}
		else
		{
			HashManager::getInstance()->m_streamstore.deleteStream(aFileName);
		}
#endif // IRAINMAN_NTFS_STREAM_TTH
	}
	catch (const Exception& e)
	{
		LogManager::message(STRING(HASHING_FAILED) + ' ' + aFileName + e.getError());
		return;
	}
	fire(HashManagerListener::TTHDone(), aFileName, tth.getRoot(), aTimeStamp, l_out_media, p_size);
	CFlylinkDBManager::getInstance()->push_add_share_tth(tth.getRoot());
	
	string fn = aFileName;
	if (count(fn.begin(), fn.end(), PATH_SEPARATOR) >= 2)
	{
		string::size_type i = fn.rfind(PATH_SEPARATOR);
		i = fn.rfind(PATH_SEPARATOR, i - 1);
		fn.erase(0, i);
		fn.insert(0, "...");
	}
	if (speed > 0)
	{
		LogManager::message(STRING(HASHING_FINISHED) + ' ' + fn + " (" + Util::formatBytes(speed) + '/' + STRING(S) + ")");
	}
	else
	{
		LogManager::message(STRING(HASHING_FINISHED) + ' ' + fn);
	}
}


void HashManager::addFile(__int64 p_path_id, const string& p_file_name, int64_t p_time_stamp, const TigerTree& p_tth, int64_t p_size, CFlyMediaInfo& p_out_media)
{
	// dcassert(p_path_id);
	p_out_media.init(); // TODO - делается двойной инит
	getMediaInfo(p_file_name, p_out_media, p_size, p_tth.getRoot());
	CFlylinkDBManager::getInstance()->add_file(p_path_id, p_file_name, p_time_stamp, p_tth, p_size, p_out_media);
}

void HashManager::Hasher::hashFile(__int64 p_path_id, const string& fileName, int64_t size)
{
	FastLock l(cs);
	CFlyHashTaskItem l_task_item;
	l_task_item.m_file_size = size;
	l_task_item.m_path_id   = p_path_id;
	
	if (w.insert(make_pair(fileName, l_task_item)). second)
	{
		m_CurrentBytesLeft += size;// [+]IRainman
		if (paused > 0)
			paused++;
		else
			m_s.signal();
			
		int64_t bytesLeft;
		size_t filesLeft;
		getBytesAndFileLeft(bytesLeft, filesLeft);
		
		if (bytesLeft > iMaxBytes)
			iMaxBytes = bytesLeft;
			
		if (filesLeft > dwMaxFiles)
			dwMaxFiles = filesLeft;
	}
}

bool HashManager::Hasher::pause()
{
	FastLock l(cs);
	return paused++ > 0;
}

void HashManager::Hasher::resume()
{
	FastLock l(cs);
	while (--paused > 0)
	{
		m_s.signal();
	}
}

bool HashManager::Hasher::isPaused() const
{
	FastLock l(cs);
	return paused > 0;
}

void HashManager::Hasher::stopHashing(const string& baseDir)
{
	FastLock l(cs);
	if (baseDir.empty())
	{
		// [+]IRainman When user closes the program with a chosen operation "abort hashing"
		// in the hashing dialog then the hesher is cleaning.
		w.clear();
		m_CurrentBytesLeft = 0;
	}
	else
	{
		for (auto i = w.cbegin(); i != w.cend();)
		{
			if (strnicmp(baseDir, i->first, baseDir.length()) == 0) // TODO сравнивать ID каталогов?
			{
				m_CurrentBytesLeft -= i->second.m_file_size;
				w.erase(i++);
			}
			else
			{
				++i;
			}
		}
	}
	// [+] brain-ripper
	// cleanup state
	m_running = false;
	dwMaxFiles = 0;
	iMaxBytes = 0;
	m_fname.erase();
	m_currentSize = 0;
	m_path_id = 0;
}

void HashManager::Hasher::instantPause()
{
	bool wait = false;
	{
		FastLock l(cs);
		if (paused > 0)
		{
			paused++;
			wait = true;
		}
	}
	if (wait)
	{
		m_s.wait();
	}
}

static size_t g_HashBufferSize = 16 * 1024 * 1024;

bool HashManager::Hasher::fastHash(const string& fname, uint8_t* buf, TigerTree& tth, int64_t& p_size, bool p_is_link)
{
	int64_t l_size = p_size;
	HANDLE h = INVALID_HANDLE_VALUE;
	DWORD l_sector_size = 0;
	DWORD l_tmp;
	BOOL l_sector_result;
	// TODO - размер сектора определять один раз для одной буквы через массив от A до Z и не звать GetDiskFreeSpaceA
	// на каждый файл
	//
	// TODO - узнать зачем его вообще определять?
	// DONE: FILE_FLAG_NO_BUFFERING - отключает кэширование чтения\записи с диска\на диск
	// для увеличения производительности дискового ввода\вывода.
	// При этом читать\писать нужно (must) секторами.
	// Ещё там есть зависящее от диска (should) ограничение на выравнивание буфера в памяти.
	// https://msdn.microsoft.com/en-us/library/windows/desktop/cc644950(v=vs.85).aspx
	
	if (fname.size() >= 3 && fname[1] == ':')
	{
		char l_drive[4];
		memcpy(l_drive, fname.c_str(), 3);
		l_drive[3] = 0;
		l_sector_result = GetDiskFreeSpaceA(l_drive, &l_tmp, &l_sector_size, &l_tmp, &l_tmp);
	}
	else
	{
		l_sector_result = GetDiskFreeSpace(Text::toT(Util::getFilePath(fname)).c_str(), &l_tmp, &l_sector_size, &l_tmp, &l_tmp);
	}
	if (!l_sector_result)
	{
		return false;
		// TODO Залогировать ошибку.
	}
	else
	{
		if ((g_HashBufferSize % l_sector_size) != 0)
		{
			return false;
		}
		else
		{
			h = ::CreateFile(File::formatPath(Text::toT(fname)).c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			                 FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
			if (h == INVALID_HANDLE_VALUE)
			{
				// dcassert(0);
				return false;
			}
			else
			{
				if (p_is_link && p_size == 0) // iSymLink?
				{
					LARGE_INTEGER x = {0};
					BOOL bRet = ::GetFileSizeEx(h, &x);
					if (bRet == FALSE)
					{
						dcassert(0);
						return false;
					}
					p_size = x.QuadPart; // fix https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/14
					l_size = p_size;
				}
			}
		}
	}
	DWORD hn = 0;
	DWORD rn = 0;
	uint8_t* hbuf = buf + g_HashBufferSize;
	uint8_t* rbuf = buf;
	
	OVERLAPPED over = { 0 };
	BOOL res = TRUE;
	over.hEvent = CreateEvent(NULL, FALSE, TRUE, NULL);
	
	bool ok = false;
	
	uint64_t lastRead = GET_TICK();
	if (!::ReadFile(h, hbuf, g_HashBufferSize, &hn, &over))
	{
		const auto l_error =   GetLastError();
		if (l_error == ERROR_HANDLE_EOF)
		{
			hn = 0;
		}
		else if (l_error == ERROR_IO_PENDING)
		{
			if (!GetOverlappedResult(h, &over, &hn, TRUE))
			{
				const auto l_error_overlapped =   GetLastError();
				if (l_error_overlapped == ERROR_HANDLE_EOF)
				{
					hn = 0;
				}
				else
				{
					goto cleanup;
				}
			}
		}
		else
		{
			goto cleanup;
		}
	}
	
	over.Offset = hn;
	l_size -= hn;
	dcassert(l_size >= 0);
	// [+] brain-ripper
	// exit loop if "running" equals false.
	// "running" sets to false in stopHashing function
	while (!m_stop && m_running && l_size >= 0)
	{
		if (l_size > 0)
		{
			// Start a new overlapped read
			ResetEvent(over.hEvent);
			if (GetMaxHashSpeed() > 0)
			{
				const uint64_t now = GET_TICK();
				const uint64_t minTime = hn * 1000LL / (GetMaxHashSpeed() * 1024LL * 1024LL);
				if (lastRead + minTime > now)
				{
					const uint64_t diff = now - lastRead;
					sleep(minTime - diff);
				}
				lastRead = lastRead + minTime;
			}
			else
			{
				lastRead = GET_TICK();
			}
			res = ReadFile(h, rbuf, g_HashBufferSize, &rn, &over);
		}
		else
		{
			rn = 0;
		}
		
		tth.update(hbuf, hn);
		
		{
			FastLock l(cs);
			m_currentSize = max(m_currentSize - hn, _LL(0));
		}
		
		if (l_size == 0)
		{
			ok = true;
			break;
		}
		
		if (!res)
		{
			// deal with the error code
			switch (GetLastError())
			{
				case ERROR_IO_PENDING:
					if (!GetOverlappedResult(h, &over, &rn, TRUE))
					{
						dcdebug("Error 0x%x: %s\n", GetLastError(), Util::translateError().c_str());
						goto cleanup;
					}
					break;
				default:
					dcdebug("Error 0x%x: %s\n", GetLastError(), Util::translateError().c_str());
					goto cleanup;
			}
		}
		
		instantPause();
		
		*((uint64_t*)&over.Offset) += rn;
		l_size -= rn;
		
		std::swap(rbuf, hbuf);
		std::swap(rn, hn);
	}
	
cleanup:
	::CloseHandle(over.hEvent);
	::CloseHandle(h);
	return ok;
}

int HashManager::Hasher::run()
{
	setThreadPriority(Thread::IDLE);
	
	uint8_t* l_buf = nullptr;
	bool l_is_virtualBuf = true;
	bool l_is_last = false;
	for (;;)
	{
		m_s.wait();
		if (m_stop)
			break;
		if (m_rebuild)
		{
			HashManager::getInstance()->doRebuild();
			m_rebuild = false;
			LogManager::message(STRING(HASH_REBUILT));
			continue;
		}
		{
			FastLock l(cs);
			if (!w.empty())
			{
				m_fname = w.begin()->first;
				m_currentSize = w.begin()->second.m_file_size;
				m_path_id = w.begin()->second.m_path_id;
				m_CurrentBytesLeft -= m_currentSize;// [+]IRainman
				w.erase(w.begin());
				l_is_last = w.empty();
				if (!m_running)
				{
					uiStartTime = GET_TICK();
					m_running = true;
				}
			}
			else
			{
				m_currentSize = 0;
				l_is_last = true;
				m_fname.clear();
				m_running = false;
				iMaxBytes = 0;
				dwMaxFiles = 0;
				m_CurrentBytesLeft = 0;// [+]IRainman
			}
		}
		string l_fname;
		{
			FastLock l(cs);
			l_fname = m_fname;
		}
		if (!l_fname.empty())
		{
			int64_t l_size = 0;
			int64_t l_outFiletime = 0;
			bool l_is_link = false;
			File::isExist(l_fname, l_size, l_outFiletime, l_is_link); // TODO - вернуть признак isLink
			int64_t l_sizeLeft = l_size;
#ifdef _WIN32
			if (l_buf == NULL)
			{
				l_is_virtualBuf = true;
				l_buf = (uint8_t*)VirtualAlloc(NULL, g_HashBufferSize, MEM_COMMIT, PAGE_READWRITE);
			}
#endif
			if (l_buf == NULL)
			{
				l_is_virtualBuf = false;
				bool l_is_bad_alloc;
				do
				{
					try
					{
						dcassert(g_HashBufferSize);
						l_is_bad_alloc = false;
						l_buf = new uint8_t[g_HashBufferSize];
					}
					catch (std::bad_alloc&)
					{
						l_buf = nullptr;
						g_HashBufferSize /= 2;
						l_is_bad_alloc = g_HashBufferSize > 128;
						if (l_is_bad_alloc == false)
						{
							throw;
						}
					}
				}
				while (l_is_bad_alloc == true);
			}
			try
			{
				if (l_size == 0 && l_is_link)
				{
					File f(l_fname, File::READ, File::OPEN);
					l_size = f.getSize(); // fix https://github.com/pavel-pimenov/flylinkdc-r5xx/issues/15
				}
				const int64_t bs = TigerTree::getMaxBlockSize(l_size);
				const uint64_t start = GET_TICK();
				const int64_t timestamp = l_outFiletime;
				int64_t speed = 0;
				size_t n = 0;
				TigerTree fastTTH(bs);
				TigerTree slowTTH(bs);
				TigerTree* tth = &fastTTH;
				bool l_is_ntfs = false;
#ifdef IRAINMAN_NTFS_STREAM_TTH
				if (l_size > 0 && HashManager::getInstance()->m_streamstore.loadTree(l_fname, fastTTH, l_size)) //[+]IRainman
				{
					l_is_ntfs = true; //[+]PPA
					LogManager::message(STRING(LOAD_TTH_FROM_NTFS) + ' ' + l_fname); //[!]NightOrion(translate)
				}
#endif
#ifdef _WIN32
#ifdef IRAINMAN_NTFS_STREAM_TTH
				if (!l_is_ntfs)
				{
#endif
					if (l_is_virtualBuf == false || !BOOLSETTING(FAST_HASH) || !fastHash(l_fname, l_buf, fastTTH, l_size, l_is_link))
					{
#else
				if (!BOOLSETTING(FAST_HASH) || !fastHash(fname, 0, fastTTH, l_size))
				{
#endif
						// [+] brain-ripper
						if (m_running)
						{
							tth = &slowTTH;
							uint64_t lastRead = GET_TICK();
							File l_slow_file_reader(l_fname, File::READ, File::OPEN);
							do
							{
								size_t bufSize = g_HashBufferSize;
								
								if (GetMaxHashSpeed() > 0) // [+] brain-ripper
								{
									const uint64_t now = GET_TICK();
									const uint64_t minTime = n * 1000LL / (GetMaxHashSpeed() * 1024LL * 1024LL);
									if (lastRead + minTime > now)
									{
										sleep(minTime - (now - lastRead));
									}
									lastRead = lastRead + minTime;
								}
								else
								{
									lastRead = GET_TICK();
								}
								n = l_slow_file_reader.read(l_buf, bufSize);
								if (n > 0) // [+]PPA
								{
									tth->update(l_buf, n);
									{
										FastLock l(cs);
										m_currentSize = max(static_cast<uint64_t>(m_currentSize - n), static_cast<uint64_t>(0)); // TODO - max от 0 для беззнакового?
									}
									l_sizeLeft -= n;
									
									instantPause();
								}
							}
							while (!m_stop && m_running && n > 0);
						}
						else
							tth = nullptr;
					}
					else
					{
						l_sizeLeft = 0; // Variable 'l_sizeLeft' is assigned a value that is never used.
					}
#ifdef IRAINMAN_NTFS_STREAM_TTH
				}
#endif
				const uint64_t end = GET_TICK();
				if (end > start) // TODO: Why is not possible?
				{
					speed = l_size * _LL(1000) / (end - start);
				}
				if (m_running)
				{
					if (m_path_id == 0)
					{
						//dcassert(m_path_id);
						const auto l_path = Text::toLower(Util::getFilePath(l_fname));
						dcassert(!l_path.empty());
						bool l_is_no_mediainfo;
						m_path_id = CFlylinkDBManager::getInstance()->get_path_id(l_path, false, false, l_is_no_mediainfo, false);
						dcassert(m_path_id);
					}
#ifdef IRAINMAN_NTFS_STREAM_TTH
					if (l_is_ntfs)
					{
						HashManager::getInstance()->hashDone(m_path_id, l_fname, timestamp, *tth, speed, l_is_ntfs, l_size);
					}
					else
#endif
						if (tth)
						{
							tth->finalize();
							HashManager::getInstance()->hashDone(m_path_id, l_fname, timestamp, *tth, speed, l_is_ntfs, l_size);
						}
				}
			}
			catch (const FileException& e)
			{
				LogManager::message(STRING(ERROR_HASHING) + ' ' + l_fname + ": " + e.getError());
			}
		}
		{
			FastLock l(cs);
			m_fname.clear();
			m_currentSize = 0;
			m_path_id = 0;
			
			if (w.empty())
			{
				m_running = false;
				iMaxBytes = 0;
				dwMaxFiles = 0;
				m_CurrentBytesLeft = 0;//[+]IRainman
			}
		}
		
		if (l_buf != NULL && (l_is_last || m_stop))
		{
			if (l_is_virtualBuf)
				VirtualFree(l_buf, 0, MEM_RELEASE);
			else
				delete [] l_buf;
			l_buf = nullptr;
		}
	}
	return 0;
}

HashManager::HashPauser::HashPauser()
{
	resume = !HashManager::getInstance()->pauseHashing();
}

HashManager::HashPauser::~HashPauser()
{
	if (resume)
	{
		HashManager::getInstance()->resumeHashing();
	}
}

bool HashManager::pauseHashing()
{
	return hasher.pause();
}

void HashManager::resumeHashing()
{
	hasher.resume();
}

bool HashManager::isHashingPaused() const
{
	return hasher.isPaused();
}

/**
 * @file
 * $Id: HashManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
