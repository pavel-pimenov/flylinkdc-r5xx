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
#include <bzlib.h>

#include "UploadManager.h"
#include "DownloadManager.h"
#include "ConnectionManager.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "FinishedManager.h"
#include "PGLoader.h"
#include "IPGrant.h"
#include "../FlyFeatures/flyServer.h"
#include "SharedFileStream.h"

STANDARD_EXCEPTION(BZ2Exception);

uint32_t UploadManager::g_count_WaitingUsersFrame = 0;
UploadManager::SlotMap UploadManager::g_reservedSlots;
bool UploadManager::g_is_reservedSlotEmpty = true;
int UploadManager::g_running = 0;
UploadList UploadManager::g_uploads;
UploadList UploadManager::g_delayUploads;
CurrentConnectionMap UploadManager::g_uploadsPerUser;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
UploadManager::BanMap UploadManager::g_lastBans;
std::unique_ptr<webrtc::RWLockWrapper> UploadManager::g_csBans = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
#endif
std::unique_ptr<webrtc::RWLockWrapper> UploadManager::g_csUploadsDelay = std::unique_ptr<webrtc::RWLockWrapper>(webrtc::RWLockWrapper::CreateRWLock());
std::unique_ptr<webrtc::RWLockWrapper> UploadManager::g_csReservedSlots = std::unique_ptr<webrtc::RWLockWrapper> (webrtc::RWLockWrapper::CreateRWLock());
int64_t UploadManager::g_runningAverage;

UploadManager::UploadManager() noexcept :
	extra(0), lastGrant(0), m_lastFreeSlots(-1),
	m_fireballStartTick(0), isFireball(false), isFileServer(false), extraPartial(0)
{
	ClientManager::getInstance()->addListener(this);
	TimerManager::getInstance()->addListener(this);
}

UploadManager::~UploadManager()
{
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	{
		CFlyLock(m_csQueue);
		m_slotQueue.clear(); // TODO - ������ �������� ������ � ����� shutdown
	}
	while (true)
	{
		{
			CFlyReadLock(*g_csUploadsDelay);
			if (g_uploads.empty())
				break;
		}
		Thread::sleep(10);
	}
	//SharedFileStream::check_before_destoy();
	dcassert(g_uploadsPerUser.empty());
}

#ifdef IRAINMAN_ENABLE_AUTO_BAN
bool UploadManager::handleBan(UserConnection* aSource/*, bool forceBan, bool noChecks*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	const UserPtr& user = aSource->getUser();
	if (!user->isOnline()) // if not online, cheat (connection without hub)
	{
		aSource->disconnect();
		return true;
	}
	bool l_is_ban = false;
	const bool l_is_favorite = FavoriteManager::isFavoriteUser(user, l_is_ban);
	const auto banType = user->hasAutoBan(nullptr, l_is_favorite);
	bool banByRules = banType != User::BAN_NONE;
	if (banByRules)
	{
		FavoriteHubEntry* hub = FavoriteManager::getFavoriteHubEntry(aSource->getHubUrl());
		if (hub && hub->getExclChecks())
		{
			banByRules = false;
		}
	}
	if (/*!forceBan &&*/ (/*noChecks ||*/ !banByRules)) return false;
	
	banmsg_t msg;
	msg.share = (int)(user->getBytesShared() / (1024 * 1024 * 1024));
	msg.slots = user->getSlots();
	msg.limit = user->getLimit();
	msg.min_share = SETTING(BAN_SHARE);
	msg.min_slots = SETTING(BAN_SLOTS);
	msg.max_slots = SETTING(BAN_SLOTS_H);
	msg.min_limit = SETTING(BAN_LIMIT);
	
	/*
	[-] brain-ripper
	follow rules are ignored!
	comments saved only for history
	//2) "�������" �� ���� �������� ���������� �������� "���� ����" (�� �� ����� 20 ���)
	//3) "�������" �� ������ �������� ���������� �������� "������� ������ � ����"/2 (�� �� ����� 15 ������)
	//4) "�������" �� ��������� �������� ���������� �������� "�������� � ����"/2 (�� �� ���� 60 ��/���)
	*/
	// BAN for Slots:[1/5] Share:[17/200]Gb Limit:[30/50]kb/s Msg: share more files, please!
	string banstr;
	banstr = "BAN for";
	
	if (banType & User::BAN_BY_MIN_SLOT)
		banstr += " Slots < "  + Util::toString(msg.min_slots);
	if (banType & User::BAN_BY_MAX_SLOT)
		banstr += " Slots > "  + Util::toString(msg.max_slots);
	if (banType & User::BAN_BY_SHARE)
		banstr += " Share < "  + Util::toString(msg.min_share) + " Gb";
	if (banType & User::BAN_BY_LIMIT)
		banstr += " Limit < "  + Util::toString(msg.min_limit) + " kB/s";
		
	banstr += " Msg: " + SETTING(BAN_MESSAGE);
#ifdef _DEBUG
	{
		auto logline = banstr + " Log autoban:";
		
		if (banType & User::BAN_BY_MIN_SLOT)
			logline += " Slots:[" + Util::toString(user->getSlots()) + '/' + Util::toString(msg.min_slots) + '/' + Util::toString(msg.max_slots) + ']';
		if (banType & User::BAN_BY_SHARE)
			logline += " Share:[" + Util::toString(int(user->getBytesShared() / (1024 * 1024 * 1024))) + '/' + Util::toString(msg.min_share) + "]Gb";
		if (banType & User::BAN_BY_LIMIT)
			logline += " Limit:[" + Util::toString(user->getLimit()) + '/' + Util::toString(msg.min_limit) + "]kb/s";
			
		logline += " Msg: " + SETTING(BAN_MESSAGE);
		
		LogManager::message("User:" + user->getLastNick() + ' ' + logline);
	}
#endif
	// old const bool sendStatus = aSource->isSet(UserConnection::FLAG_SUPPORTS_BANMSG);
	
	if (!BOOLSETTING(BAN_STEALTH))
	{
		aSource->error(banstr);
		
		if (BOOLSETTING(BAN_FORCE_PM))
		{
		
			msg.tick = TimerManager::getTick();
			const auto pmMsgPeriod = SETTING(BANMSG_PERIOD);
			const auto key = user->getCID().toBase32();
			bool sendPm;
			{
				CFlyWriteLock(*g_csBans);
				auto t = g_lastBans.find(key);
				if (t == g_lastBans.end())
				{
					g_lastBans[key] = msg;
					sendPm = pmMsgPeriod != 0;
				}
				else if (!t->second.same(msg))
				{
					t->second = msg;
					sendPm = pmMsgPeriod != 0;
				}
				else if (pmMsgPeriod && (uint32_t)(msg.tick - t->second.tick) >= (uint32_t)pmMsgPeriod * 60 * 1000)
				{
					t->second.tick = msg.tick;
					sendPm = true;
				}
				else
				{
					sendPm = false;
				}
			}
			if (sendPm)
			{
				ClientManager::privateMessage(aSource->getHintedUser(), banstr, false);
			}
			
		}
	}
	
