/*
 * Copyright (C) 2001-2013 Jacek Sieka, j_s@telia.com
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

#ifdef FLYLINKDC_TRACE_ENABLE

#include "TraceManager.h"


TraceManager* Singleton<TraceManager>::instance = nullptr;


void  TraceManager::print(const string& msg)
{
	DWORD tid;
	char buf[21];
	
	time_t now = time(NULL);
//+BugMaster: fix
	if (!strftime(buf, 21, "%Y-%m-%d %H:%M:%S ", localtime(&now)))
	{
		strcpy(buf, "xxxx-xx-xx xx:xx:xx ");
	}
//-BugMaster: fix

	tid = GetCurrentThreadId();
	
	CFlyLock(cs);
	
	try
	{
		f->write(buf + string(indents[tid], ' ') + msg + "\r\n");
	}
	catch (const FileException&)
	{
		// ...
	}
	
}

void CDECL TraceManager::trace_print(const char* format, ...) noexcept
{
	va_list args;
	va_start(args, format);
	
	char buf[512];
	buf[0] = 0;
	
	_vsnprintf(buf, _countof(buf), format, args);
	
	print(string(buf));
	va_end(args);
	
}

void CDECL TraceManager::trace_start(const char* format, ...) noexcept
{
	va_list args;
	va_start(args, format);
	
	char buf[512];
	
	_vsnprintf(buf, _countof(buf), format, args);
	
	print((string)"START " + buf);
	
	indents[GetCurrentThreadId()] += 4;
	va_end(args);
}

void CDECL TraceManager::trace_end(const char* format, ...) noexcept
{
	va_list args;
	va_start(args, format);
	
	char buf[512];
	
	_vsnprintf(buf, _countof(buf), format, args);
	
	indents[GetCurrentThreadId()] -= 4;
	
	print((string)"END " + buf);
	
	va_end(args);
}

#endif // FLYLINKDC_TRACE_ENABLE

/**
 * @file
 * $Id: TraceManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
