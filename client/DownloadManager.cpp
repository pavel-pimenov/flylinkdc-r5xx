/*
 * Copyright (C) 2001-2017 Jacek Sieka, arnetheduck on gmail point com
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
#include "ConnectionManager.h"
#include "QueueManager.h"
#include "Download.h"
#include "HashManager.h"
#include "MerkleCheckOutputStream.h"
#include "UploadManager.h"
#include "FinishedManager.h"
#include "PGLoader.h"
#include "../FlyFeatures/flyServer.h"

#ifdef FLYLINKDC_USE_TORRENT
#include "libtorrent/session.hpp"
//#include "libtorrent/session_settings.hpp"
#include "libtorrent/torrent_info.hpp"
#include "libtorrent/alert_types.hpp"
#include "libtorrent/announce_entry.hpp"
#include "libtorrent/torrent_status.hpp"
#include "libtorrent/create_torrent.hpp"
#include "libtorrent/hex.hpp"
#endif



static const string DOWNLOAD_AREA = "Downloads";
std::unique_ptr<webrtc::RWLockWrapper> DownloadManager::g_csDownload = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
DownloadMap DownloadManager::g_download_map;
IdlersMap DownloadManager::g_idlers;
int64_t DownloadManager::g_runningAverage;

DownloadManager::DownloadManager()
{
	TimerManager::getInstance()->addListener(this);
}
void DownloadManager::shutdown_torrent()
{
	if (m_torrent_session)
	{
for (auto i : m_torrents)
		{
			i.save_resume_data(libtorrent::torrent_handle::save_info_dict | libtorrent::torrent_handle::only_if_modified);
			++m_torrent_resume_count;
		}
		int l_count = 10; // TODO
		while (m_torrent_resume_count == 0 && l_count-- > 0)
		{
			Sleep(1000);
		}
		m_torrent_session.reset();
	}
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
		// TODO - возможно мы тут висим и не даем разрушиться менеджеру?
		// Добавить логирование тиков на флай сервер
	}
}

void DownloadManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
		
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
			if (d->getPos() > 0) // https://drdump.com/DumpGroup.aspx?DumpGroupID=614035&Login=guest (Wine)
			{
				TransferData l_td(d->getUserConnection()->getConnectionQueueToken());
				l_td.m_hinted_user = d->getHintedUser();
				l_td.m_pos = d->getPos();
				l_td.m_actual = d->getActual();
				l_td.m_second_left = d->getSecondsLeft();
				l_td.m_speed = d->getRunningAverage();
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
					l_td.m_status_string += _T("[SSL+");
					if (d->m_isTrusted)
					{
						l_td.m_status_string += _T("T]");
					}
					else
					{
						l_td.m_status_string += _T("U]");
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
#ifdef FLYLINKDC_USE_DROP_SLOW
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
#endif // FLYLINKDC_USE_DROP_SLOW
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
	//dcassert(!ClientManager::isBeforeShutdown());
	CFlyWriteLock(*g_csDownload);
	if (!ClientManager::isBeforeShutdown())
	{
		dcassert(aSource->getUser());
		// Могут быть не найдены.
		// лишняя проверка dcassert(m_idlers.find(aSource->getUser()) != m_idlers.end());
		g_idlers.erase(aSource->getUser());
	}
	else
	{
		g_idlers.clear();
	}
}

void DownloadManager::checkIdle(const UserPtr& aUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!ClientManager::isBeforeShutdown())
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
	dcassert(!ClientManager::isBeforeShutdown());
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
	dcassert(!ClientManager::isBeforeShutdown());
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
	if (ClientManager::isBeforeShutdown())
	{
		removeConnection(aConn);
#ifdef _DEBUG
		LogManager::message("DownloadManager::checkDownloads + isShutdown");
#endif
		return;
	}
	///////////////// dcassert(aConn->getDownload() == nullptr);
	
	auto qm = QueueManager::getInstance();
	
	const QueueItem::Priority prio = QueueManager::hasDownload(aConn->getUser());
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
		dcassert(!ClientManager::isBeforeShutdown());
		if (!ClientManager::isBeforeShutdown())
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
	dcassert(!ClientManager::isBeforeShutdown());
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
	
	const string type = cmd.getParam(0);
	const int64_t start = Util::toInt64(cmd.getParam(2));
	const int64_t bytes = Util::toInt64(cmd.getParam(3));
	
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
	dcassert(!ClientManager::isBeforeShutdown());
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
	
#ifdef FLYLINKDC_USE_DOWNLOAD_STARTING_FIRE
	fly_fire1(DownloadManagerListener::Starting(), d);
#endif
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

void DownloadManager::fireData(UserConnection* aSource, const uint8_t* aData, size_t aLen) noexcept
{
	// TODO dcassert(!ClientManager::isBeforeShutdown());
	auto d = aSource->getDownload();
	dcassert(d);
	try
	{
		d->addPos(d->getDownloadFile()->write(aData, aLen), aLen);
		
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
void DownloadManager::on(UserConnectionListener::Data, UserConnection* aSource, const uint8_t* aData, size_t aLen) noexcept
{
	fireData(aSource, aData, aLen);
}

/** Download finished! */
void DownloadManager::endData(UserConnection* aSource)
{
	// dcassert(!ClientManager::isBeforeShutdown());
	dcassert(aSource->getState() == UserConnection::STATE_RUNNING);
	auto d = aSource->getDownload();
	dcassert(d);
	d->tick(aSource->getLastActivity()); // [!] IRainman refactoring transfer mechanism
	
	if (d->getType() == Transfer::TYPE_TREE)
	{
		d->getDownloadFile()->flushBuffers(false);
		
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
			d->getDownloadFile()->flushBuffers(true);
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
	dcassert(!ClientManager::isBeforeShutdown());
	noSlots(aSource, param);
}

void DownloadManager::noSlots(UserConnection* aSource, const string& param)
{
	dcassert(!ClientManager::isBeforeShutdown());
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
	// TODO dcassert(!ClientManager::isBeforeShutdown());
	remove_idlers(aSource);
	failDownload(aSource, aError);
}

void DownloadManager::failDownload(UserConnection* aSource, const string& p_reason)
{
	// TODO dcassert(!ClientManager::isBeforeShutdown());
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
		d->m_reason = p_reason;
		QueueManager::getInstance()->putDownload(l_path, d, false);
	}
	
	removeConnection(aSource);
}

