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

#ifndef _THEME_MANAGER_H_
#define _THEME_MANAGER_H_

#pragma once

#include "../client/Singleton.h"
#include "../client/SettingsManager.h"

#ifdef IRAINMAN_THEME_MANAGER_LISTENER_ENABLE
class ThemeManagerListener
{
	public:
		virtual ~ThemeManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> ResourceLoaded;
		
		virtual void on(ResourceLoaded, const string&) { }
};
#endif // IRAINMAN_THEME_MANAGER_LISTENER_ENABLE

class ThemeManager :
#ifdef IRAINMAN_THEME_MANAGER_LISTENER_ENABLE
	public Speaker<ThemeManagerListener>,
#endif
	public Singleton<ThemeManager>
{
	public:
	
		static bool isResourceLibLoaded()
		{
			return g_resourceLibInstance != nullptr;
		}
		static HMODULE getResourceLibInstance()
		{
			return g_resourceLibInstance;
		}
		void load()
		{
			loadResourceLib();
		}
	private:
		void loadResourceLib();
	
		static void unloadResourceLib();
		
		static void setResourceLibInstance(HMODULE p_instance)
		{
			g_resourceLibInstance = p_instance;
		}
		
		friend class Singleton<ThemeManager>;
		
		~ThemeManager()
		{
			unloadResourceLib();
		}
		
		static HMODULE g_resourceLibInstance;
};

#endif // _THEME_MANAGER_H_

