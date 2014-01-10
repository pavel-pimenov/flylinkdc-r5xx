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
#include "UploadManager.h"
#include "DownloadManager.h"// !SMT!-S

#include <cmath>

#include "ConnectionManager.h"
#include "ShareManager.h"
#include "CryptoManager.h"
#include "Upload.h"
#include "QueueManager.h"
#include "FinishedManager.h"
#include "PGLoader.h"
#include "SharedFileStream.h"
#include "IPGrant.h"
#include "../FlyFeatures/flyServer.h"

static const string UPLOAD_AREA = "Uploads";

UploadManager::UploadManager() noexcept :
running(0), extra(0), lastGrant(0), lastFreeSlots(-1),
        fireballStartTick(0), isFireball(false), isFileServer(false), extraPartial(0),
        runningAverage(0)//[+] IRainman refactoring transfer mechanism
{
	ClientManager::getInstance()->addListener(this);
	TimerManager::getInstance()->addListener(this);
}

UploadManager::~UploadManager()
{
	TimerManager::getInstance()->removeListener(this);
	ClientManager::getInstance()->removeListener(this);
	{
		Lock l(m_csQueue); // [!] IRainman opt.
		for (auto ii = m_slotQueue.cbegin(); ii != m_slotQueue.cend(); ++ii)
		{
			for (auto i = ii->m_files.cbegin(); i != ii->m_files.cend(); ++i)
			{
				(*i)->dec();
			}
		}
		m_slotQueue.clear();
	}
	
	while (true)
	{
		{
			Lock l(m_csUploads); // [!] IRainman opt.
			if (m_uploads.empty())
				break;
		}
		Thread::sleep(100);
	}
}
// !SMT!-S
#ifdef IRAINMAN_ENABLE_AUTO_BAN
bool UploadManager::handleBan(UserConnection& aSource/*, bool forceBan, bool noChecks*/)
{
	const UserPtr& user = aSource.getUser();
	if (!user->isOnline()) // if not online, cheat (connection without hub)
	{
		aSource.disconnect();
		return true;
	}
	bool l_is_ban = false;
	const bool l_is_favorite = FavoriteManager::getInstance()->isFavoriteUser(user, l_is_ban);
	const auto banType = user->hasAutoBan(nullptr, l_is_favorite);
	bool banByRules = banType != User::BAN_NONE;
	if (banByRules)
	{
		FavoriteHubEntry* hub = FavoriteManager::getInstance()->getFavoriteHubEntry(aSource.getHubUrl());
		if (hub && hub->getExclChecks())
			banByRules = false;
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
	//2) "автобан" по шаре максимум ограничить размером "своя шара" (но не более 20 гиг)
	//3) "автобан" по слотам максимум ограничить размером "сколько слотов у меня"/2 (но не более 15 слотов)
	//4) "автобан" по лимиттеру максимум ограничить размером "лимиттер у меня"/2 (но не выше 60 кб/сек)
	*/
	
	// BAN for Slots:[1/5] Share:[17/200]Gb Limit:[30/50]kb/s Msg: share more files, please!
	string banstr;
	//if (!forceBan)
	//{
	/*
	if (msg.slots < msg.min_slots)
	    banstr = banstr + " Slots < "  + Util::toString(msg.min_slots);
	if (msg.slots > msg.max_slots)
	    banstr = banstr + " Slots > " + Util::toString(msg.max_slots);
	if (msg.share < msg.min_share)
	    banstr = banstr + " Share < " + Util::toString(msg.min_share) + " Gb";
	if (msg.limit && msg.limit < msg.min_limit)
	    banstr = banstr + " Limit < " + Util::toString(msg.min_limit) + " kb/s";
	banstr = banstr + " Msg: " + SETTING(BAN_MESSAGE);
	*/
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
	//}
	//else
	//{
	//  banstr = "BAN forever";
	//}
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
		
		LogManager::getInstance()->message("User:" + user->getLastNick() + ' ' + logline); //[+]PPA
	}
#endif
	// old const bool sendStatus = aSource.isSet(UserConnection::FLAG_SUPPORTS_BANMSG);
	
	if (!BOOLSETTING(BAN_STEALTH))
	{
		aSource.error(banstr);
		
		if (BOOLSETTING(BAN_FORCE_PM))
		{
			// [!] IRainman fix.
			msg.tick = TimerManager::getTick();
			const auto pmMsgPeriod = SETTING(BANMSG_PERIOD);
			const auto& key = user->getCID().toBase32();
			bool sendPm;
			{
				FastUniqueLock l(m_csBans); // [+] IRainman opt.
				auto t = m_lastBans.find(key);
				if (t == m_lastBans.end()) // new banned user
				{
					m_lastBans[key] = msg;
					sendPm = pmMsgPeriod != 0;
				}
				else if (!t->second.same(msg)) // new ban message
				{
					t->second = msg;
					sendPm = pmMsgPeriod != 0;
				}
				else if (pmMsgPeriod && (uint32_t)(msg.tick - t->second.tick) >= (uint32_t)pmMsgPeriod * 60 * 1000) // [!] IRainman fix: "Repeat PM period (0 - disable PM), min"
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
				ClientManager::getInstance()->privateMessage(aSource.getHintedUser(), banstr, false);
			}
			// [~] IRainman fix.
		}
	}
	
//	if (BOOLSETTING(BAN_STEALTH)) aSource.maxedOut();

	return true;
}
// !SMT!-S
bool UploadManager::isBanReply(const UserPtr& user) const
{
	const auto& key = user->getCID().toBase32();
	{
		FastSharedLock l(m_csBans); // [+] IRainman opt.
		const auto t = m_lastBans.find(key);
		if (t != m_lastBans.end())
			return (TimerManager::getTick() - t->second.tick) < 2000;
	}
	return false;
}
#endif // IRAINMAN_ENABLE_AUTO_BAN
// [+] FlylinkDC++
bool UploadManager::hasUpload(UserConnection& p_newLeacher) const
{
	if (p_newLeacher.getSocket())
	{
		const auto& newLeacherIp = p_newLeacher.getSocket()->getIp();
		const auto& newLeacherShare = p_newLeacher.getUser()->getBytesShared(); // [!] IRainamn fix, old code: ClientManager::getInstance()->getBytesShared(aSource.getUser());
		const auto& newLeacherNick = p_newLeacher.getUser()->getLastNick();
		
		Lock l(m_csUploads); // [+] IRainman opt.
		
		for (auto i = m_uploads.cbegin(); i != m_uploads.cend(); ++i)
		{
			const Upload* u = *i;
			dcassert(u);
			dcassert(u->getUser());
			const auto& uploadUserIp = u->getUserConnection().getSocket()->getIp();
			const auto& uploadUserShare = u->getUser()->getBytesShared(); // [!] IRainamn fix, old code: ClientManager::getInstance()->getBytesShared(u.getUser());
			const auto& uploadUserNick = u->getUser()->getLastNick();
			
			if (u->getUserConnection().getSocket() &&
			        newLeacherIp == uploadUserIp &&
			        (newLeacherShare == uploadUserShare ||
			         newLeacherNick == uploadUserNick) // [+] back port from r4xx.
			   )
			{
				/*
				#if defined(NIGHT_BUILD) || defined(DEBUG) || defined(FLYLINKDC_BETA)
				                char bufNewLeacherShare[64];
				                char bufUploadUserShare[64];
				                char bufShareDelta[64];
				
				                snprintf(bufNewLeacherShare, sizeof(bufNewLeacherShare), "%I64d", newLeacherShare);
				                snprintf(bufUploadUserShare, sizeof(bufUploadUserShare), "%I64d", uploadUserShare);
				                snprintf(bufShareDelta, sizeof(bufShareDelta), "%I64d", int64_t(std::abs(long(newLeacherShare - uploadUserShare))));
				
				                LogManager::getInstance()->message("[Drop duplicated connection] From IP =" + newLeacherIp
				                                                   + ", share con = " + bufNewLeacherShare
				                                                   + ", share exist = " + string(bufUploadUserShare)
				                                                   + " delta = " + string(bufShareDelta)
				                                                   + " nick con = " + p_newLeacher.getUser()->getLastNick()
				                                                   + " nick exist = " + u->getUser()->getLastNick()
				                                                  );
				#endif
				                                                  */
				return true;
			}
		}
	}
	return false;
}
// [~] FlylinkDC++
bool UploadManager::prepareFile(UserConnection& aSource, const string& aType, const string& aFile, int64_t aStartPos, int64_t& aBytes, bool listRecursive)
{
	dcdebug("Preparing %s %s " I64_FMT " " I64_FMT " %d\n", aType.c_str(), aFile.c_str(), aStartPos, aBytes, listRecursive);
	
	if (aFile.empty() || aStartPos < 0 || aBytes < -1 || aBytes == 0)
	{
		aSource.fileNotAvail("Invalid request");
		return false;
	}
	const bool l_is_TypeTree = aType == Transfer::g_type_names[Transfer::TYPE_TREE];
#ifdef PPA_INCLUDE_DOS_GUARD
	if (l_is_TypeTree) // && aFile == "TTH/HDWK5FVECXJDLTECQ6TY435WWEE7RU25RSQYYWY"
	{
		const HintedUser& l_User = aSource.getHintedUser();
		if (l_User.user)
		{
			// [!] IRainman opt: CID on ADC hub is unique to all hubs,
			// in NMDC it is obtained from the hash of username and hub address (computed locally).
			// There is no need to use any additional data, but it is necessary to consider TTH of the file.
			// This ensures the absence of a random user locks sitting on several ADC hubs,
			// because the number of connections from a single user much earlier limited in the ConnectionManager.
			const string l_hash_key = Util::toString(l_User.user->getCID().toHash()) + aFile;
			uint8_t l_count_dos;
			{
				FastLock l(csDos); // [!] IRainman opt.
				l_count_dos = ++m_dos_map[l_hash_key];
			}
			static const uint8_t l_count_attempts = 30;
			if (l_count_dos > l_count_attempts)
			{
				dcdebug("l_hash_key = %s ++m_dos_map[l_hash_key] = %d\n", l_hash_key.c_str(), l_count_dos);
				char l_buf[2000];
				sprintf_s(l_buf, _countof(l_buf), CSTRING(DOS_ATACK),
				          l_User.user->getLastNick().c_str(),
				          l_User.hint.c_str(),
				          l_count_attempts,
				          aFile.c_str());
				LogManager::getInstance()->message(l_buf);
				if (aSource.isSet(UserConnection::FLAG_SUPPORTS_BANMSG))
					aSource.error(UserConnection::PLEASE_UPDATE_YOUR_CLIENT);
					
				return false;
			}
			/*
			if(l_count_dos > 50)
			     {
			         LogManager::getInstance()->message("[DoS] disconnect " + aSource.getUser()->getLastNick());
			         aSource.disconnect();
			         return false;
			     }
			*/
		}
	}
#endif // PPA_INCLUDE_DOS_GUARD
	InputStream* is = nullptr;
	int64_t start = 0;
	int64_t size = 0;
	int64_t fileSize = 0;
	
	const bool userlist = (aFile == Transfer::g_user_list_name_bz || aFile == Transfer::g_user_list_name);
	bool free = userlist;
	bool partial = false;
	
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
	bool ishidingShare;
	if (aSource.getUser())
	{
		const FavoriteHubEntry* fhe = FavoriteManager::getInstance()->getFavoriteHubEntry(aSource.getHintedUser().hint);
		ishidingShare = fhe && fhe->getHideShare();
	}
	else
		ishidingShare = false;
#endif
		
	string sourceFile;
	Transfer::Type type;
	
	try
	{
		if (aType == Transfer::g_type_names[Transfer::TYPE_FILE])
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
				CryptoManager::getInstance()->decodeBZ2(reinterpret_cast<const uint8_t*>(bz2.data()), bz2.size(), xml);
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
					aSource.fileNotAvail();
					delete f;
					return false;
				}
				
				free = free || (sz <= (int64_t)(SETTING(SET_MINISLOT_SIZE) * 1024));
				
				f->setPos(start);
				is = f;
				if ((start + size) < sz)
				{
					is = new LimitedInputStream<true>(is, size);
				}
			}
			type = userlist ? Transfer::TYPE_FULL_LIST : Transfer::TYPE_FILE;
		}
		else if (l_is_TypeTree)
		{
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
			if (ishidingShare)
			{
				aSource.fileNotAvail();
				return false;
			}
#endif
			//sourceFile = ShareManager::getInstance()->toReal(aFile);
			sourceFile = aFile;
			MemoryInputStream* mis = ShareManager::getInstance()->getTree(aFile);
			if (!mis)
			{
				aSource.fileNotAvail();
				return false;
			}
			
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			free = true;
			type = Transfer::TYPE_TREE;
		}
		else if (aType == Transfer::g_type_names[Transfer::TYPE_PARTIAL_LIST])
		{
			// Partial file list
			MemoryInputStream* mis = ShareManager::getInstance()->generatePartialList(aFile, listRecursive
#ifdef IRAINMAN_INCLUDE_HIDE_SHARE_MOD
			                                                                          , ishidingShare
#endif
			                                                                         );
			if (!mis)
			{
				aSource.fileNotAvail();
				return false;
			}
			
			start = 0;
			fileSize = size = mis->getSize();
			is = mis;
			free = true;
			type = Transfer::TYPE_PARTIAL_LIST;
		}
		else
		{
			aSource.fileNotAvail("Unknown file type");
			return false;
		}
	}
	catch (const ShareException& e)
	{
		// Partial file sharing upload
		if (aType == Transfer::g_type_names[Transfer::TYPE_FILE] && aFile.compare(0, 4, "TTH/", 4) == 0)
		{
		
			TTHValue fileHash(aFile.c_str() + 4); //[+]FlylinkDC++
			
			if (QueueManager::getInstance()->isChunkDownloaded(fileHash, aStartPos, aBytes, sourceFile))
			{
				try
				{
					auto ss = new SharedFileStream(sourceFile, File::READ, File::OPEN | File::SHARED | File::NO_CACHE_HINT);
					
					start = aStartPos;
					fileSize = ss->getSize();
					size = (aBytes == -1) ? fileSize - start : aBytes;
					
					if ((start + size) > fileSize)
					{
						aSource.fileNotAvail();
						delete ss;
						return false;
					}
					
					ss->setPos(start);
					// [!] IRainman fix.
					// [-] is = ss;
					
					if ((start + size) < fileSize)
					{
						// [+]
						try
						{
							is = new LimitedInputStream<true>(ss, size); // [!]
						}
						catch (...)
						{
							delete ss;
							throw;
						}
						// [+]
					}
					else
					{
						is = ss; // [+]
					}
					// [~] IRainman fix.
					
					partial = true;
					type = Transfer::TYPE_FILE;
					goto ok; // don't fix "goto", it is fixed in wx version anyway
				}
				catch (const Exception&)
				{
					delete is;
				}
			}
		}
		aSource.fileNotAvail(e.getError());
		return false;
	}
	catch (const Exception& e)
	{
		LogManager::getInstance()->message(STRING(UNABLE_TO_SEND_FILE) + ' ' + sourceFile + ": " + e.getError());
		aSource.fileNotAvail();
		return false;
	}
	