//	if (BOOLSETTING(BAN_STEALTH)) aSource->maxedOut();

	return true;
}

bool UploadManager::isBanReply(const UserPtr& user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	const auto key = user->getCID().toBase32();
	{
		CFlyReadLock(*g_csBans);
		const auto t = g_lastBans.find(key);
		if (t != g_lastBans.end())
		{
			return (TimerManager::getTick() - t->second.tick) < 2000;
		}
	}
	return false;
}
#endif // IRAINMAN_ENABLE_AUTO_BAN
bool UploadManager::hasUpload(const UserConnection* p_newLeacher, const string& p_source_file) const
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (p_newLeacher->getSocket())
	{
		const auto& newLeacherIp = p_newLeacher->getSocket()->getIp();
		const auto& newLeacherShare = p_newLeacher->getUser()->getBytesShared();
		const auto& newLeacherNick = p_newLeacher->getUser()->getLastNick();
		
		CFlyReadLock(*g_csUploadsDelay);
		
		for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
		{
			const auto u = *i;
			dcassert(u);
			dcassert(u->getUser());
			const auto& uploadUserIp = u->getUserConnection()->getSocket()->getIp();
			const auto& uploadUserShare = u->getUser()->getBytesShared();
			const auto& uploadUserNick = u->getUser()->getLastNick();
			
			if (u->getUserConnection()->getSocket() &&
			        newLeacherIp == uploadUserIp &&
			        (newLeacherShare == uploadUserShare ||
			         newLeacherNick == uploadUserNick)
			   )
			{
#if defined(NIGHT_BUILD) || defined(_DEBUG) || defined(FLYLINKDC_BETA)
				char bufNewLeacherShare[64];
				bufNewLeacherShare[0] = 0;
				char bufUploadUserShare[64];
				bufUploadUserShare[0] = 0;
				char bufShareDelta[64];
				bufShareDelta[0] = 0;
				
				_snprintf(bufNewLeacherShare, sizeof(bufNewLeacherShare), "%I64d", newLeacherShare);
				_snprintf(bufUploadUserShare, sizeof(bufUploadUserShare), "%I64d", uploadUserShare);
				_snprintf(bufShareDelta, sizeof(bufShareDelta), "%I64d", int64_t(std::abs(long(newLeacherShare - uploadUserShare))));
#if 0
				LogManager::ddos_message("[Drop duplicated connection] From IP =" + newLeacherIp
				                         + ", share con = " + bufNewLeacherShare
				                         + ", share exist = " + string(bufUploadUserShare)
				                         + " delta = " + string(bufShareDelta)
				                         + " nick con = " + p_newLeacher->getUser()->getLastNick()
				                         + " nick exist = " + u->getUser()->getLastNick()
				                         + " source file = " + p_source_file
				                        );
#endif
#endif
				return true;
			}
		}
	}
	return false;
}

void UploadManager::decodeBZ2(const uint8_t* is, size_t sz, string& os) {
	bz_stream bs = { 0 };
	
	if (BZ2_bzDecompressInit(&bs, 0, 0) != BZ_OK)
		throw BZ2Exception(STRING(DECOMPRESSION_ERROR));
		
	// We assume that the files aren't compressed more than 2:1...if they are it'll work anyway,
	// but we'll have to do multiple passes...
	size_t bufsize = 2 * sz;
	std::unique_ptr <char[]> buf(new char[bufsize]);
	
	bs.avail_in = sz;
	bs.avail_out = bufsize;
	bs.next_in = reinterpret_cast<char*>(const_cast<uint8_t*>(is));
	bs.next_out = &buf[0];
	
	int err;
	
	os.clear();
	
	while ((err = BZ2_bzDecompress(&bs)) == BZ_OK) {
		if (bs.avail_in == 0 && bs.avail_out > 0) { // error: BZ_UNEXPECTED_EOF
			BZ2_bzDecompressEnd(&bs);
			throw BZ2Exception(STRING(DECOMPRESSION_ERROR));
		}
		os.append(&buf[0], bufsize - bs.avail_out);
		bs.avail_out = bufsize;
		bs.next_out = &buf[0];
	}
	
	if (err == BZ_STREAM_END)
		os.append(&buf[0], bufsize - bs.avail_out);
		
	BZ2_bzDecompressEnd(&bs);
	
	if (err < 0) {
		// This was a real error
		throw BZ2Exception(STRING(DECOMPRESSION_ERROR));
	}
}


