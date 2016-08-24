/*
 * Copyright (C) 2003-2005 RevConnect, http://www.revconnect.com
 * Copyright (C) 2011      Big Muscle, http://strongdc.sf.net
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
#include "SharedFileStream.h"
#include "LogManager.h"
#include "ClientManager.h"
#include "../FlyFeatures/flyServer.h"

FastCriticalSection SharedFileStream::g_shares_file_cs;
std::set<char> SharedFileStream::g_error_map_file;
#ifdef FLYLINKDC_USE_SHARED_FILE_STREAM_RW_POOL
SharedFileStream::SharedFileHandleMap SharedFileStream::g_readpool;
SharedFileStream::SharedFileHandleMap SharedFileStream::g_writepool;
#else
SharedFileStream::SharedFileHandleMap SharedFileStream::g_rwpool;
std::unordered_set<std::string> SharedFileStream::g_shared_stream_errors;
#endif

SharedFileHandle::SharedFileHandle(const string& aPath, int aAccess, int aMode) :
	m_ref_cnt(1), m_path(aPath), m_mode(aMode), m_access(aAccess), m_last_file_size(0),
	m_map_file(INVALID_HANDLE_VALUE), m_map_file_ptr(nullptr), m_is_map_file_error(false)
{
}
void SharedFileHandle::CloseMapFile()
{
	if (m_map_file != INVALID_HANDLE_VALUE)
	{
		if (m_map_file_ptr)
		{
		
			if (UnmapViewOfFile(m_map_file_ptr) == FALSE)
			{
				LogManager::message("Error UnmapViewOfFile " + m_path + " Error = " + Util::translateError());
				dcassert(0);
			}
			m_map_file_ptr = nullptr;
		}
		CloseHandle(m_map_file);
		m_map_file = INVALID_HANDLE_VALUE;
	}
	dcassert(m_map_file_ptr == nullptr);
}
SharedFileHandle::~SharedFileHandle()
{
	CloseMapFile();
}

void SharedFileHandle::init(int64_t p_file_size)
{
	m_file.init(Text::toT(m_path), m_access, m_mode, true);
	m_last_file_size = m_file.getSize();
	if (p_file_size == 0 && m_last_file_size > 0)
	{
		p_file_size = m_last_file_size;
	}
	// TODO - поддержать чтение через мапинг
	if ((m_access == File::WRITE || m_access == File::RW) && p_file_size && (p_file_size < int64_t(2) * 1024 * 1024 * 1024))
	{
		if (!m_path.empty() && SharedFileStream::g_error_map_file.find(m_path[0]) == SharedFileStream::g_error_map_file.end())
		{
			m_map_file = CreateFileMapping(m_file.getHandle(), NULL, PAGE_READWRITE | SEC_NOCACHE, 0, p_file_size, NULL);
			if (m_map_file != NULL)
			{
				m_map_file_ptr = (char*)MapViewOfFile(m_map_file, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, p_file_size);
				if (!m_map_file_ptr)
				{
				
					if (!m_is_map_file_error)
					{
						LogManager::message("Error MapViewOfFile " + m_path + " Error = " + Util::translateError());
						m_is_map_file_error = true;
					}
					dcassert(0);
					CloseMapFile();
				}
			}
			else
			{
				//dcassert(0);
				if (!m_is_map_file_error)
				{
					const auto l_error = GetLastError();
					if (l_error == 87)
					{
						SharedFileStream::g_error_map_file.insert(m_path[0]);
					}
					LogManager::message("Error mapped file " + m_path + " ErrorCode = " + Util::toString(l_error));
					m_is_map_file_error = true;
				}
			}
		}
	}
}

SharedFileStream::SharedFileStream(const string& aFileName, int aAccess, int aMode, int64_t p_file_size)
{
	dcassert(!aFileName.empty());
	CFlyFastLock(g_shares_file_cs);
	m_pos = 0;
#ifdef FLYLINKDC_USE_SHARED_FILE_STREAM_RW_POOL
	auto& pool = aAccess == File::READ ? g_readpool : g_writepool;
#else
	auto& l_pool = g_rwpool;
#endif
	auto p = l_pool.find(aFileName);
	if (p != l_pool.end())
	{
#ifdef _DEBUG
		// LogManager::message("Share SharedFileHandle aFileName = " + aFileName);
#endif
		m_sfh = p->second;
		m_sfh->m_ref_cnt++;
		dcassert(m_sfh->m_access == aAccess);
		dcassert(m_sfh->m_mode == aMode);
	}
	else
	{
#ifdef _DEBUG
		LogManager::message("new SharedFileHandle aFileName = " + aFileName);
#endif
		m_sfh = std::make_shared<SharedFileHandle>(aFileName, aAccess, aMode);
		try
		{
			m_sfh->init(p_file_size);
		}
		catch (FileException& e)
		{
			m_sfh.reset();
			const auto l_error = "error r5xx SharedFileStream::SharedFileStream aFileName = "
			                     + aFileName + " Error = " + e.getError() + " Access = " + Util::toString(aAccess) + " Mode = " + Util::toString(aMode);
			const auto l_dup_filter = g_shared_stream_errors.insert(l_error);
			if (l_dup_filter.second == true)
			{
				CFlyServerJSON::pushError(9, l_error);
				const tstring l_email_message = Text::toT(string("\r\nError in SharedFileStream::SharedFileStream. aFileName = [") + aFileName + "]\r\n" +
				                                          "Error = " + e.getError() + "\r\nSend screenshot (or text - press ctrl+c for copy to clipboard) e-mail ppa74@ya.ru for diagnostic error!");
				::MessageBox(NULL, l_email_message.c_str(), getFlylinkDCAppCaptionWithVersionT().c_str(), MB_OK | MB_ICONERROR);
			}
			else
			{
				LogManager::message(l_error);
			}
			throw;
		}
		l_pool[aFileName] = m_sfh;
	}
}

void SharedFileStream::check_before_destoy()
{
	{
		CFlyFastLock(g_shares_file_cs);
#ifdef FLYLINKDC_USE_SHARED_FILE_STREAM_RW_POOL
		auto& l_pool = m_sfh->m_mode == File::READ ? g_readpool : g_writepool;
#else
		auto& l_pool = g_rwpool;
#endif
		dcassert(l_pool.empty());
	}
	cleanup();
}
// TODO - убрать
void SharedFileStream::cleanup()
{
	CFlyFastLock(g_shares_file_cs);
#ifdef FLYLINKDC_USE_SHARED_FILE_STREAM_RW_POOL
	auto& l_pool = m_sfh->m_mode == File::READ ? g_readpool : g_writepool;
#else
	auto& l_pool = g_rwpool;
#endif
	for (auto i = l_pool.begin(); i != l_pool.end();)
	{
		if (i->second && i->second->m_ref_cnt == 0)
		{
			dcassert(0); // Разрушаем в SharedFileStream::~SharedFileStream()
#ifdef _DEBUG
			LogManager::message("[!] SharedFileStream::cleanup() aFileName = " + i->first);
#endif
			l_pool.erase(i);
			i = l_pool.begin();
		}
		else
		{
			++i;
		}
	}
}
SharedFileStream::~SharedFileStream()
{
	CFlyFastLock(g_shares_file_cs);
	
	m_sfh->m_ref_cnt--;
	if (m_sfh->m_ref_cnt == 0)
	{
#ifdef _DEBUG
		LogManager::message("m_ref_cnt = 0 ~SharedFileHandle aFileName = " + m_sfh->m_path);
#endif
#ifdef FLYLINKDC_USE_SHARED_FILE_STREAM_RW_POOL
		auto& l_pool = m_sfh->m_mode == File::READ ? g_readpool : g_writepool;
#else
		auto& l_pool = g_rwpool;
#endif
		dcassert(l_pool.find(m_sfh->m_path) != l_pool.end());
		l_pool.erase(m_sfh->m_path);
	}
}

size_t SharedFileStream::write(const void* p_buf, size_t p_len)
{
#ifdef _DEBUG
	//LogManager::message("SharedFileStream::write buf = " + Util::toString(int(buf)) + " len " + Util::toString(len));
#endif
	CFlyFastLock(m_sfh->m_cs);
	if (m_sfh->m_map_file_ptr)
	{
		memcpy(m_sfh->m_map_file_ptr + m_pos, p_buf, p_len);
		m_pos += p_len;
	}
	else
	{
		m_sfh->m_file.setPos(m_pos);
		m_sfh->m_file.write(p_buf, p_len);
		m_pos += p_len;
	}
	if (m_sfh->m_last_file_size < m_pos)
	{
		dcassert(0);
		m_sfh->m_last_file_size = m_pos;
	}
	return p_len;
}

size_t SharedFileStream::read(void* p_buf, size_t& p_len)
{
	CFlyFastLock(m_sfh->m_cs);
#ifdef _DEBUG
	//LogManager::message("SharedFileStream::read buf = " + Util::toString(buf) + " len " + Util::toString(len));
#endif
	
	m_sfh->m_file.setPos(m_pos);
	p_len = m_sfh->m_file.read(p_buf, p_len);
	m_pos += p_len;
	return p_len;
}

/*
int64_t SharedFileStream::getFileSize()
{
    CFlyFastLock(m_sfh->m_cs);
#ifdef _DEBUG
    //LogManager::message("SharedFileStream::getFileSize size = " +  Util::toString(m_sfh->m_file.getSize()));
#endif
    return m_sfh->m_file.getSize();
}
*/

