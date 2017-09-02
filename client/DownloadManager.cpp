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
#include "MappingManager.h"
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
#include "libtorrent/write_resume_data.hpp"
#include "libtorrent/magnet_uri.hpp"
#endif



std::unique_ptr<webrtc::RWLockWrapper> DownloadManager::g_csDownload = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
DownloadMap DownloadManager::g_download_map;
IdlersMap DownloadManager::g_idlers;
int64_t DownloadManager::g_runningAverage;

DownloadManager::DownloadManager()
{
	TimerManager::getInstance()->addListener(this);
}

static int g_outstanding_resume_data;

void DownloadManager::shutdown_torrent()
{
	if (m_torrent_session)
	{
		/*
		std::vector<lt::torrent_handle> handles = m_torrent_session->get_torrents();
		m_torrent_session->pause();
		while (!m_torrent_session->is_paused())
		{
		 Sleep(10);
		}
		g_outstanding_resume_data = 0;
		for (lt::torrent_handle i : handles)
		{
		 lt::torrent_handle& h = i;
		 if (!h.is_valid()) continue;
		 lt::torrent_status s = h.status();
		 if (!s.has_metadata) continue;
		 if (!h.need_save_resume_data()) continue;
		
		 h.save_resume_data();
		 ++g_outstanding_resume_data;
		}
		
		while (g_outstanding_resume_data > 0)
		{
		 auto const* a = m_torrent_session->wait_for_alert(lt::seconds(10));
		
		 // if we don't get an alert within 10 seconds, abort
		 if (a == 0)
		 {
		     dcassert(0);
		     break;
		 }
		
		 std::vector<lt::alert*> alerts;
		 m_torrent_session->pop_alerts(&alerts);
		
		 for (auto i : alerts)
		 {
		     if (lt::alert_cast<lt::save_resume_data_failed_alert>(a))
		     {
		         //process_alert(a);
		         --g_outstanding_resume_data;
		         continue;
		     }
		
		     auto const* rd = lt::alert_cast<lt::save_resume_data_alert>(a);
		     if (rd == nullptr)
		     {
		         //process_alert(a);
		         continue;
		     }
		
		     auto const l_resume = lt::write_resume_data_buf(rd->params);
		     const lt::torrent_status st = rd->handle.status(lt::torrent_handle::query_name);
		     CFlylinkDBManager::getInstance()->save_torrent_resume(rd->handle.info_hash(), st.name, l_resume);
		     --g_outstanding_resume_data;
		 }
		}
		*/
		
		m_torrent_session->pause();
		while (!m_torrent_session->is_paused())
		{
			Sleep(10);
		}
		for (auto i : m_torrents)
		{
			i.save_resume_data(); // libtorrent::torrent_handle::save_info_dict | libtorrent::torrent_handle::only_if_modified
			++m_torrent_resume_count;
		}
		int l_count = 40; // 4 сек
		while (m_torrent_resume_count > 0 && l_count-- > 0)
		{
			Sleep(100);
		}
		dcassert(m_torrent_resume_count == 0);
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
	
	std::string errorMessage;
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
		dcassert(0);
		std::string l_error = "catch (...) Error new BufferedOutputStream<true> l_buf_size (Mb) = " + Util::toString(l_buf_size / 1024 / 1024) + " email: ppa74@ya.ru";
		LogManager::message(l_error);
		d->reset_download_file();
		//failDownload(aSource, l_error);
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
		// d->resetPos(); // is there a better way than resetting the position?
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

void DownloadManager::select_files(const libtorrent::torrent_handle& p_torrent_handle)
{
	CFlyTorrentFileArray l_files;
	if (p_torrent_handle.is_valid())
	{
		auto l_file = p_torrent_handle.torrent_file();
		l_files.reserve(l_file->num_files());
		//const std::string torrentName = p_torrent_handle.status(torrent_handle::query_name).name;
		const file_storage fileStorage = l_file->files();
		for (int i = 0; i < fileStorage.num_files(); i++)
		{
			p_torrent_handle.file_priority(file_index_t(i), 0);
			CFlyTorrentFile l_item;
			l_item.m_file_path = fileStorage.file_path(file_index_t(i));
			l_item.m_size = fileStorage.file_size(file_index_t(i));
			l_files.push_back(l_item);
#ifdef _DEBUG
			LogManager::torrent_message("metadata_received_alert: File = " + l_item.m_file_path);
#endif
		}
		p_torrent_handle.unset_flags(lt::torrent_flags::auto_managed);
		p_torrent_handle.pause();
		fly_fire1(DownloadManagerListener::SelectTorrent(), l_file->info_hash(), l_files);
	}
}
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
					if (const auto l_port = lt::alert_cast<lt::log_alert>(a))
					{
						//LogManager::torrent_message("log_alert: " + a->message() + " info:" + std::string(a->what()));
					}
					
#endif
					if (const auto l_ext_ip = lt::alert_cast<lt::external_ip_alert>(a))
					{
						MappingManager::setExternaIP(l_ext_ip->external_address.to_string());
					}
					if (const auto l_port = lt::alert_cast<lt::portmap_alert>(a))
					{
						LogManager::torrent_message("portmap_alert: " + a->message() + " info:" + std::string(a->what()) + " index = " + Util::toString(l_port->mapping));
						if (l_port->mapping == m_maping_index[0])
							SettingsManager::g_upnpTCPLevel = true;
						if (l_port->mapping == m_maping_index[2])
							SettingsManager::g_upnpUDPSearchLevel = true;
						if (l_port->mapping == m_maping_index[1])
							SettingsManager::g_upnpTLSLevel = true;
						if (l_port->mapping == 0)
							SettingsManager::g_upnpTorrentLevel = true;
					}
					
					if (const auto l_port = lt::alert_cast<lt::portmap_error_alert>(a))
					{
						dcassert(0);
						LogManager::torrent_message("portmap_error_alert: " + a->message() + " info:" + std::string(a->what()) + " index = " + Util::toString(l_port->mapping));
						if (l_port->mapping == m_maping_index[0])
							SettingsManager::g_upnpTCPLevel = false;
						if (l_port->mapping == m_maping_index[2])
							SettingsManager::g_upnpUDPSearchLevel = false;
						if (l_port->mapping == m_maping_index[1])
							SettingsManager::g_upnpTLSLevel = false;
						if (l_port->mapping == 0)
							SettingsManager::g_upnpTorrentLevel = false;
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_removed_alert>(a))
					{
						LogManager::torrent_message("torrent_removed_alert: " + a->message());
						auto const l_sha1 = l_delete->info_hash;
						auto i = m_torrents.find(l_delete->handle);
						if (i == m_torrents.end())
						{
							for (i = m_torrents.cbegin(); i != m_torrents.cend(); ++i)
							{
								if (!i->is_valid())
									continue;
								if (i->info_hash() == l_sha1)
									break;
							}
						}
						else
						{
							m_torrents.erase(i);
						}
						--m_torrent_resume_count;
						CFlylinkDBManager::getInstance()->delete_torrent_resume(l_sha1);
						LogManager::torrent_message("CFlylinkDBManager::getInstance()->delete_torrent_resume(l_sha1): " + l_delete->handle.info_hash().to_string());
						fly_fire1(DownloadManagerListener::RemoveTorrent(), l_sha1);
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_delete_failed_alert>(a))
					{
						LogManager::torrent_message("torrent_delete_failed_alert: " + a->message());
					}
					if (const auto l_delete = lt::alert_cast<lt::torrent_deleted_alert>(a))
					{
						LogManager::torrent_message("torrent_deleted_alert: " + a->message());
						//fly_fire1(DownloadManagerListener::RemoveTorrent(), l_delete->info_hash);
					}
					if (const auto l_rename = lt::alert_cast<lt::file_renamed_alert>(a))
					{
						LogManager::torrent_message("file_renamed_alert: " + l_rename->message(), false);
						if (!--m_torrent_rename_count)
						{
							if (!CFlylinkDBManager::is_delete_torrent(l_rename->handle.info_hash()))
							{
								l_rename->handle.save_resume_data();
								++m_torrent_resume_count;
							}
							else
							{
								LogManager::torrent_message("file_renamed_alert: skip save_resume_data - is_delete_torrent!");
							}
						}
					}
					if (const auto l_rename = lt::alert_cast<lt::file_rename_failed_alert>(a))
					{
						LogManager::torrent_message("file_rename_failed_alert: " + a->message() +
						                            " error = " + Util::toString(l_rename->error.value()), true);
						if (!--m_torrent_rename_count)
						{
							l_rename->handle.save_resume_data();
							++m_torrent_resume_count;
						}
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
					if (auto l_metadata = lt::alert_cast<metadata_received_alert>(a))
					{
						if (!CFlylinkDBManager::is_resume_torrent(l_metadata->handle.info_hash()))
						{
							select_files(l_metadata->handle);
						}
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
						const auto l_file_name = l_files.file_name(l_a->index).to_string();
						//const auto l_file_path = l_files.file_path(l_a->index);
						auto l_full_file_path = SETTING(DOWNLOAD_DIRECTORY) + l_files.file_path(l_a->index);
						auto const l_is_tmp = l_full_file_path.find_last_of(".!fl");
						if (l_is_tmp != std::string::npos && l_is_tmp > 3)
						{
							const auto l_new_name = l_full_file_path.substr(0, l_is_tmp - 3);
							++m_torrent_rename_count;
							l_a->handle.rename_file(l_a->index, l_new_name);
						}
						
						// TODO заменить SETTING(DOWNLOAD_DIRECTORY) на путь выбора
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
						LogManager::torrent_message("file_completed_alert: " + a->message() + " Path:" + l_full_file_path +
						                            +" sha1:" + aux::to_hex(l_sha1));
						auto l_item = std::make_shared<FinishedItem>(l_full_file_path, l_sha1, l_size, 0, GET_TIME(), 0, 0);
						CFlylinkDBManager::getInstance()->save_transfer_history(true, e_TransferDownload, l_item);
						
						if (FinishedManager::isValidInstance())
						{
							FinishedManager::getInstance()->pushHistoryFinishedItem(l_item, e_TransferDownload);
							FinishedManager::getInstance()->updateStatus();
						}
						
						const CFlyTTHKey l_file(l_sha1, l_size, p_is_sha1_for_file);
						CFlyServerJSON::addDownloadCounter(l_file, l_file_name);
#ifdef _DEBUG
						CFlyServerJSON::sendDownloadCounter(false);
#endif
						fly_fire1(DownloadManagerListener::CompleteTorrentFile(), l_full_file_path);
					}
					if (const auto l_a = lt::alert_cast<lt::add_torrent_alert>(a))
					{
						auto l_name = l_a->handle.status(torrent_handle::query_name);
						LogManager::torrent_message("Add torrent: " + l_name.name);
						m_torrents.insert(l_a->handle);
						if (l_name.has_metadata)
						{
							if (!CFlylinkDBManager::is_resume_torrent(l_a->handle.info_hash()))
							{
								select_files(l_a->handle);
							}
							else
							{
#ifdef _DEBUG
								LogManager::torrent_message("CFlylinkDBManager::is_resume_torrent: sha1 = " + lt::aux::to_hex(l_a->handle.info_hash()));
#endif
							}
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
#ifdef _DEBUG
						LogManager::torrent_message("CFlylinkDBManager::delete_torrent_resume: sha1 = " + lt::aux::to_hex(l_a->handle.info_hash()));
#endif
					}
					
					if (const auto l_a = lt::alert_cast<save_resume_data_failed_alert>(a))
					{
						dcassert(m_torrent_resume_count > 0);
						--m_torrent_resume_count;
						LogManager::torrent_message("save_resume_data_failed_alert: " + l_a->message() + " info:" + std::string(a->what()), true);
					}
					if (const auto l_a = lt::alert_cast<save_resume_data_alert>(a))
					{
						auto const l_resume = lt::write_resume_data_buf(l_a->params);
						const torrent_status st = l_a->handle.status(torrent_handle::query_name);
						dcassert(st.info_hash == l_a->handle.info_hash());
						CFlylinkDBManager::getInstance()->save_torrent_resume(l_a->handle.info_hash(), st.name, l_resume);
						--m_torrent_resume_count;
						LogManager::torrent_message("save_resume_data_alert: " + l_a->message(), false);
						// TODO l_a->handle.set_pinned(false);
					}
					if (lt::alert_cast<lt::torrent_error_alert>(a))
					{
						LogManager::torrent_message("torrent_error_alert: " + a->message() + " info:" + std::string(a->what()));
					}
					if (lt::alert_cast<lt::file_error_alert>(a))
					{
						LogManager::torrent_message("file_error_alert: " + a->message() + " info:" + std::string(a->what()));
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
#ifdef FLYLINKDC_BETA
							/*
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
							                            LogManager::torrent_message(l_log, false);
							*/
							
#endif
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
std::string DownloadManager::get_torrent_name(const libtorrent::sha1_hash& p_sha1)
{
	if (m_torrent_session)
	{
		const torrent_handle h = m_torrent_session->find_torrent(p_sha1);
		if (h.is_valid())
		{
			const torrent_status st = h.status(torrent_handle::query_name | torrent_handle::query_save_path); // );
			return st.save_path + '\\' + st.name;
		}
	}
	return "";
}
void DownloadManager::fire_added_torrent(const libtorrent::sha1_hash& p_sha1)
{
	if (m_torrent_session)
	{
		fly_fire2(DownloadManagerListener::AddedTorrent(), p_sha1, get_torrent_name(p_sha1)); // TODO opt
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
bool DownloadManager::set_file_priority(const libtorrent::sha1_hash& p_sha1, const CFlyTorrentFileArray& p_files, const std::vector<int>& p_file_priority)
{
	if (m_torrent_session)
	{
		lt::error_code ec;
		try
		{
			const auto l_h = m_torrent_session->find_torrent(p_sha1);
			if (l_h.is_valid())
			{
				l_h.prioritize_files(p_file_priority);
				for (int i = 0; i < p_files.size(); i++)
				{
					++m_torrent_rename_count;
					l_h.rename_file(file_index_t(i), p_files[i].m_file_path + ".!fl");
				}
				
				l_h.set_flags(lt::torrent_flags::auto_managed);
				l_h.resume();
				// l_h.save_resume_data(); // torrent_handle::save_info_dict | torrent_handle::only_if_modified);
				// Сохраняем на диск когда все переименуется.
				//++m_torrent_resume_count;
			}
		}
		catch (const std::runtime_error& e)
		{
			LogManager::message("Error set_file_priority: " + std::string(e.what()));
		}
	}
	return false;
	
}
bool DownloadManager::pause_torrent_file(const libtorrent::sha1_hash& p_sha1, bool p_is_resume)
{
	if (m_torrent_session)
	{
		lt::error_code ec;
		try
		{
			const auto l_h = m_torrent_session->find_torrent(p_sha1);
			if (l_h.is_valid())
			{
				if (p_is_resume)
				{
					l_h.set_flags(lt::torrent_flags::auto_managed);
					l_h.resume();
				}
				else
				{
					l_h.unset_flags(lt::torrent_flags::auto_managed);
					l_h.pause();
				}
			}
			return true;
		}
		catch (const std::runtime_error& e)
		{
			LogManager::torrent_message("Error pause_torrent_file: " + std::string(e.what()));
		}
	}
	return false;
	
}
bool DownloadManager::remove_torrent_file(const libtorrent::sha1_hash& p_sha1, libtorrent::remove_flags_t p_options)
{
	if (m_torrent_session)
	{
		lt::error_code ec;
		try
		{
			const auto l_h = m_torrent_session->find_torrent(p_sha1);
			//if (l_h.is_valid())
			{
				m_torrent_session->remove_torrent(l_h, p_options);
			}
			//else
			//{
			//  LogManager::torrent_message("Error remove_torrent_file: sha1 = " + lt::aux::to_hex());
			//}
			return true;
		}
		catch (const std::runtime_error& e)
		{
			LogManager::torrent_message("Error remove_torrent_file: " + std::string(e.what()));
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
//		auto renamed_files = p.renamed_files; // ".!fl" ?
		p.storage_mode = storage_mode_sparse;
		if (!p_torrent_path.empty())
		{
			p.ti = std::make_shared<torrent_info>(Text::fromT(p_torrent_path), std::ref(ec));
			// for .torrent
			//p.flags |= lt::add_torrent_params::flag_paused;
			//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
			//p.flags &= ~lt::add_torrent_params::flag_duplicate_is_error;
		}
		else
		{
			lt::parse_magnet_uri(Text::fromT(p_torrent_url), p, ec);
			if (ec)
			{
				dcassert(0);
				LogManager::torrent_message("Error parse_magnet_uri:" + ec.message());
				return false;
			}
			//p.flags &= ~lt::add_torrent_params::flag_paused;
			//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
			//p.flags |= lt::add_torrent_params::flag_upload_mode;
		}
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
			LogManager::torrent_message("Error add_torrent_file:" + ec.message());
			return false;
		}
		
		m_torrent_session->add_torrent(p, ec);
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
			LogManager::torrent_message("Error add_torrent_file:" + ec.message());
			return false;
		}
		return true;
	}
	return false;
}
void DownloadManager::init_torrent(bool p_is_force)
{
	/*
	if (!BOOLSETTING(USE_TORRENT) && p_is_force == false)
	    {
	        LogManager::torrent_message("Disable torrent DHT...");
	        return;
	    }
	*/
	try
	{
		m_torrent_resume_count = 0;
		m_torrent_rename_count = 0;
		lt::settings_pack l_sett;
		l_sett.set_int(lt::settings_pack::alert_mask
		               , lt::alert::error_notification
		               | lt::alert::storage_notification
		               | lt::alert::status_notification
		               | lt::alert::port_mapping_notification
		               //////                  | lt::alert::all_categories
		               | lt::alert::progress_notification
#ifdef _DEBUG
		               //  | lt::alert::peer_notification
		               | lt::alert::session_log_notification
		               | lt::alert::torrent_log_notification
		               | lt::alert::peer_log_notification
#endif
		              );
		l_sett.set_str(settings_pack::user_agent, "FlylinkDC++ " A_REVISION_NUM_STR); // LIBTORRENT_VERSION //  A_VERSION_NUM_STR
		l_sett.set_int(settings_pack::choking_algorithm, settings_pack::rate_based_choker);
		
		l_sett.set_int(settings_pack::active_downloads, -1);
		l_sett.set_int(settings_pack::active_seeds, -1);
		l_sett.set_int(settings_pack::active_limit, -1);
		l_sett.set_int(settings_pack::active_tracker_limit, -1);
		l_sett.set_int(settings_pack::active_dht_limit, -1);
		l_sett.set_int(settings_pack::active_lsd_limit, -1);
		
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
			LogManager::torrent_message("Add torrent DHT router: " + l_boot_dht.getServerAndPort());
			l_dht_nodes += l_boot_dht.getIp() + ':' + Util::toString(l_boot_dht.getPort());
		}
		l_sett.set_str(settings_pack::dht_bootstrap_nodes, l_dht_nodes);
		
		m_torrent_session = std::make_unique<lt::session>(l_sett);
		m_torrent_session->set_alert_notify(std::bind(&DownloadManager::onTorrentAlertNotify, this, m_torrent_session.get()));
		//lt::dht_settings dht;
		//m_torrent_session->set_dht_settings(dht);
		if (SETTING(INCOMING_CONNECTIONS) == SettingsManager::INCOMING_FIREWALL_UPNP || BOOLSETTING(AUTO_DETECT_CONNECTION))
		{
			m_maping_index[0] = m_torrent_session->add_port_mapping(lt::session::tcp, SETTING(TCP_PORT), SETTING(TCP_PORT));
			m_maping_index[1] = m_torrent_session->add_port_mapping(lt::session::tcp, SETTING(TLS_PORT), SETTING(TLS_PORT));
			m_maping_index[2] = m_torrent_session->add_port_mapping(lt::session::udp, SETTING(UDP_PORT), SETTING(UDP_PORT));
		}
		else
		{
			m_maping_index[0] = -1;
			m_maping_index[1] = -1;
			m_maping_index[2] = -1;
		}
		
#ifdef _DEBUG
		lt::error_code ec;
		lt::add_torrent_params model;
		model.save_path = SETTING(DOWNLOAD_DIRECTORY); // "."
		model.storage_mode = storage_mode_sparse; //
		lt::add_torrent_params p = model;
		lt::parse_magnet_uri("magnet:?xt=urn:btih:df38ab92ca37c136bcdd7a6ff2aa8a644fc78a00", p, ec);
		if (ec)
		{
			dcdebug("%s\n", ec.message().c_str());
			dcassert(0);
			//return 1;
		}
		// for magnet + load metadata
		//p.flags |= lt::add_torrent_params::flag_paused;
		//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
		//p.flags |= lt::add_torrent_params::flag_upload_mode;
		
		//p.flags |= lt::add_torrent_params::flag_paused;
		p.flags |= lt::torrent_flags::auto_managed;
		p.flags |= lt::torrent_flags::duplicate_is_error;
		// TODO p.flags |= lt::add_torrent_params::flag_update_subscribe;
		
		// TODO p.flags |= lt::add_torrent_params::flag_sequential_download;
		
		// for .torrent
		//p.flags |= lt::add_torrent_params::flag_paused;
		//p.flags &= ~lt::add_torrent_params::flag_auto_managed;
		//p.flags &= ~lt::add_torrent_params::flag_duplicate_is_error;
		
		/*  TODO    if (skipChecking)
		            p.flags |= lt::add_torrent_params::flag_seed_mode;
		        else
		            p.flags &= ~lt::add_torrent_params::flag_seed_mode;
		*/
		/*
		        auto l_h = m_torrent_session->add_torrent(p, ec);
		        if (ec)
		        {
		            dcdebug("%s\n", ec.message().c_str());
		            dcassert(0);
		            return;
		        }
		*/
		/*
		while (!l_h.status().has_metadata)
		        {
		            ::Sleep(100);
		        }
		
		        l_h.unset_flags(lt::torrent_flags::auto_managed);;
		        l_h.pause();
		*/
		
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