bool UploadManager::prepareFile(UserConnection* aSource, const string& aType, const string& aFile, int64_t aStartPos, int64_t& aBytes, bool listRecursive)
{
	dcassert(!ClientManager::isBeforeShutdown());
	dcdebug("Preparing %s %s " I64_FMT " " I64_FMT " %d\n", aType.c_str(), aFile.c_str(), aStartPos, aBytes, listRecursive);
	if (ClientManager::isBeforeShutdown())
	{
		return false;
	}
	if (aFile.empty() || aStartPos < 0 || aBytes < -1 || aBytes == 0)
	{
		aSource->fileNotAvail("Invalid request");
		return false;
	}
	const auto l_ip = aSource->getRemoteIp();
	const auto l_chiper_name = aSource->getCipherName();
	const bool l_is_TypeTree = aType == Transfer::g_type_names[Transfer::TYPE_TREE];
	const bool l_is_TypePartialTree = aType == Transfer::g_type_names[Transfer::TYPE_PARTIAL_LIST];
#ifdef FLYLINKDC_USE_DOS_GUARD
	if (l_is_TypeTree || l_is_TypePartialTree) // && aFile == "TTH/HDWK5FVECXJDLTECQ6TY435WWEE7RU25RSQYYWY"
	{
		const HintedUser& l_User = aSource->getHintedUser();
		if (l_User.user)
		{
			const string l_hash_key = Util::toString(l_User.user->getCID().toHash()) + aFile;
			uint8_t l_count_dos;
			{
				CFlyFastLock(csDos);
				l_count_dos = ++m_dos_map[l_hash_key];
			}
			const uint8_t l_count_attempts = l_is_TypePartialTree ? 5 : 30;
			if (l_count_dos > l_count_attempts)
			{
				dcdebug("l_hash_key = %s ++m_dos_map[l_hash_key] = %d\n", l_hash_key.c_str(), l_count_dos);
				if (BOOLSETTING(LOG_DDOS_TRACE))
				{
					char l_buf[2000];
					l_buf[0] = 0;
					sprintf_s(l_buf, _countof(l_buf), CSTRING(DOS_ATACK),
					          l_User.user->getLastNick().c_str(),
					          l_User.hint.c_str(),
					          l_count_attempts,
					          aFile.c_str());
					LogManager::ddos_message(l_buf);
				}
#ifdef IRAINMAN_DISALLOWED_BAN_MSG
				if (aSource->isSet(UserConnection::FLAG_SUPPORTS_BANMSG))
				{
					aSource->error(UserConnection::g_PLEASE_UPDATE_YOUR_CLIENT);
				}
#endif
				if (l_is_TypePartialTree)
				{
					/*
					Fix
					[2017-10-01 16:20:21] <MikeKMV> ����������� ���� ����� �������
					[2017-10-01 16:20:25] <MikeKMV> Client: [Incoming][194.186.25.22]       $ADCGET list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 -1 ZL1
					Client: [Outgoing][194.186.25.22]       $ADCSND list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 2029 ZL1|
					Client: [Incoming][194.186.25.22]       $ADCGET list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 -1 ZL1
					Client: [Outgoing][194.186.25.22]       $ADCSND list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 2029 ZL1|
					Client: [Incoming][194.186.25.22]       $ADCGET list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 -1 ZL1
					Client: [Outgoing][194.186.25.22]       $ADCSND list /music/Vangelis/[1988]\ Vangelis\ -\ Direct/ 0 2029 ZL1|
					*/
					CFlyServerJSON::pushError(80, "Disconnect bug client: $ADCGET / $ADCSND User: " + l_User.user->getLastNick() + " Hub:" + aSource->getHubUrl());
					aSource->disconnect();
					
				}
				return false;
			}
		}
	}
#endif // FLYLINKDC_USE_DOS_GUARD
	InputStream* is = nullptr;
	int64_t start = 0;
	int64_t size = 0;
	int64_t fileSize = 0;
	
	const bool l_is_userlist = (aFile == Transfer::g_user_list_name_bz || aFile == Transfer::g_user_list_name);
	bool l_is_free = l_is_userlist;
	bool l_is_partial = false;
	
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	bool ishidingShare;
	if (aSource->getUser())
	{
		const FavoriteHubEntry* fhe = FavoriteManager::getFavoriteHubEntry(aSource->getHintedUser().hint);
		ishidingShare = fhe && fhe->getHideShare();
	}
	else
		ishidingShare = false;
#endif
		
	string sourceFile;
	const bool l_is_type_file = aType == Transfer::g_type_names[Transfer::TYPE_FILE];
	const bool l_is_tth  = l_is_type_file && aFile.size() == 43 && aFile.compare(0, 4, "TTH/", 4) == 0;
	Transfer::Type type;
	TTHValue l_tth;
	if (l_is_tth)
	{
		l_tth = TTHValue(aFile.c_str() + 4);
	}
	try
	{
		if (l_is_type_file && !ClientManager::isBeforeShutdown() && ShareManager::isValidInstance())
		{
			sourceFile = ShareManager::getInstance()->toReal(aFile
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
			                                                 , ishidingShare
#endif
			                                                );
			                                                
			if (aFile == Transfer::g_user_list_name)
			{
				// Unpack before sending...
				string bz2 = File(sourceFile, File::READ, File::OPEN).read();
				string xml;
				decodeBZ2(reinterpret_cast<const uint8_t*>(bz2.data()), bz2.size(), xml); // TODO - bzutils
				// Clear to save some memory...
				string().swap(bz2);
				is = new MemoryInputStream(xml);
				start = 0;
				fileSize = size = xml.size();
			}
			else
			{
				File* f = new File(sourceFile, File::READ, File::OPEN);
				
				start = aStartPos;
				int64_t sz = f->getSize();
				size = (aBytes == -1) ? sz - start : aBytes;
				fileSize = sz;
				
				if ((start + size) > sz)
				{
					aSource->fileNotAvail();
					delete f;
					return false;
				}
				
				l_is_free = l_is_free || (sz <= (int64_t)(SETTING(SET_MINISLOT_SIZE) * 1024));
				
				f->setPos(start);
				is = f;
				if ((start + size) < sz)
				{
					is = new LimitedInputStream<true>(is, size);
				}
			}
			type = l_is_userlist ? Transfer::TYPE_FULL_LIST : Transfer::TYPE_FILE;
		}
		else if (l_is_TypeTree)
		{
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
			if (ishidingShare)
			{
				aSource->fileNotAvail();
				return false;
			}
#endif
			sourceFile = aFile;
			MemoryInputStream* mis = ShareManager::getInstance()->getTree(aFile);
			if (!mis)
			{
				aSource->fileNotAvail();
				return false;
			}
			
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			l_is_free = true;
			type = Transfer::TYPE_TREE;
		}
		else if (l_is_TypePartialTree)
		{
			// Partial file list
			MemoryInputStream* mis = ShareManager::getInstance()->generatePartialList(aFile, listRecursive
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
			                                                                          , ishidingShare
#endif
			                                                                         );
			if (!mis)
			{
				aSource->fileNotAvail();
				return false;
			}
			
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			l_is_free = true;
			type = Transfer::TYPE_PARTIAL_LIST;
		}
		else
		{
			aSource->fileNotAvail("Unknown file type");
			return false;
		}
	}
	catch (const ShareException& e)
	{
		// Partial file sharing upload
		if (l_is_tth)
		{
			if (QueueManager::isChunkDownloaded(l_tth, aStartPos, aBytes, sourceFile))
			{
				dcassert(!sourceFile.empty());
				if (!sourceFile.empty())
				{
					try
					{
						auto f = new SharedFileStream(sourceFile, File::READ, File::OPEN | File::SHARED | File::NO_CACHE_HINT, 0);
						
						start = aStartPos;
						fileSize = f->getFastFileSize();
						size = (aBytes == -1) ? fileSize - start : aBytes;
						
						if ((start + size) > fileSize)
						{
							aSource->fileNotAvail();
							delete f;
							return false;
						}
						
						f->setPos(start);
						if ((start + size) < fileSize)
						{
							try
							{
								is = nullptr;
								is = new LimitedInputStream<true>(f, size);
							}
							catch (...)
							{
								delete f;
								if (is)
								{
									is->clean_stream();
								}
								throw;
							}
						}
						else
						{
							is = f;
						}
						l_is_partial = true;
						type = Transfer::TYPE_FILE;
						goto ok; // don't fix "goto", it is fixed in wx version anyway
					}
					catch (const Exception&)
					{
						safe_delete(is);
					}
				}
			}
		}
		aSource->fileNotAvail(e.getError());
		return false;
	}
	catch (const Exception& e)
	{
		LogManager::message(STRING(UNABLE_TO_SEND_FILE) + ' ' + sourceFile + ": " + e.getError());
		aSource->fileNotAvail();
		return false;
	}
	
ok:

	auto slotType = aSource->getSlotType();
	
	bool hasReserved;
	{
		CFlyReadLock(*g_csReservedSlots);
		hasReserved = g_reservedSlots.find(aSource->getUser()) != g_reservedSlots.end();
	}
	if (!hasReserved)
	{
		hasReserved = BOOLSETTING(EXTRASLOT_TO_DL) && DownloadManager::checkFileDownload(aSource->getUser());
	}
	
	const bool l_isFavorite = FavoriteManager::hasAutoGrantSlot(aSource->getUser())
#ifdef IRAINMAN_ENABLE_AUTO_BAN
# ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	                          || (SETTING(AUTOBAN_PPROTECT_OP) && aSource->getUser()->isSet(UserConnection::FLAG_OP))
# endif
#endif
	                          ;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	                          
	if (!l_isFavorite && SETTING(ENABLE_AUTO_BAN))
	{
		// �������� ��� ������� �� ��������
		if (!l_is_userlist && !hasReserved && handleBan(aSource))
		{
			delete is;
			addFailedUpload(aSource, sourceFile, aStartPos, size);
			aSource->disconnect();
			return false;
		}
	}
#endif // IRAINMAN_ENABLE_AUTO_BAN
	
	if (slotType != UserConnection::STDSLOT || hasReserved)
	{
		bool hasFreeSlot = getFreeSlots() > 0;
		if (hasFreeSlot)
		{
			CFlyLock(m_csQueue); ;
			hasFreeSlot = (m_slotQueue.empty() && m_notifiedUsers.empty() || (m_notifiedUsers.find(aSource->getUser()) != m_notifiedUsers.end()));
		}
		
		bool isAutoSlot = getAutoSlot();
		bool isHasUpload = hasUpload(aSource, sourceFile);
		
#ifdef SSA_IPGRANT_FEATURE
		bool hasSlotByIP = false;
		if (BOOLSETTING(EXTRA_SLOT_BY_IP))
		{
			if (!(hasReserved || l_isFavorite || isAutoSlot || hasFreeSlot || isHasUpload))
			{
				hasSlotByIP = IpGrant::check(Socket::convertIP4(aSource->getRemoteIp()));
			}
		}
#endif // SSA_IPGRANT_FEATURE
		if (!(hasReserved || l_isFavorite || isAutoSlot || hasFreeSlot
#ifdef SSA_IPGRANT_FEATURE
		        || hasSlotByIP
#endif
		     )
		        || isHasUpload)
		{
			const bool l_is_supportsFree = aSource->isSet(UserConnection::FLAG_SUPPORTS_MINISLOTS);
			const bool l_is_allowedFree = (slotType == UserConnection::EXTRASLOT)
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
			                              || aSource->isSet(UserConnection::FLAG_OP)
#endif
			                              || getFreeExtraSlots() > 0;
			bool l_is_partialFree = l_is_partial && ((slotType == UserConnection::PARTIALSLOT) || (extraPartial < SETTING(EXTRA_PARTIAL_SLOTS)));
			
			if (l_is_free && l_is_supportsFree && l_is_allowedFree)
			{
				slotType = UserConnection::EXTRASLOT;
			}
			else if (l_is_partialFree)
			{
				slotType = UserConnection::PARTIALSLOT;
			}
			else
			{
				safe_delete(is);
				aSource->maxedOut(addFailedUpload(aSource, sourceFile, aStartPos, fileSize)); // https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=130703
				aSource->disconnect();
				return false;
			}
		}
		else
		{
#ifdef SSA_IPGRANT_FEATURE
			if (hasSlotByIP)
				LogManager::message("IpGrant: " + STRING(GRANTED_SLOT_BY_IP) + ' ' + aSource->getRemoteIp());
#endif
			slotType = UserConnection::STDSLOT;
		}
		
		setLastGrant(GET_TICK());
	}
	
	{
		CFlyLock(m_csQueue); ;
		
		// remove file from upload queue
		clearUserFilesL(aSource->getUser());
		
		// remove user from notified list
		const auto cu = m_notifiedUsers.find(aSource->getUser());
		if (cu != m_notifiedUsers.end())
		{
			m_notifiedUsers.erase(cu);
		}
	}
	
	bool resumed = false;
	{
		CFlyWriteLock(*g_csUploadsDelay);
		for (auto i = g_delayUploads.cbegin(); i != g_delayUploads.cend(); ++i)
		{
			auto up = *i;
			if (aSource == up->getUserConnection())
			{
				g_delayUploads.erase(i);
				if (sourceFile != up->getPath())
				{
					logUpload(up);
				}
				else
				{
					resumed = true;
				}
				break;
			}
		}
	}
	UploadPtr u(new Upload(aSource, l_tth, sourceFile, l_ip, l_chiper_name));
	u->setReadStream(is);
	u->setSegment(Segment(start, size));
	aSource->setUpload(u);
	
	if (u->getSize() != fileSize)
		u->setFlag(Upload::FLAG_CHUNKED);
		
	if (resumed)
		u->setFlag(Upload::FLAG_RESUMED);
		
	if (l_is_partial)
		u->setFlag(Upload::FLAG_UPLOAD_PARTIAL);
		
	u->setFileSize(fileSize);
	u->setType(type);
	
	{
		CFlyWriteLock(*g_csUploadsDelay);
		g_uploads.push_back(u);
		increaseUserConnectionAmountL(u->getUser());
	}
	
	if (aSource->getSlotType() != slotType)
	{
		// remove old count
		process_slot(aSource->getSlotType(), -1);
		// set new slot count
		process_slot(slotType, 1);
		// user got a slot
		aSource->setSlotType(slotType);
	}
	
	return true;
}
bool UploadManager::getAutoSlot()
{
	dcassert(!ClientManager::isBeforeShutdown());
	/** A 0 in settings means disable */
	if (SETTING(MIN_UPLOAD_SPEED) == 0)
		return false;
	/** Max slots */
	if (getSlots() + SETTING(AUTO_SLOTS) < g_running)
		return false;
	/** Only grant one slot per 30 sec */
	if (GET_TICK() < getLastGrant() + 30 * 1000)
		return false;
	/** Grant if upload speed is less than the threshold speed */
	return getRunningAverage() < (SETTING(MIN_UPLOAD_SPEED) * 1024);
}
void UploadManager::shutdown()
{
	{
		CFlyWriteLock(*g_csUploadsDelay);
		g_uploadsPerUser.clear();
	}
	{
		CFlyWriteLock(*g_csReservedSlots);
		g_reservedSlots.clear();
		g_is_reservedSlotEmpty = g_reservedSlots.empty();
	}
}
void UploadManager::increaseUserConnectionAmountL(const UserPtr& p_user)
{
	if (!ClientManager::isBeforeShutdown())
	{
		const auto i = g_uploadsPerUser.find(p_user);
		if (i != g_uploadsPerUser.end())
		{
			i->second++;
		}
		else
		{
			g_uploadsPerUser.insert(CurrentConnectionPair(p_user, 1));
		}
	}
}
void UploadManager::decreaseUserConnectionAmountL(const UserPtr& p_user)
{
	if (!ClientManager::isBeforeShutdown())
	{
		const auto i = g_uploadsPerUser.find(p_user);
		//dcassert(i != g_uploadsPerUser.end());
		if (i != g_uploadsPerUser.end())
		{
			i->second--;
			if (i->second == 0)
			{
				g_uploadsPerUser.erase(p_user);
			}
		}
	}
}
unsigned int UploadManager::getUserConnectionAmountL(const UserPtr& p_user)
{
	dcassert(!ClientManager::isBeforeShutdown());
	const auto i = g_uploadsPerUser.find(p_user);
	if (i != g_uploadsPerUser.end())
	{
		return i->second;
	}
	
	dcassert(0);
	return 1;
}

