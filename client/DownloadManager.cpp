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
#include <cmath>

#include "DownloadManager.h"
#include "QueueManager.h"
#include "Download.h"
#include "HashManager.h"
#include "MerkleCheckOutputStream.h"
#include "UploadManager.h"
#include "PGLoader.h"
#include "../FlyFeatures/flyServer.h"

static const string DOWNLOAD_AREA = "Downloads";
std::unique_ptr<webrtc::RWLockWrapper> DownloadManager::g_csDownload = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
DownloadMap DownloadManager::g_download_map;
IdlersMap DownloadManager::g_idlers;
int64_t DownloadManager::g_runningAverage;

DownloadManager::DownloadManager()
{
	TimerManager::getInstance()->addListener(this);
}

DownloadManager::~DownloadManager()
{
	TimerManager::getInstance()->removeListener(this);
	while (true)
	{
		{
			CFlyReadLock(*g_csDownload);
			if (g_download_map.empty())
			{
				break;
			}
		}
		Thread::sleep(10);
		// dcassert(0);
		// TODO - возможно мы тут висим и не даем разрушитьс€ менеджеру?
		// ƒобавить логирование тиков на флай сервер
	}
}

void DownloadManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	dcassert(!ClientManager::isShutdown());
	typedef vector<pair<string, UserPtr> > TargetList;
	TargetList dropTargets;
	
	DownloadArray l_tickList;
	{
		int64_t l_currentSpeed = 0;// [+] IRainman refactoring transfer mechanism
		CFlyReadLock(*g_csDownload);
		// Tick each ongoing download
		l_tickList.reserve(g_download_map.size());
		for (auto i = g_download_map.cbegin(); i != g_download_map.cend(); ++i)
		{
			auto d = i->second;
			if (d->getPos() > 0)
			{
				TransferData l_td;
				l_td.m_hinted_user = d->getHintedUser();
				l_td.m_pos = d->getPos();
				l_td.m_actual = d->getActual();
				l_td.m_second_left = d->getSecondsLeft();
				l_td.m_running_average = d->getRunningAverage();
				l_td.m_start = d->getStart();
				l_td.m_size = d->getSize();
				l_td.m_type = d->getType();
				l_td.m_path = d->getPath();
				l_td.calc_percent();
				if (d->isSet(Download::FLAG_DOWNLOAD_PARTIAL))
				{
					l_td.m_status_string += _T("[P]");
				}
				if (d->m_isSecure)
				{
					if (d->m_isTrusted)
					{
						l_td.m_status_string += _T("[S]");
					}
					else
					{
						l_td.m_status_string += _T("[U]");
					}
				}
				if (d->isSet(Download::FLAG_TTH_CHECK))
				{
					l_td.m_status_string += _T("[T]");
				}
				if (d->isSet(Download::FLAG_ZDOWNLOAD))
				{
					l_td.m_status_string += _T("[Z]");
				}
				if (d->isSet(Download::FLAG_CHUNKED))
				{
					l_td.m_status_string += _T("[C]");
				}
				if (!l_td.m_status_string.empty())
				{
					l_td.m_status_string += _T(' ');
				}
				l_td.m_status_string += Text::tformat(TSTRING(DOWNLOADED_BYTES), Util::formatBytesW(l_td.m_pos).c_str(), l_td.m_percent, l_td.get_elapsed(aTick).c_str());
				l_td.log_debug();
				l_tickList.push_back(l_td);
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
		g_runningAverage = l_currentSpeed; // [+] IRainman refactoring transfer mechanism
	}
	if (!l_tickList.empty())
	{
		fly_fire1(DownloadManagerListener::Tick(), l_tickList);//[!]IRainman refactoring transfer mechanism + uint64_t aTick
	}
	
	for (auto i = dropTargets.cbegin(); i != dropTargets.cend(); ++i)
	{
		QueueManager::getInstance()->removeSource(i->first, i->second, QueueItem::Source::FLAG_SLOW_SOURCE);
	}
}

void DownloadManager::remove_idlers(UserConnection* aSource)
{
	//dcassert(!ClientManager::isShutdown());
	CFlyWriteLock(*g_csDownload);
	if (!ClientManager::isShutdown())
	{
		dcassert(aSource->getUser());
		// ћогут быть не найдены.
		// лишн€€ проверка dcassert(m_idlers.find(aSource->getUser()) != m_idlers.end());
		g_idlers.erase(aSource->getUser());
	}
	else
	{
		g_idlers.clear();
	}
}

void DownloadManager::checkIdle(const UserPtr& aUser)
{
	dcassert(!ClientManager::isShutdown());
	if (!ClientManager::isShutdown())
	{
		CFlyReadLock(*g_csDownload);
		dcassert(aUser);
		const auto& l_find = g_idlers.find(aUser);
		if (l_find != g_idlers.end())
		{
			l_find->second->updated();
		}
	}
}

void DownloadManager::addConnection(UserConnection* p_conn)
{
	dcassert(!ClientManager::isShutdown());
	if (!p_conn->isSet(UserConnection::FLAG_SUPPORTS_TTHF) || !p_conn->isSet(UserConnection::FLAG_SUPPORTS_ADCGET))
	{
		// Can't download from these...
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		ClientManager::setClientStatus(p_conn->getUser(), STRING(SOURCE_TOO_OLD), -1, true);
#endif
		QueueManager::getInstance()->removeSource(p_conn->getUser(), QueueItem::Source::FLAG_NO_TTHF);
		p_conn->disconnect();
		return;
	}
	if (p_conn->isIPGuard(ResourceManager::IPFILTER_BLOCK_OUT_CONNECTION, true))
	{
		removeConnection(p_conn, false);
		return;
	}
	p_conn->addListener(this);
	checkDownloads(p_conn);
}

bool DownloadManager::isStartDownload(QueueItem::Priority prio)
{
	dcassert(!ClientManager::isShutdown());
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
	if (ClientManager::isShutdown())
	{
		removeConnection(aConn);
#ifdef _DEBUG
		LogManager::message("DownloadManager::checkDownloads + isShutdown");
#endif
		return;
	}
	///////////////// dcassert(aConn->getDownload() == nullptr);
	
	auto qm = QueueManager::getInstance();
	
	QueueItem::Priority prio = QueueManager::hasDownload(aConn->getUser());
	if (!isStartDownload(prio))
	{
		removeConnection(aConn);
		return;
	}
	
	string errorMessage;
	auto d = qm->getDownload(aConn, errorMessage);
	
	if (!d)
	{
		if (!errorMessage.empty())
		{
			fly_fire2(DownloadManagerListener::Status(), aConn, errorMessage);
		}
		
		aConn->setState(UserConnection::STATE_IDLE);
		dcassert(!ClientManager::isShutdown());
		if (!ClientManager::isShutdown())
		{
			CFlyWriteLock(*g_csDownload);
			dcassert(aConn->getUser());
			dcassert(g_idlers.find(aConn->getUser()) == g_idlers.end());
			g_idlers[aConn->getUser()] = aConn;
		}
		return;
	}
	
	aConn->setState(UserConnection::STATE_SND);
	
	if (aConn->isSet(UserConnection::FLAG_SUPPORTS_XML_BZLIST) && d->getType() == Transfer::TYPE_FULL_LIST)
	{
		d->setFlag(Download::FLAG_XML_BZ_LIST);
	}
	
	{
		CFlyWriteLock(*g_csDownload);
		dcassert(d->getUser());
		dcassert(g_download_map.find(d->getUser()) == g_download_map.end());
		g_download_map[d->getUser()] = d;
	}
	fly_fire1(DownloadManagerListener::Requesting(), d);
	
	dcdebug("Requesting " I64_FMT "/" I64_FMT "\n", d->getStartPos(), d->getSize());
	AdcCommand cmd(AdcCommand::CMD_GET);
	d->getCommand(cmd, aConn->isSet(UserConnection::FLAG_SUPPORTS_ZLIB_GET));
	aConn->send(cmd);
}

void DownloadManager::on(AdcCommand::SND, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	dcassert(!ClientManager::isShutdown());
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
	dcassert(!ClientManager::isShutdown());
	auto d = aSource->getDownload();
	dcassert(d);
	
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
	
	const int64_t l_buf_size = SETTING(BUFFER_SIZE_FOR_DOWNLOADS) * 1024;
	try
	{
		if ((d->getType() == Transfer::TYPE_FILE || d->getType() == Transfer::TYPE_FULL_LIST) && l_buf_size > 0)
		{
			const int64_t l_file_size = d->getSize();
			const auto l_file = new BufferedOutputStream<true>(d->getDownloadFile(), std::min(l_file_size, l_buf_size));
			d->setDownloadFile(l_file);
		}
	}
	catch (const Exception& e)
	{
		failDownload(aSource, e.getError());
		return;
	}
	catch (...)
	{
		LogManager::message("catch (...) Error new BufferedOutputStream<true> l_buf_size (Mb) = " + Util::toString(l_buf_size / 1024 / 1024) + " email: ppa74@ya.ru");
		d->reset_download_file();
		return;
	}
	
	if (d->getType() == Transfer::TYPE_FILE)
	{
		typedef MerkleCheckOutputStream<TigerTree, true> MerkleStream;
		
		d->setDownloadFile(new MerkleStream(d->getTigerTree(), d->getDownloadFile(), d->getStartPos()));
		d->setFlag(Download::FLAG_TTH_CHECK);
	}
	
	// Check that we don't get too many bytes
	d->setDownloadFile(new LimitedOutputStream(d->getDownloadFile(), bytes));
	
	if (z)
	{
		d->setFlag(Download::FLAG_ZDOWNLOAD);
		d->setDownloadFile(new FilteredOutputStream<UnZFilter, true> (d->getDownloadFile()));
	}
	
	// [!] IRainman refactoring transfer mechanism
	d->setStart(aSource->getLastActivity());
	// [-] u->tick(GET_TICK());
	// [~]
	
	aSource->setState(UserConnection::STATE_RUNNING);
	
	fly_fire1(DownloadManagerListener::Starting(), d);
	
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
	// TODO dcassert(!ClientManager::isShutdown());
	auto d = aSource->getDownload();
	dcassert(d);
	try
	{
		d->addPos(d->getDownloadFile()->write(aData, aLen), aLen);
		// [-] d->tick(aSource->getLastActivity()); [-]IRainman refactoring transfer mechanism
		
		if (d->getDownloadFile()->eof())
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
	// dcassert(!ClientManager::isShutdown());
	dcassert(aSource->getState() == UserConnection::STATE_RUNNING);
	auto d = aSource->getDownload();
	dcassert(d);
	d->tick(aSource->getLastActivity()); // [!] IRainman refactoring transfer mechanism
	
	if (d->getType() == Transfer::TYPE_TREE)
	{
		d->getDownloadFile()->flush();
		
		int64_t bl = 1024;
		auto &l_getTigerTree = d->getTigerTree(); // [!] PVS V807 Decreased performance. Consider creating a reference to avoid using the 'd->getTigerTree()' expression repeatedly. downloadmanager.cpp 404
		while (bl * (int64_t)l_getTigerTree.getLeaves().size() < l_getTigerTree.getFileSize())
		{
			bl *= 2;
		}
		l_getTigerTree.setBlockSize(bl);
		l_getTigerTree.calcRoot();
		
		if (!(d->getTTH() == l_getTigerTree.getRoot()))
		{
			// This tree is for a different file, remove from queue...// [!]PPA TODO подтереть fly_hash_block
			removeDownload(d);
			fly_fire2(DownloadManagerListener::Failed(), d, STRING(INVALID_TREE));
			
			QueueManager::getInstance()->removeSource(d->getPath(), aSource->getUser(), QueueItem::Source::FLAG_BAD_TREE, false);
			
			QueueManager::getInstance()->putDownload(d->getPath(), d, false);
			
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
			d->getDownloadFile()->flush();
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
	{
		fly_fire1(DownloadManagerListener::Complete(), d);
	}
	try
	{
		QueueManager::getInstance()->putDownload(d->getPath(), d, true, false);
	}
	catch (const HashException& e)
	{
		//aSource->setDownload(nullptr);
		const string l_error = "[DownloadManager::endData]HashException - for " + d->getPath() + " Error = " = e.getError();
		LogManager::message(l_error);
		CFlyServerJSON::pushError(30, l_error);
		dcassert(0);
		//failDownload(aSource, e.getError());
		return;
	}
	checkDownloads(aSource);
}

//[-] IRainman refactoring transfer mechanism
//int64_t DownloadManager::getRunningAverage()
//{
//	CFlyReadLock(*g_csDownload);
//	int64_t avg = 0;
//	for (auto i = downloads.cbegin(); i != downloads.cend(); ++i)
//	{
//		auto d = *i;
//		avg += d->getAverageSpeed();
//	}
//	return avg;
//}

void DownloadManager::on(UserConnectionListener::MaxedOut, UserConnection* aSource, const string& param) noexcept
{
	dcassert(!ClientManager::isShutdown());
	noSlots(aSource, param);
}

void DownloadManager::noSlots(UserConnection* aSource, const string& param)
{
	dcassert(!ClientManager::isShutdown());
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
	// TODO dcassert(!ClientManager::isShutdown());
	remove_idlers(aSource);
	failDownload(aSource, aError);
}

void DownloadManager::failDownload(UserConnection* aSource, const string& p_reason)
{
	// TODO dcassert(!ClientManager::isShutdown());
	auto d = aSource->getDownload();
	
	if (d)
	{
		const string l_path = d->getPath();
		removeDownload(d);
		fly_fire2(DownloadManagerListener::Failed(), d, p_reason);
		
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		if (d->isSet(Download::FLAG_USER_CHECK))
		{
			if (reason == STRING(DISCONNECTED))
			{
				ClientManager::fileListDisconnected(aSource->getUser());
			}
			else
			{
				ClientManager::setClientStatus(aSource->getUser(), reason, -1, false);
			}
		}
#endif
		//d->m_reason = p_reason;
		QueueManager::getInstance()->putDownload(l_path, d, false);
	}
	
	removeConnection(aSource);
}

void DownloadManager::removeConnection(UserConnection* p_conn, bool p_is_remove_listener /* = true */)
{
	// dcassert(!ClientManager::isShutdown());
	///////////// dcassert(p_conn->getDownload() == nullptr);
	if (p_is_remove_listener)
	{
		p_conn->removeListener(this);
	}
	p_conn->disconnect();
}

void DownloadManager::removeDownload(const DownloadPtr& d)
{
	// TODO dcassert(!ClientManager::isShutdown());
	if (d->getDownloadFile())
	{
		if (d->getActual() > 0)
		{
			try
			{
				d->getDownloadFile()->flush();
			}
			catch (const Exception&)
			{
			}
		}
	}
	
	{
		CFlyWriteLock(*g_csDownload);
		//////////////dcassert(g_download_map.find(d->getUser()) != g_download_map.end());
		g_download_map.erase(d->getUser());
	}
}

void DownloadManager::abortDownload(const string& aTarget)
{
	dcassert(!ClientManager::isShutdown());
	CFlyReadLock(*g_csDownload);
	for (auto i = g_download_map.cbegin(); i != g_download_map.cend(); ++i)
	{
		auto d = i->second;
		if (d->getPath() == aTarget)
		{
			dcdebug("Trying to close connection for download %p\n", d.get());
			d->getUserConnection()->disconnect(true);
		}
	}
}

void DownloadManager::on(UserConnectionListener::ListLength, UserConnection* aSource, const string& aListLength) noexcept
{
	dcassert(!ClientManager::isShutdown());
	ClientManager::getInstance()->setListLength(aSource->getUser(), aListLength);
}

void DownloadManager::on(UserConnectionListener::FileNotAvailable, UserConnection* aSource) noexcept
{
	fileNotAvailable(aSource);
}

/** @todo Handle errors better */
void DownloadManager::on(AdcCommand::STA, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	dcassert(!ClientManager::isShutdown());
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
	dcassert(!ClientManager::isShutdown());
	remove_idlers(aSource);
	checkDownloads(aSource);
}

void DownloadManager::fileNotAvailable(UserConnection* aSource)
{
	dcassert(!ClientManager::isShutdown());
	if (aSource->getState() != UserConnection::STATE_SND)
	{
		dcdebug("DM::fileNotAvailable Invalid state, disconnecting");
		aSource->disconnect();
		return;
	}
	
	auto d = aSource->getDownload();
	dcassert(d);
	dcdebug("File Not Available: %s\n", d->getPath().c_str());
	
	removeDownload(d);
	fly_fire2(DownloadManagerListener::Failed(), d, STRING(FILE_NOT_AVAILABLE));
	
#ifdef IRAINMAN_INCLUDE_USER_CHECK
	if (d->isSet(Download::FLAG_USER_CHECK))
	{
		ClientManager::setClientStatus(aSource->getUser(), "Filelist Not Available", SETTING(FILELIST_UNAVAILABLE), false);
	}
#endif
	
	QueueManager::getInstance()->removeSource(d->getPath(), aSource->getUser(), (Flags::MaskType)(d->getType() == Transfer::TYPE_TREE ? QueueItem::Source::FLAG_NO_TREE : QueueItem::Source::FLAG_FILE_NOT_AVAILABLE), false);
	
	//d->m_reason = STRING(FILE_NOT_AVAILABLE);
	QueueManager::getInstance()->putDownload(d->getPath(), d, false);
	checkDownloads(aSource);
}

// !SMT!-S
bool DownloadManager::checkFileDownload(const UserPtr& aUser)
{
	dcassert(!ClientManager::isShutdown());
	CFlyReadLock(*g_csDownload);
	const auto& l_find = g_download_map.find(aUser);
	if (l_find != g_download_map.end())
		if (l_find->second->getType() != Download::TYPE_PARTIAL_LIST &&
		        l_find->second->getType() != Download::TYPE_FULL_LIST &&
		        l_find->second->getType() != Download::TYPE_TREE)
		{
			return true;
		}
		
	/*  for (auto i = m_download_map.cbegin(); i != m_download_map.cend(); ++i)
	    {
	        auto d = i->second;
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
void DownloadManager::on(UserConnectionListener::CheckUserIP, UserConnection* aSource) noexcept
{
	dcassert(!ClientManager::isShutdown());
	auto d = aSource->getDownload();
	
	dcassert(d);
	removeDownload(d);
	QueueManager::getInstance()->putDownload(d->getPath(), d, true);
	
	removeConnection(aSource);
}


/**
 * @file
 * $Id: DownloadManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
