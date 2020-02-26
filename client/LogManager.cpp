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
#include "LogManager.h"
#include "CFlylinkDBManager.h"
#include "CompatibilityManager.h"
#include "TimerManager.h"
#include "ClientManager.h"

#ifdef _DEBUG
std::unordered_map<string, pair<string, size_t> > LogManager::g_pathCache;
size_t LogManager::g_debugTotal;
size_t LogManager::g_debugMissed;
#else
std::unordered_map<string, string> LogManager::g_pathCache;
#endif
bool LogManager::g_isInit = false;
int LogManager::g_logOptions[LAST][2];
FastCriticalSection LogManager::g_csPathCache;
std::map<std::string, FastCriticalSection> LogManager::g_csFile;
FastCriticalSection LogManager::g_csFileArea;
CFlyMessagesBuffer LogManager::g_LogFilesBuffer;
FastCriticalSection LogManager::g_csLogFilesBuffer;
HWND LogManager::g_mainWnd = nullptr;
int  LogManager::g_LogMessageID = 0;
bool LogManager::g_isLogSpeakerEnabled = false;

void LogManager::init()
{
	g_logOptions[UPLOAD][FILE]        = SettingsManager::LOG_FILE_UPLOAD;
	g_logOptions[UPLOAD][FORMAT]      = SettingsManager::LOG_FORMAT_POST_UPLOAD;
	g_logOptions[DOWNLOAD][FILE]      = SettingsManager::LOG_FILE_DOWNLOAD;
	g_logOptions[DOWNLOAD][FORMAT]    = SettingsManager::LOG_FORMAT_POST_DOWNLOAD;
	g_logOptions[CHAT][FILE]          = SettingsManager::LOG_FILE_MAIN_CHAT;
	g_logOptions[CHAT][FORMAT]        = SettingsManager::LOG_FORMAT_MAIN_CHAT;
	g_logOptions[PM][FILE]            = SettingsManager::LOG_FILE_PRIVATE_CHAT;
	g_logOptions[PM][FORMAT]          = SettingsManager::LOG_FORMAT_PRIVATE_CHAT;
	g_logOptions[SYSTEM][FILE]        = SettingsManager::LOG_FILE_SYSTEM;
	g_logOptions[SYSTEM][FORMAT]      = SettingsManager::LOG_FORMAT_SYSTEM;
	g_logOptions[STATUS][FILE]        = SettingsManager::LOG_FILE_STATUS;
	g_logOptions[STATUS][FORMAT]      = SettingsManager::LOG_FORMAT_STATUS;
#ifdef RIP_USE_LOG_PROTOCOL
	g_logOptions[PROTOCOL][FILE]      = SettingsManager::LOG_FILE_PROTOCOL;
	g_logOptions[PROTOCOL][FORMAT]    = SettingsManager::LOG_FORMAT_PROTOCOL;
#endif
	g_logOptions[CUSTOM_LOCATION][FILE]     = SettingsManager::LOG_FILE_CUSTOM_LOCATION;
	g_logOptions[CUSTOM_LOCATION][FORMAT]   = SettingsManager::LOG_FORMAT_CUSTOM_LOCATION;
	
	g_logOptions[TRACE_SQLITE][FILE]     = SettingsManager::LOG_FILE_TRACE_SQLITE;
	g_logOptions[TRACE_SQLITE][FORMAT]   = SettingsManager::LOG_FORMAT_TRACE_SQLITE;
	g_logOptions[VIRUS_TRACE][FILE] = SettingsManager::LOG_FILE_VIRUS_TRACE;
	g_logOptions[VIRUS_TRACE][FORMAT] = SettingsManager::LOG_FORMAT_VIRUS_TRACE;
	g_logOptions[DDOS_TRACE][FILE] = SettingsManager::LOG_FILE_DDOS_TRACE;
	g_logOptions[DDOS_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_DDOS_TRACE;
	g_logOptions[CMDDEBUG_TRACE][FILE]     = SettingsManager::LOG_FILE_CMDDEBUG_TRACE;
	g_logOptions[CMDDEBUG_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_CMDDEBUG_TRACE;
	g_logOptions[PSR_TRACE][FILE]     = SettingsManager::LOG_FILE_PSR_TRACE;
	g_logOptions[PSR_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_PSR_TRACE;
	g_logOptions[FLOOD_TRACE][FILE]     = SettingsManager::LOG_FILE_FLOOD_TRACE;
	g_logOptions[FLOOD_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_FLOOD_TRACE;
	g_logOptions[TORRENT_TRACE][FILE] = SettingsManager::LOG_FILE_TORRENT_TRACE;
	g_logOptions[TORRENT_TRACE][FORMAT] = SettingsManager::LOG_FORMAT_TORRENT_TRACE;
	
	g_isInit = true;
	
	if (!CompatibilityManager::getStartupInfo().empty())
	{
		message(CompatibilityManager::getStartupInfo());
	}
	
	if (CompatibilityManager::isIncompatibleSoftwareFound())
	{
		message(CompatibilityManager::getIncompatibleSoftwareMessage());
	}
}

LogManager::LogManager()
{
	flush_all_log();
}

void LogManager::log(const string& p_area, const string& p_msg) noexcept
{
	dcassert(g_isInit);
	if (!g_isInit)
		return;
	string l_area;
	bool l_is_new_path;
	{
		CFlyFastLock(g_csPathCache);
		const auto l_fine_it = g_pathCache.find(p_area);
		l_is_new_path = l_fine_it == g_pathCache.end();
		if (l_is_new_path)
		{
			if (g_pathCache.size() > 250)
			{
				g_pathCache.clear();
			}
			
			l_area = Util::validateFileName(p_area);
#ifdef _DEBUG
			g_debugMissed++;
			const std::pair<string, size_t> l_pair(l_area, 0);
			g_pathCache[p_area] = l_pair;
#else
			g_pathCache[p_area] = l_area;
#endif
		}
		else
		{
#ifdef _DEBUG
			g_debugTotal++;
			if (++l_fine_it->second.second % 100 == 0)
			{
				// dcdebug("log path cache: size=%i [%s] %i\n", g_pathCache.size(), p_area.c_str(), l_fine_it->second.second);
			}
			
			l_area = l_fine_it->second.first;
#else
			l_area = l_fine_it->second;
#endif
		}
	}
	{
		CFlyFastLock(g_csLogFilesBuffer);
		string l_msg;
#ifdef _DEBUG
		if (ClientManager::isStartup())
			l_msg += "[init]";
		if (ClientManager::isBeforeShutdown())
			l_msg += "[before_shutdown]";
		if (ClientManager::isShutdown())
			l_msg += "[shutdown]";
		l_msg += "[" + Util::toString(::GetCurrentThreadId()) + "]";
#endif
		l_msg += p_msg + "\r\n";
		auto l_log = g_LogFilesBuffer.insert(make_pair(l_area, l_msg));
		if (l_log.second == false)
		{
			l_log.first->second += l_msg;
		}
	}
	dcassert(!l_area.empty());
	if (l_is_new_path)
	{
		File::ensureDirectory(l_area);
	}
#ifndef _DEBUG
	if (ClientManager::isStartup() == true)
#endif
	{
		flush_all_log();
	}
}
void LogManager::flush_all_log()
{
	CFlyMessagesBuffer l_buffer;
	{
		CFlyFastLock(g_csLogFilesBuffer);
		l_buffer.swap(g_LogFilesBuffer);
	}
	for (auto i = l_buffer.cbegin(); i != l_buffer.cend(); ++i)
	{
		try
		{
			flush_file(i->first, i->second);
		}
		catch (const FileException& e)
		{
			const auto l_code = GetLastError();
			if (l_code == 3) // ERROR_PATH_NOT_FOUND
			{
				try
				{
					File::ensureDirectory(i->first);
					flush_file(i->first, i->second);
				}
				catch (const FileException& e)
				{
					dcassert(0);
				}
			}
			else
			{
				dcassert(0);
			}
		}
	}
}

void LogManager::flush_file(const string& p_area, const string& p_msg)
{
	g_csFileArea.lock();
	FastCriticalSection& l_cs = g_csFile[p_area];
	g_csFileArea.unlock();
	CFlyFastLock(l_cs);
	File f(p_area, File::WRITE, File::OPEN | File::CREATE);
	if (f.setEndPos(0) == 0)
	{
		f.write("\xef\xbb\xbf");
	}
	f.write(p_msg);
}

const string& LogManager::getSetting(int area, int sel)
{
	return SettingsManager::get(static_cast<SettingsManager::StrSetting>(g_logOptions[area][sel]), true);
}

void LogManager::saveSetting(int area, int sel, const string& setting)
{
	SettingsManager::set(static_cast<SettingsManager::StrSetting>(g_logOptions[area][sel]), setting);
}

void LogManager::log(LogArea area, const StringMap& params, bool p_only_file /* = false */) noexcept
{
#ifdef FLYLINKDC_LOG_IN_SQLITE_BASE
	if (!p_only_file && BOOLSETTING(FLY_SQLITE_LOG) && CFlylinkDBManager::isValidInstance())
	{
		dcassert(area != TRACE_SQLITE);
		CFlylinkDBManager::getInstance()->log(area, params);
	}
	else
#endif // FLYLINKDC_LOG_IN_SQLITE_BASE
	{
		const string path = SETTING(LOG_DIRECTORY) + Util::formatParams(getSetting(area, FILE), params, false);
		const string msg = Util::formatParams(getSetting(area, FORMAT), params, false);
		log(path, msg);
	}
}

void LogManager::virus_message(const string& p_message)
{
	if (BOOLSETTING(LOG_VIRUS_TRACE))
	{
		StringMap params;
		params["message"] = p_message;
		LOG(VIRUS_TRACE, params);
	}
}

void LogManager::ddos_message(const string& p_message)
{
	if (BOOLSETTING(LOG_DDOS_TRACE))
	{
		StringMap params;
		params["message"] = p_message;
		LOG(DDOS_TRACE, params);
	}
}


void LogManager::cmd_debug_message(const string& p_message)
{
	if (BOOLSETTING(LOG_CMDDEBUG_TRACE))
	{
		StringMap params;
		params["message"] = p_message;
		LOG(CMDDEBUG_TRACE, params);
	}
}

void LogManager::flood_message(const string& p_message)
{
	if (BOOLSETTING(LOG_FLOOD_TRACE))
	{
		StringMap params;
		params["message"] = p_message;
		LOG(FLOOD_TRACE, params);
	}
}

void LogManager::psr_message(const string& p_message)
{
	if (BOOLSETTING(LOG_PSR_TRACE))
	{
		StringMap params;
		params["message"] = p_message;
		LOG(PSR_TRACE, params);
	}
}

void LogManager::torrent_message(const string& p_message, bool p_is_add_sys_message /*= true*/)
{
	if (BOOLSETTING(LOG_TORRENT_TRACE))
	{
		StringMap params;
		params["message"] = p_message;
		LOG(TORRENT_TRACE, params);
	}
	if (p_is_add_sys_message)
	{
		StringMap params;
		params["message"] = p_message;
		LOG(SYSTEM, params);
	}
}

void LogManager::speak_status_message(const string& p_msg)
{
	if (LogManager::g_isLogSpeakerEnabled == true && ClientManager::isStartup() == false && ClientManager::isBeforeShutdown() == false)
	{
		if (LogManager::g_mainWnd)
		{
			auto l_str_messages = new string(p_msg);
			// TODO safe_post_message(LogManager::g_mainWnd, g_LogMessageID, l_str_messages);
			if (::PostMessage(LogManager::g_mainWnd, WM_SPEAKER, g_LogMessageID, (LPARAM)l_str_messages) == FALSE)
			{
				// TODO - LOG dcassert(0);
				dcdebug("[LogManager::g_mainWnd] PostMessage error %d\n", GetLastError());
				delete l_str_messages;
				LogManager::g_isLogSpeakerEnabled = false; // Fix error 1816
			}
		}
	}
}

void LogManager::message(const string& p_msg, bool p_only_file /*= false */)
{
#if !defined(FLYLINKDC_BETA)
	if (BOOLSETTING(LOG_SYSTEM))
#endif
	{
		StringMap params;
		params["message"] = p_msg;
		log(SYSTEM, params, p_only_file); // [1] https://www.box.net/shared/9e63916273d37e5b2932
	}
	speak_status_message(p_msg);
}
CFlyLog::CFlyLog(const string& p_message
                 , bool p_skip_start /* = true */
                 , bool p_only_file  /* = false */
                ) :
	m_start(GET_TICK()),
	m_message(p_message),
	m_tc(m_start),
	m_skip_start(p_skip_start), // TODO - ����� ��� �� �����?
	m_skip_stop(false),
	m_only_file(p_only_file)
{
	if (!m_skip_start)
	{
		log("[Start] " + m_message);
	}
}

CFlyLog::~CFlyLog()
{
	if (!m_skip_stop)
	{
		const uint64_t l_current = GET_TICK();
		log("[Stop] " + m_message + " [" + Util::toString(l_current - m_tc) + " ms, Total: " + Util::toString(l_current - m_start) + " ms]");
	}
}
uint64_t CFlyLog::calcSumTime() const
{
	const uint64_t l_current = GET_TICK();
	return l_current - m_start;
}
void CFlyLog::step(const string& p_message_step, const bool p_reset_count /*= true */)
{
	const uint64_t l_current = GET_TICK();
	dcassert(p_message_step.size() == string(p_message_step.c_str()).size());
	log("[Step] " + m_message + ' ' + p_message_step + " [" + Util::toString(l_current - m_tc) + " ms]");
	if (p_reset_count)
		m_tc = l_current;
}
void CFlyLog::loadStep(const string& p_message_step, const bool p_reset_count /*= true */)
{
	const uint64_t l_current = GET_TICK();
	const uint64_t l_step = l_current - m_tc;
	const uint64_t l_total = l_current - m_start;
	
	m_tc = l_current;
	if (p_reset_count)
	{
		log("[Step] " + m_message + " Begin load " + p_message_step + " [" + Util::toString(l_step) + " ms]");
	}
	else
	{
		log("[Step] " + m_message + " End load " + p_message_step + " [" + Util::toString(l_step) + " ms and " + Util::toString(l_total) + " ms after start]");
	}
}


/**
 * @file
 * $Id: LogManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
