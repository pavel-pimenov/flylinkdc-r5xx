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

#ifndef DCPLUSPLUS_DCPP_LOG_MANAGER_H
#define DCPLUSPLUS_DCPP_LOG_MANAGER_H

#include "Util.h"

//#define FMT_HEADER_ONLY
//#include "../cppformat/format.h"
//#include "dcformat.h"

typedef std::unordered_map<string, string> CFlyMessagesBuffer;
class LogMessage
{
	public:
		enum Severity
		{
			SEV_NOTIFY, // Messages with this severity won't be saved to system log, only the event is fired
			SEV_INFO,
			SEV_WARNING,
			SEV_ERROR,
			SEV_LAST
		};
};
class LogManager
{
	public:
		enum LogArea { CHAT, PM, DOWNLOAD, UPLOAD, SYSTEM, STATUS,
		               WEBSERVER,
#ifdef RIP_USE_LOG_PROTOCOL
		               PROTOCOL,
#endif
		               CUSTOM_LOCATION, // [+] IRainman
		               TRACE_SQLITE,
		               VIRUS_TRACE,
		               DDOS_TRACE,
		               CMDDEBUG_TRACE,
		               PSR_TRACE,
		               FLOOD_TRACE,
		               TORRENT_TRACE,
		               LAST
		             };
		             
		enum {FILE, FORMAT};
		static void init();
		static void ddos_message(const string& params);
		static void virus_message(const string& params);
		static void flood_message(const string& params);
		static void cmd_debug_message(const string& params);
		static void torrent_message(const string& params, bool p_is_add_sys_message = true);
		static void psr_message(const string& params);
		static void log(LogArea area, const StringMap& params, bool p_only_file = false) noexcept;
		static void message(const string& msg, bool p_only_file = false);
		static void message(const string& msg, LogMessage::Severity)
		{
			message(msg);
		}
		
		static const string& getSetting(int area, int sel);
		static void saveSetting(int area, int sel, const string& setting);
		
		static HWND g_mainWnd;
		static bool g_isLogSpeakerEnabled;
		static int  g_LogMessageID;
		static void flush_all_log();
	private:
		static void flush_file(const string& p_area, const string& p_msg);
		static void log(const string& p_area, const string& p_msg) noexcept;
		
		static int g_logOptions[LAST][2];
#ifdef _DEBUG
		static boost::unordered_map<string, pair<string, size_t> > g_pathCache;
		static size_t g_debugTotal;
		static size_t g_debugMissed;
		static int g_debugParallelWritesFiles;
#else
		static boost::unordered_map<string, string> g_pathCache;
#endif
		static FastCriticalSection g_csPathCache; // [!] IRainman opt: use spin lock here.
		static bool g_isInit;
		
		static CFlyMessagesBuffer g_LogFilesBuffer;
		static FastCriticalSection g_csLogFilesBuffer;
		static std::map<std::string, FastCriticalSection> g_csFile; // map - для исключения инвалидации
		static FastCriticalSection g_csFileArea;
		
		LogManager();
		~LogManager()
		{
#ifdef _DEBUG
			dcdebug("LogManager: path cache stats: total found = %i, missed = %i, current size = %i\n"
			        , g_debugTotal, g_debugMissed, g_pathCache.size()
			       ); //-V111
#endif
		}
		
};

#define LOG(area, msg)  LogManager::log(LogManager::area, msg)
#define LOG_FORCE_FILE(area, msg)  LogManager::log(LogManager::area, msg, true)

class CFlyLog
{
	public:
		const string m_message;
		const uint64_t m_start;
		uint64_t m_tc;
		bool m_skip_start;
		bool m_skip_stop;
		bool m_only_file;
		void log(const string& p_msg)
		{
			LogManager::message(p_msg, m_only_file);
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
		explicit CFlyLogFile(const string& p_message) : CFlyLog(p_message, true, true)
		{
		}
};

#endif // !defined(LOG_MANAGER_H)

/**
 * @file
 * $Id: LogManager.h 568 2011-07-24 18:28:43Z bigmuscle $
 */
