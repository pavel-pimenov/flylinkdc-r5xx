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

#if !defined(_INET_DOWNLOAD_REPORTER_)
#define _INET_DOWNLOAD_REPORTER_

#pragma once

#include <queue>
#include "../client/Singleton.h"
#include "../client/Semaphore.h"

#define WM_IDR_TOTALRESULT_WAIT WM_USER+0x15
#define WM_IDR_RESULT_RECEIVED WM_USER+0x16
#define WM_IDR_CURRENT_STAGE WM_USER+0x17

class InetDownloadReporter : public IDateReceiveReporter, public Singleton<InetDownloadReporter>
{
		typedef std::deque<HWND> HWNDReceivers;
		typedef HWNDReceivers::const_iterator HWNDReceiversIter;
		
	public:
		void SetCurrentHWND(HWND windowReceiver);
		void RemoveCurrentHWND(HWND windowReceiver);
		
		bool ReportResultWait(DWORD totalDataWait);
		bool ReportResultReceive(DWORD currentDataReceive);
		bool SetCurrentStage(std::string& value);
		
		const std::string& GetCurrentStage()
		{
			return _currentStage;
		}
		
	private:
	
		InetDownloadReporter()
		{
			// [-] _windows.clear(); [-] IRainman fix.
		}
		
		friend class Singleton<InetDownloadReporter>;
		HWNDReceivers _windows;
		std::string _currentStage;
};

#endif //_INET_DOWNLOAD_REPORTER_

