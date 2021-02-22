/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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


#ifndef DCPLUSPLUS_DCPP_EXCEPTION_H
#define DCPLUSPLUS_DCPP_EXCEPTION_H

#include <string>
#include <exception>

#include "debug.h"

using std::string;

class Exception : public std::exception
{
	public:
		Exception() { }
		virtual ~Exception() noexcept
		{
		}
		explicit Exception(const string& aError);
		const char* what() const
		{
			return m_error.c_str();
		}
		const string& getError() const
		{
			return m_error;
		}
	protected:
		const string m_error;
	private:
		Exception& operator = (const Exception& Source);
};

#ifdef _DEBUG

#define STANDARD_EXCEPTION(name) class name : public Exception { \
		public:\
			explicit name(const string& aError) : Exception(#name ": " + aError) { } \
	}

#define STANDARD_EXCEPTION_ADD_INFO(name) class name : public Exception { \
		public:\
			name(const string& aError, const string& aInfo) : Exception(#name ": " + aError + "\r\n [" + aInfo + "]" ) { } \
	}

#else // _DEBUG

#define STANDARD_EXCEPTION(name) class name : public Exception { \
		public:\
			explicit name(const string& aError) : Exception(aError) { } \
	}

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
