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
	: virtual NonDerivable<HIconWrapper> , boost::noncopyable // [+] IRainman fix.
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
		
		~HIconWrapper()
		{
			if (m_icon && (m_fuload & LR_SHARED) == 0) //
				// "It is only necessary to call DestroyIcon for icons and cursors created with the following functions:
				// CreateIconFromResourceEx (if called without the LR_SHARED flag), CreateIconIndirect, and CopyIcon.
				// Do not use this function to destroy a shared icon.
				//A shared icon is valid as long as the module from which it was loaded remains in memory.
				// The following functions obtain a shared icon.
			{
				dcdrun(const BOOL l_res =) DestroyIcon(m_icon);
				dcassert(l_res);
			}
		}
		
		operator HICON() const
		{
			return m_icon;
		}
		
	private:
		HICON load(WORD id, int cx, int cy, UINT p_fuLoad)
		{
			// [!] IRainman opt.
			dcassert(id);
			m_fuload |= LR_SHARED;
			HICON icon = nullptr;
			const auto l_ThemeHandle = ThemeManager::getResourceLibInstance();
			if (l_ThemeHandle)
			{
				icon = (HICON)::LoadImage(ThemeManager::getResourceLibInstance(), MAKEINTRESOURCE(id), IMAGE_ICON, cx, cy, m_fuload); // [!] IRainman fix done: Crash wine.
				if (!icon)
				{
					dcdebug("!!!!!!!![Error - 1] (HICON)::LoadImage: ID = %d icon = %p this = %p fuLoad = %x\n", id, icon, this, m_fuload);
				}
			}
			if (!icon)
			{
				static const HMODULE g_current = GetModuleHandle(nullptr);
				if (l_ThemeHandle != g_current)
				{
					icon = (HICON)::LoadImage(g_current, MAKEINTRESOURCE(id), IMAGE_ICON, cx, cy, m_fuload);
					dcdebug("!!!!!!!![step - 2] (HICON)::LoadImage: ID = %d icon = %p this = %p fuLoad = %x\n", id, icon, this, m_fuload);
				}
			}
			dcassert(icon);
			return icon;
		}
		
	private:
		HICON m_icon;
		UINT m_fuload;
};

#endif //_H_ICON_WRAPPER_H_