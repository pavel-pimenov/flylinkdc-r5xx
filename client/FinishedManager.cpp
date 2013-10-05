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

#include "stdinc.h"
#include "FinishedManager.h"
#include "FinishedManagerListener.h"
#include "Download.h"
#include "Upload.h"
#include "QueueManager.h"
#include "UploadManager.h"
#include "LogManager.h"
#include "CFlylinkDBManager.h"

FinishedManager::FinishedManager()
#ifndef IRAINMAN_USE_SEPARATE_CS_IN_FINISHED_MANAGER
	: csU(csD)
#endif
{
	QueueManager::getInstance()->addListener(this);
	UploadManager::getInstance()->addListener(this);
}

FinishedManager::~FinishedManager()
{
	QueueManager::getInstance()->removeListener(this);
	UploadManager::getInstance()->removeListener(this);
	{
		FastUniqueLock l(csD); // TODO: needs?
		for_each(downloads.begin(), downloads.end(), DeleteFunction());
	}
	{
		FastUniqueLock l(csU); // TODO: needs?
		for_each(uploads.begin(), uploads.end(), DeleteFunction());
	}
}

void FinishedManager::remove(FinishedItemPtr item, bool upload /* = false */)
{
	auto remove = [item](FinishedItemList & listptr, FastSharedCriticalSection & cs) -> void
	{
		FastUniqueLock l(cs);
		const FinishedItemList::iterator it = find(listptr.begin(), listptr.end(), item);
		
		if (it != listptr.end())
			listptr.erase(it);
			
		delete item; // [+] IRainman fix memory leak.
	};
	
	if (upload)
	{
		remove(uploads, csU);
	}
	else
	{
		remove(downloads, csD);
	}
}

void FinishedManager::removeAll(bool upload /* = false */)
{
	auto removeAll = [](FinishedItemList & listptr, FastSharedCriticalSection & cs) -> void
	{
		FastUniqueLock l(cs);
		for_each(listptr.begin(), listptr.end(), DeleteFunction());
		listptr.clear();
	};
	
	if (upload)
	{
		removeAll(uploads, csU);
	}
	else
	{
		removeAll(downloads, csD);
	}
}

void FinishedManager::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string&, const Download* d) noexcept
{
	const bool isFile = !qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP);
	
	if (isFile)
	{
		PLAY_SOUND(SOUND_FINISHFILE);
	}
	
	if (isFile || (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST) && BOOLSETTING(LOG_FILELIST_TRANSFERS)))
	{
	
		const FinishedItemPtr item = new FinishedItem(qi->getTarget(), d->getHintedUser(), qi->getSize(), d->getRunningAverage(), GET_TIME(), qi->getTTH().toBase32(), d->getUser()->getIP());
		{
			FastUniqueLock l(csD);
			// [+] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=601
			while (!downloads.empty() && downloads.size() > static_cast<size_t>(SETTING(MAX_FINISHED_DOWNLOADS)))
			{
				/* TODO remove from the frame, please see FinishedFrameBase::onSpeaker
				FinishedItemPtr l_orig = *(downloads.cbegin());
				FinishedItemPtr l_copy = new FinishedItem(*l_orig);
				fire(FinishedManagerListener::RemovedDl(), l_copy);
				*/
				delete *downloads.cbegin();
				downloads.erase(downloads.cbegin());
			}
			// [~] IRainman
			downloads.push_back(item);
		}
		
		fire(FinishedManagerListener::AddedDl(), item);
		
		const size_t BUF_SIZE = STRING(FINISHED_DOWNLOAD).size() + FULL_MAX_PATH + 128;
		std::unique_ptr<char[]> buf(new char[BUF_SIZE]);
		snprintf(&buf[0], BUF_SIZE, CSTRING(FINISHED_DOWNLOAD), Util::getFileName(qi->getTarget()).c_str(),
		         Util::toString(ClientManager::getInstance()->getNicks(d->getUser()->getCID(), Util::emptyString)).c_str());
		LogManager::getInstance()->message(&buf[0]);
	}
}

void FinishedManager::on(UploadManagerListener::Complete, const Upload* u) noexcept
{
	if (u->getType() == Transfer::TYPE_FILE || (u->getType() == Transfer::TYPE_FULL_LIST && BOOLSETTING(LOG_FILELIST_TRANSFERS)))
	{
		PLAY_SOUND(SOUND_UPLOADFILE);
		
		const FinishedItemPtr item = new FinishedItem(u->getPath(), u->getHintedUser(), u->getFileSize(), u->getRunningAverage(), GET_TIME(), Util::emptyString, u->getUser()->getIP());
		{
			FastUniqueLock l(csU);
			// [+] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=601
			while (!uploads.empty() && uploads.size() > static_cast<size_t>(SETTING(MAX_FINISHED_UPLOADS)))
			{
				/* TODO remove from the frame, please see FinishedFrameBase::onSpeaker
				FinishedItemPtr l_orig = *(uploads.cbegin());
				FinishedItemPtr l_copy = new FinishedItem(*l_orig);
				fire(FinishedManagerListener::RemovedUl(), l_copy);
				*/
				delete *uploads.cbegin();
				uploads.erase(uploads.cbegin());
			}
			// [~] IRainman
			uploads.push_back(item);
		}
		
		fire(FinishedManagerListener::AddedUl(), item);
		
		const size_t BUF_SIZE = STRING(FINISHED_UPLOAD).size() + FULL_MAX_PATH + 128;
		{
			std::unique_ptr<char[]> buf(new char[BUF_SIZE]);
			snprintf(&buf[0], BUF_SIZE, CSTRING(FINISHED_UPLOAD), (Util::getFileName(u->getPath())).c_str(),
			         Util::toString(ClientManager::getInstance()->getNicks(u->getUser()->getCID(), Util::emptyString)).c_str());
			         
			LogManager::getInstance()->message(&buf[0]);
		}
		const string l_name = Text::toLower(Util::getFileName(u->getPath()));
		const string l_path = Text::toLower(Util::getFilePath(u->getPath()));
		CFlylinkDBManager::getInstance()->Hit(l_path, l_name);
	}
}

/**
 * @file
 * $Id: FinishedManager.cpp 571 2011-07-27 19:18:04Z bigmuscle $
 */
