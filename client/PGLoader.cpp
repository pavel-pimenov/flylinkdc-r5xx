/*
 * Copyright (C) 2011-2015 FlylinkDC++ Team http://flylinkdc.com/
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

#include "PGLoader.h"
#include "ResourceManager.h"
#include "LogManager.h"
#include "File.h"
#include "../windows/resource.h"
#include <boost/algorithm/string.hpp>

#ifdef PPA_INCLUDE_IPFILTER

void PGLoader::load(const string& p_data /*= Util::emptyString*/)
{
	string l_data;
	const string& l_url = SETTING(URL_IPTRUST);
	bool l_is_download = false;
	CFlyLog l_IPTrust_log("[IPTrust]");
	if (p_data.empty())
	{
		if (!l_url.empty())
		{
			try
			{
				const string l_log_message = STRING(DOWNLOAD) + ": " + l_url;
				if (Util::getDataFromInet(l_url, l_data, 0) == 0)
				{
					l_IPTrust_log.step(l_log_message + " [" + STRING(ERROR_STRING) + ']');
				}
				else
				{
					l_IPTrust_log.step(l_log_message + " [OK]");
					// "распарсим" ответ (TODO - перейти на regexp)
					{
						string::size_type linestart = 0;
						string::size_type lineend = 0;
						for (;;)
						{
							lineend = l_data.find('\n', linestart);
							if (lineend == string::npos)
								break;
							const string line = l_data.substr(linestart, lineend - linestart - 1);
							int u1, u2, u3, u4, u5, u6, u7, u8;
							int iItems = sscanf_s(line.c_str(), "%u.%u.%u.%u - %u.%u.%u.%u", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8);
							if (iItems == 8 || iItems == 4)
							{
								l_is_download = true;
								break;
							}
							iItems = sscanf_s(line.c_str(), "#%u.%u.%u.%u - %u.%u.%u.%u", &u1, &u2, &u3, &u4, &u5, &u6, &u7, &u8);
							if (iItems == 8 || iItems == 4)
							{
								l_is_download = true;
								break;
							}
							linestart = lineend + 1;
						}
					}
					if (l_is_download)
					{
						const auto l_IPTrust_ini = getConfigFileName();
						File(l_IPTrust_ini, File::WRITE, File::CREATE | File::TRUNCATE).write(l_data);
						l_IPTrust_log.step(l_IPTrust_ini + " [" + STRING(SAVE) + ']');
					}
				}
			}
			catch (const FileException& e)
			{
				l_IPTrust_log.step("Can't write IPTrust.ini file: " + e.getError());
				dcdebug("IPTrust::save: %s\n", e.getError().c_str());
			}
		}
		if (l_data.empty())
		{
			const string l_IPTrust_ini = getConfigFileName();
			if (File::isExist(l_IPTrust_ini))
			{
				try
				{
					l_data = File(l_IPTrust_ini, File::READ, File::OPEN).read();
				}
				catch (const FileException& e)
				{
					l_IPTrust_log.step("Can't read IPTrust.ini file: " + e.getError());
					dcdebug("IPTrust::load: %s\n", e.getError().c_str());
				}
			}
			else
			{
				Util::WriteTextResourceToFile(IDR_IPTRUST_EXAMPLE, Text::toT(l_IPTrust_ini));
				l_IPTrust_log.step("IPTrust.ini restored from internal resources");
			}
		}
		
	}
	else
	{
		l_data = p_data;
	}
	
	FastLock l(m_cs);
	l_IPTrust_log.step("parse IPTrust.ini");
	m_ipTrustListAllow.clear();
	m_ipTrustListBlock.clear();
	
	if (!l_data.empty())
	{
		string::size_type linestart = 0;
		string::size_type lineend = 0;
		for (;;)
		{
			lineend = l_data.find('\n', linestart);
			if (lineend == string::npos)
			{
				string l_line = l_data.substr(linestart);
				addLine(l_line, l_IPTrust_log);
				break;
			}
			else
			{
				string l_line = l_data.substr(linestart, lineend - linestart - 1);
				addLine(l_line, l_IPTrust_log);
				linestart = lineend + 1;
			}
		}
	}
	l_IPTrust_log.step("parse IPTrust.ini done");
}

bool PGLoader::check(const string& aIP)
{
	if (aIP.empty())
		return false;
		
#ifdef _DEBUG
	static boost::atomic_int g_count(0);
	dcdebug("PGLoader::check  count = %d\n", int(++g_count));
#endif
	
	FastLock l(m_cs);
	
	if (!m_ipTrustListBlock.empty())
	{
		if (m_ipTrustListBlock.checkIp(aIP))
		{
			return true;
		}
	}
	if (!m_ipTrustListAllow.empty())
	{
		const auto l_ipguard = m_ipTrustListAllow.checkIp(aIP);
		return !l_ipguard;
	}
	else
	{
		return false;
	}
}

void PGLoader::addLine(string& p_Line, CFlyLog& p_log)
{
	boost::replace_all(p_Line, " ", "");
	if (p_Line.empty())
		return;
		
	if (p_Line[0] == '#')
		return;
		
	if (p_Line[0] == '-')
	{
		p_Line.erase(0, 1);
		m_ipTrustListBlock.addLine(p_Line, p_log);
	}
	else
	{
		m_ipTrustListAllow.addLine(p_Line, p_log);
	}
}

#endif // PPA_INCLUDE_IPFILTER
