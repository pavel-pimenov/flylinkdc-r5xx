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


#pragma once

#ifndef DCPLUSPLUS_DCPP_STREAMS_H
#define DCPLUSPLUS_DCPP_STREAMS_H

#include "SettingsManager.h"
#include "ResourceManager.h"

STANDARD_EXCEPTION(FileException);

/**
 * A simple output stream. Intended to be used for nesting streams one inside the other.
 */
class OutputStream
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		//OutputStream() { } [-] IRainman.
		virtual ~OutputStream() {}
		
		/**
		 * @return The actual number of bytes written. len bytes will always be
		 *         consumed, but fewer or more bytes may actually be written,
		 *         for example if the stream is being compressed.
		 */
		virtual size_t write(const void* p_buf, size_t p_len) = 0;
		/**
		* This must be called before destroying the object to make sure all data
		* is properly written (we don't want destructors that throw exceptions
		* and the last flush might actually throw). Note that some implementations
		* might not need it...
		*
		* If aForce is false, only data that is subject to be deleted otherwise will be flushed.
		* This applies especially for files for which the operating system should generally decide
		* when the buffered data is flushed on disk.
		*/
		
		virtual size_t flushBuffers(bool aForce) = 0;
		/* This only works for file streams */
		virtual void setPos(int64_t /*p_pos*/) { }
		
		/**
		 * @return True if stream is at expected end
		 */
		virtual bool eof() const
		{
			return false;
		}
		
		size_t write(const string& str)
		{
			return write(str.c_str(), str.size());
		}
};

class InputStream
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		//InputStream() { } [-] IRainman.
		virtual ~InputStream() {}
		/**
		 * Call this function until it returns 0 to get all bytes.
		 * @return The number of bytes read. len reflects the number of bytes
		 *         actually read from the stream source in this call.
		 */
		virtual size_t read(void* p_buf, size_t& p_len) = 0;
		/* This only works for file streams */
		virtual void setPos(int64_t /*p_pos*/) { }
		
		virtual void clean_stream()
		{
		}
};

class MemoryInputStream : public InputStream
{
	public:
		MemoryInputStream(const uint8_t* src, size_t len) : m_pos(0), m_buf_size(len), m_buf(new uint8_t[len])
		{
			memcpy(m_buf, src, len);
		}
		explicit MemoryInputStream(const string& src) : m_pos(0), m_buf_size(src.size()), m_buf(src.size() ? new uint8_t[src.size()] : nullptr)
		{
			dcassert(src.size());
			memcpy(m_buf, src.data(), src.size());
		}
		
		~MemoryInputStream()
		{
			delete[] m_buf;
		}
		
		size_t read(void* tgt, size_t& p_len) override
		{
			p_len = std::min(p_len, m_buf_size - m_pos);
			memcpy(tgt, m_buf + m_pos, p_len);
			m_pos += p_len;
			return p_len;
		}
		
		size_t getSize() const
		{
			return m_buf_size;
		}
		
	private:
		size_t m_pos;
		size_t m_buf_size;
		uint8_t* m_buf;
};

class IOStream : public InputStream, public OutputStream
{
};

template<const bool managed>
class LimitedInputStream : public InputStream
{
	public:
		explicit LimitedInputStream(InputStream* is, int64_t aMaxBytes) : s(is), maxBytes(aMaxBytes)
		{
		}
		~LimitedInputStream()
		{
			if (managed)
			{
				delete s;
			}
		}
		
		size_t read(void* buf, size_t& len) override
		{
			dcassert(maxBytes >= 0);
			len = (size_t)min(maxBytes, (int64_t)len);
			if (len == 0)
				return 0;
			size_t x = s->read(buf, len);
			maxBytes -= x;
			return x;
		}
		void clean_stream() override
		{
			s = nullptr;
		}
		
	private:
		InputStream* s;
		int64_t maxBytes;
};

/** Limits the number of bytes that are requested to be written (not the number actually written!) */
class LimitedOutputStream : public OutputStream
{
	public:
		explicit LimitedOutputStream(OutputStream* os, uint64_t aMaxBytes) : s(os), maxBytes(aMaxBytes)
		{
		}
		~LimitedOutputStream()
		{
			delete s;
		}
		
		size_t write(const void* buf, size_t len) override
		{
			//dcassert(len > 0);
			if (maxBytes < len)
			{
				throw FileException(STRING(TOO_MUCH_DATA));
			}
			maxBytes -= len;
			return s->write(buf, len);
		}
		
		size_t flushBuffers(bool aForce) override
		{
			return s->flushBuffers(aForce);
		}
		
		
		virtual bool eof() const
		{
			return maxBytes == 0;
		}
		
	private:
		OutputStream* s;
		uint64_t maxBytes;
};

template<const bool managed>
class BufferedOutputStream : public OutputStream
{
	public:
		using OutputStream::write;
		
		explicit BufferedOutputStream(OutputStream* aStream, size_t aBufSize) : s(aStream),
			m_pos(0), m_buf_size(aBufSize), m_buf(aBufSize ? new uint8_t[aBufSize] : nullptr) //, m_is_flush(false)
		{
		}
		~BufferedOutputStream()
		{
			// https://drdump.com/UploadedReport.aspx?DumpID=3549421
			try
			{
				// We must do this in order not to lose bytes when a download
				// is disconnected prematurely
				flushBuffers(true);
			}
			catch (const Exception&)
			{
			}
			if (managed)
			{
				delete s;
			}
			delete [] m_buf;
		}
		
		size_t flushBuffers(bool aForce) override
		{
			if (m_pos > 0)
			{
				s->write(&m_buf[0], m_pos);
				//m_is_flush = false;
			}
			m_pos = 0;
			// Делаем сброс пока всегда.
			// if (m_is_flush == false)
			{
				s->flushBuffers(aForce);
				//m_is_flush = true;
			}
			return 0;
		}
		
		size_t write(const void* wbuf, size_t len) override
		{
			uint8_t* b = (uint8_t*)wbuf;
			size_t l2 = len;
			while (len > 0)
			{
				if (m_pos == 0 && len >= m_buf_size)
				{
					s->write(b, len);
					//m_is_flush = false;
					break;
				}
				else
				{
					size_t n = min(m_buf_size - m_pos, len);
					memcpy(&m_buf[m_pos], b, n);
					b += n;
					m_pos += n; /// [1] ? https://www.box.net/shared/2a9b903842d575efa031
					len -= n;
					dcassert(m_buf_size != 0);
					if (m_pos == m_buf_size)
					{
						s->write(&m_buf[0], m_buf_size);
						//m_is_flush = false; // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=132490
						m_pos = 0;
					}
				}
			}
			return l2;
		}
	private:
		OutputStream* s;
		size_t m_pos;
		size_t m_buf_size;
		uint8_t* m_buf;
		//  bool m_is_flush;
};

class StringOutputStream : public OutputStream
{
	public:
		explicit StringOutputStream(string& p_out) : m_str(p_out) { }
		using OutputStream::write;
		
		size_t flushBuffers(bool aForce) override
		{
			return 0;
		}
		size_t write(const void* p_buf, size_t p_len) override
		{
			m_str.append((char*)p_buf, p_len);
			return p_len;
		}
	private:
		string& m_str;
};

#endif // !defined(STREAMS_H)

/**
* @file
* $Id: Streams.h 568 2011-07-24 18:28:43Z bigmuscle $
*/