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

#pragma once


#ifndef USERINFOBASE_H
#define USERINFOBASE_H

#include "UserManager.h"
#include "FavoriteManager.h"

class UserInfoBase
#ifdef _DEBUG
	: private boost::noncopyable // [+] IRainman fix.
#endif
{
	public:
		UserInfoBase() { }
		virtual ~UserInfoBase() { }
		
		void getList();
		void browseList();
		
		void getUserResponses(); // [+] SSA
#ifdef IRAINMAN_INCLUDE_USER_CHECK
		void checkList();
#endif
		void matchQueue();
		
		void doReport(const string& hubHint);
		
#ifdef FLYLINKDC_USE_SQL_EXPLORER
		void browseSqlExplorer(const string& hubHint);
#endif
		
		void pm(const string& hubHint);
		void pm_msg(const string& hubHint, const tstring& p_message); // !SMT!-S
		
		void createSummaryInfo(const string& p_selectedHint);// [+] IRainman
		
		void grantSlotPeriod(const string& hubHint, const uint64_t period); // !SMT!-UI
		void ungrantSlot(const string& hubHint); // [!] IRainman fix: add hubhint.
		void addFav();
		void delFav();
		void setUploadLimit(const int limit);
		void setIgnorePM();
		void setFreePM();
		void setNormalPM();
		void ignoreOrUnignoreUserByName(); // [!] IRainman moved from gui and clean.
		void removeAll();
		void connectFav();
		
		virtual const UserPtr& getUser() const = 0;
		static uint8_t getImage(const OnlineUser& ou); // [!] IRainman fix: use online user here!
		static uint8_t getStateImageIndex()
		{
			return 0;
		}
		
};

struct FavUserTraits // [!] IRainman moved from WinUtil and review.
{
	FavUserTraits() :
		isEmpty(true),
#ifndef IRAINMAN_ALLOW_ALL_CLIENT_FEATURES_ON_NMDC
		adcOnly(true),
#endif
		isFav(false),
		isAutoGrantSlot(false),
		uploadLimit(0),
		isIgnoredPm(false), isFreePm(false),
		isIgnoredByName(false)
	{
	}
	void init(const UserInfoBase& ui);
	
	int uploadLimit;
	
#ifndef IRAINMAN_ALLOW_ALL_CLIENT_FEATURES_ON_NMDC
	bool adcOnly;
#endif
	bool isAutoGrantSlot;
	bool isFav;
	bool isEmpty;
	bool isIgnoredPm;
	bool isFreePm;
	bool isIgnoredByName;
};

#endif