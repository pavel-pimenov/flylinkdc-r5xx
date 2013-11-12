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
#include "DownloadManager.h"
#include "QueueManager.h"
#include "Download.h"
#include "HashManager.h"
#include "MerkleCheckOutputStream.h"
#include "UploadManager.h"
#include "PGLoader.h"
#include <cmath>

static const string DOWNLOAD_AREA = "Downloads";

DownloadManager::DownloadManager()
	: runningAverage(0)//[+] IRainman refactoring transfer mechanism
{
	TimerManager::getInstance()->addListener(this);
}

DownloadManager::~DownloadManager()
{
	TimerManager::getInstance()->removeListener(this);
	while (true)
	{
		{
			// [-] Lock l(cs); [-] IRainman opt.
			if (m_download_map.empty())
				break;
		}
		Thread::sleep(100);
		dcassert(0);
		// TODO - возможно мы тут висим и не даем разрушитьс€ менеджеру?
		// ƒобавить логирование тиков на флай сервер
	}
}

void DownloadManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	typedef vector<pair<string, UserPtr> > TargetList;
	TargetList dropTargets;
	
	{
		// [-] Lock l(cs); [-] IRainman opt.
		DownloadMap l_tickList;
		int64_t l_currentSpeed = 0;// [+] IRainman refactoring transfer mechanism
		SharedLock l(cs); // [+] IRainman opt.
		// Tick each ongoing download
		for (auto i = m_download_map.cbegin(); i != m_download_map.cend(); ++i)
		{
			Download* d = i->second;
			
			if (d->getPos() > 0)
			{
				l_tickList.insert(*i);
				d->tick(aTick); //[!]IRainman refactoring transfer mechanism
			}
			const int64_t l_currentSingleSpeed = d->getRunningAverage();//[+]IRainman refactoring transfer mechanism
			l_currentSpeed += l_currentSingleSpeed;//[+]IRainman refactoring transfer mechanism
#ifdef PPA_INCLUDE_DROP_SLOW
			if (BOOLSETTING(DISCONNECTING_ENABLE))
			{
				if (d->getType() == Transfer::TYPE_FILE && d->getStart() > 0)
				{
					if (d->getTigerTree().getFileSize() > (SETTING(DISCONNECT_FILESIZE) * 1048576))
					{
						if (l_currentSingleSpeed < SETTING(DISCONNECT_SPEED) * 1024 && d->getLastNormalSpeed()) // [!] IRainman refactoring transfer mechanism
						{
							if (aTick - d->getLastNormalSpeed() > (uint32_t)SETTING(DISCONNECT_TIME) * 1000)
							{
								if (QueueManager::getInstance()->dropSource(d))
								{
									dropTargets.push_back(make_pair(d->getPath(), d->getUser()));
									d->setLastNormalSpeed(0);
								}
							}
						}
						else
						{
							d->setLastNormalSpeed(aTick);
						}
					}
				}
			}
#endif // PPA_INCLUDE_DROP_SLOW
		}
		runningAverage = l_currentSpeed; // [+] IRainman refactoring transfer mechanism
		
		if (!l_tickList.empty())
		{
			fire(DownloadManagerListener::Tick(), l_tickList, aTick);//[!]IRainman refactoring transfer mechanism + uint64_t aTick
		}
	}
	
	for (auto i = dropTargets.cbegin(); i != dropTargets.cend(); ++i)
	{
		QueueManager::getInstance()->removeSource(i->first, i->second, QueueItem::Source::FLAG_SLOW_SOURCE);
	}
}

void DownloadManager::checkIdle(const UserPtr& aUser)
{
	SharedLock l(cs);
	dcassert(aUser);
	const auto & l_find = m_idlers.find(aUser);
	if (l_find != m_idlers.end())
		l_find->second->updated();
}

void DownloadManager::addConnection(UserConnection* p_conn)
{
	if (!p_conn->isSet(UserConnection::FLAG_SUPPORTS_TTHF) || !p_conn->isSet(UserConnection::FLAG_SUPPORTS_ADCGET))
	{
		// Can't download from these...
		ClientManager::getInstance()->setClientStatus(p_conn->getUser(), STRING(SOURCE_TOO_OLD), -1, true);
		QueueManager::getInstance()->removeSource(p_conn->getUser(), QueueItem::Source::FLAG_NO_TTHF);
		p_conn->disconnect();
		return;
	}
#ifdef PPA_INCLUDE_IPFILTER
	if (PGLoader::getInstance()->check(p_conn->getRemoteIp()))
	{
		p_conn->error(STRING(YOUR_IP_IS_BLOCKED));
		p_conn->getUser()->setFlag(User::PG_BLOCK);
		LogManager::getInstance()->message("IPFilter: " + STRING(IPFILTER_BLOCK_OUT_CONNECTION) + ' ' + p_conn->getRemoteIp());
		QueueManager::getInstance()->removeSource(p_conn->getUser(), QueueItem::Source::FLAG_REMOVED);
		removeConnection(p_conn);
		return;
	}
#endif
	p_conn->addListener(this);
	checkDownloads(p_conn);
}