void UploadManager::removeUpload(UploadPtr& aUpload, bool delay)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	CFlyWriteLock(*g_csUploadsDelay);
	//dcassert(find(g_uploads.begin(), g_uploads.end(), aUpload) != g_uploads.end());
	if (!g_uploads.empty())
	{
		g_uploads.erase(remove(g_uploads.begin(), g_uploads.end(), aUpload), g_uploads.end());
	}
	decreaseUserConnectionAmountL(aUpload->getUser());
	
	if (delay)
	{
		g_delayUploads.push_back(aUpload);
	}
	else
	{
		aUpload.reset();
	}
}

void UploadManager::reserveSlot(const HintedUser& hintedUser, uint64_t aTime)
{
	dcassert(!ClientManager::isBeforeShutdown());
	{
		CFlyWriteLock(*g_csReservedSlots);
		g_reservedSlots[hintedUser.user] = GET_TICK() + aTime * 1000;
		g_is_reservedSlotEmpty = false;
	}
	save();
	if (hintedUser.user->isOnline())
	{
		CFlyLock(m_csQueue);
		// find user in uploadqueue to connect with correct token
		auto it = std::find_if(m_slotQueue.cbegin(), m_slotQueue.cend(), [&](const UserPtr & u)
		{
			return u == hintedUser.user;
		});
		if (it != m_slotQueue.cend())
		{
			bool l_is_active_client;
			ClientManager::getInstance()->connect(hintedUser, it->getToken(), false, l_is_active_client);
		}/* else {
            token = Util::toString(Util::rand());
        }*/
	}
	
	if (BOOLSETTING(SEND_SLOTGRANT_MSG))
	{
		ClientManager::privateMessage(hintedUser, "+me " + STRING(SLOT_GRANTED_MSG) + ' ' + Util::formatSeconds(aTime), false);
	}
}