ok: //[!] TODO убрать goto

	uint8_t slotType = aSource.getSlotType();
	
	// [-] Lock l(cs); [-] IRainman opt.
	
	//[!] IRainman autoban fix: please check this code after merge
	bool hasReserved;
	{
		FastSharedLock l(m_csReservedSlots); // [+] IRainman opt.
		hasReserved = m_reservedSlots.find(aSource.getUser()) != m_reservedSlots.end();
	}
	if (!hasReserved)
	{
		hasReserved = BOOLSETTING(EXTRASLOT_TO_DL) && DownloadManager::getInstance()->checkFileDownload(aSource.getUser());// !SMT!-S
	}
	
	const bool isFavorite = FavoriteManager::getInstance()->hasAutoGrantSlot(aSource.getUser())
#ifdef IRAINMAN_ENABLE_AUTO_BAN
# ifdef IRAINMAN_ENABLE_OP_VIP_MODE
	                        || (SETTING(AUTOBAN_PPROTECT_OP) && aSource.getUser()->isSet(UserConnection::FLAG_OP))
# endif
#endif
	                        ;
#ifdef IRAINMAN_ENABLE_AUTO_BAN
	// !SMT!-S
	if (!isFavorite && SETTING(ENABLE_AUTO_BAN))
	{
		// фавориты под автобан не попадают
		if (!userlist && !hasReserved && handleBan(aSource))
		{
			delete is;
			addFailedUpload(aSource, sourceFile, aStartPos, size);
			aSource.disconnect();
			return false;
		}
	}
