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

#ifndef _THROTTLEMANAGER_H
#define _THROTTLEMANAGER_H

#include "Socket.h"
#include "TimerManager.h"
#include "SettingsManager.h"
#include <boost/thread/condition_variable.hpp>

/**
 * Manager for throttling traffic flow speed.
 * Inspired by Token Bucket algorithm: http://en.wikipedia.org/wiki/Token_bucket
 */
class ThrottleManager :
	public Singleton<ThrottleManager>, private TimerManagerListener
{
	public:
	
		/*
		 * Limits a traffic and reads a packet from the network
		 */
		int read(Socket* sock, void* buffer, size_t len);
		
		/*
		 * Limits a traffic and writes a packet to the network
		 * We must handle this a little bit differently than downloads, because of that stupidity in OpenSSL
		 */
		int write(Socket* sock, const void* buffer, size_t& len);
		
		/*
		 * Returns current download limit.
		 */
		size_t getDownloadLimitInKBytes() const
		{
			return downLimit / 1024;
		}
		
		size_t getDownloadLimitInBytes() const
		{
			return downLimit;
		}
		
		void setDownloadLimit(size_t p_NewDownLimit) //[+]IRainman SpeedLimiter
		{
			downLimit = p_NewDownLimit * 1024;
		}
		
		/*
		 * Returns current download limit.
		 */
		size_t getUploadLimitInKBytes() const
		{
			return upLimit / 1024;
		}
		
		size_t getUploadLimitInBytes() const
		{
			return upLimit;
		}
		
		void setUploadLimit(size_t p_NewUploadLimit) //[+]IRainman SpeedLimiter
		{
			upLimit = p_NewUploadLimit * 1024;
		}
		
		void updateLimits();// [+] IRainman SpeedLimiter
		
		void startup()
		{
			TimerManager::getInstance()->addListener(this);
			updateLimits();
		}
	private:
		// download limiter
		size_t             downLimit;
		size_t                     downTokens;
		boost::condition_variable   downCond;
		boost::mutex                downMutex;
		
		// upload limiter
		size_t             upLimit;
		size_t                     upTokens;
		boost::condition_variable   upCond;
		boost::mutex                upMutex;
		
		friend class Singleton<ThrottleManager>;
		
		ThrottleManager(void);
		
		~ThrottleManager(void);
		
		// TimerManagerListener
		void on(TimerManagerListener::Minute, uint64_t aTick) noexcept;//[+] IRainman SpeedLimiter
		void on(TimerManagerListener::Second, uint64_t aTick) noexcept;
};

#endif  // _THROTTLEMANAGER_H