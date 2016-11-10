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


#ifndef DCPLUSPLUS_DCPP_CONNECTIVITY_MANAGER_H
#define DCPLUSPLUS_DCPP_CONNECTIVITY_MANAGER_H

#include "Util.h"
#include "Speaker.h"
#include "Singleton.h"

#ifdef FLYLINKDC_USE_DEAD_CODE
class ConnectivityManagerListener
{
	public:
		virtual ~ConnectivityManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Message;
		typedef X<1> Started;
		typedef X<2> Finished;
		typedef X<3> SettingChanged; // auto-detection has been enabled / disabled
		
		virtual void on(Message, const string&) noexcept { }
		virtual void on(Started) noexcept { }
		virtual void on(Finished) noexcept { }
		virtual void on(SettingChanged) noexcept { }
};
#endif // FLYLINKDC_USE_DEAD_CODE

class ConnectivityManager : public Singleton<ConnectivityManager>
#ifdef FLYLINKDC_USE_DEAD_CODE
	, public Speaker<ConnectivityManagerListener>
#endif
{
	public:
		void detectConnection();
		void setup_connections(bool settingsChanged);
		static void test_all_ports();
		
	private:
		friend class Singleton<ConnectivityManager>;
		friend class MappingManager;
		
		ConnectivityManager();
		virtual ~ConnectivityManager() { }
		
		static void mappingFinished(const string& mapper);
		static void log(const string& msg);
		
        static string getInformation();
        static const string& getStatus()
        {
            return g_status;
        }
        void startSocket();
		void listen();
		void disconnect();
		
		bool autoDetected;
		static bool g_is_running;
		
		static string g_status;
};

#endif // !defined(CONNECTIVITY_MANAGER_H)