bool DownloadManager::startDownload(QueueItem::Priority prio)
{
	const size_t downloadCount = getDownloadCount();
	
	bool full = (SETTING(DOWNLOAD_SLOTS) != 0) && (downloadCount >= (size_t)SETTING(DOWNLOAD_SLOTS));
	full = full || ((SETTING(MAX_DOWNLOAD_SPEED) != 0) && (getRunningAverage() >= (SETTING(MAX_DOWNLOAD_SPEED) * 1024)));
	
	if (full)
	{
		bool extraFull = (SETTING(DOWNLOAD_SLOTS) != 0) && (downloadCount >= (size_t)(SETTING(DOWNLOAD_SLOTS) + SETTING(EXTRA_DOWNLOAD_SLOTS)));
		if (extraFull)
		{
			return false;
		}
		return prio == QueueItem::HIGHEST;
	}
	
	if (downloadCount > 0)
	{
		return prio != QueueItem::LOWEST;
	}
	
	return true;
}

void DownloadManager::checkDownloads(UserConnection* aConn)
{
	dcassert(aConn->getDownload() == nullptr);
	
	auto qm = QueueManager::getInstance();
	
	QueueItem::Priority prio = qm->hasDownload(aConn->getUser()); // 2012-06-17_22-40-29_TGNMP7OOMS76PE46VDZ5XTHIBKSK6H3VLQXCNHY_C3DEC470_crash-stack-r502-beta36-x64-build-10378.dmp
	if (!startDownload(prio))
	{
		removeConnection(aConn);
		return;
	}
	
	string errorMessage;
	Download* d = qm->getDownload(*aConn, errorMessage);
	
	if (!d)
	{
		if (!errorMessage.empty())
		{
			fire(DownloadManagerListener::Status(), aConn, errorMessage);
		}
		
		// [-] Lock l(cs); [-] IRainman fix.
		aConn->setState(UserConnection::STATE_IDLE);
		UniqueLock l(cs); // [+] IRainman fix.
		dcassert(aConn->getUser());
		dcassert(m_idlers.find(aConn->getUser()) == m_idlers.end());
		m_idlers[aConn->getUser()] = aConn;
		return;
	}
	
	aConn->setState(UserConnection::STATE_SND);
	
	if (aConn->isSet(UserConnection::FLAG_SUPPORTS_XML_BZLIST) && d->getType() == Transfer::TYPE_FULL_LIST)
	{
		d->setFlag(Download::FLAG_XML_BZ_LIST);
	}
	
	{
		UniqueLock l(cs);
		dcassert(d->getUser());
		dcassert(m_download_map.find(d->getUser()) == m_download_map.end());
		m_download_map[d->getUser()] = d;
	}
	fire(DownloadManagerListener::Requesting(), d);
	
	dcdebug("Requesting " I64_FMT "/" I64_FMT "\n", d->getStartPos(), d->getSize());
	
	aConn->send(d->getCommand(aConn->isSet(UserConnection::FLAG_SUPPORTS_ZLIB_GET)));
}

void DownloadManager::on(AdcCommand::SND, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	if (aSource->getState() != UserConnection::STATE_SND)
	{
		dcdebug("DM::onFileLength Bad state, ignoring\n");
		return;
	}
	if (!aSource->getDownload())
	{
		aSource->disconnect(true);
		return;
	}
	
	const string& type = cmd.getParam(0);
	int64_t start = Util::toInt64(cmd.getParam(2));
	int64_t bytes = Util::toInt64(cmd.getParam(3));
	
	if (type != Transfer::g_type_names[aSource->getDownload()->getType()])
	{
		// Uhh??? We didn't ask for this...
		aSource->disconnect();
		return;
	}
	
	startData(aSource, start, bytes, cmd.hasFlag("ZL", 4));
}

