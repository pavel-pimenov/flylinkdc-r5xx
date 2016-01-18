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

#ifndef DCPLUSPLUS_DCPP_EXCEPTION_H
#define DCPLUSPLUS_DCPP_EXCEPTION_H

#include <string>
#include <exception>

#include "debug.h"
#include "noexcept.h"

using std::string;

class Exception : public std::exception
#ifdef _DEBUG
	//, boost::noncopyable 
#endif
{
	public:
		Exception() { }
		virtual ~Exception() noexcept
		{
		}
		explicit Exception(const string& aError) : error(aError)
		{
			dcdrun(if (!error.empty())) dcdebug("Thrown: %s\n", error.c_str()); //-V111
		}
		const char* what() const
		{
			return getError().c_str();
		}
		const string& getError() const
		{
			return error;
		}
	protected:
		const string error; // [!] IRainman: this is const string.
	private:
		Exception& operator = (const Exception& Source);
};

#ifdef _DEBUG

#define STANDARD_EXCEPTION(name) class name : public Exception { \
		public:\
			explicit name(const string& aError) : Exception(#name ": " + aError) { } \
	}

// [!] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1318
#define STANDARD_EXCEPTION_ADD_INFO(name) class name : public Exception { \
		public:\
			name(const string& aError, const string& aInfo) : Exception(#name ": " + aError + "\r\n [" + aInfo + "]" ) { } \
	}

#else // _DEBUG

#define STANDARD_EXCEPTION(name) class name : public Exception { \
		public:\
			explicit name(const string& aError) : Exception(aError) { } \
	}

// [!] IRainman fix https://code.google.com/p/flylinkdc/issues/detail?id=1318
#define STANDARD_EXCEPTION_ADD_INFO(name) class name : public Exception { \
		public:\
			name(const string& aError, const string& aInfo) : Exception(aError + "\r\n [" + aInfo + "]" ) { } \
	}
#endif

#endif // !defined(EXCEPTION_H)

/**
 * @file
 * $Id: Exception.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