#endif // IRAINMAN_ENABLE_AUTO_BAN
	
	if (slotType != UserConnection::STDSLOT || hasReserved)
	{
		//[-] IRainman autoban fix: please check this code after merge
		//bool hasReserved = reservedSlots.find(aSource.getUser()) != reservedSlots.end();
		//bool isFavorite = FavoriteManager::getInstance()->hasSlot(aSource.getUser());
		bool hasFreeSlot = getFreeSlots() > 0;
		if (hasFreeSlot)
		{
			Lock l(m_csQueue);  // [+] IRainman opt.
			hasFreeSlot = (m_slotQueue.empty() && m_notifiedUsers.empty() || (m_notifiedUsers.find(aSource.getUser()) != m_notifiedUsers.end()));
		}
		
		bool isAutoSlot = getAutoSlot();
		bool isHasUpload = hasUpload(aSource);
		
#ifdef SSA_IPGRANT_FEATURE // SSA - additional slots for special IP's
		bool hasSlotByIP = false;
		if (BOOLSETTING(EXTRA_SLOT_BY_IP))
		{
			if (!(hasReserved || isFavorite || isAutoSlot || hasFreeSlot || isHasUpload))
			{
				hasSlotByIP = IpGrant::getInstance()->check(aSource.getRemoteIp());
			}
		}
#endif // SSA_IPGRANT_FEATURE
		if (!(hasReserved || isFavorite || isAutoSlot || hasFreeSlot
#ifdef SSA_IPGRANT_FEATURE
		        || hasSlotByIP
#endif
		     )
		        || isHasUpload)
		{
			bool supportsFree = aSource.isSet(UserConnection::FLAG_SUPPORTS_MINISLOTS);
			bool allowedFree = (slotType == UserConnection::EXTRASLOT)
#ifdef IRAINMAN_ENABLE_OP_VIP_MODE
			                   || aSource.isSet(UserConnection::FLAG_OP)
#endif
			                   || getFreeExtraSlots() > 0;
			bool partialFree = partial && ((slotType == UserConnection::PARTIALSLOT) || (extraPartial < SETTING(EXTRA_PARTIAL_SLOTS)));
			
			if (free && supportsFree && allowedFree)
			{
				slotType = UserConnection::EXTRASLOT;
			}
			else if (partialFree)
			{
				slotType = UserConnection::PARTIALSLOT;
			}
			else
			{
				delete is;
				aSource.maxedOut(addFailedUpload(aSource, sourceFile, aStartPos, fileSize));
				aSource.disconnect();
				return false;
			}
		}
		else
		{
#ifdef SSA_IPGRANT_FEATURE
			if (hasSlotByIP)
				LogManager::getInstance()->message("IpGrant: " + STRING(GRANTED_SLOT_BY_IP) + ' ' + aSource.getRemoteIp());
#endif
			slotType = UserConnection::STDSLOT;
		}
		
		setLastGrant(GET_TICK());
	}
	
	{
		Lock l(m_csQueue);  // [+] IRainman opt.
		
		// remove file from upload queue
		clearUserFilesL(aSource.getUser());
		
		// remove user from notified list
		const auto& cu = m_notifiedUsers.find(aSource.getUser());
		if (cu != m_notifiedUsers.end())
		{
			m_notifiedUsers.erase(cu);
		}
	}
	
	bool resumed = false;
	{
		Lock l(m_csUploads); // [+] IRainman opt.
		for (auto i = m_delayUploads.cbegin(); i != m_delayUploads.cend(); ++i)
		{
			Upload* up = *i;
			if (&aSource == &up->getUserConnection())
			{
				m_delayUploads.erase(i);
				if (sourceFile != up->getPath())
				{
					logUpload(up);
				}
				else
				{
					resumed = true;
				}
				delete up;
				break;
			}
		}
	}
	
	Upload* u = new Upload(aSource, sourceFile); // [!] IRainman fix.
	u->setStream(is);
	u->setSegment(Segment(start, size));
	
	if (u->getSize() != fileSize)
		u->setFlag(Upload::FLAG_CHUNKED);
		
	if (resumed)
		u->setFlag(Upload::FLAG_RESUMED);
		
	if (partial)
		u->setFlag(Upload::FLAG_PARTIAL);
		
	u->setFileSize(fileSize);
	u->setType(type);
	
	{
		Lock l(m_csUploads); // [+] IRainman opt.
		m_uploads.push_back(u);
		increaseUserConnectionAmountL(u->getUser());// [+] IRainman SpeedLimiter
	}
	
	if (aSource.getSlotType() != slotType)
	{
		// remove old count
		switch (aSource.getSlotType())
		{
			case UserConnection::STDSLOT:
				running--;
				break;
			case UserConnection::EXTRASLOT:
				extra--;
				break;
			case UserConnection::PARTIALSLOT:
				extraPartial--;
				break;
		}
		// set new slot count
		switch (slotType)
		{
			case UserConnection::STDSLOT:
				running++;
				break;
			case UserConnection::EXTRASLOT:
				extra++;
				break;
			case UserConnection::PARTIALSLOT:
				extraPartial++;
				break;
		}
		
		// user got a slot
		aSource.setSlotType(slotType);
	}
	
	return true;
}
/*[-] IRainman refactoring transfer mechanism
int64_t UploadManager::getRunningAverage()
{
    Lock l(cs);
    int64_t avg = 0;
    for (auto i = uploads.cbegin(); i != uploads.cend(); ++i)
    {
        Upload* u = *i;
        avg += u->getAverageSpeed();
    }
    return avg;
}
*/
bool UploadManager::getAutoSlot()
{
	/** A 0 in settings means disable */
	if (SETTING(MIN_UPLOAD_SPEED) == 0)
		return false;
	/** Max slots */
	if (getSlots() + SETTING(AUTO_SLOTS) < running)
		return false;
	/** Only grant one slot per 30 sec */
	if (GET_TICK() < getLastGrant() + 30 * 1000)
		return false;
	/** Grant if upload speed is less than the threshold speed */
	return getRunningAverage() < (SETTING(MIN_UPLOAD_SPEED) * 1024);
}

