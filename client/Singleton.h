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

#ifndef DCPLUSPLUS_DCPP_SINGLETON_H
#define DCPLUSPLUS_DCPP_SINGLETON_H

#include "typedefs.h"

template<typename T>
class Singleton
#ifdef _DEBUG
	: boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		explicit Singleton() { }
		virtual ~Singleton() { }
		
		static bool isValidInstance() //[+]PPA
		{
			return instance != nullptr;
		}
		static T* getInstance()
		{
#ifdef _DEBUG
			if (!isValidInstance())
			{
				::MessageBoxA(nullptr, typeid(T).name(), "getInstance is null", MB_OK);
				dcassert(isValidInstance());
			}
#endif
			return instance;
		}
		
		static void newInstance()
		{
			// [!] PVS V809 Verifying that a pointer value is not NULL is not required. The 'if (instance)' check can be removed. singleton.h 50
#ifdef _DEBUG
			if (isValidInstance())
			{
				::MessageBoxA(nullptr, typeid(T).name(), "recreate instance!", MB_OK);
				dcassert(!isValidInstance());
			}
#endif
			delete instance;
			instance = new T();
		}
		
		static void deleteInstance()
		{
#ifdef _DEBUG
			if (!isValidInstance())
			{
				::MessageBoxA(nullptr, typeid(T).name(), "attempt to delete the deleted object!", MB_OK);
				dcassert(isValidInstance());
			}
#endif
			delete instance;
			instance = nullptr;
		}
	protected:
		static T* instance;
};

template<class T> T* Singleton<T>::instance = nullptr;

#endif // DCPLUSPLUS_DCPP_SINGLETON_H

/**
 * @file
 * $Id: Singleton.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
