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

#pragma once


#ifndef DCPLUSPLUS_DCPP_HUBENTRY_H_
#define DCPLUSPLUS_DCPP_HUBENTRY_H_


#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
#include "TimerManager.h"

class ConnectionStatus
{
	public:
		enum Status
		{
			UNKNOWN = -1,
			SUCCES = 0,
			CONNECTION_FAILURE,
			// TODO?
			//DNS_FAILURE,
			//TIMEOUT,
			//REDIRECT,
			//BANNED,
			//MANY_HUBS,
			//BAD_NICKNAME,
		};
		
		explicit ConnectionStatus() : status(UNKNOWN), lastattempts(0), lastsucces(0)
		{
		}
		
		void setStatus(Status NewStatus)
		{
			status = NewStatus;
			const time_t l_cur = GET_TIME();
			setLastAttempts(l_cur);
			
			if (NewStatus == SUCCES)
				setLastSucces(l_cur);
				
		}
		
		void setSavedStatus(int NewStatus, time_t p_lastattempts, time_t p_lastsucces)
		{
			status = Status(NewStatus);
			setLastAttempts(p_lastattempts);
			setLastSucces(p_lastsucces);
		}
		
		Status getStatus() const
		{
			return status;
		}
		
		tstring getStatusText() const
		{
			switch (status)
			{
				case SUCCES:
					return TSTRING(SUCCESSFULLY);
				case CONNECTION_FAILURE:
					return TSTRING(FAILED_TO_CONNECT);
					// TODO?
					//case DNS_FAILURE:
					//  return "Domain name not resolved";
					//case TIMEOUT:
					//  return "Timeout";
					//case REDIRECT:
					//  return "Your redirect to " /*add link?*/;
					//case BANNED:
					//  return "You are banned :(";
					//case MANY_HUBS:
					//  return "Too many hubs";
					//case BAD_NICKNAME:
					//  return "Bad nickname";
				default:
					return TSTRING(UNKNOWN);
			}
		}
		
		~ConnectionStatus() noexcept { }
		
	private:
		GETSET(time_t, lastsucces, LastSucces);
		GETSET(time_t, lastattempts, LastAttempts);
		Status status;
};
#endif // IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS


class HubEntry
#ifdef _DEBUG
	// TODO : boost::noncopyable
#endif
{
	public:
		typedef deque<HubEntry> List; // [!] IRainman opt: change vector to deque
		
		HubEntry() : reliability(0), shared(0), minShare(0), users(0), minSlots(0), maxHubs(0), maxUsers(0)
		{
		}
		HubEntry(const string& aName, const string& aServer, const string& aDescription, const string& aUsers, const string& aCountry,
		         const string& aShared, const string& aMinShare, const string& aMinSlots, const string& aMaxHubs, const string& aMaxUsers,
		         const string& aReliability, const string& aRating) : name(aName),
			server(Util::formatDchubUrl(aServer)), // [!] IRainman fix.
			description(aDescription), country(aCountry),
			rating(aRating), reliability((float)(Util::toFloat(aReliability) / 100.0)), shared(Util::toInt64(aShared)), minShare(Util::toInt64(aMinShare)),
			users(Util::toInt(aUsers)), minSlots(Util::toInt(aMinSlots)), maxHubs(Util::toInt(aMaxHubs)), maxUsers(Util::toInt(aMaxUsers))
		{
		}
#ifdef FLYLINKDC_USE_DEAD_CODE
	HubEntry(const string& aName, const string& aServer, const string& aDescription, const string& aUsers) noexcept :
		name(aName), server(aServer), description(aDescription),
		reliability(0.0), shared(0), minShare(0), users(Util::toInt(aUsers)), minSlots(0), maxHubs(0), maxUsers(0) { }
		
		
	HubEntry(const HubEntry& rhs) noexcept :
		name(rhs.name), server(rhs.server), description(rhs.description), country(rhs.country),
		rating(rhs.rating), reliability(rhs.reliability), shared(rhs.shared), minShare(rhs.minShare), users(rhs.users), minSlots(rhs.minSlots),
		maxHubs(rhs.maxHubs), maxUsers(rhs.maxUsers) { }
#endif
		~HubEntry() noexcept { }
		
		GETSET(string, name, Name);
		GETSET(string, server, Server);
		GETSET(string, description, Description);
		GETSET(string, country, Country);
		GETSET(string, rating, Rating);
		GETSET(float, reliability, Reliability);
		GETSET(int64_t, shared, Shared);
		GETSET(int64_t, minShare, MinShare);
		GETSET(int, users, Users);
		GETSET(int, minSlots, MinSlots);
		GETSET(int, maxHubs, MaxHubs);
		GETSET(int, maxUsers, MaxUsers);
};

class FavoriteHubEntry
#ifdef _DEBUG
	// TODO : boost::noncopyable
