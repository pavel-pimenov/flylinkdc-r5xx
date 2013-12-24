
/*
 * ApexDC speedmod (c) SMT 2007
 */

#include "stdinc.h"
#include "DCPlusPlus.h"
#include "Util.h"
#include "ShareManager.h"
#include "QueueManager.h"
#include "UploadManager.h"
#include "UserInfoBase.h"

void UserInfoBase::matchQueue()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->addList(getUser(), QueueItem::FLAG_MATCH_QUEUE);
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message(e.getError());
		}
	}
}

void UserInfoBase::getUserResponses()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->addCheckUserIP(getUser()); // [+] SSA
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message(e.getError());
		}
	}
}

void UserInfoBase::checkList()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->addList(getUser(), QueueItem::FLAG_USER_CHECK);
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message(e.getError());
		}
	}
}

void UserInfoBase::doReport(const string& hubHint)
{
	if (getUser())
	{
		ClientManager::getInstance()->reportUser(HintedUser(getUser(), hubHint));
	}
}

void UserInfoBase::getList()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->addList(getUser(), QueueItem::FLAG_CLIENT_VIEW);
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message(e.getError());
		}
	}
}

void UserInfoBase::browseList()
{
	if (getUser())
	{
		try
		{
			QueueManager::getInstance()->addList(getUser(), QueueItem::FLAG_CLIENT_VIEW | QueueItem::FLAG_PARTIAL_LIST);
		}
		catch (const Exception& e)
		{
			LogManager::getInstance()->message(e.getError());
		}
	}
}

void UserInfoBase::addFav()
{
	if (getUser())
	{
		FavoriteManager::getInstance()->addFavoriteUser(getUser());
	}
}

void UserInfoBase::setIgnorePM() // [+] SSA.
{
	if (getUser())
	{
		FavoriteManager::getInstance()->setIgnorePM(getUser(), true);
	}
}

void UserInfoBase::setFreePM() // [+] IRainman.
{
	if (getUser())
	{
		FavoriteManager::getInstance()->setFreePM(getUser(), true);
	}
}

void UserInfoBase::setNormalPM() // [+] IRainman.
{
	if (getUser())
	{
		FavoriteManager::getInstance()->setNormalPM(getUser());
	}
}

void UserInfoBase::setUploadLimit(const int limit) // [+] IRainman.
{
	if (getUser())
	{
		FavoriteManager::getInstance()->setUploadLimit(getUser(), limit);
	}
}

void UserInfoBase::delFav()
{
	if (getUser())
	{
		FavoriteManager::getInstance()->removeFavoriteUser(getUser());
	}
}

void UserInfoBase::ignoreOrUnignoreUserByName() // [!] IRainman moved from gui and clean
{
	if (getUser())
	{
		const auto& nick = getUser()->getLastNick();
		if (UserManager::isInIgnoreList(nick))
		{
			UserManager::removeFromIgnoreList(nick);
		}
		else
		{
			UserManager::addToIgnoreList(nick);
		}
	}
}

void UserInfoBase::pm(const string& hubHint)
{
	if (getUser() && !ClientManager::isMe(getUser())) // [!] FlylinkDC do not send messages to themselves.
	{
		UserManager::getInstance()->outgoingPrivateMessage(getUser(), hubHint, Util::emptyStringT);
	}
}

// !SMT!-S
void UserInfoBase::pm_msg(const string& hubHint, const tstring& p_message)
{
	if (!p_message.empty()) // [~] SCALOlaz: support for abolition and prohibition to send a blank line https://code.google.com/p/flylinkdc/issues/detail?id=1034
	{
		UserManager::getInstance()->outgoingPrivateMessage(getUser(), hubHint, p_message);
	}
}

void UserInfoBase::createSummaryInfo() // [+] IRainman
{
	if (getUser())
	{
		UserManager::getInstance()->collectSummaryInfo(getUser());
	}
}

void UserInfoBase::removeAll()
{
	if (getUser())
	{
		QueueManager::getInstance()->removeSource(getUser(), QueueItem::Source::FLAG_REMOVED);
	}
}

void UserInfoBase::connectFav()
{
	if (getUser())
	{
		UserManager::getInstance()->openUserUrl(getUser());
	}
}

// !SMT!-UI
void UserInfoBase::grantSlotPeriod(const string& hubHint, const uint64_t period)
{
	if (period && getUser())
		UploadManager::getInstance()->reserveSlot(HintedUser(getUser(), hubHint), period);
}

void UserInfoBase::ungrantSlot(const string& hubHint) // [!] IRainman fix: add hubhint.
{
	if (getUser())
	{
		UploadManager::getInstance()->unreserveSlot(HintedUser(getUser(), hubHint));
	}
}

// [->] IRainman moved from User.cpp
uint8_t UserInfoBase::getImage(const OnlineUser& ou)
{
#ifdef _DEBUG
//	static int g_count = 0;
//	dcdebug("UserInfoBase::getImage count = %d ou = %p\n", ++g_count, &ou);
#endif
	const auto& id = ou.getIdentity();
	const auto& u = ou.getUser();
	uint8_t image;
	if (id.isOp())
	{
		image = 0;
	}
	else if (u->isServer())
	{
		image = 1;
	}
	else
	{
		auto speed = id.getLimit();
		if (!speed)
		{
			speed = id.getDownloadSpeed();
		}
		/*
		if (!speed)
		{
		    // TODO: needs icon for user with no limit?
		}
		*/
		if (speed >= 10 * 1024 * 1024) // over 10 MB
		{
			image = 2;
		}
		else if (speed > 1024 * 1024 / 10) // over 100 KB
		{
			image = 3;
		}
		else
		{
			image = 4; //-V112
		}
	}
	
	if (u->isAway())
	{
		//User away...
		image += 5;
	}
	
	/* TODO const string freeSlots = identity.get("FS");
	if(!freeSlots.empty() && identity.supports(AdcHub::ADCS_FEATURE) && identity.supports(AdcHub::SEGA_FEATURE) &&
	    ((identity.supports(AdcHub::TCP4_FEATURE) && identity.supports(AdcHub::UDP4_FEATURE)) || identity.supports(AdcHub::NAT0_FEATURE)))
	{
	    image += 10;
	}*/
	
	if (!id.isTcpActive(&ou.getClient()))
	{
		// Users we can't connect to...
		image += 10;// 20
	}
	return image;
}
#ifdef FLYLINKDC_USE_SQL_EXPLORER
void UserInfoBase::browseSqlExplorer(const string& hubHint)
{
	if (getUser())
	{
		UserManager::getInstance()->browseSqlExplorer(getUser(), hubHint);
	}
}
#endif