void UploadManager::unreserveSlot(const HintedUser& hintedUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
	{
		CFlyWriteLock(*g_csReservedSlots);
		g_reservedSlots.erase(hintedUser.user);
		g_is_reservedSlotEmpty = g_reservedSlots.empty();
	}
	save();
	if (BOOLSETTING(SEND_SLOTGRANT_MSG))
	{
		ClientManager::privateMessage(hintedUser, "+me " + STRING(SLOT_REMOVED_MSG), false);
	}
}

void UploadManager::on(UserConnectionListener::Get, UserConnection* aSource, const string& aFile, int64_t aResume) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
	{
		return;
	}
	if (aSource->getState() != UserConnection::STATE_GET)
	{
		dcdebug("UM::onGet Bad state, ignoring\n");
		return;
	}
	
	int64_t bytes = -1;
	if (prepareFile(aSource, Transfer::g_type_names[Transfer::TYPE_FILE], Util::toAdcFile(aFile), aResume, bytes))
	{
		aSource->setState(UserConnection::STATE_SEND);
		aSource->fileLength(Util::toString(aSource->getUpload()->getSize()));
	}
}

void UploadManager::on(UserConnectionListener::Send, UserConnection* aSource) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (ClientManager::isBeforeShutdown())
	{
		return;
	}
	if (aSource->getState() != UserConnection::STATE_SEND)
	{
		dcdebug("UM::onSend Bad state, ignoring\n");
		return;
	}
	
	auto u = aSource->getUpload();
	dcassert(u != nullptr);
	u->setStart(aSource->getLastActivity(true));
	
	aSource->setState(UserConnection::STATE_RUNNING);
	aSource->transmitFile(u->getReadStream());
	fly_fire1(UploadManagerListener::Starting(), u);
}

void UploadManager::on(AdcCommand::GET, UserConnection* aSource, const AdcCommand& c) noexcept
{
	if (ClientManager::isBeforeShutdown())
	{
		dcassert(0);
		return;
	}
	if (aSource->getState() != UserConnection::STATE_GET)
	{
		dcassert(0);
		dcdebug("UM::onGET Bad state, ignoring\n");
		return;
	}
	
	const string l_type = c.getParam(0);
	const string l_fname = c.getParam(1);
	int64_t aStartPos = Util::toInt64(c.getParam(2));
	int64_t aBytes = Util::toInt64(c.getParam(3));
#ifdef _DEBUG
//	LogManager::message("on(AdcCommand::GET aStartPos = " + Util::toString(aStartPos) + " aBytes = " + Util::toString(aBytes));
#endif

	if (prepareFile(aSource, l_type, l_fname, aStartPos, aBytes, c.hasFlag("RE", 4)))
	{
		auto u = aSource->getUpload();
		dcassert(u != nullptr);
		AdcCommand cmd(AdcCommand::CMD_SND);
		cmd.addParam(l_type);
		cmd.addParam(l_fname);
		cmd.addParam(Util::toString(u->getStartPos()));
		cmd.addParam(Util::toString(u->getSize()));
		
#ifdef _DEBUG
		ZFilter::g_is_disable_compression = true;
#endif
		if (SETTING(MAX_COMPRESSION) && ZFilter::g_is_disable_compression == false)
		{
			string l_name = Util::getFileName(u->getPath());
			string l_ext = Util::getFileExtWithoutDot(l_name);
			if (l_name.length() > 46 && l_ext == g_dc_temp_extension)  // 46 it one character first dot + 39 characters TTH + 6 characters .dctmp
			{
				l_name = l_name.erase(l_name.length() - 46);
				l_ext = Util::getFileExtWithoutDot(l_name);
			}
			if (!CFlyServerConfig::isCompressExt(Text::toLower(l_ext)))
			{
				if (c.hasFlag("ZL", 4))
				{
					try
					{
						u->setReadStream(new FilteredInputStream<ZFilter, true>(u->getReadStream()));
						u->setFlag(Upload::FLAG_ZUPLOAD);
						cmd.addParam("ZL1");
					}
					catch (Exception& e)
					{
						const string l_message = "Error UploadManager::on(AdcCommand::GET) ext = " + l_ext + " error: " + e.getError();
						CFlyServerJSON::pushError(59, l_message);
						LogManager::message(l_message);
						fly_fire2(UploadManagerListener::Failed(), u, l_message);
						return;
					}
				}
			}
		}
		aSource->send(cmd);
		
		u->setStart(aSource->getLastActivity(true));
		aSource->setState(UserConnection::STATE_RUNNING);
		aSource->transmitFile(u->getReadStream());
		fly_fire1(UploadManagerListener::Starting(), u);
	}
}

/*
void UploadManager::on(UserConnectionListener::UserBytesSent, UserConnection* aSource, size_t aBytes, size_t aActual) noexcept
{
    //dcassert(!ClientManager::isBeforeShutdown());
    dcassert(aSource->getState() == UserConnection::STATE_RUNNING);
    auto u = aSource->getUpload();
    dcassert(u != nullptr);
    u->addPos(aBytes, aActual);
}
*/

