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

std::unique_ptr<webrtc::RWLockWrapper> FinishedManager::g_cs[2];

FinishedManager::FinishedManager()
{
	g_cs[e_Download] = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
	g_cs[e_Upload] = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
	
	QueueManager::getInstance()->addListener(this);
	UploadManager::getInstance()->addListener(this);
}

FinishedManager::~FinishedManager()
{
	QueueManager::getInstance()->removeListener(this);
	UploadManager::getInstance()->removeListener(this);
	{
		webrtc::WriteLockScoped l(*g_cs[e_Upload]);
		for_each(m_finished[e_Upload].begin(), m_finished[e_Upload].end(), DeleteFunction());
	}
	{
		webrtc::WriteLockScoped l(*g_cs[e_Download]);
		for_each(m_finished[e_Download].begin(), m_finished[e_Download].end(), DeleteFunction());
	}
}

void FinishedManager::removeItem(FinishedItem* p_item, eType p_type)
{
	webrtc::WriteLockScoped l(*g_cs[p_type]);
	const auto it = find(m_finished[p_type].begin(), m_finished[p_type].end(), p_item);
	
	if (it != m_finished[p_type].end())
	{
		m_finished[p_type].erase(it);
	}
	else
	{
		dcassert(0);
	}
}

void FinishedManager::removeAll(eType p_type)
{
	webrtc::WriteLockScoped l(*g_cs[p_type]);
	for_each(m_finished[p_type].begin(), m_finished[p_type].end(), DeleteFunction());
	m_finished[p_type].clear();
}

void FinishedManager::rotation_items(FinishedItem* p_item, eType p_type)
{
	webrtc::WriteLockScoped l(*g_cs[p_type]);
	// [+] IRainman http://code.google.com/p/flylinkdc/issues/detail?id=601
	auto& l_item_array = m_finished[p_type];
	while (!l_item_array.empty() &&
	        l_item_array.size() > static_cast<size_t>(p_type == e_Download ? SETTING(MAX_FINISHED_DOWNLOADS) : SETTING(MAX_FINISHED_UPLOADS)))
	{
		if (p_type == e_Download)
			fire(FinishedManagerListener::RemovedDl(), *l_item_array.cbegin());
		else
			fire(FinishedManagerListener::RemovedUl(), *l_item_array.cbegin());
		delete *l_item_array.cbegin();
		l_item_array.pop_front();
	}
	// [~] IRainman
	l_item_array.push_back(p_item);
}

void FinishedManager::on(QueueManagerListener::Finished, const QueueItemPtr& qi, const string&, const Download* p_download) noexcept
{
	const bool isFile = !qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST | QueueItem::FLAG_USER_GET_IP);
	
	if (isFile)
	{
		PLAY_SOUND(SOUND_FINISHFILE);
	}
	
	if (isFile || (qi->isAnySet(QueueItem::FLAG_USER_LIST | QueueItem::FLAG_DCLST_LIST) && BOOLSETTING(LOG_FILELIST_TRANSFERS)))
	{
		FinishedItem* item = new FinishedItem(qi->getTarget(), p_download->getHintedUser(), qi->getSize(), p_download->getRunningAverage(), GET_TIME(), qi->getTTH(), p_download->getUser()->getIPAsString());
		if (SETTING(DB_LOG_FINISHED_DOWNLOADS))
		{
			CFlylinkDBManager::getInstance()->save_transfer_history(e_TransferDownload, item);
		}
		rotation_items(item, e_Download);
		fire(FinishedManagerListener::AddedDl(), item, false);
		log(p_download->getUser()->getCID(), qi->getTarget(), STRING(FINISHED_DOWNLOAD));
	}
}
void FinishedManager::pushHistoryFinishedItem(FinishedItem* p_item, int p_type)
{
	if (p_type)
		fire(FinishedManagerListener::AddedUl(), p_item, true);
	else
		fire(FinishedManagerListener::AddedDl(), p_item, true);
}
void FinishedManager::on(UploadManagerListener::Complete, const Upload* u) noexcept
{
	if (u->getType() == Transfer::TYPE_FILE || (u->getType() == Transfer::TYPE_FULL_LIST && BOOLSETTING(LOG_FILELIST_TRANSFERS)))
	{
		PLAY_SOUND(SOUND_UPLOADFILE);
		
		FinishedItem* item = new FinishedItem(u->getPath(), u->getHintedUser(), u->getFileSize(), u->getRunningAverage(), GET_TIME(), u->getTTH(), u->getUser()->getIPAsString());
		if (SETTING(DB_LOG_FINISHED_UPLOADS))
		{
			CFlylinkDBManager::getInstance()->save_transfer_history(e_TransferUpload, item);
		}
		rotation_items(item, e_Upload);
		fire(FinishedManagerListener::AddedUl(), item, false);
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
		         
		LogManager::message(&buf[0]);
	}
	return l_file_name;
}
/**
 * @file
 * $Id: FinishedManager.cpp 571 2011-07-27 19:18:04Z bigmuscle $
 */
