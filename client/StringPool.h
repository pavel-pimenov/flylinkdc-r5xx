/*
 * Copyright (C) 2013 FlylinkDC++ Team http://flylinkdc.com/
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

#ifndef STRING_POOL_H
#define STRING_POOL_H

#ifdef IRAINMAN_USE_STRING_POOL

#include "CFlyThread.h"
#include "TimerManager.h"

class StringPool : public Singleton<StringPool>, private TimerManagerListener
{
		friend Singleton<StringPool>;
		
		typedef boost::unordered_map<string, volatile long> StringMap;
		
		static StringMap g_stringMap;
		static FastCriticalSection g_csStringMap;
		
	public:
	
		StringPool()
		{
			TimerManager::getInstance()->addListener(this);
		}
		~StringPool()
		{
			TimerManager::getInstance()->removeListener(this);
		}
		
		typedef pair<const string, volatile long>* StringPoolItemPtr;
		
		static StringPoolItemPtr addString(const string& str)
		{
			StringPoolItemPtr ptr;
			{
				FastLock l(g_csStringMap);
				auto i = g_stringMap.insert(make_pair(str, 0));
				ptr = &(*i.first);
			}
			Thread::safeInc(ptr->second);
			return ptr;
		}
		static void removeString(StringPoolItemPtr ptr)
		{
			Thread::safeDec(ptr->second);
		}
		void on(TimerManagerListener::Minute, uint64_t /*aTick*/) noexcept;
};

#endif // IRAINMAN_USE_STRING_POOL

#endif // STRING_POOL_H