void DownloadManager::removeConnection(UserConnection* p_conn, bool p_is_remove_listener /* = true */)
{
	// dcassert(!ClientManager::isBeforeShutdown());
	///////////// dcassert(p_conn->getDownload() == nullptr);
	if (p_is_remove_listener)
	{
		p_conn->removeListener(this);
	}
	p_conn->disconnect();
}

void DownloadManager::removeDownload(const DownloadPtr& d)
{
	// TODO dcassert(!ClientManager::isBeforeShutdown());
	if (d->getDownloadFile())
	{
		if (d->getActual() > 0)
		{
			try
			{
				d->getDownloadFile()->flushBuffers(false);
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
	dcassert(!ClientManager::isBeforeShutdown());
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
	dcassert(!ClientManager::isBeforeShutdown());
	ClientManager::setListLength(aSource->getUser(), aListLength);
}

void DownloadManager::on(UserConnectionListener::FileNotAvailable, UserConnection* aSource) noexcept
{
	fileNotAvailable(aSource);
}

/** @todo Handle errors better */
void DownloadManager::on(AdcCommand::STA, UserConnection* aSource, const AdcCommand& cmd) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
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
	dcassert(!ClientManager::isBeforeShutdown());
	remove_idlers(aSource);
	checkDownloads(aSource);
}

void DownloadManager::fileNotAvailable(UserConnection* aSource)
{
	dcassert(!ClientManager::isBeforeShutdown());
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
	
	d->m_reason = STRING(FILE_NOT_AVAILABLE);
	QueueManager::getInstance()->putDownload(d->getPath(), d, false);
	checkDownloads(aSource);
}

// !SMT!-S
bool DownloadManager::checkFileDownload(const UserPtr& aUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
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
	dcassert(!ClientManager::isBeforeShutdown());
	auto d = aSource->getDownload();
	
	dcassert(d);
	removeDownload(d);
	QueueManager::getInstance()->putDownload(d->getPath(), d, true);
	
	removeConnection(aSource);
}

#ifdef FLYLINKDC_USE_TORRENT

using namespace libtorrent;
namespace lt = libtorrent;

void DownloadManager::onTorrentAlertNotify(libtorrent::session* p_torrent_sesion)
{
	try
	{
		p_torrent_sesion->get_io_service().post([p_torrent_sesion, this]
		{
			p_torrent_sesion->post_torrent_updates();
			p_torrent_sesion->post_session_stats();
			p_torrent_sesion->post_dht_stats();
			std::vector<lt::alert*> alerts;
			p_torrent_sesion->pop_alerts(&alerts);
for (lt::alert const * a : alerts)
			{
				try
				{
#ifdef _DEBUG
					//LogManager::torrent_message(".:::. TorrentAllert:" + a->message() + " info:" + std::string(a->what()));
#endif
					if (const auto l_port = lt::alert_cast<lt::portmap_alert>(a))
					{
						LogManager::message("portmap_alert: " + a->message() + " info:" + std::string(a->what()));
						SettingsManager::g_upnpTorrentLevel = true;
					}
					if (const auto l_port = lt::alert_cast<lt::portmap_error_alert>(a))
					{
						LogManager::message("portmap_error_alert: " + a->message() + " info:" + std::string(a->what()));
						SettingsManager::g_upnpTorrentLevel = false;
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_removed_alert>(a))
					{
						LogManager::torrent_message("torrent_removed_alert: " + a->message());
						{
							auto i = m_torrents.find(l_delete->handle);
							if (i == m_torrents.end())
							{
								for (i = m_torrents.cbegin(); i != m_torrents.cend(); ++i)
								{
									if (!i->is_valid())
										continue;
									if (i->info_hash() == l_delete->info_hash)
										break;
								}
							}
							if (i != m_torrents.end())
							{
								m_torrents.erase(i);
							}
						}
						CFlylinkDBManager::getInstance()->delete_torrent_resume(l_delete->info_hash);
						fly_fire1(DownloadManagerListener::RemoveTorrent(), l_delete->info_hash);
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_delete_failed_alert>(a))
					{
						LogManager::torrent_message("torrent_deleted_alert: " + a->message());
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_deleted_alert>(a))
					{
						LogManager::torrent_message("torrent_deleted_alert: " + a->message());
						fly_fire1(DownloadManagerListener::RemoveTorrent(), l_delete->info_hash);
					}
					if (lt::alert_cast<lt::peer_connect_alert>(a))
					{
						LogManager::torrent_message("peer_connect_alert: " + a->message());
					}
					if (lt::alert_cast<lt::peer_disconnected_alert>(a))
					{
						LogManager::torrent_message("peer_disconnected_alert: " + a->message());
					}
					if (lt::alert_cast<lt::peer_error_alert>(a))
					{
						LogManager::torrent_message("peer_error_alert: " + a->message());
					}
					if (const auto l_metadata = lt::alert_cast<metadata_received_alert>(a))
					{
						l_metadata->handle.save_resume_data(torrent_handle::save_info_dict | torrent_handle::only_if_modified);
						++m_torrent_resume_count;
					}
					if (const auto l_a = lt::alert_cast<torrent_paused_alert>(a))
					{
						LogManager::torrent_message("torrent_paused_alert: " + a->message());
						// TODO - тут разобрать файлы и показать что хочется качать
					}
					if (const auto l_a = lt::alert_cast<lt::file_completed_alert>(a))
					{
						auto l_files = l_a->handle.torrent_file()->files();
						const auto l_hash_file = l_files.hash(l_a->index);
						const auto l_size = l_files.file_size(l_a->index);
						const auto l_name = l_files.file_name(l_a->index).to_string();
						const auto l_file_path = SETTING(DOWNLOAD_DIRECTORY) + l_files.file_path(l_a->index) + l_name;
						// заменить SETTING(DOWNLOAD_DIRECTORY) на путь выбора когда научусь качать файлы в нужный каталог
						// const torrent_handle h = l_a->handle;
						libtorrent::sha1_hash l_sha1;
						bool p_is_sha1_for_file = false;
						if (!l_hash_file.is_all_zeros())
						{
							l_sha1 = l_hash_file;
							p_is_sha1_for_file = true;
						}
						else
						{
							l_sha1 = l_a->handle.info_hash();
						}
						LogManager::torrent_message("file_completed_alert: " + a->message() + " Path:" + l_file_path +
						                            +" sha1:" + aux::to_hex(l_sha1));
						auto l_item = std::make_shared<FinishedItem>(l_file_path, l_sha1, l_size, 0, GET_TIME(), 0, 0);
						CFlylinkDBManager::getInstance()->save_transfer_history(true, e_TransferDownload, l_item);
						if (FinishedManager::isValidInstance())
						{
							FinishedManager::getInstance()->pushHistoryFinishedItem(l_item, e_TransferDownload);
							FinishedManager::getInstance()->updateStatus();
						}
						
						const CFlyTTHKey l_file(l_sha1, l_size, p_is_sha1_for_file);
						CFlyServerJSON::addDownloadCounter(l_file, l_name); // Util::getFileName(l_path)
#ifdef _DEBUG
						CFlyServerJSON::sendDownloadCounter(false);
#endif
					}
					if (const auto l_a = lt::alert_cast<lt::add_torrent_alert>(a))
					{
						auto l_name = l_a->handle.status(torrent_handle::query_name);
						LogManager::message("Added torrent: " + l_name.name);
						m_torrents.insert(l_a->handle);
						if (l_name.has_metadata)
						{
							// TODO - не писать в базу если идет загрузка торрентов из базы
							l_a->handle.save_resume_data(torrent_handle::save_info_dict | torrent_handle::only_if_modified);
							++m_torrent_resume_count;
						}
					}
					if (const auto l_a = lt::alert_cast<lt::torrent_finished_alert>(a))
					{
						LogManager::torrent_message("torrent_finished_alert: " + a->message());
#ifdef _DEBUG
						CFlyServerJSON::sendDownloadCounter(false);
#endif
						
						//TODO
						//l_a->handle.set_max_connections(max_connections / 2);
						// TODO ?
						CFlylinkDBManager::getInstance()->delete_torrent_resume(l_a->handle.info_hash());
						m_torrents.erase(l_a->handle);
						//l_a->handle.save_resume_data(torrent_handle::save_info_dict | torrent_handle::only_if_modified);
						//++m_torrent_resume_count;
					}
					
					if (const auto l_a = lt::alert_cast<save_resume_data_failed_alert>(a))
					{
						dcassert(m_torrent_resume_count > 0);
						--m_torrent_resume_count;
						LogManager::torrent_message("save_resume_data_failed_alert: " + l_a->message() + " info:" + std::string(a->what()));
					}
					if (const auto l_a = lt::alert_cast<save_resume_data_alert>(a))
					{
						torrent_handle h = l_a->handle;
						TORRENT_ASSERT(l_a->resume_data);
						if (l_a->resume_data)
						{
							--m_torrent_resume_count;
							std::vector<char> l_resume;
							bencode(std::back_inserter(l_resume), *l_a->resume_data);
							const torrent_status st = h.status(torrent_handle::query_save_path | torrent_handle::query_name);
							CFlylinkDBManager::getInstance()->save_torrent_resume(st.info_hash, st.name, l_resume);
							// TODO l_a->handle.set_pinned(false);
						}
					}
					if (lt::alert_cast<lt::torrent_error_alert>(a))
					{
						LogManager::torrent_message("torrent_error_alert: " + a->message() + " info:" + std::string(a->what()));
					}
					if (auto st = lt::alert_cast<lt::state_update_alert>(a))
					{
						if (st->status.empty())
						{
							continue;
						}
						int l_pos = 1;
for (const auto j : st->status)
						{
							lt::torrent_status const& s = j;
							std::string l_log = "[" + Util::toString(l_pos) + "] Status: " + st->message() + " [ " + s.save_path + "\\" + s.name
							                    + " ] Download: " + Util::toString(s.download_payload_rate / 1000) + " kB/s "
							                    + " ] Upload: " + Util::toString(s.upload_payload_rate / 1000) + " kB/s "
							                    + Util::toString(s.total_done / 1000) + " kB ("
							                    + Util::toString(s.progress_ppm / 10000) + "%) downloaded sha1: " + aux::to_hex(s.info_hash);
							static std::string g_last_log;
							if (g_last_log != l_log)
							{
								g_last_log = l_log;
							}
							LogManager::torrent_message(l_log);
							l_pos++;
							DownloadArray l_tickList;
							{
								TransferData l_td("");
								l_td.init(s);
								l_tickList.push_back(l_td);
								/*                      for (int i = 0; i < l_count_files; ++i)
								{
								const std::string l_file_name = ti->files().file_name(i).to_string();
								const std::string l_file_path = ti->files().file_path(i);
								TransferData l_td(");
								l_td.init(s);
								l_td.m_size = ti->files().file_size(i); // s.total_wanted; - это полный размер торрента
								l_td.log_debug();
								l_tickList.push_back(l_td);
								}
								*/
							}
							if (!l_tickList.empty())
							{
								fly_fire1(DownloadManagerListener::TorrentEvent(), l_tickList);
							}
						}
					}
				}
				catch (const system_error& e)
				{
					const std::string l_error = "[system_error-1] DownloadManager::onTorrentAlertNotify " + std::string(e.what());
					CFlyServerJSON::pushError(75, l_error);
				}
				catch (const std::runtime_error& e)
				{
					const std::string l_error = "[runtime_error-1] DownloadManager::onTorrentAlertNotify " + std::string(e.what());
					CFlyServerJSON::pushError(75, l_error);
				}
			}
		}
		                                       );
	}
	catch (const system_error& e)
	{
		const std::string l_error = "[system_error-2] DownloadManager::onTorrentAlertNotify " + std::string(e.what());
		CFlyServerJSON::pushError(75, l_error);
	}
	catch (const std::runtime_error& e)
	{
		const std::string l_error = "[runtime_error-2] DownloadManager::onTorrentAlertNotify " + std::string(e.what());
		CFlyServerJSON::pushError(75, l_error);
	}
}

int DownloadManager::listen_torrent_port()
{
	if (m_torrent_session)
	{
		return m_torrent_session->listen_port();
	}
	return 0;
}
bool DownloadManager::remove_torrent_file(const libtorrent::sha1_hash& p_sha1, const int p_delete_options)
{
	if (m_torrent_session)
	{
		lt::error_code ec;
		try
		{
			const auto l_h = m_torrent_session->find_torrent(p_sha1);
			//if (l_h.is_valid())
			{
				m_torrent_session->remove_torrent(l_h, p_delete_options);
			}
			//else
			//{
			//  LogManager::message("Error remove_torrent_file: sha1 = ");
			//}
		}
		catch (const std::runtime_error& e)
		{
			LogManager::message("Error remove_torrent_file: " + std::string(e.what()));
		}
	}
	return false;
}
bool DownloadManager::add_torrent_file(const tstring& p_torrent_path, const tstring& p_torrent_url)
{
	if (!m_torrent_session)
	{
		DownloadManager::getInstance()->init_torrent(true);
	}
	if (m_torrent_session)
	{
		lt::error_code ec;
		lt::add_torrent_params p;
		p.save_path = SETTING(DOWNLOAD_DIRECTORY);
		p.storage_mode = storage_mode_sparse;
		if (!p_torrent_path.empty())
		{
			p.ti = std::make_shared<torrent_info>(Text::fromT(p_torrent_path), std::ref(ec), 0);
			// for .torrent
			//p.flags |= lt::add_torrent_params::flag_paused;
			//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
			//p.flags &= ~lt::add_torrent_params::flag_duplicate_is_error;
		}
		else
		{
			p.url = Text::fromT(p_torrent_url);
			//p.flags &= ~lt::add_torrent_params::flag_paused;
			//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
			//p.flags |= lt::add_torrent_params::flag_upload_mode;
		}
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
			LogManager::message("Error add_torrent_file:" + ec.message());
			return false;
		}
		
		m_torrent_session->add_torrent(p, ec);
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
			LogManager::message("Error add_torrent_file:" + ec.message());
			return false;
		}
		return true;
	}
	return false;
}
void DownloadManager::init_torrent(bool p_is_force)
{
	if (!BOOLSETTING(USE_DHT) && p_is_force == false)
	{
		LogManager::message("Disable torrent DHT...");
		return;
	}
	try
	{
		m_torrent_resume_count = 0;
		lt::settings_pack l_sett;
		l_sett.set_int(lt::settings_pack::alert_mask
		               , lt::alert::error_notification
		               | lt::alert::storage_notification
		               | lt::alert::status_notification
		               | lt::alert::port_mapping_notification
		               //////                  | lt::alert::all_categories
		               | lt::alert::progress_notification
		               //  | lt::alert::peer_notification
		              );
		l_sett.set_str(settings_pack::user_agent, "FlylinkDC++ " A_REVISION_NUM_STR); // LIBTORRENT_VERSION //  A_VERSION_NUM_STR
		l_sett.set_int(settings_pack::choking_algorithm, settings_pack::rate_based_choker);
		// l_sett.set_int(settings_pack::active_loaded_limit, 50);
		l_sett.set_bool(settings_pack::enable_upnp, true);
		l_sett.set_bool(settings_pack::enable_natpmp, true);
		l_sett.set_bool(settings_pack::enable_lsd, true);
		l_sett.set_bool(settings_pack::enable_dht, true);
		l_sett.set_str(settings_pack::listen_interfaces, "0.0.0.0:8999");
		std::string l_dht_nodes;
for (const auto & j : CFlyServerConfig::getTorrentDHTServer())
		{
			if (!l_dht_nodes.empty())
				l_dht_nodes += ",";
			const auto l_boot_dht = j;
			LogManager::message("Add torrent DHT router: " + l_boot_dht.getServerAndPort());
			l_dht_nodes += l_boot_dht.getIp() + ':' + Util::toString(l_boot_dht.getPort());
		}
		l_sett.set_str(settings_pack::dht_bootstrap_nodes, l_dht_nodes);
		
		m_torrent_session = std::make_unique<lt::session>(l_sett);
		m_torrent_session->set_alert_notify(std::bind(&DownloadManager::onTorrentAlertNotify, this, m_torrent_session.get()));
		lt::dht_settings dht;
		m_torrent_session->set_dht_settings(dht);
		
#ifdef _DEBUG
		lt::error_code ec;
		lt::add_torrent_params p;
		p.save_path = SETTING(DOWNLOAD_DIRECTORY); // "."
		p.storage_mode = storage_mode_sparse; //
		//p.ti = std::make_shared<torrent_info>(std::string("D:\\1.torrent"), std::ref(ec), 0); // Strain - 4 серии!
		//p.url = "magnet:?xt=urn:btih:519b3ea00fe118a1cc0b2c6b90d62c6f5ff204c0&dn=rutor.info_%D0%98%D0%BB%D0%BB%D1%8E%D0%B7%D0%B8%D1%8F+%D0%BE%D0%B1%D0%BC%D0%B0%D0%BD%D0%B0+2+%2F+Now+You+See+Me+2+%282016%29+BDRip+%D0%BE%D1%82+GeneralFilm+%7C+RUS+Transfer+%7C+%D0%9B%D0%B8%D1%86%D0%B5%D0%BD%D0%B7%D0%B8%D1%8F&tr=udp://opentor.org:2710&tr=udp://opentor.org:2710&tr=http://retracker.local/announce";
		//p.url = "magnet:?xt=urn:btih:519b3ea00fe118a1cc0b2c6b90d62c6f5ff204c0&dn=rutor.info_%D0%98%D0%BB%D0%BB%D1%8E%D0%B7%D0%B8%D1%8F+%D0%BE%D0%B1%D0%BC%D0%B0%D0%BD%D0%B0+2+%2F+Now+You+See+Me+2+%282016%29+BDRip+%D0%BE%D1%82+GeneralFilm+%7C+RUS+Transfer+%7C+%D0%9B%D0%B8%D1%86%D0%B5%D0%BD%D0%B7%D0%B8%D1%8F";
		//p.url = "magnet:?xt=urn:btih:b71555ecfd11c82c12ff1f94c7ec1c095b6deb63&dn=rutor.info_Stand+Up+SATRip";
		//p.url = "magnet:?xt=urn:btih:0099d4cbf3b027f8f6e0d9bac7c57a16a56f7458&dn=rutor.info_Lake+Of+Tears+-+By+The+Black+Sea+%282014%29+MP3&tr=udp://opentor.org:2710&tr=udp://opentor.org:2710&tr=http://retracker.local/announce";
		//p.url = "magnet:?xt=urn:btih:9017152afa06f5964c88faad524a6d443fdea787";
		// проблемный урл - двоится
		// http://kino-tor.org/torrent/526186/devjataja-zhizn-lui-draksa_the-9th-life-of-louis-drax-2016-web-dlrip-itunes
		//p.url = "magnet:?xt=urn:btih:c5e31d0c60206be872f6d51cf7f0b64ff83806ff&dn=rutor.info_%D0%94%D0%B5%D0%B2%D1%8F%D1%82%D0%B0%D1%8F+%D0%B6%D0%B8%D0%B7%D0%BD%D1%8C+%D0%9B%D1%83%D0%B8+%D0%94%D1%80%D0%B0%D0%BA%D1%81%D0%B0+%2F+The+9th+Life+of+Louis+Drax+%282016%29+WEB-DLRip+%7C+iTunes&tr=udp://opentor.org:2710&tr=udp://opentor.org:2710&tr=http://retracker.local/announce";
		// Урл размножается в передаче!
		//p.url = "magnet:?xt=urn:btih:a012fbbffb23df3b9f1bda4080c5ca9a09aad898&dn=rutor.info_%D0%A4%D0%BB%D1%8D%D1%88+%2F+The+Flash+%5B03%D1%8501+%D0%B8%D0%B7+23%5D+%282016%29+WEBRip+%7C+Sunshine+Studio&tr=udp://opentor.org:2710&tr=udp://opentor.org:2710&tr=http://retracker.local/announce";
		//p.url = "magnet:?xt=urn:btih:cb1ca015295aa892e8408fdbacdbe7b0b490e0d4&dn=rutor.info_Kuedo+-+Slow+Knife+%282016%29+FLAC&tr=udp://opentor.org:2710&tr=udp://opentor.org:2710&tr=http://retracker.local/announce";
		//p.url = "magnet:?xt=urn:btih:976774f4b1a6f20636a93df3c7d6a9e307381240&dn=rutor.info_%D0%A4%D0%BB%D1%8D%D1%88+%2F+The+Flash+%5B03%D1%8501-02+%D0%B8%D0%B7+23%5D+%282016%29+WEB-DLRip+%D0%BE%D1%82+qqss44+%7C+LostFilm&tr=udp://opentor.org:2710&tr=udp://opentor.org:2710&tr=http://retracker.local/announce";
		
		p.url = "magnet:?xt=urn:btih:a70a398daf1f3090b2a49aec3e7dde5cfcecd462";
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
			//return 1;
		}
		// for magnet + load metadata
		p.flags &= ~lt::add_torrent_params::flag_paused;
		p.flags &= ~lt::add_torrent_params::flag_auto_managed;
		p.flags |= lt::add_torrent_params::flag_upload_mode;
		
		// for .torrent
		//p.flags |= lt::add_torrent_params::flag_paused;
		//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
		//p.flags &= ~lt::add_torrent_params::flag_duplicate_is_error;
		
		/*  TODO    if (skipChecking)
		            p.flags |= lt::add_torrent_params::flag_seed_mode;
		        else
		            p.flags &= ~lt::add_torrent_params::flag_seed_mode;
		*/
		// m_torrent_session->add_torrent(p, ec);
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
		}
#endif
		CFlylinkDBManager::getInstance()->load_torrent_resume(*m_torrent_session);
	}
	catch (const std::exception& e)
	{
		CFlyServerJSON::pushError(72, "DownloadManager::init_torrent error: " + std::string(e.what()));
	}
}
#endif


/**
 * @file
 * $Id: DownloadManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