#endif
{
	public:
		typedef vector<FavoriteHubEntry*> List;
		
	FavoriteHubEntry() noexcept :
		connect(false), encoding(Text::g_systemCharset), windowposx(0), windowposy(0), windowsizex(0),
		        windowsizey(0), windowtype(0), chatusersplit(0),
		        userliststate(true),
		        m_ISPDisableFlylinkDCSupportHub(false),
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		        chatusersplitstate(true),
#endif
		        hideShare(false),
		        exclusiveHub(false), showJoins(false), exclChecks(false), mode(0),
		        searchInterval(SETTING(MINIMUM_SEARCH_INTERVAL)), overrideId(0),
		        headerSort(-1), headerSortAsc(true), suppressChatAndPM(false),
		        autobanAntivirusIP(false), autobanAntivirusNick(false)
		{
		} // !SMT!-S
		virtual ~FavoriteHubEntry() noexcept { }
		
		const string getNick(bool useDefault = true) const
		{
			return (!m_nick.empty() || !useDefault) ? m_nick : SETTING(NICK);
		}
		
		void setNick(const string& aNick)
		{
			m_nick = aNick;
		}
		
		GETSET(string, userdescription, UserDescription);
		GETSET(string, awaymsg, AwayMsg);
		GETSET(string, email, Email);
		GETSET(string, name, Name);
		GETSET(string, server, Server);
		GETSET(string, description, Description);
		GETSET(string, password, Password);
		GETSET_BOOL(string, headerOrder, HeaderOrder);
		GETSET_BOOL(string, headerWidths, HeaderWidths);
		GETSET_BOOL(string, headerVisible, HeaderVisible);
		GETSET_BOOL(int, headerSort, HeaderSort);
		GETSET_BOOL(bool, headerSortAsc, HeaderSortAsc);
		GETSET(bool, connect, Connect);
		GETSET_BOOL(int, windowposx, WindowPosX);
		GETSET_BOOL(int, windowposy, WindowPosY);
		GETSET_BOOL(int, windowsizex, WindowSizeX);
		GETSET_BOOL(int, windowsizey, WindowSizeY);
		GETSET_BOOL(int, windowtype, WindowType);
		GETSET_BOOL(int, chatusersplit, ChatUserSplit);
		GETSET_BOOL(bool, userliststate, UserListState);
		GETSET(bool, m_ISPDisableFlylinkDCSupportHub, ISPDisableFlylinkDCSupportHub);
#ifdef SCALOLAZ_HUB_SWITCH_BTN
		GETSET(bool, chatusersplitstate, ChatUserSplitState);
#endif
		GETSET(bool, hideShare, HideShare); // Save paramethers always IRAINMAN_INCLUDE_HIDE_SHARE_MOD
		GETSET(bool, showJoins, ShowJoins);
		GETSET(bool, autobanAntivirusIP, AutobanAntivirusIP);
		GETSET(bool, autobanAntivirusNick, AutobanAntivirusNick);
		GETSET(bool, exclChecks, ExclChecks); // Excl. from client checking
		GETSET(bool, exclusiveHub, ExclusiveHub); // Exclusive Hub Mod
		GETSET(bool, suppressChatAndPM, SuppressChatAndPM);
		GETSET(string, rawOne, RawOne);
		GETSET(string, rawTwo, RawTwo);
		GETSET(string, rawThree, RawThree);
		GETSET(string, rawFour, RawFour);
		GETSET(string, rawFive, RawFive);
		GETSET(int, mode, Mode); // 0 = default, 1 = active, 2 = passive
		GETSET(string, ip, IP);
		GETSET(string, opChat, OpChat);
		// [!] IRainman mimicry function
		GETSET(string, clientName, ClientName);
		GETSET(string, clientVersion, ClientVersion);
		GETSET(bool, overrideId, OverrideId); // !SMT!-S
		// [~] IRainman mimicry function
		
		GETSET(string, antivirusCommandIP, AntivirusCommandIP);
		
		GETSET(uint32_t, searchInterval, SearchInterval);
		GETSET(string, encoding, Encoding);
		GETSET(string, group, Group);
		
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		const ConnectionStatus& getConnectionStatus() const
		{
			return connectionStatus;
		}
		void setConnectionStatus(ConnectionStatus::Status p_status)
		{
			connectionStatus.setStatus(p_status);
		}
		void setSavedConnectionStatus(int p_status, time_t p_lastattempts, time_t p_lastsucces)
		{
			connectionStatus.setSavedStatus(p_status, p_lastattempts, p_lastsucces);
		}
#endif // IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		
	private:
		string m_nick;
#ifdef IRAINMAN_ENABLE_CON_STATUS_ON_FAV_HUBS
		ConnectionStatus connectionStatus;
#endif
};

class RecentHubEntry
{
	public:
		typedef RecentHubEntry* Ptr;
		typedef vector<Ptr> List;
		typedef List::const_iterator Iter;
		
		explicit RecentHubEntry() : name("*"),
			description("*"),
			users("*"),
			shared("*"),
			lastseen("*"),
			opentab("-"),
			autoopen(false),
			redirect(false) {}
		~RecentHubEntry() { }
		
		GETSET(string, name, Name);
		GETSET(string, server, Server);
		GETSET(string, description, Description);
		GETSET(string, users, Users);
		GETSET(string, shared, Shared);
		GETSET(string, lastseen, LastSeen);
		GETSET(string, opentab, OpenTab);
		GETSET(bool, autoopen, AutoOpen);
		GETSET(bool, redirect, Redirect);
};

#endif /*HUBENTRY_H_*/
