/*
 * Copyright (C) 2011-2015 FlylinkDC++ Team http://flylinkdc.com/
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
#include "ThemeManager.h"
#include "../client/Util.h"

HMODULE ThemeManager::g_resourceLibInstance = nullptr;
#ifdef _DEBUG
// [+] IRainman fix: not reload lib in runtime, prevent potential crash.
bool g_debugResourceLibIsLoaded = false;
bool g_debugResourceLibIsUnloaded = false;
#endif // _DEBUG


void ThemeManager::unloadResourceLib()
{
#ifdef _DEBUG
	dcassert(!g_debugResourceLibIsUnloaded);
	g_debugResourceLibIsUnloaded = true;
#endif // _DEBUG
	if (isResourceLibLoaded())
	{
		::FreeLibrary(getResourceLibInstance());
		setResourceLibInstance(nullptr);
	}
}

void ThemeManager::loadResourceLib()
{
#ifdef _DEBUG
	dcassert(!g_debugResourceLibIsLoaded);
	g_debugResourceLibIsLoaded = true;
	dcassert(!isResourceLibLoaded());
#endif // _DEBUG
	const auto& themeDllName = SETTING(THEME_MANAGER_THEME_DLL_NAME);
	if (!themeDllName.empty())
	{
		const auto themeFullPath = Util::getThemesPath() + themeDllName;

		setResourceLibInstance(::LoadLibrary(Text::toT(themeFullPath).c_str()));
#ifdef IRAINMAN_THEME_MANAGER_LISTENER_ENABLE
		if (isResourceLibLoaded())
		{
			fire(ThemeManagerListener::ResourceLoaded(), themeDllName);
		}
#endif
	}
}
