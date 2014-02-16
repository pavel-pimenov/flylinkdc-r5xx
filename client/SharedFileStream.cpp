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

FastCriticalSection SharedFileStream::g_cs;
SharedFileStream::SharedFileHandleMap SharedFileStream::g_readpool;
SharedFileStream::SharedFileHandleMap SharedFileStream::g_writepool;

SharedFileHandle::SharedFileHandle(const string& aPath, int aAccess, int aMode) :
	File(aPath, aAccess, aMode), m_ref_cnt(1), m_path(aPath), m_mode(aMode)
{
}

SharedFileStream::SharedFileStream(const string& aFileName, int aAccess, int aMode)
{
	FastLock l(g_cs);
	auto& pool = aAccess == File::READ ? g_readpool : g_writepool;
	auto p = pool.find(aFileName);
	if (p != pool.end())
	{
		m_sfh = p->second.get();
		m_sfh->m_ref_cnt++;
	}
	else
	{
		m_sfh = new SharedFileHandle(aFileName, aAccess, aMode);
		pool[aFileName] = unique_ptr<SharedFileHandle>(m_sfh);
	}
}

SharedFileStream::~SharedFileStream()
{
	FastLock l(g_cs);
	
	m_sfh->m_ref_cnt--;
	if (m_sfh->m_ref_cnt == 0)
	{
		auto& pool = m_sfh->m_mode == File::READ ? g_readpool : g_writepool;
		pool.erase(m_sfh->m_path);
	}
}

size_t SharedFileStream::write(const void* buf, size_t len)
{
	FastLock l(m_sfh->m_cs);
	
	m_sfh->setPos(m_pos);
	m_sfh->write(buf, len);
	
	m_pos += len;
	return len;
}

size_t SharedFileStream::read(void* buf, size_t& len)
{
	FastLock l(m_sfh->m_cs);
	
	m_sfh->setPos(m_pos);
	len = m_sfh->read(buf, len);
	
	m_pos += len;
	return len;
}

int64_t SharedFileStream::getSize() const
{
	FastLock l(m_sfh->m_cs);
	return m_sfh->getSize();
}

void SharedFileStream::setSize(int64_t newSize)
{
	FastLock l(m_sfh->m_cs);
	m_sfh->setSize(newSize);
}

size_t SharedFileStream::flush()
{
	FastLock l(m_sfh->m_cs);
	return m_sfh->flush();
}

void SharedFileStream::setPos(int64_t aPos)
{
	FastLock l(m_sfh->m_cs);
	m_pos = aPos;
}