void UploadManager::removeUpload(Upload* aUpload, bool delay)
{
	Lock l(m_csUploads); // [!] IRainman opt.
	dcassert(find(m_uploads.begin(), m_uploads.end(), aUpload) != m_uploads.end());
	m_uploads.erase(remove(m_uploads.begin(), m_uploads.end(), aUpload), m_uploads.end());
	decreaseUserConnectionAmountL(aUpload->getUser());// [+] IRainman SpeedLimiter
	
	if (delay)
	{
		m_delayUploads.push_back(aUpload);
	}
	else
	{
		delete aUpload;
	}
}

void UploadManager::reserveSlot(const HintedUser& hintedUser, uint64_t aTime)
{
	{
		FastUniqueLock l(m_csReservedSlots); // [!] IRainman opt.
		m_reservedSlots[hintedUser.user] = GET_TICK() + aTime * 1000;
	}
	save(); // !SMT!-S
	if (hintedUser.user->isOnline())
	{
		Lock l(m_csQueue); // [+] IRainman opt.
		// find user in uploadqueue to connect with correct token
		auto it = std::find_if(m_slotQueue.cbegin(), m_slotQueue.cend(), [&](const UserPtr & u)
		{
			return u == hintedUser.user;
		});
		if (it != m_slotQueue.cend())
		{
			ClientManager::getInstance()->connect(hintedUser, it->getToken());
		}/* else {
            token = Util::toString(Util::rand());
        }*/
	}
	
	if (BOOLSETTING(SEND_SLOTGRANT_MSG)) // !SMT!-S
	{
		ClientManager::getInstance()->privateMessage(hintedUser, "+me " + STRING(SLOT_GRANTED_MSG) + ' ' + Util::formatSeconds(aTime), false); // !SMT!-S
	}
}