void UploadManager::on(UserConnectionListener::Failed, UserConnection* aSource, const string& aError) noexcept
{
	auto u = aSource->getUpload();
	
	if (u)
	{
		fly_fire2(UploadManagerListener::Failed(), u, aError);
		
		dcdebug("UM::onFailed (%s): Removing upload\n", aError.c_str());
		removeUpload(u);
	}
	
	removeConnection(aSource);
}

void UploadManager::on(UserConnectionListener::TransmitDone, UserConnection* aSource) noexcept
{
	//dcassert(!ClientManager::isBeforeShutdown());
	dcassert(aSource->getState() == UserConnection::STATE_RUNNING);
	auto u = aSource->getUpload();
	dcassert(u != nullptr);
	u->tick(aSource->getLastActivity(true));
	
	aSource->setState(UserConnection::STATE_GET);
	
	if (!u->isSet(Upload::FLAG_CHUNKED))
	{
		logUpload(u);
		removeUpload(u);
	}
	else
	{
		removeUpload(u, true);
	}
}

void UploadManager::logUpload(const UploadPtr& aUpload)
{
	if (!ClientManager::isBeforeShutdown())
	{
		if (BOOLSETTING(LOG_UPLOADS) && aUpload->getType() != Transfer::TYPE_TREE && (BOOLSETTING(LOG_FILELIST_TRANSFERS) || aUpload->getType() != Transfer::TYPE_FULL_LIST))
		{
			StringMap params;
			aUpload->getParams(params);
			LOG(UPLOAD, params);
		}
		fly_fire1(UploadManagerListener::Complete(), aUpload);
	}
}

size_t UploadManager::addFailedUpload(const UserConnection* aSource, const string& file, int64_t pos, int64_t size)
{
	dcassert(!ClientManager::isBeforeShutdown());
	size_t queue_position = 0;
	
	CFlyLock(m_csQueue);
	
	auto it = std::find_if(m_slotQueue.begin(), m_slotQueue.end(), [&](const UserPtr & u) -> bool { ++queue_position; return u == aSource->getUser(); });
	if (it != m_slotQueue.end())
	{
		it->setToken(aSource->getUserConnectionToken());
		// https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=130703
		for (auto i = it->m_waiting_files.cbegin(); i != it->m_waiting_files.cend(); ++i) //TODO https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=128318
		{
			if ((*i)->getFile() == file)
			{
				(*i)->setPos(pos);
				return queue_position;
			}
		}
	}
	UploadQueueItemPtr uqi(new UploadQueueItem(aSource->getHintedUser(), file, pos, size));
	if (it == m_slotQueue.end())
	{
		++queue_position;
		m_slotQueue.push_back(WaitingUser(aSource->getHintedUser(), aSource->getUserConnectionToken(), uqi));
	}
	else
	{
		it->m_waiting_files.push_back(uqi);
	}
	// Crash https://www.crash-server.com/Problem.aspx?ClientID=guest&ProblemID=29270
	if (g_count_WaitingUsersFrame)
	{
		fly_fire1(UploadManagerListener::QueueAdd(), uqi);
	}
	return queue_position;
}
void UploadManager::clearWaitingFilesL(const WaitingUser& p_wu)
{
	dcassert(!ClientManager::isBeforeShutdown());
	for (auto i = p_wu.m_waiting_files.cbegin(); i != p_wu.m_waiting_files.cend(); ++i)
	{
		if (g_count_WaitingUsersFrame)
		{
			fly_fire1(UploadManagerListener::QueueItemRemove(), (*i));
		}
	}
}
void UploadManager::clearUserFilesL(const UserPtr& aUser)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	auto it = std::find_if(m_slotQueue.cbegin(), m_slotQueue.cend(), [&](const UserPtr & u)
	{
		return u == aUser;
	});
	if (it != m_slotQueue.end())
	{
		clearWaitingFilesL(*it);
		if (g_count_WaitingUsersFrame && !ClientManager::isBeforeShutdown())
		{
			fly_fire1(UploadManagerListener::QueueRemove(), aUser);
		}
		m_slotQueue.erase(it);
	}
}

void UploadManager::addConnection(UserConnection* p_conn)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (p_conn->isIPGuard(ResourceManager::BLOCKED_INCOMING_CONN, false))
	{
		removeConnection(p_conn, false);
		return;
	}
	p_conn->addListener(this);
	p_conn->setState(UserConnection::STATE_GET);
}

void UploadManager::testSlotTimeout(uint64_t aTick /*= GET_TICK()*/)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (!g_is_reservedSlotEmpty)
	{
		CFlyWriteLock(*g_csReservedSlots);
		for (auto j = g_reservedSlots.cbegin(); j != g_reservedSlots.cend();)
		{
			if (j->second < aTick)
			{
				g_reservedSlots.erase(j++);
			}
			else
			{
				++j;
			}
		}
		g_is_reservedSlotEmpty = g_reservedSlots.empty();
	}
}
void UploadManager::process_slot(UserConnection::SlotTypes p_slot_type, int p_delta)
{
	switch (p_slot_type)
	{
		case UserConnection::STDSLOT:
			g_running += p_delta;
			break;
		case UserConnection::EXTRASLOT:
			extra += p_delta;
			break;
		case UserConnection::PARTIALSLOT:
			extraPartial += p_delta;
			break;
	}
}
void UploadManager::removeConnection(UserConnection* aSource, bool p_is_remove_listener /*= true */)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	//dcassert(aSource->getUpload() == nullptr);
	if (p_is_remove_listener)
	{
		aSource->removeListener(this);
	}
	process_slot(aSource->getSlotType(), -1);
	aSource->setSlotType(UserConnection::NOSLOT);
}

void UploadManager::notifyQueuedUsers(int64_t p_tick)
{
	dcassert(!ClientManager::isBeforeShutdown());
	// ������ ������� ����� m_csQueue
	if (m_slotQueue.empty())
		return; //no users to notify
	vector<WaitingUser> l_notifyList;
	{
		CFlyLock(m_csQueue);
		int freeslots = getFreeSlots();
		if (freeslots > 0)
		{
			freeslots -= m_notifiedUsers.size();
			while (freeslots > 0 && !m_slotQueue.empty())
			{
				// let's keep him in the connectingList until he asks for a file
				const WaitingUser& wu = m_slotQueue.front(); // TODO -  https://crash-server.com/DumpGroup.aspx?ClientID=guest&DumpGroupID=128150
				//         https://crash-server.com/Problem.aspx?ClientID=guest&ProblemID=56833
				clearWaitingFilesL(wu);
				if (g_count_WaitingUsersFrame)
				{
					fly_fire1(UploadManagerListener::QueueRemove(), wu.getUser()); // TODO ������ �� ����?
				}
				if (wu.getUser()->isOnline())
				{
					m_notifiedUsers[wu.getUser()] = p_tick;
					l_notifyList.push_back(wu);
					freeslots--;
				}
				m_slotQueue.pop_front();
			}
		}
	}
	for (auto it = l_notifyList.cbegin(); it != l_notifyList.cend(); ++it)
	{
		bool l_is_active_client;
		ClientManager::getInstance()->connect(it->m_hintedUser, it->getToken(), false, l_is_active_client);
	}
	
}

void UploadManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	UserList l_disconnects;
	{
#ifdef FLYLINKDC_USE_DOS_GUARD
		{
			CFlyFastLock(csDos);
			m_dos_map.clear();
		}
#endif
		testSlotTimeout(aTick);
		
		{
			CFlyLock(m_csQueue);
			
			for (auto i = m_notifiedUsers.cbegin(); i != m_notifiedUsers.cend();)
			{
				if ((i->second + (90 * 1000)) < aTick)
				{
					clearUserFilesL(i->first);
					m_notifiedUsers.erase(i++);
				}
				else
					++i;
			}
		}
		
		if (BOOLSETTING(AUTO_KICK))
		{
			CFlyReadLock(*g_csUploadsDelay);
			
			for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
			{
				if (auto u = *i)
				{
					if (u->getUser() && u->getUser()->isOnline())
					{
						u->unsetFlag(Upload::FLAG_PENDING_KICK);
						continue;
					}
					
					if (u->isSet(Upload::FLAG_PENDING_KICK))
					{
						l_disconnects.push_back(u->getUser());
						continue;
					}
					bool l_is_ban = false;
					if (BOOLSETTING(AUTO_KICK_NO_FAVS) && FavoriteManager::isFavoriteUser(u->getUser(), l_is_ban))
					{
						continue;
					}
					u->setFlag(Upload::FLAG_PENDING_KICK);
				}
			}
		}
	}
	
	for (auto i = l_disconnects.cbegin(); i != l_disconnects.cend(); ++i)
	{
		LogManager::message(STRING(DISCONNECTED_USER) + ' ' + Util::toString(ClientManager::getNicks((*i)->getCID(), Util::emptyString)));
		ConnectionManager::disconnect(*i, false);
	}
	
	const int l_freeSlots = getFreeSlots();
	if (l_freeSlots != m_lastFreeSlots)
	{
		m_lastFreeSlots = l_freeSlots;
	}
}
void UploadManager::on(GetListLength, UserConnection* conn) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	conn->error("GetListLength not supported");
	conn->disconnect(false);
}

void UploadManager::on(AdcCommand::GFI, UserConnection* aSource, const AdcCommand& c) noexcept
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (aSource->getState() != UserConnection::STATE_GET)
	{
		dcdebug("UM::onSend Bad state, ignoring\n");
		return;
	}
	
	if (c.getParameters().size() < 2)
	{
		aSource->send(AdcCommand(AdcCommand::SEV_RECOVERABLE, AdcCommand::ERROR_PROTOCOL_GENERIC, "Missing parameters"));
		return;
	}
	
	const string type = c.getParam(0);
	const string ident = c.getParam(1);
	
	if (type == Transfer::g_type_names[Transfer::TYPE_FILE])
	{
		try
		{
			AdcCommand cmd(AdcCommand::CMD_RES);
			ShareManager::getInstance()->getFileInfo(cmd, ident);
			aSource->send(cmd);
		}
		catch (const ShareException&)
		{
			aSource->fileNotAvail();
		}
	}
	else
	{
		aSource->fileNotAvail();
	}
}

// TimerManagerListener
void UploadManager::on(TimerManagerListener::Second, uint64_t aTick) noexcept
{
	if (ClientManager::isBeforeShutdown())
		return;
	{
		UploadArray l_tickList;
		{
			int64_t l_currentSpeed = 0;
			{
				CFlyWriteLock(*g_csUploadsDelay);
				for (auto i = g_delayUploads.cbegin(); i != g_delayUploads.cend();)
				{
					if (auto u = *i)
						if (++u->m_delayTime > 10)
						{
							logUpload(u);
							///////delete u;
							
							g_delayUploads.erase(i);
							i = g_delayUploads.cbegin();
						}
						else
						{
							++i;
						}
				}
			}
			static int g_count = 11;
			if (++g_count % 10 == 0)
			{
				SharedFileStream::cleanup();
			}
			l_tickList.reserve(g_uploads.size());
			CFlyReadLock(*g_csUploadsDelay);
			for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
			{
				auto u = *i;
				if (u->getPos() > 0)
				{
					TransferData l_td(u->getConnectionQueueToken());
					l_td.m_hinted_user = u->getHintedUser();
					l_td.m_pos = u->getStartPos() + u->getPos();
					l_td.m_actual = u->getStartPos() + u->getActual();
					l_td.m_second_left = u->getSecondsLeft(true);
					l_td.m_speed = u->getRunningAverage();
					l_td.m_start = u->getStart();
					l_td.m_size = u->getType() == Transfer::TYPE_TREE ? u->getSize() : u->getFileSize();
					l_td.m_type = u->getType();
					l_td.m_path = u->getPath();
					l_td.calc_percent();
					if (u->isSet(Upload::FLAG_UPLOAD_PARTIAL))
					{
						l_td.m_status_string += _T("[P]");
					}
					if (u->m_isSecure)
					{
						l_td.m_status_string += _T("[SSL+");
						if (u->m_isTrusted)
						{
							l_td.m_status_string += _T("T]");
						}
						else
						{
							l_td.m_status_string += _T("U]");
						}
					}
					if (u->isSet(Upload::FLAG_ZUPLOAD))
					{
						l_td.m_status_string += _T("[Z]");
					}
					if (u->isSet(Upload::FLAG_CHUNKED))
					{
						l_td.m_status_string += _T("[C]");
					}
					if (!l_td.m_status_string.empty())
					{
						l_td.m_status_string += _T(' ');
					}
					l_td.m_status_string += Text::tformat(TSTRING(UPLOADED_BYTES), Util::formatBytesW(l_td.m_pos).c_str(), l_td.m_percent, l_td.get_elapsed(aTick).c_str());
					l_td.log_debug();
					l_tickList.push_back(l_td);
					u->tick(aTick);
				}
				u->getUserConnection()->getSocket()->updateSocketBucket(getUserConnectionAmountL(u->getUser()));
				l_currentSpeed += u->getRunningAverage();
			}
			g_runningAverage = l_currentSpeed;
		}
		if (!l_tickList.empty())
		{
			fly_fire1(UploadManagerListener::Tick(), l_tickList);
			// TODO - ��������� ��� �����
		}
	}
	notifyQueuedUsers(aTick);
	
	if (g_count_WaitingUsersFrame)
	{
		fly_fire(UploadManagerListener::QueueUpdate());
	}
	
	if (!isFireball)
	{
		if (getRunningAverage() >= 1024 * 1024)
		{
			if (m_fireballStartTick == 0)
			{
				if ((aTick - m_fireballStartTick) > 60 * 1000)
				{
					isFireball = true;
					ClientManager::infoUpdated();
				}
			}
			else
			{
				// speed dropped below 100 kB/s
				m_fireballStartTick = 0;
			}
		}
		if (!isFireball && !isFileServer)
		{
			if ((Util::getUpTime() > 10 * 24 * 60 * 60) && // > 10 days uptime
			        (Socket::g_stats.m_tcp.totalUp > 100ULL * 1024 * 1024 * 1024) && // > 100 GiB uploaded
			        (ShareManager::getShareSize() > 1.5 * 1024 * 1024 * 1024 * 1024)) // > 1.5 TiB shared
			{
				isFileServer = true;
				ClientManager::infoUpdated();
			}
		}
	}
}

