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

#ifndef DCPLUSPLUS_DCPP_LOG_MANAGER_H
#define DCPLUSPLUS_DCPP_LOG_MANAGER_H

#include "Singleton.h"
#include "Speaker.h"
#include "Util.h"

class LogManagerListener
{
	public:
		virtual ~LogManagerListener() { }
		template<int I> struct X
		{
			enum { TYPE = I };
		};
		
		typedef X<0> Message;
		virtual void on(Message, const string&) noexcept { }
};

class LogManager : public Singleton<LogManager>, public Speaker<LogManagerListener>
{
	public:
		enum LogArea { CHAT, PM, DOWNLOAD, UPLOAD, SYSTEM, STATUS,
		               WEBSERVER,
#ifdef RIP_USE_LOG_PROTOCOL
		               PROTOCOL,
#endif
		               CUSTOM_LOCATION, // [+] IRainman
		               TRACE_SQLITE,
		               DDOS_TRACE,
		               DHT_TRACE,
		               PSR_TRACE,
		               FLOOD_TRACE,
		               CMDDEBUG_TRACE,
		               LAST
		             };
		enum {FILE, FORMAT};
		void ddos_message(const string& params);
		void flood_message(const string& params);
		void cmd_debug_message(const string& params);
		void dht_message(const string& params);
		void psr_message(const string& params);
		void log(LogArea area, const StringMap& params, bool p_only_file = false) noexcept;
		void message(const string& msg, bool p_only_file = false);
		
		const string& getSetting(int area, int sel) const;
		void saveSetting(int area, int sel, const string& setting);
		
	private:
		void log(const string& p_area, const string& p_msg) noexcept;
		
		friend class Singleton<LogManager>;
		
		int logOptions[LAST][2];
#ifdef _DEBUG
		boost::unordered_map<string, pair<string, size_t> > m_patchCache;
		size_t _debugTotal, _debugMissed;
#else
		boost::unordered_map<string, string> m_patchCache;
#endif
		FastCriticalSection m_csPatchCache; // [!] IRainman opt: use spin lock here.
		
#ifdef IRAINMAN_USE_NG_LOG_MANAGER
		std::unordered_set<string> m_currentlyOpenedFiles;
		FastCriticalSection m_csCurrentlyOpenedFiles;
		dcdrun(int _debugConcurrencyEventCount; int _debugParallelWritesFiles;)
#endif
		
		LogManager();
		~LogManager()
		{
#ifdef _DEBUG
			dcdebug("LogManager: path cache stats: total found = %i, missed = %i, current size = %i\n"
#ifdef IRAINMAN_USE_NG_LOG_MANAGER
			        "LogManager: parallel writes files = %i, concurrency event count = %i\n"
#endif
			        , _debugTotal, _debugMissed, m_patchCache.size()
#ifdef IRAINMAN_USE_NG_LOG_MANAGER
			        , _debugParallelWritesFiles, _debugConcurrencyEventCount
#endif
			       ); //-V111
#endif
		}
		
};

#define LOG(area, msg)  LogManager::getInstance()->log(LogManager::area, msg)
#define LOG_FORCE_FILE(area, msg)  LogManager::getInstance()->log(LogManager::area, msg, true)

class CFlyLog
{
	public:
		const string m_message;
		const uint64_t m_start;
		uint64_t m_tc;
		bool m_skip_start;
		bool m_only_file;
		void log(const string& p_msg)
		{
			LogManager::getInstance()->message(p_msg, m_only_file);
		}
	public:
		CFlyLog(const string& p_message
		        , bool p_skip_start = true
		                              , bool p_only_file  = false
		       );
		~CFlyLog();
		uint64_t calcSumTime() const;
		void step(const string& p_message_step, const bool p_reset_count = true);
		void loadStep(const string& p_message_step, const bool p_reset_count = true);
};
class CFlyLogFile : public CFlyLog
{
	public:
		CFlyLogFile(const string& p_message) : CFlyLog(p_message, true, true)
		{
		}
};

#endif // !defined(LOG_MANAGER_H)

/**
 * @file
 * $Id: LogManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
