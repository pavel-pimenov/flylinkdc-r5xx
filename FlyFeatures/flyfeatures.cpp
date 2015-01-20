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

#include "stdinc.h"
#include "ResourceManager.h"
#include "CustomMenuManager.h"
#include "RSSManager.h"
#include "ThemeManager.h"
#include "AutoUpdate.h"
#include "DCPlusPlus.h"
#include "InetDownloaderReporter.h"
#include "VideoPreview.h"

void createFlyFeatures()
{
	ThemeManager::newInstance();      // [+] SSA
	InetDownloadReporter::newInstance(); // [+] SSA
	AutoUpdate::newInstance();        // [+] SSA
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
	CustomMenuManager::newInstance(); // [+] SSA
#endif
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	VideoPreview::newInstance(); // [+] SSA
#endif
#ifdef IRAINMAN_INCLUDE_RSS
	RSSManager::newInstance();        // [+] SSA
#endif
}

void startupFlyFeatures(PROGRESSCALLBACKPROC pProgressCallbackProc, void* pProgressParam)
{
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
	if (pProgressCallbackProc != NULL)
		pProgressCallbackProc(pProgressParam, TSTRING(CUSTOM_MENU));
		
	CustomMenuManager::getInstance()->load();
#endif
	ThemeManager::getInstance()->load(); //[+] SSA
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	VideoPreview::getInstance()->initialize();
#endif
}

void loadingAfterGuiFlyFeatures(HWND p_mainFrameHWND, AutoUpdateGUIMethod* p_guiDelegate)
{
	AutoUpdate::getInstance()->initialize(p_mainFrameHWND, p_guiDelegate); // [+] SSA
}

void shutdownFlyFeatures()
{
	AutoUpdate::getInstance()->shutdownAndUpdate();
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	VideoPreview::getInstance()->shutdown();
#endif
}

void deleteFlyFeatures()
{
#ifdef IRAINMAN_INCLUDE_RSS
	RSSManager::deleteInstance();        //[+] SSA
#endif
#ifdef SSA_VIDEO_PREVIEW_FEATURE
	VideoPreview::deleteInstance();
#endif
#ifdef IRAINMAN_INCLUDE_PROVIDER_RESOURCES_AND_CUSTOM_MENU
	CustomMenuManager::deleteInstance(); //[+] SSA
#endif
	AutoUpdate::deleteInstance();
	InetDownloadReporter::deleteInstance(); // [+] SSA
	ThemeManager::deleteInstance();   //[+] SSA
}