void DownloadManager::startData(UserConnection* aSource, int64_t start, int64_t bytes, bool z)
{
	Download* d = aSource->getDownload();
	dcassert(d != nullptr);
	
	dcdebug("Preparing " I64_FMT ":" I64_FMT ", " I64_FMT ":" I64_FMT "\n", d->getStartPos(), start, d->getSize(), bytes);
	if (d->getSize() == -1)
	{
		if (bytes >= 0)
		{
			d->setSize(bytes);
		}
		else
		{
			failDownload(aSource, STRING(INVALID_SIZE));
			return;
		}
	}
	else if (d->getSize() != bytes || d->getStartPos() != start)
	{
		// This is not what we requested...
		failDownload(aSource, STRING(INVALID_SIZE));
		return;
	}
	
	try
	{
		QueueManager::getInstance()->setFile(d);
	}
	catch (const FileException& e)
	{
		failDownload(aSource, STRING(COULD_NOT_OPEN_TARGET_FILE) + ' ' + e.getError());
		return;
	}
	catch (const QueueException& e) // [!] IRainman fix: only QueueException allowed here.
	{
		failDownload(aSource, e.getError());
		return;
	}
	
	try
	{
		if ((d->getType() == Transfer::TYPE_FILE || d->getType() == Transfer::TYPE_FULL_LIST) && SETTING(BUFFER_SIZE_FOR_DOWNLOADS) > 0)
		{
			d->setFile(new BufferedOutputStream<true>(d->getFile()));
		}
	}
	catch (const Exception& e)
	{
		failDownload(aSource, e.getError());
		return;
	}
	catch (...)
	{
		delete d->getFile();
		d->setFile(nullptr);
		return;
	}
	
	if (d->getType() == Transfer::TYPE_FILE)
	{
		typedef MerkleCheckOutputStream<TigerTree, true> MerkleStream;
		
		d->setFile(new MerkleStream(d->getTigerTree(), d->getFile(), d->getStartPos()));
		d->setFlag(Download::FLAG_TTH_CHECK);
	}
	
	// Check that we don't get too many bytes
	d->setFile(new LimitedOutputStream(d->getFile(), bytes));
	
	if (z)
	{
		d->setFlag(Download::FLAG_ZDOWNLOAD);
		d->setFile(new FilteredOutputStream<UnZFilter, true>(d->getFile()));
	}
	
	// [!] IRainman refactoring transfer mechanism
	d->setStart(aSource->getLastActivity());
	// [-] u->tick(GET_TICK());
	// [~]
	
	aSource->setState(UserConnection::STATE_RUNNING);
	
	fire(DownloadManagerListener::Starting(), d);
	
	if (d->getPos() == d->getSize())
	{
		try
		{
			// Already finished? A zero-byte file list could cause this...
			endData(aSource);
		}
		catch (const Exception& e)
		{
			failDownload(aSource, e.getError());
		}
	}
	else
	{
		aSource->setDataMode();
	}
}

void DownloadManager::on(UserConnectionListener::Data, UserConnection* aSource, const uint8_t* aData, size_t aLen) noexcept
{
	Download* d = aSource->getDownload();
	dcassert(d != nullptr);
	try
	{
		d->addPos(d->getFile()->write(aData, aLen), aLen); // TODO // r502-sp2 2012-04-23_22-28-18_ETFY7EDN5BIPZZSIMVUUBOZFZOGWBZY3F4D2HUA_2C747F0D_crash-stack-r501-build-9812.dmp // 2012-05-03_22-00-59_WNNHCEA5ALKEWJ3V6JCDBFS75243SQ455Y6NG7Q_059D0ACA_crash-stack-r502-beta24-build-9900.dmp
		// [-] d->tick(aSource->getLastActivity()); [-]IRainman refactoring transfer mechanism
		
		if (d->getFile()->eof())
		{
			endData(aSource);
			aSource->setLineMode(0);
		}
	}
	catch (const Exception& e)
	{
		// [!] IRainman fix: no needs.
		// d->resetPos(); // is there a better way than resetting the position?
		// [~] IRainman fix: no needs.
		failDownload(aSource, e.getError());
	}
}

