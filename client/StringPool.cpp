/*
 * Copyright (C) 2016 FlylinkDC++ Team http://flylinkdc.com
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

#ifdef IRAINMAN_USE_STRING_POOL

#include "StringPool.h"

StringPool::StringMap StringPool::g_stringMap;
FastCriticalSection StringPool::g_csStringMap;

void StringPool::on(TimerManagerListener::Minute, uint64_t /*aTick*/) noexcept
{
	CFlyFastLock(g_csStringMap);
	auto i = g_stringMap.cbegin();
	while (i != g_stringMap.end())
	{
		if (i->second == 0)
		{
			g_stringMap.erase(i++);
		}
		else
		{
			++i;
		}
	}
}

#endif // IRAINMAN_USE_STRING_POOL