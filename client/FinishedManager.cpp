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
		for_each(m_downloads.begin(), m_downloads.end(), DeleteFunction());
	}
	{
		FastUniqueLock l(csU); // TODO: needs?
		for_each(m_uploads.begin(), m_uploads.end(), DeleteFunction());
	}
}

void FinishedManager::remove(FinishedItemPtr item, bool upload /* = false */)
{
	auto remove = [item](FinishedItemList & listptr, FastSharedCriticalSection & cs) -> void
	{
		FastUniqueLock l(cs);
		const FinishedItemList::const_iterator it = find(listptr.begin(), listptr.end(), item);
		
		if (it != listptr.end())
			listptr.erase(it);
			
		delete item; // [+] IRainman fix memory leak.
	};
	
	if (upload)
	{
		remove(m_uploads, csU);
	}
	else
	{
		remove(m_downloads, csD);
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
		removeAll(m_uploads, csU);
	}
	else
	{
		removeAll(m_downloads, csD);
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
		CFlylinkDBManager::getInstance()->clear_tiger_tree_cache(qi->getTTH());
		const FinishedItemPtr item = new FinishedItem(qi->getTarget(), d->getHintedUser(), qi->getSize(), d->getRunningAverage(), GET_TIME(), qi->getTTH().toBase32(), d->getUser()->getIPAsString());
		{
			FastUniqueLock l(csD);
			// TODO - fix copy-paste
			// [+] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=601
			while (!m_downloads.empty() && m_downloads.size() > static_cast<size_t>(SETTING(MAX_FINISHED_DOWNLOADS)))
			{
				delete *m_downloads.cbegin();
				m_downloads.pop_front();
			}
			// [~] IRainman
			m_downloads.push_back(item);
		}
		
		fire(FinishedManagerListener::AddedDl(), item);
		log(d->getUser()->getCID(), qi->getTarget(), STRING(FINISHED_DOWNLOAD));
	}
}

void FinishedManager::on(UploadManagerListener::Complete, const Upload* u) noexcept
{
	if (u->getType() == Transfer::TYPE_FILE || (u->getType() == Transfer::TYPE_FULL_LIST && BOOLSETTING(LOG_FILELIST_TRANSFERS)))
	{
		PLAY_SOUND(SOUND_UPLOADFILE);
		
		const FinishedItemPtr item = new FinishedItem(u->getPath(), u->getHintedUser(), u->getFileSize(), u->getRunningAverage(), GET_TIME(), Util::emptyString, u->getUser()->getIPAsString());
		{
			FastUniqueLock l(csU);
			// TODO - fix copy-paste
			// [+] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=601
			while (!m_uploads.empty() && m_uploads.size() > static_cast<size_t>(SETTING(MAX_FINISHED_UPLOADS)))
			{
				delete *m_uploads.cbegin();
				m_uploads.pop_front();
			}
			// [~] IRainman
			m_uploads.push_back(item);
		}
		fire(FinishedManagerListener::AddedUl(), item);
		const auto l_file_name = log(u->getUser()->getCID(), u->getPath(), STRING(FINISHED_UPLOAD));
		const string l_name = Text::toLower(l_file_name);
		const string l_path = Text::toLower(Util::getFilePath(u->getPath()));
		CFlylinkDBManager::getInstance()->Hit(l_path, l_name);
	}
}

string FinishedManager::log(const CID& p_CID, const string& p_path, const string& p_message)
{
	const auto l_file_name = Util::getFileName(p_path);
	const size_t BUF_SIZE = p_message.size() + FULL_MAX_PATH + 128;
	{
		std::unique_ptr<char[]> buf(new char[BUF_SIZE]);
		snprintf(&buf[0], BUF_SIZE, p_message.c_str(), l_file_name.c_str(),
		         Util::toString(ClientManager::getNicks(p_CID, Util::emptyString)).c_str());
		         
		LogManager::getInstance()->message(&buf[0]);
	}
	return l_file_name;
}
/**
 * @file
 * $Id: FinishedManager.cpp 571 2011-07-27 19:18:04Z bigmuscle $
 */