void UploadManager::unreserveSlot(const HintedUser& hintedUser)
{
	{
		FastUniqueLock l(m_csReservedSlots); // [!] IRainman opt.
		m_reservedSlots.erase(hintedUser.user);
	}
	save(); // !SMT!-S
	if (BOOLSETTING(SEND_SLOTGRANT_MSG)) // !SMT!-S
	{
		ClientManager::getInstance()->privateMessage(hintedUser, "+me " + STRING(SLOT_REMOVED_MSG), false); // !SMT!-S
	}
}

void UploadManager::on(UserConnectionListener::Get, UserConnection* aSource, const string& aFile, int64_t aResume) noexcept
{
	if (aSource->getState() != UserConnection::STATE_GET)
	{
		dcdebug("UM::onGet Bad state, ignoring\n");
		return;
	}
	
	int64_t bytes = -1;
	if (prepareFile(*aSource, Transfer::g_type_names[Transfer::TYPE_FILE], Util::toAdcFile(aFile), aResume, bytes))
	{
		aSource->setState(UserConnection::STATE_SEND);
		aSource->fileLength(Util::toString(aSource->getUpload()->getSize()));
	}
}

void UploadManager::on(UserConnectionListener::Send, UserConnection* aSource) noexcept
{
	if (aSource->getState() != UserConnection::STATE_SEND)
	{
		dcdebug("UM::onSend Bad state, ignoring\n");
		return;
	}
	
	Upload* u = aSource->getUpload();
	dcassert(u != nullptr);
	// [-] if (!u) return; [-] IRainman fix: please don't problem maskerate.
	
	// [!] IRainman refactoring transfer mechanism
	u->setStart(aSource->getLastActivity());
	// [-] u->tick(GET_TICK());
	// [~]
	
	aSource->setState(UserConnection::STATE_RUNNING);
	aSource->transmitFile(u->getStream());
	fire(UploadManagerListener::Starting(), u);
}

void UploadManager::on(AdcCommand::GET, UserConnection* aSource, const AdcCommand& c) noexcept
{
	if (aSource->getState() != UserConnection::STATE_GET)
	{
		dcdebug("UM::onGET Bad state, ignoring\n");
		return;
	}
	
	const string& type = c.getParam(0);
	const string& fname = c.getParam(1);
	int64_t aStartPos = Util::toInt64(c.getParam(2));
	int64_t aBytes = Util::toInt64(c.getParam(3));
	
	if (prepareFile(*aSource, type, fname, aStartPos, aBytes, c.hasFlag("RE", 4)))
	{
		Upload* u = aSource->getUpload();
		dcassert(u != nullptr);
		// [-] if (!u) return; [-] IRainman fix: please don't problem maskerate.
		AdcCommand cmd(AdcCommand::CMD_SND);
		cmd.addParam(type).addParam(fname)
		.addParam(Util::toString(u->getStartPos()))
		.addParam(Util::toString(u->getSize()));
		
		string l_name = Util::getFileName(u->getPath());
		string l_ext = Util::getFileExtWithoutDot(l_name);
		if (l_ext == TEMP_EXTENSION && l_name.length() > 46) // 46 it one character first dot + 39 characters TTH + 6 characters .dctmp
		{
			l_name = l_name.erase(l_name.length() - 46);
			l_ext = Util::getFileExtWithoutDot(l_name);
		}
		if (!g_fly_server_config.isCompressExt(Text::toLower(l_ext)))
			if (c.hasFlag("ZL", 4))
			{
				u->setStream(new FilteredInputStream<ZFilter, true>(u->getStream()));
				u->setFlag(Upload::FLAG_ZUPLOAD);
				cmd.addParam("ZL1");
			}
			
		aSource->send(cmd);
		
		// [!] IRainman refactoring transfer mechanism
		u->setStart(aSource->getLastActivity());
		// [-] u->tick(GET_TICK());
		// [~]
		aSource->setState(UserConnection::STATE_RUNNING);
		aSource->transmitFile(u->getStream());
		fire(UploadManagerListener::Starting(), u);
	}
}

