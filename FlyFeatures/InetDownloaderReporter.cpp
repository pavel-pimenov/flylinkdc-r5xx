/*
 * Copyright (C) 2011-2012 FlylinkDC++ Team http://flylinkdc.com/
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
#include "InetDownloaderReporter.h"


void InetDownloadReporter::SetCurrentHWND(HWND windowReceiver)
{
	if (::IsWindow(windowReceiver))
		_windows.push_front(windowReceiver);
}
void InetDownloadReporter::RemoveCurrentHWND(HWND windowReceiver)
{

	HWNDReceiversIter i = _windows.begin();
	while (i != _windows.end())
		(*i == windowReceiver) ? i = _windows.erase(i) : ++i;
		
}

bool InetDownloadReporter::ReportResultWait(DWORD totalDataWait)
{
	if (_windows.empty())
		return true;
	HWND currentWindow = _windows.at(0);
	if (::IsWindow(currentWindow))
	{
		::PostMessage(currentWindow, WM_IDR_TOTALRESULT_WAIT, (WPARAM)totalDataWait, NULL);
	}
	
	return true;
}
bool InetDownloadReporter::ReportResultReceive(DWORD currentDataReceive)
{
	if (_windows.empty())
		return true;
	HWND currentWindow = _windows.at(0);
	if (::IsWindow(currentWindow))
	{
		::PostMessage(currentWindow, WM_IDR_RESULT_RECEIVED, (WPARAM)currentDataReceive, NULL);
	}
	return true;
}
bool InetDownloadReporter::SetCurrentStage(std::string& value)
{
	if (_windows.empty())
		return true;
		
	_currentStage = value;
	HWND currentWindow = _windows.at(0);
	if (::IsWindow(currentWindow))
	{
		::PostMessage(currentWindow, WM_IDR_CURRENT_STAGE, NULL, NULL);
	}
	
	return true;
}
