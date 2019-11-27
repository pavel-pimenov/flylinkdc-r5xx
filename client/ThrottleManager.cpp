/*
 * Copyright (C) 2009-2013 Big Muscle
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
#include "ThrottleManager.h"

#include "DownloadManager.h"
#include "UploadManager.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#ifdef FLYLINKDC_USE_BOOST_LOCK
// #include <boost/thread/condition_variable.hpp>
// #include <boost/thread.hpp>
// #include <boost/detail/lightweight_mutex.hpp>
#endif

#define CONDWAIT_TIMEOUT        250

ThrottleManager::ThrottleManager(void) : downTokens(0), upTokens(0), downLimit(0), upLimit(0)
{
}

ThrottleManager::~ThrottleManager(void)
{
	TimerManager::getInstance()->removeListener(this);
	
	// release conditional variables on exit
	downCond.notify_all();
	upCond.notify_all();
}

/*
 * Limits a traffic and reads a packet from the network
 */
int ThrottleManager::read(Socket* sock, void* buffer, size_t len)
{
	const size_t downs = downLimit ? DownloadManager::getDownloadCount() : 0;
	if (downLimit == 0 || downs == 0)
		return sock->read(buffer, len);
		
	boost::unique_lock<boost::mutex> lock(downMutex);
	
	if (downTokens > 0)
	{
		const size_t slice = getDownloadLimitInBytes() / downs;
		size_t readSize = std::min(slice, std::min(len, downTokens));
		
		// read from socket
		readSize = sock->read(buffer, readSize);
		
		if (readSize > 0)
			downTokens -= readSize;
			
		// next code can't be in critical section, so we must unlock here
		lock.unlock();
		
		// give a chance to other transfers to get a token
		Thread::yield();
		return readSize;
	}
	
	// no tokens, wait for them
	downCond.timed_wait(lock, boost::posix_time::millisec(CONDWAIT_TIMEOUT));
	return -1;  // from BufferedSocket: -1 = retry, 0 = connection close
}

/*
 * Limits a traffic and writes a packet to the network
 * We must handle this a little bit differently than downloads, because of that stupidity in OpenSSL
 */
int ThrottleManager::write(Socket* p_sock, const void* p_buffer, size_t& p_len)
{
	const auto currentMaxSpeed = p_sock->getMaxSpeed();
	if (currentMaxSpeed < 0) // SU
	{
		// write to socket
		const int sent = p_sock->write(p_buffer, p_len);
		return sent;
	}
	else if (currentMaxSpeed > 0) // individual
	{
		// Apply individual restriction to the user if it is
		const int64_t l_currentBucket = p_sock->getCurrentBucket();
		p_len = std::min(p_len, static_cast<size_t>(l_currentBucket));
		p_sock->setCurrentBucket(l_currentBucket - p_len);
		
		// write to socket
		const int sent = p_sock->write(p_buffer, p_len);
		return sent;
	}
	else // general
	{
		if (upLimit == 0 || UploadManager::getUploadCount() == 0)
		{
			const int sent = p_sock->write(p_buffer, p_len);
			return sent;
		}
		
		boost::unique_lock<boost::mutex> lock(upMutex);
		if (upTokens > 0)
		{
			const size_t ups = UploadManager::getUploadCount();
			const size_t slice = getUploadLimitInBytes() / (ups ? ups : 1);
			p_len = std::min(slice, std::min(p_len, upTokens));
			
			// Pour buckets of the calculated number of bytes,
			// but as a real restriction on the specified number of bytes
			upTokens -= p_len;
			
			// next code can't be in critical section, so we must unlock here
			lock.unlock();
			
			// write to socket
			const int sent = p_sock->write(p_buffer, p_len);
			
			// give a chance to other transfers to get a token
			Thread::yield();
			return sent;
		}
		
		// no tokens, wait for them
		upCond.timed_wait(lock, boost::posix_time::millisec(CONDWAIT_TIMEOUT));
		return 0;   // from BufferedSocket: -1 = failed, 0 = retry
	}
}

// TimerManagerListener
void ThrottleManager::on(TimerManagerListener::Second, uint64_t /*aTick*/) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	if (!BOOLSETTING(THROTTLE_ENABLE))
	{
		downLimit = 0;
		upLimit = 0;
		return;
	}
	// readd tokens
	if (downLimit > 0)
	{
		boost::lock_guard<boost::mutex> lock(downMutex);
		downTokens = downLimit;
		downCond.notify_all();
	}
	
	if (upLimit > 0)
	{
		boost::lock_guard<boost::mutex> lock(upMutex);
		upTokens = upLimit;
		upCond.notify_all();
	}
}

void ThrottleManager::on(TimerManagerListener::Minute, uint64_t /*aTick*/) noexcept
{
	if (!BOOLSETTING(THROTTLE_ENABLE))
		return;
		
	updateLimits();
}

static bool isAltLimiterTime()
{
	if (!SETTING(TIME_DEPENDENT_THROTTLE))
		return false;
		
	time_t currentTime;
	time(&currentTime);
	const auto currentHour = localtime(&currentTime)->tm_hour;
	const auto s = SETTING(BANDWIDTH_LIMIT_START);
	const auto e = SETTING(BANDWIDTH_LIMIT_END);
	return ((s < e &&
	         currentHour >= s && currentHour < e) ||
	        (s > e &&
	         (currentHour >= s || currentHour < e)));
}

void ThrottleManager::updateLimits()
{
	if (isAltLimiterTime())
	{
		setUploadLimit(SETTING(MAX_UPLOAD_SPEED_LIMIT_TIME));
		setDownloadLimit(SETTING(MAX_DOWNLOAD_SPEED_LIMIT_TIME));
	}
	else
	{
		setUploadLimit(SETTING(MAX_UPLOAD_SPEED_LIMIT_NORMAL));
		setDownloadLimit(SETTING(MAX_DOWNLOAD_SPEED_LIMIT_NORMAL));
	}
}
