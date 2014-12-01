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
#include "LogManager.h"
#include "SettingsManager.h"
#include "CFlylinkDBManager.h"
#include "CompatibilityManager.h"
#include "TimerManager.h"

LogManager::LogManager()
#ifdef _DEBUG
	: _debugTotal(0), _debugMissed(0), _debugConcurrencyEventCount(0), _debugParallelWritesFiles(0)
#endif
{
	logOptions[UPLOAD][FILE]        = SettingsManager::LOG_FILE_UPLOAD;
	logOptions[UPLOAD][FORMAT]      = SettingsManager::LOG_FORMAT_POST_UPLOAD;
	logOptions[DOWNLOAD][FILE]      = SettingsManager::LOG_FILE_DOWNLOAD;
	logOptions[DOWNLOAD][FORMAT]    = SettingsManager::LOG_FORMAT_POST_DOWNLOAD;
	logOptions[CHAT][FILE]          = SettingsManager::LOG_FILE_MAIN_CHAT;
	logOptions[CHAT][FORMAT]        = SettingsManager::LOG_FORMAT_MAIN_CHAT;
	logOptions[PM][FILE]            = SettingsManager::LOG_FILE_PRIVATE_CHAT;
	logOptions[PM][FORMAT]          = SettingsManager::LOG_FORMAT_PRIVATE_CHAT;
	logOptions[SYSTEM][FILE]        = SettingsManager::LOG_FILE_SYSTEM;
	logOptions[SYSTEM][FORMAT]      = SettingsManager::LOG_FORMAT_SYSTEM;
	logOptions[STATUS][FILE]        = SettingsManager::LOG_FILE_STATUS;
	logOptions[STATUS][FORMAT]      = SettingsManager::LOG_FORMAT_STATUS;
	logOptions[WEBSERVER][FILE]     = SettingsManager::LOG_FILE_WEBSERVER;
	logOptions[WEBSERVER][FORMAT]   = SettingsManager::LOG_FORMAT_WEBSERVER;
#ifdef RIP_USE_LOG_PROTOCOL
	logOptions[PROTOCOL][FILE]      = SettingsManager::LOG_FILE_PROTOCOL;
	logOptions[PROTOCOL][FORMAT]    = SettingsManager::LOG_FORMAT_PROTOCOL;
#endif
	logOptions[CUSTOM_LOCATION][FILE]     = SettingsManager::LOG_FILE_CUSTOM_LOCATION;
	logOptions[CUSTOM_LOCATION][FORMAT]   = SettingsManager::LOG_FORMAT_CUSTOM_LOCATION;
	
	logOptions[TRACE_SQLITE][FILE]     = SettingsManager::LOG_FILE_TRACE_SQLITE;
	logOptions[TRACE_SQLITE][FORMAT]   = SettingsManager::LOG_FORMAT_TRACE_SQLITE;
	logOptions[DDOS_TRACE][FILE]     = SettingsManager::LOG_FILE_DDOS_TRACE;
	logOptions[DDOS_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_DDOS_TRACE;
	logOptions[CMDDEBUG_TRACE][FILE]     = SettingsManager::LOG_FILE_CMDDEBUG_TRACE;
	logOptions[CMDDEBUG_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_CMDDEBUG_TRACE;
	logOptions[DHT_TRACE][FILE]     = SettingsManager::LOG_FILE_DHT_TRACE;
	logOptions[DHT_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_DHT_TRACE;
	logOptions[PSR_TRACE][FILE]     = SettingsManager::LOG_FILE_PSR_TRACE;
	logOptions[PSR_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_PSR_TRACE;
	logOptions[FLOOD_TRACE][FILE]     = SettingsManager::LOG_FILE_FLOOD_TRACE;
	logOptions[FLOOD_TRACE][FORMAT]   = SettingsManager::LOG_FORMAT_FLOOD_TRACE;
	
	if (!CompatibilityManager::getStartupInfo().empty())
		message(CompatibilityManager::getStartupInfo());
		
	if (CompatibilityManager::isIncompatibleSoftwareFound()) //[+]IRainman http://code.google.com/p/flylinkdc/issues/detail?id=574
		message(CompatibilityManager::getIncompatibleSoftwareMessage());
}

void LogManager::log(const string& p_area, const string& p_msg) noexcept
{
#ifndef IRAINMAN_USE_NG_LOG_MANAGER
	FastLock l(m_csPatchCache);
#endif
	string l_area;
	bool newPatch;
	{
#ifdef IRAINMAN_USE_NG_LOG_MANAGER
		FastLock l(m_csPatchCache);
#endif
		const auto l_fine_it = m_patchCache.find(p_area);
		newPatch = l_fine_it == m_patchCache.end();
		if (newPatch)
		{
			if (m_patchCache.size() > 250)
				m_patchCache.clear();
				
			l_area = Util::validateFileName(p_area);
#ifdef _DEBUG
			_debugMissed++;
			const std::pair<string, size_t> l_pair(l_area, 0);
			m_patchCache[p_area] = l_pair;
#else
			m_patchCache[p_area] = l_area;
#endif
		}
		else
		{
#ifdef _DEBUG
			_debugTotal++;
			if (++l_fine_it->second.second % 100 == 0)
				dcdebug("log path cache: size=%i [%s] %i\n", m_patchCache.size(), p_area.c_str(), l_fine_it->second.second);
				
			l_area = l_fine_it->second.first;
#else
			l_area = l_fine_it->second;
#endif
		}
	}
#ifdef IRAINMAN_USE_NG_LOG_MANAGER
	while (true)
	{
		{
			FastLock l(m_csCurrentlyOpenedFiles);
#ifdef _DEBUG
			if (!m_currentlyOpenedFiles.empty())
			{
				_debugParallelWritesFiles++;
			}
#endif
			if (m_currentlyOpenedFiles.insert(l_area).second == true)
			{
				break;
			}
#ifdef _DEBUG
			else
			{
				++_debugConcurrencyEventCount;
			}
#endif
		}
		Thread::sleep(1);
	}
#endif // IRAINMAN_USE_NG_LOG_MANAGER
	try
	{
		if (newPatch)
		{
			File::ensureDirectory(l_area);
		}
		dcassert(!l_area.empty());
		File f(l_area, File::WRITE, File::OPEN | File::CREATE);
		f.setEndPos(0);
		if (f.getPos() == 0)
		{
			f.write("\xef\xbb\xbf");
		}
		f.write(p_msg + "\r\n");
	}
	catch (const FileException&)
	{
		//-V565
		//dcassert(0);
	}
#ifdef IRAINMAN_USE_NG_LOG_MANAGER
	{
		FastLock l(m_csCurrentlyOpenedFiles);
		m_currentlyOpenedFiles.erase(l_area);
	}
#endif
}
const string& LogManager::getSetting(int area, int sel) const
{
	return SettingsManager::get(static_cast<SettingsManager::StrSetting>(logOptions[area][sel]), true);
}

void LogManager::saveSetting(int area, int sel, const string& setting)
{
	SettingsManager::set(static_cast<SettingsManager::StrSetting>(logOptions[area][sel]), setting);
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
		//Lock l(cs); [-] IRainman fix: no needs lock here, see log function.
		const string path = SETTING(LOG_DIRECTORY) + Util::formatParams(getSetting(area, FILE), params, false);
		const string msg = Util::formatParams(getSetting(area, FORMAT), params, false);
		log(path, msg);
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

void LogManager::dht_message(const string& p_message)
{
	if (BOOLSETTING(LOG_DHT_TRACE))
	{
		StringMap params;
		params["message"] = p_message;
		LOG(DHT_TRACE, params);
	}
}

void LogManager::message(const string& msg, bool p_only_file /*= false */)
{
#if !defined(FLYLINKDC_BETA) || defined(FLYLINKDC_HE)
	if (BOOLSETTING(LOG_SYSTEM))
#endif
	{
		StringMap params;
		params["message"] = msg;
		log(SYSTEM, params, p_only_file); // [1] https://www.box.net/shared/9e63916273d37e5b2932
	}
	if (TimerManager::g_isStartupShutdownProcess == false)
	{
		fire(LogManagerListener::Message(), msg);
	}
}
CFlyLog::CFlyLog(const string& p_message
                 , bool p_skip_start /* = true */
                 , bool p_only_file  /* = false */
                ) :
	m_start(GET_TICK()),
	m_message(p_message),
	m_tc(m_start),
	m_skip_start(p_skip_start), // TODO - может оно не нужно?
	m_only_file(p_only_file)
{
	if (!m_skip_start)
	{
		log("[Start] " + m_message);
	}
}

CFlyLog::~CFlyLog()
{
	//if(!m_skip_start_stop)
	{
		const uint64_t l_current = GET_TICK();
		log("[Stop ] " + m_message + " [" + Util::toString(l_current - m_tc) + " ms, Total: " + Util::toString(l_current - m_start) + " ms]");
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
	log("[Step ] " + m_message + ' ' + p_message_step + " [" + Util::toString(l_current - m_tc) + " ms]");
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
		log("[Step ] " + m_message + " Begin load " + p_message_step + " [" + Util::toString(l_step) + " ms]");
	}
	else
	{
		log("[Step ] " + m_message + " End load " + p_message_step + " [" + Util::toString(l_step) + " ms and " + Util::toString(l_total) + " ms after start]");
	}
}


/**
 * @file
 * $Id: LogManager.cpp 568 2011-07-24 18:28:43Z bigmuscle $
 */