int64_t SharedFileStream::getFastFileSize()
{
	CFlyFastLock(m_sfh->m_cs);
#ifdef _DEBUG
	//LogManager::message("SharedFileStream::getFastFileSize size = " +  Util::toString(m_sfh->m_file.getSize()));
#endif
	//dcassert(m_sfh->m_last_file_size == m_sfh->m_file.getSize());
	return m_sfh->m_last_file_size;
}


void SharedFileStream::setSize(int64_t p_new_size)
{
	CFlyFastLock(m_sfh->m_cs);
#ifdef _DEBUG
	//LogManager::message("SharedFileStream::setSize size = " +  Util::toString(newSize));
#endif
	m_sfh->m_file.setSize(p_new_size);
	m_sfh->m_last_file_size = p_new_size;
}

size_t SharedFileStream::flushBuffers(bool aForce)
{
	if (!ClientManager::isShutdown()) // fix https://drdump.com/Problem.aspx?ProblemID=130529
		// при закрытии файлов - буфера и так скидываются на винты.
	{
		try
		{
			CFlyFastLock(m_sfh->m_cs);
			if (m_sfh->m_map_file_ptr)
			{
				return 0;
			}
			else
			{
				return m_sfh->m_file.flushBuffers(aForce);
			}
		}
		catch (const Exception& e)
		{
			dcassert(0);
			//LogManager::message("SharedFileStream::flush() = " + e.getError());
		}
	}
	return 0;
}

void SharedFileStream::setPos(int64_t p_pos)
{
	CFlyFastLock(m_sfh->m_cs);
#ifdef _DEBUG
	//LogManager::message("SharedFileStream::setPos aPos = " +  Util::toString(aPos));
#endif
	m_pos = p_pos;
}