void UploadManager::on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept
{
	//dcassert(!ClientManager::isBeforeShutdown());
	if (!aUser->isOnline())
	{
		CFlyLock(m_csQueue); ;
		clearUserFilesL(aUser);
	}
}

void UploadManager::removeDelayUpload(const UserPtr& aUser)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	CFlyWriteLock(*g_csUploadsDelay);
	for (auto i = g_delayUploads.cbegin(); i != g_delayUploads.cend(); ++i)
	{
		auto up = *i;
		if (aUser == up->getUser())
		{
			g_delayUploads.erase(i);
			/////// delete up;
			break;
		}
	}
}

/**
 * Abort upload of specific file
 */
void UploadManager::abortUpload(const string& aFile, bool waiting)
{
	//dcassert(!ClientManager::isBeforeShutdown());
	bool nowait = true;
	{
		CFlyReadLock(*g_csUploadsDelay);
		for (auto i = g_uploads.cbegin(); i != g_uploads.cend(); ++i)
		{
			auto u = *i;
			if (u->getPath() == aFile)
			{
				u->getUserConnection()->disconnect(true);
				nowait = false;
			}
		}
	}
	
	if (nowait) return;
	if (!waiting) return;
	
	for (int i = 0; i < 20 && nowait == false; i++)
	{
		Thread::sleep(100);
		{
			CFlyReadLock(*g_csUploadsDelay);
			nowait = true;
			for (auto j = g_uploads.cbegin(); j != g_uploads.cend(); ++j)
			{
				if ((*j)->getPath() == aFile)
				{
					dcdebug("upload %s is not removed\n", aFile.c_str());
					nowait = false;
					break;
				}
			}
		}
	}
	
	if (!nowait)
	{
		dcdebug("abort upload timeout %s\n", aFile.c_str());
	}
}


time_t UploadManager::getReservedSlotTime(const UserPtr& aUser)
{
	dcassert(!ClientManager::isBeforeShutdown());
	if (g_is_reservedSlotEmpty)
		return 0;
	CFlyReadLock(*g_csReservedSlots);
	const auto j = g_reservedSlots.find(aUser);
	return j != g_reservedSlots.end() ? j->second : 0;
}


void UploadManager::save()
{
	CFlyRegistryMap values;
	{
		CFlyReadLock(*g_csReservedSlots);
		for (auto i = g_reservedSlots.cbegin(); i != g_reservedSlots.cend(); ++i)
		{
			values[i->first->getCID().toBase32()] = CFlyRegistryValue(i->second);
		}
	}
	CFlylinkDBManager::getInstance()->save_registry(values, e_ExtraSlot, true);
}


void UploadManager::load()
{
	CFlyRegistryMap l_values;
	CFlylinkDBManager::getInstance()->load_registry(l_values, e_ExtraSlot);
	for (auto k = l_values.cbegin(); k != l_values.cend(); ++k)
	{
		auto user = ClientManager::createUser(CID(k->first), "", 0);
		CFlyWriteLock(*g_csReservedSlots);
		g_reservedSlots[user] = uint32_t(k->second.m_val_int64);
		g_is_reservedSlotEmpty = g_reservedSlots.empty();
	}
	testSlotTimeout();
}
int UploadQueueItem::compareItems(const UploadQueueItem* a, const UploadQueueItem* b, uint8_t col)
{
	dcassert(!ClientManager::isBeforeShutdown());
	//+BugMaster: small optimization; fix; correct IP sorting
	switch (col)
	{
		case COLUMN_FILE:
		case COLUMN_TYPE:
		case COLUMN_PATH:
		case COLUMN_NICK:
		case COLUMN_HUB:
			return stricmp(a->getText(col), b->getText(col));
		case COLUMN_TRANSFERRED:
			return compare(a->m_pos, b->m_pos);
		case COLUMN_SIZE:
			return compare(a->m_size, b->m_size);
		case COLUMN_ADDED:
		case COLUMN_WAITING:
			return compare(a->m_time, b->m_time);
		case COLUMN_SLOTS:
			return compare(a->getUser()->getSlots(), b->getUser()->getSlots());
		case COLUMN_SHARE:
			return compare(a->getUser()->getBytesShared(), b->getUser()->getBytesShared());
		case COLUMN_IP:
			return compare(Socket::convertIP4(Text::fromT(a->getText(col))), Socket::convertIP4(Text::fromT(b->getText(col))));
	}
	return stricmp(a->getText(col), b->getText(col));
}

void UploadQueueItem::update()
{
	dcassert(!ClientManager::isBeforeShutdown());
	
	setText(COLUMN_FILE, Text::toT(Util::getFileName(getFile())));
	setText(COLUMN_TYPE, Text::toT(Util::getFileExtWithoutDot(getFile())));
	setText(COLUMN_PATH, Text::toT(Util::getFilePath(getFile())));
	setText(COLUMN_NICK, getUser()->getLastNickT()); // [1] https://www.box.net/shared/plriwg50qendcr3kbjp5
	setText(COLUMN_HUB, getHintedUser().user ? Text::toT(Util::toString(ClientManager::getHubNames(getHintedUser().user->getCID(), Util::emptyString))) : Util::emptyStringT);
	setText(COLUMN_TRANSFERRED, Util::formatBytesW(getPos()) + _T(" (") + Util::toStringW((double)getPos() * 100.0 / (double)getSize()) + _T("%)"));
	setText(COLUMN_SIZE, Util::formatBytesW(getSize()));
	setText(COLUMN_ADDED, Text::toT(Util::formatDigitalClock(getTime())));
	setText(COLUMN_WAITING, Util::formatSecondsW(GET_TIME() - getTime()));
	setText(COLUMN_SHARE, Util::formatBytesW(getUser()->getBytesShared()));
	setText(COLUMN_SLOTS, Util::toStringW(getUser()->getSlots()));
	
	if (m_location.isNew() && !getUser()->getIP().is_unspecified())
	{
		m_location = Util::getIpCountry(getUser()->getIP().to_ulong());
		setText(COLUMN_IP, Text::toT(getUser()->getIPAsString()));
	}
	if (m_location.isKnown())
	{
		setText(COLUMN_LOCATION, m_location.getDescription());
	}
#ifdef FLYLINKDC_USE_DNS
	
	if (m_dns.empty())
	{
		m_dns = Socket::nslookup(m_ip);
		setText(COLUMN_DNS, Text::toT(m_dns)); // todo: paint later if not resolved yet
	}
	
#endif
}

/**
 * @file
 * $Id: UploadManager.cpp 581 2011-11-02 18:59:46Z bigmuscle $
 */
