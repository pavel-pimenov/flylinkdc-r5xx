/*
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

#ifndef _H_ICON_WRAPPER_H_
#define _H_ICON_WRAPPER_H_

#pragma once

#include "../FlyFeatures/ThemeManager.h"

class HIconWrapper
#ifdef _DEBUG
	: boost::noncopyable
#endif
{
	public:
		explicit HIconWrapper(WORD id, int cx = 16, int cy = 16, UINT p_fuLoad = LR_DEFAULTCOLOR) :
			m_fuload(p_fuLoad)
		{
			m_icon = load(id, cx, cy, p_fuLoad);
			dcassert(m_icon);
		}
		explicit HIconWrapper(HICON p_icon) : m_icon(p_icon), m_fuload(LR_DEFAULTCOLOR)
		{
			dcassert(m_icon);
		}
		
		~HIconWrapper();
		operator HICON() const
		{
			return m_icon;
		}
		
	private:
		HICON load(WORD id, int cx, int cy, UINT p_fuLoad);
		//HICON load_vip();
		
	private:
		HICON m_icon;
		UINT m_fuload;
};

#endif //_H_ICON_WRAPPER_H_