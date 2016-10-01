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

#pragma once


#ifndef DCPLUSPLUS_CLIENT_RESOURCE_MANAGER_H
#define DCPLUSPLUS_CLIENT_RESOURCE_MANAGER_H

#include "debug.h"
#include "dcformat.h"

#define STRING(x) ResourceManager::getString(ResourceManager::x) //-V:STRING:807 
#define CSTRING(x) ResourceManager::getString(ResourceManager::x).c_str() //-V:CSTRING:807 
#define WSTRING(x) ResourceManager::getStringW(ResourceManager::x) //-V:WSTRING:807 
#define CWSTRING(x) ResourceManager::getStringW(ResourceManager::x).c_str() //-V:CWSTRING:807 

#define STRING_I(x) ResourceManager::getString(x) //-V:STRING_I:807 
#define CSTRING_I(x) ResourceManager::getString(x).c_str() //-V:CSTRING_I:807 
#define WSTRING_I(x) ResourceManager::getStringW(x) //-V:WSTRING_I:807 
#define CWSTRING_I(x) ResourceManager::getStringW(x).c_str() //-V:CWSTRING_I:807 

#define STRING_F(x, args) (dcpp_fmt(ResourceManager::getString(ResourceManager::x)) % args).str()
#define CSTRING_F(x, args) (dcpp_fmt(ResourceManager::getString(ResourceManager::x)) % args).str().c_str()
#define WSTRING_F(x, args) (dcpp_fmt(ResourceManager::getStringW(ResourceManager::x)) % args).str()
#define CWSTRING_F(x, args) (dcpp_fmt(ResourceManager::getStringW(ResourceManager::x)) % args).str().c_str()

#ifdef UNICODE
#define TSTRING WSTRING
#define CTSTRING CWSTRING
#define TSTRING_I WSTRING_I
#define CTSTRING_I CWSTRING_I
#else
#define TSTRING STRING
#define CTSTRING CSTRING
#define TSTRING_I STRING_I
#define CTSTRING_I CSTRING_I
#endif

class ResourceManager
{
	public:
	
#include "StringDefs.h"
	
		static void startup(bool p_is_create_wide) // [+] IRainman fix.
		{
			if (p_is_create_wide)
			{
				createWide();
			}
			dcdrun(g_debugStarted = true;) // [+] IRainman fix.
		}
		static bool loadLanguage(const string& aFile); // [!] IRainman fix: is its static function.
		static const string& getString(Strings x) // [!] IRainman fix: is its static function.
		{
			return g_strings[x];
		}
		static const wstring& getStringW(Strings x) // [!] IRainman fix: is its static function.
		{
			dcassert(g_debugStarted); // [+] IRainman fix.
			return g_wstrings[x];
		}
	private:
	
		ResourceManager()
		{
		}
		
		~ResourceManager() { }
		
		static string g_strings[LAST];
		static wstring g_wstrings[LAST];
		static string g_names[LAST];
		static void createWide();
		dcdrun(static bool g_debugStarted;) // [+] IRainman fix.
};

#endif // !defined(RESOURCE_MANAGER_H)

/**
 * @file
 * $Id: ResourceManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
