/*
 * Copyright (C) 2011-2021 FlylinkDC++ Team http://flylinkdc.com
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

#if !defined(_SETTINGS_AUTO_UPDATE_H_)
#define _SETTINGS_AUTO_UPDATE_H_

#pragma once

#ifdef FLYLINKDC_USE_SETTINGS_AUTO_UPDATE

#include "../client/Singleton.h"
#include "../client/SettingsManager.h"

class SettingsAutoUpdateListener
{
	public:
		virtual ~SettingsAutoUpdateListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> UpdateStarted;
		typedef X<1> UpdateFinished;
		typedef X<2> Failed;
		
		virtual void on(UpdateStarted, const string&) noexcept { }
		virtual void on(UpdateFinished, const string&) noexcept { }
		virtual void on(Failed, const string&) noexcept { }
};

enum SettingsAutoUpdateTasks
{
	START_UPDATE_FILE,
};
struct SettingsAutoUpdateTaskData
{
	virtual ~SettingsAutoUpdateTaskData() { }
	
	SettingsAutoUpdateTaskData(const string& localPath, const string& urlPath) : _localPath(localPath), _urlPath(urlPath) {}
	string _localPath;
	string _urlPath;
};

class SettingsAutoUpdate :
	public Singleton<SettingsAutoUpdate>,
	public Speaker<SettingsAutoUpdateListener>,
	public BackgroundTaskExecuter<pair<SettingsAutoUpdateTasks, SettingsAutoUpdateTaskData*>>
{
	public:
		void initialize();
		
		void UpdateFileFromURL(string& file, string& url)
		{
			addTask(make_pair(START_UPDATE_FILE, new SettingsAutoUpdateTaskData(file, url)));
		}
		
	private:
		friend class Singleton<SettingsAutoUpdate>;
		SettingsAutoUpdate() { }
		
		virtual ~SettingsAutoUpdate();
		
		void fail(const string& aError);
		//void execute(const pair<SettingsAutoUpdateTasks, SettingsAutoUpdateTaskData*>& p_task);
		
		void _StartFileUpdateThisThread(const string& localPath, const string& urlPath);
};

#endif // FLYLINKDC_USE_SETTINGS_AUTO_UPDATE

#endif // _SETTINGS_AUTO_UPDATE_H_