void UploadManager::on(UserConnectionListener::BytesSent, UserConnection* aSource, size_t aBytes, size_t aActual) noexcept
{
	dcassert(aSource->getState() == UserConnection::STATE_RUNNING);
	Upload* u = aSource->getUpload();
	dcassert(u != nullptr);
	u->addPos(aBytes, aActual);
}

void UploadManager::on(UserConnectionListener::Failed, UserConnection* aSource, const string& aError) noexcept
{
	Upload* u = aSource->getUpload();
	
	if (u)
	{
		fire(UploadManagerListener::Failed(), u, aError);
		
		dcdebug("UM::onFailed (%s): Removing upload\n", aError.c_str());
		removeUpload(u);
	}
	
	removeConnection(aSource);
}

void UploadManager::on(UserConnectionListener::TransmitDone, UserConnection* aSource) noexcept
{
	dcassert(aSource->getState() == UserConnection::STATE_RUNNING);
	Upload* u = aSource->getUpload();
	dcassert(u != nullptr);
	u->tick(aSource->getLastActivity()); // [!] IRainman refactoring transfer mechanism
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

void UploadManager::logUpload(const Upload* u)
{
	if (BOOLSETTING(LOG_UPLOADS) && u->getType() != Transfer::TYPE_TREE && (BOOLSETTING(LOG_FILELIST_TRANSFERS) || u->getType() != Transfer::TYPE_FULL_LIST))
	{
		StringMap params;
		u->getParams(u->getUserConnection(), params);
		LOG(UPLOAD, params);
	}
	
	fire(UploadManagerListener::Complete(), u);
}

size_t UploadManager::addFailedUpload(const UserConnection& source, const string& file, int64_t pos, int64_t size)
{
	size_t queue_position = 0;
	
	Lock l(m_csQueue); // [+] IRainman opt.
	
	auto it = std::find_if(m_slotQueue.begin(), m_slotQueue.end(), [&](const UserPtr & u) -> bool { ++queue_position; return u == source.getUser(); });
	if (it != m_slotQueue.end())
	{
		it->setToken(source.getToken());
		for (auto fileIter = it->m_files.cbegin(); fileIter != it->m_files.cend(); ++fileIter)
		{
			if ((*fileIter)->getFile() == file)
			{
				(*fileIter)->setPos(pos);
				return queue_position;
			}
		}
	}
	
	UploadQueueItem* uqi = new UploadQueueItem(source.getHintedUser(), file, pos, size);
	if (it == m_slotQueue.end())
	{
		++queue_position;
		m_slotQueue.push_back(WaitingUser(source.getUser(), source.getToken(), uqi));
	}
	else
	{
		it->m_files.insert(uqi);
	}
	// Crash https://www.crash-server.com/Problem.aspx?ClientID=ppa&ProblemID=29270
	fire(UploadManagerListener::QueueAdd(), uqi);
	return queue_position;
}

void UploadManager::clearUserFilesL(const UserPtr& aUser)
{
	auto it = std::find_if(m_slotQueue.cbegin(), m_slotQueue.cend(), [&](const UserPtr & u)
	{
		return u == aUser;
	});
	if (it != m_slotQueue.end())
	{
		for (auto i = it->m_files.cbegin(); i != it->m_files.cend(); ++i)
		{
			fire(UploadManagerListener::QueueItemRemove(), (*i));
			(*i)->dec();
		}
		fire(UploadManagerListener::QueueRemove(), aUser);
		m_slotQueue.erase(it);
	}
}

void UploadManager::addConnection(UserConnection* p_conn)
{
#ifdef PPA_INCLUDE_IPFILTER
	if (PGLoader::getInstance()->check(p_conn->getRemoteIp()))
	{
		p_conn->error(STRING(YOUR_IP_IS_BLOCKED));
		p_conn->getUser()->setFlag(User::PG_BLOCK);
		LogManager::getInstance()->message("IPFilter: " + STRING(BLOCKED_INCOMING_CONN) + ' ' + p_conn->getRemoteIp());
		QueueManager::getInstance()->removeSource(p_conn->getUser(), QueueItem::Source::FLAG_REMOVED);
		removeConnection(p_conn);
		return;
	}
#endif
	p_conn->addListener(this);
	p_conn->setState(UserConnection::STATE_GET);
}

void UploadManager::testSlotTimeout(uint64_t aTick /*= GET_TICK()*/)
{
	FastUniqueLock l(m_csReservedSlots); // [+] IRainman opt.
	for (auto j = m_reservedSlots.cbegin(); j != m_reservedSlots.cend();)
	{
		if (j->second < aTick)  // !SMT!-S
		{
			m_reservedSlots.erase(j++);
		}
		else
		{
			++j;
		}
	}
}

void UploadManager::removeConnection(UserConnection* aSource)
{
	dcassert(aSource->getUpload() == nullptr);
	aSource->removeListener(this);
	
	// slot lost
	switch (aSource->getSlotType())
	{
		case UserConnection::STDSLOT:
			running--;
			break;
		case UserConnection::EXTRASLOT:
			extra--;
			break;
		case UserConnection::PARTIALSLOT:
			extraPartial--;
			break;
	}
	aSource->setSlotType(UserConnection::NOSLOT);
}

void UploadManager::notifyQueuedUsersL(int64_t tick)
{
	if (m_slotQueue.empty())
		return; //no users to notify
		
	size_t freeslots = getFreeSlots();
	if (freeslots > 0)
	{
		freeslots -= m_notifiedUsers.size();
		while (!m_slotQueue.empty() && freeslots > 0)
		{
			// let's keep him in the notifiedList until he asks for a file
			const WaitingUser wu = m_slotQueue.front();
			clearUserFilesL(wu.getUser());
			
			m_notifiedUsers[wu.getUser()] = tick;//[!]IRainman refactoring transfer mechanism
			
			ClientManager::getInstance()->connect(HintedUser(wu.getUser(), Util::emptyString), wu.getToken());
			
			freeslots--;
		}
	}
}

void UploadManager::on(TimerManagerListener::Minute, uint64_t aTick) noexcept
{
	UserList disconnects;
	{
#ifdef PPA_INCLUDE_DOS_GUARD
		{
			FastLock l(csDos);
			m_dos_map.clear();
		}
#endif
		// [-] Lock l(cs); [-] IRainman opt.
		testSlotTimeout(aTick);//[!] FlylinkDC
		
		{
			Lock l(m_csQueue); // [+] IRainman opt.
			
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
			Lock l(m_csUploads); // [+] IRainman opt.
			
			for (auto i = m_uploads.cbegin(); i != m_uploads.cend(); ++i)
			{
				if (Upload* u = *i)
				{
					if (u->getUser() && u->getUser()->isOnline())
					{
						u->unsetFlag(Upload::FLAG_PENDING_KICK);
						continue;
					}
					
					if (u->isSet(Upload::FLAG_PENDING_KICK))
					{
						disconnects.push_back(u->getUser());
						continue;
					}
					bool l_is_ban = false;
					if (BOOLSETTING(AUTO_KICK_NO_FAVS) && FavoriteManager::getInstance()->isFavoriteUser(u->getUser(), l_is_ban))
					{
						continue;
					}
					u->setFlag(Upload::FLAG_PENDING_KICK);
				}
			}
		}
	}
	
	for (auto i = disconnects.cbegin(); i != disconnects.cend(); ++i)
	{
		LogManager::getInstance()->message(STRING(DISCONNECTED_USER) + ' ' + Util::toString(ClientManager::getNicks((*i)->getCID(), Util::emptyString)));
		ConnectionManager::getInstance()->disconnect(*i, false);
	}
	
	int freeSlots = getFreeSlots();
	if (freeSlots != lastFreeSlots)
	{
		lastFreeSlots = freeSlots;
	}
}

void UploadManager::on(AdcCommand::GFI, UserConnection* aSource, const AdcCommand& c) noexcept
{
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
	
	const string& type = c.getParam(0);
	const string& ident = c.getParam(1);
	
	if (type == Transfer::g_type_names[Transfer::TYPE_FILE])
	{
		try
		{
			aSource->send(ShareManager::getInstance()->getFileInfo(ident));
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
	{
		int64_t l_currentSpeed = 0;//[+]IRainman refactoring transfer mechanism
		UploadList ticks;
		
		Lock l(m_csUploads); // [+] IRainman opt.
		for (auto i = m_delayUploads.cbegin(); i != m_delayUploads.cend();)
		{
			if (Upload* u = *i)
				if (++u->m_delayTime > 10)
				{
					logUpload(u);
					delete u;
					
					m_delayUploads.erase(i);
					i = m_delayUploads.cbegin();
				}
				else
				{
					++i;
				}
		}
		
		for (auto i = m_uploads.cbegin(); i != m_uploads.cend(); ++i)
		{
			Upload* u = *i;
			if (u->getPos() > 0)
			{
				ticks.push_back(u); // https://www.box.net/shared/83e9d01b2bbd06812496
				u->tick(aTick); // [!]IRainman refactoring transfer mechanism
			}
			u->getUserConnection().getSocket()->updateSocketBucket(getUserConnectionAmountL(u->getUser()));// [+] IRainman SpeedLimiter
			l_currentSpeed += u->getRunningAverage();//[+] IRainman refactoring transfer mechanism
		}
		runningAverage = l_currentSpeed; // [+] IRainman refactoring transfer mechanism
		
		if (!ticks.empty())
		{
			fire(UploadManagerListener::Tick(), ticks, aTick);//[!]IRainman refactoring transfer mechanism + uint64_t aTick
		}
	}
	
	{
		Lock l(m_csQueue); // [+] IRainman opt.
		notifyQueuedUsersL(aTick);
	}
	fire(UploadManagerListener::QueueUpdate());
	
	if (!isFireball)
	{
		if (getRunningAverage() >= 1024 * 1024)
		{
			if (fireballStartTick == 0)
			{
				if ((aTick - fireballStartTick) > 60 * 1000)
				{
					isFireball = true;
					ClientManager::infoUpdated();
				}
			}
			else
			{
				// speed dropped below 100 kB/s
				fireballStartTick = 0;
			}
		}
		if (!isFireball && !isFileServer)
		{
			if ((Util::getUpTime() > 10 * 24 * 60 * 60) && // > 10 days uptime
			        (Socket::getTotalUp() > 100ULL * 1024 * 1024 * 1024) && // > 100 GiB uploaded
			        (ShareManager::getInstance()->getSharedSize() > 1.5 * 1024 * 1024 * 1024 * 1024)) // > 1.5 TiB shared
			{
				isFileServer = true;
				ClientManager::infoUpdated();
			}
		}
	}
}

void UploadManager::on(ClientManagerListener::UserDisconnected, const UserPtr& aUser) noexcept
{
	if (!aUser->isOnline())
	{
		Lock l(m_csQueue);  // [+] IRainman opt.
		clearUserFilesL(aUser);
	}
}

void UploadManager::removeDelayUpload(const UserPtr& aUser)
{
	Lock l(m_csUploads); // [!] IRainman opt.
	for (auto i = m_delayUploads.cbegin(); i != m_delayUploads.cend(); ++i)
	{
		Upload* up = *i;
		if (aUser == up->getUser())
		{
			m_delayUploads.erase(i);
			delete up;
			break;
		}
	}
}

/**
 * Abort upload of specific file
 */
void UploadManager::abortUpload(const string& aFile, bool waiting)
{
	bool nowait = true;
	
	{
		Lock l(m_csUploads); // [!] IRainman opt.
		
		for (auto i = m_uploads.cbegin(); i != m_uploads.cend(); ++i)
		{
			Upload* u = (*i);
			
			if (u->getPath() == aFile)
			{
				u->getUserConnection().disconnect(true);
				nowait = false;
			}
		}
	}
	
	if (nowait) return;
	if (!waiting) return;
	
	for (int i = 0; i < 20 && nowait == false; i++)
	{
		Thread::sleep(250);
		{
			Lock l(m_csUploads); // [!] IRainman opt.
			
			nowait = true;
			for (auto j = m_uploads.cbegin(); j != m_uploads.cend(); ++j)
			{
				Upload* u = *j;
				
				if (u->getPath() == aFile)
				{
					dcdebug("upload %s is not removed\n", aFile.c_str());
					nowait = false;
					break;
				}
			}
		}
	}
	
	if (!nowait)
		dcdebug("abort upload timeout %s\n", aFile.c_str());
}

// !SMT!-S
time_t UploadManager::getReservedSlotTime(const UserPtr& aUser) const
{
	FastSharedLock l(m_csReservedSlots); // [!] IRainman opt.
	SlotMap::const_iterator j = m_reservedSlots.find(aUser);
	return j != m_reservedSlots.end() ? j->second : 0;
}

// !SMT!-S
void UploadManager::save() const
{
	CFlyRegistryMap values;
	{
		FastSharedLock l(m_csReservedSlots); // [!] IRainman opt.
		for (auto i = m_reservedSlots.cbegin(); i != m_reservedSlots.cend(); ++i)
		{
			values[i->first->getCID().toBase32()] = i->second;
		}
	}
	CFlylinkDBManager::getInstance()->save_registry(values, e_ExtraSlot);
}

// !SMT!-S
void UploadManager::load()
{
	CFlyRegistryMap l_values;
	CFlylinkDBManager::getInstance()->load_registry(l_values, e_ExtraSlot);
	// [-] Lock l(cs); [-] IRainman opt.
	for (auto k = l_values.cbegin(); k != l_values.cend(); ++k)
	{
		auto user = ClientManager::getUser(CID(k->first));
		FastUniqueLock l(m_csReservedSlots); // [+] IRainman opt.
		m_reservedSlots[user] = uint32_t(k->second.m_val_int64);
	}
	testSlotTimeout();
}
// http://code.google.com/p/flylinkdc/issues/detail?id=1413
void UploadQueueItem::update()
{
	setText(COLUMN_FILE, Text::toT(Util::getFileName(getFile())));
	setText(COLUMN_PATH, Text::toT(Util::getFilePath(getFile())));
	setText(COLUMN_NICK, getUser()->getLastNickT()); // [1] https://www.box.net/shared/plriwg50qendcr3kbjp5
	setText(COLUMN_HUB, Text::toT(Util::toString(ClientManager::getHubNames(getHintedUser().user->getCID(), Util::emptyString))));
	setText(COLUMN_TRANSFERRED, Util::formatBytesW(getPos()) + _T(" (") + Util::toStringW((double)getPos() * 100.0 / (double)getSize()) + _T("%)"));
	setText(COLUMN_SIZE, Util::formatBytesW(getSize()));
	setText(COLUMN_ADDED, Text::toT(Util::formatDigitalClock(getTime())));
	setText(COLUMN_WAITING, Util::formatSecondsW(GET_TIME() - getTime()));
	setText(COLUMN_SHARE, Util::formatBytesW(getUser()->getBytesShared())); //[+]PPA
	setText(COLUMN_SLOTS, Util::toStringW(getUser()->getSlots())); //[+]PPA
	// !SMT!-IP
	if (!m_location.isSet() && !getUser()->getIP().empty()) // [!] IRainman opt: Prevent multiple repeated requests to the database if the location has not been found!
	{
		m_location = Util::getIpCountry(getUser()->getIP());
		setText(COLUMN_IP, Text::toT(getUser()->getIP()));
	}
	if (m_location.isKnown())
	{
		setText(COLUMN_LOCATION, m_location.getDescription());
	}
#ifdef PPA_INCLUDE_DNS
	// [!] IRainman opt.
	if (m_dns.empty())
	{
		m_dns = Socket::nslookup(m_ip);
		setText(COLUMN_DNS, Text::toT(m_dns)); // todo: paint later if not resolved yet
	}
	// [~] IRainman opt.
#endif
}

/**
 * @file
 * $Id: UploadManager.cpp 581 2011-11-02 18:59:46Z bigmuscle $
 */