/** Download finished! */
void DownloadManager::endData(UserConnection* aSource)
{
	dcassert(aSource->getState() == UserConnection::STATE_RUNNING);
	Download* d = aSource->getDownload();
	dcassert(d != nullptr);
	d->tick(aSource->getLastActivity()); // [!] IRainman refactoring transfer mechanism
	
	if (d->getType() == Transfer::TYPE_TREE)
	{
		d->getFile()->flush();
		
		int64_t bl = 1024;
		auto &l_getTigerTree = d->getTigerTree(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'd->getTigerTree()' expression repeatedly. downloadmanager.cpp 404
		while (bl * (int64_t)l_getTigerTree.getLeaves().size() < l_getTigerTree.getFileSize())
			bl *= 2;
		l_getTigerTree.setBlockSize(bl);
		l_getTigerTree.calcRoot();
		
		if (!(d->getTTH() == l_getTigerTree.getRoot()))
		{
			// This tree is for a different file, remove from queue...// [!]PPA TODO подтереть fly_hash_block
			removeDownload(d);
			fire(DownloadManagerListener::Failed(), d, STRING(INVALID_TREE));
			
			QueueManager::getInstance()->removeSource(d->getPath(), aSource->getUser(), QueueItem::Source::FLAG_BAD_TREE, false);
			
			QueueManager::getInstance()->putDownload(d, false);
			
			checkDownloads(aSource);
			return;
		}
		d->setTreeValid(true);
	}
	else
	{
		// First, finish writing the file (flushing the buffers and closing the file...)
		try
		{
			d->getFile()->flush();
		}
		catch (const Exception& e) //http://bazaar.launchpad.net/~dcplusplus-team/dcplusplus/trunk/revision/2154
		{
			d->resetPos();
			failDownload(aSource, e.getError());
			return;
		}
		
		aSource->setSpeed(d->getRunningAverage());
		aSource->updateChunkSize(d->getTigerTree().getBlockSize(), d->getSize(), GET_TICK() - d->getStart());
		
		dcdebug("Download finished: %s, size " I64_FMT ", downloaded " I64_FMT "\n", d->getPath().c_str(), d->getSize(), d->getPos());
	}
	
	removeDownload(d);
	
	if (d->getType() != Transfer::TYPE_FILE)
		fire(DownloadManagerListener::Complete(), d, d->getType() == Transfer::TYPE_TREE);
		
	QueueManager::getInstance()->putDownload(d, true, false);
	checkDownloads(aSource);
}

//[-] IRainman refactoring transfer mechanism
//int64_t DownloadManager::getRunningAverage()
//{
//	Lock l(cs);
//	int64_t avg = 0;
//	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
//	{
//		Download* d = *i;
//		avg += d->getAverageSpeed();
//	}
//	return avg;
//}

void DownloadManager::on(UserConnectionListener::MaxedOut, UserConnection* aSource, const string& param) noexcept
{
	noSlots(aSource, param);
}

void DownloadManager::noSlots(UserConnection* aSource, const string& param)
{
	if (aSource->getState() != UserConnection::STATE_SND)
	{
		dcdebug("DM::noSlots Bad state, disconnecting\n");
		aSource->disconnect();
		return;
	}
	
	string extra = param.empty() ? Util::emptyString : " - " + STRING(QUEUED) + ' ' + param;
	failDownload(aSource, STRING(NO_SLOTS_AVAILABLE) + extra);
}

void DownloadManager::onFailed(UserConnection* aSource, const string& aError)
{
	remove_idlers(aSource);
	failDownload(aSource, aError);
}

void DownloadManager::failDownload(UserConnection* aSource, const string& reason)
{
	Download* d = aSource->getDownload();
	
	if (d)
	{
		removeDownload(d);
		fire(DownloadManagerListener::Failed(), d, reason);
		
		if (d->isSet(Download::FLAG_USER_CHECK))
		{
			if (reason == STRING(DISCONNECTED))
			{
				ClientManager::getInstance()->fileListDisconnected(aSource->getUser());
			}
			else
			{
				ClientManager::getInstance()->setClientStatus(aSource->getUser(), reason, -1, false);
			}
		}
		
		QueueManager::getInstance()->putDownload(d, false);
	}
	
	removeConnection(aSource);
}

void DownloadManager::removeConnection(UserConnection* p_conn)
{
	dcassert(p_conn->getDownload() == nullptr);
	p_conn->removeListener(this);
	p_conn->disconnect();
}

void DownloadManager::removeDownload(Download* d)
{
	if (d->getFile())
	{
		if (d->getActual() > 0)
		{
			try
			{
				d->getFile()->flush();
			}
			catch (const Exception&)
			{
			}
		}
	}
	
	{
		UniqueLock l(cs);
		dcassert(m_download_map.find(d->getUser()) != m_download_map.end());
		m_download_map.erase(d->getUser());
	}
}

void DownloadManager::abortDownload(const string& aTarget)
{
	SharedLock l(cs);
	
	for (auto i = m_download_map.cbegin(); i != m_download_map.cend(); ++i)
	{
		Download* d = i->second;
		if (d->getPath() == aTarget)
		{
			dcdebug("Trying to close connection for download %p\n", d);
			d->getUserConnection().disconnect(true);
		}
	}
}

void DownloadManager::on(UserConnectionListener::ListLength, UserConnection* aSource, const string& aListLength)
{
	ClientManager::getInstance()->setListLength(aSource->getUser(), aListLength);
}

void DownloadManager::on(UserConnectionListener::FileNotAvailable, UserConnection* aSource) noexcept
{
	/* [-] IRainman fix.
	   [-]if (!aSource->getDownload())
	   [-]{
	   [-]  aSource->disconnect(true);
	   [-]  return;
	   [-]}
	   [~]*/
	fileNotAvailable(aSource);
}

/** @todo Handle errors better */
void DownloadManager::on(AdcCommand::STA, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	if (cmd.getParameters().size() < 2)
	{
		aSource->disconnect();
		return;
	}
	
	const string& err = cmd.getParameters()[0];
	if (err.length() != 3)
	{
		aSource->disconnect();
		return;
	}
	
	switch (Util::toInt(err.substr(0, 1)))
	{
		case AdcCommand::SEV_FATAL:
			aSource->disconnect();
			return;
		case AdcCommand::SEV_RECOVERABLE:
			switch (Util::toInt(err.substr(1)))
			{
				case AdcCommand::ERROR_FILE_NOT_AVAILABLE:
					fileNotAvailable(aSource);
					return;
				case AdcCommand::ERROR_SLOTS_FULL:
					string param;
					noSlots(aSource, cmd.getParam("QP", 0, param) ? param : Util::emptyString);
					return;
			}
		case AdcCommand::SEV_SUCCESS:
			// We don't know any messages that would give us these...
			dcdebug("Unknown success message %s %s", err.c_str(), cmd.getParam(1).c_str());
			return;
	}
	aSource->disconnect();
}

void DownloadManager::on(UserConnectionListener::Updated, UserConnection* aSource) noexcept
{
	remove_idlers(aSource);
	checkDownloads(aSource);
}

void DownloadManager::fileNotAvailable(UserConnection* aSource)
{
	if (aSource->getState() != UserConnection::STATE_SND)
	{
		dcdebug("DM::fileNotAvailable Invalid state, disconnecting");
		aSource->disconnect();
		return;
	}
	
	Download* d = aSource->getDownload();
	dcassert(d != nullptr);
	dcdebug("File Not Available: %s\n", d->getPath().c_str());
	
	removeDownload(d);
	fire(DownloadManagerListener::Failed(), d, STRING(FILE_NOT_AVAILABLE));
	
	if (d->isSet(Download::FLAG_USER_CHECK))
	{
		ClientManager::getInstance()->setClientStatus(aSource->getUser(), "Filelist Not Available", SETTING(FILELIST_UNAVAILABLE), false);
	}
	
	QueueManager::getInstance()->removeSource(d->getPath(), aSource->getUser(), (Flags::MaskType)(d->getType() == Transfer::TYPE_TREE ? QueueItem::Source::FLAG_NO_TREE : QueueItem::Source::FLAG_FILE_NOT_AVAILABLE), false);
	
	QueueManager::getInstance()->putDownload(d, false);
	checkDownloads(aSource);
}

// !SMT!-S
bool DownloadManager::checkFileDownload(const UserPtr& aUser)
{
	SharedLock l(cs);
	const auto& l_find = m_download_map.find(aUser);
	if (l_find != m_download_map.end())
		if (l_find->second->getType() != Download::TYPE_PARTIAL_LIST &&
		        l_find->second->getType() != Download::TYPE_FULL_LIST &&
		        l_find->second->getType() != Download::TYPE_TREE)
		{
			return true;
		}
		
	/*  for (auto i = m_download_map.cbegin(); i != m_download_map.cend(); ++i)
	    {
	        Download* d = i->second;
	            if (d->getUser() == aUser && d->getType() != Download::TYPE_PARTIAL_LIST && d->getType() != Download::TYPE_FULL_LIST && d->getType() != Download::TYPE_TREE)
	                return true;
	    }
	*/
	return false;
}
/*#ifdef IRAINMAN_ENABLE_AUTO_BAN
void DownloadManager::on(UserConnectionListener::BanMessage, UserConnection* aSource, const string& aMessage) noexcept // !SMT!-B
{
    failDownload(aSource, aMessage);
}
#endif*/
// [+] SSA
void DownloadManager::on(UserConnectionListener::CheckUserIP, UserConnection* aSource)
{
	Download* d = aSource->getDownload();
	
	dcassert(d);
	removeDownload(d);
	QueueManager::getInstance()->putDownload(d, true);
	
	removeConnection(aSource);
}


/**
 * @file
 * $Id: DownloadManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
