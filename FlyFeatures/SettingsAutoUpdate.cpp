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

#ifdef FLYLINKDC_USE_SETTINGS_AUTO_UPDATE
#include "SettingsAutoUpdate.h"
#include "../client/ResourceManager.h"
#include "../client/File.h"

SettingsAutoUpdate::~SettingsAutoUpdate()
{
	SettingsManager::getInstance()->removeListener(this);
	waitShutdown();
}

void
SettingsAutoUpdate::initialize()
{
	SettingsManager::getInstance()->addListener(this);
}

void SettingsAutoUpdate::execute(const pair<SettingsAutoUpdateTasks, SettingsAutoUpdateTaskData*>& p_task)
{
	switch (p_task.first)
	{
		case START_UPDATE_FILE:
			_StartFileUpdateThisThread(p_task.second->_localPath, p_task.second->_urlPath);
		break;
	}
			
	delete p_task.second;
}

void SettingsAutoUpdate::fail(const string& p_error)
{
	dcdebug("SettingsAutoUpdate: New command when already failed: %s\n", p_error.c_str());
	//LogManager::getInstance()->message("SettingsAutoUpdate: " + p_error);
	fire(SettingsAutoUpdateListener::Failed(), p_error);
}

void SettingsAutoUpdate::_StartFileUpdateThisThread(const string& localPath, const string& urlPath)
{
	fire(SettingsAutoUpdateListener::UpdateStarted(), localPath);
	string tempPath = Util::getTempPath();
	AppendPathSeparator(tempPath);
	tempPath += Util::getFileName(localPath);
	try
	{
		File temp(tempPath, File::WRITE, File::CREATE | File::TRUNCATE);
		if (Util::getDataFromInet(urlPath, temp))
		{
			if (temp.isOpen())
				temp.close();
			try
			{
				File::renameFile(tempPath, localPath);
				fire(SettingsAutoUpdateListener::UpdateFinished(), localPath);
			}
			catch (FileException& ex)
			{
				fail(ex.getError());
			}
		}
		if (File::isExist(tempPath))
			File::deleteFile(tempPath);
	}
	catch (const Exception& e)
	{
		fail(e.getError());
	}	
}

#endif // FLYLINKDC_USE_SETTINGS_AUTO_UPDATE