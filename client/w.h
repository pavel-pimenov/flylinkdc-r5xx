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

#ifndef DCPLUSPLUS_DCPP_W_H_
#define DCPLUSPLUS_DCPP_W_H_

#include "compiler.h"
#include <boost/atomic.hpp>

#ifdef _WIN32

#include "w_flylinkdc.h"

#ifndef STRICT
#define STRICT 1
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX 1
#endif

#include <windows.h>
#include <mmsystem.h>
#include <tchar.h>

#endif
class CFlyTickDelta
{
	private:
		DWORD& m_tc;
	public:
		CFlyTickDelta(DWORD& p_tc): m_tc(p_tc)
		{
			m_tc = GetTickCount();
		}
		~CFlyTickDelta()
		{
			m_tc = GetTickCount() - m_tc;
		}
};

template <class T> class CFlySafeGuard
{
		T& m_counter;
	public:
		CFlySafeGuard(T& p_counter) : m_counter(p_counter)
		{
			++m_counter;
		}
		~CFlySafeGuard()
		{
			--m_counter;
		}
};

#endif /* W_H_ */
